import admin from "firebase-admin";
import Bin from "./models/bin.js";
import Classification from "./models/classification.js";
import Alert from "./models/alert.js";

// ===== Firebase Initialization =====
if (!admin.apps.length) {
  admin.initializeApp({
    credential: admin.credential.cert("./serviceAccountKey.json"),
    databaseURL: "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app/"
  });
}
const db = admin.database();

// ===== CONFIG =====
const validBinTypes = ["recyclable", "biodegradable", "non_biodegradable"];
const FULL_THRESHOLD_DISTANCE = 5; // cm
const FULL_DETECTION_MS = 2000;    // 2 seconds threshold
const EMPTY_DISTANCE_CM = 15;      // must match ESP32

// ===== GLOBALS =====
const previousBinStates = {};
const fullDetectionTimers = {};

// ===== HELPERS =====
const calculateFillPercentage = (distance) => {
  if (distance >= EMPTY_DISTANCE_CM) return 0;
  if (distance <= 0) return 100;
  return Math.min(Math.max((1 - distance / EMPTY_DISTANCE_CM) * 100, 0), 100);
};

const normalizeBinData = (binId, binData) => {
  if (!binData) binData = {};

  // Use Firebase data if provided, fallback to calculation
  let distance = Number(binData.distance_cm ?? binData.distance ?? EMPTY_DISTANCE_CM);
  let fill_level = Number(binData.fill_level ?? calculateFillPercentage(distance));
  let status = binData.status ?? "OK";

  return {
    binId,
    type: binId,
    distance_cm: distance,
    fill_level,
    fill_level_percent: fill_level,
    status,
    last_update: new Date(),
    isDeleted: false
  };
};

// ===== ALERTS =====
const createFullAlertIfNeeded = async (bin) => {
  if (bin.status !== "FULL") return false;
  try {
    const existingAlert = await Alert.findOne({
      bin_id: bin._id,
      alert_type: "FULL",
      resolved: false
    });

    if (!existingAlert) {
      const distance = Number(bin.distance_cm ?? 0).toFixed(1);
      const fill = Number(bin.fill_level ?? 0);

      const alert = new Alert({
        bin_id: bin._id,
        bin_type: bin.type,
        alert_type: "FULL",
        distance_cm: distance,
        fill_level: fill,
        timestamp: new Date(),
        message: `Bin ${bin.type} is FULL! Distance: ${distance}cm`
      });

      await alert.save();
      console.log(`🚨 ALERT created for ${bin.type}`);
      return true;
    }
    return false;

  } catch (err) {
    console.error(`❌ Error creating alert for ${bin.type}:`, err);
    return false;
  }
};
// ===== 
//  BIN =====
// ===== UPSERT BIN (sync with Firebase directly) =====
// ===== UPSERT BIN (mirror Firebase exactly) =====
// ===== UPSERT BIN (mirror Firebase safely) =====
const upsertBin = async (binId, binData) => {
  if (!validBinTypes.includes(binId)) return;

  try {
    if (!binData) binData = {};

    // Read values directly from Firebase with defaults
    const distance = Number(binData.fill_level ?? binData.distance_cm ?? 0);      // optional raw distance
    const fill_level_percent = Number(binData.fill_level_percent ?? 0);           // use ESP32-provided percent
    const status = binData.status ?? "OK";                                        // use ESP32-provided status

    // Prepare normalized bin object
    const normalizedBin = {
      binId,
      type: binId,
      distance_cm: distance,
      fill_level: fill_level_percent,          // store percent in MongoDB
      fill_level_percent: fill_level_percent,
      status,
      last_update: new Date(),
      isDeleted: false
    };

    const previousState = previousBinStates[binId];
    previousBinStates[binId] = { ...normalizedBin };

    // Upsert MongoDB safely
    const updatedBin = await Bin.findOneAndUpdate(
      { binId },
      normalizedBin,
      { upsert: true, new: true, setDefaultsOnInsert: true }
    );

    if (!updatedBin) {
      console.warn(`⚠️ Upsert returned null for ${binId}`);
      return;
    }

    // Safely log values
    const fillPercentLog = updatedBin.fill_level_percent ?? 0;
    const statusLog = updatedBin.status ?? "OK";
    console.log(`✅ ${binId}: ${fillPercentLog.toFixed(1)}% - ${statusLog}`);

    // Create alert if status is FULL
    if (statusLog === "FULL") await createFullAlertIfNeeded(updatedBin);

    // Resolve previous FULL alerts if status changed
    if (previousState && previousState.status === "FULL" && statusLog !== "FULL") {
      await Alert.updateMany(
        { bin_id: updatedBin._id, alert_type: "FULL", resolved: false },
        { resolved: true, resolved_at: new Date() }
      );
      console.log(`✅ ${binId} FULL alerts resolved`);
    }

  } catch (err) {
    console.error(`❌ Error upserting ${binId}:`, err);
  }
};



// ===== INITIAL LOAD =====
const loadAllBins = async () => {
  try {
    const snapshot = await db.ref("/bins").get();
    const bins = snapshot.val();
    if (!bins) return;

    for (const [binId, binData] of Object.entries(bins)) {
      if (validBinTypes.includes(binId)) await upsertBin(binId, binData);
    }

    console.log("✅ Initial load complete");
    startBinListeners();
  } catch (err) {
    console.error("❌ Failed to load bins:", err);
    startBinListeners(); // ensure listeners start even if load fails
  }
};

// ===== BIN LISTENERS =====
const lastBinValues = {};
// ===== REAL-TIME BIN LISTENER =====
const startBinListeners = () => {
  console.log("👂 Listening for bin changes...");

  validBinTypes.forEach(binId => {
    const ref = db.ref(`/bins/${binId}`);

    const listener = async snapshot => {
      try {
        const binData = snapshot.val();
        if (!binData) return;

        // Optional: stringify to detect duplicate updates
        const newDataString = JSON.stringify(binData);
        if (lastBinValues[binId] === newDataString) return; // skip if unchanged
        lastBinValues[binId] = newDataString;

        // Upsert into MongoDB
        await upsertBin(binId, binData);

      } catch (err) {
        console.error(`❌ Listener error for ${binId}:`, err);
      }
    };

    ref.on("value", listener);
  });
};


// ===== CLASSIFICATION LISTENER =====
db.ref("/classification/latest").on("value", async snapshot => {
  try {
    const data = snapshot.val();
    if (!data) return;

    const bin = await Bin.findOne({ type: data.type });
    if (!bin) return;

    const classification = new Classification({
      bin_id: bin._id,
      bin_type: bin.type,
      label: data.type,
      confidence: data.confidence,
      image_url: data.image_url,
      timestamp: new Date()
    });

    await classification.save();
    console.log(`📸 Classification saved: ${bin.type} (${data.confidence}%)`);
  } catch (err) {
    console.error("❌ Classification listener error:", err);
  }
});

// ===== START SERVER =====
console.log("🚀 Firebase listener starting...");
console.log(`📏 FULL: ≤${FULL_THRESHOLD_DISTANCE}cm for ${FULL_DETECTION_MS / 1000}s`);
loadAllBins();

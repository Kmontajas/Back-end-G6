import admin from "firebase-admin";
import Bin from "./models/bin.js";
import Classification from "./models/classification.js";
import Alert from "./models/alert.js";

// Firebase initialization
if (!admin.apps.length) {
  admin.initializeApp({
    credential: admin.credential.cert("./serviceAccountKey.json"),
    databaseURL: "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app/"
  });
}

const db = admin.database();

// ===== CONFIG =====
const validBinTypes = ["recyclable", "biodegradable", "non_biodegradable"];
const FULL_THRESHOLD_DISTANCE = 10; // cm
const EMPTY_DISTANCE = 40;           // cm

const previousBinStates = {};
const fullDetectionTimers = {};

// ===== HELPERS =====
const calculateFillPercentage = (distance) => {
  if (distance >= EMPTY_DISTANCE) return 0;
  if (distance <= 0) return 100;
  const percentage = (1 - distance / EMPTY_DISTANCE) * 100;
  return Math.min(Math.max(percentage, 0), 100);
};

const normalizeBinData = (binId, binData) => {
  if (!binData) binData = {};

  let distance = EMPTY_DISTANCE;
  let fill_level = 0;

  // Determine fill_level correctly 
  if (binData.distance !== undefined) {
    distance = Number(binData.distance);
    fill_level = calculateFillPercentage(distance);
  } else if (binData.distance_cm !== undefined) {
    distance = Number(binData.distance_cm);
    fill_level = calculateFillPercentage(distance);
  } else if (binData.fill_level !== undefined) {
    fill_level = Number(binData.fill_level); // Use Firebase value directly
    // Optionally, estimate distance for FULL detection
    distance = EMPTY_DISTANCE * (1 - fill_level / 100);
  } else {
    fill_level = 0;
  }

  const status = fill_level >= 100 || distance <= FULL_THRESHOLD_DISTANCE ? "FULL" : "OK";

  return {
    binId,
    type: binId,
    distance_cm: distance,
    fill_level,
    status,
    last_update: new Date(),
    isDeleted: false
  };
};

// ===== ALERTS =====
const createFullAlertIfNeeded = async (bin) => {
  if (bin.status !== "FULL") return false;
  try {
    const existingAlert = await Alert.findOne({ bin_id: bin._id, alert_type: "FULL", resolved: false });
    if (!existingAlert) {
      const alert = new Alert({
        bin_id: bin._id,
        bin_type: bin.type,
        alert_type: "FULL",
        distance_cm: bin.distance_cm,
        fill_level: bin.fill_level,
        timestamp: new Date(),
        message: `Bin ${bin.type} is FULL! Distance: ${bin.distance_cm.toFixed(1)}cm`
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

// ===== UPSERT BIN =====
const upsertBin = async (binId, binData) => {
  if (!validBinTypes.includes(binId)) return;

  try {
    const normalizedBin = normalizeBinData(binId, binData);

    // 2-second FULL detection
    if (normalizedBin.distance_cm <= FULL_THRESHOLD_DISTANCE) {
      if (!fullDetectionTimers[binId]) fullDetectionTimers[binId] = Date.now();
      else if (Date.now() - fullDetectionTimers[binId] >= 2000) normalizedBin.status = "FULL";
      else normalizedBin.status = "OK";
    } else {
      delete fullDetectionTimers[binId];
      normalizedBin.status = "OK";
    }

    const previousState = previousBinStates[binId];
    previousBinStates[binId] = { ...normalizedBin };

    const updatedBin = await Bin.findOneAndUpdate({ binId }, normalizedBin, { upsert: true, new: true });
    console.log(`✅ ${binId}: ${updatedBin.fill_level.toFixed(2)}% - ${updatedBin.status}`);

    if (updatedBin.status === "FULL") await createFullAlertIfNeeded(updatedBin);

    // Resolve previous FULL alerts if status changed
    if (previousState && previousState.status === "FULL" && updatedBin.status === "OK") {
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

// ===== LISTENERS =====
const lastBinValues = {};
const startBinListeners = () => {
  console.log("👂 Listening for bin changes...");

  validBinTypes.forEach(binId => {
    const ref = db.ref(`/bins/${binId}`);
    const listener = async snapshot => {
      try {
        const binData = snapshot.val();
        if (!binData) return;

        const newDataString = JSON.stringify(binData);
        if (lastBinValues[binId] === newDataString) return;
        lastBinValues[binId] = newDataString;

        await upsertBin(binId, binData);
      } catch (err) {
        console.error(`❌ Listener error for ${binId}:`, err);
      }
    };

    ref.on("value", listener);

    // Reconnect on disconnect
    ref.onDisconnect().cancel(err => {
      if (err) console.error(`❌ onDisconnect cancel error for ${binId}:`, err);
    });
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

// ===== START =====
console.log("🚀 Firebase listener starting...");
console.log(`📏 FULL: ≤${FULL_THRESHOLD_DISTANCE}cm for 2 seconds`);
loadAllBins();

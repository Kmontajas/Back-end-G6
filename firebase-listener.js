import admin from "firebase-admin";
import Bin from "./models/bin.js";
import Classification from "./models/classification.js";
import Alert from "./models/alert.js";

// Firebase
if (!admin.apps.length) {
  admin.initializeApp({
    credential: admin.credential.cert("./serviceAccountKey.json"),
    databaseURL: "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app/"
  });
}

const db = admin.database();

// Bins 
const validBinTypes = ["recyclable", "biodegradable", "nonbiodegradable"];
const MAX_CAPACITY = 100;

// Helpers
const normalizeBinData = (binId, binData) => {
  if (!binData) binData = {};

  let fillLevel = 0;
  if (binData.fill_level !== undefined) fillLevel = Number(binData.fill_level);
  else if (binData.fillLevel !== undefined) fillLevel = Number(binData.fillLevel);

  let status = "OK";
  if (typeof binData.status === "string") status = binData.status.toUpperCase().trim();
  else if (typeof binData.Status === "string") status = binData.Status.toUpperCase().trim();
  else if (fillLevel >= MAX_CAPACITY) status = "FULL";

  return {
    binId,
    type: binId,        
    fill_level: fillLevel,
    status,
    last_update: new Date(),
    isDeleted: false      
  };
};

const createFullAlertIfNeeded = async (bin) => {
  if (bin.status !== "FULL") return;

  const existingAlert = await Alert.findOne({
    bin_id: bin._id,
    alert_type: "FULL",
    resolved: false
  });

  if (!existingAlert) {
    const alert = new Alert({
      bin_id: bin._id,
      alert_type: "FULL",
      timestamp: new Date()
    });
    await alert.save();
    console.log(`🚨 Alert created for bin ${bin.type}`);
  }
};

// Bin upsert
const upsertBin = async (binId, binData) => {
  if (!validBinTypes.includes(binId)) return;

  const normalizedBin = normalizeBinData(binId, binData);

  const updatedBin = await Bin.findOneAndUpdate(
    { binId },
    normalizedBin,
    { upsert: true, new: true }
  );

  console.log(`✅ Bin ${binId} updated in MongoDB: fill_level=${updatedBin.fill_level}, status=${updatedBin.status}`);
  await createFullAlertIfNeeded(updatedBin);
};

// Initial
const loadAllBins = async () => {
  try {
    const snapshot = await db.ref("/bins").get();
    const bins = snapshot.val();
    if (!bins) return;

    for (const [binId, binData] of Object.entries(bins)) {
      await upsertBin(binId, binData);
    }

    console.log("✅ Initial bin load complete");
    startBinListeners();
  } catch (err) {
    console.error("❌ Failed to load bins from Firebase:", err);
  }
};

// Dynamic listeners
const lastBinValues = {};

const startBinListeners = () => {
  validBinTypes.forEach(binId => {
    db.ref(`/bins/${binId}`).on("value", async snapshot => {
      try {
        const binData = snapshot.val();
        if (!binData) return;

        const newDataString = JSON.stringify(binData);
        if (lastBinValues[binId] === newDataString) return;

        lastBinValues[binId] = newDataString;
        console.log(`🔄 Bin ${binId} changed in Firebase`);
        await upsertBin(binId, binData);
      } catch (err) {
        console.error(`❌ Firebase listener error for ${binId}:`, err);
      }
    });
  });
};

// Classification listeners
db.ref("/classification/latest").on("value", async snapshot => {
  try {
    const data = snapshot.val();
    if (!data) return;

    const bin = await Bin.findOne({ type: data.type });
    if (!bin) return;

    const classification = new Classification({
      bin_id: bin._id,
      label: data.type,
      confidence: data.confidence,
      image_url: data.image_url
    });

    await classification.save();
    console.log(`📸 Saved classification for bin ${bin.type}`);
  } catch (err) {
    console.error("❌ Classification listener error:", err);
  }
});

loadAllBins();

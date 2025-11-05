import admin from "firebase-admin";
import mongoose from "mongoose";
import dotenv from "dotenv";
import Bin from "./models/Bin.js";
import Classification from "./models/Classification.js";
import Alert from "./models/Alert.js";

dotenv.config();

// Initialize Firebase Admin
admin.initializeApp({
  credential: admin.credential.cert("./serviceAccountKey.json"),
  databaseURL: "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app/"
});

const db = admin.database();

// Connection to MongoDB
mongoose.connect(process.env.MONGO_URI)
  .then(() => console.log("✅ Connected to MongoDB"))
  .catch(err => console.error("MongoDB connection error:", err));

// Firebase listener for ESP32-CAM classifications
const classificationRef = db.ref("/classification/latest");
classificationRef.on("value", async (snapshot) => {
  const data = snapshot.val();
  if (!data) return;

  console.log("📸 New Classification:", data);

  try {
    // Save classification to MongoDB
    const classification = new Classification(data);
    await classification.save();
    console.log("✅ Saved classification to MongoDB");
  } catch (err) {
    console.error("❌ Error saving classification:", err);
  }
});

// Firebase listener for ESP32 bin alerts
const binRef = db.ref("/bins");
binRef.on("child_changed", async (snapshot) => {
  const bins = snapshot.val();
  if (!bins) return;

  console.log("🗑️ Bin data update detected.");

for (const [binId, binData] of Object.entries(bins)) {
  try {
    await Bin.findOneAndUpdate({ binId }, { ...binData, binId }, { upsert: true, new: true });
    console.log(`✅ Bin ${binId} updated or inserted successfully.`);
  } catch (err) {
    console.error(`❌ Error updating bin ${binId}:`, err.message);
  }

  if (binData.status === "FULL") {
    const alert = new Alert({
      bin_id: binId,
      alert_type: "FULL",
      timestamp: new Date(),
    });
    await alert.save();
    console.log(`🚨 Alert created for bin ${binId}`);
  }
}
});

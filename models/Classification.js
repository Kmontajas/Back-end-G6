import mongoose from "mongoose";

const classificationSchema = new mongoose.Schema({
  bin_id: { type: mongoose.Schema.Types.ObjectId, ref: "Bin" },
  label: { type: String, enum: ["recyclable", "biodegradable", "nonbiodegradable"], required: true },
  confidence: Number,
  image_url: String,
  timestamp: { type: Date, default: Date.now }
});

export default mongoose.model("Classification", classificationSchema);

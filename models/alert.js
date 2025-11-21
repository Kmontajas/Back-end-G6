import mongoose from "mongoose";

const alertSchema = new mongoose.Schema({
  bin_id: { type: mongoose.Schema.Types.ObjectId, ref: "Bin", required: true },
  alert_type: { type: String, enum: ["FULL", "Error", "Maintenance Needed"], required: true },
  timestamp: { type: Date, default: Date.now },
  resolved: { type: Boolean, default: false },
  resolved_by: { type: mongoose.Schema.Types.ObjectId, ref: "User" },
  resolved_at: { type: Date }
});
const Alert = mongoose.models.Alert || mongoose.model("Alert", alertSchema);

export default Alert;

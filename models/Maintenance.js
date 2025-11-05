import mongoose from "mongoose";

const maintenanceSchema = new mongoose.Schema({
  bin_id: { type: mongoose.Schema.Types.ObjectId, ref: "Bin" },
  performed_by: { type: mongoose.Schema.Types.ObjectId, ref: "User" },
  action: { type: String, required: true },
  notes: String,
  timestamp: { type: Date, default: Date.now }
});

export default mongoose.model("Maintenance", maintenanceSchema);

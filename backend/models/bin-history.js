import mongoose from "mongoose";

const binHistorySchema = new mongoose.Schema({
  bin_id: { type: mongoose.Schema.Types.ObjectId, ref: "Bin" },
  action: { type: String, enum: ["Opened Lid", "Closed Lid", "Filled", "Emptied"], required: true },
  timestamp: { type: Date, default: Date.now }
});
const BinHistory = mongoose.models.BinHistory || mongoose.model("BinHistory", binHistorySchema);

export default BinHistory;

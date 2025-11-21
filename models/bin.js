import mongoose from "mongoose";

const binSchema = new mongoose.Schema({
  binId: { type: String, required: true, unique: true },
  type: { type: String, enum: ["recyclable", "biodegradable", "nonbiodegradable"], required: true },
  location: { type: String },
  status: { type: String, enum: ["OK", "FULL", "MAINTENANCE"], default: "OK" },
  fill_level: { type: Number, default: 0 },
  last_update: { type: Date, default: Date.now },
  servo_pin: Number,
  ultrasonic_pins: {
    trig: Number,
    echo: Number
  }
});
const Bin = mongoose.models.Bin || mongoose.model("Bin", binSchema);

export default Bin;

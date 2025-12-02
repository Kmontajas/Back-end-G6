import express from "express";
import mongoose from "mongoose";
import dotenv from "dotenv";
import cors from "cors";

import "./firebase-listener.js";

import binRoutes from "./routes/bin.js";
import alertRoutes from "./routes/alert.js";
import classificationRoutes from "./routes/classification.js";
import binHistoryRoutes from "./routes/bin-history.js";
import maintenanceRoutes from "./routes/maintenance.js";
import userRoutes from "./routes/user.js"; 

dotenv.config();

const app = express();

app.use(cors({
  origin: "*" 
}));

app.use(express.json());

// MONGODB 
mongoose.connect(process.env.MONGO_URI)
  .then(() => console.log("Connected to MongoDB"))
  .catch(err => console.error("MongoDB connection error:", err));

// Base route
app.get("/", (req, res) => res.send("🚀 Smart Trash Bin API is running!"));

// Resource routes
app.use("/bins", binRoutes);
app.use("/alerts", alertRoutes);
app.use("/classifications", classificationRoutes);
app.use("/bin-history", binHistoryRoutes);
app.use("/maintenance", maintenanceRoutes);
app.use("/users", userRoutes);

// 404 Handler
app.use((req, res) => {
  res.status(404).json({ error: "Route not found" });
});

// Start server
const PORT = process.env.PORT || 3000;
app.listen(PORT, "0.0.0.0", () => console.log(`🚀 API server running on http://localhost:${PORT}`));


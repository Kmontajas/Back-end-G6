import express from "express";
import { getMaintenance, getMaintenanceById, createMaintenance, updateMaintenance, deleteMaintenance } from "../controllers/maintenance-controller.js";

const router = express.Router();

router.get("/", getMaintenance);
router.get("/:id", getMaintenanceById);
router.post("/", createMaintenance);
router.put("/:id", updateMaintenance);
router.delete("/:id", deleteMaintenance);

export default router;

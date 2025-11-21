import express from "express";
import { getBins, getBinById, createBin, updateBin, deleteBin } from "../controllers/bin-controller.js";

const router = express.Router();

router.get("/", getBins);
router.get("/:id", getBinById);
router.post("/", createBin);
router.put("/:id", updateBin);
router.delete("/:id", deleteBin);

export default router;

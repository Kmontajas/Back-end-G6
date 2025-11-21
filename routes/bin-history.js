import express from "express";
import { getBinHistory, getBinHistoryById, createBinHistory, updateBinHistory, deleteBinHistory } from "../controllers/bin-history-controller.js";

const router = express.Router();

router.get("/", getBinHistory);
router.get("/:id", getBinHistoryById);
router.post("/", createBinHistory);
router.put("/:id", updateBinHistory);
router.delete("/:id", deleteBinHistory);

export default router;

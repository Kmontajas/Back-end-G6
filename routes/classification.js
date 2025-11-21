import express from "express";
import { getClassifications, getLatestClassification, createClassification, updateClassification, deleteClassification } from "../controllers/classification-controller.js";

const router = express.Router();

router.get("/", getClassifications);
router.get("/latest", getLatestClassification);
router.post("/", createClassification);
router.put("/:id", updateClassification);
router.delete("/:id", deleteClassification);

export default router;

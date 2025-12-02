import BinHistory from "../models/bin-history.js";

// GET all bin history
export const getBinHistory = async (req, res) => {
  try {
    const history = await BinHistory.find({ isDeleted: { $ne: true } });
    res.json(history);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch bin history" });
  }
};

// GET bin history by ID
export const getBinHistoryById = async (req, res) => {
  try {
    const record = await BinHistory.findById(req.params.id);
    if (!record || record.isDeleted) return res.status(404).json({ error: "History record not found" });
    res.json(record);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch bin history record" });
  }
};

// POST new bin history
export const createBinHistory = async (req, res) => {
  try {
    const newRecord = new BinHistory(req.body);
    await newRecord.save();
    res.status(201).json({ message: "Bin history record created", record: newRecord });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to create bin history record" });
  }
};

// PUT / PATCH bin history
export const updateBinHistory = async (req, res) => {
  try {
    const updated = await BinHistory.findByIdAndUpdate(req.params.id, req.body, { new: true });
    if (!updated) return res.status(404).json({ error: "Record not found" });
    res.json({ message: "Bin history record updated", record: updated });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to update bin history record" });
  }
};

// DELETE (soft-delete)
export const deleteBinHistory = async (req, res) => {
  try {
    const deleted = await BinHistory.findByIdAndUpdate(req.params.id, { isDeleted: true }, { new: true });
    if (!deleted) return res.status(404).json({ error: "Record not found" });
    res.json({ message: "Bin history record soft-deleted", record: deleted });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to delete bin history record" });
  }
};

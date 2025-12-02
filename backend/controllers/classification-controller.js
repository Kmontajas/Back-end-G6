import Classification from "../models/classification.js";

// GET all classifications
export const getClassifications = async (req, res) => {
  try {
    const classifications = await Classification.find({ isDeleted: { $ne: true } });
    res.json(classifications);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch classifications" });
  }
};

// GET latest classification
export const getLatestClassification = async (req, res) => {
  try {
    const latest = await Classification.findOne({ isDeleted: { $ne: true } }).sort({ timestamp: -1 });
    res.json(latest);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch latest classification" });
  }
};

// POST new classification
export const createClassification = async (req, res) => {
  try {
    const newClassification = new Classification(req.body);
    await newClassification.save();
    res.status(201).json({ message: "Classification created successfully", classification: newClassification });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to create classification" });
  }
};

// PUT / PATCH classification
export const updateClassification = async (req, res) => {
  try {
    const updated = await Classification.findByIdAndUpdate(req.params.id, req.body, { new: true });
    if (!updated) return res.status(404).json({ error: "Classification not found" });
    res.json({ message: "Classification updated successfully", classification: updated });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to update classification" });
  }
};

// DELETE (soft-delete)
export const deleteClassification = async (req, res) => {
  try {
    const deleted = await Classification.findByIdAndUpdate(req.params.id, { isDeleted: true }, { new: true });
    if (!deleted) return res.status(404).json({ error: "Classification not found" });
    res.json({ message: "Classification soft-deleted", classification: deleted });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to delete classification" });
  }
};

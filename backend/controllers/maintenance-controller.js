import Maintenance from "../models/maintenance.js";

// GET all maintenance records
export const getMaintenance = async (req, res) => {
  try {
    const records = await Maintenance.find({ isDeleted: { $ne: true } });
    res.json(records);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch maintenance records" });
  }
};

// GET maintenance by ID
export const getMaintenanceById = async (req, res) => {
  try {
    const record = await Maintenance.findById(req.params.id);
    if (!record || record.isDeleted) return res.status(404).json({ error: "Record not found" });
    res.json(record);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch maintenance record" });
  }
};

// POST new maintenance record
export const createMaintenance = async (req, res) => {
  try {
    const newRecord = new Maintenance(req.body);
    await newRecord.save();
    res.status(201).json({ message: "Maintenance record created", record: newRecord });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to create maintenance record" });
  }
};

// PUT / PATCH maintenance record
export const updateMaintenance = async (req, res) => {
  try {
    const updated = await Maintenance.findByIdAndUpdate(req.params.id, req.body, { new: true });
    if (!updated) return res.status(404).json({ error: "Record not found" });
    res.json({ message: "Maintenance record updated", record: updated });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to update maintenance record" });
  }
};

// DELETE (soft-delete)
export const deleteMaintenance = async (req, res) => {
  try {
    const deleted = await Maintenance.findByIdAndUpdate(req.params.id, { isDeleted: true }, { new: true });
    if (!deleted) return res.status(404).json({ error: "Record not found" });
    res.json({ message: "Maintenance record soft-deleted", record: deleted });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to delete maintenance record" });
  }
};

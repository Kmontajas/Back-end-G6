import Alert from "../models/alert.js";

// GET all alerts
export const getAlerts = async (req, res) => {
  try {
    const alerts = await Alert.find({ isDeleted: { $ne: true } });
    res.json(alerts);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch alerts" });
  }
};

// GET alert by ID
export const getAlertById = async (req, res) => {
  try {
    const alert = await Alert.findById(req.params.id);
    if (!alert || alert.isDeleted) return res.status(404).json({ error: "Alert not found" });
    res.json(alert);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch alert" });
  }
};

// POST new alert
export const createAlert = async (req, res) => {
  try {
    const newAlert = new Alert(req.body);
    await newAlert.save();
    res.status(201).json({ message: "Alert created successfully", alert: newAlert });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to create alert" });
  }
};

// PUT / PATCH alert
export const updateAlert = async (req, res) => {
  try {
    const updatedAlert = await Alert.findByIdAndUpdate(req.params.id, req.body, { new: true });
    if (!updatedAlert) return res.status(404).json({ error: "Alert not found" });
    res.json({ message: "Alert updated successfully", alert: updatedAlert });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to update alert" });
  }
};

// DELETE (soft-delete)
export const deleteAlert = async (req, res) => {
  try {
    const deletedAlert = await Alert.findByIdAndUpdate(req.params.id, { isDeleted: true }, { new: true });
    if (!deletedAlert) return res.status(404).json({ error: "Alert not found" });
    res.json({ message: "Alert soft-deleted", alert: deletedAlert });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to delete alert" });
  }
};

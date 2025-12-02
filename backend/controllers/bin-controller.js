import Bin from "../models/bin.js";
import Classification from "../models/classification.js";
import Alert from "../models/alert.js";

// GET all bins
export const getBins = async (req, res) => {
  try {
    const bins = await Bin.find();
    const binsWithDetails = await Promise.all(
      bins.map(async (bin) => {
        const latestClassification = await Classification.findOne({ bin_id: bin._id }).sort({ timestamp: -1 });
        const activeAlerts = await Alert.find({ bin_id: bin._id, resolved: false });
        return {
          ...bin.toObject(),
          latest_classification: latestClassification ? latestClassification.label : null,
          active_alerts: activeAlerts.map(a => ({ type: a.alert_type, timestamp: a.timestamp }))
        };
      })
    );
    res.json(binsWithDetails);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch bins" });
  }
};

// GET bin by ID
export const getBinById = async (req, res) => {
  try {
    const bin = await Bin.findOne({ binId: req.params.id });
    if (!bin) return res.status(404).json({ error: "Bin not found" });

    const latestClassification = await Classification.findOne({ bin_id: bin._id }).sort({ timestamp: -1 });
    const activeAlerts = await Alert.find({ bin_id: bin._id, resolved: false });

    res.json({
      ...bin.toObject(),
      latest_classification: latestClassification ? latestClassification.label : null,
      active_alerts: activeAlerts.map(a => ({ type: a.alert_type, timestamp: a.timestamp }))
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to fetch bin" });
  }
};

// POST new bin
export const createBin = async (req, res) => {
  try {
    const newBin = new Bin(req.body);
    await newBin.save();
    res.status(201).json({ message: "Bin created successfully", bin: newBin });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to create bin" });
  }
};

// PUT / PATCH bin
export const updateBin = async (req, res) => {
  try {
    const updatedBin = await Bin.findOneAndUpdate({ binId: req.params.id }, req.body, { new: true });
    if (!updatedBin) return res.status(404).json({ error: "Bin not found" });
    res.json({ message: "Bin updated successfully", bin: updatedBin });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to update bin" });
  }
};

// DELETE (soft-delete)
export const deleteBin = async (req, res) => {
  try {
    const deletedBin = await Bin.findOneAndUpdate(
      { binId: req.params.id },
      { isDeleted: true },
      { new: true }
    );
    if (!deletedBin) return res.status(404).json({ error: "Bin not found" });
    res.json({ message: "Bin soft-deleted", bin: deletedBin });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Failed to delete bin" });
  }
};

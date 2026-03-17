const express = require('express');
const { authenticateToken } = require('./auth');
const router = express.Router();

// Get user statistics
router.get('/user', authenticateToken, async (req, res) => {
  try {
    const db = req.app.locals.db;
    const days = parseInt(req.query.days) || 30;

    const stats = await db.getUserStatistics(req.user.userId, days);
    const user = await db.getUserById(req.user.userId);

    // Calculate totals
    const totals = stats.reduce((acc, day) => {
      acc.scans += day.scans_count;
      acc.detections += day.detections_count;
      acc.clean += day.clean_count;
      return acc;
    }, { scans: 0, detections: 0, clean: 0 });

    res.json({
      period: `${days} days`,
      total_scans: totals.scans,
      total_detections: totals.detections,
      total_clean: totals.clean,
      detection_rate: totals.scans > 0 ? (totals.detections / totals.scans * 100).toFixed(2) : 0,
      daily_stats: stats,
      user_info: {
        plan_type: user.plan_type,
        scans_remaining: user.scans_remaining,
        total_scans: user.total_scans,
        balance: user.balance
      }
    });
  } catch (error) {
    console.error('Stats error:', error);
    res.status(500).json({ error: 'Failed to fetch statistics' });
  }
});

// Get detection trends
router.get('/trends', authenticateToken, async (req, res) => {
  try {
    const db = req.app.locals.db;
    const stats = await db.getUserStatistics(req.user.userId, 30);

    const trends = stats.map(day => ({
      date: day.date,
      scans: day.scans_count,
      detections: day.detections_count,
      clean: day.clean_count,
      detection_rate: day.scans_count > 0 ? (day.detections_count / day.scans_count * 100).toFixed(2) : 0
    }));

    res.json(trends);
  } catch (error) {
    console.error('Trends error:', error);
    res.status(500).json({ error: 'Failed to fetch trends' });
  }
});

module.exports = router;

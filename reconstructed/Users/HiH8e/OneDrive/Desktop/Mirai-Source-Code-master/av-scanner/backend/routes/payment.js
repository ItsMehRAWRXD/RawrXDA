const express = require('express');
const { authenticateToken } = require('./auth');
const router = express.Router();

// Get pricing plans
router.get('/plans', (req, res) => {
  const discount = parseFloat(process.env.DISCOUNT_RATE) || 0;

  const plans = [
    {
      name: 'Basic',
      type: 'pay-per-scan',
      price: parseFloat(process.env.BASIC_PRICE_PER_SCAN) || 0.30,
      discounted_price: (parseFloat(process.env.BASIC_PRICE_PER_SCAN) || 0.30) * (1 - discount),
      scans: 1,
      description: 'Pay as you go'
    },
    {
      name: 'Personal',
      type: 'monthly',
      price: parseFloat(process.env.PERSONAL_PLAN_PRICE) || 60,
      discounted_price: (parseFloat(process.env.PERSONAL_PLAN_PRICE) || 60) * (1 - discount),
      scans: parseInt(process.env.PERSONAL_PLAN_SCANS) || 200,
      description: '200 scans per month'
    },
    {
      name: 'Professional',
      type: 'monthly',
      price: parseFloat(process.env.PROFESSIONAL_PLAN_PRICE) || 180,
      discounted_price: (parseFloat(process.env.PROFESSIONAL_PLAN_PRICE) || 180) * (1 - discount),
      scans: parseInt(process.env.PROFESSIONAL_PLAN_SCANS) || 600,
      description: '600 scans per month'
    },
    {
      name: 'Enterprise',
      type: 'monthly',
      price: parseFloat(process.env.ENTERPRISE_PLAN_PRICE) || 360,
      discounted_price: (parseFloat(process.env.ENTERPRISE_PLAN_PRICE) || 360) * (1 - discount),
      scans: parseInt(process.env.ENTERPRISE_PLAN_SCANS) || 1200,
      description: '1200 scans per month'
    }
  ];

  res.json({
    plans,
    discount_percentage: (discount * 100).toFixed(0) + '%'
  });
});

// Purchase scans (basic plan)
router.post('/purchase', authenticateToken, async (req, res) => {
  try {
    const { quantity } = req.body;
    const db = req.app.locals.db;

    if (!quantity || quantity < 1) {
      return res.status(400).json({ error: 'Invalid quantity' });
    }

    const pricePerScan = parseFloat(process.env.BASIC_PRICE_PER_SCAN) || 0.30;
    const discount = parseFloat(process.env.DISCOUNT_RATE) || 0;
    const totalPrice = quantity * pricePerScan * (1 - discount);

    // In production, integrate with payment processor (Stripe, PayPal, etc.)
    // For now, simulate successful payment

    // Add scans to user account
    await db.updateUserScans(req.user.userId, quantity);

    // Record transaction
    await db.createTransaction({
      user_id: req.user.userId,
      transaction_type: 'purchase',
      amount: totalPrice,
      description: `Purchased ${quantity} scan(s)`,
      scans_added: quantity
    });

    res.json({
      message: 'Purchase successful',
      scans_added: quantity,
      total_cost: totalPrice.toFixed(2)
    });
  } catch (error) {
    console.error('Purchase error:', error);
    res.status(500).json({ error: 'Purchase failed' });
  }
});

// Subscribe to plan
router.post('/subscribe', authenticateToken, async (req, res) => {
  try {
    const { plan_type } = req.body;
    const db = req.app.locals.db;

    const planConfig = {
      personal: {
        scans: parseInt(process.env.PERSONAL_PLAN_SCANS) || 200,
        price: parseFloat(process.env.PERSONAL_PLAN_PRICE) || 60
      },
      professional: {
        scans: parseInt(process.env.PROFESSIONAL_PLAN_SCANS) || 600,
        price: parseFloat(process.env.PROFESSIONAL_PLAN_PRICE) || 180
      },
      enterprise: {
        scans: parseInt(process.env.ENTERPRISE_PLAN_SCANS) || 1200,
        price: parseFloat(process.env.ENTERPRISE_PLAN_PRICE) || 360
      }
    };

    if (!planConfig[plan_type]) {
      return res.status(400).json({ error: 'Invalid plan type' });
    }

    const discount = parseFloat(process.env.DISCOUNT_RATE) || 0;
    const totalPrice = planConfig[plan_type].price * (1 - discount);

    // In production, set up recurring payment with payment processor

    // Update user plan and add scans
    await db.updateUserScans(req.user.userId, planConfig[plan_type].scans);

    // Update user plan type (you'll need to add this method to Database class)
    const user = await db.getUserById(req.user.userId);

    // Record transaction
    await db.createTransaction({
      user_id: req.user.userId,
      transaction_type: 'subscription',
      amount: totalPrice,
      description: `Subscribed to ${plan_type} plan`,
      scans_added: planConfig[plan_type].scans
    });

    res.json({
      message: 'Subscription successful',
      plan: plan_type,
      scans_added: planConfig[plan_type].scans,
      monthly_cost: totalPrice.toFixed(2)
    });
  } catch (error) {
    console.error('Subscribe error:', error);
    res.status(500).json({ error: 'Subscription failed' });
  }
});

// Add balance
router.post('/add-balance', authenticateToken, async (req, res) => {
  try {
    const { amount } = req.body;
    const db = req.app.locals.db;

    if (!amount || amount <= 0) {
      return res.status(400).json({ error: 'Invalid amount' });
    }

    // In production, process payment first

    await db.updateUserBalance(req.user.userId, amount);

    await db.createTransaction({
      user_id: req.user.userId,
      transaction_type: 'deposit',
      amount: amount,
      description: 'Balance added',
      scans_added: 0
    });

    res.json({
      message: 'Balance added successfully',
      amount: amount
    });
  } catch (error) {
    console.error('Add balance error:', error);
    res.status(500).json({ error: 'Failed to add balance' });
  }
});

module.exports = router;

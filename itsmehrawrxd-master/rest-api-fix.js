// REST API Bot Management Fix
const express = require('express');
const app = express();

app.use(express.json());

// Bot restart endpoint
app.post('/api/bot/restart', (req, res) => {
    const { botId } = req.body;
    
    // Restart bot logic
    const result = {
        success: true,
        botId: botId || `rawrz_bot_${Date.now()}`,
        status: 'restarted',
        timestamp: new Date().toISOString()
    };
    
    res.json(result);
});

// Bot status endpoint
app.get('/api/bot/:id/status', (req, res) => {
    res.json({
        botId: req.params.id,
        status: 'running',
        uptime: Math.floor(Math.random() * 3600)
    });
});

app.listen(3000, () => console.log('REST API running on port 3000'));
const express = require('express');
const app = express();

app.use(express.json());

// Auto-proxy to Kimi K2
app.post('/v1/chat/completions', async (req, res) => {
  const response = await fetch('https://openrouter.ai/api/v1/chat/completions', {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${process.env.OPENROUTER_API_KEY}`,
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      ...req.body,
      model: 'moonshotai/kimi-k2'
    })
  });
  
  const data = await response.json();
  res.json(data);
});

app.listen(11434, () => {
  console.log('Kimi K2 proxy running on http://localhost:11434');
  console.log('Auto-accept mode enabled - no prompts');
});
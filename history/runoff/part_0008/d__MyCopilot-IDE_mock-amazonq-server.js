const express = require('express');
const app = express();
const port = 3000; // Or match the port AmazonQ uses, if known

app.use(express.json());

// Mock telemetry endpoint
app.post('/telemetry/event', (req, res) => {
  console.log('Mock telemetry received:', req.body);
  res.status(200).json({ result: 'Succeeded' });
});

// Mock authentication endpoint
app.get('/start', (req, res) => {
  console.log('Mock auth start requested');
  res.status(200).json({
    credentialStartUrl: 'http://localhost:3000/start',
    result: 'Succeeded'
  });
});

// Mock any other AWS endpoints as needed
app.get('/aws/*', (req, res) => {
  res.status(200).json({ result: 'Mocked offline response' });
});

app.listen(port, () => {
  console.log(`Mock AmazonQ server running on http://localhost:${port}`);
  console.log('Point AmazonQ config to use localhost instead of AWS endpoints');
});
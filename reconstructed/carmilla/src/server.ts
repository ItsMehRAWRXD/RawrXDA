import express from 'express';
import cors from 'cors';
import path from 'path';
import encryptionRouter from './api/encryption';

const app = express();
const PORT = process.env['PORT'] || 3000;

// Middleware
app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));

// API Routes
app.use('/api', encryptionRouter);

// Serve static files (for web UI)
app.use(express.static(path.join(__dirname, '../public')));

// Serve React app (if public/index.html exists)
app.get('*', (_req, res) => {
  const indexPath = path.join(__dirname, '../public/index.html');
  if (require('fs').existsSync(indexPath)) {
    res.sendFile(indexPath);
  } else {
    res.json({
      message: 'Carmilla Encryption System API',
      docs: '/api/health',
      note: 'Web UI not built. Use the REST API endpoints.',
    });
  }
});

// Error handling middleware
app.use((err: any, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
  console.error('Error:', err);
  res.status(500).json({
    error: 'Internal server error',
    message: err.message,
    timestamp: new Date().toISOString()
  });
});

// Start server
app.listen(PORT, () => {
  console.log(`🚀 Carmilla Encryption Server running on port ${PORT}`);
  console.log(`📖 API Documentation: http://localhost:${PORT}/api/health`);
  console.log(`🌐 Web UI: http://localhost:${PORT}`);
  console.log(`🔧 Features: Encryption, Decryption, Car(); Patching, Selective Encryption`);
});

export default app;
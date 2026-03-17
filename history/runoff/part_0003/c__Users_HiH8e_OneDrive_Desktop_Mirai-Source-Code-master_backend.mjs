import express from 'express';
import cors from 'cors';
import fs from 'fs/promises';
import path from 'path';

const app = express();
const PORT = 9000;

app.use(cors());
app.use(express.json());

// Health check
app.get('/api/health', (req, res) => {
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString(),
    version: '1.0.0'
  });
});

// File operations
app.get('/api/files', async (req, res) => {
  try {
    const dirPath = req.query.path || process.cwd();
    const files = await fs.readdir(dirPath, { withFileTypes: true });

    const fileList = files.map(file => ({
      name: file.name,
      type: file.isDirectory() ? 'directory' : 'file',
      path: path.join(dirPath, file.name)
    }));

    res.json({ files: fileList });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

app.get('/api/file', async (req, res) => {
  try {
    const filePath = req.query.path;
    if (!filePath) {
      return res.status(400).json({ error: 'Path required' });
    }

    const content = await fs.readFile(filePath, 'utf-8');
    res.json({ content });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

app.post('/api/file', async (req, res) => {
  try {
    const { path: filePath, content } = req.body;
    if (!filePath) {
      return res.status(400).json({ error: 'Path required' });
    }

    await fs.writeFile(filePath, content, 'utf-8');
    res.json({ success: true });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

app.listen(PORT, () => {
  console.log(`🔧 Backend API Server running on http://localhost:${PORT}`);
});

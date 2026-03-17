const express = require('express');
const multer = require('multer');
const path = require('path');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
require('dotenv').config();

const Database = require('../database/db');
const AVScannerEngine = require('./scanner-engine');
const TelegramBotService = require('./telegram-bot');
const authRoutes = require('./routes/auth');
const scanRoutes = require('./routes/scan');
const statsRoutes = require('./routes/stats');
const paymentRoutes = require('./routes/payment');
const pdfRoutes = require('./routes/pdf');

const app = express();
const db = new Database();
const scanner = new AVScannerEngine();
const telegramBot = new TelegramBotService(db, scanner);

// Security middleware
app.use(helmet());
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Rate limiting
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 100 // limit each IP to 100 requests per windowMs
});
app.use(limiter);

// File upload configuration
const storage = multer.diskStorage({
  destination: (req, file, cb) => {
    cb(null, process.env.UPLOAD_DIR || './uploads');
  },
  filename: (req, file, cb) => {
    const uniqueSuffix = Date.now() + '-' + Math.round(Math.random() * 1E9);
    cb(null, uniqueSuffix + path.extname(file.originalname));
  }
});

const upload = multer({
  storage: storage,
  limits: {
    fileSize: parseInt(process.env.MAX_FILE_SIZE) || 52428800 // 50MB default
  }
});

// Make dependencies available to routes
app.locals.db = db;
app.locals.scanner = scanner;
app.locals.upload = upload;

// Routes
app.use('/api/auth', authRoutes);
app.use('/api/scan', scanRoutes);
app.use('/api/stats', statsRoutes);
app.use('/api/payment', paymentRoutes);
app.use('/api/pdf', pdfRoutes);

// Serve frontend
app.use(express.static(path.join(__dirname, '../frontend')));

// Health check
app.get('/api/health', (req, res) => {
  res.json({ status: 'OK', timestamp: new Date().toISOString() });
});

// Error handler
app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).json({
    error: 'Something went wrong!',
    message: process.env.NODE_ENV === 'development' ? err.message : undefined
  });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`AV Scanner Server running on port ${PORT}`);
  console.log(`Environment: ${process.env.NODE_ENV}`);

  // Start Telegram bot
  telegramBot.start().catch(console.error);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\nShutting down gracefully...');
  telegramBot.stop();
  db.close();
  process.exit(0);
});

module.exports = app;

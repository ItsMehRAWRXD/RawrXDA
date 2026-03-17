const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');

const dbPath = path.join(__dirname, 'av_scanner.db');

// Create database
const db = new sqlite3.Database(dbPath, (err) => {
  if (err) {
    console.error('Error opening database:', err.message);
  } else {
    console.log('Connected to SQLite database.');
  }
});

// Initialize database schema
db.serialize(() => {
  // Users table
  db.run(`CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    display_name TEXT,
    plan_type TEXT DEFAULT 'basic',
    scans_remaining INTEGER DEFAULT 0,
    total_scans INTEGER DEFAULT 0,
    balance REAL DEFAULT 0.0,
    theme TEXT DEFAULT 'dark',
    font_choice TEXT DEFAULT 'default',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_login DATETIME,
    subscription_expires DATETIME
  )`);

  // Scans table
  db.run(`CREATE TABLE IF NOT EXISTS scans (
    id TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL,
    filename TEXT NOT NULL,
    file_hash TEXT NOT NULL,
    file_size INTEGER NOT NULL,
    file_type TEXT,
    scan_status TEXT DEFAULT 'pending',
    detection_count INTEGER DEFAULT 0,
    total_engines INTEGER DEFAULT 27,
    scan_results TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    completed_at DATETIME,
    FOREIGN KEY (user_id) REFERENCES users (id)
  )`);

  // Scan results detail table
  db.run(`CREATE TABLE IF NOT EXISTS scan_results_detail (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    scan_id TEXT NOT NULL,
    engine_name TEXT NOT NULL,
    engine_version TEXT,
    detected BOOLEAN DEFAULT 0,
    detection_name TEXT,
    scan_time INTEGER,
    FOREIGN KEY (scan_id) REFERENCES scans (id)
  )`);

  // Statistics table
  db.run(`CREATE TABLE IF NOT EXISTS statistics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    date DATE NOT NULL,
    scans_count INTEGER DEFAULT 0,
    detections_count INTEGER DEFAULT 0,
    clean_count INTEGER DEFAULT 0,
    FOREIGN KEY (user_id) REFERENCES users (id),
    UNIQUE(user_id, date)
  )`);

  // Transactions table
  db.run(`CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    transaction_type TEXT NOT NULL,
    amount REAL NOT NULL,
    description TEXT,
    scans_added INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users (id)
  )`);

  // API keys for AV engines (stored securely)
  db.run(`CREATE TABLE IF NOT EXISTS av_engine_config (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    engine_name TEXT UNIQUE NOT NULL,
    api_endpoint TEXT,
    api_key TEXT,
    enabled BOOLEAN DEFAULT 1,
    priority INTEGER DEFAULT 0
  )`);

  console.log('Database schema initialized successfully.');
});

module.exports = db;

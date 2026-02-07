'use strict';

const sqlite3 = require('sqlite3').verbose();
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');

// Real Database Management Engine with SQLite
class DatabaseEngine {
  constructor() {
    this.id = 'database';
    this.name = 'Database Management';
    this.version = '1.0.0';
    this.icon = '🗄️';
    this.group = 'Data';
    
    // Self-sufficiency detection
    this.selfSufficient = false; // Requires sqlite3 dependency
    this.externalDependencies = ['sqlite3'];
    this.requiredDependencies = ['sqlite3'];
    
    this.db = null;
    this.dbPath = path.join(__dirname, '../../data/database.db');
    this.initialized = false;
    
    this.actions = [
      {
        method: 'POST',
        path: '/api/db/query',
        summary: 'Execute SQL Query (Read-Only)',
        inputs: [
          { name: 'query', type: 'textarea', required: true, help: 'SQL SELECT query only', maxLength: 10000 },
          { name: 'limit', type: 'number', required: false, help: 'Maximum rows to return', placeholder: '100' }
        ]
      },
      {
        method: 'POST',
        path: '/api/db/execute',
        summary: 'Execute SQL Command (Write)',
        inputs: [
          { name: 'query', type: 'textarea', required: true, help: 'SQL INSERT/UPDATE/DELETE query', maxLength: 10000 }
        ]
      },
      {
        method: 'GET',
        path: '/api/db/tables',
        summary: 'List Database Tables',
        inputs: []
      },
      {
        method: 'GET',
        path: '/api/db/schema',
        summary: 'Get Table Schema',
        inputs: [
          { name: 'table', type: 'text', required: true, help: 'Table name to inspect' }
        ]
      },
      {
        method: 'GET',
        path: '/api/db/stats',
        summary: 'Get Database Statistics',
        inputs: []
      },
      {
        method: 'POST',
        path: '/api/db/backup',
        summary: 'Create Database Backup',
        inputs: [
          { name: 'backupName', type: 'text', required: false, help: 'Backup filename (optional)' }
        ]
      }
    ];
  }

  async initialize() {
    if (this.initialized) return true;
    
    try {
      // Ensure data directory exists
      const dataDir = path.dirname(this.dbPath);
      await fs.mkdir(dataDir, { recursive: true });
      
      // Initialize SQLite database
      this.db = new sqlite3.Database(this.dbPath, (err) => {
        if (err) {
          console.error('Error opening database:', err.message);
          throw err;
        }
        console.log('Connected to SQLite database');
      });
      
      // Create sample tables if they don't exist
      await this.createSampleTables();
      
      this.initialized = true;
      return true;
    } catch (error) {
      console.error('Database initialization failed:', error);
      throw error;
    }
  }

  async createSampleTables() {
    const tables = [
      `CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        email TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
      )`,
      
      `CREATE TABLE IF NOT EXISTS sessions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        session_token TEXT UNIQUE NOT NULL,
        expires_at DATETIME NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users (id)
      )`,
      
      `CREATE TABLE IF NOT EXISTS logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        level TEXT NOT NULL,
        message TEXT NOT NULL,
        metadata TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
      )`,
      
      `CREATE TABLE IF NOT EXISTS files (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        filename TEXT NOT NULL,
        original_name TEXT NOT NULL,
        size INTEGER NOT NULL,
        mime_type TEXT,
        hash TEXT,
        uploaded_by INTEGER,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (uploaded_by) REFERENCES users (id)
      )`
    ];

    for (const table of tables) {
      await this.executeQuery(table);
    }
    
    // Insert sample data
    await this.insertSampleData();
  }

  async insertSampleData() {
    const sampleData = [
      `INSERT OR IGNORE INTO users (username, email, password_hash) VALUES 
        ('admin', 'admin@example.com', '${crypto.createHash('sha256').update('admin123').digest('hex')}'),
        ('user1', 'user1@example.com', '${crypto.createHash('sha256').update('user123').digest('hex')}'),
        ('user2', 'user2@example.com', '${crypto.createHash('sha256').update('user456').digest('hex')}')`,
      
      `INSERT OR IGNORE INTO logs (level, message, metadata) VALUES 
        ('INFO', 'System initialized', '{"component": "database"}'),
        ('WARN', 'High memory usage detected', '{"memory": "85%"}'),
        ('ERROR', 'Connection timeout', '{"timeout": 30000}')`,
      
      `INSERT OR IGNORE INTO files (filename, original_name, size, mime_type, hash) VALUES 
        ('sample1.txt', 'sample1.txt', 1024, 'text/plain', '${crypto.createHash('sha256').update('sample content').digest('hex')}'),
        ('sample2.pdf', 'sample2.pdf', 2048, 'application/pdf', '${crypto.createHash('sha256').update('pdf content').digest('hex')}')`
    ];

    for (const query of sampleData) {
      await this.executeQuery(query);
    }
  }

  async executeQuery(query, params = []) {
    return new Promise((resolve, reject) => {
      this.db.all(query, params, (err, rows) => {
        if (err) {
          reject(err);
        } else {
          resolve(rows);
        }
      });
    });
  }

  async executeCommand(query, params = []) {
    return new Promise((resolve, reject) => {
      this.db.run(query, params, function(err) {
        if (err) {
          reject(err);
        } else {
          resolve({
            lastID: this.lastID,
            changes: this.changes
          });
        }
      });
    });
  }

  async getTables() {
    const query = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'";
    return await this.executeQuery(query);
  }

  async getTableSchema(tableName) {
    const query = `PRAGMA table_info(${tableName})`;
    return await this.executeQuery(query);
  }

  async getDatabaseStats() {
    const tables = await this.getTables();
    const stats = {
      tables: tables.length,
      tableDetails: []
    };

    for (const table of tables) {
      const countQuery = `SELECT COUNT(*) as count FROM ${table.name}`;
      const countResult = await this.executeQuery(countQuery);
      const count = countResult[0].count;
      
      const schema = await this.getTableSchema(table.name);
      
      stats.tableDetails.push({
        name: table.name,
        rowCount: count,
        columns: schema.length,
        schema: schema
      });
    }

    return stats;
  }

  async createBackup(backupName = null) {
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const backupFileName = backupName || `backup_${timestamp}.db`;
    const backupPath = path.join(path.dirname(this.dbPath), 'backups', backupFileName);
    
    // Ensure backup directory exists
    const backupDir = path.dirname(backupPath);
    await fs.mkdir(backupDir, { recursive: true });
    
    // Copy database file
    await fs.copyFile(this.dbPath, backupPath);
    
    return {
      backupPath,
      backupFileName,
      size: (await fs.stat(backupPath)).size,
      created: new Date().toISOString()
    };
  }

  // Method to check if engine is self-sufficient
  isSelfSufficient() {
    return this.selfSufficient;
  }

  // Method to get dependency information
  getDependencyInfo() {
    return {
      selfSufficient: this.selfSufficient,
      externalDependencies: this.externalDependencies,
      requiredDependencies: this.requiredDependencies,
      hasExternalDependencies: this.externalDependencies.length > 0,
      hasRequiredDependencies: this.requiredDependencies.length > 0
    };
  }

  // Close database connection
  async close() {
    if (this.db) {
      return new Promise((resolve) => {
        this.db.close((err) => {
          if (err) {
            console.error('Error closing database:', err.message);
          } else {
            console.log('Database connection closed');
          }
          resolve();
        });
      });
    }
  }
}

module.exports = new DatabaseEngine();

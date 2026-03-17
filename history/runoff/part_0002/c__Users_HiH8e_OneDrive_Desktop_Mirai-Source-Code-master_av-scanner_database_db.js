const sqlite3 = require('sqlite3').verbose();
const path = require('path');

class Database {
  constructor() {
    const dbPath = process.env.DB_PATH || path.join(__dirname, 'av_scanner.db');
    this.db = new sqlite3.Database(dbPath);
  }

  // User operations
  createUser(userData) {
    return new Promise((resolve, reject) => {
      const { username, email, password_hash, display_name } = userData;
      this.db.run(
        `INSERT INTO users (username, email, password_hash, display_name) VALUES (?, ?, ?, ?)`,
        [username, email, password_hash, display_name || username],
        function (err) {
          if (err) reject(err);
          else resolve({ id: this.lastID });
        }
      );
    });
  }

  getUserByUsername(username) {
    return new Promise((resolve, reject) => {
      this.db.get(`SELECT * FROM users WHERE username = ?`, [username], (err, row) => {
        if (err) reject(err);
        else resolve(row);
      });
    });
  }

  getUserById(id) {
    return new Promise((resolve, reject) => {
      this.db.get(`SELECT * FROM users WHERE id = ?`, [id], (err, row) => {
        if (err) reject(err);
        else resolve(row);
      });
    });
  }

  updateUserScans(userId, scansToAdd) {
    return new Promise((resolve, reject) => {
      this.db.run(
        `UPDATE users SET scans_remaining = scans_remaining + ? WHERE id = ?`,
        [scansToAdd, userId],
        (err) => {
          if (err) reject(err);
          else resolve();
        }
      );
    });
  }

  decrementUserScans(userId) {
    return new Promise((resolve, reject) => {
      this.db.run(
        `UPDATE users SET scans_remaining = scans_remaining - 1, total_scans = total_scans + 1 WHERE id = ?`,
        [userId],
        (err) => {
          if (err) reject(err);
          else resolve();
        }
      );
    });
  }

  updateUserBalance(userId, amount) {
    return new Promise((resolve, reject) => {
      this.db.run(
        `UPDATE users SET balance = balance + ? WHERE id = ?`,
        [amount, userId],
        (err) => {
          if (err) reject(err);
          else resolve();
        }
      );
    });
  }

  // Scan operations
  createScan(scanData) {
    return new Promise((resolve, reject) => {
      const { id, user_id, filename, file_hash, file_size, file_type } = scanData;
      this.db.run(
        `INSERT INTO scans (id, user_id, filename, file_hash, file_size, file_type) 
         VALUES (?, ?, ?, ?, ?, ?)`,
        [id, user_id, filename, file_hash, file_size, file_type],
        (err) => {
          if (err) reject(err);
          else resolve({ id });
        }
      );
    });
  }

  updateScanStatus(scanId, status, detectionCount, results) {
    return new Promise((resolve, reject) => {
      this.db.run(
        `UPDATE scans SET scan_status = ?, detection_count = ?, scan_results = ?, completed_at = CURRENT_TIMESTAMP 
         WHERE id = ?`,
        [status, detectionCount, JSON.stringify(results), scanId],
        (err) => {
          if (err) reject(err);
          else resolve();
        }
      );
    });
  }

  getScanById(scanId) {
    return new Promise((resolve, reject) => {
      this.db.get(`SELECT * FROM scans WHERE id = ?`, [scanId], (err, row) => {
        if (err) reject(err);
        else {
          if (row && row.scan_results) {
            row.scan_results = JSON.parse(row.scan_results);
          }
          resolve(row);
        }
      });
    });
  }

  getUserScans(userId, limit = 50) {
    return new Promise((resolve, reject) => {
      this.db.all(
        `SELECT * FROM scans WHERE user_id = ? ORDER BY created_at DESC LIMIT ?`,
        [userId, limit],
        (err, rows) => {
          if (err) reject(err);
          else resolve(rows);
        }
      );
    });
  }

  // Statistics operations
  updateStatistics(userId, date, detected) {
    return new Promise((resolve, reject) => {
      this.db.run(
        `INSERT INTO statistics (user_id, date, scans_count, detections_count, clean_count)
         VALUES (?, ?, 1, ?, ?)
         ON CONFLICT(user_id, date) DO UPDATE SET
         scans_count = scans_count + 1,
         detections_count = detections_count + ?,
         clean_count = clean_count + ?`,
        [userId, date, detected ? 1 : 0, detected ? 0 : 1, detected ? 1 : 0, detected ? 0 : 1],
        (err) => {
          if (err) reject(err);
          else resolve();
        }
      );
    });
  }

  getUserStatistics(userId, days = 30) {
    return new Promise((resolve, reject) => {
      this.db.all(
        `SELECT * FROM statistics WHERE user_id = ? 
         AND date >= date('now', '-' || ? || ' days')
         ORDER BY date DESC`,
        [userId, days],
        (err, rows) => {
          if (err) reject(err);
          else resolve(rows);
        }
      );
    });
  }

  // Transaction operations
  createTransaction(transactionData) {
    return new Promise((resolve, reject) => {
      const { user_id, transaction_type, amount, description, scans_added } = transactionData;
      this.db.run(
        `INSERT INTO transactions (user_id, transaction_type, amount, description, scans_added)
         VALUES (?, ?, ?, ?, ?)`,
        [user_id, transaction_type, amount, description, scans_added],
        function (err) {
          if (err) reject(err);
          else resolve({ id: this.lastID });
        }
      );
    });
  }

  close() {
    this.db.close();
  }
}

module.exports = Database;

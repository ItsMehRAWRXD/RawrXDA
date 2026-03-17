"""
Auto-Crypt Panel
Web-based panel for automatic file crypting
Features: Batch processing, API endpoints, queue management
"""

from flask import Flask, request, jsonify, render_template_string, send_file
from flask_cors import CORS
import os
import sys
import hashlib
import sqlite3
import threading
import queue
import time
from pathlib import Path
from datetime import datetime
import uuid

# Import our crypter
sys.path.append(os.path.dirname(__file__))
from fud_crypter import FUDCrypter

app = Flask(__name__)
CORS(app)

# Configuration
UPLOAD_FOLDER = "panel_uploads"
CRYPTED_FOLDER = "panel_output"
DATABASE = "crypt_panel.db"

Path(UPLOAD_FOLDER).mkdir(exist_ok=True)
Path(CRYPTED_FOLDER).mkdir(exist_ok=True)

# Job queue for batch processing
crypt_queue = queue.Queue()
crypter = FUDCrypter()

# Initialize database
def init_db():
    """Initialize SQLite database"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    
    c.execute('''CREATE TABLE IF NOT EXISTS crypt_jobs
                 (job_id TEXT PRIMARY KEY,
                  original_file TEXT,
                  output_format TEXT,
                  status TEXT,
                  created_at TEXT,
                  completed_at TEXT,
                  output_file TEXT,
                  file_hash TEXT,
                  fud_score INTEGER)''')
    
    c.execute('''CREATE TABLE IF NOT EXISTS api_keys
                 (api_key TEXT PRIMARY KEY,
                  user_id TEXT,
                  created_at TEXT,
                  requests_used INTEGER DEFAULT 0,
                  rate_limit INTEGER DEFAULT 100)''')
    
    conn.commit()
    conn.close()

init_db()

def generate_api_key() -> str:
    """Generate API key"""
    return str(uuid.uuid4())

def verify_api_key(key: str) -> bool:
    """Verify API key"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT * FROM api_keys WHERE api_key=?", (key,))
    result = c.fetchone()
    conn.close()
    return result is not None

def increment_api_usage(key: str):
    """Increment API usage count"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("UPDATE api_keys SET requests_used = requests_used + 1 WHERE api_key=?", (key,))
    conn.commit()
    conn.close()

def save_job(job_id: str, original_file: str, output_format: str):
    """Save crypt job to database"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("""INSERT INTO crypt_jobs VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
              (job_id, original_file, output_format, "pending", 
               datetime.now().isoformat(), None, None, None, None))
    conn.commit()
    conn.close()

def update_job(job_id: str, status: str, output_file: str = None, 
               file_hash: str = None, fud_score: int = None):
    """Update job status"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("""UPDATE crypt_jobs SET status=?, completed_at=?, output_file=?, file_hash=?, fud_score=? 
                 WHERE job_id=?""",
              (status, datetime.now().isoformat(), output_file, file_hash, fud_score, job_id))
    conn.commit()
    conn.close()

def get_job(job_id: str) -> dict:
    """Get job details"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT * FROM crypt_jobs WHERE job_id=?", (job_id,))
    result = c.fetchone()
    conn.close()
    
    if result:
        return {
            "job_id": result[0],
            "original_file": result[1],
            "output_format": result[2],
            "status": result[3],
            "created_at": result[4],
            "completed_at": result[5],
            "output_file": result[6],
            "file_hash": result[7],
            "fud_score": result[8]
        }
    return None

# Background worker thread
def crypt_worker():
    """Background worker for processing crypt jobs"""
    while True:
        try:
            job = crypt_queue.get()
            
            if job is None:
                break
            
            job_id = job['job_id']
            input_file = job['input_file']
            output_format = job['output_format']
            
            print(f"[Worker] Processing job {job_id}")
            
            # Perform crypting
            try:
                output_file = crypter.crypt_file(input_file, output_format)
                
                if output_file:
                    # Calculate hash
                    with open(output_file, 'rb') as f:
                        file_hash = hashlib.sha256(f.read()).hexdigest()
                    
                    # Check FUD score
                    fud_check = crypter.scan_check(output_file)
                    fud_score = fud_check['fud_score']
                    
                    # Update job
                    update_job(job_id, "completed", output_file, file_hash, fud_score)
                    print(f"[Worker] Job {job_id} completed (FUD: {fud_score}/100)")
                else:
                    update_job(job_id, "failed")
                    print(f"[Worker] Job {job_id} failed")
                    
            except Exception as e:
                update_job(job_id, "error")
                print(f"[Worker] Job {job_id} error: {e}")
            
            crypt_queue.task_done()
            
        except Exception as e:
            print(f"[Worker] Error: {e}")
            time.sleep(1)

# Start worker thread
worker_thread = threading.Thread(target=crypt_worker, daemon=True)
worker_thread.start()

# HTML Template
PANEL_HTML = '''
<!DOCTYPE html>
<html>
<head>
    <title>Auto-Crypt Panel</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { background: white; padding: 30px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .header h1 { color: #667eea; margin-bottom: 10px; }
        .card { background: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .card h2 { color: #333; margin-bottom: 15px; }
        .upload-area { border: 2px dashed #667eea; padding: 40px; text-align: center; border-radius: 8px; cursor: pointer; }
        .upload-area:hover { background: #f8f9ff; }
        input[type="file"] { display: none; }
        select, button { padding: 10px 20px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }
        button { background: linear-gradient(135deg, #667eea, #764ba2); color: white; border: none; cursor: pointer; margin: 5px; }
        button:hover { opacity: 0.9; }
        .job-list { margin-top: 20px; }
        .job-item { background: #f5f5f5; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #667eea; }
        .status-pending { border-left-color: #ffc107; }
        .status-completed { border-left-color: #28a745; }
        .status-failed { border-left-color: #dc3545; }
        .api-key { background: #f8f9fa; padding: 10px; border-radius: 5px; font-family: monospace; word-break: break-all; margin: 10px 0; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }
        .stat-box { background: #f8f9fa; padding: 20px; border-radius: 8px; text-align: center; }
        .stat-value { font-size: 2em; color: #667eea; font-weight: bold; }
        .stat-label { color: #666; margin-top: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔐 Auto-Crypt Panel</h1>
            <p>Automated file crypting with batch processing & API</p>
        </div>
        
        <div class="card">
            <h2>API Key Management</h2>
            <button onclick="generateApiKey()">Generate New API Key</button>
            <div id="api-key-display" class="api-key" style="display:none;"></div>
        </div>
        
        <div class="card">
            <h2>Upload & Crypt</h2>
            <div class="upload-area" onclick="document.getElementById('fileInput').click()">
                <h3>📁 Click to Upload File</h3>
                <p>Supported: .exe, .dll, .bin</p>
            </div>
            <input type="file" id="fileInput" onchange="uploadFile()">
            
            <div style="margin-top: 20px;">
                <label>Output Format:</label>
                <select id="outputFormat">
                    <option value="exe">EXE</option>
                    <option value="msi">MSI</option>
                    <option value="msix">MSIX</option>
                </select>
                <button onclick="startCrypt()">Start Crypting</button>
            </div>
        </div>
        
        <div class="card">
            <h2>Statistics</h2>
            <div class="stats">
                <div class="stat-box">
                    <div class="stat-value" id="total-jobs">0</div>
                    <div class="stat-label">Total Jobs</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="completed-jobs">0</div>
                    <div class="stat-label">Completed</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="pending-jobs">0</div>
                    <div class="stat-label">Pending</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="avg-fud">0%</div>
                    <div class="stat-label">Avg FUD Score</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>Crypt Jobs</h2>
            <button onclick="refreshJobs()">Refresh</button>
            <div id="job-list" class="job-list"></div>
        </div>
    </div>
    
    <script>
        const API_URL = 'http://localhost:5001';
        let currentFile = null;
        
        async function generateApiKey() {
            const response = await fetch(`${API_URL}/api/generate-key`, { method: 'POST' });
            const data = await response.json();
            
            document.getElementById('api-key-display').textContent = data.api_key;
            document.getElementById('api-key-display').style.display = 'block';
            localStorage.setItem('crypt_api_key', data.api_key);
            alert('API Key generated! Saved to localStorage.');
        }
        
        async function uploadFile() {
            const fileInput = document.getElementById('fileInput');
            currentFile = fileInput.files[0];
            alert(`File selected: ${currentFile.name}`);
        }
        
        async function startCrypt() {
            if (!currentFile) {
                alert('Please select a file first!');
                return;
            }
            
            const apiKey = localStorage.getItem('crypt_api_key');
            if (!apiKey) {
                alert('Please generate an API key first!');
                return;
            }
            
            const formData = new FormData();
            formData.append('file', currentFile);
            formData.append('output_format', document.getElementById('outputFormat').value);
            
            const response = await fetch(`${API_URL}/api/crypt`, {
                method: 'POST',
                headers: { 'X-API-Key': apiKey },
                body: formData
            });
            
            const data = await response.json();
            alert(`Job created: ${data.job_id}\\nStatus: ${data.status}`);
            refreshJobs();
        }
        
        async function refreshJobs() {
            const response = await fetch(`${API_URL}/api/jobs`);
            const data = await response.json();
            
            const jobList = document.getElementById('job-list');
            jobList.innerHTML = '';
            
            let totalJobs = data.jobs.length;
            let completedJobs = 0;
            let pendingJobs = 0;
            let totalFud = 0;
            let fudCount = 0;
            
            data.jobs.forEach(job => {
                const jobDiv = document.createElement('div');
                jobDiv.className = `job-item status-${job.status}`;
                jobDiv.innerHTML = `
                    <strong>Job ID:</strong> ${job.job_id}<br>
                    <strong>File:</strong> ${job.original_file}<br>
                    <strong>Format:</strong> ${job.output_format}<br>
                    <strong>Status:</strong> ${job.status}<br>
                    <strong>FUD Score:</strong> ${job.fud_score || 'N/A'}/100<br>
                    ${job.output_file ? `<button onclick="downloadFile('${job.job_id}')">Download</button>` : ''}
                `;
                jobList.appendChild(jobDiv);
                
                if (job.status === 'completed') {
                    completedJobs++;
                    if (job.fud_score) {
                        totalFud += job.fud_score;
                        fudCount++;
                    }
                } else if (job.status === 'pending') {
                    pendingJobs++;
                }
            });
            
            document.getElementById('total-jobs').textContent = totalJobs;
            document.getElementById('completed-jobs').textContent = completedJobs;
            document.getElementById('pending-jobs').textContent = pendingJobs;
            document.getElementById('avg-fud').textContent = fudCount > 0 ? Math.round(totalFud / fudCount) + '%' : '0%';
        }
        
        async function downloadFile(jobId) {
            window.location.href = `${API_URL}/api/download/${jobId}`;
        }
        
        // Auto-refresh every 5 seconds
        setInterval(refreshJobs, 5000);
        refreshJobs();
    </script>
</body>
</html>
'''

@app.route('/')
def index():
    """Main panel page"""
    return render_template_string(PANEL_HTML)

@app.route('/api/generate-key', methods=['POST'])
def api_generate_key():
    """Generate new API key"""
    api_key = generate_api_key()
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("INSERT INTO api_keys VALUES (?, ?, ?, ?, ?)",
              (api_key, "user_" + str(uuid.uuid4())[:8], datetime.now().isoformat(), 0, 100))
    conn.commit()
    conn.close()
    
    return jsonify({"success": True, "api_key": api_key})

@app.route('/api/crypt', methods=['POST'])
def api_crypt():
    """Upload and crypt file"""
    
    # Verify API key
    api_key = request.headers.get('X-API-Key')
    if not api_key or not verify_api_key(api_key):
        return jsonify({"success": False, "error": "Invalid API key"}), 401
    
    # Check file
    if 'file' not in request.files:
        return jsonify({"success": False, "error": "No file uploaded"}), 400
    
    file = request.files['file']
    output_format = request.form.get('output_format', 'exe')
    
    # Save uploaded file
    job_id = str(uuid.uuid4())
    upload_path = Path(UPLOAD_FOLDER) / f"{job_id}_{file.filename}"
    file.save(upload_path)
    
    # Create job
    save_job(job_id, file.filename, output_format)
    
    # Add to queue
    crypt_queue.put({
        'job_id': job_id,
        'input_file': str(upload_path),
        'output_format': output_format
    })
    
    # Increment API usage
    increment_api_usage(api_key)
    
    return jsonify({
        "success": True,
        "job_id": job_id,
        "status": "pending"
    })

@app.route('/api/jobs', methods=['GET'])
def api_list_jobs():
    """List all crypt jobs"""
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT * FROM crypt_jobs ORDER BY created_at DESC LIMIT 50")
    results = c.fetchall()
    conn.close()
    
    jobs = []
    for row in results:
        jobs.append({
            "job_id": row[0],
            "original_file": row[1],
            "output_format": row[2],
            "status": row[3],
            "created_at": row[4],
            "completed_at": row[5],
            "output_file": row[6],
            "file_hash": row[7],
            "fud_score": row[8]
        })
    
    return jsonify({"success": True, "jobs": jobs})

@app.route('/api/job/<job_id>', methods=['GET'])
def api_get_job(job_id: str):
    """Get job status"""
    
    job = get_job(job_id)
    
    if not job:
        return jsonify({"success": False, "error": "Job not found"}), 404
    
    return jsonify({"success": True, "job": job})

@app.route('/api/download/<job_id>', methods=['GET'])
def api_download(job_id: str):
    """Download crypted file"""
    
    job = get_job(job_id)
    
    if not job or not job['output_file']:
        return jsonify({"success": False, "error": "File not ready"}), 404
    
    return send_file(job['output_file'], as_attachment=True)

@app.route('/api/stats', methods=['GET'])
def api_stats():
    """Get statistics"""
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    
    c.execute("SELECT COUNT(*) FROM crypt_jobs")
    total = c.fetchone()[0]
    
    c.execute("SELECT COUNT(*) FROM crypt_jobs WHERE status='completed'")
    completed = c.fetchone()[0]
    
    c.execute("SELECT AVG(fud_score) FROM crypt_jobs WHERE fud_score IS NOT NULL")
    avg_fud = c.fetchone()[0] or 0
    
    conn.close()
    
    return jsonify({
        "total_jobs": total,
        "completed_jobs": completed,
        "average_fud_score": round(avg_fud, 2)
    })

if __name__ == '__main__':
    print("=" * 60)
    print("Auto-Crypt Panel")
    print("=" * 60)
    print()
    print("Starting server on http://localhost:5001")
    print()
    print("Features:")
    print("  - Web-based interface")
    print("  - Batch processing queue")
    print("  - REST API endpoints")
    print("  - API key authentication")
    print("  - Real-time status updates")
    print()
    
    app.run(host='0.0.0.0', port=5001, debug=True)

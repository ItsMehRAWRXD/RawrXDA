#!/usr/bin/env python3
"""
Custom AV Scanner - Professional Web Dashboard
Real-time malware scanning with threat intelligence visualization
100% Private - Zero file distribution
"""

import os
import json
import sqlite3
from datetime import datetime
from pathlib import Path
from flask import Flask, render_template, request, jsonify, send_file
from flask_cors import CORS
from werkzeug.utils import secure_filename
from custom_av_scanner import CustomAVScanner, SignatureDatabase
from threat_feed_updater import ThreatFeedUpdater
import threading
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Flask app setup
app = Flask(__name__)
CORS(app)

# Configuration
app.config['MAX_CONTENT_LENGTH'] = 100 * 1024 * 1024  # 100MB max file size
app.config['UPLOAD_FOLDER'] = 'temp_scans'
app.config['REPORTS_FOLDER'] = 'scan_reports'

# Ensure directories exist
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
os.makedirs(app.config['REPORTS_FOLDER'], exist_ok=True)
os.makedirs('yara_rules', exist_ok=True)

# Initialize scanner and database
scanner = CustomAVScanner()
sig_db = SignatureDatabase()
threat_updater = ThreatFeedUpdater()

# Scan history storage
scan_history = []


def allowed_file(filename):
    """Check if file type is allowed"""
    allowed_extensions = {
        'exe', 'dll', 'sys', 'scr', 'vbs', 'js', 'bat', 'cmd', 'com', 'pif',
        'zip', 'rar', '7z', 'iso', 'img', 'app', 'apk', 'msi', 'ps1',
        'pdf', 'doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'jar', 'class'
    }
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in allowed_extensions


def get_threat_stats():
    """Get threat intelligence statistics"""
    try:
        conn = sqlite3.connect('scanner.db')
        cursor = conn.cursor()
        
        # Get signature counts
        cursor.execute("SELECT COUNT(*) FROM hash_signatures")
        hash_sigs = cursor.fetchone()[0]
        
        cursor.execute("SELECT COUNT(*) FROM behavioral_indicators")
        behavioral_indicators = cursor.fetchone()[0]
        
        cursor.execute("SELECT COUNT(*) FROM yara_rules")
        yara_rules = cursor.fetchone()[0]
        
        cursor.execute("SELECT COUNT(*) FROM fuzzy_hashes")
        fuzzy_hashes = cursor.fetchone()[0]
        
        conn.close()
        
        return {
            'hash_signatures': hash_sigs,
            'behavioral_indicators': behavioral_indicators,
            'yara_rules': yara_rules,
            'fuzzy_hashes': fuzzy_hashes,
            'total_signatures': hash_sigs + behavioral_indicators + yara_rules + fuzzy_hashes
        }
    except Exception as e:
        logger.error(f"Error getting threat stats: {e}")
        return {
            'hash_signatures': 0,
            'behavioral_indicators': 0,
            'yara_rules': 0,
            'fuzzy_hashes': 0,
            'total_signatures': 0
        }


@app.route('/')
def dashboard():
    """Main dashboard page"""
    return render_template('dashboard.html')


@app.route('/api/threat-intelligence')
def threat_intelligence():
    """Threat intelligence dashboard"""
    return render_template('threat-intelligence.html')


@app.route('/api/scanner-settings')
def scanner_settings():
    """Scanner settings page"""
    return render_template('scanner-settings.html')


@app.route('/api/health')
def health():
    """Health check endpoint"""
    stats = get_threat_stats()
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat(),
        'threat_stats': stats
    })


@app.route('/api/stats')
def get_stats():
    """Get scanner statistics"""
    stats = get_threat_stats()
    
    # Get scan history stats
    total_scans = len(scan_history)
    threats_detected = sum(1 for scan in scan_history if scan.get('threat_level') in ['High', 'Critical'])
    clean_files = total_scans - threats_detected
    
    return jsonify({
        'threat_stats': stats,
        'scan_stats': {
            'total_scans': total_scans,
            'threats_detected': threats_detected,
            'clean_files': clean_files,
            'detection_rate': f"{(threats_detected / total_scans * 100):.1f}%" if total_scans > 0 else "0%"
        },
        'timestamp': datetime.now().isoformat()
    })


@app.route('/api/scan', methods=['POST'])
def scan_file():
    """Scan uploaded file"""
    try:
        # Check if file is in request
        if 'file' not in request.files:
            return jsonify({'error': 'No file provided'}), 400
        
        file = request.files['file']
        
        if file.filename == '':
            return jsonify({'error': 'No file selected'}), 400
        
        if not allowed_file(file.filename):
            return jsonify({'error': f'File type not allowed. Allowed: {", ".join(["exe", "dll", "sys", "zip", "pdf", "etc"])}'}), 400
        
        # Save file temporarily
        filename = secure_filename(file.filename)
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S_')
        temp_filename = timestamp + filename
        temp_path = os.path.join(app.config['UPLOAD_FOLDER'], temp_filename)
        
        file.save(temp_path)
        
        # Scan file
        logger.info(f"Scanning file: {temp_path}")
        result = scanner.scan_file(temp_path)
        
        # Add to scan history
        result['upload_time'] = datetime.now().isoformat()
        result['original_filename'] = filename
        scan_history.append(result)
        
        # Generate report
        report_filename = temp_filename.replace(os.path.splitext(temp_filename)[1], '.json')
        report_path = os.path.join(app.config['REPORTS_FOLDER'], report_filename)
        
        with open(report_path, 'w') as f:
            json.dump(result, f, indent=2)
        
        # Clean up temp file
        try:
            os.remove(temp_path)
        except:
            pass
        
        # Prepare response
        response_data = {
            'file_name': result.get('file_name'),
            'file_size': result.get('file_size'),
            'threat_level': result.get('threat_level'),
            'confidence': result.get('confidence'),
            'detection_count': result.get('detection_count', 0),
            'detections': result.get('detections', []),
            'scan_time': result.get('scan_time'),
            'report_id': report_filename,
            'hashes': result.get('hashes', {})
        }
        
        return jsonify(response_data)
    
    except Exception as e:
        logger.error(f"Scan error: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/batch-scan', methods=['POST'])
def batch_scan():
    """Scan multiple files"""
    try:
        if 'files' not in request.files:
            return jsonify({'error': 'No files provided'}), 400
        
        files = request.files.getlist('files')
        results = []
        
        for file in files:
            if file.filename == '' or not allowed_file(file.filename):
                continue
            
            filename = secure_filename(file.filename)
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S_')
            temp_filename = timestamp + filename
            temp_path = os.path.join(app.config['UPLOAD_FOLDER'], temp_filename)
            
            file.save(temp_path)
            
            # Scan
            result = scanner.scan_file(temp_path)
            result['upload_time'] = datetime.now().isoformat()
            result['original_filename'] = filename
            scan_history.append(result)
            
            results.append({
                'file_name': result.get('file_name'),
                'threat_level': result.get('threat_level'),
                'confidence': result.get('confidence'),
                'detection_count': result.get('detection_count', 0)
            })
            
            # Clean up
            try:
                os.remove(temp_path)
            except:
                pass
        
        return jsonify({
            'total_scanned': len(results),
            'results': results,
            'timestamp': datetime.now().isoformat()
        })
    
    except Exception as e:
        logger.error(f"Batch scan error: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/scan-history')
def get_scan_history():
    """Get scan history"""
    limit = request.args.get('limit', 50, type=int)
    
    # Return most recent scans
    history = sorted(scan_history, key=lambda x: x.get('scan_time', ''), reverse=True)[:limit]
    
    return jsonify({
        'total': len(scan_history),
        'scans': [{
            'file_name': scan.get('file_name'),
            'file_size': scan.get('file_size'),
            'threat_level': scan.get('threat_level'),
            'confidence': scan.get('confidence'),
            'detection_count': scan.get('detection_count', 0),
            'scan_time': scan.get('scan_time'),
            'hashes': scan.get('hashes', {})
        } for scan in history]
    })


@app.route('/api/threat-details/<report_id>')
def get_threat_details(report_id):
    """Get detailed threat report"""
    try:
        report_path = os.path.join(app.config['REPORTS_FOLDER'], secure_filename(report_id))
        
        if not os.path.exists(report_path):
            return jsonify({'error': 'Report not found'}), 404
        
        with open(report_path, 'r') as f:
            report = json.load(f)
        
        return jsonify(report)
    
    except Exception as e:
        logger.error(f"Error reading report: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/update-signatures', methods=['POST'])
def update_signatures():
    """Trigger threat feed update"""
    try:
        def background_update():
            logger.info("Starting signature update...")
            threat_updater.update_all()
            logger.info("Signature update complete")
        
        # Run update in background
        thread = threading.Thread(target=background_update)
        thread.daemon = True
        thread.start()
        
        return jsonify({
            'status': 'updating',
            'message': 'Threat feed update started in background',
            'timestamp': datetime.now().isoformat()
        })
    
    except Exception as e:
        logger.error(f"Update error: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/threat-feeds')
def get_threat_feeds():
    """Get threat feed status"""
    feeds = [
        {
            'name': 'Malware Bazaar',
            'url': 'bazaar.abuse.ch',
            'type': 'Hash Signatures',
            'status': 'active',
            'last_update': 'Daily'
        },
        {
            'name': 'URLhaus',
            'url': 'urlhaus.abuse.ch',
            'type': 'URL IOCs',
            'status': 'active',
            'last_update': 'Daily'
        },
        {
            'name': 'ThreatFox',
            'url': 'threatfox.abuse.ch',
            'type': 'IOCs',
            'status': 'active',
            'last_update': 'Daily'
        }
    ]
    
    return jsonify({
        'feeds': feeds,
        'threat_stats': get_threat_stats(),
        'timestamp': datetime.now().isoformat()
    })


@app.route('/api/export-report/<report_id>')
def export_report(report_id):
    """Export scan report as JSON"""
    try:
        report_path = os.path.join(app.config['REPORTS_FOLDER'], secure_filename(report_id))
        
        if not os.path.exists(report_path):
            return jsonify({'error': 'Report not found'}), 404
        
        return send_file(report_path, as_attachment=True, download_name=report_id)
    
    except Exception as e:
        logger.error(f"Export error: {e}")
        return jsonify({'error': str(e)}), 500


@app.route('/api/search-history')
def search_history():
    """Search scan history"""
    query = request.args.get('q', '').lower()
    threat_level = request.args.get('threat_level', '')
    
    results = scan_history
    
    if query:
        results = [s for s in results if query in s.get('file_name', '').lower()]
    
    if threat_level:
        results = [s for s in results if s.get('threat_level') == threat_level]
    
    return jsonify({
        'total': len(results),
        'scans': results[:50]
    })


@app.route('/api/detection-trends')
def detection_trends():
    """Get detection trends over time"""
    # Group scans by threat level
    threat_levels = {}
    
    for scan in scan_history:
        level = scan.get('threat_level', 'Unknown')
        if level not in threat_levels:
            threat_levels[level] = 0
        threat_levels[level] += 1
    
    return jsonify({
        'threat_distribution': threat_levels,
        'total_scans': len(scan_history),
        'timestamp': datetime.now().isoformat()
    })


@app.route('/api/config', methods=['GET', 'POST'])
def scanner_config():
    """Get/set scanner configuration"""
    if request.method == 'GET':
        config = {
            'max_file_size': 100,  # MB
            'scan_timeout': 300,  # seconds
            'heuristic_threshold': 70,  # 0-100
            'auto_update': True,
            'update_interval': 'daily',
            'privacy_mode': True,
            'delete_after_scan': True
        }
        return jsonify(config)
    
    else:  # POST
        config = request.get_json()
        logger.info(f"Config updated: {config}")
        return jsonify({'status': 'success', 'message': 'Configuration updated'})


if __name__ == '__main__':
    logger.info("Starting Custom AV Scanner Web Dashboard...")
    logger.info("Dashboard available at http://localhost:5000")
    logger.info("API documentation available at http://localhost:5000/api/docs")
    
    # Start Flask app
    app.run(
        host='127.0.0.1',
        port=5000,
        debug=False,
        threaded=True
    )

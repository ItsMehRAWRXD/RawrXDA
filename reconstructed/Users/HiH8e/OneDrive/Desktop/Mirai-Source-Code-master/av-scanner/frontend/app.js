// API Base URL
const API_URL = window.location.origin + '/api';

// State
let currentUser = null;
let currentScanId = null;
let pollInterval = null;

// Initialize
document.addEventListener('DOMContentLoaded', () => {
  // Check if user is logged in
  const token = localStorage.getItem('token');
  if (token) {
    fetchUserProfile();
  } else {
    showAuthPage();
  }

  // Setup event listeners
  setupEventListeners();
});

function setupEventListeners() {
  // Login form
  document.getElementById('login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('login-username').value;
    const password = document.getElementById('login-password').value;
    await login(username, password);
  });

  // Register form
  document.getElementById('register-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('register-username').value;
    const email = document.getElementById('register-email').value;
    const password = document.getElementById('register-password').value;
    const display_name = document.getElementById('register-display-name').value;
    await register(username, email, password, display_name);
  });

  // File upload
  const fileInput = document.getElementById('file-input');
  fileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
      uploadFile(e.target.files[0]);
    }
  });

  // Drag and drop
  const uploadArea = document.getElementById('upload-area');
  uploadArea.addEventListener('dragover', (e) => {
    e.preventDefault();
    uploadArea.style.borderColor = 'var(--primary-color)';
  });

  uploadArea.addEventListener('dragleave', () => {
    uploadArea.style.borderColor = 'var(--border-color)';
  });

  uploadArea.addEventListener('drop', (e) => {
    e.preventDefault();
    uploadArea.style.borderColor = 'var(--border-color)';
    if (e.dataTransfer.files.length > 0) {
      uploadFile(e.dataTransfer.files[0]);
    }
  });
}

// Auth functions
async function login(username, password) {
  try {
    const response = await fetch(`${API_URL}/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, password })
    });

    const data = await response.json();

    if (response.ok) {
      localStorage.setItem('token', data.token);
      currentUser = data.user;
      showDashboard();
    } else {
      document.getElementById('login-error').textContent = data.error || 'Login failed';
    }
  } catch (error) {
    document.getElementById('login-error').textContent = 'Connection error';
  }
}

async function register(username, email, password, display_name) {
  try {
    const response = await fetch(`${API_URL}/auth/register`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, email, password, display_name })
    });

    const data = await response.json();

    if (response.ok) {
      // Auto login after registration
      await login(username, password);
    } else {
      document.getElementById('register-error').textContent = data.error || 'Registration failed';
    }
  } catch (error) {
    document.getElementById('register-error').textContent = 'Connection error';
  }
}

async function fetchUserProfile() {
  try {
    const response = await fetch(`${API_URL}/auth/profile`, {
      headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` }
    });

    if (response.ok) {
      currentUser = await response.json();
      showDashboard();
    } else {
      logout();
    }
  } catch (error) {
    logout();
  }
}

function logout() {
  localStorage.removeItem('token');
  currentUser = null;
  showAuthPage();
}

// Page navigation
function showAuthPage() {
  document.getElementById('auth-page').classList.add('active');
  document.getElementById('dashboard-page').classList.remove('active');
}

function showDashboard() {
  document.getElementById('auth-page').classList.remove('active');
  document.getElementById('dashboard-page').classList.add('active');

  // Update user info
  document.getElementById('user-display-name').textContent = currentUser.display_name;
  document.getElementById('scans-count').textContent = currentUser.scans_remaining;

  // Apply user theme
  if (currentUser.theme === 'light') {
    document.body.setAttribute('data-theme', 'light');
    document.getElementById('theme-toggle').textContent = '☀️';
  }

  // Apply user font
  if (currentUser.font_choice) {
    document.body.className = `font-${currentUser.font_choice}`;
  }
}

function showTab(tab) {
  const tabs = document.querySelectorAll('.tab-btn');
  const forms = document.querySelectorAll('.auth-form');

  tabs.forEach(t => t.classList.remove('active'));
  forms.forEach(f => f.classList.remove('active'));

  event.target.classList.add('active');
  document.getElementById(`${tab}-form`).classList.add('active');
}

function showPage(page) {
  // Update nav links
  const navLinks = document.querySelectorAll('.nav-links a');
  navLinks.forEach(link => link.classList.remove('active'));
  event.target.classList.add('active');

  // Show section
  const sections = document.querySelectorAll('.section');
  sections.forEach(section => section.classList.remove('active'));
  document.getElementById(`${page}-section`).classList.add('active');

  // Load page data
  if (page === 'history') loadHistory();
  if (page === 'stats') loadStatistics();
  if (page === 'pricing') loadPricing();
}

// File upload and scanning
async function uploadFile(file) {
  const formData = new FormData();
  formData.append('file', file);

  // Show progress
  document.getElementById('upload-area').classList.add('hidden');
  document.getElementById('scan-results').classList.add('hidden');
  const progressDiv = document.getElementById('scan-progress');
  progressDiv.classList.remove('hidden');
  document.getElementById('scan-filename').textContent = file.name;

  try {
    const response = await fetch(`${API_URL}/scan/upload`, {
      method: 'POST',
      headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` },
      body: formData
    });

    const data = await response.json();

    if (response.ok) {
      currentScanId = data.scanId;
      pollScanStatus();
    } else {
      alert(data.error || 'Upload failed');
      newScan();
    }
  } catch (error) {
    alert('Connection error');
    newScan();
  }
}

async function pollScanStatus() {
  let progress = 0;
  const progressFill = document.getElementById('progress-fill');
  const statusText = document.getElementById('scan-status');

  pollInterval = setInterval(async () => {
    try {
      const response = await fetch(`${API_URL}/scan/result/${currentScanId}`, {
        headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` }
      });

      const scan = await response.json();

      if (scan.scan_status === 'completed') {
        clearInterval(pollInterval);
        progressFill.style.width = '100%';
        statusText.textContent = 'Scan complete!';
        setTimeout(() => displayResults(scan), 500);
      } else if (scan.scan_status === 'failed') {
        clearInterval(pollInterval);
        alert('Scan failed');
        newScan();
      } else {
        // Simulate progress
        progress = Math.min(progress + 5, 95);
        progressFill.style.width = progress + '%';
        statusText.textContent = 'Scanning with ' + (scan.total_engines || 27) + ' AV engines...';
      }
    } catch (error) {
      console.error('Poll error:', error);
    }
  }, 1000);
}

function displayResults(scan) {
  document.getElementById('scan-progress').classList.add('hidden');
  const resultsDiv = document.getElementById('scan-results');
  resultsDiv.classList.remove('hidden');

  // Update result header
  document.getElementById('result-filename').textContent = scan.filename;
  document.getElementById('result-hash').textContent = scan.file_hash;
  document.getElementById('result-scan-id').textContent = scan.id;

  // Update detection badge
  const detectionBadge = document.getElementById('detection-badge');
  document.getElementById('detection-count').textContent = scan.detection_count;

  if (scan.detection_count === 0) {
    detectionBadge.classList.add('clean');
  } else {
    detectionBadge.classList.remove('clean');
  }

  // Display engine results
  const resultsBody = document.getElementById('results-body');
  resultsBody.innerHTML = '';

  if (scan.scan_results && scan.scan_results.engines) {
    scan.scan_results.engines.forEach(engine => {
      const row = document.createElement('tr');
      row.innerHTML = `
                <td>${engine.engine}</td>
                <td>${engine.version || 'N/A'}</td>
                <td class="${engine.detected ? 'status-detected' : 'status-clean'}">
                    ${engine.detected ? '⚠️ DETECTED' : '✅ CLEAN'}
                </td>
                <td>${engine.threatName || '-'}</td>
            `;
      resultsBody.appendChild(row);
    });
  }
}

function newScan() {
  document.getElementById('scan-results').classList.add('hidden');
  document.getElementById('scan-progress').classList.add('hidden');
  document.getElementById('upload-area').classList.remove('hidden');
  document.getElementById('file-input').value = '';
  currentScanId = null;
}

async function shareResults() {
  const shareUrl = `${window.location.origin}/api/scan/public/${currentScanId}`;
  try {
    await navigator.clipboard.writeText(shareUrl);
    alert('Share link copied to clipboard!');
  } catch (error) {
    prompt('Copy this link:', shareUrl);
  }
}

async function downloadPDF() {
  if (!currentScanId) {
    alert('No scan selected');
    return;
  }

  try {
    const response = await fetch(`${API_URL}/pdf/generate/${currentScanId}`, {
      headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` }
    });

    if (response.ok) {
      // Create blob from response
      const blob = await response.blob();

      // Create download link
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `scan-report-${currentScanId}.pdf`;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } else {
      const error = await response.json();
      alert(error.error || 'Failed to generate PDF');
    }
  } catch (error) {
    console.error('PDF download error:', error);
    alert('Failed to download PDF report');
  }
}// History
async function loadHistory() {
  try {
    const response = await fetch(`${API_URL}/scan/history`, {
      headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` }
    });

    const scans = await response.json();
    const historyList = document.getElementById('history-list');

    if (scans.length === 0) {
      historyList.innerHTML = '<p class="loading">No scan history yet</p>';
      return;
    }

    historyList.innerHTML = scans.map(scan => `
            <div class="history-item" onclick="viewScan('${scan.id}')">
                <div class="history-info">
                    <h4>${scan.filename}</h4>
                    <p>${new Date(scan.created_at).toLocaleString()}</p>
                </div>
                <div class="history-badge ${scan.detection_count > 0 ? 'detected' : 'clean'}">
                    ${scan.detection_count}/${scan.total_engines}
                </div>
            </div>
        `).join('');
  } catch (error) {
    console.error('History error:', error);
  }
}

async function viewScan(scanId) {
  try {
    const response = await fetch(`${API_URL}/scan/result/${scanId}`, {
      headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` }
    });

    const scan = await response.json();
    currentScanId = scanId;
    showPage('scan');
    displayResults(scan);
  } catch (error) {
    alert('Failed to load scan');
  }
}

// Statistics
async function loadStatistics() {
  try {
    const response = await fetch(`${API_URL}/stats/user`, {
      headers: { 'Authorization': `Bearer ${localStorage.getItem('token')}` }
    });

    const stats = await response.json();

    document.getElementById('stat-total-scans').textContent = stats.total_scans;
    document.getElementById('stat-detections').textContent = stats.total_detections;
    document.getElementById('stat-clean').textContent = stats.total_clean;
    document.getElementById('stat-rate').textContent = stats.detection_rate + '%';
  } catch (error) {
    console.error('Stats error:', error);
  }
}

// Pricing
async function loadPricing() {
  try {
    const response = await fetch(`${API_URL}/payment/plans`);
    const data = await response.json();

    const pricingGrid = document.getElementById('pricing-plans');
    pricingGrid.innerHTML = data.plans.map(plan => `
            <div class="pricing-card">
                <h3>${plan.name}</h3>
                <p class="scans-count">${plan.scans} scan${plan.scans > 1 ? 's' : ''}</p>
                <div>
                    <div class="old-price">$${plan.price.toFixed(2)}</div>
                    <div class="price">$${plan.discounted_price.toFixed(2)}</div>
                </div>
                <p>${plan.description}</p>
                <button class="btn-primary" onclick="purchasePlan('${plan.type}', '${plan.name}', ${plan.scans})">
                    ${plan.type === 'pay-per-scan' ? 'Purchase' : 'Subscribe'}
                </button>
            </div>
        `).join('');
  } catch (error) {
    console.error('Pricing error:', error);
  }
}

async function purchasePlan(type, name, scans) {
  if (type === 'pay-per-scan') {
    const quantity = prompt('How many scans would you like to purchase?', '10');
    if (!quantity || quantity < 1) return;

    try {
      const response = await fetch(`${API_URL}/payment/purchase`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${localStorage.getItem('token')}`
        },
        body: JSON.stringify({ quantity: parseInt(quantity) })
      });

      const data = await response.json();
      if (response.ok) {
        alert(`Purchase successful! ${data.scans_added} scans added.`);
        fetchUserProfile();
      } else {
        alert(data.error || 'Purchase failed');
      }
    } catch (error) {
      alert('Connection error');
    }
  } else {
    // Monthly subscription
    if (!confirm(`Subscribe to ${name} plan?`)) return;

    try {
      const response = await fetch(`${API_URL}/payment/subscribe`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${localStorage.getItem('token')}`
        },
        body: JSON.stringify({ plan_type: name.toLowerCase() })
      });

      const data = await response.json();
      if (response.ok) {
        alert(`Subscription successful! ${data.scans_added} scans added.`);
        fetchUserProfile();
      } else {
        alert(data.error || 'Subscription failed');
      }
    } catch (error) {
      alert('Connection error');
    }
  }
}

// Settings
function toggleTheme() {
  const currentTheme = document.body.getAttribute('data-theme');
  const newTheme = currentTheme === 'light' ? 'dark' : 'light';

  document.body.setAttribute('data-theme', newTheme);
  document.getElementById('theme-toggle').textContent = newTheme === 'light' ? '☀️' : '🌙';

  currentUser.theme = newTheme;
}

async function saveSettings() {
  alert('Settings saved! (Settings persistence to be implemented)');
}

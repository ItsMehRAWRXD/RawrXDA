# Start IDE Backend Servers
# Launches Orchestra (port 11442) and Backend API (port 9000) for IDEre2.html

Write-Host "🚀 Starting IDE Backend Servers..." -ForegroundColor Cyan
Write-Host ""

# Check if Ollama is running
$ollamaRunning = $false
try {
    $ollamaTest = Invoke-WebRequest -Uri "http://localhost:11434/api/version" -Method Get -TimeoutSec 2 -ErrorAction SilentlyContinue
    if ($ollamaTest.StatusCode -eq 200) {
        $ollamaRunning = $true
        Write-Host "✅ Ollama is running on port 11434" -ForegroundColor Green
    }
} catch {
    Write-Host "⚠️  Ollama not detected on port 11434" -ForegroundColor Yellow
    Write-Host "   Start Ollama first: ollama serve" -ForegroundColor Gray
}

# Create Orchestra Server (port 11442) - Ollama Proxy
$orchestraScript = @'
const express = require('express');
const cors = require('cors');
const fetch = (...args) => import('node-fetch').then(({default: fetch}) => fetch(...args));

const app = express();
const PORT = 11442;
const OLLAMA_URL = 'http://localhost:11434';

app.use(cors());
app.use(express.json());

// Health check
app.get('/health', (req, res) => {
    res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// List models
app.get('/models', async (req, res) => {
    try {
        const response = await fetch(`${OLLAMA_URL}/api/tags`);
        const data = await response.json();
        res.json({ models: data.models || [] });
    } catch (error) {
        console.error('Failed to fetch models:', error);
        res.status(500).json({ error: 'Failed to fetch models from Ollama' });
    }
});

// Generate completion
app.post('/generate', async (req, res) => {
    try {
        const { model, prompt, options } = req.body;
        
        const response = await fetch(`${OLLAMA_URL}/api/generate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                model: model || 'llama2',
                prompt: prompt,
                stream: false,
                options: options || {}
            })
        });

        const data = await response.json();
        res.json(data);
    } catch (error) {
        console.error('Generation error:', error);
        res.status(500).json({ error: error.message });
    }
});

app.listen(PORT, () => {
    console.log(`🎵 Orchestra Server running on http://localhost:${PORT}`);
    console.log(`📡 Proxying to Ollama at ${OLLAMA_URL}`);
});
'@

$orchestraPath = Join-Path $PSScriptRoot "orchestra-server.js"
$orchestraScript | Set-Content $orchestraPath -Encoding UTF8

# Create Backend API Server (port 9000)
$backendScript = @'
const express = require('express');
const cors = require('cors');
const fs = require('fs').promises;
const path = require('path');

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
'@

$backendPath = Join-Path $PSScriptRoot "backend-server.js"
$backendScript | Set-Content $backendPath -Encoding UTF8

# Check for Node.js
$nodeExists = $null -ne (Get-Command node -ErrorAction SilentlyContinue)

if (-not $nodeExists) {
    Write-Host "❌ Node.js not found. Please install Node.js first." -ForegroundColor Red
    Write-Host "   Download from: https://nodejs.org/" -ForegroundColor Gray
    exit 1
}

Write-Host "✅ Node.js detected" -ForegroundColor Green

# Install dependencies
Write-Host ""
Write-Host "📦 Installing dependencies..." -ForegroundColor Cyan

$packageJson = @"
{
  "name": "ide-servers",
  "version": "1.0.0",
  "type": "module",
  "dependencies": {
    "express": "^4.18.2",
    "cors": "^2.8.5",
    "node-fetch": "^3.3.2"
  }
}
"@

$packageJsonPath = Join-Path $PSScriptRoot "package.json"
$packageJson | Set-Content $packageJsonPath -Encoding UTF8

Push-Location $PSScriptRoot
npm install --silent 2>&1 | Out-Null
Pop-Location

Write-Host "✅ Dependencies installed" -ForegroundColor Green
Write-Host ""

# Start servers in background
Write-Host "🚀 Starting Orchestra Server (port 11442)..." -ForegroundColor Cyan
$orchestraJob = Start-Job -ScriptBlock {
    param($serverPath)
    Set-Location (Split-Path $serverPath)
    node $serverPath
} -ArgumentList $orchestraPath

Start-Sleep -Seconds 2

Write-Host "🚀 Starting Backend API Server (port 9000)..." -ForegroundColor Cyan
$backendJob = Start-Job -ScriptBlock {
    param($serverPath)
    Set-Location (Split-Path $serverPath)
    node $serverPath
} -ArgumentList $backendPath

Start-Sleep -Seconds 2

# Check if servers started
Write-Host ""
Write-Host "🔍 Testing server connections..." -ForegroundColor Cyan

try {
    $orchestraHealth = Invoke-WebRequest -Uri "http://localhost:11442/health" -Method Get -TimeoutSec 3
    if ($orchestraHealth.StatusCode -eq 200) {
        Write-Host "✅ Orchestra Server: http://localhost:11442/health" -ForegroundColor Green
    }
} catch {
    Write-Host "❌ Orchestra Server failed to start" -ForegroundColor Red
    Write-Host "   Error: $($_.Exception.Message)" -ForegroundColor Gray
}

try {
    $backendHealth = Invoke-WebRequest -Uri "http://localhost:9000/api/health" -Method Get -TimeoutSec 3
    if ($backendHealth.StatusCode -eq 200) {
        Write-Host "✅ Backend Server: http://localhost:9000/api/health" -ForegroundColor Green
    }
} catch {
    Write-Host "❌ Backend Server failed to start" -ForegroundColor Red
    Write-Host "   Error: $($_.Exception.Message)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "📂 IDE Path: C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html" -ForegroundColor Cyan
Write-Host ""
Write-Host "🎯 Servers are running! Open your IDE in a browser:" -ForegroundColor Green
Write-Host "   file:///C:/Users/HiH8e/OneDrive/Desktop/IDEre2.html" -ForegroundColor White
Write-Host ""
Write-Host "💡 Tip: Use http-server or similar to serve IDEre2.html over HTTP for better CORS support" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press Ctrl+C to stop servers and clean up..." -ForegroundColor Gray

# Keep script running and monitor jobs
try {
    while ($true) {
        Start-Sleep -Seconds 5
        
        # Check if jobs are still running
        if ($orchestraJob.State -ne 'Running') {
            Write-Host "⚠️  Orchestra server stopped unexpectedly" -ForegroundColor Yellow
            Receive-Job -Job $orchestraJob
        }
        
        if ($backendJob.State -ne 'Running') {
            Write-Host "⚠️  Backend server stopped unexpectedly" -ForegroundColor Yellow
            Receive-Job -Job $backendJob
        }
    }
} finally {
    Write-Host ""
    Write-Host "🛑 Stopping servers..." -ForegroundColor Yellow
    Stop-Job -Job $orchestraJob -ErrorAction SilentlyContinue
    Stop-Job -Job $backendJob -ErrorAction SilentlyContinue
    Remove-Job -Job $orchestraJob -Force -ErrorAction SilentlyContinue
    Remove-Job -Job $backendJob -Force -ErrorAction SilentlyContinue
    Write-Host "✅ Servers stopped" -ForegroundColor Green
}

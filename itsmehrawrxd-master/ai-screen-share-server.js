const express = require('express');
const WebSocket = require('ws');
const cors = require('cors');
const helmet = require('helmet');
const { spawn } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const { v4: uuidv4 } = require('uuid');

class AIScreenShareServer {
    constructor() {
        this.app = express();
        this.server = null;
        this.wss = null;
        this.port = process.env.SCREEN_SHARE_PORT || 8888;
        this.connectedAgents = new Map();
        this.screenStreams = new Map();
        this.collaborationSessions = new Map();
        
        this.setupMiddleware();
        this.setupRoutes();
        this.setupWebSocket();
    }

    setupMiddleware() {
        this.app.use(helmet({
            contentSecurityPolicy: false, // Allow screen sharing
            crossOriginEmbedderPolicy: false
        }));
        
        this.app.use(cors({
            origin: '*',
            credentials: true
        }));
        
        this.app.use(express.json({ limit: '50mb' }));
        this.app.use(express.urlencoded({ extended: true, limit: '50mb' }));
    }

    setupRoutes() {
        // Main screen sharing interface
        this.app.get('/', (req, res) => {
            res.send(this.getScreenShareHTML());
        });

        // AI Agent registration endpoint
        this.app.post('/api/agents/register', (req, res) => {
            const { agentId, agentType, capabilities } = req.body;
            const sessionId = uuidv4();
            
            this.connectedAgents.set(agentId, {
                id: agentId,
                type: agentType,
                capabilities: capabilities || [],
                sessionId: sessionId,
                connectedAt: new Date(),
                status: 'active'
            });

            res.json({
                success: true,
                sessionId: sessionId,
                message: 'AI Agent registered successfully'
            });
        });

        // Screen capture endpoint
        this.app.post('/api/screen/capture', async (req, res) => {
            try {
                const { sessionId, region, quality } = req.body;
                const screenshot = await this.captureScreen(region, quality);
                
                // Broadcast to all connected AI agents
                this.broadcastToAgents({
                    type: 'screen_update',
                    sessionId: sessionId,
                    timestamp: Date.now(),
                    data: screenshot,
                    region: region,
                    quality: quality
                });

                res.json({
                    success: true,
                    screenshot: screenshot,
                    timestamp: Date.now()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });

        // AI Agent response endpoint
        this.app.post('/api/agents/response', (req, res) => {
            const { agentId, sessionId, response, action, coordinates } = req.body;
            
            // Store AI response
            if (!this.collaborationSessions.has(sessionId)) {
                this.collaborationSessions.set(sessionId, {
                    id: sessionId,
                    createdAt: new Date(),
                    interactions: []
                });
            }

            const session = this.collaborationSessions.get(sessionId);
            session.interactions.push({
                agentId: agentId,
                timestamp: new Date(),
                response: response,
                action: action,
                coordinates: coordinates
            });

            // Broadcast AI response to user
            this.broadcastToUser(sessionId, {
                type: 'ai_response',
                agentId: agentId,
                response: response,
                action: action,
                coordinates: coordinates,
                timestamp: Date.now()
            });

            res.json({
                success: true,
                message: 'AI response processed'
            });
        });

        // Get collaboration session
        this.app.get('/api/sessions/:sessionId', (req, res) => {
            const sessionId = req.params.sessionId;
            const session = this.collaborationSessions.get(sessionId);
            
            if (!session) {
                return res.status(404).json({
                    success: false,
                    error: 'Session not found'
                });
            }

            res.json({
                success: true,
                session: session
            });
        });

        // List connected agents
        this.app.get('/api/agents', (req, res) => {
            const agents = Array.from(this.connectedAgents.values());
            res.json({
                success: true,
                agents: agents,
                count: agents.length
            });
        });

        // Start screen sharing session
        this.app.post('/api/sessions/start', (req, res) => {
            const { agentId, sessionType } = req.body;
            const sessionId = uuidv4();
            
            this.collaborationSessions.set(sessionId, {
                id: sessionId,
                agentId: agentId,
                type: sessionType || 'collaboration',
                createdAt: new Date(),
                interactions: [],
                status: 'active'
            });

            res.json({
                success: true,
                sessionId: sessionId,
                message: 'Screen sharing session started'
            });
        });

        // Stop screen sharing session
        this.app.post('/api/sessions/:sessionId/stop', (req, res) => {
            const sessionId = req.params.sessionId;
            const session = this.collaborationSessions.get(sessionId);
            
            if (session) {
                session.status = 'stopped';
                session.endedAt = new Date();
            }

            res.json({
                success: true,
                message: 'Screen sharing session stopped'
            });
        });
    }

    setupWebSocket() {
        this.wss = new WebSocket.Server({ 
            server: this.server,
            path: '/ws'
        });

        this.wss.on('connection', (ws, req) => {
            console.log(' New WebSocket connection established');
            
            ws.on('message', (message) => {
                try {
                    const data = JSON.parse(message);
                    this.handleWebSocketMessage(ws, data);
                } catch (error) {
                    console.error('Error parsing WebSocket message:', error);
                }
            });

            ws.on('close', () => {
                console.log(' WebSocket connection closed');
                this.removeAgentConnection(ws);
            });

            ws.on('error', (error) => {
                console.error('WebSocket error:', error);
            });
        });
    }

    handleWebSocketMessage(ws, data) {
        switch (data.type) {
            case 'agent_register':
                this.registerAgentWebSocket(ws, data);
                break;
            case 'screen_capture':
                this.handleScreenCaptureRequest(ws, data);
                break;
            case 'ai_response':
                this.handleAIResponse(ws, data);
                break;
            case 'user_action':
                this.handleUserAction(ws, data);
                break;
            default:
                console.log('Unknown WebSocket message type:', data.type);
        }
    }

    registerAgentWebSocket(ws, data) {
        const { agentId, agentType, capabilities } = data;
        ws.agentId = agentId;
        ws.agentType = agentType;
        ws.capabilities = capabilities;
        
        this.connectedAgents.set(agentId, {
            id: agentId,
            type: agentType,
            capabilities: capabilities,
            websocket: ws,
            connectedAt: new Date(),
            status: 'active'
        });

        ws.send(JSON.stringify({
            type: 'registration_confirmed',
            agentId: agentId,
            message: 'Successfully registered as AI agent'
        }));

        console.log(` AI Agent registered: ${agentId} (${agentType})`);
    }

    async captureScreen(region = null, quality = 'medium') {
        try {
            // Use platform-specific screen capture
            const platform = process.platform;
            let command, args;

            if (platform === 'win32') {
                // Windows: Use PowerShell with Add-Type for screen capture
                command = 'powershell';
                args = [
                    '-Command',
                    `Add-Type -AssemblyName System.Windows.Forms,System.Drawing; 
                     $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds;
                     $bitmap = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height;
                     $graphics = [System.Drawing.Graphics]::FromImage($bitmap);
                     $graphics.CopyFromScreen($screen.Left, $screen.Top, 0, 0, $screen.Size);
                     $graphics.Dispose();
                     $ms = New-Object System.IO.MemoryStream;
                     $bitmap.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png);
                     $bytes = $ms.ToArray();
                     $ms.Dispose();
                     $bitmap.Dispose();
                     [Convert]::ToBase64String($bytes)`
                ];
            } else if (platform === 'darwin') {
                // macOS: Use screencapture
                command = 'screencapture';
                args = ['-x', '-t', 'png', '-'];
            } else {
                // Linux: Use import (ImageMagick)
                command = 'import';
                args = ['-window', 'root', '-format', 'png', '-'];
            }

            return new Promise((resolve, reject) => {
                const child = spawn(command, args);
                let data = '';
                let error = '';

                child.stdout.on('data', (chunk) => {
                    data += chunk;
                });

                child.stderr.on('data', (chunk) => {
                    error += chunk;
                });

                child.on('close', (code) => {
                    if (code === 0) {
                        if (platform === 'win32') {
                            // Windows returns base64 string
                            resolve(data.trim());
                        } else {
                            // Unix systems return binary data
                            resolve(Buffer.from(data, 'binary').toString('base64'));
                        }
                    } else {
                        reject(new Error(`Screen capture failed: ${error}`));
                    }
                });
            });
        } catch (error) {
            console.error('Screen capture error:', error);
            throw error;
        }
    }

    broadcastToAgents(data) {
        this.connectedAgents.forEach((agent, agentId) => {
            if (agent.websocket && agent.websocket.readyState === WebSocket.OPEN) {
                try {
                    agent.websocket.send(JSON.stringify(data));
                } catch (error) {
                    console.error(`Error sending to agent ${agentId}:`, error);
                }
            }
        });
    }

    broadcastToUser(sessionId, data) {
        // Broadcast to all WebSocket connections (users)
        this.wss.clients.forEach((ws) => {
            if (ws.readyState === WebSocket.OPEN && !ws.agentId) {
                try {
                    ws.send(JSON.stringify({
                        ...data,
                        sessionId: sessionId
                    }));
                } catch (error) {
                    console.error('Error sending to user:', error);
                }
            }
        });
    }

    removeAgentConnection(ws) {
        if (ws.agentId) {
            this.connectedAgents.delete(ws.agentId);
            console.log(` AI Agent disconnected: ${ws.agentId}`);
        }
    }

    getScreenShareHTML() {
        return `
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI Screen Share - Real-time Collaboration</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        .header p {
            font-size: 1.2em;
            opacity: 0.9;
        }
        
        .main-content {
            display: grid;
            grid-template-columns: 1fr 300px;
            gap: 20px;
            height: calc(100vh - 200px);
        }
        
        .screen-panel {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 20px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.2);
        }
        
        .screen-display {
            width: 100%;
            height: 400px;
            background: #000;
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 20px;
            position: relative;
            overflow: hidden;
        }
        
        .screen-image {
            max-width: 100%;
            max-height: 100%;
            border-radius: 5px;
        }
        
        .controls {
            display: flex;
            gap: 10px;
            margin-bottom: 20px;
        }
        
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: bold;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .btn-primary {
            background: linear-gradient(45deg, #4CAF50, #45a049);
            color: white;
        }
        
        .btn-primary:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(76, 175, 80, 0.4);
        }
        
        .btn-secondary {
            background: linear-gradient(45deg, #2196F3, #1976D2);
            color: white;
        }
        
        .btn-secondary:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(33, 150, 243, 0.4);
        }
        
        .btn-danger {
            background: linear-gradient(45deg, #f44336, #d32f2f);
            color: white;
        }
        
        .btn-danger:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(244, 67, 54, 0.4);
        }
        
        .sidebar {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        
        .agents-panel, .session-panel, .ai-responses {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 20px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.2);
        }
        
        .panel-title {
            font-size: 1.3em;
            margin-bottom: 15px;
            color: #FFD700;
            text-align: center;
        }
        
        .agent-item, .response-item {
            background: rgba(255,255,255,0.1);
            padding: 10px;
            border-radius: 8px;
            margin-bottom: 10px;
            border-left: 4px solid #4CAF50;
        }
        
        .agent-status {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #4CAF50;
            margin-right: 10px;
        }
        
        .agent-status.inactive {
            background: #f44336;
        }
        
        .status-indicator {
            position: absolute;
            top: 10px;
            right: 10px;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #f44336;
            animation: pulse 2s infinite;
        }
        
        .status-indicator.active {
            background: #4CAF50;
        }
        
        @keyframes pulse {
            0% { transform: scale(1); opacity: 1; }
            50% { transform: scale(1.1); opacity: 0.7; }
            100% { transform: scale(1); opacity: 1; }
        }
        
        .ai-response {
            background: rgba(33, 150, 243, 0.2);
            border-left-color: #2196F3;
            margin-top: 10px;
        }
        
        .coordinates {
            font-size: 0.9em;
            color: #FFD700;
            margin-top: 5px;
        }
        
        .loading {
            display: none;
            text-align: center;
            padding: 20px;
        }
        
        .spinner {
            border: 4px solid rgba(255,255,255,0.3);
            border-top: 4px solid white;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 0 auto 10px;
        }
        
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        
        .error {
            background: rgba(244, 67, 54, 0.2);
            border: 1px solid #f44336;
            color: #ffcdd2;
            padding: 10px;
            border-radius: 8px;
            margin: 10px 0;
        }
        
        .success {
            background: rgba(76, 175, 80, 0.2);
            border: 1px solid #4CAF50;
            color: #c8e6c9;
            padding: 10px;
            border-radius: 8px;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1> AI Screen Share</h1>
            <p>Real-time collaboration with AI agents</p>
        </div>
        
        <div class="main-content">
            <div class="screen-panel">
                <div class="screen-display">
                    <div class="status-indicator" id="statusIndicator"></div>
                    <img id="screenImage" class="screen-image" style="display: none;" alt="Screen Capture">
                    <div id="noScreen" style="text-align: center; color: #666;">
                        <h3>No Screen Capture</h3>
                        <p>Click "Start Sharing" to begin</p>
                    </div>
                </div>
                
                <div class="controls">
                    <button class="btn btn-primary" onclick="startScreenShare()">Start Sharing</button>
                    <button class="btn btn-secondary" onclick="captureScreen()">Capture Now</button>
                    <button class="btn btn-danger" onclick="stopScreenShare()">Stop Sharing</button>
                </div>
                
                <div class="loading" id="loading">
                    <div class="spinner"></div>
                    <p>Processing...</p>
                </div>
            </div>
            
            <div class="sidebar">
                <div class="agents-panel">
                    <h3 class="panel-title"> Connected AI Agents</h3>
                    <div id="agentsList">
                        <p style="text-align: center; opacity: 0.7;">No agents connected</p>
                    </div>
                </div>
                
                <div class="session-panel">
                    <h3 class="panel-title"> Session Info</h3>
                    <div id="sessionInfo">
                        <p><strong>Session ID:</strong> <span id="sessionId">Not started</span></p>
                        <p><strong>Status:</strong> <span id="sessionStatus">Inactive</span></p>
                        <p><strong>Duration:</strong> <span id="sessionDuration">00:00:00</span></p>
                    </div>
                </div>
                
                <div class="ai-responses">
                    <h3 class="panel-title"> AI Responses</h3>
                    <div id="responsesList">
                        <p style="text-align: center; opacity: 0.7;">No responses yet</p>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        class AIScreenShare {
            constructor() {
                this.ws = null;
                this.sessionId = null;
                this.isSharing = false;
                this.captureInterval = null;
                this.sessionStartTime = null;
                this.connectedAgents = new Map();
                
                this.init();
            }
            
            init() {
                this.connectWebSocket();
                this.updateSessionInfo();
                setInterval(() => this.updateSessionInfo(), 1000);
            }
            
            connectWebSocket() {
                const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
                const wsUrl = \`\${protocol}//\${window.location.host}/ws\`;
                
                this.ws = new WebSocket(wsUrl);
                
                this.ws.onopen = () => {
                    console.log('WebSocket connected');
                    this.updateStatus(true);
                };
                
                this.ws.onmessage = (event) => {
                    const data = JSON.parse(event.data);
                    this.handleMessage(data);
                };
                
                this.ws.onclose = () => {
                    console.log('WebSocket disconnected');
                    this.updateStatus(false);
                    setTimeout(() => this.connectWebSocket(), 3000);
                };
                
                this.ws.onerror = (error) => {
                    console.error('WebSocket error:', error);
                };
            }
            
            handleMessage(data) {
                switch (data.type) {
                    case 'ai_response':
                        this.handleAIResponse(data);
                        break;
                    case 'agent_connected':
                        this.addAgent(data.agent);
                        break;
                    case 'agent_disconnected':
                        this.removeAgent(data.agentId);
                        break;
                    default:
                        console.log('Unknown message type:', data.type);
                }
            }
            
            handleAIResponse(data) {
                this.addAIResponse(data);
            }
            
            addAIResponse(response) {
                const responsesList = document.getElementById('responsesList');
                const responseDiv = document.createElement('div');
                responseDiv.className = 'response-item ai-response';
                
                const timestamp = new Date(response.timestamp).toLocaleTimeString();
                responseDiv.innerHTML = \`
                    <div><strong>Agent:</strong> \${response.agentId}</div>
                    <div><strong>Response:</strong> \${response.response}</div>
                    \${response.action ? \`<div><strong>Action:</strong> \${response.action}</div>\` : ''}
                    \${response.coordinates ? \`<div class="coordinates">Coordinates: \${response.coordinates.x}, \${response.coordinates.y}</div>\` : ''}
                    <div style="font-size: 0.8em; opacity: 0.7;">\${timestamp}</div>
                \`;
                
                responsesList.insertBefore(responseDiv, responsesList.firstChild);
                
                // Keep only last 10 responses
                while (responsesList.children.length > 10) {
                    responsesList.removeChild(responsesList.lastChild);
                }
            }
            
            addAgent(agent) {
                this.connectedAgents.set(agent.id, agent);
                this.updateAgentsList();
            }
            
            removeAgent(agentId) {
                this.connectedAgents.delete(agentId);
                this.updateAgentsList();
            }
            
            updateAgentsList() {
                const agentsList = document.getElementById('agentsList');
                
                if (this.connectedAgents.size === 0) {
                    agentsList.innerHTML = '<p style="text-align: center; opacity: 0.7;">No agents connected</p>';
                    return;
                }
                
                agentsList.innerHTML = '';
                this.connectedAgents.forEach(agent => {
                    const agentDiv = document.createElement('div');
                    agentDiv.className = 'agent-item';
                    agentDiv.innerHTML = \`
                        <div>
                            <span class="agent-status \${agent.status === 'active' ? '' : 'inactive'}"></span>
                            <strong>\${agent.id}</strong>
                        </div>
                        <div style="font-size: 0.9em; opacity: 0.8;">\${agent.type}</div>
                        <div style="font-size: 0.8em; opacity: 0.7;">Capabilities: \${agent.capabilities.join(', ')}</div>
                    \`;
                    agentsList.appendChild(agentDiv);
                });
            }
            
            updateStatus(connected) {
                const indicator = document.getElementById('statusIndicator');
                if (connected) {
                    indicator.classList.add('active');
                } else {
                    indicator.classList.remove('active');
                }
            }
            
            updateSessionInfo() {
                const sessionIdEl = document.getElementById('sessionId');
                const statusEl = document.getElementById('sessionStatus');
                const durationEl = document.getElementById('sessionDuration');
                
                sessionIdEl.textContent = this.sessionId || 'Not started';
                statusEl.textContent = this.isSharing ? 'Active' : 'Inactive';
                
                if (this.sessionStartTime) {
                    const duration = Date.now() - this.sessionStartTime;
                    const hours = Math.floor(duration / 3600000);
                    const minutes = Math.floor((duration % 3600000) / 60000);
                    const seconds = Math.floor((duration % 60000) / 1000);
                    durationEl.textContent = \`\${hours.toString().padStart(2, '0')}:\${minutes.toString().padStart(2, '0')}:\${seconds.toString().padStart(2, '0')}\`;
                } else {
                    durationEl.textContent = '00:00:00';
                }
            }
            
            async startScreenShare() {
                try {
                    this.showLoading(true);
                    
                    const response = await fetch('/api/sessions/start', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({
                            sessionType: 'screen_share'
                        })
                    });
                    
                    const result = await response.json();
                    
                    if (result.success) {
                        this.sessionId = result.sessionId;
                        this.isSharing = true;
                        this.sessionStartTime = Date.now();
                        
                        // Start periodic screen capture
                        this.captureInterval = setInterval(() => {
                            this.captureScreen();
                        }, 2000); // Capture every 2 seconds
                        
                        this.showMessage('Screen sharing started successfully!', 'success');
                    } else {
                        this.showMessage('Failed to start screen sharing', 'error');
                    }
                } catch (error) {
                    this.showMessage(\`Error: \${error.message}\`, 'error');
                } finally {
                    this.showLoading(false);
                }
            }
            
            async captureScreen() {
                if (!this.sessionId) return;
                
                try {
                    const response = await fetch('/api/screen/capture', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({
                            sessionId: this.sessionId,
                            quality: 'medium'
                        })
                    });
                    
                    const result = await response.json();
                    
                    if (result.success) {
                        this.displayScreenCapture(result.screenshot);
                    }
                } catch (error) {
                    console.error('Screen capture error:', error);
                }
            }
            
            displayScreenCapture(screenshotData) {
                const screenImage = document.getElementById('screenImage');
                const noScreen = document.getElementById('noScreen');
                
                screenImage.src = \`data:image/png;base64,\${screenshotData}\`;
                screenImage.style.display = 'block';
                noScreen.style.display = 'none';
            }
            
            stopScreenShare() {
                if (this.captureInterval) {
                    clearInterval(this.captureInterval);
                    this.captureInterval = null;
                }
                
                this.isSharing = false;
                this.sessionStartTime = null;
                
                if (this.sessionId) {
                    fetch(\`/api/sessions/\${this.sessionId}/stop\`, {
                        method: 'POST'
                    });
                }
                
                this.showMessage('Screen sharing stopped', 'success');
            }
            
            showLoading(show) {
                const loading = document.getElementById('loading');
                loading.style.display = show ? 'block' : 'none';
            }
            
            showMessage(message, type) {
                const messageDiv = document.createElement('div');
                messageDiv.className = \`\${type} message\`;
                messageDiv.textContent = message;
                
                document.body.appendChild(messageDiv);
                
                setTimeout(() => {
                    document.body.removeChild(messageDiv);
                }, 5000);
            }
        }
        
        // Global functions for buttons
        let screenShare;
        
        window.addEventListener('load', () => {
            screenShare = new AIScreenShare();
        });
        
        function startScreenShare() {
            screenShare.startScreenShare();
        }
        
        function captureScreen() {
            screenShare.captureScreen();
        }
        
        function stopScreenShare() {
            screenShare.stopScreenShare();
        }
    </script>
</body>
</html>`;
    }

    start() {
        this.server = this.app.listen(this.port, () => {
            console.log(` AI Screen Share Server running on port ${this.port}`);
            console.log(` Access the interface at: http://localhost:${this.port}`);
            console.log(` WebSocket endpoint: ws://localhost:${this.port}/ws`);
        });
    }

    stop() {
        if (this.server) {
            this.server.close();
        }
        if (this.wss) {
            this.wss.close();
        }
    }
}

// Start the server if this file is run directly
if (require.main === module) {
    const server = new AIScreenShareServer();
    server.start();
    
    // Graceful shutdown
    process.on('SIGINT', () => {
        console.log('\n Shutting down AI Screen Share Server...');
        server.stop();
        process.exit(0);
    });
}

module.exports = AIScreenShareServer;

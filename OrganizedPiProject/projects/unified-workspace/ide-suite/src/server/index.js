import express from 'express';
import { WebSocketServer } from 'ws';
import cors from 'cors';
import helmet from 'helmet';
import compression from 'compression';
import { RateLimiterMemory } from 'rate-limiter-flexible';
import { v4 as uuidv4 } from 'uuid';
import * as fs from 'fs/promises';
import * as path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

class SecureIDEServer {
    constructor() {
        this.app = express();
        this.port = process.env.PORT || 3000;
        this.wsPort = process.env.WS_PORT || 3001;
        this.workspace = process.env.WORKSPACE || './workspace';
        this.clients = new Map();
        this.rateLimiter = new RateLimiterMemory({
            keyPrefix: 'secure_ide',
            points: 100, // Number of requests
            duration: 60, // Per 60 seconds
        });
        
        this.setupMiddleware();
        this.setupRoutes();
        this.setupWebSocket();
        this.initializeWorkspace();
    }

    setupMiddleware() {
        // Security middleware
        this.app.use(helmet({
            contentSecurityPolicy: {
                directives: {
                    defaultSrc: ["'self'"],
                    styleSrc: ["'self'", "'unsafe-inline'", "https://cdnjs.cloudflare.com", "https://fonts.googleapis.com"],
                    scriptSrc: ["'self'", "'unsafe-inline'", "https://cdnjs.cloudflare.com"],
                    fontSrc: ["'self'", "https://fonts.gstatic.com", "https://cdnjs.cloudflare.com"],
                    imgSrc: ["'self'", "data:", "https:"],
                    connectSrc: ["'self'", "ws:", "wss:"]
                }
            }
        }));

        this.app.use(cors({
            origin: process.env.ALLOWED_ORIGINS?.split(',') || ['http://localhost:3000'],
            credentials: true
        }));

        this.app.use(compression());
        this.app.use(express.json({ limit: '10mb' }));
        this.app.use(express.urlencoded({ extended: true, limit: '10mb' }));

        // Rate limiting
        this.app.use(async (req, res, next) => {
            try {
                await this.rateLimiter.consume(req.ip);
                next();
            } catch (rejRes) {
                res.status(429).json({ error: 'Too many requests' });
            }
        });
    }

    setupRoutes() {
        // Serve static files
        this.app.use(express.static(path.join(__dirname, '../')));

        // API routes
        this.app.get('/api/status', (req, res) => {
            res.json({
                status: 'running',
                security: 'high',
                ai: 'enabled',
                timestamp: new Date().toISOString()
            });
        });

        this.app.get('/api/workspace', async (req, res) => {
            try {
                const files = await this.getWorkspaceFiles(this.workspace);
                res.json({ files });
            } catch (error) {
                res.status(500).json({ error: error.message });
            }
        });

        this.app.post('/api/file', async (req, res) => {
            try {
                const { filePath, content } = req.body;
                const fullPath = path.join(this.workspace, filePath);
                
                // Security check
                if (!this.isPathSafe(fullPath)) {
                    return res.status(403).json({ error: 'Access denied' });
                }

                await fs.writeFile(fullPath, content, 'utf8');
                res.json({ success: true });
            } catch (error) {
                res.status(500).json({ error: error.message });
            }
        });

        this.app.get('/api/file/*', async (req, res) => {
            try {
                const filePath = req.params[0];
                const fullPath = path.join(this.workspace, filePath);
                
                if (!this.isPathSafe(fullPath)) {
                    return res.status(403).json({ error: 'Access denied' });
                }

                const content = await fs.readFile(fullPath, 'utf8');
                const language = this.detectLanguage(filePath);
                
                res.json({ content, language, filePath });
            } catch (error) {
                res.status(500).json({ error: error.message });
            }
        });

        this.app.post('/api/ai', async (req, res) => {
            try {
                const { type, content, context } = req.body;
                const response = await this.processAIRequest(type, content, context);
                res.json(response);
            } catch (error) {
                res.status(500).json({ error: error.message });
            }
        });

        // Catch all route for SPA
        this.app.get('*', (req, res) => {
            res.sendFile(path.join(__dirname, '../index.html'));
        });
    }

    setupWebSocket() {
        this.wss = new WebSocketServer({ port: this.wsPort });
        
        this.wss.on('connection', (ws, req) => {
            const clientId = uuidv4();
            this.clients.set(clientId, ws);
            
            console.log(`Client connected: ${clientId}`);
            
            ws.on('message', async (data) => {
                try {
                    const message = JSON.parse(data.toString());
                    await this.handleWebSocketMessage(clientId, message);
                } catch (error) {
                    console.error('WebSocket message error:', error);
                    this.sendToClient(ws, { type: 'error', data: { error: 'Invalid message format' } });
                }
            });

            ws.on('close', () => {
                console.log(`Client disconnected: ${clientId}`);
                this.clients.delete(clientId);
            });

            ws.on('error', (error) => {
                console.error(`WebSocket error for client ${clientId}:`, error);
                this.clients.delete(clientId);
            });

            // Send welcome message
            this.sendToClient(ws, {
                type: 'status',
                data: { message: 'Connected to Secure IDE', clientId }
            });
        });
    }

    async handleWebSocketMessage(clientId, message) {
        const ws = this.clients.get(clientId);
        if (!ws) return;

        try {
            switch (message.type) {
                case 'load_workspace':
                    await this.handleLoadWorkspace(ws);
                    break;
                case 'open_file':
                    await this.handleOpenFile(ws, message.data);
                    break;
                case 'save_file':
                    await this.handleSaveFile(ws, message.data);
                    break;
                case 'create_file':
                    await this.handleCreateFile(ws, message.data);
                    break;
                case 'create_folder':
                    await this.handleCreateFolder(ws, message.data);
                    break;
                case 'search_files':
                    await this.handleSearchFiles(ws, message.data);
                    break;
                case 'ai_request':
                    await this.handleAIRequest(ws, message.data);
                    break;
                case 'terminal_input':
                    await this.handleTerminalInput(ws, message.data);
                    break;
                default:
                    this.sendToClient(ws, { type: 'error', data: { error: 'Unknown message type' } });
            }
        } catch (error) {
            console.error('Error handling WebSocket message:', error);
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleLoadWorkspace(ws) {
        try {
            const files = await this.getWorkspaceFiles(this.workspace);
            this.sendToClient(ws, { type: 'file_tree', data: { files } });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleOpenFile(ws, data) {
        try {
            const { filePath } = data;
            const fullPath = path.join(this.workspace, filePath);
            
            if (!this.isPathSafe(fullPath)) {
                this.sendToClient(ws, { type: 'error', data: { error: 'Access denied' } });
                return;
            }

            const content = await fs.readFile(fullPath, 'utf8');
            const language = this.detectLanguage(filePath);
            
            this.sendToClient(ws, {
                type: 'file_content',
                data: { filePath, content, language }
            });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleSaveFile(ws, data) {
        try {
            const { filePath, content } = data;
            const fullPath = path.join(this.workspace, filePath);
            
            if (!this.isPathSafe(fullPath)) {
                this.sendToClient(ws, { type: 'error', data: { error: 'Access denied' } });
                return;
            }

            await fs.writeFile(fullPath, content, 'utf8');
            this.sendToClient(ws, { type: 'file_saved', data: { filePath } });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleCreateFile(ws, data) {
        try {
            const { fileName } = data;
            const fullPath = path.join(this.workspace, fileName);
            
            if (!this.isPathSafe(fullPath)) {
                this.sendToClient(ws, { type: 'error', data: { error: 'Access denied' } });
                return;
            }

            await fs.writeFile(fullPath, '', 'utf8');
            this.sendToClient(ws, { type: 'file_created', data: { fileName } });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleCreateFolder(ws, data) {
        try {
            const { folderName } = data;
            const fullPath = path.join(this.workspace, folderName);
            
            if (!this.isPathSafe(fullPath)) {
                this.sendToClient(ws, { type: 'error', data: { error: 'Access denied' } });
                return;
            }

            await fs.mkdir(fullPath, { recursive: true });
            this.sendToClient(ws, { type: 'folder_created', data: { folderName } });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleSearchFiles(ws, data) {
        try {
            const { query } = data;
            const results = await this.searchFiles(this.workspace, query);
            this.sendToClient(ws, { type: 'search_results', data: { results } });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleAIRequest(ws, data) {
        try {
            const { type, content, context } = data;
            const response = await this.processAIRequest(type, content, context);
            this.sendToClient(ws, { type: 'ai_response', data: response });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async handleTerminalInput(ws, data) {
        try {
            const { input } = data;
            // Process terminal input (placeholder)
            this.sendToClient(ws, { type: 'terminal_output', data: { output: `$ ${input}` } });
        } catch (error) {
            this.sendToClient(ws, { type: 'error', data: { error: error.message } });
        }
    }

    async processAIRequest(type, content, context) {
        // Local AI processing (placeholder for actual AI integration)
        switch (type) {
            case 'chat':
                return {
                    type: 'chat',
                    content: `I understand you want to: ${content}. Here's my response based on local processing.`,
                    timestamp: new Date().toISOString()
                };
            case 'code_completion':
                return {
                    type: 'suggestions',
                    suggestions: [
                        { text: 'console.log()', type: 'function', confidence: 0.9 },
                        { text: 'const ', type: 'keyword', confidence: 0.8 },
                        { text: 'function ', type: 'keyword', confidence: 0.7 }
                    ]
                };
            case 'code_review':
                return {
                    type: 'review',
                    content: 'Code review completed locally. No issues found.',
                    suggestions: []
                };
            default:
                return {
                    type: 'response',
                    content: 'AI request processed locally for security.',
                    timestamp: new Date().toISOString()
                };
        }
    }

    async getWorkspaceFiles(dir) {
        const files = [];
        const entries = await fs.readdir(dir, { withFileTypes: true });
        
        for (const entry of entries) {
            const fullPath = path.join(dir, entry.name);
            const relativePath = path.relative(this.workspace, fullPath);
            
            if (entry.isDirectory()) {
                files.push({
                    name: entry.name,
                    path: relativePath,
                    isDirectory: true,
                    children: await this.getWorkspaceFiles(fullPath)
                });
            } else {
                files.push({
                    name: entry.name,
                    path: relativePath,
                    isDirectory: false
                });
            }
        }
        
        return files;
    }

    async searchFiles(dir, query) {
        const results = [];
        const entries = await fs.readdir(dir, { withFileTypes: true });
        
        for (const entry of entries) {
            const fullPath = path.join(dir, entry.name);
            
            if (entry.isDirectory()) {
                results.push(...await this.searchFiles(fullPath, query));
            } else {
                try {
                    const content = await fs.readFile(fullPath, 'utf8');
                    if (content.toLowerCase().includes(query.toLowerCase())) {
                        results.push({
                            fileName: entry.name,
                            path: path.relative(this.workspace, fullPath),
                            matches: content.toLowerCase().split(query.toLowerCase()).length - 1
                        });
                    }
                } catch (error) {
                    // Skip files that can't be read
                }
            }
        }
        
        return results;
    }

    detectLanguage(filePath) {
        const ext = path.extname(filePath).toLowerCase();
        const languageMap = {
            '.js': 'javascript',
            '.ts': 'typescript',
            '.jsx': 'javascript',
            '.tsx': 'typescript',
            '.py': 'python',
            '.java': 'java',
            '.cpp': 'cpp',
            '.c': 'c',
            '.cs': 'csharp',
            '.php': 'php',
            '.rb': 'ruby',
            '.go': 'go',
            '.rs': 'rust',
            '.html': 'html',
            '.css': 'css',
            '.scss': 'scss',
            '.json': 'json',
            '.xml': 'xml',
            '.yaml': 'yaml',
            '.yml': 'yaml',
            '.md': 'markdown'
        };
        
        return languageMap[ext] || 'plaintext';
    }

    isPathSafe(filePath) {
        const resolvedPath = path.resolve(filePath);
        const workspacePath = path.resolve(this.workspace);
        return resolvedPath.startsWith(workspacePath);
    }

    sendToClient(ws, message) {
        if (ws.readyState === 1) { // WebSocket.OPEN
            ws.send(JSON.stringify(message));
        }
    }

    async initializeWorkspace() {
        try {
            await fs.mkdir(this.workspace, { recursive: true });
            console.log(`Workspace initialized at: ${this.workspace}`);
        } catch (error) {
            console.error('Failed to initialize workspace:', error);
        }
    }

    start() {
        this.app.listen(this.port, () => {
            console.log(`Secure IDE server running on port ${this.port}`);
            console.log(`WebSocket server running on port ${this.wsPort}`);
            console.log(`Workspace: ${this.workspace}`);
        });
    }
}

// Start the server
const server = new SecureIDEServer();
server.start();

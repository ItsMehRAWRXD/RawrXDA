const express = require('express');
const fs = require('fs').promises;
const path = require('path');
const { exec } = require('child_process');
const util = require('util');
const execAsync = util.promisify(exec);

class IDEBackendServer {
    constructor() {
        this.app = express();
        this.port = process.env.PORT || 3001;
        this.workspaceDir = path.join(__dirname, 'workspace');
        this.setupMiddleware();
        this.setupRoutes();
    }

    setupMiddleware() {
        this.app.use(express.json({ limit: '50mb' }));
        this.app.use(express.urlencoded({ extended: true }));
        
        // CORS middleware
        this.app.use((req, res, next) => {
            res.header('Access-Control-Allow-Origin', '*');
            res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
            res.header('Access-Control-Allow-Headers', 'Content-Type, Authorization');
            
            if (req.method === 'OPTIONS') {
                res.sendStatus(200);
            } else {
                next();
            }
        });

        // Ensure workspace directory exists
        this.ensureWorkspaceDir();
    }

    async ensureWorkspaceDir() {
        try {
            await fs.access(this.workspaceDir);
        } catch {
            await fs.mkdir(this.workspaceDir, { recursive: true });
            console.log(`Created workspace directory: ${this.workspaceDir}`);
        }
    }

    setupRoutes() {
        // Health check
        this.app.get('/health', (req, res) => {
            res.json({ status: 'ok', timestamp: new Date().toISOString() });
        });

        // File operations
        this.app.get('/api/files', this.listFiles.bind(this));
        this.app.get('/api/files/*', this.getFile.bind(this));
        this.app.post('/api/files/*', this.saveFile.bind(this));
        this.app.delete('/api/files/*', this.deleteFile.bind(this));
        this.app.post('/api/mkdir', this.createDirectory.bind(this));

        // Code execution
        this.app.post('/api/execute', this.executeCode.bind(this));
        this.app.post('/api/run-file', this.runFile.bind(this));

        // File system operations
        this.app.post('/api/upload', this.uploadFile.bind(this));
        this.app.get('/api/download/*', this.downloadFile.bind(this));

        // Project operations
        this.app.post('/api/project/new', this.createProject.bind(this));
        this.app.get('/api/project/templates', this.getProjectTemplates.bind(this));
    }

    async listFiles(req, res) {
        try {
            const dirPath = req.query.path ? 
                path.join(this.workspaceDir, req.query.path) : 
                this.workspaceDir;

            const items = await fs.readdir(dirPath, { withFileTypes: true });
            const files = [];

            for (const item of items) {
                const fullPath = path.join(dirPath, item.name);
                const stats = await fs.stat(fullPath);
                
                files.push({
                    name: item.name,
                    type: item.isDirectory() ? 'directory' : 'file',
                    size: stats.size,
                    modified: stats.mtime,
                    path: path.relative(this.workspaceDir, fullPath)
                });
            }

            res.json({ success: true, files });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    async getFile(req, res) {
        try {
            const filePath = path.join(this.workspaceDir, req.params[0]);
            const content = await fs.readFile(filePath, 'utf8');
            
            res.json({ 
                success: true, 
                content,
                path: req.params[0]
            });
        } catch (error) {
            res.status(404).json({ success: false, error: 'File not found' });
        }
    }

    async saveFile(req, res) {
        try {
            const filePath = path.join(this.workspaceDir, req.params[0]);
            const { content } = req.body;

            // Ensure directory exists
            await fs.mkdir(path.dirname(filePath), { recursive: true });
            
            await fs.writeFile(filePath, content, 'utf8');
            
            res.json({ 
                success: true, 
                message: 'File saved successfully',
                path: req.params[0]
            });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    async deleteFile(req, res) {
        try {
            const filePath = path.join(this.workspaceDir, req.params[0]);
            const stats = await fs.stat(filePath);
            
            if (stats.isDirectory()) {
                await fs.rmdir(filePath, { recursive: true });
            } else {
                await fs.unlink(filePath);
            }
            
            res.json({ 
                success: true, 
                message: 'File deleted successfully',
                path: req.params[0]
            });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    async createDirectory(req, res) {
        try {
            const { path: dirPath } = req.body;
            const fullPath = path.join(this.workspaceDir, dirPath);
            
            await fs.mkdir(fullPath, { recursive: true });
            
            res.json({ 
                success: true, 
                message: 'Directory created successfully',
                path: dirPath
            });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    async executeCode(req, res) {
        try {
            const { code, language } = req.body;
            let result;

            switch (language) {
                case 'javascript':
                case 'js':
                    result = await this.executeJavaScript(code);
                    break;
                case 'python':
                    result = await this.executePython(code);
                    break;
                case 'node':
                    result = await this.executeNode(code);
                    break;
                default:
                    throw new Error(`Unsupported language: ${language}`);
            }

            res.json({ success: true, result });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    async executeJavaScript(code) {
        try {
            // Create a safe execution context
            const vm = require('vm');
            const sandbox = {
                console: {
                    log: (...args) => sandbox._output.push(args.join(' ')),
                    error: (...args) => sandbox._errors.push(args.join(' ')),
                    warn: (...args) => sandbox._warnings.push(args.join(' '))
                },
                _output: [],
                _errors: [],
                _warnings: []
            };

            vm.createContext(sandbox);
            const result = vm.runInContext(code, sandbox, { timeout: 5000 });

            return {
                output: sandbox._output,
                errors: sandbox._errors,
                warnings: sandbox._warnings,
                result: result
            };
        } catch (error) {
            return {
                output: [],
                errors: [error.message],
                warnings: [],
                result: null
            };
        }
    }

    async executePython(code) {
        try {
            const tempFile = path.join(__dirname, 'temp', `script_${Date.now()}.py`);
            await fs.mkdir(path.dirname(tempFile), { recursive: true });
            await fs.writeFile(tempFile, code);

            const { stdout, stderr } = await execAsync(`python "${tempFile}"`);
            
            // Clean up temp file
            await fs.unlink(tempFile);

            return {
                output: stdout ? [stdout] : [],
                errors: stderr ? [stderr] : [],
                warnings: [],
                result: stdout
            };
        } catch (error) {
            return {
                output: [],
                errors: [error.message],
                warnings: [],
                result: null
            };
        }
    }

    async executeNode(code) {
        try {
            const tempFile = path.join(__dirname, 'temp', `script_${Date.now()}.js`);
            await fs.mkdir(path.dirname(tempFile), { recursive: true });
            await fs.writeFile(tempFile, code);

            const { stdout, stderr } = await execAsync(`node "${tempFile}"`);
            
            // Clean up temp file
            await fs.unlink(tempFile);

            return {
                output: stdout ? [stdout] : [],
                errors: stderr ? [stderr] : [],
                warnings: [],
                result: stdout
            };
        } catch (error) {
            return {
                output: [],
                errors: [error.message],
                warnings: [],
                result: null
            };
        }
    }

    async runFile(req, res) {
        try {
            const { filePath } = req.body;
            const fullPath = path.join(this.workspaceDir, filePath);
            const ext = path.extname(filePath).toLowerCase();

            let result;
            switch (ext) {
                case '.js':
                    result = await execAsync(`node "${fullPath}"`);
                    break;
                case '.py':
                    result = await execAsync(`python "${fullPath}"`);
                    break;
                case '.html':
                    result = { stdout: 'HTML file opened in browser', stderr: '' };
                    break;
                default:
                    throw new Error(`Cannot execute files with extension: ${ext}`);
            }

            res.json({ 
                success: true, 
                output: result.stdout,
                errors: result.stderr
            });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    async uploadFile(req, res) {
        // This would handle file uploads
        res.json({ success: false, error: 'File upload not implemented yet' });
    }

    async downloadFile(req, res) {
        try {
            const filePath = path.join(this.workspaceDir, req.params[0]);
            res.download(filePath);
        } catch (error) {
            res.status(404).json({ success: false, error: 'File not found' });
        }
    }

    async createProject(req, res) {
        try {
            const { name, template } = req.body;
            const projectPath = path.join(this.workspaceDir, name);

            await fs.mkdir(projectPath, { recursive: true });

            const templates = {
                'vanilla-js': {
                    'index.html': `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>${name}</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <h1>Hello, ${name}!</h1>
    <script src="script.js"></script>
</body>
</html>`,
                    'style.css': `body {
    font-family: Arial, sans-serif;
    margin: 0;
    padding: 20px;
    background-color: #f0f0f0;
}

h1 {
    color: #333;
    text-align: center;
}`,
                    'script.js': `console.log('Welcome to ${name}!');

document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM loaded');
});`
                },
                'node-app': {
                    'package.json': JSON.stringify({
                        name: name,
                        version: '1.0.0',
                        description: '',
                        main: 'index.js',
                        scripts: {
                            start: 'node index.js'
                        }
                    }, null, 2),
                    'index.js': `const http = require('http');

const server = http.createServer((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end('<h1>Hello from ${name}!</h1>');
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(\`Server running on port \${PORT}\`);
});`
                }
            };

            const selectedTemplate = templates[template] || templates['vanilla-js'];

            for (const [fileName, content] of Object.entries(selectedTemplate)) {
                const filePath = path.join(projectPath, fileName);
                await fs.writeFile(filePath, content, 'utf8');
            }

            res.json({ 
                success: true, 
                message: 'Project created successfully',
                path: name
            });
        } catch (error) {
            res.status(500).json({ success: false, error: error.message });
        }
    }

    getProjectTemplates(req, res) {
        const templates = [
            {
                id: 'vanilla-js',
                name: 'Vanilla JavaScript',
                description: 'Basic HTML, CSS, and JavaScript project'
            },
            {
                id: 'node-app',
                name: 'Node.js Application',
                description: 'Simple Node.js server application'
            }
        ];

        res.json({ success: true, templates });
    }

    start() {
        this.app.listen(this.port, () => {
            console.log(`IDE Backend Server running on http://localhost:${this.port}`);
            console.log(`Workspace directory: ${this.workspaceDir}`);
            console.log('Available endpoints:');
            console.log('  GET  /health');
            console.log('  GET  /api/files');
            console.log('  POST /api/execute');
            console.log('  POST /api/project/new');
        });
    }
}

// Start the server
const server = new IDEBackendServer();
server.start();
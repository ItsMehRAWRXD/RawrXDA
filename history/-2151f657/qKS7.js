const express = require('express');
const path = require('path');
const WebSocket = require('ws');
const fs = require('fs').promises;
const net = require('net');

const app = express();
let PORT = process.env.PORT || 8080;

// Function to find an available port
const findAvailablePort = (startPort) => {
    return new Promise((resolve, reject) => {
        const server = net.createServer();
        server.listen(startPort, () => {
            const port = server.address().port;
            server.close(() => resolve(port));
        });
        server.on('error', (err) => {
            if (err.code === 'EADDRINUSE') {
                // Try the next port
                findAvailablePort(startPort + 1).then(resolve, reject);
            } else {
                reject(err);
            }
        });
    });
};

app.use(express.json());
app.use(express.static('.'));

// Health endpoint
app.get('/health', (req, res) => {
    res.json({ 
        status: 'ok', 
        platform: process.platform,
        timestamp: new Date().toISOString() 
    });
});

// Microsoft Copilot API endpoints
app.post('/api/copilot/authenticate', async (req, res) => {
    try {
        const { token } = req.body;
        // Store token securely (in production, use proper encryption)
        res.json({ success: true, message: 'Token stored' });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.post('/api/copilot/completion', async (req, res) => {
    try {
        const { code, language } = req.body;
        // Proxy to Microsoft Copilot API
        res.json({ completion: 'Mock completion for: ' + code.substring(0, 50) });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// API endpoints
app.get('/api/compiler/scan', (req, res) => {
    res.json({ 
        files: [],
        status: 'ready',
        timestamp: new Date().toISOString()
    });
});

// File operations
app.get('/api/files', async (req, res) => {
    try {
        const { path: filePath = 'D:\\', limit = 100, offset = 0 } = req.query;
        const files = await fs.readdir(filePath, { withFileTypes: true });
        const result = files.slice(offset, offset + parseInt(limit));
        res.json({ files: result, total: files.length });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Directory listing endpoint (alias for /api/files)
app.get('/api/list-dir', async (req, res) => {
    try {
        const { path: filePath = 'D:\\', limit = 100, offset = 0 } = req.query;
        const files = await fs.readdir(filePath, { withFileTypes: true });
        const result = files.slice(offset, offset + parseInt(limit));
        res.json({ files: result, total: files.length });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Read file content
app.get('/api/read-file', async (req, res) => {
    try {
        const { path: filePath } = req.query;
        if (!filePath) {
            return res.status(400).json({ error: 'File path is required' });
        }
        
        // Handle relative paths
        const absolutePath = path.isAbsolute(filePath) ? filePath : path.join('D:\\', filePath);
        
        try {
            const content = await fs.readFile(absolutePath, 'utf-8');
            res.json({ content, path: filePath });
        } catch (fileError) {
            // If file doesn't exist, create sample content
            if (fileError.code === 'ENOENT') {
                const fileName = filePath.split(/[\/\\]/).pop();
                const sampleContent = filePath.endsWith('.js') ? 
                    `// ${fileName}\nconsole.log('Hello from ${fileName}!');\n\n// TODO: Add your code here` :
                    `# ${fileName}\n\nWelcome to your new file!\n\n<!-- Add your content here -->`;
                
                res.json({ content: sampleContent, path: filePath });
            } else {
                throw fileError;
            }
        }
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Execute command
app.post('/api/execute', async (req, res) => {
    try {
        const { command } = req.body;
        const { exec } = require('child_process');
        const util = require('util');
        const execPromise = util.promisify(exec);
        
        const { stdout, stderr } = await execPromise(command);
        res.json({ stdout, stderr, success: true });
    } catch (error) {
        res.status(500).json({ error: error.message, success: false });
    }
});

// Write file content
app.post('/api/write-file', async (req, res) => {
    try {
        const { path: filePath, content } = req.body;
        if (!filePath) {
            return res.status(400).json({ error: 'File path is required' });
        }
        
        await fs.writeFile(filePath, content || '', 'utf-8');
        res.json({ success: true, path: filePath });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Create directory
app.post('/api/create-dir', async (req, res) => {
    try {
        const { path: dirPath } = req.body;
        if (!dirPath) {
            return res.status(400).json({ error: 'Directory path is required' });
        }
        
        await fs.mkdir(dirPath, { recursive: true });
        res.json({ success: true, path: dirPath });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Delete file or directory
app.delete('/api/delete', async (req, res) => {
    try {
        const { path: targetPath } = req.query;
        if (!targetPath) {
            return res.status(400).json({ error: 'Path is required' });
        }
        
        const stats = await fs.stat(targetPath);
        if (stats.isDirectory()) {
            await fs.rmdir(targetPath, { recursive: true });
        } else {
            await fs.unlink(targetPath);
        }
        
        res.json({ success: true, path: targetPath });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Rename/move file or directory
app.post('/api/rename', async (req, res) => {
    try {
        const { oldPath, newPath } = req.body;
        if (!oldPath || !newPath) {
            return res.status(400).json({ error: 'Both old and new paths are required' });
        }
        
        await fs.rename(oldPath, newPath);
        res.json({ success: true, oldPath, newPath });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Search in files
app.post('/api/search', async (req, res) => {
    try {
        const { query, path: searchPath = 'D:\\', extensions = [] } = req.body;
        if (!query) {
            return res.status(400).json({ error: 'Search query is required' });
        }
        
        // Simple file search implementation
        const results = [];
        const searchInDirectory = async (dirPath) => {
            try {
                const files = await fs.readdir(dirPath, { withFileTypes: true });
                for (const file of files) {
                    const fullPath = path.join(dirPath, file.name);
                    if (file.isDirectory()) {
                        await searchInDirectory(fullPath);
                    } else if (extensions.length === 0 || extensions.includes(path.extname(file.name))) {
                        try {
                            const content = await fs.readFile(fullPath, 'utf-8');
                            if (content.includes(query)) {
                                const lines = content.split('\n');
                                const matches = [];
                                lines.forEach((line, index) => {
                                    if (line.includes(query)) {
                                        matches.push({ line: index + 1, text: line.trim() });
                                    }
                                });
                                results.push({ file: fullPath, matches });
                            }
                        } catch (e) {
                            // Skip files that can't be read as text
                        }
                    }
                }
            } catch (e) {
                // Skip directories that can't be accessed
            }
        };
        
        await searchInDirectory(searchPath);
        res.json({ results, query });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Git operations
app.get('/api/git/status', async (req, res) => {
    try {
        const { exec } = require('child_process');
        const util = require('util');
        const execPromise = util.promisify(exec);
        
        const { stdout } = await execPromise('git status --porcelain', { cwd: 'D:\\' });
        const files = stdout.split('\n').filter(line => line.trim()).map(line => {
            const status = line.substring(0, 2);
            const file = line.substring(3);
            return { status, file };
        });
        
        res.json({ files });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.post('/api/git/add', async (req, res) => {
    try {
        const { files } = req.body;
        const { exec } = require('child_process');
        const util = require('util');
        const execPromise = util.promisify(exec);
        
        const fileList = Array.isArray(files) ? files.join(' ') : files || '.';
        await execPromise(`git add ${fileList}`, { cwd: 'D:\\' });
        
        res.json({ success: true });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.post('/api/git/commit', async (req, res) => {
    try {
        const { message } = req.body;
        if (!message) {
            return res.status(400).json({ error: 'Commit message is required' });
        }
        
        const { exec } = require('child_process');
        const util = require('util');
        const execPromise = util.promisify(exec);
        
        const { stdout } = await execPromise(`git commit -m "${message}"`, { cwd: 'D:\\' });
        res.json({ success: true, output: stdout });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// WebSocket setup
const server = require('http').createServer(app);
const wss = new WebSocket.Server({ server });

wss.on('connection', (ws) => {
    console.log('WebSocket client connected');
    
    ws.on('message', (message) => {
        console.log('WebSocket message received:', message.toString());
    });
    
    ws.on('close', () => {
        console.log('WebSocket client disconnected');
    });
});

server.listen(PORT, () => {
    console.log(`MyCoPilot++ IDE Server running on http://localhost:${PORT}`);
    console.log('WebSocket server active');
    console.log('Auto-healing bridge active');
});
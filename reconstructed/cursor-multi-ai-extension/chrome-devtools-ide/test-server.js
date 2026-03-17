const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 3000;

// MIME types for common file extensions
const mimeTypes = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.css': 'text/css',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon'
};

const server = http.createServer((req, res) => {
    console.log(`${req.method} ${req.url}`);

    // Handle API endpoints for the IDE
    if (req.url.startsWith('/api/')) {
        res.setHeader('Content-Type', 'application/json');
        
        if (req.url === '/api/execute') {
            // Mock code execution API
            res.writeHead(200);
            res.end(JSON.stringify({
                success: true,
                result: "Code execution simulated successfully (test server)"
            }));
            return;
        }
        
        if (req.url === '/api/performance') {
            // Mock performance API
            res.writeHead(200);
            res.end(JSON.stringify({
                success: true,
                metrics: {
                    loadTime: 150,
                    domContentLoaded: 120,
                    JSHeapUsedSize: 2048576,
                    JSHeapTotalSize: 4194304,
                    Nodes: 45
                }
            }));
            return;
        }
        
        if (req.url === '/api/debug') {
            // Mock debug/screenshot API
            res.writeHead(200);
            res.end(JSON.stringify({
                success: true,
                result: "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
            }));
            return;
        }
        
        // Unknown API endpoint
        res.writeHead(404);
        res.end(JSON.stringify({ error: 'API endpoint not found' }));
        return;
    }

    // Serve static files
    let filePath = path.join(__dirname, 'public', req.url === '/' ? 'index.html' : req.url);
    
    // Security: prevent directory traversal
    if (filePath.indexOf(path.join(__dirname, 'public')) !== 0) {
        res.writeHead(403);
        res.end('Forbidden');
        return;
    }

    fs.stat(filePath, (err, stats) => {
        if (err || !stats.isFile()) {
            res.writeHead(404);
            res.end('File not found');
            return;
        }

        const ext = path.extname(filePath).toLowerCase();
        const contentType = mimeTypes[ext] || 'application/octet-stream';

        res.setHeader('Content-Type', contentType);
        res.setHeader('Cache-Control', 'no-cache'); // Disable caching for development
        
        const readStream = fs.createReadStream(filePath);
        readStream.pipe(res);
        
        readStream.on('error', (err) => {
            console.error('Read stream error:', err);
            res.writeHead(500);
            res.end('Internal Server Error');
        });
    });
});

server.listen(PORT, () => {
    console.log(`Chrome DevTools IDE Test Server running at http://localhost:${PORT}`);
    console.log('Open your browser and navigate to the URL above to test the IDE');
    console.log('Press Ctrl+C to stop the server');
});

server.on('error', (err) => {
    console.error('Server error:', err);
});
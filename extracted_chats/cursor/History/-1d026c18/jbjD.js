// Quick start script for BigDaddyG Server
const http = require('http');

const PORT = 11441;
const OLLAMA_URL = 'http://localhost:11434';

const server = http.createServer(async (req, res) => {
    // Enable CORS
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }

    // Proxy to Ollama
    if (req.url?.startsWith('/v1/')) {
        const ollamaPath = req.url.replace('/v1/', '/api/');
        const proxyUrl = `${OLLAMA_URL}${ollamaPath}`;

        console.log(`[${new Date().toISOString()}] Proxying ${req.method} ${proxyUrl}`);

        const proxyReq = http.request(proxyUrl, {
            method: req.method,
            headers: {
                'Content-Type': 'application/json',
            }
        }, (proxyRes) => {
            res.writeHead(proxyRes.statusCode || 200, {
                'Content-Type': 'application/json'
            });
            proxyRes.pipe(res);
        });

        proxyReq.on('error', (err) => {
            console.error('[ERROR]', err.message);
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: err.message }));
        });

        if (req.method === 'POST') {
            req.pipe(proxyReq);
        } else {
            proxyReq.end();
        }
    } else {
        res.writeHead(404, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Not found' }));
    }
});

server.listen(PORT, () => {
    console.log(`\n✅ BigDaddyG Server Proxy`);
    console.log(`📍 Running on http://localhost:${PORT}`);
    console.log(`🔗 Proxying to Ollama at ${OLLAMA_URL}`);
    console.log(`\nPress Ctrl+C to stop\n`);
});

server.on('error', (err) => {
    if (err.code === 'EADDRINUSE') {
        console.log(`⚠️  Port ${PORT} already in use - server likely already running`);
    } else {
        console.error('[ERROR]', err);
    }
});

process.on('SIGINT', () => {
    console.log('\n\nStopping BigDaddyG Server...');
    server.close(() => {
        console.log('✅ Server stopped');
        process.exit(0);
    });
});

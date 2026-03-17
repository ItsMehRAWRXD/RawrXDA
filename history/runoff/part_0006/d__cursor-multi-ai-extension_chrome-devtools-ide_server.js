const express = require('express');
const WebSocket = require('ws');
const puppeteer = require('puppeteer');
const path = require('path');

class ChromeDevToolsIDE {
    constructor() {
        this.app = express();
        this.browser = null;
        this.page = null;
        this.wss = null;
    }

    async init() {
        // Launch Chrome with DevTools
        this.browser = await puppeteer.launch({
            headless: false,
            devtools: true,
            args: ['--remote-debugging-port=9222', '--disable-web-security']
        });

        this.page = await this.browser.newPage();
        
        // Enable DevTools domains
        const client = await this.page.target().createCDPSession();
        await client.send('Runtime.enable');
        await client.send('Network.enable');
        await client.send('Performance.enable');
        await client.send('Console.enable');

        this.setupServer();
        this.setupWebSocket();
    }

    setupServer() {
        this.app.use(express.static(path.join(__dirname, 'public')));
        this.app.use(express.json());

        // API endpoints
        this.app.post('/api/execute', async (req, res) => {
            try {
                const { code, language } = req.body;
                const result = await this.executeCode(code, language);
                res.json({ success: true, result });
            } catch (error) {
                res.json({ success: false, error: error.message });
            }
        });

        this.app.post('/api/debug', async (req, res) => {
            try {
                const { action, params } = req.body;
                const result = await this.debugAction(action, params);
                res.json({ success: true, result });
            } catch (error) {
                res.json({ success: false, error: error.message });
            }
        });

        this.app.get('/api/performance', async (req, res) => {
            try {
                const metrics = await this.getPerformanceMetrics();
                res.json({ success: true, metrics });
            } catch (error) {
                res.json({ success: false, error: error.message });
            }
        });
    }

    setupWebSocket() {
        this.server = require('http').createServer(this.app);
        this.wss = new WebSocket.Server({ server: this.server });

        this.wss.on('connection', (ws) => {
            console.log('Client connected');
            
            ws.on('message', async (message) => {
                try {
                    const data = JSON.parse(message);
                    const result = await this.handleWebSocketMessage(data);
                    ws.send(JSON.stringify({ id: data.id, result }));
                } catch (error) {
                    ws.send(JSON.stringify({ id: data.id, error: error.message }));
                }
            });
        });

        server.listen(3001, () => {
            console.log('🚀 Chrome DevTools IDE running on http://localhost:3001');
            console.log('📡 WebSocket server running on port 3002');
        });
    }

    async executeCode(code, language) {
        switch (language) {
            case 'javascript':
                return await this.page.evaluate(code);
            case 'html':
                await this.page.setContent(code);
                return 'HTML rendered';
            case 'css':
                await this.page.addStyleTag({ content: code });
                return 'CSS applied';
            default:
                throw new Error(`Unsupported language: ${language}`);
        }
    }

    async debugAction(action, params) {
        switch (action) {
            case 'screenshot':
                return await this.page.screenshot({ encoding: 'base64' });
            case 'console':
                return await this.getConsoleLogs();
            case 'network':
                return await this.getNetworkRequests();
            case 'elements':
                return await this.inspectElements(params.selector);
            default:
                throw new Error(`Unknown debug action: ${action}`);
        }
    }

    async getPerformanceMetrics() {
        const metrics = await this.page.metrics();
        const timing = await this.page.evaluate(() => performance.timing);
        
        return {
            ...metrics,
            loadTime: timing.loadEventEnd - timing.navigationStart,
            domContentLoaded: timing.domContentLoadedEventEnd - timing.navigationStart,
            firstPaint: timing.responseEnd - timing.navigationStart
        };
    }

    async getConsoleLogs() {
        return new Promise((resolve) => {
            const logs = [];
            this.page.on('console', (msg) => {
                logs.push({
                    type: msg.type(),
                    text: msg.text(),
                    timestamp: Date.now()
                });
            });
            setTimeout(() => resolve(logs), 1000);
        });
    }

    async getNetworkRequests() {
        return new Promise((resolve) => {
            const requests = [];
            this.page.on('request', (req) => {
                requests.push({
                    url: req.url(),
                    method: req.method(),
                    headers: req.headers(),
                    timestamp: Date.now()
                });
            });
            setTimeout(() => resolve(requests), 1000);
        });
    }

    async inspectElements(selector) {
        return await this.page.evaluate((sel) => {
            const elements = document.querySelectorAll(sel);
            return Array.from(elements).map(el => ({
                tagName: el.tagName,
                className: el.className,
                id: el.id,
                textContent: el.textContent.substring(0, 100),
                attributes: Array.from(el.attributes).map(attr => ({
                    name: attr.name,
                    value: attr.value
                }))
            }));
        }, selector);
    }

    async handleWebSocketMessage(data) {
        const { type, payload } = data;
        
        switch (type) {
            case 'execute':
                return await this.executeCode(payload.code, payload.language);
            case 'debug':
                return await this.debugAction(payload.action, payload.params);
            case 'performance':
                return await this.getPerformanceMetrics();
            default:
                throw new Error(`Unknown message type: ${type}`);
        }
    }

    startServer() {
        const PORT = process.env.PORT || 3000;
        this.server.listen(PORT, () => {
            console.log(`Chrome DevTools IDE Server running on http://localhost:${PORT}`);
        });
    }
}

// Start the IDE
const ide = new ChromeDevToolsIDE();
ide.init().then(() => {
    ide.startServer();
}).catch(console.error);
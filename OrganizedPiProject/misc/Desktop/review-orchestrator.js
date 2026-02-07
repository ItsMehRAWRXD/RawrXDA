#!/usr/bin/env node
/**
 * Review Orchestrator - Multi-AI Code Review Service
 * 
 * Provides intelligent code review using multiple AI reviewers with configurable modes:
 * - offline: Returns deterministic review structure (no network calls)
 * - online: Uses configured AI API (default)
 * - local: Routes to local AI endpoint
 * 
 * Usage:
 *   POST /review with JSON body containing files and optional mode
 *   
 * Example:
 *   curl -X POST http://localhost:5050/review \
 *     -H 'Content-Type: application/json' \
 *     -d '{"files":["src/app.js"],"mode":"online"}'
 */

const { spawn } = require('child_process');
const http = require('http');
const url = require('url');
const fs = require('fs').promises;
const path = require('path');

// Configuration
const PORT = process.env.REVIEW_PORT || 5050;
const CLI = process.env.REVIEW_CLI || 'php';
const CLI_PATH = process.env.REVIEW_CLI_PATH || 'ultra_turbo_ai_cli.php';
const MODEL = process.env.REVIEW_MODEL || 'gemini-1.5-flash';
const USE_ENV = process.env.REVIEW_USE_ENV !== 'false';

// Reviewers configuration
const REVIEWERS = [
    {
        id: 'security',
        title: 'Security Reviewer',
        system: 'You are a security expert. Review code for vulnerabilities, security best practices, and potential security issues. Provide specific, actionable feedback.'
    },
    {
        id: 'performance',
        title: 'Performance Reviewer', 
        system: 'You are a performance expert. Review code for performance issues, optimization opportunities, and efficiency improvements. Focus on algorithmic complexity and resource usage.'
    },
    {
        id: 'readability',
        title: 'Readability Reviewer',
        system: 'You are a code quality expert. Review code for readability, maintainability, naming conventions, and overall code structure. Suggest improvements for clarity and maintainability.'
    },
    {
        id: 'correctness',
        title: 'Correctness Reviewer',
        system: 'You are a correctness expert. Review code for bugs, logical errors, edge cases, and potential runtime issues. Identify areas that might fail or behave unexpectedly.'
    }
];

/**
 * Choose mode based on request and environment
 */
function chooseMode(body) {
    const st = { default_mode: process.env.REVIEW_DEFAULT_MODE || 'online' };
    const m = (body.mode || st.default_mode || 'online').toLowerCase();
    return ['offline','online','local'].includes(m) ? m : 'online';
}

/**
 * Run CLI with specified parameters
 */
function runCli(prompt, { system, json = true, mode = 'online' }) {
    return new Promise((resolve, reject) => {
        const args = [ 
            CLI_PATH, 
            '--ask', prompt, 
            '--model', MODEL, 
            '--timeout', '45', 
            '--retries', '2', 
            '--mode', mode 
        ];
        
        if (USE_ENV) args.push('--env');
        if (system) { args.push('--system', system); }
        if (json) { args.push('--json'); }
        
        const child = spawn(CLI, args, { stdio: ['ignore','pipe','pipe'] });
        let out = '', err = '';
        
        child.stdout.on('data', d => out += d.toString());
        child.stderr.on('data', d => err += d.toString());
        
        child.on('close', code => {
            if (code !== 0) {
                reject(new Error(`CLI exit ${code}: ${err}`));
            } else {
                resolve(out.trim());
            }
        });
    });
}

/**
 * Generate real offline review using linters
 */
async function lintOffline(files) {
    // files: array of paths on disk -> send as {path, content}
    const payload = { files: await Promise.all(files.map(async f => ({
        path: path.relative(process.cwd(), f),
        content: await fs.readFile(f, 'utf8')
    }))) };

    const data = await new Promise((resolve, reject) => {
        const req = http.request({ 
            hostname: '127.0.0.1', 
            port: 4040, 
            path: '/lint', 
            method: 'POST',
            headers: { 'content-type': 'application/json' }
        }, res => {
            const bufs = []; 
            res.on('data', d => bufs.push(d));
            res.on('end', () => resolve(Buffer.concat(bufs).toString('utf8')));
        });
        req.on('error', reject);
        req.write(JSON.stringify(payload)); 
        req.end();
    });

    const resp = JSON.parse(data);
    const lint = resp.artifacts && resp.artifacts['lint.json'] ? JSON.parse(resp.artifacts['lint.json']) : {};
    const comments = [];

    const push = (tool, file, line, severity, rule, message) =>
        comments.push({ file, line: Number(line) || 1, severity, title: `[${tool}] ${rule || 'issue'}`, body: message });

    (lint.eslint || []).forEach(x => push('eslint', x.file, x.line, x.severity, x.rule, x.message));
    (lint.flake8 || []).forEach(x => push('flake8', x.file, x.line, x.severity, x.rule, x.message));
    (lint.phpcs || []).forEach(x => push('phpcs', x.file, x.line, x.severity, x.rule, x.message));

    return {
        reviewers: [
            { reviewer: 'static-analysis', title: 'Static Linters', result: { comments, patches: [], tests: [] } }
        ],
        aggregate: { comments, patches: [], tests: [] }
    };
}

/**
 * Generate deterministic offline review structure (fallback)
 */
function generateOfflineReview(files) {
    return {
        reviewers: REVIEWERS.map(reviewer => ({
            reviewer: reviewer.id,
            title: reviewer.title,
            result: {
                comments: [
                    {
                        line: 1,
                        message: `[${reviewer.id}] This is a deterministic review comment for demonstration.`,
                        severity: 'info'
                    }
                ],
                patches: [],
                tests: []
            }
        })),
        aggregate: {
            comments: [
                {
                    line: 1,
                    message: 'This is an offline review - no actual AI analysis was performed.',
                    severity: 'info'
                }
            ],
            patches: [],
            tests: []
        }
    };
}

/**
 * Process files and generate review prompts
 */
function processFiles(files) {
    if (!Array.isArray(files)) {
        return { error: 'Files must be an array' };
    }
    
    // In a real implementation, you would read the actual file contents
    // For this example, we'll create a summary of what would be reviewed
    const fileSummary = files.map(file => `File: ${file}`).join('\n');
    const reviewPrompt = `Please review the following code files and provide detailed feedback on security, performance, readability, and correctness:\n\n${fileSummary}`;
    
    return { prompt: reviewPrompt, fileCount: files.length };
}

/**
 * Handle review requests
 */
async function handleReview(req, res) {
    let body = '';
    
    req.on('data', chunk => {
        body += chunk.toString();
    });
    
    req.on('end', async () => {
        try {
            const data = JSON.parse(body);
            const { files, mode } = data;
            
            if (!files) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Missing files parameter' }));
                return;
            }
            
            const chosenMode = chooseMode(data);
            
            // OFFLINE: use real linters instead of deterministic stub
            if (chosenMode === 'offline') {
                try {
                    const result = await lintOffline(files);
                    res.writeHead(200, {'content-type':'application/json'});
                    return res.end(JSON.stringify(result, null, 2));
                } catch (error) {
                    console.error('Offline linting failed:', error);
                    // Fallback to deterministic stub
                    const stub = generateOfflineReview(files);
                    res.writeHead(200, { 'content-type':'application/json' });
                    return res.end(JSON.stringify(stub, null, 2));
                }
            }
            
            // Process files and generate review
            const fileProcessing = processFiles(files);
            if (fileProcessing.error) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: fileProcessing.error }));
                return;
            }
            
            // Run reviews for each reviewer
            const reviewResults = [];
            
            for (const reviewer of REVIEWERS) {
                try {
                    const prompt = `${fileProcessing.prompt}\n\nPlease focus on ${reviewer.id} aspects in your review.`;
                    const raw = await runCli(prompt, { 
                        system: reviewer.system, 
                        json: true, 
                        mode: chosenMode 
                    });
                    
                    // Parse the AI response and structure it
                    const aiResponse = JSON.parse(raw);
                    const reviewText = aiResponse.candidates?.[0]?.content?.parts?.[0]?.text || 'No review content received';
                    
                    reviewResults.push({
                        reviewer: reviewer.id,
                        title: reviewer.title,
                        result: {
                            comments: [
                                {
                                    line: 1,
                                    message: reviewText,
                                    severity: 'info'
                                }
                            ],
                            patches: [],
                            tests: []
                        }
                    });
                } catch (error) {
                    console.error(`Reviewer ${reviewer.id} failed:`, error.message);
                    reviewResults.push({
                        reviewer: reviewer.id,
                        title: reviewer.title,
                        result: {
                            comments: [
                                {
                                    line: 1,
                                    message: `Review failed: ${error.message}`,
                                    severity: 'error'
                                }
                            ],
                            patches: [],
                            tests: []
                        }
                    });
                }
            }
            
            // Generate aggregate results
            const aggregate = {
                comments: [],
                patches: [],
                tests: []
            };
            
            // Combine all comments from reviewers
            reviewResults.forEach(review => {
                if (review.result.comments) {
                    aggregate.comments.push(...review.result.comments);
                }
            });
            
            const response = {
                reviewers: reviewResults,
                aggregate: aggregate
            };
            
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(response, null, 2));
            
        } catch (error) {
            console.error('Review request error:', error);
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: error.message }));
        }
    });
}

/**
 * Handle health check requests
 */
function handleHealth(req, res) {
    const health = {
        status: 'healthy',
        timestamp: new Date().toISOString(),
        port: PORT,
        reviewers: REVIEWERS.length,
        mode: process.env.REVIEW_DEFAULT_MODE || 'online'
    };
    
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(health, null, 2));
}

/**
 * Create and start the HTTP server
 */
function serve() {
    const server = http.createServer((req, res) => {
        const parsedUrl = url.parse(req.url, true);
        const pathname = parsedUrl.pathname;
        const method = req.method;
        
        // Enable CORS
        res.setHeader('Access-Control-Allow-Origin', '*');
        res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
        res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
        
        if (method === 'OPTIONS') {
            res.writeHead(204);
            res.end();
            return;
        }
        
        if (pathname === '/review' && method === 'POST') {
            handleReview(req, res);
        } else if (pathname === '/health' && method === 'GET') {
            handleHealth(req, res);
        } else if (pathname === '/' && method === 'GET') {
            res.writeHead(200, { 'Content-Type': 'text/plain' });
            res.end(`Review Orchestrator v1.0.0\n\nEndpoints:\n  POST /review - Submit files for review\n  GET /health - Health check\n\nEnvironment:\n  REVIEW_PORT=${PORT}\n  REVIEW_DEFAULT_MODE=${process.env.REVIEW_DEFAULT_MODE || 'online'}\n`);
        } else {
            res.writeHead(404, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Not found' }));
        }
    });
    
    server.listen(PORT, () => {
        console.log(`=== Review Orchestrator ===`);
        console.log(`Listening on: http://localhost:${PORT}`);
        console.log(`Default mode: ${process.env.REVIEW_DEFAULT_MODE || 'online'}`);
        console.log(`CLI: ${CLI} ${CLI_PATH}`);
        console.log(`Model: ${MODEL}`);
        console.log(`Reviewers: ${REVIEWERS.length}`);
        console.log(`========================`);
    });
    
    server.on('error', (err) => {
        if (err.code === 'EADDRINUSE') {
            console.error(`Port ${PORT} is already in use. Try a different port.`);
        } else {
            console.error('Server error:', err);
        }
        process.exit(1);
    });
}

// Start the server
serve();

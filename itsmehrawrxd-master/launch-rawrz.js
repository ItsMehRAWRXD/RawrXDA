#!/usr/bin/env node

/**
 * RawrZ Security Platform - Auto-Launcher
 * 
 * This script automatically starts the RawrZ server when needed.
 * No manual startup required - just run this and the server is ready!
 */

const { spawn } = require('child_process');
const http = require('http');
const path = require('path');

const PORT = process.env.PORT || 8080;
const SERVER_FILE = path.join(__dirname, 'auto-server.js');

console.log(' RawrZ Security Platform - Auto-Launcher');
console.log('='.repeat(50));

// Check if server is already running
function checkServerRunning() {
    return new Promise((resolve) => {
        const req = http.get(`http://localhost:${PORT}/health`, (res) => {
            resolve(true);
        });
        
        req.on('error', () => {
            resolve(false);
        });
        
        req.setTimeout(1000, () => {
            req.destroy();
            resolve(false);
        });
    });
}

// Start the server
async function startServer() {
    console.log(`[AUTO-LAUNCHER] Checking if server is already running on port ${PORT}...`);
    
    const isRunning = await checkServerRunning();
    
    if (isRunning) {
        console.log(`[AUTO-LAUNCHER]  Server is already running on port ${PORT}`);
        console.log(`[AUTO-LAUNCHER]  Access the platform at: http://localhost:${PORT}`);
        console.log(`[AUTO-LAUNCHER]  API endpoint: http://localhost:${PORT}/api/rawrz/execute`);
        return;
    }
    
    console.log(`[AUTO-LAUNCHER]  Starting RawrZ server...`);
    
    const serverProcess = spawn('node', [SERVER_FILE], {
        stdio: 'inherit',
        cwd: __dirname
    });
    
    serverProcess.on('error', (error) => {
        console.error(`[AUTO-LAUNCHER]  Failed to start server: ${error.message}`);
        process.exit(1);
    });
    
    serverProcess.on('exit', (code) => {
        if (code !== 0) {
            console.error(`[AUTO-LAUNCHER]  Server exited with code ${code}`);
            process.exit(code);
        }
    });
    
    // Wait a moment for server to start
    setTimeout(async () => {
        const isNowRunning = await checkServerRunning();
        if (isNowRunning) {
            console.log(`[AUTO-LAUNCHER]  Server started successfully!`);
            console.log(`[AUTO-LAUNCHER]  Access the platform at: http://localhost:${PORT}`);
            console.log(`[AUTO-LAUNCHER]  API endpoint: http://localhost:${PORT}/api/rawrz/execute`);
            console.log(`[AUTO-LAUNCHER]  Health check: http://localhost:${PORT}/health`);
            console.log(`[AUTO-LAUNCHER]  Single airtight endpoint ready for use!`);
        } else {
            console.log(`[AUTO-LAUNCHER] ⏳ Server is starting up...`);
        }
    }, 2000);
    
    // Handle process termination
    process.on('SIGINT', () => {
        console.log('\n[AUTO-LAUNCHER]  Shutting down server...');
        serverProcess.kill('SIGINT');
        process.exit(0);
    });
    
    process.on('SIGTERM', () => {
        console.log('\n[AUTO-LAUNCHER]  Shutting down server...');
        serverProcess.kill('SIGTERM');
        process.exit(0);
    });
}

// Start the server
startServer().catch(console.error);

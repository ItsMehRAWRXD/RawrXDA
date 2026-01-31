#!/usr/bin/env node

/**
 * Test script to verify server startup and engine loading
 */

const { spawn } = require('child_process');
const http = require('http');

console.log('🧪 Testing RawrZ Server Startup...\n');

// Start the server
const serverProcess = spawn('node', ['server.js'], {
    cwd: __dirname,
    stdio: ['pipe', 'pipe', 'pipe']
});

let serverStarted = false;
let enginesLoaded = false;
let warningsFound = [];

serverProcess.stdout.on('data', (data) => {
    const output = data.toString();
    console.log(output);
    
    if (output.includes('Server is running on port')) {
        serverStarted = true;
        console.log('✅ Server started successfully');
    }
    
    if (output.includes('[OK] Module') || output.includes('[INFO]')) {
        enginesLoaded = true;
    }
    
    if (output.includes('[WARN]') && output.includes('not available')) {
        warningsFound.push(output.trim());
    }
});

serverProcess.stderr.on('data', (data) => {
    const error = data.toString();
    console.log('🔴 Error:', error);
});

// Wait a few seconds for server to start
setTimeout(() => {
    if (serverStarted) {
        console.log('\n🎉 Server test completed!');
        console.log(`📊 Status: ${serverStarted ? '✅ Started' : '❌ Failed'}`);
        console.log(`🔧 Engines: ${enginesLoaded ? '✅ Loaded' : '❌ Failed'}`);
        
        if (warningsFound.length === 0) {
            console.log('✅ No warnings found - all engines working!');
        } else {
            console.log(`⚠️  Found ${warningsFound.length} warnings:`);
            warningsFound.forEach(warning => console.log(`   ${warning}`));
        }
        
        // Test health endpoint
        testHealthEndpoint();
    } else {
        console.log('❌ Server failed to start');
        serverProcess.kill();
        process.exit(1);
    }
}, 5000);

function testHealthEndpoint() {
    console.log('\n🏥 Testing health endpoint...');
    
    const req = http.get('http://localhost:8080/health', (res) => {
        let data = '';
        
        res.on('data', (chunk) => {
            data += chunk;
        });
        
        res.on('end', () => {
            try {
                const health = JSON.parse(data);
                if (health.ok && health.status === 'healthy') {
                    console.log('✅ Health endpoint working');
                } else {
                    console.log('❌ Health endpoint failed');
                }
            } catch (e) {
                console.log('❌ Health endpoint returned invalid JSON');
            }
            
            // Clean up
            serverProcess.kill();
            console.log('\n🏁 Test completed - server stopped');
            process.exit(0);
        });
    });
    
    req.on('error', (err) => {
        console.log('❌ Health endpoint test failed:', err.message);
        serverProcess.kill();
        process.exit(1);
    });
    
    req.setTimeout(5000, () => {
        console.log('⏰ Health endpoint test timed out');
        serverProcess.kill();
        process.exit(1);
    });
}

// Handle cleanup
process.on('SIGINT', () => {
    console.log('\n🛑 Test interrupted - stopping server...');
    serverProcess.kill();
    process.exit(0);
});

process.on('exit', () => {
    if (serverProcess && !serverProcess.killed) {
        serverProcess.kill();
    }
});
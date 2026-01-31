#!/bin/bash
set -e

echo " Node.js Application Exit Diagnosis"
echo "====================================="

# Stop the service
systemctl stop rawrz.service || true

echo "1. Testing Node.js application with detailed logging..."
cd /root/RawrZApp

# Create a test script that captures all output and errors
cat <<'EOF' > test-nodejs-detailed.js
console.log('=== Starting RawrZ Application Test ===');
console.log('Node.js version:', process.version);
console.log('Current directory:', process.cwd());
console.log('Environment PORT:', process.env.PORT);
console.log('Environment NODE_ENV:', process.env.NODE_ENV);

// Add error handlers
process.on('uncaughtException', (error) => {
    console.error('UNCAUGHT EXCEPTION:', error);
    process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
    console.error('UNHANDLED REJECTION at:', promise, 'reason:', reason);
    process.exit(1);
});

process.on('exit', (code) => {
    console.log('Process exiting with code:', code);
});

process.on('SIGTERM', () => {
    console.log('Received SIGTERM');
    process.exit(0);
});

process.on('SIGINT', () => {
    console.log('Received SIGINT');
    process.exit(0);
});

try {
    console.log('Loading api-server-no-cli.js...');
    require('./api-server-no-cli.js');
    console.log('Application loaded successfully - should be running now');
} catch (error) {
    console.error('Error loading application:', error.message);
    console.error('Stack trace:', error.stack);
    process.exit(1);
}
EOF

echo "Running detailed test..."
timeout 30s node test-nodejs-detailed.js || echo "Test completed or timed out"

echo ""
echo "2. Checking if there are any port binding issues..."
echo "Testing if port 3000 is already in use:"
lsof -i :3000 || echo "Port 3000 is free"

echo ""
echo "3. Testing with a different port..."
export PORT=3001
echo "Testing with PORT=3001..."
timeout 15s node api-server-no-cli.js || echo "Test with PORT=3001 completed or timed out"

echo ""
echo "4. Checking for any missing dependencies..."
echo "Checking if all required modules are available:"
node -e "
try {
    require('express');
    console.log(' express available');
} catch(e) { console.log(' express missing'); }

try {
    require('cors');
    console.log(' cors available');
} catch(e) { console.log(' cors missing'); }

try {
    require('multer');
    console.log(' multer available');
} catch(e) { console.log(' multer missing'); }

try {
    require('crypto');
    console.log(' crypto available');
} catch(e) { console.log(' crypto missing'); }
"

echo ""
echo "5. Testing with minimal Node.js application..."
cat <<'EOF' > test-minimal.js
const express = require('express');
const app = express();
const PORT = process.env.PORT || 3000;

app.get('/test', (req, res) => {
    res.json({ status: 'ok', message: 'Minimal app working' });
});

console.log('Starting minimal test server...');
const server = app.listen(PORT, () => {
    console.log(`Minimal server running on port ${PORT}`);
});

// Keep the process alive
process.on('SIGTERM', () => {
    console.log('Received SIGTERM, shutting down...');
    server.close(() => {
        process.exit(0);
    });
});

process.on('SIGINT', () => {
    console.log('Received SIGINT, shutting down...');
    server.close(() => {
        process.exit(0);
    });
});
EOF

echo "Testing minimal application..."
timeout 15s node test-minimal.js || echo "Minimal test completed or timed out"

echo ""
echo " Diagnosis completed!"

#!/bin/bash
set -e

echo " Pre-Startup Diagnosis - What's Happening Before Node.js Starts"
echo "================================================================="

# Stop the service
systemctl stop rawrz.service || true

echo "1. Testing Node.js basic functionality..."
cd /root/RawrZApp

# Test 1: Basic Node.js execution
echo "Testing basic Node.js execution:"
node -e "console.log('Node.js is working:', process.version)" || echo " Node.js basic test failed"

echo ""
echo "2. Testing module loading one by one..."
echo "Testing each required module individually:"

# Test each module that the application needs
modules=("express" "cors" "path" "fs" "multer" "crypto")

for module in "${modules[@]}"; do
    echo -n "Testing $module: "
    if node -e "require('$module'); console.log(' OK')" 2>/dev/null; then
        echo " $module loaded successfully"
    else
        echo " $module failed to load"
        echo "Error details:"
        node -e "require('$module')" 2>&1 | head -3
    fi
done

echo ""
echo "3. Testing file system access..."
echo "Testing if we can read the application file:"
if [ -f "api-server-no-cli.js" ]; then
    echo " api-server-no-cli.js exists"
    echo "File size: $(wc -c < api-server-no-cli.js) bytes"
    echo "File permissions: $(ls -la api-server-no-cli.js)"
else
    echo " api-server-no-cli.js not found"
fi

echo ""
echo "4. Testing directory creation (what the app tries to do first)..."
echo "Testing directory creation in /app/uploads and /app/processed:"
mkdir -p /app/uploads /app/processed 2>/dev/null && echo " Directory creation successful" || echo " Directory creation failed"

echo "Testing directory creation in current directory:"
mkdir -p uploads processed 2>/dev/null && echo " Local directory creation successful" || echo " Local directory creation failed"

echo ""
echo "5. Testing port binding..."
echo "Testing if port 3000 is available:"
if command -v lsof >/dev/null 2>&1; then
    lsof -i :3000 || echo " Port 3000 is free"
else
    echo "Port check skipped (lsof not available)"
fi

echo ""
echo "6. Testing environment variables..."
echo "Current environment variables that matter:"
echo "PORT: ${PORT:-'not set'}"
echo "NODE_ENV: ${NODE_ENV:-'not set'}"
echo "PWD: $PWD"

echo ""
echo "7. Testing the exact startup sequence the app uses..."
echo "Creating a step-by-step test script:"

cat <<'EOF' > test-startup-sequence.js
console.log('=== STARTUP SEQUENCE TEST ===');
console.log('Step 1: Basic imports...');

try {
    const express = require('express');
    console.log(' express imported');
} catch (e) {
    console.log(' express import failed:', e.message);
    process.exit(1);
}

try {
    const cors = require('cors');
    console.log(' cors imported');
} catch (e) {
    console.log(' cors import failed:', e.message);
    process.exit(1);
}

try {
    const path = require('path');
    console.log(' path imported');
} catch (e) {
    console.log(' path import failed:', e.message);
    process.exit(1);
}

try {
    const fs = require('fs').promises;
    console.log(' fs.promises imported');
} catch (e) {
    console.log(' fs.promises import failed:', e.message);
    process.exit(1);
}

try {
    const multer = require('multer');
    console.log(' multer imported');
} catch (e) {
    console.log(' multer import failed:', e.message);
    process.exit(1);
}

try {
    const crypto = require('crypto');
    console.log(' crypto imported');
} catch (e) {
    console.log(' crypto import failed:', e.message);
    process.exit(1);
}

console.log('Step 2: Creating express app...');
try {
    const app = express();
    console.log(' express app created');
} catch (e) {
    console.log(' express app creation failed:', e.message);
    process.exit(1);
}

console.log('Step 3: Setting up middleware...');
try {
    app.use(cors());
    app.use(express.json({ limit: '100mb' }));
    app.use(express.urlencoded({ extended: true, limit: '100mb' }));
    console.log(' middleware setup complete');
} catch (e) {
    console.log(' middleware setup failed:', e.message);
    process.exit(1);
}

console.log('Step 4: Testing directory creation...');
try {
    const fs = require('fs').promises;
    await fs.mkdir('/app/uploads', { recursive: true });
    await fs.mkdir('/app/processed', { recursive: true });
    console.log(' directory creation successful');
} catch (e) {
    console.log(' directory creation failed:', e.message);
    console.log('Trying local directories...');
    try {
        await fs.mkdir('uploads', { recursive: true });
        await fs.mkdir('processed', { recursive: true });
        console.log(' local directory creation successful');
    } catch (e2) {
        console.log(' local directory creation also failed:', e2.message);
    }
}

console.log('Step 5: Testing port binding...');
try {
    const PORT = process.env.PORT || 3000;
    console.log('Attempting to bind to port:', PORT);
    
    const server = app.listen(PORT, () => {
        console.log(' Server bound to port', PORT);
        console.log(' STARTUP SEQUENCE COMPLETED SUCCESSFULLY');
        server.close(() => {
            console.log('Test server closed');
        });
    });
    
    // Close after 2 seconds
    setTimeout(() => {
        server.close();
    }, 2000);
    
} catch (e) {
    console.log(' port binding failed:', e.message);
    process.exit(1);
}
EOF

echo "Running startup sequence test..."
timeout 10s node test-startup-sequence.js || echo "Startup sequence test completed or timed out"

echo ""
echo "8. Testing the actual application file with detailed error capture..."
echo "Creating a wrapper that captures ALL output and errors:"

cat <<'EOF' > test-actual-app.js
console.log('=== TESTING ACTUAL APPLICATION ===');

// Capture all possible errors
process.on('uncaughtException', (error) => {
    console.log('UNCAUGHT EXCEPTION:', error.message);
    console.log('Stack:', error.stack);
    process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
    console.log('UNHANDLED REJECTION:', reason);
    process.exit(1);
});

process.on('exit', (code) => {
    console.log('PROCESS EXITING WITH CODE:', code);
});

process.on('SIGTERM', () => {
    console.log('RECEIVED SIGTERM');
    process.exit(0);
});

process.on('SIGINT', () => {
    console.log('RECEIVED SIGINT');
    process.exit(0);
});

console.log('Loading api-server-no-cli.js...');
try {
    require('./api-server-no-cli.js');
    console.log(' Application loaded without immediate exit');
} catch (error) {
    console.log(' Application failed to load:', error.message);
    console.log('Stack trace:', error.stack);
    process.exit(1);
}
EOF

echo "Testing actual application with full error capture..."
timeout 15s node test-actual-app.js || echo "Application test completed or timed out"

echo ""
echo "9. Checking system resources and limits..."
echo "Memory usage:"
free -h
echo "Disk space:"
df -h /
echo "Process limits:"
ulimit -a

echo ""
echo "10. Checking for any system-level issues..."
echo "Checking if there are any systemd-related environment issues:"
env | grep -E "(SYSTEMD|SERVICE)" || echo "No systemd environment variables found"

echo ""
echo " Pre-startup diagnosis completed!"
echo "Review the output above to identify what's causing the immediate exit."

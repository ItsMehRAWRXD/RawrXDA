// Test script to verify elevated privileges functionality (Linux/Unix)
const fs = require('fs');
const path = require('path');
const os = require('os');
const { execSync } = require('child_process');

console.log('========================================');
console.log('RawrZ Security Platform - Linux Privilege Test');
console.log('========================================');
console.log('');

// Check if running as root
function isRunningAsRoot() {
    try {
        return process.getuid && process.getuid() === 0;
    } catch (error) {
        return false;
    }
}

// Test system file access
function testSystemFileAccess() {
    try {
        // Try to access a system file that requires root privileges
        fs.accessSync('/etc/shadow', fs.constants.R_OK);
        return true;
    } catch (error) {
        return false;
    }
}

// Test process management
function testProcessManagement() {
    try {
        // Try to list all processes (requires root for some systems)
        execSync('ps aux', { stdio: 'pipe' });
        return true;
    } catch (error) {
        return false;
    }
}

// Test network configuration
function testNetworkConfiguration() {
    try {
        // Try to access network configuration
        execSync('ip addr show', { stdio: 'pipe' });
        return true;
    } catch (error) {
        return false;
    }
}

// Test file system operations
function testFileSystemAccess() {
    try {
        const testFile = '/tmp/rawrz-test-' + Date.now() + '.txt';
        fs.writeFileSync(testFile, 'test');
        fs.unlinkSync(testFile);
        return true;
    } catch (error) {
        return false;
    }
}

// Test system information access
function testSystemInfoAccess() {
    try {
        // Try to access system information
        execSync('uname -a', { stdio: 'pipe' });
        execSync('cat /proc/version', { stdio: 'pipe' });
        return true;
    } catch (error) {
        return false;
    }
}

// Run tests
console.log('Testing elevated privileges on Linux...');
console.log('');

const tests = [
    { name: 'Root User Check', test: isRunningAsRoot },
    { name: 'System File Access', test: testSystemFileAccess },
    { name: 'Process Management', test: testProcessManagement },
    { name: 'Network Configuration', test: testNetworkConfiguration },
    { name: 'File System Access', test: testFileSystemAccess },
    { name: 'System Info Access', test: testSystemInfoAccess }
];

let allPassed = true;

tests.forEach(test => {
    try {
        const result = test.test();
        const status = result ? ' PASS' : ' FAIL';
        console.log(`${test.name}: ${status}`);
        if (!result) allPassed = false;
    } catch (error) {
        console.log(`${test.name}:  ERROR - ${error.message}`);
        allPassed = false;
    }
});

console.log('');
console.log('========================================');
if (allPassed) {
    console.log(' ALL TESTS PASSED - Full privileges available!');
    console.log('');
    console.log('All engines will have maximum functionality:');
    console.log('  - Red Killer: Full system access and control');
    console.log('  - Private Virus Scanner: Complete system scanning');
    console.log('  - AI Threat Detector: Full model training');
    console.log('  - All other engines: Maximum capabilities');
} else {
    console.log('  SOME TESTS FAILED - Limited privileges detected');
    console.log('');
    console.log('Some engines may have reduced functionality:');
    console.log('  - Red Killer: Limited system access');
    console.log('  - Private Virus Scanner: Basic scanning only');
    console.log('  - AI Threat Detector: Limited model operations');
    console.log('');
    console.log('To enable full functionality:');
    console.log('  - Run container with --privileged flag');
    console.log('  - Or use privileged Docker deployment script');
}
console.log('========================================');

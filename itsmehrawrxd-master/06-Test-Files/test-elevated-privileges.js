// Test script to verify elevated privileges functionality
const fs = require('fs');
const path = require('path');
const os = require('os');

console.log('========================================');
console.log('RawrZ Security Platform - Privilege Test');
console.log('========================================');
console.log('');

// Check if running as administrator
function isRunningAsAdmin() {
    try {
        // Try to access a system directory that requires admin privileges
        fs.accessSync('C:\\Windows\\System32\\config\\SAM', fs.constants.R_OK);
        return true;
    } catch (error) {
        return false;
    }
}

// Test registry access (Windows specific)
function testRegistryAccess() {
    try {
        const { execSync } = require('child_process');
        execSync('reg query "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion" /v ProgramFilesDir', { stdio: 'pipe' });
        return true;
    } catch (error) {
        return false;
    }
}

// Test service control access
function testServiceControl() {
    try {
        const { execSync } = require('child_process');
        execSync('sc query "Spooler"', { stdio: 'pipe' });
        return true;
    } catch (error) {
        return false;
    }
}

// Test file system access
function testFileSystemAccess() {
    try {
        const testFile = 'C:\\Windows\\Temp\\rawrz-test-' + Date.now() + '.txt';
        fs.writeFileSync(testFile, 'test');
        fs.unlinkSync(testFile);
        return true;
    } catch (error) {
        return false;
    }
}

// Run tests
console.log('Testing elevated privileges...');
console.log('');

const tests = [
    { name: 'Administrator Check', test: isRunningAsAdmin },
    { name: 'Registry Access', test: testRegistryAccess },
    { name: 'Service Control', test: testServiceControl },
    { name: 'File System Access', test: testFileSystemAccess }
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
    console.log('  - Red Killer: Full registry and service control');
    console.log('  - Private Virus Scanner: Complete system scanning');
    console.log('  - AI Threat Detector: Full model training');
    console.log('  - All other engines: Maximum capabilities');
} else {
    console.log('  SOME TESTS FAILED - Limited privileges detected');
    console.log('');
    console.log('Some engines may have reduced functionality:');
    console.log('  - Red Killer: Limited registry and service access');
    console.log('  - Private Virus Scanner: Basic scanning only');
    console.log('  - AI Threat Detector: Limited model operations');
    console.log('');
    console.log('To enable full functionality:');
    console.log('  - Run as Administrator: start-elevated.bat');
    console.log('  - Or use privileged Docker: deploy-privileged.ps1');
}
console.log('========================================');

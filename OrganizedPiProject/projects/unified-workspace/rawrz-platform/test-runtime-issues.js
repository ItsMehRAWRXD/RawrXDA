#!/usr/bin/env node

/**
 * Runtime Issues Test Script
 * Tests the previously identified runtime configuration issues
 */

const fs = require('fs');
const path = require('path');

console.log('🔍 Testing Runtime Configuration Issues');
console.log('=====================================\n');

// Test 1: Check environment variables
console.log('1. Environment Variables:');
console.log(`   NODE_ENV: ${process.env.NODE_ENV || 'undefined'}`);
console.log(`   NODE_OPTIONS: ${process.env.NODE_OPTIONS || 'undefined'}`);
console.log(`   DEBUG_CRYPTO: ${process.env.DEBUG_CRYPTO || 'undefined'}`);
console.log(`   ENABLE_ANTI_DEBUG: ${process.env.ENABLE_ANTI_DEBUG || 'undefined'}`);
console.log(`   DISABLE_ANTI_DEBUG: ${process.env.DISABLE_ANTI_DEBUG || 'undefined'}\n`);

// Test 2: Check if problematic conditions would trigger
console.log('2. Potential Issue Conditions:');
const isDevelopment = process.env.NODE_ENV === 'development';
const hasDebugCrypto = process.env.DEBUG_CRYPTO === 'true';
const hasAntiDebug = process.env.ENABLE_ANTI_DEBUG === 'true';

console.log(`   Development Mode: ${isDevelopment ? '✅ SAFE' : '⚠️  Could trigger anti-debug'}`);
console.log(`   Debug Crypto: ${hasDebugCrypto ? '⚠️  Could expose sensitive data' : '✅ SAFE'}`);
console.log(`   Anti-Debug Enabled: ${hasAntiDebug ? '⚠️  Could cause crashes' : '✅ SAFE'}\n`);

// Test 3: Check if fixes are in place
console.log('3. Code Fixes Verification:');

// Check beaconism-dll-sideloading.js
const beaconismPath = path.join(__dirname, 'src', 'engines', 'beaconism-dll-sideloading.js');
if (fs.existsSync(beaconismPath)) {
    const content = fs.readFileSync(beaconismPath, 'utf8');
    const hasExitFix = content.includes('// process.exit(0); // COMMENTED OUT');
    const hasVMFix = content.includes('console.warn(\'[BEACONISM] VM detected');
    console.log(`   Beaconism process.exit(0) fix: ${hasExitFix ? '✅ APPLIED' : '❌ MISSING'}`);
    console.log(`   Beaconism VM detection fix: ${hasVMFix ? '✅ APPLIED' : '❌ MISSING'}`);
} else {
    console.log('   ❌ Beaconism file not found');
}

// Check advanced-crypto.js
const cryptoPath = path.join(__dirname, 'src', 'engines', 'advanced-crypto.js');
if (fs.existsSync(cryptoPath)) {
    const content = fs.readFileSync(cryptoPath, 'utf8');
    const hasSecurityFix = content.includes('SECURITY FIX: Don\'t log actual');
    console.log(`   Advanced Crypto security fix: ${hasSecurityFix ? '✅ APPLIED' : '❌ MISSING'}`);
} else {
    console.log('   ❌ Advanced Crypto file not found');
}

console.log('\n4. Recommendation:');
console.log('   Run the server and access http://localhost:8080/panel to verify');
console.log('   all engines are working without crashes or data exposure.\n');

// Test 5: Simulate problematic conditions (safely)
console.log('5. Safe Test Mode:');
console.log('   Setting DEBUG_CRYPTO=true for testing...');
process.env.DEBUG_CRYPTO = 'true';

// Test if the crypto engine loads without exposing sensitive data
try {
    const cryptoEngine = require('./src/engines/advanced-crypto');
    console.log('   ✅ Advanced Crypto engine loads without issues');
} catch (error) {
    console.log(`   ❌ Advanced Crypto engine failed: ${error.message}`);
}

console.log('\n🎯 Test completed. Check the output above for any issues.');

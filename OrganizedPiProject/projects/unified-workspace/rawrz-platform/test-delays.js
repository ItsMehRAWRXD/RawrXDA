#!/usr/bin/env node

/**
 * Test script for RawrZ Delay Configuration System
 */

const delayManager = require('./src/config/delays');

async function testDelays() {
    console.log('🧪 Testing RawrZ Delay Configuration System\n');
    
    // Test 1: Default delays
    console.log('1️⃣ Testing default delays...');
    const defaultDelay = delayManager.getDelay('crypto', 'encryption');
    console.log(`   Default crypto encryption delay: ${defaultDelay}ms\n`);
    
    // Test 2: Preset application
    console.log('2️⃣ Testing preset application...');
    console.log('   Applying "fast" preset...');
    delayManager.applyPreset('fast');
    const fastDelay = delayManager.getDelay('crypto', 'encryption');
    console.log(`   Fast preset crypto encryption delay: ${fastDelay}ms`);
    
    console.log('   Applying "realistic" preset...');
    delayManager.applyPreset('realistic');
    const realisticDelay = delayManager.getDelay('crypto', 'encryption');
    console.log(`   Realistic preset crypto encryption delay: ${realisticDelay}ms\n`);
    
    // Test 3: Custom delay setting
    console.log('3️⃣ Testing custom delay setting...');
    delayManager.setDelay('assembly', 'encryption', 150);
    const customDelay = delayManager.getDelay('assembly', 'encryption');
    console.log(`   Custom assembly encryption delay: ${customDelay}ms\n`);
    
    // Test 4: Random variation
    console.log('4️⃣ Testing random variation...');
    delayManager.setRandomVariation(0.3);
    console.log('   Random variation set to 30%');
    for (let i = 0; i < 5; i++) {
        const randomDelay = delayManager.getDelay('crypto', 'encryption');
        console.log(`   Random delay ${i + 1}: ${randomDelay}ms`);
    }
    console.log();
    
    // Test 5: Delay execution
    console.log('5️⃣ Testing delay execution...');
    const startTime = Date.now();
    await delayManager.wait(100);
    const endTime = Date.now();
    const actualDelay = endTime - startTime;
    console.log(`   Requested delay: 100ms, Actual delay: ${actualDelay}ms`);
    console.log(`   Accuracy: ${Math.abs(actualDelay - 100) <= 10 ? '✅ Good' : '❌ Poor'}\n`);
    
    // Test 6: Disable/enable
    console.log('6️⃣ Testing enable/disable...');
    delayManager.setEnabled(false);
    const disabledDelay = delayManager.getDelay('crypto', 'encryption');
    console.log(`   Delays disabled - returned delay: ${disabledDelay}ms`);
    
    delayManager.setEnabled(true);
    const enabledDelay = delayManager.getDelay('crypto', 'encryption');
    console.log(`   Delays enabled - returned delay: ${enabledDelay}ms\n`);
    
    // Test 7: All categories
    console.log('7️⃣ Testing all delay categories...');
    const allDelays = delayManager.getAllDelays();
    Object.entries(allDelays).forEach(([category, operations]) => {
        console.log(`   ${category}: ${Object.keys(operations).length} operations`);
        Object.entries(operations).forEach(([operation, delay]) => {
            console.log(`     - ${operation}: ${delay}ms`);
        });
    });
    console.log();
    
    // Test 8: Reset to defaults
    console.log('8️⃣ Testing reset to defaults...');
    delayManager.resetDelays();
    const resetDelay = delayManager.getDelay('crypto', 'encryption');
    console.log(`   Reset crypto encryption delay: ${resetDelay}ms`);
    console.log(`   Matches original: ${resetDelay === defaultDelay ? '✅ Yes' : '❌ No'}\n`);
    
    console.log('✅ All delay tests completed successfully!');
    console.log('\n🎯 Delay Configuration Features:');
    console.log('   • Configurable delays for all operation types');
    console.log('   • 5 built-in presets (fast, normal, slow, realistic, stealth)');
    console.log('   • Random variation for realistic timing');
    console.log('   • Enable/disable functionality');
    console.log('   • Category-based organization');
    console.log('   • API integration ready');
    console.log('   • Web UI panel available at /delay-panel');
}

// Run tests
testDelays().catch(console.error);

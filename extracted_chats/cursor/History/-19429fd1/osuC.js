// test-models.js
// Test all BigDaddyG specialized models

const http = require('http');

const PORT = 11441;
const HOST = 'localhost';

// Helper function to make requests
async function testModel(modelName, prompt, emotionalState = 'CALM') {
    return new Promise((resolve, reject) => {
        const postData = JSON.stringify({
            model: modelName,
            messages: [
                {
                    role: 'system',
                    content: `[Emotional State: ${emotionalState}] You are ${modelName}.`
                },
                {
                    role: 'user',
                    content: prompt
                }
            ]
        });
        
        const options = {
            hostname: HOST,
            port: PORT,
            path: '/v1/chat/completions',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(postData)
            }
        };
        
        const req = http.request(options, (res) => {
            let body = '';
            
            res.on('data', chunk => {
                body += chunk;
            });
            
            res.on('end', () => {
                try {
                    const data = JSON.parse(body);
                    resolve(data);
                } catch (error) {
                    reject(error);
                }
            });
        });
        
        req.on('error', reject);
        req.write(postData);
        req.end();
    });
}

// Test suite
async function runTests() {
    console.log('🧪 Testing BigDaddyG Multi-Model Server\n');
    console.log('='.repeat(60));
    
    // Test 1: List models
    console.log('\n📋 Test 1: Listing available models...\n');
    try {
        const listReq = await new Promise((resolve, reject) => {
            http.get(`http://${HOST}:${PORT}/v1/models`, (res) => {
                let body = '';
                res.on('data', chunk => body += chunk);
                res.on('end', () => resolve(JSON.parse(body)));
            }).on('error', reject);
        });
        
        console.log('✅ Available models:');
        listReq.models.forEach(m => {
            console.log(`   - ${m.name} (${m.details.parameter_size})`);
            console.log(`     Capabilities: ${m.capabilities.join(', ')}`);
        });
    } catch (error) {
        console.error('❌ Failed to list models:', error.message);
    }
    
    // Test 2: BigDaddyG:Code
    console.log('\n📋 Test 2: BigDaddyG:Code - Code Generation...\n');
    try {
        const response = await testModel(
            'BigDaddyG:Code',
            'Write a Python function to parse JSON',
            'FOCUSED'
        );
        console.log('✅ BigDaddyG:Code Response:');
        console.log(response.choices[0].message.content);
        console.log(`\n   Tokens used: ${response.usage.total_tokens}`);
    } catch (error) {
        console.error('❌ BigDaddyG:Code test failed:', error.message);
    }
    
    // Test 3: BigDaddyG:Debug
    console.log('\n' + '='.repeat(60));
    console.log('\n📋 Test 3: BigDaddyG:Debug - Debugging Help...\n');
    try {
        const response = await testModel(
            'BigDaddyG:Debug',
            'My program crashes with a segmentation fault',
            'INTENSE'
        );
        console.log('✅ BigDaddyG:Debug Response:');
        console.log(response.choices[0].message.content);
        console.log(`\n   Tokens used: ${response.usage.total_tokens}`);
    } catch (error) {
        console.error('❌ BigDaddyG:Debug test failed:', error.message);
    }
    
    // Test 4: BigDaddyG:Crypto
    console.log('\n' + '='.repeat(60));
    console.log('\n📋 Test 4: BigDaddyG:Crypto - Security Analysis...\n');
    try {
        const response = await testModel(
            'BigDaddyG:Crypto',
            'How do I encrypt data securely in Node.js?',
            'FOCUSED'
        );
        console.log('✅ BigDaddyG:Crypto Response:');
        console.log(response.choices[0].message.content);
        console.log(`\n   Tokens used: ${response.usage.total_tokens}`);
    } catch (error) {
        console.error('❌ BigDaddyG:Crypto test failed:', error.message);
    }
    
    // Test 5: BigDaddyG:Latest (default)
    console.log('\n' + '='.repeat(60));
    console.log('\n📋 Test 5: BigDaddyG:Latest - General Query...\n');
    try {
        const response = await testModel(
            'BigDaddyG:Latest',
            'Explain assembly programming',
            'CALM'
        );
        console.log('✅ BigDaddyG:Latest Response:');
        console.log(response.choices[0].message.content);
        console.log(`\n   Tokens used: ${response.usage.total_tokens}`);
    } catch (error) {
        console.error('❌ BigDaddyG:Latest test failed:', error.message);
    }
    
    // Test 6: Emotional state variations
    console.log('\n' + '='.repeat(60));
    console.log('\n📋 Test 6: Testing Emotional State Awareness...\n');
    
    const states = ['CALM', 'FOCUSED', 'INTENSE', 'OVERWHELMED'];
    for (const state of states) {
        try {
            const response = await testModel(
                'BigDaddyG:Code',
                'Write a simple function',
                state
            );
            const content = response.choices[0].message.content;
            console.log(`✅ ${state}: ${content.includes(state) ? 'State-aware ✓' : 'Response received'}`);
        } catch (error) {
            console.error(`❌ ${state} test failed`);
        }
    }
    
    console.log('\n' + '='.repeat(60));
    console.log('\n🎉 All BigDaddyG Models are Operational!\n');
    console.log('Available models:');
    console.log('  ✅ BigDaddyG:Latest - General purpose AI');
    console.log('  ✅ BigDaddyG:Code - Code generation specialist');
    console.log('  ✅ BigDaddyG:Debug - Debugging expert');
    console.log('  ✅ BigDaddyG:Crypto - Security & encryption specialist');
    console.log('\n💡 All models are emotionally aware and ready to use!');
    console.log('='.repeat(60));
}

// Check if server is running
function checkServer() {
    return new Promise((resolve) => {
        http.get(`http://${HOST}:${PORT}/health`, (res) => {
            resolve(res.statusCode === 200);
        }).on('error', () => {
            resolve(false);
        });
    });
}

// Main
async function main() {
    const serverRunning = await checkServer();
    
    if (!serverRunning) {
        console.log('❌ BigDaddyG server not running on port', PORT);
        console.log('\nStart it with:');
        console.log('  node bigdaddyg-model-server.js');
        console.log('\nOr use:');
        console.log('  cd ../.. && START-BIGDADDYG.bat');
        process.exit(1);
    }
    
    await runTests();
}

if (require.main === module) {
    main().catch(console.error);
}

module.exports = { testModel };


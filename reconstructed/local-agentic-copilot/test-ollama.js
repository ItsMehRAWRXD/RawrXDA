const Ollama = require('./modules/ollama-client.js');

async function testOllamaClient() {
    console.log('🧪 Testing Local Agentic Copilot Ollama Client\n');
    
    // Initialize client with faster model
    const client = new Ollama({ 
        agenticModel: 'qwen2.5:7b',  // Using faster model for testing
        standardModel: 'bigdaddyg-fast:latest'
    });

    // Listen to events
    client.on('info', msg => console.log('ℹ️ ', msg));
    client.on('error', msg => console.error('❌', msg));

    try {
        // Step 1: Check if Ollama is running
        console.log('1️⃣  Checking Ollama health...');
        const isHealthy = await client.checkHealth();
        if (!isHealthy) {
            console.error('\n⚠️  Ollama is not running!');
            console.log('📝 Start Ollama with: ollama serve');
            process.exit(1);
        }
        console.log('');

        // Step 2: List available models
        console.log('2️⃣  Listing available models...');
        const models = await client.listModels();
        console.log(`Found ${models.length} models:`);
        models.forEach(model => {
            console.log(`   - ${model.name}`);
        });
        console.log('');

        // Step 3: Check current status
        console.log('3️⃣  Current client status:');
        const status = client.getStatus();
        console.log(`   Mode: ${status.mode}`);
        console.log(`   Model: ${status.model}`);
        console.log(`   Temperature: ${status.temperature}`);
        console.log(`   Endpoint: ${status.endpoint}`);
        console.log('');

        // Step 4: Test code generation (with shorter timeout)
        console.log('4️⃣  Testing code generation...');
        console.log('   Prompt: Create a TypeScript calculator function\n');
        
        const code = await client.generateCode(
            'Create a simple TypeScript calculator function with add, subtract, multiply, divide methods',
            'typescript'
        );
        
        console.log('\n✅ Generated Code:');
        console.log('─'.repeat(60));
        console.log(code);
        console.log('─'.repeat(60));

        // Step 5: Test mode toggle
        console.log('\n5️⃣  Testing mode toggle...');
        client.toggleAgenticMode();
        const newStatus = client.getStatus();
        console.log(`   New mode: ${newStatus.mode}`);

        console.log('\n🎉 All tests passed!');

    } catch (error) {
        console.error('\n❌ Test failed:', error.message);
        
        if (error.message.includes('timeout')) {
            console.log('\n💡 Tips:');
            console.log('   - The model might be loading for the first time (can take 1-2 minutes)');
            console.log('   - Try a smaller/faster model like: qwen2.5:7b');
            console.log('   - Check Ollama logs: ollama logs');
        } else if (error.message.includes('Cannot connect')) {
            console.log('\n💡 Make sure Ollama is running:');
            console.log('   ollama serve');
        }
        
        process.exit(1);
    }
}

// Run the test
testOllamaClient();

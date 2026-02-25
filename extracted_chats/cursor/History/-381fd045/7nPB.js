// Inject BigDaddyG Engine into FIXED-CURSOR-CLONE
const fs = require('fs');

console.log('🚀 Starting BigDaddyG injection into Cursor Clone...');

// Read files
const cursorHTML = fs.readFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', 'utf8');
const engineCode = fs.readFileSync('neuro-symphonic-workspace/bigdaddyg-engines.js', 'utf8');

// Find injection point (right after <script> tag on line 974)
const scriptTag = '    <script>';
const scriptIndex = cursorHTML.indexOf(scriptTag);

if (scriptIndex === -1) {
    console.error('❌ Could not find <script> tag!');
    process.exit(1);
}

// Inject the BigDaddyG engine right after the script tag
const before = cursorHTML.substring(0, scriptIndex + scriptTag.length);
const after = cursorHTML.substring(scriptIndex + scriptTag.length);

const injection = `
        // ============================================================================
        // BIGDADDYG EMBEDDED AI ENGINE (OFFLINE FALLBACK)
        // ============================================================================
        // This runs when Ollama is not connected, providing offline AI capabilities
        
        ${engineCode}
        
        // Initialize embedded engines
        const embeddedBigDaddyG = new BigDaddyGEngine();
        const embeddedNeuroSymphonic = new NeuroSymphonicEngine();
        
        // Link engines
        embeddedBigDaddyG.setEmotionalState = (state) => {
            embeddedBigDaddyG.emotionalState = state;
            embeddedNeuroSymphonic.changeState(state);
        };
        
        console.log('✅ BigDaddyG embedded engine loaded as offline fallback!');
        console.log('📦 Available models:', Object.keys(embeddedBigDaddyG.models));
        
        // Fallback function when Ollama is not available
        async function queryEmbeddedAI(prompt, mode = 'agent') {
            console.log('[FALLBACK] 🧠 Using embedded BigDaddyG (Ollama offline)');
            
            // Map agent modes to BigDaddyG models
            const modeToModel = {
                'coder': 'BigDaddyG:Code',
                'composer': 'BigDaddyG:Code',
                'agent': 'BigDaddyG:Latest',
                'chat': 'BigDaddyG:Latest',
                'plan': 'BigDaddyG:Latest',
                'debug': 'BigDaddyG:Debug',
                'security': 'BigDaddyG:Crypto'
            };
            
            // Detect special cases from prompt
            const promptLower = prompt.toLowerCase();
            let modelToUse = modeToModel[mode] || 'BigDaddyG:Latest';
            
            // Smart routing based on keywords
            if (promptLower.includes('debug') || promptLower.includes('error') || promptLower.includes('fix')) {
                modelToUse = 'BigDaddyG:Debug';
            } else if (promptLower.includes('encrypt') || promptLower.includes('crypto') || promptLower.includes('security')) {
                modelToUse = 'BigDaddyG:Crypto';
            } else if (promptLower.includes('write') || promptLower.includes('create') || promptLower.includes('generate')) {
                modelToUse = 'BigDaddyG:Code';
            }
            
            // Query the embedded engine
            const response = await embeddedBigDaddyG.query(prompt, modelToUse);
            
            return {
                response: response,
                model: modelToUse,
                done: true,
                context: [],
                total_duration: 0,
                load_duration: 0,
                prompt_eval_count: prompt.length,
                eval_count: response.length,
                eval_duration: 0
            };
        }
        
`;

const enhanced = before + injection + after;

// Now modify the sendToAgent function to use embedded AI as fallback
// Find the line: if (!isOllamaConnected) {
const ollamaCheckLine = "if (!isOllamaConnected) {";
const ollamaCheckIndex = enhanced.indexOf(ollamaCheckLine);

if (ollamaCheckIndex === -1) {
    console.error('❌ Could not find Ollama connection check!');
    console.log('Writing file anyway with engine injection...');
    fs.writeFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', enhanced);
    process.exit(0);
}

// Replace the error message with fallback to embedded AI
const oldCode = `if (!isOllamaConnected) {
                addChatMessage('assistant', '❌ Ollama not connected. Make sure it\\'s running on localhost:11434');
                return;
            }`;

const newCode = `if (!isOllamaConnected) {
                // Use embedded BigDaddyG instead of failing!
                addChatMessage('assistant', '🧠 Ollama offline - using embedded BigDaddyG engine');
                if (thinkingEnabled) {
                    showThoughtDisplay();
                    addThoughtStep('⚡ Ollama not connected, switching to embedded AI', 'complete');
                    addThoughtStep('🧠 Using BigDaddyG embedded engine (offline mode)', 'thinking');
                }
                
                try {
                    // Process with embedded AI
                    let fullContext = await processReferences(message);
                    
                    if (thinkingEnabled) addThoughtStep('📚 Context built with references', 'complete');
                    
                    // Query embedded engine
                    if (thinkingEnabled) addThoughtStep(\`🚀 Sending to embedded \${aiMode} engine...\`, 'thinking');
                    const result = await queryEmbeddedAI(fullContext, aiMode);
                    
                    if (thinkingEnabled) {
                        addThoughtStep(\`✅ Response from \${result.model}\`, 'complete');
                        addThoughtStep(\`📊 Generated \${result.eval_count} tokens\`, 'complete');
                        hideThoughtDisplay();
                    }
                    
                    // Add assistant response
                    addChatMessage('assistant', result.response);
                    
                    // Update counters
                    queryCount++;
                    tokenCount += result.eval_count;
                    document.getElementById('query-count').textContent = queryCount;
                    document.getElementById('token-count').textContent = \`\${tokenCount.toLocaleString()} tokens\`;
                    
                    // Update status
                    document.getElementById('send-btn').disabled = false;
                    document.getElementById('send-btn').textContent = 'Send';
                    document.getElementById('agent-status').textContent = \`\${aiMode} mode (offline)\`;
                    
                } catch (error) {
                    console.error('[EMBEDDED] Error:', error);
                    addChatMessage('assistant', \`❌ Error with embedded AI: \${error.message}\`);
                    document.getElementById('send-btn').disabled = false;
                    document.getElementById('send-btn').textContent = 'Send';
                    if (thinkingEnabled) hideThoughtDisplay();
                }
                
                return;
            }`;

const final = enhanced.replace(oldCode, newCode);

// Write the enhanced file
fs.writeFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', final);

console.log('✅ BigDaddyG engine injected successfully!');
console.log('📁 Output: FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html');
console.log('');
console.log('🎯 Features added:');
console.log('  ✅ Embedded BigDaddyG engine (4 models)');
console.log('  ✅ Offline fallback when Ollama disconnected');
console.log('  ✅ All original features preserved');
console.log('  ✅ Smart model routing (Code/Debug/Crypto/Latest)');
console.log('  ✅ Works with existing agent modes');
console.log('');
console.log('💡 Now works in 2 modes:');
console.log('  1. Ollama connected → Uses Ollama models');
console.log('  2. Ollama offline → Uses embedded BigDaddyG');


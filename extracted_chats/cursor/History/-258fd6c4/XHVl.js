// Remove ALL Ollama dependencies - Orchestra is the ONLY backend
const fs = require('fs');

console.log('🔧 Removing ALL Ollama dependencies...');
console.log('✅ Making Orchestra (port 11441) the ONLY backend\n');

let html = fs.readFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', 'utf8');

// 1. Change isOllamaConnected to isBigDaddyGServerConnected everywhere
html = html.replace(/isOllamaConnected/g, 'isOrchestraConnected');
html = html.replace(/let isOrchestraConnected = false/g, 'let isOrchestraConnected = false');

// 2. Remove Ollama status display, replace with Orchestra
html = html.replace(
    '<span id="ollama-status">🧠 Embedded AI</span>',
    '<span id="orchestra-status">🎼 Orchestra</span>'
);

// 3. Change all references from ollama-status to orchestra-status
html = html.replace(/getElementById\('ollama-status'\)/g, "getElementById('orchestra-status')");
html = html.replace(/ollama-status/g, 'orchestra-status');

// 4. Replace checkOllama function with checkOrchestra
html = html.replace(
    /async function checkOllama\(\) \{[\s\S]*?^\s*\}/m,
    `async function checkOrchestra() {
            try {
                const response = await fetch('http://localhost:11441/health', {
                    method: 'GET',
                    signal: AbortSignal.timeout(2000)
                });
                
                if (response.ok) {
                    const data = await response.json();
                    isOrchestraConnected = true;
                    document.getElementById('orchestra-status').textContent = '🟢 Orchestra';
                    document.getElementById('orchestra-status').style.color = 'var(--accent-green)';
                    showToast('Connected to BigDaddyG Orchestra! REAL AI ready 🚀', 'success', 6000);
                    console.log('✅ Orchestra connected:', data.models);
                } else {
                    throw new Error('Not OK');
                }
            } catch (error) {
                isOrchestraConnected = false;
                document.getElementById('orchestra-status').textContent = '🔴 Orchestra Offline';
                document.getElementById('orchestra-status').style.color = 'var(--red)';
                showToast('⚠️ BigDaddyG Orchestra is offline. Start: node BigDaddyG-Orchestra-Server.js', 'error', 10000);
                console.error('❌ Orchestra not connected. Start the server!');
            }
        }`
);

// 5. Replace checkOllama calls with checkOrchestra
html = html.replace(/checkOllama\(\)/g, 'checkOrchestra()');

// 6. Replace the fetch to Ollama (localhost:11434) with Orchestra (localhost:11441)
html = html.replace(
    /fetch\('http:\/\/localhost:11434\/api\/generate'/g,
    "fetch('http://localhost:11441/v1/chat/completions'"
);

// 7. Update the request body to OpenAI format instead of Ollama format
html = html.replace(
    /body: JSON\.stringify\(\{[\s\S]*?model: modelToUse,[\s\S]*?prompt: finalPrompt,[\s\S]*?stream: false,[\s\S]*?options: \{[\s\S]*?\}[\s\S]*?\}\)/,
    `body: JSON.stringify({
                        model: 'BigDaddyG:Latest',  // Always use Latest, let Orchestra route
                        messages: [
                            { role: 'system', content: systemPrompt },
                            { role: 'user', content: fullContext }
                        ],
                        temperature: modelParams.temperature,
                        max_tokens: modelParams.num_predict
                    })`
);

// 8. Update response parsing from Ollama format to OpenAI format
html = html.replace(
    /const data = await response\.json\(\);[\s\S]*?const aiResponse = data\.response;/,
    `const data = await response.json();
                const aiResponse = data.choices[0].message.content;`
);

// 9. Remove model selection logic (Orchestra handles it)
html = html.replace(
    /\/\/ YOUR ACTUAL INSTALLED MODELS \(verified from Ollama\)[\s\S]*?const installedModels = \[.*?\];/,
    "// Orchestra handles model routing automatically"
);

// 10. Simplify to always use Orchestra
html = html.replace(
    /if \(aiQuality === 'auto'\) \{[\s\S]*?\} else if \(aiQuality === 'fast'\) \{[\s\S]*?\} else if \(aiQuality === 'max'\) \{[\s\S]*?\}/,
    `// Orchestra server handles model selection based on context
                console.log('Using BigDaddyG Orchestra for AI generation');`
);

// 11. Remove "Ollama offline" messages
html = html.replace(
    /Ollama offline/g,
    'Orchestra offline'
);

html = html.replace(
    /Ollama not connected/g,
    'Orchestra not connected'
);

// 12. Update startup message
html = html.replace(
    /showToast\('Using embedded BigDaddyG AI[\s\S]*?', 'success', \d+\);/,
    "showToast('🎼 BigDaddyG Orchestra - Connect to port 11441 for REAL AI', 'info', 8000);"
);

// Write the updated file
fs.writeFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', html);

console.log('✅ DONE! Ollama completely removed\n');
console.log('Changes made:');
console.log('  ✅ Removed all Ollama checks');
console.log('  ✅ Changed to Orchestra (port 11441) ONLY');
console.log('  ✅ Updated status display');
console.log('  ✅ Changed API calls to OpenAI format');
console.log('  ✅ Removed model selection (Orchestra handles it)');
console.log('  ✅ Updated error messages');
console.log('\n💡 Now start ONLY your Orchestra server:');
console.log('   cd neuro-symphonic-workspace');
console.log('   node BigDaddyG-Orchestra-Server.js');


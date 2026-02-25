// Script to add BigDaddyG AI Chat tab to the IDE
const fs = require('fs');

// Read the enhanced IDE
let content = fs.readFileSync('BigDaddyG-IDE-Enhanced.html', 'utf8');

// 1. Add the tab button after line 1733 (after Agents tab button)
const tabButtonHTML = `        <button class="bottom-tab-btn" onclick="switchTab('bigdaddyg-ai')">🧠 BigDaddyG AI</button>`;

content = content.replace(
    `<button class="bottom-tab-btn active" onclick="switchTab('agents')">🤖 Agents</button>`,
    `<button class="bottom-tab-btn active" onclick="switchTab('agents')">🤖 Agents</button>\n${tabButtonHTML}`
);

// 2. Add the AI Chat tab content after the agents tab (before the extensions tab)
const aiTabHTML = `
        <!-- BigDaddyG AI Chat Tab -->
        <div id="bigdaddyg-ai-tab" class="bottom-tab-content">
            <div style="padding: 15px; background: var(--void); color: var(--cyan); height: 100%; display: flex; flex-direction: column;">
                <div style="border-bottom: 1px solid var(--cyan); padding-bottom: 10px; margin-bottom: 10px;">
                    <h3 style="margin: 0;">🧠 BigDaddyG AI - Embedded Intelligence</h3>
                    <div style="display: flex; gap: 10px; margin-top: 10px; flex-wrap: wrap;">
                        <button onclick="selectAIModel('BigDaddyG:Latest')" id="model-latest" class="ai-model-btn ai-model-active" style="background: var(--cyan); color: var(--void); border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px; font-size: 11px;">Latest</button>
                        <button onclick="selectAIModel('BigDaddyG:Code')" id="model-code" class="ai-model-btn" style="background: var(--green); color: var(--void); border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px; font-size: 11px;">Code</button>
                        <button onclick="selectAIModel('BigDaddyG:Debug')" id="model-debug" class="ai-model-btn" style="background: var(--orange); color: var(--void); border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px; font-size: 11px;">Debug</button>
                        <button onclick="selectAIModel('BigDaddyG:Crypto')" id="model-crypto" class="ai-model-btn" style="background: var(--red); color: var(--void); border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px; font-size: 11px;">Crypto</button>
                    </div>
                    <div style="display: flex; gap: 10px; margin-top: 10px; align-items: center;">
                        <span style="font-size: 12px;">Emotional State:</span>
                        <button onclick="setEmotionalState('CALM')" class="emotion-btn" style="background: #6c7a89; color: white; border: none; padding: 3px 8px; cursor: pointer; border-radius: 3px; font-size: 10px;">CALM</button>
                        <button onclick="setEmotionalState('FOCUSED')" class="emotion-btn" style="background: #f39c12; color: white; border: none; padding: 3px 8px; cursor: pointer; border-radius: 3px; font-size: 10px;">FOCUSED</button>
                        <button onclick="setEmotionalState('INTENSE')" class="emotion-btn" style="background: #e74c3c; color: white; border: none; padding: 3px 8px; cursor: pointer; border-radius: 3px; font-size: 10px;">INTENSE</button>
                        <button onclick="setEmotionalState('OVERWHELMED')" class="emotion-btn" style="background: #8e44ad; color: white; border: none; padding: 3px 8px; cursor: pointer; border-radius: 3px; font-size: 10px;">OVERWHELMED</button>
                        <span id="current-emotion" style="font-size: 12px; margin-left: 10px; font-weight: bold; color: var(--green);">CALM</span>
                    </div>
                </div>
                
                <div id="ai-chat-container" style="flex: 1; overflow-y: auto; background: var(--bg); padding: 10px; border: 1px solid var(--cyan); margin-bottom: 10px; min-height: 200px;">
                    <div class="ai-message" style="margin-bottom: 10px; padding: 8px; background: rgba(0, 212, 255, 0.1); border-left: 3px solid var(--cyan); color: var(--cyan); font-size: 12px;">
                        <strong>BigDaddyG:</strong> Hello! I'm BigDaddyG, your embedded AI assistant. I can help with code generation, debugging, security, and more. Select a model above and ask me anything!
                    </div>
                </div>
                
                <div style="display: flex; gap: 5px;">
                    <input type="text" id="ai-input" placeholder="Ask BigDaddyG..." style="flex: 1; background: var(--bg); color: var(--cyan); border: 1px solid var(--cyan); padding: 8px; font-family: monospace; font-size: 12px;" onkeypress="handleAIKeyPress(event)" />
                    <button onclick="sendToAI()" style="background: var(--teal); color: var(--void); border: none; padding: 8px 15px; cursor: pointer; border-radius: 3px; font-weight: bold;">Send</button>
                    <button onclick="clearAIChat()" style="background: var(--red); color: var(--void); border: none; padding: 8px 15px; cursor: pointer; border-radius: 3px;">Clear</button>
                </div>
                
                <div style="margin-top: 10px; font-size: 11px; color: #888;">
                    <span>Total Queries: <span id="ai-query-count">0</span></span> | 
                    <span>Model: <span id="ai-current-model">BigDaddyG:Latest</span></span> | 
                    <span>Avg Response: <span id="ai-avg-response">0ms</span></span>
                </div>
            </div>
        </div>
`;

// Find the position to insert (after agents-tab div closes)
const extensionsTabIndex = content.indexOf(`<!-- Extensions Marketplace Tab -->`);
if (extensionsTabIndex === -1) {
    console.error('Could not find Extensions tab!');
    console.log('Looking for: <!-- Extensions Marketplace Tab -->');
    process.exit(1);
}

// Insert the AI tab before the Extensions tab
content = content.substring(0, extensionsTabIndex) + aiTabHTML + '\n\n' + content.substring(extensionsTabIndex);

// 3. Add JavaScript functions for AI chat
const aiJavaScriptHTML = `
        // ========================================
        // BIGDADDYG AI CHAT FUNCTIONS
        // ========================================
        let currentAIModel = 'BigDaddyG:Latest';
        
        function selectAIModel(model) {
            currentAIModel = model;
            document.getElementById('ai-current-model').textContent = model;
            
            // Update button styles
            document.querySelectorAll('.ai-model-btn').forEach(btn => {
                btn.classList.remove('ai-model-active');
                btn.style.opacity = '0.6';
            });
            
            const modelMap = {
                'BigDaddyG:Latest': 'model-latest',
                'BigDaddyG:Code': 'model-code',
                'BigDaddyG:Debug': 'model-debug',
                'BigDaddyG:Crypto': 'model-crypto'
            };
            
            const activeBtn = document.getElementById(modelMap[model]);
            if (activeBtn) {
                activeBtn.classList.add('ai-model-active');
                activeBtn.style.opacity = '1';
            }
            
            logToConsole(\`AI Model switched to: \${model}\`, 'info');
        }
        
        async function sendToAI() {
            const input = document.getElementById('ai-input');
            const prompt = input.value.trim();
            
            if (!prompt) {
                logToConsole('Please enter a message', 'error');
                return;
            }
            
            // Clear input
            input.value = '';
            
            // Add user message to chat
            const chatContainer = document.getElementById('ai-chat-container');
            const userMsg = document.createElement('div');
            userMsg.className = 'ai-message';
            userMsg.style.cssText = 'margin-bottom: 10px; padding: 8px; background: rgba(255, 107, 53, 0.1); border-left: 3px solid var(--orange); color: var(--orange); font-size: 12px;';
            userMsg.innerHTML = \`<strong>You:</strong> \${prompt}\`;
            chatContainer.appendChild(userMsg);
            
            // Scroll to bottom
            chatContainer.scrollTop = chatContainer.scrollHeight;
            
            // Show thinking indicator
            const thinkingMsg = document.createElement('div');
            thinkingMsg.className = 'ai-message thinking';
            thinkingMsg.style.cssText = 'margin-bottom: 10px; padding: 8px; background: rgba(0, 212, 255, 0.1); border-left: 3px solid var(--cyan); color: var(--cyan); font-size: 12px;';
            thinkingMsg.innerHTML = \`<strong>BigDaddyG:</strong> <em>Thinking...</em>\`;
            chatContainer.appendChild(thinkingMsg);
            chatContainer.scrollTop = chatContainer.scrollHeight;
            
            try {
                // Call the embedded BigDaddyG engine
                const response = await window.askBigDaddyG(prompt, currentAIModel);
                
                // Remove thinking indicator
                chatContainer.removeChild(thinkingMsg);
                
                // Add AI response
                const aiMsg = document.createElement('div');
                aiMsg.className = 'ai-message';
                aiMsg.style.cssText = 'margin-bottom: 10px; padding: 8px; background: rgba(0, 212, 255, 0.1); border-left: 3px solid var(--cyan); color: var(--cyan); font-size: 12px; white-space: pre-wrap; font-family: monospace;';
                aiMsg.innerHTML = \`<strong>BigDaddyG (\${currentAIModel}):</strong>\\n\\n\${response}\`;
                chatContainer.appendChild(aiMsg);
                
                // Update stats
                const stats = window.getBigDaddyGStats();
                document.getElementById('ai-query-count').textContent = stats.total_queries;
                document.getElementById('ai-avg-response').textContent = stats.avg_response_time + 'ms';
                
                // Scroll to bottom
                chatContainer.scrollTop = chatContainer.scrollHeight;
                
                logToConsole(\`AI response generated (\${currentAIModel})\`, 'success');
            } catch (error) {
                // Remove thinking indicator
                chatContainer.removeChild(thinkingMsg);
                
                // Show error
                const errorMsg = document.createElement('div');
                errorMsg.className = 'ai-message';
                errorMsg.style.cssText = 'margin-bottom: 10px; padding: 8px; background: rgba(255, 71, 87, 0.1); border-left: 3px solid var(--red); color: var(--red); font-size: 12px;';
                errorMsg.innerHTML = \`<strong>Error:</strong> \${error.message}\`;
                chatContainer.appendChild(errorMsg);
                chatContainer.scrollTop = chatContainer.scrollHeight;
                
                logToConsole('AI error: ' + error.message, 'error');
            }
        }
        
        function handleAIKeyPress(event) {
            if (event.key === 'Enter') {
                sendToAI();
            }
        }
        
        function clearAIChat() {
            const chatContainer = document.getElementById('ai-chat-container');
            chatContainer.innerHTML = \`<div class="ai-message" style="margin-bottom: 10px; padding: 8px; background: rgba(0, 212, 255, 0.1); border-left: 3px solid var(--cyan); color: var(--cyan); font-size: 12px;">
                <strong>BigDaddyG:</strong> Chat cleared. Ready for new questions!
            </div>\`;
            logToConsole('AI chat cleared', 'info');
        }
        
        function setEmotionalState(state) {
            window.setEmotionalState(state);
            document.getElementById('current-emotion').textContent = state;
            
            // Update emotion button styles
            document.querySelectorAll('.emotion-btn').forEach(btn => {
                btn.style.opacity = '0.5';
            });
            event.target.style.opacity = '1';
        }
`;

// Find where to insert the JavaScript (before the closing </script>)
const scriptCloseIndex = content.lastIndexOf('    </script>');
if (scriptCloseIndex === -1) {
    console.error('Could not find </script> tag!');
    process.exit(1);
}

content = content.substring(0, scriptCloseIndex) + aiJavaScriptHTML + '\n' + content.substring(scriptCloseIndex);

// Write the enhanced IDE
fs.writeFileSync('BigDaddyG-IDE-Enhanced.html', content);

console.log('✅ BigDaddyG AI Chat tab added successfully!');
console.log('📁 Output: BigDaddyG-IDE-Enhanced.html');
console.log('🧠 Features added:');
console.log('   - AI Chat interface with 4 models');
console.log('   - Emotional state controls');
console.log('   - Real-time stats display');
console.log('   - Chat history management');


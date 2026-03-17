/**
 * BigDaddyG IDE - Floating Centered Chat Panel
 * Like Cursor's Composer - opens in center, doesn't block sidebar
 */

(function() {
'use strict';

class FloatingChat {
    constructor() {
        this.isOpen = false;
        this.panel = null;
        this.deepResearchEnabled = false;
        this.selectedModel = 'auto'; // Default to auto-selection
        this.useNeuralNetwork = false; // Default to fast pattern matching
        this.attachedFiles = []; // Files attached to next message
        this.isDragging = false; // For draggable functionality
        this.dragOffset = { x: 0, y: 0 }; // Mouse offset during drag
        this.init();
    }
    
    init() {
        console.log('[FloatingChat] 🎯 Initializing floating chat...');
        
        // Wait for DOM to be ready before creating panel
        if (document.body) {
            this.createPanel();
            this.registerHotkeys();
            console.log('[FloatingChat] ✅ Floating chat ready (Ctrl+L to open)');
        } else {
            // DOM not ready yet, wait for it
            document.addEventListener('DOMContentLoaded', () => {
                this.createPanel();
                this.registerHotkeys();
                console.log('[FloatingChat] ✅ Floating chat ready (Ctrl+L to open)');
            });
        }
    }
    
    createPanel() {
        this.panel = document.createElement('div');
        this.panel.id = 'floating-chat-panel';
        this.panel.style.cssText = `
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%) scale(0.9);
            width: 800px;
            max-width: 90vw;
            height: 600px;
            max-height: 85vh;
            background: var(--cursor-bg);
            border: 2px solid var(--cursor-jade-dark);
            border-radius: 16px;
            box-shadow: 0 24px 48px rgba(0, 0, 0, 0.5), 0 0 0 1px rgba(119, 221, 190, 0.2);
            z-index: 10000;
            display: none;
            opacity: 0;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            backdrop-filter: blur(10px);
            overflow: hidden;
        `;
        
        this.panel.innerHTML = `
            <!-- Header -->
            <div id="floating-chat-header" style="padding: 16px 20px; background: linear-gradient(135deg, var(--cursor-bg-secondary), var(--cursor-bg-tertiary)); border-bottom: 1px solid var(--cursor-border); cursor: move;">
                <!-- Title Row -->
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
                    <div style="display: flex; align-items: center; gap: 12px;">
                        <span style="font-size: 20px;">🤖</span>
                        <span style="font-weight: 600; font-size: 15px; color: var(--cursor-jade-dark);">BigDaddyG AI</span>
                        <span style="font-size: 11px; color: var(--cursor-text-secondary); background: rgba(119, 221, 190, 0.1); padding: 3px 8px; border-radius: 12px;">Ctrl+L • Drag to move</span>
                    </div>
                    <div style="display: flex; gap: 8px;">
                        <button id="toggle-research-btn" onclick="floatingChat.toggleDeepResearch()" style="background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600; transition: all 0.2s;" onmouseover="this.style.background='rgba(119, 221, 190, 0.2)'" onmouseout="this.style.background='rgba(119, 221, 190, 0.1)'">
                            🔬 Deep Research: OFF
                        </button>
                        <button onclick="floatingChat.toggleSettings()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; transition: all 0.2s;" onmouseover="this.style.background='var(--cursor-bg-tertiary)'" onmouseout="this.style.background='none'">
                            ⚙️ Settings
                        </button>
                        <button onclick="floatingChat.minimize()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; transition: all 0.2s;" onmouseover="this.style.background='var(--cursor-bg-tertiary)'" onmouseout="this.style.background='none'">
                            ➖ Minimize
                        </button>
                        <button onclick="floatingChat.close()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; transition: all 0.2s;" onmouseover="this.style.background='rgba(255,71,87,0.1)'; this.style.borderColor='#ff4757'" onmouseout="this.style.background='none'; this.style.borderColor='var(--cursor-border)'">
                            ✕ Close
                        </button>
                    </div>
                </div>
                
                <!-- Model Selector Row -->
                <div style="display: flex; align-items: center; gap: 12px;">
                    <label style="font-size: 12px; color: var(--cursor-text-secondary); font-weight: 600;">🎯 Model:</label>
                    <select id="floating-model-selector" onchange="floatingChat.selectModel(this.value)" style="
                        flex: 1;
                        background: var(--cursor-bg);
                        border: 1px solid var(--cursor-jade-light);
                        color: var(--cursor-text);
                        padding: 6px 12px;
                        border-radius: 6px;
                        cursor: pointer;
                        font-size: 12px;
                        font-family: inherit;
                        outline: none;
                        transition: all 0.2s;
                    " onfocus="this.style.borderColor='var(--cursor-jade-dark)'; this.style.boxShadow='0 0 0 2px rgba(119, 221, 190, 0.2)'" onblur="this.style.borderColor='var(--cursor-jade-light)'; this.style.boxShadow='none'">
                        <option value="auto">🤖 Auto (Smart Selection)</option>
                        <option value="BigDaddyG:Latest">💎 BigDaddyG Latest (200K ASM/Security)</option>
                        <optgroup label="🎨 Language Specialists">
                            <option value="bigdaddyg-c">⚙️ C/C++ Specialist</option>
                            <option value="bigdaddyg-csharp">🎮 C# Specialist</option>
                            <option value="bigdaddyg-visualbasic">📊 Visual Basic Specialist</option>
                            <option value="bigdaddyg-python">🐍 Python Specialist</option>
                            <option value="bigdaddyg-javascript">🌐 JavaScript Specialist</option>
                            <option value="bigdaddyg-asm">🔧 Assembly Specialist</option>
                        </optgroup>
                        <optgroup label="🎭 Special Purpose">
                            <option value="bigdaddyg-image">🖼️ Image Generation</option>
                            <option value="cheetah-stealth">🔐 Cheetah Stealth (Security)</option>
                            <option value="code-supernova">⭐ Code Supernova (Multi-lang)</option>
                        </optgroup>
                        <optgroup label="🤖 Neural Network Models (Built-in)" id="neural-models-group">
                            <option value="bigdaddyg:latest">💎 BigDaddyG Latest (4.7GB, Built-in)</option>
                        </optgroup>
                    </select>
                    <button onclick="floatingChat.toggleAIMode()" id="ai-mode-toggle" style="
                        background: rgba(119, 221, 190, 0.1);
                        border: 1px solid var(--cursor-jade-light);
                        color: var(--cursor-jade-dark);
                        padding: 6px 12px;
                        border-radius: 6px;
                        cursor: pointer;
                        font-size: 11px;
                        font-weight: 600;
                        white-space: nowrap;
                        transition: all 0.2s;
                    " title="Toggle between Pattern Matching (⚡ Fast 0.1s) and Neural Network (🤖 Smart 2-10s)" onmouseover="this.style.background='rgba(119, 221, 190, 0.2)'" onmouseout="this.style.background='rgba(119, 221, 190, 0.1)'">
                        <span id="mode-toggle-text">⚡ Pattern Mode</span>
                    </button>
                </div>
            </div>
            
            <!-- Settings Panel (Collapsible) -->
            <div id="floating-settings-panel" style="display: none; background: var(--cursor-bg-secondary); border-bottom: 1px solid var(--cursor-border); padding: 16px 20px; max-height: 300px; overflow-y: auto;">
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px;">
                    <!-- Temperature -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">🌡️ Temperature (Creativity)</label>
                        <input type="range" id="param-temperature" min="0" max="2" step="0.1" value="0.7" style="width: 100%;" oninput="floatingChat.updateParamDisplay('temperature', this.value)">
                        <div style="font-size: 10px; color: var(--cursor-jade-dark); margin-top: 2px;">Value: <span id="param-temperature-val">0.7</span></div>
                    </div>
                    
                    <!-- Top P -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">🎯 Top P (Nucleus)</label>
                        <input type="range" id="param-top_p" min="0" max="1" step="0.05" value="0.9" style="width: 100%;" oninput="floatingChat.updateParamDisplay('top_p', this.value)">
                        <div style="font-size: 10px; color: var(--cursor-jade-dark); margin-top: 2px;">Value: <span id="param-top_p-val">0.9</span></div>
                    </div>
                    
                    <!-- Top K -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">🔢 Top K</label>
                        <input type="range" id="param-top_k" min="1" max="100" step="1" value="40" style="width: 100%;" oninput="floatingChat.updateParamDisplay('top_k', this.value)">
                        <div style="font-size: 10px; color: var(--cursor-jade-dark); margin-top: 2px;">Value: <span id="param-top_k-val">40</span></div>
                    </div>
                    
                    <!-- Repeat Penalty -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">🔁 Repeat Penalty</label>
                        <input type="range" id="param-repeat_penalty" min="1" max="2" step="0.1" value="1.1" style="width: 100%;" oninput="floatingChat.updateParamDisplay('repeat_penalty', this.value)">
                        <div style="font-size: 10px; color: var(--cursor-jade-dark); margin-top: 2px;">Value: <span id="param-repeat_penalty-val">1.1</span></div>
                    </div>
                    
                    <!-- Response Style -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">📝 Response Style</label>
                        <select id="param-response_style" style="width: 100%; padding: 6px; background: var(--cursor-bg); border: 1px solid var(--cursor-border); border-radius: 4px; color: var(--cursor-text); font-size: 12px;">
                            <option value="concise">Concise</option>
                            <option value="detailed" selected>Detailed</option>
                            <option value="technical">Technical</option>
                        </select>
                    </div>
                    
                    <!-- Code Quality -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">⭐ Code Quality</label>
                        <select id="param-code_quality" style="width: 100%; padding: 6px; background: var(--cursor-bg); border: 1px solid var(--cursor-border); border-radius: 4px; color: var(--cursor-text); font-size: 12px;">
                            <option value="prototype">Prototype</option>
                            <option value="production" selected>Production</option>
                            <option value="optimized">Optimized</option>
                        </select>
                    </div>
                    
                    <!-- Explanation Level -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">🎓 Explanation Level</label>
                        <select id="param-explanation_level" style="width: 100%; padding: 6px; background: var(--cursor-bg); border: 1px solid var(--cursor-border); border-radius: 4px; color: var(--cursor-text); font-size: 12px;">
                            <option value="beginner">Beginner</option>
                            <option value="intermediate" selected>Intermediate</option>
                            <option value="expert">Expert</option>
                        </select>
                    </div>
                    
                    <!-- Max Tokens -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">📊 Max Tokens</label>
                        <input type="number" id="param-max_tokens" min="100" max="100000" step="100" value="4000" style="width: 100%; padding: 6px; background: var(--cursor-bg); border: 1px solid var(--cursor-border); border-radius: 4px; color: var(--cursor-text); font-size: 12px;">
                    </div>
                </div>
                
                <!-- Thinking Time & Model Size -->
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 12px; padding-top: 12px; border-top: 1px solid var(--cursor-border);">
                    <!-- Thinking Time -->
                    <div style="grid-column: 1 / -1;">
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">⏱️ Max Thinking Time</label>
                        <input type="range" id="param-thinking_time" min="5" max="600" step="5" value="30" style="width: 100%;" oninput="floatingChat.updateThinkingTimeDisplay(this.value)">
                        <div style="display: flex; justify-content: space-between; font-size: 10px; color: var(--cursor-jade-dark); margin-top: 4px;">
                            <span>Value: <strong id="param-thinking_time-val">30s</strong></span>
                            <div style="display: flex; gap: 4px;">
                                <button onclick="floatingChat.setThinkingTime(30)" style="padding: 2px 6px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); border-radius: 3px; cursor: pointer; font-size: 9px;">Quick (30s)</button>
                                <button onclick="floatingChat.setThinkingTime(120)" style="padding: 2px 6px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); border-radius: 3px; cursor: pointer; font-size: 9px;">Normal (2m)</button>
                                <button onclick="floatingChat.setThinkingTime(300)" style="padding: 2px 6px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); border-radius: 3px; cursor: pointer; font-size: 9px;">Deep (5m)</button>
                                <button onclick="floatingChat.setThinkingTime(600)" style="padding: 2px 6px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); border-radius: 3px; cursor: pointer; font-size: 9px;">Max (10m)</button>
                            </div>
                        </div>
                    </div>
                    
                    <!-- Model Size Preset -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">🧠 Model Size Preset</label>
                        <select id="param-model_preset" onchange="floatingChat.applyModelPreset(this.value)" style="width: 100%; padding: 6px; background: var(--cursor-bg); border: 1px solid var(--cursor-border); border-radius: 4px; color: var(--cursor-text); font-size: 12px;">
                            <option value="custom">Custom Settings</option>
                            <option value="7b">7B Model (Fast)</option>
                            <option value="13b">13B Model (Balanced)</option>
                            <option value="34b">34B Model (Quality)</option>
                            <option value="40gb" selected>40GB+ Model (Elite)</option>
                            <option value="70b">70B+ Model (Maximum)</option>
                        </select>
                    </div>
                    
                    <!-- Timeout Strategy -->
                    <div>
                        <label style="display: block; font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px; font-weight: 600;">⚡ Timeout Strategy</label>
                        <select id="param-timeout_strategy" style="width: 100%; padding: 6px; background: var(--cursor-bg); border: 1px solid var(--cursor-border); border-radius: 4px; color: var(--cursor-text); font-size: 12px;">
                            <option value="strict">Strict (Fail on timeout)</option>
                            <option value="graceful" selected>Graceful (Return partial)</option>
                            <option value="retry">Retry (Try again)</option>
                            <option value="wait">Wait (No limit)</option>
                        </select>
                    </div>
                </div>
                
                <!-- Context & Actions -->
                <div style="display: flex; gap: 8px; padding-top: 12px; border-top: 1px solid var(--cursor-border);">
                    <button onclick="floatingChat.clearContext()" style="flex: 1; background: rgba(255, 152, 0, 0.1); border: 1px solid var(--orange); color: var(--orange); padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;" onmouseover="this.style.background='rgba(255, 152, 0, 0.2)'" onmouseout="this.style.background='rgba(255, 152, 0, 0.1)'">
                        🗑️ Clear Context (1M Tokens)
                    </button>
                    <button onclick="floatingChat.applyParameters()" style="flex: 1; background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; box-shadow: 0 2px 8px rgba(119, 221, 190, 0.3);">
                        ✅ Apply Settings
                    </button>
                    <button onclick="floatingChat.resetParameters()" style="flex: 1; background: rgba(255, 71, 87, 0.1); border: 1px solid #ff4757; color: #ff4757; padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;" onmouseover="this.style.background='rgba(255, 71, 87, 0.2)'" onmouseout="this.style.background='rgba(255, 71, 87, 0.1)'">
                        🔄 Reset to Defaults
                    </button>
                </div>
                
                <div id="context-info" style="margin-top: 12px; padding: 8px; background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-jade-light); border-radius: 6px; font-size: 10px; color: var(--cursor-text-secondary);">
                    💎 Context Window: 1M tokens | 🧠 Memory: Loading...
                    <div style="margin-top: 4px; font-family: monospace; font-size: 9px;">
                        <span id="context-usage-bar" style="display: inline-block; width: 100px; height: 8px; background: rgba(0,0,0,0.2); border-radius: 4px; overflow: hidden;">
                            <span id="context-usage-fill" style="display: block; height: 100%; background: linear-gradient(90deg, var(--cursor-jade-dark), var(--cursor-accent)); width: 0%; transition: width 0.3s;"></span>
                        </span>
                        <span id="context-percentage" style="margin-left: 8px;">0%</span>
                    </div>
                </div>
            </div>
            
            <!-- Chat Messages -->
            <div id="floating-chat-messages" style="
                height: calc(100% - 180px); 
                overflow-y: auto !important; 
                overflow-x: hidden;
                padding: 20px; 
                scroll-behavior: smooth;
                display: flex;
                flex-direction: column;
                max-height: calc(100% - 180px);
            ">
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 40px 20px;">
                    <div style="font-size: 48px; margin-bottom: 16px;">💬</div>
                    <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">Start a conversation</div>
                    <div style="font-size: 13px;">Ask me anything about your code or project!</div>
                </div>
            </div>
            
            <!-- Input Area -->
            <div style="position: absolute; bottom: 0; left: 0; right: 0; padding: 16px 20px; background: var(--cursor-bg-secondary); border-top: 1px solid var(--cursor-border);">
                <div style="position: relative;">
                    <textarea 
                        id="floating-chat-input" 
                        placeholder="@ for context, / for commands, or drag files here 📎"
                        style="
                            width: 100%;
                            min-height: 80px;
                            max-height: 200px;
                            padding: 12px 80px 12px 12px;
                            background: var(--cursor-bg);
                            border: 1px solid var(--cursor-jade-light);
                            border-radius: 8px;
                            color: var(--cursor-text);
                            font-size: 13px;
                            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                            resize: vertical;
                            outline: none;
                            transition: all 0.2s;
                        "
                        onkeydown="if (event.ctrlKey && event.key === 'Enter') { event.preventDefault(); floatingChat.send(); }"
                        onfocus="this.style.borderColor='var(--cursor-jade-dark)'; this.style.boxShadow='0 0 0 3px rgba(119, 221, 190, 0.1)'"
                        onblur="this.style.borderColor='var(--cursor-jade-light)'; this.style.boxShadow='none'"
                    ></textarea>
                    <!-- File Upload Button -->
                    <input type="file" id="floating-file-input" multiple style="display: none;" onchange="floatingChat.handleFileSelect(event)">
                    <button 
                        onclick="document.getElementById('floating-file-input').click()" 
                        style="
                            position: absolute;
                            right: 120px;
                            bottom: 12px;
                            background: rgba(119, 221, 190, 0.1);
                            border: 1px solid var(--cursor-jade-light);
                            color: var(--cursor-jade-dark);
                            padding: 10px 14px;
                            border-radius: 6px;
                            cursor: pointer;
                            font-size: 16px;
                            font-weight: 600;
                            transition: all 0.2s;
                        "
                        onmouseover="this.style.background='rgba(119, 221, 190, 0.2)'; this.style.transform='translateY(-1px)'"
                        onmouseout="this.style.background='rgba(119, 221, 190, 0.1)'; this.style.transform='translateY(0)'"
                        title="Attach files (or drag & drop)"
                    >
                        📎
                    </button>
                    <button 
                        onclick="floatingChat.send()" 
                        style="
                            position: absolute;
                            right: 12px;
                            bottom: 12px;
                            background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                            border: none;
                            color: white;
                            padding: 10px 16px;
                            border-radius: 6px;
                            cursor: pointer;
                            font-size: 13px;
                            font-weight: 600;
                            transition: all 0.2s;
                            box-shadow: 0 2px 8px rgba(119, 221, 190, 0.3);
                        "
                        onmouseover="this.style.transform='translateY(-1px)'; this.style.boxShadow='0 4px 12px rgba(119, 221, 190, 0.4)'"
                        onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='0 2px 8px rgba(119, 221, 190, 0.3)'"
                    >
                        ↑ Send
                    </button>
                </div>
                <div id="floating-attached-files" style="margin-top: 8px; display: none; flex-wrap: wrap; gap: 8px;"></div>
                <div style="margin-top: 8px; font-size: 11px; color: var(--cursor-text-secondary); display: flex; justify-content: space-between;">
                    <span>💡 Press Ctrl+Enter to send • 📎 Drag files or click attach</span>
                    <span id="floating-chat-counter">0 / 10,000</span>
                </div>
            </div>
        `;
        
        document.body.appendChild(this.panel);
        
        // Make draggable
        this.makeDraggable();
        
        // Add character counter
        const input = document.getElementById('floating-chat-input');
        if (input) {
            input.addEventListener('input', () => {
                const counter = document.getElementById('floating-chat-counter');
                if (counter) {
                    counter.textContent = `${input.value.length} / 10,000`;
                }
            });
        }
    }
    
    makeDraggable() {
        const header = document.getElementById('floating-chat-header');
        if (!header) {
            console.error('[FloatingChat] ❌ Header not found for dragging');
            return;
        }
        
        let isDragging = false;
        let currentX = 0;
        let currentY = 0;
        let initialX = 0;
        let initialY = 0;
        
        header.addEventListener('mousedown', (e) => {
            // Don't drag if clicking buttons or select elements
            if (e.target.tagName === 'BUTTON' || 
                e.target.tagName === 'SELECT' || 
                e.target.tagName === 'INPUT' ||
                e.target.closest('button') ||
                e.target.closest('select')) {
                return;
            }
            
            isDragging = true;
            
            // Get panel's current position
            const rect = this.panel.getBoundingClientRect();
            
            // Calculate offset from click to panel edge
            initialX = e.clientX - rect.left;
            initialY = e.clientY - rect.top;
            
            // Change cursor
            document.body.style.cursor = 'grabbing';
            header.style.cursor = 'grabbing';
            
            e.preventDefault();
        });
        
        document.addEventListener('mousemove', (e) => {
            if (!isDragging) return;
            
            e.preventDefault();
            
            // Calculate new position
            currentX = e.clientX - initialX;
            currentY = e.clientY - initialY;
            
            // Constrain to viewport
            const maxX = window.innerWidth - this.panel.offsetWidth;
            const maxY = window.innerHeight - this.panel.offsetHeight;
            
            currentX = Math.max(0, Math.min(currentX, maxX));
            currentY = Math.max(0, Math.min(currentY, maxY));
            
            // Update panel position
            this.panel.style.transform = 'none';
            this.panel.style.left = currentX + 'px';
            this.panel.style.top = currentY + 'px';
        });
        
        document.addEventListener('mouseup', () => {
            if (isDragging) {
                isDragging = false;
                document.body.style.cursor = '';
                header.style.cursor = 'move';
            }
        });
        
        console.log('[FloatingChat] ✅ Draggable functionality enabled');
    }
    
    registerHotkeys() {
        // Ctrl+L now handled by hotkey-manager.js
        // Keep Escape handler for modal closing
        document.addEventListener('keydown', (e) => {
            // Escape to close (specific to this modal)
            if (e.key === 'Escape' && this.isOpen) {
                e.preventDefault();
                this.close();
            }
        });
    }
    
    open() {
        if (this.isOpen) return;
        
        this.isOpen = true;
        this.panel.style.display = 'block';
        
        // Animate in
        setTimeout(() => {
            this.panel.style.opacity = '1';
            this.panel.style.transform = 'translate(-50%, -50%) scale(1)';
        }, 10);
        
        // Focus input
        setTimeout(() => {
            const input = document.getElementById('floating-chat-input');
            if (input) input.focus();
        }, 300);
        
        // Load available models (neural networks, Ollama blobs, etc.)
        this.loadAvailableModels();
        
        console.log('[FloatingChat] ✨ Opened');
    }
    
    close() {
        if (!this.isOpen) return;
        
        this.isOpen = false;
        this.panel.style.opacity = '0';
        this.panel.style.transform = 'translate(-50%, -50%) scale(0.9)';
        
        setTimeout(() => {
            this.panel.style.display = 'none';
        }, 300);
        
        console.log('[FloatingChat] 🔽 Closed');
    }
    
    minimize() {
        if (!this.isOpen) return;
        
        this.isOpen = false;
        this.panel.style.opacity = '0';
        this.panel.style.transform = 'translate(-50%, -50%) scale(0.9)';
        
        setTimeout(() => {
            this.panel.style.display = 'none';
        }, 300);
        
        console.log('[FloatingChat] ➖ Minimized');
    }
    
    toggle() {
        if (this.isOpen) {
            this.close();
        } else {
            this.open();
        }
    }
    
    // Helper for demos to ensure chat is closed
    ensureClosed() {
        if (this.isOpen) {
            this.close();
        }
    }
    
    async selectModel(modelName) {
        try {
            console.log(`[FloatingChat] 🎯 Switching to model: ${modelName}`);
            
            this.selectedModel = modelName;
            
            // Update mode indicator
            const indicator = document.getElementById('model-mode-indicator');
            if (indicator) {
                if (modelName === 'auto') {
                    indicator.textContent = '⚡ Auto Mode';
                    indicator.style.background = 'rgba(119, 221, 190, 0.1)';
                } else if (modelName.includes('sha256-')) {
                    indicator.textContent = '🤖 Neural Network';
                    indicator.style.background = 'rgba(0, 150, 255, 0.1)';
                } else {
                    indicator.textContent = '⚡ Fast Mode';
                    indicator.style.background = 'rgba(119, 221, 190, 0.1)';
                }
            }
            
            // Show notification
            this.addSystemMessage(`✅ Switched to: ${this.getModelDisplayName(modelName)}`);
            
        } catch (error) {
            console.error('[FloatingChat] ❌ Error selecting model:', error);
        }
    }
    
    getModelDisplayName(modelName) {
        const displayNames = {
            'auto': '🤖 Auto (Smart Selection)',
            'BigDaddyG:Latest': '💎 BigDaddyG Latest',
            'bigdaddyg-c': '⚙️ C/C++ Specialist',
            'bigdaddyg-csharp': '🎮 C# Specialist',
            'bigdaddyg-visualbasic': '📊 Visual Basic Specialist',
            'bigdaddyg-python': '🐍 Python Specialist',
            'bigdaddyg-javascript': '🌐 JavaScript Specialist',
            'bigdaddyg-asm': '🔧 Assembly Specialist',
            'bigdaddyg-image': '🖼️ Image Generation',
            'cheetah-stealth': '🔐 Cheetah Stealth',
            'code-supernova': '⭐ Code Supernova'
        };
        return displayNames[modelName] || modelName;
    }
    
    // Toggle AI Mode (Pattern Matching vs Neural Network)
    toggleAIMode() {
        this.useNeuralNetwork = !this.useNeuralNetwork;
        const mode = this.useNeuralNetwork ? 'Neural Network' : 'Pattern Matching';
        console.log(`[FloatingChat] 🔄 AI Mode: ${mode}`);
        this.addSystemMessage(`🔄 Switched to ${mode} mode`);
        
        // Update UI indicator if present
        const indicator = document.querySelector('[data-ai-mode]');
        if (indicator) {
            indicator.textContent = this.useNeuralNetwork ? '🤖 Neural' : '⚡ Pattern';
        }
    }
    
    // Handle file selection from file input
    handleFileSelect(event) {
        const files = event.target.files;
        if (!files || files.length === 0) return;
        
        console.log(`[FloatingChat] 📎 Selected ${files.length} file(s)`);
        
        // Read files and attach to next message
        const filePromises = Array.from(files).map(file => {
            return new Promise((resolve, reject) => {
                const reader = new FileReader();
                reader.onload = (e) => {
                    resolve({
                        name: file.name,
                        size: file.size,
                        type: file.type,
                        content: e.target.result
                    });
                };
                reader.onerror = reject;
                reader.readAsText(file);
            });
        });
        
        Promise.all(filePromises).then(fileContents => {
            this.attachedFiles = fileContents;
            this.addSystemMessage(`📎 Attached ${fileContents.length} file(s): ${fileContents.map(f => f.name).join(', ')}`);
        }).catch(error => {
            console.error('[FloatingChat] ❌ Error reading files:', error);
            this.addSystemMessage(`❌ Error reading files: ${error.message}`);
        });
    }
    
    async loadAvailableModels() {
        try {
            // Query Orchestra for available neural network models
            const response = await fetch('http://localhost:11441/api/ai-mode', {
                signal: AbortSignal.timeout(3000) // 3 second timeout
            });
            
            if (!response.ok) {
                // Endpoint doesn't exist - silently continue with default models
                return;
            }
            
            const data = await response.json();
            
            // Check if neural network models are available
            if (data.loaded && data.mode === 'neural_network') {
                const neuralGroup = document.getElementById('neural-models-group');
                if (neuralGroup) {
                    neuralGroup.style.display = 'block';
                    
                    // Add the loaded model
                    const option = document.createElement('option');
                    option.value = 'neural_network_active';
                    option.textContent = `🤖 ${data.model || 'Neural Network'} (${data.mode})`;
                    neuralGroup.appendChild(option);
                }
                
                // Update mode indicator
                const indicator = document.getElementById('model-mode-indicator');
                if (indicator) {
                    indicator.textContent = '🤖 Neural Network';
                    indicator.style.background = 'rgba(0, 150, 255, 0.1)';
                }
            }
            
            console.log('[FloatingChat] ✅ Available models loaded');
        } catch (error) {
            console.log('[FloatingChat] ℹ️ Could not load neural network models (using pattern matching)');
        }
    }
    
    addSystemMessage(text) {
        const messagesContainer = document.getElementById('floating-chat-messages');
        if (!messagesContainer) return;
        
        const messageDiv = document.createElement('div');
        messageDiv.style.cssText = `
            padding: 8px 12px;
            margin: 8px 0;
            background: rgba(119, 221, 190, 0.1);
            border-left: 3px solid var(--cursor-jade-dark);
            border-radius: 4px;
            font-size: 12px;
            color: var(--cursor-text-secondary);
        `;
        messageDiv.textContent = text;
        messagesContainer.appendChild(messageDiv);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }
    
    async send() {
        try {
            const input = document.getElementById('floating-chat-input');
            if (!input) {
                console.error('[FloatingChat] ❌ Input element not found');
                return;
            }
            
            const message = input.value.trim();
            if (!message) return;
            
            input.value = '';
            console.log(`[FloatingChat] 📤 Sending message (Deep Research: ${this.deepResearchEnabled ? 'ON' : 'OFF'})`);
            
            // Add user message
            this.addUserMessage(message);
            
            // Create AI response with expandable sections
            await this.sendToAI(message);
            
        } catch (error) {
            console.error('[FloatingChat] ❌ Error in send():', error);
            this.addMessage('assistant', `❌ Error: ${error.message}`, true);
        }
    }
    
    addUserMessage(message) {
        try {
            const container = document.getElementById('floating-chat-messages');
            if (!container) {
                console.error('[FloatingChat] ❌ Messages container not found');
                return;
            }
            
            // Clear welcome if present
            if (container.querySelector('[style*="text-align: center"]')) {
                container.innerHTML = '';
            }
            
            const msgDiv = document.createElement('div');
            msgDiv.style.cssText = `
                margin-bottom: 16px;
                padding: 14px;
                background: rgba(255, 152, 0, 0.1);
                border-left: 4px solid var(--orange);
                border-radius: 8px;
            `;
            msgDiv.innerHTML = `
                <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 8px;">
                    <span style="font-size: 16px;">👤</span>
                    <span style="font-weight: 600; color: var(--orange);">You</span>
                    <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
                </div>
                <div style="color: var(--cursor-text); white-space: pre-wrap; font-size: 13px;">${this.escapeHtml(message)}</div>
            `;
            container.appendChild(msgDiv);
            container.scrollTop = container.scrollHeight;
            
            console.log('[FloatingChat] ✅ User message added');
        } catch (error) {
            console.error('[FloatingChat] ❌ Error adding user message:', error);
        }
    }
    
    async sendToAI(message) {
        const container = document.getElementById('floating-chat-messages');
        if (!container) {
            console.error('[FloatingChat] ❌ Messages container not found');
            return;
        }
        
        const responseId = `ai-response-${Date.now()}`;
        
        // Add to context manager if available
        if (window.contextManager) {
            window.contextManager.addToContext(message, 'user');
        }
        
        try {
            // Create AI response container with thinking/reading sections
            const msgDiv = document.createElement('div');
            msgDiv.id = responseId;
            msgDiv.style.cssText = `
                margin-bottom: 16px;
                padding: 14px;
                background: rgba(119, 221, 190, 0.1);
                border-left: 4px solid var(--cursor-jade-dark);
                border-radius: 8px;
            `;
            
            const thinkingDisplay = this.deepResearchEnabled ? 'block' : 'none';
            
            msgDiv.innerHTML = `
                <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 12px;">
                    <span style="font-size: 16px;">🤖</span>
                    <span style="font-weight: 600; color: var(--cursor-jade-dark);">BigDaddyG</span>
                    <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
                    <span id="${responseId}-status" style="font-size: 10px; color: var(--cursor-accent); margin-left: auto;">● Thinking...</span>
                    <span id="${responseId}-timer" style="font-size: 10px; color: var(--cursor-jade-dark); font-family: monospace; background: rgba(119, 221, 190, 0.1); padding: 2px 8px; border-radius: 10px;">0.0s</span>
                </div>
                
                <!-- Expandable Thinking Section -->
                <div id="${responseId}-thinking-section" style="display: ${thinkingDisplay}; margin-bottom: 12px;">
                    <button onclick="floatingChat.toggleSection('${responseId}-thinking')" style="width: 100%; text-align: left; background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-jade-light); padding: 8px 12px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; color: var(--cursor-jade-dark); transition: all 0.2s; display: flex; align-items: center; gap: 8px; user-select: none;">
                        <span id="${responseId}-thinking-toggle">▶</span>
                        <span>💭 Thinking Process</span>
                        <span style="margin-left: auto; font-size: 10px; color: var(--cursor-text-muted);">Click to expand</span>
                    </button>
                    <div id="${responseId}-thinking" style="display: none; margin-top: 8px; padding: 10px; background: rgba(0,0,0,0.2); border-radius: 6px; font-family: 'Courier New', monospace; font-size: 11px; color: var(--cursor-text-secondary); max-height: 200px; overflow-y: auto; user-select: text; cursor: text;">
                        <div style="color: var(--cursor-accent); user-select: text;">Analyzing your request...</div>
                    </div>
                </div>
                
                <!-- Expandable Read Section -->
                <div id="${responseId}-read-section" style="display: ${thinkingDisplay}; margin-bottom: 12px;">
                    <button onclick="floatingChat.toggleSection('${responseId}-read')" style="width: 100%; text-align: left; background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-jade-light); padding: 8px 12px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; color: var(--cursor-jade-dark); transition: all 0.2s; display: flex; align-items: center; gap: 8px; user-select: none;">
                        <span id="${responseId}-read-toggle">▶</span>
                        <span>📖 Files Read</span>
                        <span id="${responseId}-read-count" style="margin-left: auto; font-size: 10px; background: rgba(119, 221, 190, 0.2); padding: 2px 8px; border-radius: 10px;">0 files</span>
                    </button>
                    <div id="${responseId}-read" style="display: none; margin-top: 8px; padding: 10px; background: rgba(0,0,0,0.2); border-radius: 6px; font-size: 11px; color: var(--cursor-text-secondary); max-height: 200px; overflow-y: auto; user-select: text; cursor: text;">
                        <div style="color: var(--cursor-text-muted); font-style: italic; user-select: text;">No files analyzed yet...</div>
                    </div>
                </div>
                
                 <!-- AI Response Content -->
                <div id="${responseId}-content" style="color: var(--cursor-text); white-space: pre-wrap; font-size: 13px; line-height: 1.6; user-select: text; cursor: text;">
                    <div style="display: inline-block; width: 8px; height: 12px; background: var(--cursor-jade-dark); animation: blink 1s infinite; margin-right: 2px; user-select: none;"></div>
                </div>
            `;
            
            container.appendChild(msgDiv);
            container.scrollTop = container.scrollHeight;
            
            // Start thinking timer
            const startTime = Date.now();
            const timerInterval = setInterval(() => {
                const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
                const timerEl = document.getElementById(`${responseId}-timer`);
                if (timerEl) {
                    timerEl.textContent = `${elapsed}s`;
                    
                    // Change color based on elapsed time
                    if (elapsed > 60) {
                        timerEl.style.color = '#ff4757'; // Red after 1 minute
                    } else if (elapsed > 30) {
                        timerEl.style.color = 'var(--orange)'; // Orange after 30s
                    }
                }
            }, 100);
            
            // Get parameters if settings panel is open
            const params = this.getParameters();
            console.log('[FloatingChat] 🎯 Sending to Orchestra with params:', params);
            console.log(`[FloatingChat] ⏱️ Max thinking time: ${params.thinking_time}s, Strategy: ${params.timeout_strategy}`);
            
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message,
                    model: this.selectedModel || 'auto',
                    parameters: params,
                    deep_research: this.deepResearchEnabled,
                    include_thinking: this.deepResearchEnabled,
                    include_reading: this.deepResearchEnabled
                })
            });
            
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}`);
            }
            
            const data = await response.json();
            console.log('[FloatingChat] 📨 Response received:', data);
            
            // Update thinking section if provided
            if (data.thinking && this.deepResearchEnabled) {
                const thinkingContent = document.getElementById(`${responseId}-thinking`);
                if (thinkingContent) {
                    thinkingContent.innerHTML = this.formatThinking(data.thinking);
                    console.log('[FloatingChat] 💭 Thinking process displayed');
                }
            }
            
            // Update read section if provided
            if (data.files_read && this.deepResearchEnabled) {
                const readContent = document.getElementById(`${responseId}-read`);
                const readCount = document.getElementById(`${responseId}-read-count`);
                if (readContent && data.files_read.length > 0) {
                    readContent.innerHTML = data.files_read.map(file => `
                        <div style="margin-bottom: 6px; padding: 6px; background: rgba(119, 221, 190, 0.05); border-radius: 4px;">
                            <div style="font-weight: 600; color: var(--cursor-jade-dark); font-size: 11px;">📄 ${file.name || file.path}</div>
                            <div style="font-size: 10px; color: var(--cursor-text-muted); margin-top: 2px;">${file.lines || '?'} lines read</div>
                        </div>
                    `).join('');
                    
                    if (readCount) {
                        readCount.textContent = `${data.files_read.length} file${data.files_read.length > 1 ? 's' : ''}`;
                    }
                    console.log(`[FloatingChat] 📖 Displayed ${data.files_read.length} files read`);
                }
            }
            
            // Stop timer
            clearInterval(timerInterval);
            
            // Update status
            const status = document.getElementById(`${responseId}-status`);
            if (status) {
                status.textContent = '✓ Complete';
                status.style.color = 'var(--cursor-jade-dark)';
            }
            
            // Final timer update
            const finalTime = ((Date.now() - startTime) / 1000).toFixed(1);
            const timerEl = document.getElementById(`${responseId}-timer`);
            if (timerEl) {
                timerEl.textContent = `✓ ${finalTime}s`;
                timerEl.style.background = 'rgba(119, 221, 190, 0.2)';
                timerEl.style.color = 'var(--cursor-jade-dark)';
            }
            
            // Display response
            const contentDiv = document.getElementById(`${responseId}-content`);
            if (contentDiv) {
                const responseText = data.response || data.message || 'No response';
                contentDiv.innerHTML = this.formatResponse(responseText);
                
                // Add to context manager
                if (window.contextManager) {
                    window.contextManager.addToContext(responseText, 'assistant');
                    // Update context display
                    this.updateContextDisplay();
                }
                
                // CRITICAL: Scroll to show full response after content is rendered
                this.scrollToBottom();
            }
            
            console.log(`[FloatingChat] ✅ AI response rendered successfully in ${finalTime}s`);
            
        } catch (error) {
            console.error('[FloatingChat] ❌ Error sending to AI:', error);
            
            // Stop timer
            if (typeof timerInterval !== 'undefined') {
                clearInterval(timerInterval);
            }
            
            const status = document.getElementById(`${responseId}-status`);
            if (status) {
                status.textContent = '✕ Error';
                status.style.color = '#ff4757';
            }
            
            // Update timer to show error
            const timerEl = document.getElementById(`${responseId}-timer`);
            if (timerEl) {
                timerEl.textContent = '✕ Failed';
                timerEl.style.background = 'rgba(255, 71, 87, 0.2)';
                timerEl.style.color = '#ff4757';
            }
            
            const contentDiv = document.getElementById(`${responseId}-content`);
            if (contentDiv) {
                contentDiv.innerHTML = `
                    <div style="color: #ff4757; font-weight: 600; margin-bottom: 8px;">❌ Connection Error</div>
                    <div style="font-size: 12px; color: var(--cursor-text-secondary); line-height: 1.6;">
                        Could not connect to Orchestra server at <code>http://localhost:11441/api/chat</code>
                        <br><br>
                        <strong>Error:</strong> ${this.escapeHtml(error.message)}
                        <br><br>
                        <strong>Stack:</strong>
                        <pre style="font-size: 10px; background: rgba(0,0,0,0.3); padding: 8px; border-radius: 4px; overflow-x: auto; margin-top: 6px;">${this.escapeHtml(error.stack || 'No stack trace')}</pre>
                        <br>
                        💡 Check the Console panel (Orchestra tab) for server status
                    </div>
                `;
            }
        }
        
        container.scrollTop = container.scrollHeight;
    }
    
    addMessage(role, content, isError = false) {
        const container = document.getElementById('floating-chat-messages');
        if (!container) return;
        
        // Clear welcome message on first message
        if (container.children.length === 1 && container.children[0].textContent.includes('Start a conversation')) {
            container.innerHTML = '';
        }
        
        const messageDiv = document.createElement('div');
        messageDiv.style.cssText = `
            margin-bottom: 16px;
            padding: 14px 16px;
            background: ${role === 'user' ? 'var(--cursor-bg-tertiary)' : 'rgba(119, 221, 190, 0.05)'};
            border-left: 4px solid ${role === 'user' ? 'var(--cursor-accent)' : 'var(--cursor-jade-dark)'};
            border-radius: 8px;
            font-size: 13px;
            line-height: 1.6;
            color: ${isError ? 'var(--red)' : 'var(--cursor-text)'};
            animation: slideInUp 0.3s ease-out;
        `;
        
        messageDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 8px;">
                <span style="font-size: 16px;">${role === 'user' ? '👤' : '🤖'}</span>
                <span style="font-weight: 600; color: ${role === 'user' ? 'var(--cursor-accent)' : 'var(--cursor-jade-dark)'};">
                    ${role === 'user' ? 'You' : 'BigDaddyG'}
                </span>
                <span style="font-size: 10px; color: var(--cursor-text-secondary);">
                    ${new Date().toLocaleTimeString()}
                </span>
            </div>
            <div style="white-space: pre-wrap; word-wrap: break-word;">
                ${this.escapeHtml(content)}
            </div>
        `;
        
        container.appendChild(messageDiv);
        
        // CRITICAL: Use triple-retry scroll to show full content
        this.scrollToBottom();
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    // Force scroll to bottom of chat (with retry for slow renders)
    scrollToBottom(immediate = false) {
        const container = document.getElementById('floating-chat-messages');
        if (!container) return;
        
        const doScroll = () => {
            container.scrollTop = container.scrollHeight;
            console.log(`[FloatingChat] 📜 Scrolled to: ${container.scrollTop} / ${container.scrollHeight}`);
        };
        
        if (immediate) {
            doScroll();
        } else {
            // Aggressive multi-retry to handle slow content rendering
            doScroll(); // Immediate
            setTimeout(doScroll, 10);   // Fast retry
            setTimeout(doScroll, 50);   // Medium retry
            setTimeout(doScroll, 150);  // Slow retry
            setTimeout(doScroll, 300);  // Final retry
            setTimeout(doScroll, 500);  // Extra insurance
        }
    }
    
    // ========================================================================
    // DEEP RESEARCH & TUNING METHODS
    // ========================================================================
    
    toggleDeepResearch() {
        try {
            this.deepResearchEnabled = !this.deepResearchEnabled;
            const btn = document.getElementById('toggle-research-btn');
            
            if (btn) {
                if (this.deepResearchEnabled) {
                    btn.innerHTML = '🔬 Deep Research: ON';
                    btn.style.background = 'linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent))';
                    btn.style.color = 'white';
                } else {
                    btn.innerHTML = '🔬 Deep Research: OFF';
                    btn.style.background = 'rgba(119, 221, 190, 0.1)';
                    btn.style.color = 'var(--cursor-jade-dark)';
                }
            }
            
            console.log(`[FloatingChat] 🔬 Deep Research: ${this.deepResearchEnabled ? 'ON' : 'OFF'}`);
        } catch (error) {
            console.error('[FloatingChat] ❌ Error toggling deep research:', error);
            alert(`Error: ${error.message}`);
        }
    }
    
    toggleSettings() {
        try {
            const panel = document.getElementById('floating-settings-panel');
            if (!panel) {
                console.error('[FloatingChat] ❌ Settings panel not found');
                return;
            }
            
            if (panel.style.display === 'none') {
                panel.style.display = 'block';
                this.loadContextInfo();
                console.log('[FloatingChat] ⚙️ Settings panel opened');
            } else {
                panel.style.display = 'none';
                console.log('[FloatingChat] ⚙️ Settings panel closed');
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error toggling settings:', error);
            alert(`Error: ${error.message}`);
        }
    }
    
    toggleSection(sectionId) {
        try {
            const section = document.getElementById(sectionId);
            const toggle = document.getElementById(`${sectionId}-toggle`);
            
            if (!section || !toggle) {
                console.error(`[FloatingChat] ❌ Section not found: ${sectionId}`);
                return;
            }
            
            if (section.style.display === 'none') {
                section.style.display = 'block';
                toggle.textContent = '▼';
                console.log(`[FloatingChat] 📖 Expanded: ${sectionId}`);
            } else {
                section.style.display = 'none';
                toggle.textContent = '▶';
                console.log(`[FloatingChat] 📖 Collapsed: ${sectionId}`);
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error toggling section:', error);
        }
    }
    
    updateParamDisplay(param, value) {
        try {
            const display = document.getElementById(`param-${param}-val`);
            if (display) {
                display.textContent = value;
                console.log(`[FloatingChat] 🎛️ ${param} = ${value}`);
            } else {
                console.warn(`[FloatingChat] ⚠️ Display not found for param: ${param}`);
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error updating param display:', error);
        }
    }
    
    getParameters() {
        try {
            const thinkingTime = parseInt(document.getElementById('param-thinking_time')?.value || 30);
            
            const params = {
                temperature: parseFloat(document.getElementById('param-temperature')?.value || 0.7),
                top_p: parseFloat(document.getElementById('param-top_p')?.value || 0.9),
                top_k: parseInt(document.getElementById('param-top_k')?.value || 40),
                repeat_penalty: parseFloat(document.getElementById('param-repeat_penalty')?.value || 1.1),
                max_tokens: parseInt(document.getElementById('param-max_tokens')?.value || 4000),
                response_style: document.getElementById('param-response_style')?.value || 'detailed',
                code_quality: document.getElementById('param-code_quality')?.value || 'production',
                explanation_level: document.getElementById('param-explanation_level')?.value || 'intermediate',
                thinking_time: thinkingTime,
                timeout: thinkingTime * 1000, // Convert to milliseconds
                timeout_strategy: document.getElementById('param-timeout_strategy')?.value || 'graceful'
            };
            
            console.log('[FloatingChat] 📊 Current parameters:', params);
            return params;
        } catch (error) {
            console.error('[FloatingChat] ❌ Error getting parameters:', error);
            return {}; // Return defaults on error
        }
    }
    
    updateThinkingTimeDisplay(seconds) {
        try {
            const display = document.getElementById('param-thinking_time-val');
            if (display) {
                if (seconds < 60) {
                    display.textContent = `${seconds}s`;
                } else {
                    const minutes = Math.floor(seconds / 60);
                    const secs = seconds % 60;
                    display.textContent = secs > 0 ? `${minutes}m ${secs}s` : `${minutes}m`;
                }
                console.log(`[FloatingChat] ⏱️ Thinking time: ${seconds}s`);
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error updating thinking time:', error);
        }
    }
    
    setThinkingTime(seconds) {
        try {
            const slider = document.getElementById('param-thinking_time');
            if (slider) {
                slider.value = seconds;
                this.updateThinkingTimeDisplay(seconds);
                console.log(`[FloatingChat] ⏱️ Set thinking time to ${seconds}s`);
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error setting thinking time:', error);
        }
    }
    
    applyModelPreset(preset) {
        try {
            console.log(`[FloatingChat] 🧠 Applying ${preset} model preset...`);
            
            const presets = {
                '7b': {
                    thinking_time: 30,
                    temperature: 0.7,
                    top_p: 0.9,
                    top_k: 40,
                    max_tokens: 2048,
                    timeout_strategy: 'strict'
                },
                '13b': {
                    thinking_time: 60,
                    temperature: 0.75,
                    top_p: 0.92,
                    top_k: 50,
                    max_tokens: 4096,
                    timeout_strategy: 'graceful'
                },
                '34b': {
                    thinking_time: 120,
                    temperature: 0.8,
                    top_p: 0.95,
                    top_k: 60,
                    max_tokens: 8192,
                    timeout_strategy: 'graceful'
                },
                '40gb': {
                    thinking_time: 180,
                    temperature: 0.85,
                    top_p: 0.95,
                    top_k: 80,
                    max_tokens: 16384,
                    timeout_strategy: 'wait'
                },
                '70b': {
                    thinking_time: 300,
                    temperature: 0.9,
                    top_p: 0.98,
                    top_k: 100,
                    max_tokens: 32768,
                    timeout_strategy: 'wait'
                }
            };
            
            if (preset === 'custom') return;
            
            const config = presets[preset];
            if (!config) {
                console.warn(`[FloatingChat] ⚠️ Unknown preset: ${preset}`);
                return;
            }
            
            // Apply preset values
            document.getElementById('param-thinking_time').value = config.thinking_time;
            this.updateThinkingTimeDisplay(config.thinking_time);
            
            document.getElementById('param-temperature').value = config.temperature;
            document.getElementById('param-temperature-val').textContent = config.temperature;
            
            document.getElementById('param-top_p').value = config.top_p;
            document.getElementById('param-top_p-val').textContent = config.top_p;
            
            document.getElementById('param-top_k').value = config.top_k;
            document.getElementById('param-top_k-val').textContent = config.top_k;
            
            document.getElementById('param-max_tokens').value = config.max_tokens;
            document.getElementById('param-timeout_strategy').value = config.timeout_strategy;
            
            console.log(`[FloatingChat] ✅ Applied ${preset} preset:`, config);
            
            // Show notification
            const notification = document.createElement('div');
            notification.style.cssText = `
                position: fixed;
                top: 20px;
                right: 20px;
                background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                color: white;
                padding: 12px 20px;
                border-radius: 8px;
                box-shadow: 0 4px 12px rgba(0,0,0,0.3);
                z-index: 20000;
                font-size: 12px;
                font-weight: 600;
                animation: slideInUp 0.3s ease-out;
            `;
            notification.textContent = `✅ ${preset.toUpperCase()} preset applied! Thinking time: ${config.thinking_time}s`;
            document.body.appendChild(notification);
            
            setTimeout(() => notification.remove(), 3000);
            
        } catch (error) {
            console.error('[FloatingChat] ❌ Error applying model preset:', error);
            alert(`Error applying preset: ${error.message}`);
        }
    }
    
    async applyParameters() {
        try {
            const params = this.getParameters();
            console.log('[FloatingChat] 🔧 Applying parameters to Orchestra...', params);
            
            const response = await fetch('http://localhost:11441/api/parameters/set', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(params)
            });
            
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}`);
            }
            
            const data = await response.json();
            
            if (data.success) {
                alert('✅ Parameters applied successfully!\n\n' + JSON.stringify(params, null, 2));
                console.log('[FloatingChat] ✅ Parameters updated successfully');
            } else {
                throw new Error(data.error || 'Unknown error');
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error applying parameters:', error);
            alert(`❌ Failed to apply parameters:\n\n${error.message}\n\nCheck if Orchestra server is running.`);
        }
    }
    
    async resetParameters() {
        try {
            if (!confirm('Reset all AI parameters to defaults?')) {
                return;
            }
            
            console.log('[FloatingChat] 🔄 Resetting parameters...');
            
            const response = await fetch('http://localhost:11441/api/parameters/reset', {
                method: 'POST'
            });
            
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}`);
            }
            
            const data = await response.json();
            
            if (data.success) {
                // Update UI with defaults
                document.getElementById('param-temperature').value = 0.7;
                document.getElementById('param-top_p').value = 0.9;
                document.getElementById('param-top_k').value = 40;
                document.getElementById('param-repeat_penalty').value = 1.1;
                document.getElementById('param-max_tokens').value = 4000;
                document.getElementById('param-response_style').value = 'detailed';
                document.getElementById('param-code_quality').value = 'production';
                document.getElementById('param-explanation_level').value = 'intermediate';
                
                // Update displays
                document.getElementById('param-temperature-val').textContent = '0.7';
                document.getElementById('param-top_p-val').textContent = '0.9';
                document.getElementById('param-top_k-val').textContent = '40';
                document.getElementById('param-repeat_penalty-val').textContent = '1.1';
                
                alert('✅ Parameters reset to defaults!');
                console.log('[FloatingChat] ✅ Parameters reset successfully');
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error resetting parameters:', error);
            alert(`❌ Failed to reset parameters:\n\n${error.message}`);
        }
    }
    
    async clearContext() {
        try {
            if (!confirm('Clear BigDaddyG\'s 1M token conversation history?\n\nThis will free up memory but remove all context from previous messages.')) {
                return;
            }
            
            console.log('[FloatingChat] 🗑️ Clearing context...');
            
            const response = await fetch('http://localhost:11441/api/context/clear', {
                method: 'POST'
            });
            
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}`);
            }
            
            const data = await response.json();
            
            if (data.success) {
                alert(`✅ Context cleared!\n\nFreed: ${data.tokensFreed || 'Unknown'} tokens\nContext window reset to 1M tokens`);
                this.loadContextInfo();
                console.log('[FloatingChat] ✅ Context cleared, tokens freed:', data.tokensFreed);
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Error clearing context:', error);
            alert(`❌ Failed to clear context:\n\n${error.message}\n\nCheck if Orchestra server is running.`);
        }
    }
    
    async loadContextInfo() {
        try {
            console.log('[FloatingChat] 📊 Loading context info...');
            
            let contextData = null;
            
            // Try Orchestra server first
            try {
                const response = await fetch('http://localhost:11441/api/context');
                if (response.ok) {
                    contextData = await response.json();
                }
            } catch (e) {
                console.log('[FloatingChat] Orchestra not available, using local context manager');
            }
            
            // Fallback to local context manager
            if (!contextData && window.contextManager) {
                const status = window.contextManager.getStatus();
                contextData = {
                    tokens: status.current,
                    maxTokens: status.max,
                    contextWindow: '1M tokens',
                    messageCount: 0
                };
            }
            
            const infoDiv = document.getElementById('context-info');
            
            if (infoDiv && contextData) {
                const tokenPercent = contextData.maxTokens ? Math.round((contextData.tokens / contextData.maxTokens) * 100) : 0;
                
                infoDiv.innerHTML = `
                    💎 Context Window: ${contextData.contextWindow || '1M tokens'} | 
                    🧠 Memory: ${contextData.tokens || 0} / ${contextData.maxTokens || '1,000,000'} tokens (${tokenPercent}%) | 
                    💬 Messages: ${contextData.messageCount || 0}
                    <div style="margin-top: 4px; font-family: monospace; font-size: 9px;">
                        <span id="context-usage-bar" style="display: inline-block; width: 100px; height: 8px; background: rgba(0,0,0,0.2); border-radius: 4px; overflow: hidden;">
                            <span id="context-usage-fill" style="display: block; height: 100%; background: linear-gradient(90deg, var(--cursor-jade-dark), var(--cursor-accent)); width: ${tokenPercent}%; transition: width 0.3s;"></span>
                        </span>
                        <span id="context-percentage" style="margin-left: 8px;">${tokenPercent}%</span>
                    </div>
                `;
                
                console.log('[FloatingChat] ✅ Context info loaded:', contextData);
            } else if (infoDiv) {
                infoDiv.innerHTML = `
                    💎 Context Window: 1M tokens | 🧠 Memory: Ready
                    <div style="margin-top: 4px; font-family: monospace; font-size: 9px;">
                        <span style="color: var(--cursor-jade-dark);">Context manager initialized</span>
                    </div>
                `;
            }
        } catch (error) {
            console.error('[FloatingChat] ❌ Failed to load context info:', error);
            const infoDiv = document.getElementById('context-info');
            if (infoDiv) {
                infoDiv.innerHTML = `❌ Could not load context info - ${error.message}`;
            }
        }
    }
    
    formatThinking(thinking) {
        try {
            if (Array.isArray(thinking)) {
                return thinking.map((step, idx) => `
                    <div style="margin-bottom: 6px; padding: 6px; background: rgba(119, 221, 190, 0.05); border-left: 2px solid var(--cursor-jade-light); border-radius: 4px;">
                        <strong style="color: var(--cursor-jade-dark);">Step ${idx + 1}:</strong> ${this.escapeHtml(step)}
                    </div>
                `).join('');
            } else if (typeof thinking === 'string') {
                return thinking.split('\n').map(line => 
                    `<div style="margin-bottom: 4px; color: var(--cursor-text-secondary);">→ ${this.escapeHtml(line)}</div>`
                ).join('');
            } else if (typeof thinking === 'object') {
                return `<pre style="color: var(--cursor-text-secondary); font-size: 10px; overflow-x: auto;">${JSON.stringify(thinking, null, 2)}</pre>`;
            }
            return '<div style="color: var(--cursor-text-muted);">No thinking data available</div>';
        } catch (error) {
            console.error('[FloatingChat] ❌ Error formatting thinking:', error);
            return `<div style="color: #ff4757;">Error formatting thinking: ${error.message}</div>`;
        }
    }
    
    updateContextDisplay() {
        if (!window.contextManager) return;
        
        const status = window.contextManager.getStatus();
        const usageFill = document.getElementById('context-usage-fill');
        const usagePercent = document.getElementById('context-percentage');
        
        if (usageFill && usagePercent) {
            const percent = parseFloat(status.usage);
            usageFill.style.width = `${percent}%`;
            usagePercent.textContent = status.usage;
            
            // Change color based on usage
            if (percent > 90) {
                usageFill.style.background = 'linear-gradient(90deg, #ff4757, #ff6b7a)';
            } else if (percent > 70) {
                usageFill.style.background = 'linear-gradient(90deg, #ffa502, #ff6348)';
            } else {
                usageFill.style.background = 'linear-gradient(90deg, var(--cursor-jade-dark), var(--cursor-accent))';
            }
        }
    }
    
    formatResponse(text) {
        try {
            if (!text) return '<div style="color: var(--cursor-text-muted);">No response</div>';
            
            // Simple markdown-like formatting
            let formatted = this.escapeHtml(text);
            
             // Code blocks
            formatted = formatted.replace(/`([^`]+)`/g, '<code style="background: rgba(119, 221, 190, 0.15); padding: 2px 6px; border-radius: 3px; font-family: \'Courier New\', monospace; color: var(--cursor-jade-dark); font-size: 12px; user-select: text; cursor: text;">$1</code>');
            
            // Bold
            formatted = formatted.replace(/\*\*([^*]+)\*\*/g, '<strong style="color: var(--cursor-jade-dark); font-weight: 700; user-select: text;">$1</strong>');
            
            // Italic
            formatted = formatted.replace(/\*([^*]+)\*/g, '<em style="color: var(--cursor-accent); user-select: text;">$1</em>');
            
            // Line breaks
            formatted = formatted.replace(/\n/g, '<br>');
            
            return formatted;
        } catch (error) {
            console.error('[FloatingChat] ❌ Error formatting response:', error);
            return this.escapeHtml(text);
        }
    }
}

// Add CSS animations
const style = document.createElement('style');
style.textContent = `
    @keyframes slideInUp {
        from {
            opacity: 0;
            transform: translateY(10px);
        }
        to {
            opacity: 1;
            transform: translateY(0);
        }
    }
    
    @keyframes blink {
        0%, 100% { opacity: 1; }
        50% { opacity: 0; }
    }
    
    @keyframes pulse {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.5; }
    }
`;
document.head.appendChild(style);

// Initialize immediately (don't wait for DOMContentLoaded)
window.floatingChat = new FloatingChat();

console.log('[FloatingChat] ✅ FloatingChat instance created and attached to window.floatingChat');

// Export
window.FloatingChat = FloatingChat;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FloatingChat;
}

console.log('[FloatingChat] 📦 Floating chat module loaded');

})(); // End IIFE


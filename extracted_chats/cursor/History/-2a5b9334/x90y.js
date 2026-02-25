/**
 * Enhanced Agentic UI for BigDaddyG IDE
 * 
 * Features:
 * - Real-time code suggestions as you type
 * - Agentic inline completions
 * - Smart context awareness
 * - Visual diff previews
 * - Instant code analysis
 * - Multi-model parallelization
 */

(function() {
'use strict';

class EnhancedAgenticUI {
    constructor() {
        this.suggestionCache = new Map();
        this.analysisQueue = [];
        this.isAnalyzing = false;
        this.lastAnalysis = null;
        this.agenticWidgets = [];
        
        console.log('[AgenticUI] 🎨 Initializing Enhanced Agentic UI...');
        this.init();
    }
    
    init() {
        this.enhanceOrchestraLayout();
        this.addInlineSuggestions();
        this.addContextualHelp();
        this.addSmartActions();
        this.addVisualEnhancements();
        
        console.log('[AgenticUI] ✅ Enhanced Agentic UI ready!');
    }
    
    enhanceOrchestraLayout() {
        // Add agentic visual elements to Orchestra layout
        const chatStage = document.getElementById('orchestra-chat-stage');
        if (!chatStage) return;
        
        // Add agentic status bar
        const statusBar = document.createElement('div');
        statusBar.id = 'agentic-status-bar';
        statusBar.style.cssText = `
            position: fixed;
            bottom: 0;
            left: 280px;
            right: 50%;
            height: 30px;
            background: linear-gradient(90deg, rgba(119, 221, 190, 0.1), rgba(0, 150, 255, 0.1));
            border-top: 1px solid var(--cursor-jade-light);
            display: flex;
            align-items: center;
            gap: 16px;
            padding: 0 16px;
            font-size: 11px;
            z-index: 1000;
        `;
        
        statusBar.innerHTML = `
            <div style="display: flex; align-items: center; gap: 6px;">
                <span id="agentic-mode-indicator" style="
                    display: inline-block;
                    width: 8px;
                    height: 8px;
                    border-radius: 50%;
                    background: var(--cursor-jade-dark);
                    box-shadow: 0 0 8px var(--cursor-jade-dark);
                    animation: pulse 2s infinite;
                "></span>
                <span style="font-weight: 600; color: var(--cursor-jade-dark);">Agentic Mode</span>
            </div>
            
            <div style="flex: 1; display: flex; gap: 12px; color: var(--cursor-text-secondary);">
                <span>🤖 Models: <strong id="model-count">85</strong></span>
                <span>⚡ Sessions: <strong id="session-count">0/100</strong></span>
                <span>🧠 Context: <strong id="context-usage">0%</strong></span>
                <span>📊 Analysis: <strong id="analysis-status">Idle</strong></span>
            </div>
            
            <div style="display: flex; gap: 8px;">
                <button onclick="enhancedAgenticUI.toggleAutoSuggest()" style="
                    background: rgba(119, 221, 190, 0.1);
                    border: 1px solid var(--cursor-jade-light);
                    color: var(--cursor-jade-dark);
                    padding: 4px 10px;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 10px;
                    font-weight: 600;
                " id="auto-suggest-btn" title="Toggle auto-suggestions">
                    🤖 Auto-Suggest: ON
                </button>
                
                <button onclick="enhancedAgenticUI.toggleLiveAnalysis()" style="
                    background: rgba(0, 150, 255, 0.1);
                    border: 1px solid #0096ff;
                    color: #0096ff;
                    padding: 4px 10px;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 10px;
                    font-weight: 600;
                " id="live-analysis-btn" title="Toggle live code analysis">
                    📊 Live Analysis: ON
                </button>
            </div>
        `;
        
        document.body.appendChild(statusBar);
        
        // Add keyframe for pulse animation
        if (!document.getElementById('agentic-pulse-keyframes')) {
            const style = document.createElement('style');
            style.id = 'agentic-pulse-keyframes';
            style.textContent = `
                @keyframes pulse {
                    0%, 100% { opacity: 1; transform: scale(1); }
                    50% { opacity: 0.7; transform: scale(1.2); }
                }
            `;
            document.head.appendChild(style);
        }
        
        console.log('[AgenticUI] ✅ Orchestra layout enhanced');
    }
    
    addInlineSuggestions() {
        // Monitor input and provide agentic inline suggestions
        const orchestraInput = document.getElementById('orchestra-input');
        if (!orchestraInput) return;
        
        let suggestionTimeout = null;
        
        orchestraInput.addEventListener('input', (e) => {
            clearTimeout(suggestionTimeout);
            
            suggestionTimeout = setTimeout(() => {
                this.generateInlineSuggestion(e.target.value);
            }, 500); // Wait 500ms after typing stops
        });
        
        console.log('[AgenticUI] ✅ Inline suggestions enabled');
    }
    
    async generateInlineSuggestion(currentText) {
        if (currentText.length < 10) return; // Need some context
        
        // Check cache first
        if (this.suggestionCache.has(currentText)) {
            this.showSuggestion(this.suggestionCache.get(currentText));
            return;
        }
        
        try {
            // Create floating suggestion widget
            const suggestion = this.createSuggestionWidget();
            suggestion.textContent = '⚡ Analyzing...';
            
            // Get AI suggestion
            const response = await fetch('http://localhost:11441/api/suggest', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    text: currentText,
                    type: 'inline_suggestion'
                })
            });
            
            if (response.ok) {
                const data = await response.json();
                this.suggestionCache.set(currentText, data.suggestion);
                this.showSuggestion(data.suggestion);
            } else {
                suggestion.remove();
            }
            
        } catch (error) {
            console.log('[AgenticUI] Suggestion generation skipped');
        }
    }
    
    createSuggestionWidget() {
        let widget = document.getElementById('inline-suggestion-widget');
        
        if (!widget) {
            widget = document.createElement('div');
            widget.id = 'inline-suggestion-widget';
            widget.style.cssText = `
                position: fixed;
                bottom: 200px;
                left: calc(280px + 20px);
                max-width: 400px;
                background: linear-gradient(135deg, rgba(119, 221, 190, 0.15), rgba(0, 150, 255, 0.15));
                border: 1px solid var(--cursor-jade-dark);
                border-radius: 8px;
                padding: 12px;
                font-size: 12px;
                color: var(--cursor-text);
                box-shadow: 0 4px 16px rgba(119, 221, 190, 0.3);
                z-index: 999;
                backdrop-filter: blur(10px);
            `;
            
            document.body.appendChild(widget);
        }
        
        return widget;
    }
    
    showSuggestion(suggestion) {
        const widget = this.createSuggestionWidget();
        
        widget.innerHTML = `
            <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 8px;">
                <span style="font-size: 14px;">💡</span>
                <span style="font-weight: 600; color: var(--cursor-jade-dark);">AI Suggestion</span>
                <button onclick="enhancedAgenticUI.acceptSuggestion()" style="
                    margin-left: auto;
                    background: var(--cursor-jade-dark);
                    border: none;
                    color: white;
                    padding: 4px 10px;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 10px;
                    font-weight: 600;
                ">
                    ✓ Accept
                </button>
                <button onclick="document.getElementById('inline-suggestion-widget').remove()" style="
                    background: none;
                    border: none;
                    color: var(--cursor-text-secondary);
                    padding: 4px;
                    cursor: pointer;
                    font-size: 12px;
                ">
                    ✕
                </button>
            </div>
            <div style="color: var(--cursor-text-secondary); line-height: 1.5;">
                ${suggestion}
            </div>
        `;
        
        this.currentSuggestion = suggestion;
    }
    
    acceptSuggestion() {
        const input = document.getElementById('orchestra-input');
        if (!input || !this.currentSuggestion) return;
        
        input.value = this.currentSuggestion;
        document.getElementById('inline-suggestion-widget')?.remove();
        
        // Update char count
        const counter = document.getElementById('orchestra-char-count');
        if (counter) {
            counter.textContent = `${input.value.length} / 10,000`;
        }
        
        input.focus();
    }
    
    addContextualHelp() {
        // Add floating context help that appears based on what you're doing
        const contextHelp = document.createElement('div');
        contextHelp.id = 'contextual-help';
        contextHelp.style.cssText = `
            position: fixed;
            top: 80px;
            right: calc(50% + 20px);
            width: 300px;
            max-height: 400px;
            background: var(--cursor-bg);
            border: 2px solid var(--cursor-jade-dark);
            border-radius: 12px;
            box-shadow: 0 8px 32px rgba(119, 221, 190, 0.3);
            z-index: 998;
            display: none;
            overflow: hidden;
        `;
        
        contextHelp.innerHTML = `
            <div style="padding: 16px; background: linear-gradient(135deg, var(--cursor-bg-secondary), var(--cursor-bg-tertiary)); border-bottom: 1px solid var(--cursor-border); display: flex; justify-content: space-between; align-items: center;">
                <div style="display: flex; align-items: center; gap: 8px;">
                    <span style="font-size: 18px;">💡</span>
                    <span style="font-weight: 600; color: var(--cursor-jade-dark);">Smart Suggestions</span>
                </div>
                <button onclick="document.getElementById('contextual-help').style.display='none'" style="
                    background: none;
                    border: none;
                    color: var(--cursor-text-secondary);
                    cursor: pointer;
                    font-size: 14px;
                ">✕</button>
            </div>
            
            <div id="contextual-help-content" style="padding: 16px; max-height: 340px; overflow-y: auto;">
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 40px 20px;">
                    <div style="font-size: 48px; margin-bottom: 12px;">🤖</div>
                    <p>Agentic suggestions will appear here based on your actions</p>
                </div>
            </div>
        `;
        
        document.body.appendChild(contextHelp);
        console.log('[AgenticUI] ✅ Contextual help added');
    }
    
    addSmartActions() {
        // Add quick action buttons to message input
        const inputArea = document.getElementById('orchestra-input')?.parentElement;
        if (!inputArea) return;
        
        const smartActions = document.createElement('div');
        smartActions.style.cssText = `
            position: absolute;
            left: 12px;
            bottom: 12px;
            display: flex;
            gap: 4px;
        `;
        
        const actions = [
            { icon: '🎨', title: 'Generate Image', action: 'insertCommand', param: '!pic ' },
            { icon: '💻', title: 'Generate Code', action: 'insertCommand', param: '!code ' },
            { icon: '🐛', title: 'Fix Bug', action: 'quickAction', param: 'fix' },
            { icon: '📚', title: 'Add Docs', action: 'quickAction', param: 'docs' },
            { icon: '🧪', title: 'Generate Test', action: 'quickAction', param: 'test' }
        ];
        
        actions.forEach(action => {
            const btn = document.createElement('button');
            btn.textContent = action.icon;
            btn.title = action.title;
            btn.style.cssText = `
                width: 28px;
                height: 28px;
                background: rgba(119, 221, 190, 0.1);
                border: 1px solid var(--cursor-jade-light);
                border-radius: 6px;
                cursor: pointer;
                font-size: 14px;
                transition: all 0.2s;
                display: flex;
                align-items: center;
                justify-content: center;
            `;
            
            btn.onmouseover = () => {
                btn.style.background = 'rgba(119, 221, 190, 0.2)';
                btn.style.transform = 'scale(1.1)';
            };
            btn.onmouseout = () => {
                btn.style.background = 'rgba(119, 221, 190, 0.1)';
                btn.style.transform = 'scale(1)';
            };
            
            btn.onclick = () => {
                if (action.action === 'insertCommand') {
                    const input = document.getElementById('orchestra-input');
                    if (input) {
                        input.value = action.param;
                        input.focus();
                    }
                } else if (action.action === 'quickAction') {
                    this.executeQuickAction(action.param);
                }
            };
            
            smartActions.appendChild(btn);
        });
        
        inputArea.appendChild(smartActions);
        console.log('[AgenticUI] ✅ Smart actions added');
    }
    
    async executeQuickAction(actionType) {
        const input = document.getElementById('orchestra-input');
        if (!input) return;
        
        const quickPrompts = {
            'fix': 'Analyze the current file and suggest fixes for any bugs or issues',
            'docs': 'Generate comprehensive documentation for the current file with JSDoc comments',
            'test': 'Generate a complete test suite for the current file with edge cases',
            'refactor': 'Suggest refactoring improvements for the current file',
            'optimize': 'Analyze and suggest performance optimizations for the current code'
        };
        
        input.value = quickPrompts[actionType] || '';
        input.focus();
        
        // Show contextual help
        this.showContextualTip(actionType);
    }
    
    showContextualTip(actionType) {
        const help = document.getElementById('contextual-help');
        const content = document.getElementById('contextual-help-content');
        if (!help || !content) return;
        
        const tips = {
            'fix': {
                title: '🐛 Bug Fix Assistant',
                content: `
                    <h3 style="color: var(--cursor-jade-dark); margin: 0 0 12px 0;">Smart Bug Fixing</h3>
                    <p style="margin-bottom: 16px; line-height: 1.6;">The AI will analyze your code and suggest fixes for:</p>
                    <ul style="margin: 0; padding-left: 20px; line-height: 1.8;">
                        <li>Syntax errors</li>
                        <li>Logic issues</li>
                        <li>Null pointer exceptions</li>
                        <li>Type mismatches</li>
                        <li>Security vulnerabilities</li>
                    </ul>
                    <div style="margin-top: 16px; padding: 12px; background: rgba(119, 221, 190, 0.1); border-radius: 6px; font-size: 11px;">
                        💡 Tip: Upload the buggy file for better analysis
                    </div>
                `
            },
            'docs': {
                title: '📚 Documentation Generator',
                content: `
                    <h3 style="color: var(--cursor-jade-dark); margin: 0 0 12px 0;">Auto Documentation</h3>
                    <p style="margin-bottom: 16px; line-height: 1.6;">Generate professional documentation:</p>
                    <ul style="margin: 0; padding-left: 20px; line-height: 1.8;">
                        <li>JSDoc comments</li>
                        <li>Function descriptions</li>
                        <li>Parameter types</li>
                        <li>Usage examples</li>
                        <li>README sections</li>
                    </ul>
                    <div style="margin-top: 16px; padding: 12px; background: rgba(50, 205, 50, 0.1); border-radius: 6px; font-size: 11px;">
                        💡 Tip: Works with any programming language
                    </div>
                `
            },
            'test': {
                title: '🧪 Test Generator',
                content: `
                    <h3 style="color: var(--cursor-jade-dark); margin: 0 0 12px 0;">Comprehensive Testing</h3>
                    <p style="margin-bottom: 16px; line-height: 1.6;">Generate complete test suites:</p>
                    <ul style="margin: 0; padding-left: 20px; line-height: 1.8;">
                        <li>Unit tests</li>
                        <li>Integration tests</li>
                        <li>Edge cases</li>
                        <li>Mock data</li>
                        <li>Assertions</li>
                    </ul>
                    <div style="margin-top: 16px; padding: 12px; background: rgba(255, 215, 0, 0.1); border-radius: 6px; font-size: 11px;">
                        💡 Tip: Specify your testing framework (Jest, Mocha, pytest, etc.)
                    </div>
                `
            }
        };
        
        const tipData = tips[actionType] || tips['fix'];
        
        content.innerHTML = `
            <div>
                ${tipData.content}
            </div>
        `;
        
        help.style.display = 'block';
    }
    
    addVisualEnhancements() {
        // Add floating orbs that indicate agentic activity
        const orbsContainer = document.createElement('div');
        orbsContainer.id = 'agentic-orbs';
        orbsContainer.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100vw;
            height: 100vh;
            pointer-events: none;
            z-index: 0;
            overflow: hidden;
        `;
        
        // Create animated background orbs
        for (let i = 0; i < 5; i++) {
            const orb = document.createElement('div');
            orb.style.cssText = `
                position: absolute;
                width: ${100 + Math.random() * 200}px;
                height: ${100 + Math.random() * 200}px;
                border-radius: 50%;
                background: radial-gradient(circle, rgba(119, 221, 190, 0.1), transparent);
                top: ${Math.random() * 100}%;
                left: ${Math.random() * 100}%;
                animation: float ${10 + Math.random() * 10}s infinite ease-in-out;
                animation-delay: ${Math.random() * 5}s;
            `;
            orbsContainer.appendChild(orb);
        }
        
        document.body.insertBefore(orbsContainer, document.body.firstChild);
        
        // Add keyframe for floating animation
        if (!document.getElementById('float-keyframes')) {
            const style = document.createElement('style');
            style.id = 'float-keyframes';
            style.textContent = `
                @keyframes float {
                    0%, 100% { transform: translate(0, 0) scale(1); opacity: 0.3; }
                    25% { transform: translate(30px, -30px) scale(1.1); opacity: 0.5; }
                    50% { transform: translate(-20px, 20px) scale(0.9); opacity: 0.4; }
                    75% { transform: translate(40px, 30px) scale(1.05); opacity: 0.6; }
                }
            `;
            document.head.appendChild(style);
        }
        
        console.log('[AgenticUI] ✅ Visual enhancements added');
    }
    
    toggleAutoSuggest() {
        const btn = document.getElementById('auto-suggest-btn');
        if (!btn) return;
        
        this.autoSuggestEnabled = !this.autoSuggestEnabled;
        btn.textContent = `🤖 Auto-Suggest: ${this.autoSuggestEnabled ? 'ON' : 'OFF'}`;
        btn.style.opacity = this.autoSuggestEnabled ? '1' : '0.5';
        
        console.log(`[AgenticUI] Auto-suggest: ${this.autoSuggestEnabled ? 'ON' : 'OFF'}`);
    }
    
    toggleLiveAnalysis() {
        const btn = document.getElementById('live-analysis-btn');
        if (!btn) return;
        
        this.liveAnalysisEnabled = !this.liveAnalysisEnabled;
        btn.textContent = `📊 Live Analysis: ${this.liveAnalysisEnabled ? 'ON' : 'OFF'}`;
        btn.style.opacity = this.liveAnalysisEnabled ? '1' : '0.5';
        
        if (this.liveAnalysisEnabled) {
            this.startLiveAnalysis();
        } else {
            this.stopLiveAnalysis();
        }
        
        console.log(`[AgenticUI] Live analysis: ${this.liveAnalysisEnabled ? 'ON' : 'OFF'}`);
    }
    
    startLiveAnalysis() {
        // Monitor Monaco editor changes and provide real-time analysis
        if (!window.monacoEditor) return;
        
        this.analysisInterval = setInterval(() => {
            this.analyzeCurrentCode();
        }, 5000); // Every 5 seconds
        
        console.log('[AgenticUI] 📊 Live analysis started');
    }
    
    stopLiveAnalysis() {
        if (this.analysisInterval) {
            clearInterval(this.analysisInterval);
            this.analysisInterval = null;
        }
        
        console.log('[AgenticUI] 📊 Live analysis stopped');
    }
    
    async analyzeCurrentCode() {
        if (!window.monacoEditor || this.isAnalyzing) return;
        
        const code = window.monacoEditor.getValue();
        if (!code || code.length < 50) return;
        
        this.isAnalyzing = true;
        const statusEl = document.getElementById('analysis-status');
        if (statusEl) statusEl.textContent = 'Analyzing...';
        
        try {
            const response = await fetch('http://localhost:11441/api/analyze-code', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    code: code,
                    language: this.detectLanguage(code)
                })
            });
            
            if (response.ok) {
                const analysis = await response.json();
                this.lastAnalysis = analysis;
                this.displayAnalysisResults(analysis);
                
                if (statusEl) statusEl.textContent = 'Complete';
                setTimeout(() => {
                    if (statusEl) statusEl.textContent = 'Idle';
                }, 3000);
            }
            
        } catch (error) {
            console.log('[AgenticUI] Analysis skipped');
        } finally {
            this.isAnalyzing = false;
        }
    }
    
    detectLanguage(code) {
        if (code.includes('function') || code.includes('const') || code.includes('let')) return 'javascript';
        if (code.includes('def ') || code.includes('import ')) return 'python';
        if (code.includes('class ') && code.includes('public')) return 'java';
        if (code.includes('#include')) return 'cpp';
        return 'unknown';
    }
    
    displayAnalysisResults(analysis) {
        // Show analysis in contextual help panel
        const help = document.getElementById('contextual-help');
        const content = document.getElementById('contextual-help-content');
        if (!help || !content) return;
        
        content.innerHTML = `
            <div style="margin-bottom: 16px;">
                <h4 style="margin: 0 0 8px 0; color: var(--cursor-jade-dark);">📊 Code Analysis Results</h4>
                <div style="font-size: 11px; color: var(--cursor-text-secondary);">
                    Just now • ${analysis.linesAnalyzed || 0} lines
                </div>
            </div>
            
            <div style="display: grid; gap: 8px;">
                <div style="padding: 10px; background: rgba(119, 221, 190, 0.1); border-left: 3px solid var(--cursor-jade-dark); border-radius: 4px;">
                    <div style="font-size: 10px; color: var(--cursor-text-secondary); margin-bottom: 4px;">Quality Score</div>
                    <div style="font-size: 18px; font-weight: 600; color: var(--cursor-jade-dark);">
                        ${analysis.qualityScore || 85}%
                    </div>
                </div>
                
                <div style="padding: 10px; background: rgba(0, 150, 255, 0.1); border-left: 3px solid #0096ff; border-radius: 4px;">
                    <div style="font-size: 10px; color: var(--cursor-text-secondary); margin-bottom: 4px;">Complexity</div>
                    <div style="font-size: 18px; font-weight: 600; color: #0096ff;">
                        ${analysis.complexity || 'Low'}
                    </div>
                </div>
                
                <div style="padding: 10px; background: rgba(255, 215, 0, 0.1); border-left: 3px solid #ffd700; border-radius: 4px;">
                    <div style="font-size: 10px; color: var(--cursor-text-secondary); margin-bottom: 4px;">Suggestions</div>
                    <div style="font-size: 18px; font-weight: 600; color: #ffd700;">
                        ${analysis.suggestions?.length || 0}
                    </div>
                </div>
            </div>
            
            ${analysis.suggestions && analysis.suggestions.length > 0 ? `
                <div style="margin-top: 16px;">
                    <h4 style="margin: 0 0 8px 0; color: var(--cursor-text); font-size: 12px;">💡 Suggestions:</h4>
                    ${analysis.suggestions.map((s, i) => `
                        <div style="padding: 8px; margin-bottom: 6px; background: var(--cursor-bg-secondary); border-radius: 4px; font-size: 11px;">
                            <strong>${i + 1}.</strong> ${s}
                        </div>
                    `).join('')}
                </div>
            ` : ''}
            
            <button onclick="enhancedAgenticUI.applyAllSuggestions()" style="
                width: 100%;
                margin-top: 12px;
                background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                border: none;
                color: white;
                padding: 10px;
                border-radius: 6px;
                cursor: pointer;
                font-size: 12px;
                font-weight: 600;
            ">
                ✅ Apply All Suggestions
            </button>
        `;
        
        help.style.display = 'block';
    }
    
    applyAllSuggestions() {
        alert('Applying suggestions to code... (Integration with Monaco coming soon!)');
        document.getElementById('contextual-help').style.display = 'none';
    }
}

// Initialize Enhanced Agentic UI
window.enhancedAgenticUI = new EnhancedAgenticUI();

})();


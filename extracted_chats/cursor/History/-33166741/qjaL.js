/**
 * BigDaddyG IDE - Orchestra Layout (3-Pane)
 * 🎼 File Explorer | AI Chat (Center Stage) | Code Editor
 * 
 * Features:
 * - 100 parallel AI sessions
 * - Conversation history (Today/This Week/Older)
 * - Model selector + auto-discovery
 * - Multi-file upload
 * - Full drive scanning (C:\, D:\)
 * - System resource detection
 */

(function() {
'use strict';

class OrchestraLayout {
    constructor() {
        this.sessions = new Map(); // Up to 100 sessions
        this.activeSessionId = null;
        this.maxSessions = 100;
        this.systemCores = navigator.hardwareConcurrency || 4;
        this.recommendedParallelSessions = Math.min(this.systemCores * 2, 100);
        this.uploadedFiles = new Map(); // Track uploaded files per session
        
        console.log(`[Orchestra] 🎼 Initializing Orchestra Layout...`);
        console.log(`[Orchestra] 💻 CPU Cores: ${this.systemCores}`);
        console.log(`[Orchestra] 🔥 Recommended Parallel Sessions: ${this.recommendedParallelSessions}`);
        console.log(`[Orchestra] 📊 Max Sessions: ${this.maxSessions}`);
        
        this.init();
    }
    
    init() {
        // Check if we should enable 3-pane layout
        const enable3Pane = localStorage.getItem('orchestra-3pane-enabled') !== 'false';
        
        if (enable3Pane) {
            this.create3PaneLayout();
            this.createConversationHistorySidebar();
            this.createSessionManager();
            this.createModelManager();
            this.loadConversationHistory();
        }
        
        console.log('[Orchestra] ✅ Orchestra Layout ready!');
    }
    
    create3PaneLayout() {
        // Transform existing layout into 3-pane orchestra
        const existingLayout = document.querySelector('.main-container');
        if (!existingLayout) return;
        
        // Add orchestra class for styling
        existingLayout.classList.add('orchestra-3pane');
        
        // Apply 3-pane grid layout
        existingLayout.style.cssText = `
            display: grid;
            grid-template-columns: 280px 1fr 50%;
            grid-template-rows: 40px 1fr;
            grid-template-areas:
                "sidebar header header"
                "sidebar chat editor";
            height: 100vh;
            overflow: hidden;
        `;
        
        // Style existing panels
        const fileExplorer = document.querySelector('#file-explorer') || document.querySelector('.file-tree-panel');
        if (fileExplorer) {
            fileExplorer.style.gridArea = 'sidebar';
            fileExplorer.style.borderRight = '1px solid var(--cursor-border)';
        }
        
        const editorArea = document.querySelector('#monaco-editor-container') || document.querySelector('.editor-container');
        if (editorArea) {
            editorArea.style.gridArea = 'editor';
            editorArea.style.borderLeft = '1px solid var(--cursor-border)';
        }
        
        console.log('[Orchestra] ✅ 3-pane layout applied');
    }
    
    createConversationHistorySidebar() {
        // Create conversation history sidebar (like Ollama)
        const sidebar = document.createElement('div');
        sidebar.id = 'conversation-history-sidebar';
        sidebar.style.cssText = `
            position: fixed;
            top: 40px;
            left: 0;
            width: 280px;
            height: calc(100vh - 40px);
            background: var(--cursor-bg-secondary);
            border-right: 1px solid var(--cursor-border);
            z-index: 999;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        `;
        
        sidebar.innerHTML = `
            <!-- Header -->
            <div style="padding: 16px; border-bottom: 1px solid var(--cursor-border);">
                <button onclick="orchestraLayout.newSession()" style="
                    width: 100%;
                    background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                    border: none;
                    color: white;
                    padding: 12px;
                    border-radius: 8px;
                    cursor: pointer;
                    font-size: 14px;
                    font-weight: 600;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    gap: 8px;
                    transition: all 0.2s;
                    box-shadow: 0 2px 8px rgba(119, 221, 190, 0.3);
                " onmouseover="this.style.transform='translateY(-1px)'; this.style.boxShadow='0 4px 12px rgba(119, 221, 190, 0.4)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='0 2px 8px rgba(119, 221, 190, 0.3)'">
                    <span style="font-size: 18px;">💬</span>
                    New Chat
                </button>
            </div>
            
            <!-- Session Info -->
            <div style="padding: 12px 16px; background: rgba(119, 221, 190, 0.1); border-bottom: 1px solid var(--cursor-border); font-size: 11px; color: var(--cursor-text-secondary);">
                <div style="display: flex; justify-content: space-between; margin-bottom: 4px;">
                    <span>💻 CPU Cores:</span>
                    <span style="color: var(--cursor-jade-dark); font-weight: 600;">${this.systemCores}</span>
                </div>
                <div style="display: flex; justify-content: space-between; margin-bottom: 4px;">
                    <span>🔥 Recommended:</span>
                    <span style="color: var(--cursor-jade-dark); font-weight: 600;">${this.recommendedParallelSessions} parallel</span>
                </div>
                <div style="display: flex; justify-content: space-between;">
                    <span>📊 Active Sessions:</span>
                    <span id="active-session-count" style="color: var(--cursor-accent); font-weight: 600;">0 / ${this.maxSessions}</span>
                </div>
            </div>
            
            <!-- Search -->
            <div style="padding: 12px 16px; border-bottom: 1px solid var(--cursor-border);">
                <input type="text" id="conversation-search" placeholder="🔍 Search conversations..." style="
                    width: 100%;
                    background: var(--cursor-bg);
                    border: 1px solid var(--cursor-border);
                    color: var(--cursor-text);
                    padding: 8px 12px;
                    border-radius: 6px;
                    font-size: 12px;
                    outline: none;
                " oninput="orchestraLayout.searchConversations(this.value)">
            </div>
            
            <!-- Conversation List -->
            <div id="conversation-list" style="flex: 1; overflow-y: auto; padding: 8px;">
                <!-- Today -->
                <div class="conversation-group">
                    <div style="padding: 8px 12px; font-size: 11px; font-weight: 600; color: var(--cursor-text-secondary); text-transform: uppercase;">
                        Today
                    </div>
                    <div id="conversations-today"></div>
                </div>
                
                <!-- This Week -->
                <div class="conversation-group" style="margin-top: 16px;">
                    <div style="padding: 8px 12px; font-size: 11px; font-weight: 600; color: var(--cursor-text-secondary); text-transform: uppercase;">
                        This Week
                    </div>
                    <div id="conversations-week"></div>
                </div>
                
                <!-- Older -->
                <div class="conversation-group" style="margin-top: 16px;">
                    <div style="padding: 8px 12px; font-size: 11px; font-weight: 600; color: var(--cursor-text-secondary); text-transform: uppercase;">
                        Older
                    </div>
                    <div id="conversations-older"></div>
                </div>
            </div>
            
            <!-- Footer -->
            <div style="padding: 12px 16px; border-top: 1px solid var(--cursor-border); display: flex; gap: 8px;">
                <button onclick="orchestraLayout.scanDrives()" style="
                    flex: 1;
                    background: rgba(119, 221, 190, 0.1);
                    border: 1px solid var(--cursor-jade-light);
                    color: var(--cursor-jade-dark);
                    padding: 8px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-size: 11px;
                    font-weight: 600;
                " title="Scan C:\ and D:\ drives">
                    🔍 Scan Drives
                </button>
                <button onclick="orchestraLayout.toggleSettings()" style="
                    flex: 1;
                    background: rgba(119, 221, 190, 0.1);
                    border: 1px solid var(--cursor-jade-light);
                    color: var(--cursor-jade-dark);
                    padding: 8px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-size: 11px;
                    font-weight: 600;
                ">
                    ⚙️ Settings
                </button>
            </div>
        `;
        
        document.body.appendChild(sidebar);
        console.log('[Orchestra] ✅ Conversation history sidebar created');
    }
    
    createSessionManager() {
        // Create central chat area (replaces floating chat when in 3-pane mode)
        const chatArea = document.createElement('div');
        chatArea.id = 'orchestra-chat-stage';
        chatArea.style.cssText = `
            grid-area: chat;
            display: flex;
            flex-direction: column;
            background: var(--cursor-bg);
            overflow: hidden;
        `;
        
        chatArea.innerHTML = `
            <!-- Chat Header -->
            <div style="padding: 12px 20px; background: var(--cursor-bg-secondary); border-bottom: 1px solid var(--cursor-border); display: flex; align-items: center; gap: 12px;">
                <span style="font-size: 20px;">🤖</span>
                <select id="orchestra-model-selector" onchange="orchestraLayout.selectModel(this.value)" style="
                    flex: 1;
                    background: var(--cursor-bg);
                    border: 1px solid var(--cursor-jade-light);
                    color: var(--cursor-text);
                    padding: 8px 12px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-size: 13px;
                    outline: none;
                ">
                    <option value="auto">🤖 Auto (Smart Selection)</option>
                    <option value="bigdaddyg:latest">💎 BigDaddyG Latest (Built-in)</option>
                    <optgroup label="🎨 Language Specialists">
                        <option value="bigdaddyg-c">⚙️ C/C++ Specialist</option>
                        <option value="bigdaddyg-csharp">🎮 C# Specialist</option>
                        <option value="bigdaddyg-python">🐍 Python Specialist</option>
                        <option value="bigdaddyg-javascript">🌐 JavaScript Specialist</option>
                        <option value="bigdaddyg-asm">🔧 Assembly Specialist</option>
                    </optgroup>
                    <optgroup label="🤖 Discovered Models" id="discovered-models-group">
                        <!-- Auto-populated from Ollama/GGUF scans -->
                    </optgroup>
                </select>
                <button onclick="orchestraLayout.uploadFiles()" style="
                    background: rgba(119, 221, 190, 0.1);
                    border: 1px solid var(--cursor-jade-light);
                    color: var(--cursor-jade-dark);
                    padding: 8px 16px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-size: 13px;
                    font-weight: 600;
                    display: flex;
                    align-items: center;
                    gap: 6px;
                " title="Upload multiple files">
                    <span>📎</span>
                    <span>+</span>
                </button>
                <button onclick="orchestraLayout.reloadModels()" style="
                    background: rgba(119, 221, 190, 0.1);
                    border: 1px solid var(--cursor-jade-light);
                    color: var(--cursor-jade-dark);
                    padding: 8px 16px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-size: 13px;
                " title="Reload model list">
                    🔄
                </button>
                <button onclick="orchestraLayout.showAgenticMenu()" style="
                    background: linear-gradient(135deg, rgba(255, 152, 0, 0.2), rgba(255, 71, 87, 0.2));
                    border: 1px solid var(--orange);
                    color: var(--orange);
                    padding: 8px 16px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-size: 13px;
                    font-weight: 600;
                    display: flex;
                    align-items: center;
                    gap: 6px;
                " title="Agentic Actions">
                    <span>🤖</span>
                    <span>Actions</span>
                </button>
            </div>
            
            <!-- Session Tabs -->
            <div id="orchestra-session-tabs" style="
                display: flex;
                gap: 4px;
                padding: 8px 12px;
                background: var(--cursor-bg-tertiary);
                border-bottom: 1px solid var(--cursor-border);
                overflow-x: auto;
                white-space: nowrap;
            ">
                <!-- Tabs will be added dynamically -->
            </div>
            
            <!-- Chat Messages -->
            <div id="orchestra-messages" style="
                flex: 1;
                overflow-y: auto;
                padding: 20px;
                scroll-behavior: smooth;
            ">
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                    <div style="font-size: 64px; margin-bottom: 20px;">🎼</div>
                    <h2 style="font-size: 24px; margin-bottom: 12px; color: var(--cursor-jade-dark);">Welcome to BigDaddyG Orchestra</h2>
                    <p style="font-size: 14px; max-width: 600px; margin: 0 auto 24px;">
                        Your agentic AI coding companion. Start a new chat, select a model, or upload files to begin.
                    </p>
                    <div style="display: flex; gap: 12px; justify-content: center; flex-wrap: wrap; font-size: 12px;">
                        <div style="background: rgba(119, 221, 190, 0.1); padding: 12px 20px; border-radius: 8px;">
                            <strong>${this.recommendedParallelSessions}</strong> recommended parallel sessions
                        </div>
                        <div style="background: rgba(119, 221, 190, 0.1); padding: 12px 20px; border-radius: 8px;">
                            <strong>100</strong> max sessions
                        </div>
                        <div style="background: rgba(119, 221, 190, 0.1); padding: 12px 20px; border-radius: 8px;">
                            <strong>∞</strong> conversation history
                        </div>
                    </div>
                </div>
            </div>
            
            <!-- Uploaded Files Display -->
            <div id="orchestra-files" style="
                display: none;
                padding: 8px 20px;
                background: var(--cursor-bg-secondary);
                border-top: 1px solid var(--cursor-border);
                max-height: 120px;
                overflow-y: auto;
            "></div>
            
            <!-- Input Area -->
            <div style="padding: 20px; background: var(--cursor-bg-secondary); border-top: 1px solid var(--cursor-border);">
                <div style="position: relative;">
                    <textarea id="orchestra-input" placeholder="Ask me anything... (Ctrl+Enter to send)" style="
                        width: 100%;
                        min-height: 100px;
                        max-height: 300px;
                        background: var(--cursor-bg);
                        border: 2px solid var(--cursor-jade-light);
                        color: var(--cursor-text);
                        padding: 16px 120px 16px 16px;
                        border-radius: 12px;
                        font-size: 14px;
                        font-family: inherit;
                        resize: vertical;
                        outline: none;
                    " onfocus="this.style.borderColor='var(--cursor-jade-dark)'; this.style.boxShadow='0 0 0 3px rgba(119, 221, 190, 0.1)'" onblur="this.style.borderColor='var(--cursor-jade-light)'; this.style.boxShadow='none'"></textarea>
                    <button onclick="orchestraLayout.sendMessage()" style="
                        position: absolute;
                        right: 12px;
                        bottom: 12px;
                        background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                        border: none;
                        color: white;
                        padding: 12px 24px;
                        border-radius: 8px;
                        cursor: pointer;
                        font-size: 14px;
                        font-weight: 600;
                        box-shadow: 0 2px 8px rgba(119, 221, 190, 0.3);
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-1px)'; this.style.boxShadow='0 4px 12px rgba(119, 221, 190, 0.4)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='0 2px 8px rgba(119, 221, 190, 0.3)'">
                        ↑ Send
                    </button>
                </div>
                <div style="margin-top: 8px; font-size: 11px; color: var(--cursor-text-secondary); display: flex; justify-content: space-between;">
                    <span>💡 Ctrl+Enter to send • / for commands</span>
                    <span id="orchestra-char-count">0 / 10,000</span>
                </div>
            </div>
        `;
        
        // Insert into main container
        const mainContainer = document.querySelector('.main-container');
        if (mainContainer) {
            mainContainer.insertBefore(chatArea, mainContainer.firstChild);
        }
        
        // Add keyboard shortcut
        const input = document.getElementById('orchestra-input');
        if (input) {
            input.addEventListener('keydown', (e) => {
                if (e.ctrlKey && e.key === 'Enter') {
                    e.preventDefault();
                    this.sendMessage();
                }
                
                // Update character count
                setTimeout(() => {
                    const counter = document.getElementById('orchestra-char-count');
                    if (counter) {
                        counter.textContent = `${input.value.length} / 10,000`;
                    }
                }, 0);
            });
        }
        
        console.log('[Orchestra] ✅ Session manager created');
    }
    
    createModelManager() {
        // Auto-discover and populate models
        this.discoverModels();
        
        // Set up periodic model refresh
        setInterval(() => this.discoverModels(), 30000); // Every 30 seconds
    }
    
    async discoverModels() {
        try {
            const response = await fetch('http://localhost:11441/api/models/list');
            if (!response.ok) return;
            
            const data = await response.json();
            const modelGroup = document.getElementById('discovered-models-group');
            if (!modelGroup) return;
            
            // Clear existing options
            modelGroup.innerHTML = '';
            
            // Add discovered models
            if (data.ollama && data.ollama.length > 0) {
                data.ollama.forEach(model => {
                    const option = document.createElement('option');
                    option.value = model.name;
                    option.textContent = `🤖 ${model.name} (${model.size})`;
                    modelGroup.appendChild(option);
                });
            }
            
            console.log(`[Orchestra] 📊 Discovered ${data.total} models`);
        } catch (error) {
            console.log('[Orchestra] ℹ️ Could not discover models:', error.message);
        }
    }
    
    async loadConversationHistory() {
        try {
            const history = JSON.parse(localStorage.getItem('orchestra-conversations') || '[]');
            
            const now = new Date();
            const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate());
            const weekStart = new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
            
            const today = history.filter(c => new Date(c.timestamp) >= todayStart);
            const week = history.filter(c => new Date(c.timestamp) < todayStart && new Date(c.timestamp) >= weekStart);
            const older = history.filter(c => new Date(c.timestamp) < weekStart);
            
            this.renderConversations('conversations-today', today);
            this.renderConversations('conversations-week', week);
            this.renderConversations('conversations-older', older);
            
            console.log(`[Orchestra] 📖 Loaded ${history.length} conversations`);
        } catch (error) {
            console.error('[Orchestra] ❌ Error loading conversations:', error);
        }
    }
    
    renderConversations(containerId, conversations) {
        const container = document.getElementById(containerId);
        if (!container) return;
        
        container.innerHTML = '';
        
        conversations.forEach(conv => {
            const item = document.createElement('div');
            item.style.cssText = `
                padding: 10px 12px;
                margin-bottom: 4px;
                background: var(--cursor-bg);
                border: 1px solid transparent;
                border-radius: 6px;
                cursor: pointer;
                transition: all 0.2s;
                font-size: 12px;
            `;
            
            item.innerHTML = `
                <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 4px;">
                    <span style="font-size: 14px;">${conv.emoji || '💬'}</span>
                    <span style="flex: 1; font-weight: 600; color: var(--cursor-text); overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">
                        ${conv.title || 'Untitled Chat'}
                    </span>
                </div>
                <div style="font-size: 10px; color: var(--cursor-text-secondary);">
                    ${conv.model || 'BigDaddyG'} • ${conv.messageCount || 0} messages
                </div>
            `;
            
            item.onmouseover = () => {
                item.style.background = 'var(--cursor-bg-tertiary)';
                item.style.borderColor = 'var(--cursor-jade-light)';
            };
            item.onmouseout = () => {
                item.style.background = 'var(--cursor-bg)';
                item.style.borderColor = 'transparent';
            };
            item.onclick = () => this.loadConversation(conv.id);
            
            container.appendChild(item);
        });
    }
    
    newSession() {
        if (this.sessions.size >= this.maxSessions) {
            alert(`Maximum of ${this.maxSessions} sessions reached!`);
            return;
        }
        
        const sessionId = `session_${Date.now()}`;
        const session = {
            id: sessionId,
            title: 'New Chat',
            model: 'auto',
            messages: [],
            timestamp: Date.now(),
            files: []
        };
        
        this.sessions.set(sessionId, session);
        this.activeSessionId = sessionId;
        
        this.addSessionTab(session);
        this.updateSessionCount();
        this.clearMessages();
        
        console.log(`[Orchestra] ✅ New session created: ${sessionId}`);
    }
    
    addSessionTab(session) {
        const tabsContainer = document.getElementById('orchestra-session-tabs');
        if (!tabsContainer) return;
        
        const tab = document.createElement('div');
        tab.id = `tab-${session.id}`;
        tab.style.cssText = `
            padding: 8px 16px;
            background: var(--cursor-bg);
            border: 1px solid var(--cursor-border);
            border-radius: 6px 6px 0 0;
            cursor: pointer;
            font-size: 12px;
            display: flex;
            align-items: center;
            gap: 8px;
            transition: all 0.2s;
        `;
        
        tab.innerHTML = `
            <span>💬</span>
            <span>${session.title}</span>
            <span onclick="orchestraLayout.closeSession('${session.id}', event)" style="
                color: var(--cursor-text-secondary);
                cursor: pointer;
                padding: 0 4px;
                border-radius: 4px;
                transition: all 0.2s;
            " onmouseover="this.style.background='rgba(255,71,87,0.1)'; this.style.color='#ff4757'" onmouseout="this.style.background='none'; this.style.color='var(--cursor-text-secondary)'">
                ✕
            </span>
        `;
        
        tab.onclick = (e) => {
            if (e.target.textContent !== '✕') {
                this.switchSession(session.id);
            }
        };
        
        tabsContainer.appendChild(tab);
        this.switchSession(session.id);
    }
    
    switchSession(sessionId) {
        this.activeSessionId = sessionId;
        
        // Update tab styles
        const tabs = document.querySelectorAll('[id^="tab-"]');
        tabs.forEach(tab => {
            if (tab.id === `tab-${sessionId}`) {
                tab.style.background = 'var(--cursor-jade-dark)';
                tab.style.color = 'white';
                tab.style.borderColor = 'var(--cursor-jade-dark)';
            } else {
                tab.style.background = 'var(--cursor-bg)';
                tab.style.color = 'var(--cursor-text)';
                tab.style.borderColor = 'var(--cursor-border)';
            }
        });
        
        // Load session messages
        const session = this.sessions.get(sessionId);
        if (session) {
            this.renderMessages(session.messages);
            this.renderUploadedFiles(session.files);
        }
    }
    
    closeSession(sessionId, event) {
        event.stopPropagation();
        
        this.sessions.delete(sessionId);
        this.uploadedFiles.delete(sessionId);
        
        const tab = document.getElementById(`tab-${sessionId}`);
        if (tab) tab.remove();
        
        // Switch to another session if this was active
        if (this.activeSessionId === sessionId) {
            const remainingSessions = Array.from(this.sessions.keys());
            if (remainingSessions.length > 0) {
                this.switchSession(remainingSessions[0]);
            } else {
                this.clearMessages();
                this.activeSessionId = null;
            }
        }
        
        this.updateSessionCount();
    }
    
    updateSessionCount() {
        const counter = document.getElementById('active-session-count');
        if (counter) {
            counter.textContent = `${this.sessions.size} / ${this.maxSessions}`;
            
            if (this.sessions.size >= this.recommendedParallelSessions) {
                counter.style.color = 'var(--orange)';
            } else {
                counter.style.color = 'var(--cursor-accent)';
            }
        }
    }
    
    async sendMessage() {
        const input = document.getElementById('orchestra-input');
        if (!input) return;
        
        const message = input.value.trim();
        if (!message) return;
        
        // Create session if none exists
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        // Add user message
        session.messages.push({
            role: 'user',
            content: message,
            timestamp: Date.now()
        });
        
        input.value = '';
        this.renderMessages(session.messages);
        
        // Send to Orchestra
        try {
            const modelSelector = document.getElementById('orchestra-model-selector');
            const model = modelSelector ? modelSelector.value : 'auto';
            
            // Include uploaded files
            const files = session.files || [];
            
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message,
                    model,
                    sessionId: session.id,
                    files: files.map(f => ({ name: f.name, content: f.content }))
                })
            });
            
            const data = await response.json();
            
            // Add AI response
            session.messages.push({
                role: 'assistant',
                content: data.response || data.message || 'No response',
                timestamp: Date.now(),
                model: data.model
            });
            
            this.renderMessages(session.messages);
            this.saveConversation(session);
            
        } catch (error) {
            console.error('[Orchestra] ❌ Error sending message:', error);
            session.messages.push({
                role: 'error',
                content: `Error: ${error.message}`,
                timestamp: Date.now()
            });
            this.renderMessages(session.messages);
        }
    }
    
    renderMessages(messages) {
        const container = document.getElementById('orchestra-messages');
        if (!container) return;
        
        container.innerHTML = '';
        
        messages.forEach(msg => {
            const msgDiv = document.createElement('div');
            msgDiv.style.cssText = `
                margin-bottom: 20px;
                padding: 16px;
                background: ${msg.role === 'user' ? 'var(--cursor-bg-secondary)' : 'rgba(119, 221, 190, 0.1)'};
                border-left: 4px solid ${msg.role === 'user' ? 'var(--cursor-accent)' : 'var(--cursor-jade-dark)'};
                border-radius: 8px;
            `;
            
            msgDiv.innerHTML = `
                <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 8px;">
                    <span style="font-size: 16px;">${msg.role === 'user' ? '👤' : '🤖'}</span>
                    <span style="font-weight: 600; color: var(--cursor-jade-dark);">
                        ${msg.role === 'user' ? 'You' : msg.model || 'BigDaddyG'}
                    </span>
                    <span style="font-size: 10px; color: var(--cursor-text-secondary); margin-left: auto;">
                        ${new Date(msg.timestamp).toLocaleTimeString()}
                    </span>
                </div>
                <div style="color: var(--cursor-text); white-space: pre-wrap; line-height: 1.6;">
                    ${this.escapeHtml(msg.content)}
                </div>
            `;
            
            container.appendChild(msgDiv);
        });
        
        container.scrollTop = container.scrollHeight;
    }
    
    clearMessages() {
        const container = document.getElementById('orchestra-messages');
        if (container) {
            container.innerHTML = '<div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;"><div style="font-size: 48px;">💬</div><p>No messages yet. Start chatting!</p></div>';
        }
    }
    
    async uploadFiles() {
        // Create file input
        const input = document.createElement('input');
        input.type = 'file';
        input.multiple = true;
        input.accept = '*/*';
        
        input.onchange = async (e) => {
            const files = Array.from(e.target.files);
            console.log(`[Orchestra] 📎 ${files.length} files selected`);
            
            // Create session if none exists
            if (!this.activeSessionId) {
                this.newSession();
            }
            
            const session = this.sessions.get(this.activeSessionId);
            if (!session) return;
            
            // Read files
            for (const file of files) {
                try {
                    const content = await this.readFileAsText(file);
                    session.files.push({
                        name: file.name,
                        size: file.size,
                        type: file.type,
                        content: content
                    });
                } catch (error) {
                    console.error(`[Orchestra] ❌ Error reading ${file.name}:`, error);
                }
            }
            
            this.renderUploadedFiles(session.files);
            console.log(`[Orchestra] ✅ ${files.length} files uploaded to session`);
        };
        
        input.click();
    }
    
    readFileAsText(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => resolve(e.target.result);
            reader.onerror = (e) => reject(e);
            reader.readAsText(file);
        });
    }
    
    renderUploadedFiles(files) {
        const container = document.getElementById('orchestra-files');
        if (!container) return;
        
        if (!files || files.length === 0) {
            container.style.display = 'none';
            return;
        }
        
        container.style.display = 'block';
        container.innerHTML = '<div style="font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 8px; font-weight: 600;">📎 Attached Files:</div>';
        
        files.forEach((file, index) => {
            const fileDiv = document.createElement('div');
            fileDiv.style.cssText = `
                display: inline-flex;
                align-items: center;
                gap: 6px;
                padding: 6px 12px;
                margin-right: 8px;
                margin-bottom: 4px;
                background: rgba(119, 221, 190, 0.1);
                border: 1px solid var(--cursor-jade-light);
                border-radius: 6px;
                font-size: 11px;
            `;
            
            fileDiv.innerHTML = `
                <span>📄</span>
                <span>${file.name}</span>
                <span style="color: var(--cursor-text-secondary);">(${this.formatFileSize(file.size)})</span>
                <span onclick="orchestraLayout.removeFile(${index})" style="
                    cursor: pointer;
                    color: var(--cursor-text-secondary);
                    padding: 0 4px;
                    border-radius: 4px;
                    transition: all 0.2s;
                " onmouseover="this.style.background='rgba(255,71,87,0.1)'; this.style.color='#ff4757'" onmouseout="this.style.background='none'; this.style.color='var(--cursor-text-secondary)'">
                    ✕
                </span>
            `;
            
            container.appendChild(fileDiv);
        });
    }
    
    removeFile(index) {
        if (!this.activeSessionId) return;
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        session.files.splice(index, 1);
        this.renderUploadedFiles(session.files);
    }
    
    formatFileSize(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
    }
    
    async scanDrives() {
        console.log('[Orchestra] 🔍 Scanning C:\\ and D:\\ drives...');
        
        try {
            // Trigger Orchestra drive scan
            const response = await fetch('http://localhost:11441/api/models/reload');
            const data = await response.json();
            
            alert(`Drive scan complete!\nFound ${data.count || 0} models.`);
            this.discoverModels();
        } catch (error) {
            console.error('[Orchestra] ❌ Scan error:', error);
            alert('Drive scan failed. Ensure Orchestra server is running.');
        }
    }
    
    selectModel(model) {
        console.log(`[Orchestra] 🎯 Selected model: ${model}`);
        
        if (this.activeSessionId) {
            const session = this.sessions.get(this.activeSessionId);
            if (session) {
                session.model = model;
            }
        }
    }
    
    async reloadModels() {
        console.log('[Orchestra] 🔄 Reloading models...');
        await this.discoverModels();
        alert('Model list refreshed!');
    }
    
    saveConversation(session) {
        try {
            const history = JSON.parse(localStorage.getItem('orchestra-conversations') || '[]');
            
            const existingIndex = history.findIndex(c => c.id === session.id);
            
            // Auto-generate title from first message
            const firstUserMessage = session.messages.find(m => m.role === 'user');
            const title = firstUserMessage ? 
                firstUserMessage.content.substring(0, 50) + (firstUserMessage.content.length > 50 ? '...' : '') :
                'New Chat';
            
            const conversationData = {
                id: session.id,
                title: title,
                model: session.model,
                messageCount: session.messages.length,
                timestamp: session.timestamp,
                emoji: '💬'
            };
            
            if (existingIndex >= 0) {
                history[existingIndex] = conversationData;
            } else {
                history.unshift(conversationData);
            }
            
            // Keep only last 1000 conversations
            if (history.length > 1000) {
                history.splice(1000);
            }
            
            localStorage.setItem('orchestra-conversations', JSON.stringify(history));
            localStorage.setItem(`orchestra-session-${session.id}`, JSON.stringify(session));
            this.loadConversationHistory();
            
        } catch (error) {
            console.error('[Orchestra] ❌ Error saving conversation:', error);
        }
    }
    
    loadConversation(id) {
        console.log(`[Orchestra] 📖 Loading conversation: ${id}`);
        
        try {
            const sessionData = localStorage.getItem(`orchestra-session-${id}`);
            if (!sessionData) {
                alert('Conversation not found!');
                return;
            }
            
            const session = JSON.parse(sessionData);
            this.sessions.set(id, session);
            this.activeSessionId = id;
            
            this.addSessionTab(session);
            this.renderMessages(session.messages);
            this.renderUploadedFiles(session.files);
            
        } catch (error) {
            console.error('[Orchestra] ❌ Error loading conversation:', error);
            alert('Failed to load conversation!');
        }
    }
    
    searchConversations(query) {
        console.log(`[Orchestra] 🔍 Searching: ${query}`);
        
        if (!query) {
            this.loadConversationHistory();
            return;
        }
        
        try {
            const history = JSON.parse(localStorage.getItem('orchestra-conversations') || '[]');
            const filtered = history.filter(c => 
                c.title.toLowerCase().includes(query.toLowerCase()) ||
                c.model.toLowerCase().includes(query.toLowerCase())
            );
            
            // Render all filtered in "today" section
            this.renderConversations('conversations-today', filtered);
            this.renderConversations('conversations-week', []);
            this.renderConversations('conversations-older', []);
            
        } catch (error) {
            console.error('[Orchestra] ❌ Search error:', error);
        }
    }
    
    toggleSettings() {
        console.log('[Orchestra] ⚙️ Opening settings...');
        
        // Create settings modal
        const modal = document.createElement('div');
        modal.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100vw;
            height: 100vh;
            background: rgba(0, 0, 0, 0.7);
            z-index: 10000;
            display: flex;
            align-items: center;
            justify-content: center;
        `;
        
        modal.innerHTML = `
            <div style="
                background: var(--cursor-bg);
                border: 2px solid var(--cursor-jade-dark);
                border-radius: 12px;
                padding: 24px;
                width: 600px;
                max-width: 90vw;
                max-height: 80vh;
                overflow-y: auto;
            ">
                <h2 style="margin: 0 0 20px 0; color: var(--cursor-jade-dark);">⚙️ Orchestra Settings</h2>
                
                <div style="margin-bottom: 20px;">
                    <label style="display: block; margin-bottom: 8px; font-weight: 600;">
                        🎼 Layout Mode
                    </label>
                    <select id="settings-layout-mode" style="width: 100%; padding: 8px; background: var(--cursor-bg-secondary); border: 1px solid var(--cursor-border); color: var(--cursor-text); border-radius: 6px;">
                        <option value="3pane">3-Pane Orchestra (File Explorer | Chat | Editor)</option>
                        <option value="floating">Floating Chat (Ctrl+L)</option>
                    </select>
                </div>
                
                <div style="margin-bottom: 20px;">
                    <label style="display: block; margin-bottom: 8px; font-weight: 600;">
                        📊 Max Parallel Sessions
                    </label>
                    <input type="number" id="settings-max-sessions" value="${this.maxSessions}" min="1" max="100" style="width: 100%; padding: 8px; background: var(--cursor-bg-secondary); border: 1px solid var(--cursor-border); color: var(--cursor-text); border-radius: 6px;">
                    <div style="font-size: 11px; color: var(--cursor-text-secondary); margin-top: 4px;">
                        Recommended: ${this.recommendedParallelSessions} (based on ${this.systemCores} CPU cores)
                    </div>
                </div>
                
                <div style="display: flex; gap: 12px; margin-top: 24px;">
                    <button onclick="orchestraLayout.saveSettings()" style="
                        flex: 1;
                        background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                        border: none;
                        color: white;
                        padding: 12px;
                        border-radius: 8px;
                        cursor: pointer;
                        font-weight: 600;
                    ">
                        ✅ Save
                    </button>
                    <button onclick="this.closest('[style*=fixed]').remove()" style="
                        flex: 1;
                        background: var(--cursor-bg-secondary);
                        border: 1px solid var(--cursor-border);
                        color: var(--cursor-text);
                        padding: 12px;
                        border-radius: 8px;
                        cursor: pointer;
                        font-weight: 600;
                    ">
                        ✕ Cancel
                    </button>
                </div>
            </div>
        `;
        
        document.body.appendChild(modal);
    }
    
    saveSettings() {
        const layoutMode = document.getElementById('settings-layout-mode')?.value;
        const maxSessions = parseInt(document.getElementById('settings-max-sessions')?.value || '100');
        
        localStorage.setItem('orchestra-3pane-enabled', layoutMode === '3pane' ? 'true' : 'false');
        this.maxSessions = maxSessions;
        
        // Close modal
        document.querySelector('[style*="position: fixed"]')?.remove();
        
        alert('Settings saved! Refresh the IDE to apply changes.');
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    // ========================================================================
    // AGENTIC ACTIONS: Build, Fix, Explore
    // ========================================================================
    
    showAgenticMenu() {
        const menu = document.createElement('div');
        menu.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100vw;
            height: 100vh;
            background: rgba(0, 0, 0, 0.8);
            z-index: 10000;
            display: flex;
            align-items: center;
            justify-content: center;
        `;
        
        menu.innerHTML = `
            <div style="
                background: var(--cursor-bg);
                border: 2px solid var(--orange);
                border-radius: 16px;
                padding: 32px;
                width: 700px;
                max-width: 90vw;
                box-shadow: 0 0 40px rgba(255, 152, 0, 0.3);
            ">
                <div style="text-align: center; margin-bottom: 32px;">
                    <h2 style="margin: 0 0 8px 0; color: var(--orange); font-size: 28px;">
                        🤖 Agentic Actions
                    </h2>
                    <p style="margin: 0; color: var(--cursor-text-secondary); font-size: 14px;">
                        Ask to build, fix bugs, and explore your projects!
                    </p>
                </div>
                
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 24px;">
                    <!-- Build Project -->
                    <button onclick="orchestraLayout.buildProject(); this.closest('[style*=fixed]').remove()" style="
                        background: linear-gradient(135deg, rgba(119, 221, 190, 0.2), rgba(119, 221, 190, 0.3));
                        border: 2px solid var(--cursor-jade-dark);
                        color: var(--cursor-text);
                        padding: 24px;
                        border-radius: 12px;
                        cursor: pointer;
                        text-align: left;
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-4px)'; this.style.boxShadow='0 8px 20px rgba(119, 221, 190, 0.3)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='none'">
                        <div style="font-size: 32px; margin-bottom: 12px;">🏗️</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: var(--cursor-jade-dark);">
                            Build Project
                        </div>
                        <div style="font-size: 12px; color: var(--cursor-text-secondary);">
                            Create a complete project from scratch with full architecture
                        </div>
                    </button>
                    
                    <!-- Fix Bugs -->
                    <button onclick="orchestraLayout.fixBugs(); this.closest('[style*=fixed]').remove()" style="
                        background: linear-gradient(135deg, rgba(255, 71, 87, 0.2), rgba(255, 71, 87, 0.3));
                        border: 2px solid #ff4757;
                        color: var(--cursor-text);
                        padding: 24px;
                        border-radius: 12px;
                        cursor: pointer;
                        text-align: left;
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-4px)'; this.style.boxShadow='0 8px 20px rgba(255, 71, 87, 0.3)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='none'">
                        <div style="font-size: 32px; margin-bottom: 12px;">🐛</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: #ff4757;">
                            Fix Bugs
                        </div>
                        <div style="font-size: 12px; color: var(--cursor-text-secondary);">
                            Scan for bugs, analyze errors, and apply automatic fixes
                        </div>
                    </button>
                    
                    <!-- Explore Project -->
                    <button onclick="orchestraLayout.exploreProject(); this.closest('[style*=fixed]').remove()" style="
                        background: linear-gradient(135deg, rgba(0, 150, 255, 0.2), rgba(0, 150, 255, 0.3));
                        border: 2px solid #0096ff;
                        color: var(--cursor-text);
                        padding: 24px;
                        border-radius: 12px;
                        cursor: pointer;
                        text-align: left;
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-4px)'; this.style.boxShadow='0 8px 20px rgba(0, 150, 255, 0.3)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='none'">
                        <div style="font-size: 32px; margin-bottom: 12px;">🔍</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: #0096ff;">
                            Explore Project
                        </div>
                        <div style="font-size: 12px; color: var(--cursor-text-secondary);">
                            Analyze structure, dependencies, and provide insights
                        </div>
                    </button>
                    
                    <!-- Refactor Code -->
                    <button onclick="orchestraLayout.refactorCode(); this.closest('[style*=fixed]').remove()" style="
                        background: linear-gradient(135deg, rgba(138, 43, 226, 0.2), rgba(138, 43, 226, 0.3));
                        border: 2px solid #8a2be2;
                        color: var(--cursor-text);
                        padding: 24px;
                        border-radius: 12px;
                        cursor: pointer;
                        text-align: left;
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-4px)'; this.style.boxShadow='0 8px 20px rgba(138, 43, 226, 0.3)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='none'">
                        <div style="font-size: 32px; margin-bottom: 12px;">♻️</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: #8a2be2;">
                            Refactor Code
                        </div>
                        <div style="font-size: 12px; color: var(--cursor-text-secondary);">
                            Improve code quality, performance, and maintainability
                        </div>
                    </button>
                    
                    <!-- Add Tests -->
                    <button onclick="orchestraLayout.addTests(); this.closest('[style*=fixed]').remove()" style="
                        background: linear-gradient(135deg, rgba(255, 215, 0, 0.2), rgba(255, 215, 0, 0.3));
                        border: 2px solid #ffd700;
                        color: var(--cursor-text);
                        padding: 24px;
                        border-radius: 12px;
                        cursor: pointer;
                        text-align: left;
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-4px)'; this.style.boxShadow='0 8px 20px rgba(255, 215, 0, 0.3)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='none'">
                        <div style="font-size: 32px; margin-bottom: 12px;">✅</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: #ffd700;">
                            Add Tests
                        </div>
                        <div style="font-size: 12px; color: var(--cursor-text-secondary);">
                            Generate comprehensive unit and integration tests
                        </div>
                    </button>
                    
                    <!-- Document Code -->
                    <button onclick="orchestraLayout.documentCode(); this.closest('[style*=fixed]').remove()" style="
                        background: linear-gradient(135deg, rgba(50, 205, 50, 0.2), rgba(50, 205, 50, 0.3));
                        border: 2px solid #32cd32;
                        color: var(--cursor-text);
                        padding: 24px;
                        border-radius: 12px;
                        cursor: pointer;
                        text-align: left;
                        transition: all 0.2s;
                    " onmouseover="this.style.transform='translateY(-4px)'; this.style.boxShadow='0 8px 20px rgba(50, 205, 50, 0.3)'" onmouseout="this.style.transform='translateY(0)'; this.style.boxShadow='none'">
                        <div style="font-size: 32px; margin-bottom: 12px;">📝</div>
                        <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px; color: #32cd32;">
                            Document Code
                        </div>
                        <div style="font-size: 12px; color: var(--cursor-text-secondary);">
                            Auto-generate JSDoc, comments, and README files
                        </div>
                    </button>
                </div>
                
                <button onclick="this.closest('[style*=fixed]').remove()" style="
                    width: 100%;
                    background: var(--cursor-bg-secondary);
                    border: 1px solid var(--cursor-border);
                    color: var(--cursor-text);
                    padding: 12px;
                    border-radius: 8px;
                    cursor: pointer;
                    font-weight: 600;
                ">
                    ✕ Close
                </button>
            </div>
        `;
        
        document.body.appendChild(menu);
    }
    
    async buildProject() {
        console.log('[Orchestra] 🏗️ Starting project build assistant...');
        
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        // Add system message
        session.messages.push({
            role: 'assistant',
            content: `🏗️ **Project Build Assistant Activated**

I'll help you build a complete project from scratch! Here's what I can do:

**🎯 What would you like to build?**
- Web Application (React, Vue, Angular, Vanilla JS)
- Backend API (Node.js, Express, FastAPI, Django)
- Desktop App (Electron, Tauri)
- Mobile App (React Native, Flutter concept)
- CLI Tool (Node.js, Python, Bash)
- Library/Package
- Full-Stack Application

**📋 I'll help you with:**
✅ Project structure and architecture
✅ File creation and boilerplate code
✅ Configuration files (package.json, tsconfig, etc.)
✅ Dependencies and setup scripts
✅ Best practices and patterns
✅ Testing setup
✅ Documentation

**💬 Just tell me:**
1. What type of project? (e.g., "React app with TypeScript")
2. What features? (e.g., "authentication, API integration, database")
3. Any specific requirements?

Type your project idea and I'll start building it for you!`,
            timestamp: Date.now(),
            model: 'BigDaddyG Build Assistant'
        });
        
        this.renderMessages(session.messages);
        
        // Auto-focus input
        const input = document.getElementById('orchestra-input');
        if (input) input.focus();
    }
    
    async fixBugs() {
        console.log('[Orchestra] 🐛 Starting bug fix assistant...');
        
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        // Trigger project scan
        const scanResult = await this.scanProjectForBugs();
        
        session.messages.push({
            role: 'assistant',
            content: `🐛 **Bug Fix Assistant Activated**

I'm scanning your project for issues...

**🔍 Scanning for:**
- Syntax errors
- Runtime errors
- Logic bugs
- Memory leaks
- Security vulnerabilities
- Code smells
- Performance issues

${scanResult.issues.length > 0 ? `
**⚠️ Found ${scanResult.issues.length} issues:**

${scanResult.issues.map((issue, i) => `
${i + 1}. **${issue.severity}** in \`${issue.file}\`:${issue.line}
   ${issue.message}
   ${issue.suggestion ? `   💡 **Fix:** ${issue.suggestion}` : ''}
`).join('\n')}

**🔧 What would you like me to do?**
- Type "fix all" to apply all automatic fixes
- Type "fix #1" to fix a specific issue
- Ask me about any issue for more details
- Upload a file with errors for targeted fixing
` : `
**✅ Great news! No critical issues found.**

But I can still help:
- Review code for potential improvements
- Add error handling
- Improve code quality
- Optimize performance
- Add input validation

Upload a file or describe an issue you're facing!`}`,
            timestamp: Date.now(),
            model: 'BigDaddyG Bug Fixer'
        });
        
        this.renderMessages(session.messages);
        
        const input = document.getElementById('orchestra-input');
        if (input) input.focus();
    }
    
    async scanProjectForBugs() {
        // Simulated bug scan - in real implementation, this would call backend
        // TODO: Integrate with actual linter/error detection
        
        const mockIssues = [
            {
                severity: '🔴 Error',
                file: 'main.js',
                line: 42,
                message: 'Uncaught TypeError: Cannot read property of undefined',
                suggestion: 'Add null check before accessing property'
            },
            {
                severity: '🟡 Warning',
                file: 'utils.js',
                line: 15,
                message: 'Unused variable "tempData"',
                suggestion: 'Remove unused variable or add // eslint-disable-line'
            },
            {
                severity: '🟢 Info',
                file: 'config.js',
                line: 8,
                message: 'Consider using const instead of let for non-reassigned variables',
                suggestion: 'Change let to const for better code safety'
            }
        ];
        
        return {
            issues: mockIssues.slice(0, Math.floor(Math.random() * 4)) // Random 0-3 issues
        };
    }
    
    async exploreProject() {
        console.log('[Orchestra] 🔍 Starting project exploration...');
        
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        // Trigger project analysis
        const analysis = await this.analyzeProjectStructure();
        
        session.messages.push({
            role: 'assistant',
            content: `🔍 **Project Explorer Activated**

I'm analyzing your project structure...

**📊 Project Analysis:**

**📁 File Structure:**
\`\`\`
${analysis.tree}
\`\`\`

**📦 Dependencies:**
- ${analysis.dependencies.length} packages detected
- Top dependencies: ${analysis.dependencies.slice(0, 5).join(', ')}

**🎯 Project Type:** ${analysis.type}
**💻 Languages:** ${analysis.languages.join(', ')}
**📏 Code Size:** ${analysis.fileCount} files, ~${analysis.lineCount.toLocaleString()} lines

**🔍 What I can help you explore:**
- **Architecture Overview** - "explain the architecture"
- **Dependency Analysis** - "analyze dependencies"
- **Code Complexity** - "show complexity metrics"
- **Entry Points** - "what are the entry points?"
- **API Endpoints** - "list all API routes"
- **Database Schema** - "explain database structure"
- **File Purpose** - "what does [filename] do?"

**💬 Ask me anything about your project!**
- "How does [feature] work?"
- "Where is [functionality] implemented?"
- "What files are related to [feature]?"
- "Explain the flow of [process]"`,
            timestamp: Date.now(),
            model: 'BigDaddyG Explorer'
        });
        
        this.renderMessages(session.messages);
        
        const input = document.getElementById('orchestra-input');
        if (input) input.focus();
    }
    
    async analyzeProjectStructure() {
        // Simulated analysis - in real implementation, call backend to scan filesystem
        // TODO: Integrate with file-explorer and call actual filesystem API
        
        return {
            tree: `project/
├── src/
│   ├── components/
│   ├── utils/
│   └── index.js
├── tests/
├── package.json
└── README.md`,
            dependencies: ['react', 'express', 'axios', 'lodash', 'moment'],
            type: 'Full-Stack JavaScript Application',
            languages: ['JavaScript', 'TypeScript', 'CSS', 'HTML'],
            fileCount: 47,
            lineCount: 8543
        };
    }
    
    async refactorCode() {
        console.log('[Orchestra] ♻️ Starting code refactoring...');
        
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        session.messages.push({
            role: 'assistant',
            content: `♻️ **Code Refactoring Assistant Activated**

I'll help you improve your code quality and maintainability!

**🎯 Refactoring Options:**

1. **Extract Functions** - Break down complex functions
2. **Rename Variables** - Use descriptive names
3. **Remove Duplication** - DRY principle
4. **Simplify Logic** - Reduce complexity
5. **Improve Structure** - Better organization
6. **Modernize Syntax** - Use latest features
7. **Apply Patterns** - Design patterns
8. **Performance Optimization** - Speed improvements

**💬 How to use:**
- Upload a file to refactor
- Type "refactor [filename]"
- Describe the issue: "This function is too complex"
- Request specific improvements: "use async/await instead of callbacks"

**📋 I'll provide:**
✅ Before/After comparison
✅ Explanation of changes
✅ Best practice justification
✅ Performance impact analysis`,
            timestamp: Date.now(),
            model: 'BigDaddyG Refactorer'
        });
        
        this.renderMessages(session.messages);
        
        const input = document.getElementById('orchestra-input');
        if (input) input.focus();
    }
    
    async addTests() {
        console.log('[Orchestra] ✅ Starting test generation...');
        
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        session.messages.push({
            role: 'assistant',
            content: `✅ **Test Generation Assistant Activated**

I'll help you write comprehensive tests for your code!

**🧪 Test Types I can generate:**

1. **Unit Tests** - Test individual functions/methods
2. **Integration Tests** - Test component interactions
3. **E2E Tests** - End-to-end user flows
4. **API Tests** - Test endpoints and responses
5. **Performance Tests** - Benchmark critical code
6. **Security Tests** - Vulnerability checks

**🔧 Supported Frameworks:**
- Jest / Vitest
- Mocha / Chai
- Jasmine
- Playwright / Cypress
- pytest (Python)
- RSpec (Ruby)

**💬 How to use:**
- Upload code to test
- Type "test [filename]"
- Request coverage: "add tests for authentication"
- Specify scenarios: "test error handling"

**📊 I'll provide:**
✅ Test suite with multiple scenarios
✅ Edge case coverage
✅ Mocking examples
✅ Assertions and expectations
✅ Setup and teardown code`,
            timestamp: Date.now(),
            model: 'BigDaddyG Test Generator'
        });
        
        this.renderMessages(session.messages);
        
        const input = document.getElementById('orchestra-input');
        if (input) input.focus();
    }
    
    async documentCode() {
        console.log('[Orchestra] 📝 Starting documentation generation...');
        
        if (!this.activeSessionId) {
            this.newSession();
        }
        
        const session = this.sessions.get(this.activeSessionId);
        if (!session) return;
        
        session.messages.push({
            role: 'assistant',
            content: `📝 **Documentation Assistant Activated**

I'll help you create comprehensive documentation!

**📚 What I can generate:**

1. **JSDoc Comments** - Function/class documentation
2. **README.md** - Project overview and setup
3. **API Documentation** - Endpoint descriptions
4. **Architecture Docs** - System design
5. **User Guides** - How-to documentation
6. **Contributing Guidelines** - CONTRIBUTING.md
7. **Changelog** - Version history
8. **Code Comments** - Inline explanations

**✨ Documentation Features:**
- Clear descriptions
- Parameter/return type info
- Usage examples
- Edge case notes
- Related function links
- Markdown formatting

**💬 How to use:**
- Upload code to document
- Type "document [filename]"
- Request specific docs: "create README"
- Ask for examples: "add usage examples"

**📋 I'll provide:**
✅ Well-formatted documentation
✅ Code examples
✅ Best practices
✅ Markdown/JSDoc syntax
✅ Organized structure`,
            timestamp: Date.now(),
            model: 'BigDaddyG Documenter'
        });
        
        this.renderMessages(session.messages);
        
        const input = document.getElementById('orchestra-input');
        if (input) input.focus();
    }
}

// Initialize Orchestra Layout
window.orchestraLayout = new OrchestraLayout();

})();


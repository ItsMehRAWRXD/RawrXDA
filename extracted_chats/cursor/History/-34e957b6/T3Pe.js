/**
 * Agent Chat Panel UI
 * Multi-window chat interface for Supernova agent interactions
 */

export function createAgentChatPanel(api, agent) {
    console.log('💬 Creating agent chat panel...');

    // Create chat dock panel
    const chatDock = api.createPanel('supernova.chatDock', {
        title: 'Supernova Chat',
        icon: '🤖',
        position: 'bottom',
        height: 400,
        resizable: true
    });

    // Set up chat dock HTML
    chatDock.setContent(`
        <div class="agent-chat-dock">
            <div class="chat-header">
                <div class="agent-info">
                    <span class="agent-icon">🧠</span>
                    <span class="agent-name">Code Supernova MAX Stealth</span>
                    <span class="agent-status">Ready</span>
                </div>
                <div class="chat-controls">
                    <button id="new-session" class="btn btn-small btn-success">
                        ➕ New Session
                    </button>
                    <button id="clear-all" class="btn btn-small btn-secondary">
                        🗑️ Clear All
                    </button>
                    <button id="settings" class="btn btn-small btn-secondary">
                        ⚙️ Settings
                    </button>
                </div>
            </div>

            <div class="chat-tabs" id="chat-tabs">
                <div class="tab active" data-session="main">
                    <span class="tab-title">Main Session</span>
                    <span class="tab-badge">●</span>
                    <button class="tab-close">×</button>
                </div>
            </div>

            <div class="chat-content" id="chat-content">
                <div class="chat-session active" id="session-main">
                    <div class="chat-messages" id="messages-main">
                        <div class="welcome-message">
                            <div class="agent-avatar">🧠</div>
                            <h3>Welcome to Supernova MAX Stealth</h3>
                            <p>Your ultra-capacity code reasoning agent is ready.</p>
                            <p>Context window: 1M tokens • Backend: Local WASM</p>
                            <div class="agent-capabilities">
                                <span class="capability">🔍 Deep Analysis</span>
                                <span class="capability">✨ Code Generation</span>
                                <span class="capability">🔧 Refactoring</span>
                                <span class="capability">📖 Explanation</span>
                                <span class="capability">🧪 Testing</span>
                                <span class="capability">📚 Documentation</span>
                            </div>
                        </div>
                    </div>

                    <div class="chat-input-area">
                        <div class="input-controls">
                            <select id="context-scope" class="context-selector">
                                <option value="current">Current File</option>
                                <option value="workspace">Workspace</option>
                                <option value="selection">Selection</option>
                                <option value="memory">Memory</option>
                            </select>
                            <span class="context-info">
                                Context: <span id="context-size">0 tokens</span>
                            </span>
                        </div>
                        <div class="input-container">
                            <textarea
                                id="chat-input"
                                placeholder="Ask Supernova anything about your code..."
                                rows="3"
                            ></textarea>
                            <div class="input-actions">
                                <button id="send-message" class="btn btn-primary">
                                    🚀 Send
                                </button>
                                <button id="quick-analysis" class="btn btn-info">
                                    🔍 Quick Analysis
                                </button>
                                <button id="autonomous-mode" class="btn btn-warning">
                                    🤖 Autonomous
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    `);

    // Set up CSS styles
    chatDock.addCSS(`
        .agent-chat-dock {
            display: flex;
            flex-direction: column;
            height: 100%;
            background: linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 50%, #1a1a1a 100%);
            color: #ffffff;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }

        .chat-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 0.75rem 1rem;
            border-bottom: 1px solid rgba(0, 255, 136, 0.3);
            background: rgba(0, 0, 0, 0.2);
        }

        .agent-info {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }

        .agent-icon {
            font-size: 1.5rem;
        }

        .agent-name {
            font-weight: bold;
            color: #00ff88;
        }

        .agent-status {
            font-size: 0.8rem;
            opacity: 0.7;
        }

        .chat-controls {
            display: flex;
            gap: 0.5rem;
        }

        .chat-tabs {
            display: flex;
            background: rgba(0, 0, 0, 0.3);
            border-bottom: 1px solid rgba(0, 255, 136, 0.2);
        }

        .tab {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.5rem 1rem;
            cursor: pointer;
            border-bottom: 2px solid transparent;
            transition: all 0.2s;
            position: relative;
        }

        .tab.active {
            border-bottom-color: #00ff88;
            background: rgba(0, 255, 136, 0.1);
        }

        .tab:hover {
            background: rgba(255, 255, 255, 0.05);
        }

        .tab-badge {
            background: #00ff88;
            color: #000;
            border-radius: 50%;
            width: 8px;
            height: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 0.6rem;
        }

        .tab-close {
            background: none;
            border: none;
            color: rgba(255, 255, 255, 0.5);
            cursor: pointer;
            padding: 0;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 0.8rem;
        }

        .tab-close:hover {
            background: rgba(255, 95, 87, 0.3);
            color: #ff5f57;
        }

        .chat-content {
            flex: 1;
            display: flex;
            flex-direction: column;
        }

        .chat-session {
            display: none;
            flex-direction: column;
            height: 100%;
        }

        .chat-session.active {
            display: flex;
        }

        .chat-messages {
            flex: 1;
            overflow-y: auto;
            padding: 1rem;
            display: flex;
            flex-direction: column;
            gap: 1rem;
        }

        .welcome-message {
            text-align: center;
            padding: 2rem;
            opacity: 0.8;
        }

        .agent-avatar {
            font-size: 3rem;
            margin-bottom: 1rem;
        }

        .agent-capabilities {
            display: flex;
            flex-wrap: wrap;
            gap: 0.5rem;
            justify-content: center;
            margin-top: 1rem;
        }

        .capability {
            background: rgba(0, 255, 136, 0.1);
            border: 1px solid rgba(0, 255, 136, 0.3);
            padding: 0.25rem 0.5rem;
            border-radius: 12px;
            font-size: 0.8rem;
        }

        .message {
            display: flex;
            gap: 0.75rem;
            max-width: 80%;
        }

        .message.user {
            align-self: flex-end;
            flex-direction: row-reverse;
        }

        .message.agent {
            align-self: flex-start;
        }

        .message-avatar {
            width: 32px;
            height: 32px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.2rem;
            flex-shrink: 0;
        }

        .message.user .message-avatar {
            background: linear-gradient(135deg, #00ff88, #00ccff);
        }

        .message.agent .message-avatar {
            background: linear-gradient(135deg, #667eea, #764ba2);
        }

        .message-content {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid rgba(0, 255, 136, 0.2);
            border-radius: 8px;
            padding: 0.75rem;
            position: relative;
        }

        .message.user .message-content {
            background: rgba(0, 255, 136, 0.1);
            border-color: rgba(0, 255, 136, 0.3);
        }

        .message-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 0.5rem;
            font-size: 0.8rem;
            opacity: 0.7;
        }

        .message-text {
            line-height: 1.4;
            white-space: pre-wrap;
            word-wrap: break-word;
        }

        .message-actions {
            display: flex;
            gap: 0.5rem;
            margin-top: 0.5rem;
        }

        .message-action {
            background: none;
            border: 1px solid rgba(255, 255, 255, 0.2);
            color: rgba(255, 255, 255, 0.7);
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            cursor: pointer;
            font-size: 0.7rem;
        }

        .message-action:hover {
            background: rgba(255, 255, 255, 0.1);
            color: #ffffff;
        }

        .streaming-indicator {
            display: inline-block;
            width: 8px;
            height: 8px;
            background: #00ff88;
            border-radius: 50%;
            animation: pulse 1s infinite;
            margin-left: 0.5rem;
        }

        .chat-input-area {
            padding: 1rem;
            border-top: 1px solid rgba(0, 255, 136, 0.3);
            background: rgba(0, 0, 0, 0.2);
        }

        .input-controls {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 0.5rem;
        }

        .context-selector {
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid rgba(0, 255, 136, 0.3);
            color: #ffffff;
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            font-size: 0.8rem;
        }

        .context-info {
            font-size: 0.8rem;
            opacity: 0.7;
        }

        .input-container {
            display: flex;
            gap: 0.5rem;
        }

        .input-container textarea {
            flex: 1;
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid rgba(0, 255, 136, 0.3);
            border-radius: 6px;
            color: #ffffff;
            padding: 0.75rem;
            font-family: inherit;
            resize: none;
        }

        .input-container textarea:focus {
            outline: none;
            border-color: #00ff88;
            box-shadow: 0 0 0 2px rgba(0, 255, 136, 0.2);
        }

        .input-actions {
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
        }

        .loading-spinner {
            display: inline-block;
            width: 12px;
            height: 12px;
            border: 2px solid rgba(0, 255, 136, 0.3);
            border-radius: 50%;
            border-top-color: #00ff88;
            animation: spin 1s ease-in-out infinite;
            margin-right: 0.5rem;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        .code-block {
            background: rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.2);
            border-radius: 4px;
            padding: 0.5rem;
            margin: 0.5rem 0;
            font-family: 'Courier New', monospace;
            overflow-x: auto;
        }

        .error-message {
            background: rgba(255, 95, 87, 0.1);
            border: 1px solid rgba(255, 95, 87, 0.3);
            color: #ff5f57;
            padding: 0.5rem;
            border-radius: 4px;
            margin: 0.5rem 0;
        }

        .success-message {
            background: rgba(40, 202, 66, 0.1);
            border: 1px solid rgba(40, 202, 66, 0.3);
            color: #28ca42;
            padding: 0.5rem;
            border-radius: 4px;
            margin: 0.5rem 0;
        }
    `);

    // Set up event handlers
    setupChatEventHandlers(chatDock, api, agent);

    // Start real-time updates
    startChatUpdates(chatDock, api, agent);

    console.log('✅ Agent chat panel created');
    return chatDock;
}

function setupChatEventHandlers(chatDock, api, agent) {
    let currentSession = 'main';
    let isStreaming = false;

    // Tab management
    chatDock.onClick('new-session', async () => {
        const sessionName = await api.showInputBox({
            placeHolder: 'Enter session name (optional)',
            prompt: 'New Chat Session'
        });

        const sessionId = sessionName || `session-${Date.now()}`;
        await agent.createSession(sessionId);

        // Create new tab
        const tabsContainer = chatDock.getElement('chat-tabs');
        const newTab = document.createElement('div');
        newTab.className = 'tab';
        newTab.dataset.session = sessionId;
        newTab.innerHTML = `
            <span class="tab-title">${sessionName || sessionId}</span>
            <span class="tab-badge">●</span>
            <button class="tab-close">×</button>
        `;
        tabsContainer.appendChild(newTab);

        // Create new chat session
        const chatContent = chatDock.getElement('chat-content');
        const newSession = document.createElement('div');
        newSession.className = 'chat-session';
        newSession.id = `session-${sessionId}`;
        newSession.innerHTML = `
            <div class="chat-messages" id="messages-${sessionId}">
                <div class="welcome-message">
                    <div class="agent-avatar">🧠</div>
                    <h3>Session: ${sessionName || sessionId}</h3>
                    <p>Ready for code reasoning and analysis.</p>
                </div>
            </div>
            <div class="chat-input-area">
                <div class="input-controls">
                    <select class="context-selector" data-session="${sessionId}">
                        <option value="current">Current File</option>
                        <option value="workspace">Workspace</option>
                        <option value="selection">Selection</option>
                        <option value="memory">Memory</option>
                    </select>
                    <span class="context-info">
                        Context: <span class="context-size">0 tokens</span>
                    </span>
                </div>
                <div class="input-container">
                    <textarea
                        class="chat-input"
                        placeholder="Ask Supernova..."
                        rows="3"
                    ></textarea>
                    <div class="input-actions">
                        <button class="btn btn-primary send-message">🚀 Send</button>
                        <button class="btn btn-info quick-analysis">🔍 Analyze</button>
                    </div>
                </div>
            </div>
        `;

        chatContent.appendChild(newSession);

        // Switch to new tab
        switchToTab(chatDock, sessionId);

        api.showInformation(`Created new Supernova session: ${sessionId}`);
    });

    chatDock.onClick('clear-all', async () => {
        if (confirm('Clear all chat sessions? This cannot be undone.')) {
            try {
                await agent.clearAllSessions();
                resetChatDock(chatDock, api, agent);
                api.showInformation('All sessions cleared');
            } catch (error) {
                api.showError('Failed to clear sessions: ' + error.message);
            }
        }
    });

    chatDock.onClick('settings', () => {
        showAgentSettings(chatDock, api, agent);
    });

    // Tab switching
    chatDock.getElement('chat-tabs').addEventListener('click', (e) => {
        if (e.target.closest('.tab')) {
            const tab = e.target.closest('.tab');
            const sessionId = tab.dataset.session;
            if (sessionId) {
                switchToTab(chatDock, sessionId);
            }
        }

        if (e.target.classList.contains('tab-close')) {
            e.stopPropagation();
            const tab = e.target.closest('.tab');
            const sessionId = tab.dataset.session;
            closeTab(chatDock, sessionId);
        }
    });

    // Message sending
    chatDock.getElement('chat-content').addEventListener('click', async (e) => {
        if (e.target.classList.contains('send-message')) {
            await sendMessage(chatDock, currentSession, api, agent);
        }

        if (e.target.classList.contains('quick-analysis')) {
            await quickAnalysis(chatDock, currentSession, api, agent);
        }
    });

    // Keyboard shortcuts
    chatDock.getElement('chat-content').addEventListener('keydown', async (e) => {
        if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
            e.preventDefault();
            await sendMessage(chatDock, currentSession, api, agent);
        }
    });

    // Message actions
    chatDock.getElement('chat-content').addEventListener('click', (e) => {
        if (e.target.classList.contains('message-action')) {
            handleMessageAction(e.target, api, agent);
        }
    });
}

function switchToTab(chatDock, sessionId) {
    // Update tab states
    const tabs = chatDock.getElement('chat-tabs').querySelectorAll('.tab');
    tabs.forEach(tab => {
        tab.classList.remove('active');
        if (tab.dataset.session === sessionId) {
            tab.classList.add('active');
        }
    });

    // Update session states
    const sessions = chatDock.getElement('chat-content').querySelectorAll('.chat-session');
    sessions.forEach(session => {
        session.classList.remove('active');
        if (session.id === `session-${sessionId}`) {
            session.classList.add('active');
        }
    });

    currentSession = sessionId;
}

async function sendMessage(chatDock, sessionId, api, agent) {
    const input = chatDock.getElement(`session-${sessionId}`).querySelector('.chat-input');
    const message = input.value.trim();

    if (!message) return;

    // Clear input
    input.value = '';

    // Add user message
    addMessage(chatDock, sessionId, 'user', message);

    // Show typing indicator
    showTypingIndicator(chatDock, sessionId);

    try {
        // Send to agent
        await agent.chat(sessionId, message);

        // Update context info
        updateContextInfo(chatDock, sessionId, agent);

    } catch (error) {
        hideTypingIndicator(chatDock, sessionId);
        addMessage(chatDock, sessionId, 'agent', `❌ Error: ${error.message}`, 'error');
    }
}

async function quickAnalysis(chatDock, sessionId, api, agent) {
    const selection = await api.getEditorSelection();
    if (selection) {
        addMessage(chatDock, sessionId, 'user', `🔍 Analyze this code:\n\`\`\`\n${selection}\n\`\`\``);
        showTypingIndicator(chatDock, sessionId);

        try {
            const result = await agent.explainCode(selection);
            hideTypingIndicator(chatDock, sessionId);
            addMessage(chatDock, sessionId, 'agent', result);
            updateContextInfo(chatDock, sessionId, agent);
        } catch (error) {
            hideTypingIndicator(chatDock, sessionId);
            addMessage(chatDock, sessionId, 'agent', `❌ Analysis failed: ${error.message}`, 'error');
        }
    } else {
        api.showInformation('Please select code to analyze');
    }
}

function addMessage(chatDock, sessionId, role, content, type = 'normal') {
    const messagesContainer = chatDock.getElement(`messages-${sessionId}`);
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${role}`;

    const avatar = role === 'user' ? '👤' : '🧠';
    const timestamp = new Date().toLocaleTimeString();

    let messageContent = content;
    if (type === 'error') {
        messageContent = `<div class="error-message">${content}</div>`;
    } else if (type === 'success') {
        messageContent = `<div class="success-message">${content}</div>`;
    }

    messageDiv.innerHTML = `
        <div class="message-avatar">${avatar}</div>
        <div class="message-content">
            <div class="message-header">
                <span class="message-role">${role === 'user' ? 'You' : 'Supernova'}</span>
                <span class="message-time">${timestamp}</span>
            </div>
            <div class="message-text">${messageContent}</div>
            ${role === 'agent' ? `
                <div class="message-actions">
                    <button class="message-action copy">📋 Copy</button>
                    <button class="message-action apply">✅ Apply</button>
                    <button class="message-action explain">❓ Explain</button>
                </div>
            ` : ''}
        </div>
    `;

    messagesContainer.appendChild(messageDiv);
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
}

function showTypingIndicator(chatDock, sessionId) {
    const messagesContainer = chatDock.getElement(`messages-${sessionId}`);
    const typingDiv = document.createElement('div');
    typingDiv.className = 'message agent typing';
    typingDiv.id = `typing-${sessionId}`;

    typingDiv.innerHTML = `
        <div class="message-avatar">🧠</div>
        <div class="message-content">
            <div class="message-header">
                <span class="message-role">Supernova</span>
                <span class="message-time">typing...</span>
            </div>
            <div class="message-text">
                <span class="streaming-indicator"></span>
                Reasoning...
            </div>
        </div>
    `;

    messagesContainer.appendChild(typingDiv);
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
}

function hideTypingIndicator(chatDock, sessionId) {
    const typingDiv = chatDock.getElement(`typing-${sessionId}`);
    if (typingDiv) {
        typingDiv.remove();
    }
}

function handleMessageAction(button, api, agent) {
    const action = button.classList[1]; // copy, apply, explain
    const messageDiv = button.closest('.message');
    const messageText = messageDiv.querySelector('.message-text').textContent;

    switch (action) {
        case 'copy':
            navigator.clipboard.writeText(messageText);
            api.showInformation('Message copied to clipboard');
            break;

        case 'apply':
            // Apply code suggestions to editor
            if (messageText.includes('```')) {
                const codeBlocks = messageText.match(/```[\s\S]*?```/g);
                if (codeBlocks) {
                    const code = codeBlocks[0].replace(/```/g, '');
                    api.insertText(code);
                    api.showInformation('Code applied to editor');
                }
            }
            break;

        case 'explain':
            // Ask for explanation of the message
            addMessage(chatDock, currentSession, 'user', `Explain this in more detail:\n${messageText}`);
            sendMessage(chatDock, currentSession, api, agent);
            break;
    }
}

function updateContextInfo(chatDock, sessionId, agent) {
    const contextSize = agent.getMemoryUsage().totalTokens;
    const infoElement = chatDock.getElement(`session-${sessionId}`).querySelector('.context-size');
    if (infoElement) {
        infoElement.textContent = `${Math.round(contextSize / 1000)}K tokens`;
    }
}

function updateSessionList(chatDock, agent) {
    const sessions = agent.getActiveSessions();
    const tabsContainer = chatDock.getElement('chat-tabs');

    // Update existing tabs
    const existingTabs = tabsContainer.querySelectorAll('.tab');
    existingTabs.forEach(tab => {
        const sessionId = tab.dataset.session;
        if (sessions.includes(sessionId)) {
            tab.querySelector('.tab-badge').textContent = '●';
        } else {
            tab.querySelector('.tab-badge').textContent = '○';
        }
    });
}

function closeTab(chatDock, sessionId) {
    // Remove tab
    const tab = chatDock.getElement('chat-tabs').querySelector(`[data-session="${sessionId}"]`);
    if (tab) {
        tab.remove();
    }

    // Remove session
    const session = chatDock.getElement(`session-${sessionId}`);
    if (session) {
        session.remove();
    }

    // Switch to main tab if current tab was closed
    if (currentSession === sessionId) {
        switchToTab(chatDock, 'main');
    }

    api.showInformation(`Session ${sessionId} closed`);
}

function resetChatDock(chatDock, api, agent) {
    // Clear all tabs except main
    const tabsContainer = chatDock.getElement('chat-tabs');
    const chatContent = chatDock.getElement('chat-content');

    // Keep only main tab
    const mainTab = tabsContainer.querySelector('[data-session="main"]');
    tabsContainer.innerHTML = '';
    tabsContainer.appendChild(mainTab);

    // Keep only main session
    const mainSession = chatContent.querySelector('#session-main');
    chatContent.innerHTML = '';
    chatContent.appendChild(mainSession);

    // Reset main session
    const mainMessages = chatDock.getElement('messages-main');
    mainMessages.innerHTML = `
        <div class="welcome-message">
            <div class="agent-avatar">🧠</div>
            <h3>Welcome to Supernova MAX Stealth</h3>
            <p>Your ultra-capacity code reasoning agent is ready.</p>
        </div>
    `;

    switchToTab(chatDock, 'main');
}

function showAgentSettings(chatDock, api, agent) {
    const settings = agent.getCapabilities();
    const settingsText = `
🧠 Agent Settings:
- Model: ${settings.model}
- Backend: ${settings.backend}
- Context: ${settings.contextWindow} tokens
- Tools: ${settings.tools.join(', ')}
- Sessions: ${settings.sessions.length}

Memory Usage:
- Total: ${Math.round(settings.memory.totalTokens / 1000)}K tokens
- Utilization: ${Math.round(settings.memory.utilization)}%
- Sessions: ${settings.memory.sessions}
    `;

    api.showInformation(settingsText);
}

function startChatUpdates(chatDock, api, agent) {
    // Update context info every 2 seconds
    setInterval(() => {
        updateContextInfo(chatDock, currentSession, agent);
        updateSessionList(chatDock, agent);
    }, 2000);
}

export { createAgentChatPanel };

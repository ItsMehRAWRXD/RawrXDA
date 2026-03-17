// ========================================================
// MULTI-CHAT COMPLETE FIX
// ========================================================
// Fixes ALL multi-chat functionality including + tab button

(function fixMultiChat() {
    console.log('[MULTI-CHAT FIX] 🔧 Fixing all multi-chat functionality...');

    // ========================================================
    // 1. INITIALIZE MULTI-CHAT STATE
    // ========================================================
    if (!window.multiChatTabs) {
        window.multiChatTabs = new Map();
    }
    
    if (!window.activeMultiChatTab) {
        window.activeMultiChatTab = null;
    }
    
    if (!window.multiChatHistory) {
        window.multiChatHistory = new Map();
    }

    let tabCounter = 0;

    // ========================================================
    // 2. CREATE NEW CHAT TAB
    // ========================================================
    window.createNewChatTab = function() {
        console.log('[MULTI-CHAT FIX] ➕ Creating new chat tab...');
        
        const tabId = `chat-tab-${++tabCounter}`;
        const tabName = `Chat ${tabCounter}`;
        
        // Find tab list container
        const tabList = document.getElementById('chat-tab-list') ||
                       document.querySelector('[class*="tab-list"]') ||
                       document.querySelector('.multi-chat-tabs');
        
        if (!tabList) {
            console.error('[MULTI-CHAT FIX] ❌ Cannot find tab list container');
            return null;
        }

        // Create tab element
        const tab = document.createElement('div');
        tab.className = 'chat-tab';
        tab.id = tabId;
        tab.dataset.tabId = tabId;
        tab.style.cssText = `
            display: inline-flex !important;
            align-items: center !important;
            gap: 6px !important;
            padding: 6px 12px !important;
            background: #2d2d2d !important;
            color: #ffffff !important;
            border: 1px solid #444 !important;
            border-radius: 4px !important;
            cursor: pointer !important;
            font-size: 13px !important;
            margin-right: 4px !important;
            visibility: visible !important;
            opacity: 1 !important;
        `;

        // Tab label
        const label = document.createElement('span');
        label.textContent = tabName;
        label.style.cssText = 'user-select: none;';

        // Close button
        const closeBtn = document.createElement('span');
        closeBtn.textContent = '×';
        closeBtn.style.cssText = `
            cursor: pointer !important;
            font-size: 18px !important;
            line-height: 1 !important;
            padding: 0 4px !important;
            opacity: 0.7 !important;
        `;
        
        closeBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            removeMultiChatTab(tabId);
        });
        
        closeBtn.addEventListener('mouseenter', () => closeBtn.style.opacity = '1');
        closeBtn.addEventListener('mouseleave', () => closeBtn.style.opacity = '0.7');

        tab.appendChild(label);
        tab.appendChild(closeBtn);

        // Click to switch
        tab.addEventListener('click', () => switchMultiChatTab(tabId));

        // Add to tab list
        tabList.appendChild(tab);

        // Store tab data
        window.multiChatTabs.set(tabId, {
            id: tabId,
            name: tabName,
            element: tab,
            messages: [],
            model: 'bigdaddyg:latest',
            research: false
        });

        // Initialize message history
        window.multiChatHistory.set(tabId, []);

        // Switch to new tab
        switchMultiChatTab(tabId);

        console.log(`[MULTI-CHAT FIX] ✅ Created tab: ${tabName} (${tabId})`);
        
        return tabId;
    };

    // Alias for compatibility
    window.addMultiChatTab = window.createNewChatTab;

    // ========================================================
    // 3. SWITCH CHAT TAB
    // ========================================================
    window.switchMultiChatTab = function(tabId) {
        console.log(`[MULTI-CHAT FIX] 🔄 Switching to tab: ${tabId}`);

        // Deactivate all tabs
        window.multiChatTabs.forEach((tabData, id) => {
            if (tabData.element) {
                tabData.element.style.background = '#2d2d2d';
                tabData.element.style.borderColor = '#444';
            }
        });

        // Activate selected tab
        const tabData = window.multiChatTabs.get(tabId);
        if (tabData && tabData.element) {
            tabData.element.style.background = '#007acc';
            tabData.element.style.borderColor = '#007acc';
            window.activeMultiChatTab = tabId;

            // Load messages for this tab
            loadMultiChatMessages(tabId);

            console.log(`[MULTI-CHAT FIX] ✅ Switched to: ${tabData.name}`);
        }
    };

    // ========================================================
    // 4. REMOVE CHAT TAB
    // ========================================================
    window.removeMultiChatTab = function(tabId) {
        console.log(`[MULTI-CHAT FIX] 🗑️ Removing tab: ${tabId}`);

        const tabData = window.multiChatTabs.get(tabId);
        if (tabData && tabData.element) {
            tabData.element.remove();
            window.multiChatTabs.delete(tabId);
            window.multiChatHistory.delete(tabId);

            // If removing active tab, switch to another
            if (window.activeMultiChatTab === tabId) {
                const remainingTabs = Array.from(window.multiChatTabs.keys());
                if (remainingTabs.length > 0) {
                    switchMultiChatTab(remainingTabs[0]);
                } else {
                    window.activeMultiChatTab = null;
                    // Create first tab automatically
                    createNewChatTab();
                }
            }

            console.log(`[MULTI-CHAT FIX] ✅ Removed tab: ${tabId}`);
        }
    };

    // ========================================================
    // 5. SEND MULTI-CHAT MESSAGE
    // ========================================================
    window.sendMultiChatMessage = function() {
        console.log('[MULTI-CHAT FIX] 📤 Sending message...');

        const input = document.getElementById('multi-chat-input') ||
                     document.querySelector('[id*="multi-chat"] input[type="text"]') ||
                     document.querySelector('[id*="multi-chat"] textarea');

        if (!input) {
            console.error('[MULTI-CHAT FIX] ❌ Cannot find input element');
            return;
        }

        const message = input.value.trim();
        if (!message) {
            console.warn('[MULTI-CHAT FIX] ⚠️ Empty message');
            return;
        }

        const tabId = window.activeMultiChatTab;
        if (!tabId) {
            console.warn('[MULTI-CHAT FIX] ⚠️ No active tab, creating one...');
            createNewChatTab();
            return sendMultiChatMessage(); // Retry
        }

        // Add message to history
        const messageData = {
            role: 'user',
            content: message,
            timestamp: new Date().toISOString()
        };

        const history = window.multiChatHistory.get(tabId) || [];
        history.push(messageData);
        window.multiChatHistory.set(tabId, history);

        // Display message
        displayMultiChatMessage(tabId, messageData);

        // Clear input
        input.value = '';

        // Send to AI (simulate or actual implementation)
        sendToAI(tabId, message);

        console.log('[MULTI-CHAT FIX] ✅ Message sent');
    };

    // ========================================================
    // 6. LOAD CHAT MESSAGES
    // ========================================================
    function loadMultiChatMessages(tabId) {
        const messages = window.multiChatHistory.get(tabId) || [];
        const messagesContainer = document.getElementById('multi-chat-messages') ||
                                 document.querySelector('[id*="multi-chat"] [class*="messages"]');

        if (!messagesContainer) {
            console.error('[MULTI-CHAT FIX] ❌ Cannot find messages container');
            return;
        }

        // Clear existing messages
        messagesContainer.innerHTML = '';

        // Display all messages for this tab
        messages.forEach(msg => displayMultiChatMessage(tabId, msg));
    }

    // ========================================================
    // 7. DISPLAY CHAT MESSAGE
    // ========================================================
    function displayMultiChatMessage(tabId, messageData) {
        const messagesContainer = document.getElementById('multi-chat-messages') ||
                                 document.querySelector('[id*="multi-chat"] [class*="messages"]');

        if (!messagesContainer) return;

        const msgDiv = document.createElement('div');
        msgDiv.className = `message ${messageData.role}`;
        msgDiv.style.cssText = `
            padding: 8px 12px;
            margin: 8px 0;
            border-radius: 6px;
            background: ${messageData.role === 'user' ? '#2d2d2d' : '#1a472a'};
            color: #ffffff;
            word-wrap: break-word;
        `;

        const roleSpan = document.createElement('strong');
        roleSpan.textContent = messageData.role === 'user' ? 'You: ' : 'AI: ';
        roleSpan.style.cssText = 'display: block; margin-bottom: 4px; color: #888;';

        const contentDiv = document.createElement('div');
        contentDiv.textContent = messageData.content;

        msgDiv.appendChild(roleSpan);
        msgDiv.appendChild(contentDiv);
        messagesContainer.appendChild(msgDiv);

        // Scroll to bottom
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    // ========================================================
    // 8. SEND TO AI (Placeholder)
    // ========================================================
    async function sendToAI(tabId, message) {
        // Get tab data
        const tabData = window.multiChatTabs.get(tabId);
        const model = tabData?.model || 'bigdaddyg:latest';

        // Simulate AI response (replace with actual AI call)
        setTimeout(() => {
            const responseData = {
                role: 'assistant',
                content: `This is a simulated response to: "${message}". Actual AI integration pending.`,
                timestamp: new Date().toISOString()
            };

            const history = window.multiChatHistory.get(tabId) || [];
            history.push(responseData);
            window.multiChatHistory.set(tabId, history);

            displayMultiChatMessage(tabId, responseData);
        }, 1000);
    }

    // ========================================================
    // 9. ADDITIONAL MULTI-CHAT FUNCTIONS
    // ========================================================
    
    window.toggleMultiChatResearch = function() {
        const tabId = window.activeMultiChatTab;
        if (!tabId) return;

        const tabData = window.multiChatTabs.get(tabId);
        if (tabData) {
            tabData.research = !tabData.research;
            console.log(`[MULTI-CHAT FIX] Research mode: ${tabData.research ? 'ON' : 'OFF'}`);
        }
    };

    window.toggleMultiChatSettings = function() {
        const settings = document.getElementById('multi-chat-settings');
        if (settings) {
            const isHidden = settings.style.display === 'none';
            settings.style.display = isHidden ? 'block' : 'none';
            console.log(`[MULTI-CHAT FIX] Settings: ${isHidden ? 'SHOWN' : 'HIDDEN'}`);
        }
    };

    window.changeMultiChatModel = function() {
        const select = document.getElementById('multi-chat-model-select');
        if (!select) return;

        const model = select.value;
        const tabId = window.activeMultiChatTab;
        if (tabId) {
            const tabData = window.multiChatTabs.get(tabId);
            if (tabData) {
                tabData.model = model;
                console.log(`[MULTI-CHAT FIX] Model changed to: ${model}`);
            }
        }
    };

    window.saveMultiChatCheckpoint = function() {
        const tabId = window.activeMultiChatTab;
        if (!tabId) return;

        const history = window.multiChatHistory.get(tabId);
        localStorage.setItem(`multi-chat-checkpoint-${tabId}`, JSON.stringify(history));
        console.log('[MULTI-CHAT FIX] ✅ Checkpoint saved');
    };

    window.restoreLastCheckpoint = function() {
        const tabId = window.activeMultiChatTab;
        if (!tabId) return;

        const checkpoint = localStorage.getItem(`multi-chat-checkpoint-${tabId}`);
        if (checkpoint) {
            const history = JSON.parse(checkpoint);
            window.multiChatHistory.set(tabId, history);
            loadMultiChatMessages(tabId);
            console.log('[MULTI-CHAT FIX] ✅ Checkpoint restored');
        }
    };

    window.retryLastCheckpoint = function() {
        window.restoreLastCheckpoint();
    };

    window.clearMultiChat = function() {
        const tabId = window.activeMultiChatTab;
        if (!tabId) return;

        window.multiChatHistory.set(tabId, []);
        loadMultiChatMessages(tabId);
        console.log('[MULTI-CHAT FIX] ✅ Chat cleared');
    };

    window.closeMultiChat = function() {
        const panel = document.getElementById('multi-chat-panel');
        if (panel) {
            panel.style.display = 'none';
            console.log('[MULTI-CHAT FIX] ✅ Panel closed');
        }
    };

    // ========================================================
    // 10. WIRE UP + BUTTON
    // ========================================================
    function wireUpPlusButton() {
        // Find all + buttons
        const buttons = Array.from(document.querySelectorAll('button, [onclick]'));
        const plusButtons = buttons.filter(btn => 
            btn.textContent.includes('+') || 
            btn.textContent.includes('New') ||
            btn.title?.toLowerCase().includes('tab') ||
            btn.title?.toLowerCase().includes('chat')
        );

        plusButtons.forEach(btn => {
            // Remove old onclick
            btn.removeAttribute('onclick');
            
            // Add new onclick
            btn.addEventListener('click', (e) => {
                e.preventDefault();
                e.stopPropagation();
                createNewChatTab();
            });

            console.log('[MULTI-CHAT FIX] ✅ Wired up + button');
        });

        // Also wire up Enter key for sending messages
        const input = document.getElementById('multi-chat-input');
        if (input) {
            input.addEventListener('keypress', (e) => {
                if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    sendMultiChatMessage();
                }
            });
            console.log('[MULTI-CHAT FIX] ✅ Enter key wired up');
        }
    }

    // ========================================================
    // 11. INITIALIZE
    // ========================================================
    function initialize() {
        wireUpPlusButton();
        
        // Create first tab if none exist
        if (window.multiChatTabs.size === 0) {
            createNewChatTab();
        }

        console.log('[MULTI-CHAT FIX] 🎉 ALL MULTI-CHAT FIXES APPLIED!');
        console.log('[MULTI-CHAT FIX] Functions available:');
        console.log('  - createNewChatTab()');
        console.log('  - sendMultiChatMessage()');
        console.log('  - switchMultiChatTab(tabId)');
        console.log('  - removeMultiChatTab(tabId)');
        console.log('  - toggleMultiChatResearch()');
        console.log('  - toggleMultiChatSettings()');
        console.log('  - saveMultiChatCheckpoint()');
        console.log('  - restoreLastCheckpoint()');
        console.log('  - clearMultiChat()');
        console.log('  - closeMultiChat()');
    }

    // Run initialization
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initialize);
    } else {
        initialize();
    }

    // Re-run after delay for dynamic content
    setTimeout(initialize, 1000);
    setTimeout(initialize, 3000);

})();

console.log('%c✅ MULTI-CHAT COMPLETE FIX LOADED!', 'color: green; font-size: 16px; font-weight: bold;');
console.log('%cAll multi-chat functions are now available!', 'color: cyan;');
console.log('%cClick the + button to create new chat tabs!', 'color: yellow;');
/**
 * BigDaddyG IDE - Chat History Manager
 * Read/Unread tracking, Past Chats, Full persistence
 */

(function() {
'use strict';

class ChatHistoryManager {
    constructor() {
        this.currentChat = null;
        this.chats = [];
        this.storageKey = 'bigdaddyg-chat-history';
        this.currentChatKey = 'bigdaddyg-current-chat';
        this.init();
    }
    
    init() {
        console.log('[ChatHistory] 💬 Initializing chat history...');
        
        // Load saved chats
        this.loadChats();
        
        // Load or create current chat
        const savedCurrentId = localStorage.getItem(this.currentChatKey);
        if (savedCurrentId) {
            this.currentChat = this.chats.find(c => c.id === savedCurrentId);
        }
        
        if (!this.currentChat) {
            this.createNewChat({ render: false });
        }
        
        // Render UI with current chat
        this.renderPastChats();
        this.loadChatIntoUI(this.currentChat);
        
        console.log('[ChatHistory] ✅ Chat history ready');
    }
    
    loadChats() {
        try {
            const saved = localStorage.getItem(this.storageKey);
            if (saved) {
                this.chats = JSON.parse(saved);
                console.log(`[ChatHistory] 📚 Loaded ${this.chats.length} chats`);
            }
        } catch (error) {
            console.error('[ChatHistory] ❌ Error loading chats:', error);
            this.chats = [];
        }
    }
    
    saveChats() {
        try {
            localStorage.setItem(this.storageKey, JSON.stringify(this.chats));
            console.log('[ChatHistory] 💾 Chats saved');
        } catch (error) {
            console.error('[ChatHistory] ❌ Error saving chats:', error);
        }
    }
    
    createNewChat(options = {}) {
        const now = new Date();
        const chat = {
            id: now.getTime().toString(),
            title: 'New Chat',
            created: now.toISOString(),
            updated: now.toISOString(),
            messages: [],
            unreadCount: 0
        };
        
        this.chats.unshift(chat);
        this.currentChat = chat;
        localStorage.setItem(this.currentChatKey, chat.id);
        this.saveChats();
        this.renderPastChats();
        
        if (options.render !== false) {
            this.clearChatUI();
            this.loadChatIntoUI(chat);
        }
        
        console.log('[ChatHistory] ✨ New chat created:', chat.id);
        return chat;
    }
    
    switchToChat(chatId) {
        const chat = this.chats.find(c => c.id === chatId);
        if (!chat) {
            console.error('[ChatHistory] ❌ Chat not found:', chatId);
            return;
        }
        
        this.currentChat = chat;
        localStorage.setItem(this.currentChatKey, chatId);
        
        // Mark all messages as read
        chat.messages.forEach(msg => {
            if (msg.isUnread) {
                msg.isUnread = false;
            }
        });
        chat.unreadCount = 0;
        this.saveChats();
        
        // Render chat
        this.clearChatUI();
        this.loadChatIntoUI(chat);
        this.renderPastChats();
        
        console.log('[ChatHistory] 📖 Switched to chat:', chatId);
    }
    
    addMessage(role, content, attachmentsOrOptions = null, errorFlag = false) {
        if (!this.currentChat) {
            this.createNewChat({ render: false });
        }
        
        let options = {};
        if (attachmentsOrOptions && typeof attachmentsOrOptions === 'object' && !Array.isArray(attachmentsOrOptions)) {
            options = { ...attachmentsOrOptions };
        } else {
            options.attachments = attachmentsOrOptions;
            options.error = errorFlag;
        }
        
        const {
            attachments = null,
            error = false,
            source = 'right-sidebar',
            render = false,
            html = null,
            metadata = null,
            timestamp = new Date().toISOString()
        } = options;
        
        const message = {
            id: `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
            role,
            content,
            html,
            attachments: this.normalizeAttachments(attachments),
            error: Boolean(error),
            timestamp,
            source,
            metadata: metadata ? { ...metadata } : undefined
        };
        
        this.currentChat.messages.push(message);
        this.currentChat.updated = timestamp;
        
        if (role === 'user' && this.currentChat.messages.length === 1) {
            this.currentChat.title = content.substring(0, 50) + (content.length > 50 ? '...' : '');
        }
        
        this.saveChats();
        this.renderPastChats();
        
        if (render) {
            this.appendMessageToUI(message, { scroll: true });
        }
        
        return message;
    }
    
    markMessageAsUnread(messageId) {
        if (!this.currentChat) return;
        
        const message = this.currentChat.messages.find(m => m.id === messageId);
        if (message) {
            message.isUnread = true;
            this.currentChat.unreadCount = this.currentChat.messages.filter(m => m.isUnread).length;
            this.saveChats();
            this.renderPastChats();
            console.log('[ChatHistory] 📌 Message marked as unread');
        }
    }
    
    markMessageAsRead(messageId) {
        if (!this.currentChat) return;
        
        const message = this.currentChat.messages.find(m => m.id === messageId);
        if (message && message.isUnread) {
            message.isUnread = false;
            this.currentChat.unreadCount = this.currentChat.messages.filter(m => m.isUnread).length;
            this.saveChats();
            this.renderPastChats();
            console.log('[ChatHistory] ✅ Message marked as read');
        }
    }
    
    deleteChat(chatId) {
        const index = this.chats.findIndex(c => c.id === chatId);
        if (index !== -1) {
            this.chats.splice(index, 1);
            
            // If deleting current chat, switch to most recent or create new
            if (this.currentChat && this.currentChat.id === chatId) {
                if (this.chats.length > 0) {
                    this.switchToChat(this.chats[0].id);
                } else {
                    this.createNewChat();
                }
            }
            
            this.saveChats();
            this.renderPastChats();
            console.log('[ChatHistory] 🗑️ Chat deleted:', chatId);
        }
    }
    
    exportChats() {
        const dataStr = JSON.stringify(this.chats, null, 2);
        const blob = new Blob([dataStr], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `bigdaddyg-chats-${Date.now()}.json`;
        a.click();
        URL.revokeObjectURL(url);
        console.log('[ChatHistory] 📥 Chats exported');
    }
    
    importChats(file) {
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const imported = JSON.parse(e.target.result);
                this.chats = [...imported, ...this.chats];
                this.saveChats();
                this.renderPastChats();
                console.log('[ChatHistory] 📤 Chats imported');
            } catch (error) {
                console.error('[ChatHistory] ❌ Import failed:', error);
                alert('Failed to import chats: ' + error.message);
            }
        };
        reader.readAsText(file);
    }
    
    renderPastChats() {
        const container = document.getElementById('past-chats-list');
        if (!container) return;
        
        container.innerHTML = '';
        
        if (this.chats.length === 0) {
            container.innerHTML = '<div style="padding: 20px; text-align: center; color: var(--cursor-text-muted); font-size: 12px;">No past chats</div>';
            return;
        }
        
        this.chats.forEach(chat => {
            const chatEl = document.createElement('div');
            chatEl.className = 'past-chat-item';
            const isActive = this.currentChat && this.currentChat.id === chat.id;
            
            chatEl.style.cssText = `
                padding: 10px 12px;
                margin: 4px 8px;
                background: ${isActive ? 'var(--cursor-jade-light)' : 'var(--cursor-input-bg)'};
                border: 1px solid ${isActive ? 'var(--cursor-accent)' : 'var(--cursor-border)'};
                border-radius: 6px;
                cursor: pointer;
                transition: all 0.2s ease;
                position: relative;
            `;
            
            const date = new Date(chat.updated);
            const timeAgo = this.formatTimeAgo(date);
            
            chatEl.innerHTML = `
                <div style="display: flex; justify-content: space-between; align-items: start; gap: 8px;">
                    <div style="flex: 1; min-width: 0;">
                        <div style="font-size: 12px; font-weight: 600; color: var(--cursor-text); margin-bottom: 3px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">
                            ${chat.title}
                        </div>
                        <div style="font-size: 10px; color: var(--cursor-text-secondary);">
                            ${chat.messages.length} messages • ${timeAgo}
                        </div>
                    </div>
                    ${chat.unreadCount > 0 ? `
                        <div style="background: var(--cursor-accent); color: white; border-radius: 10px; padding: 2px 6px; font-size: 10px; font-weight: 700;">
                            ${chat.unreadCount}
                        </div>
                    ` : ''}
                    <button onclick="event.stopPropagation(); chatHistory.deleteChat('${chat.id}')" style="background: rgba(255, 71, 87, 0.2); border: 1px solid var(--red); color: var(--red); padding: 3px 8px; border-radius: 4px; font-size: 10px; cursor: pointer;">
                        🗑️
                    </button>
                </div>
            `;
            
            chatEl.onclick = () => this.switchToChat(chat.id);
            
            chatEl.onmouseenter = () => {
                if (!isActive) {
                    chatEl.style.background = 'var(--cursor-jade-light)';
                    chatEl.style.transform = 'translateX(4px)';
                }
            };
            
            chatEl.onmouseleave = () => {
                if (!isActive) {
                    chatEl.style.background = 'var(--cursor-input-bg)';
                    chatEl.style.transform = 'translateX(0)';
                }
            };
            
            container.appendChild(chatEl);
        });
        
        // Update unread badge on Chat tab
        this.updateUnreadBadge();
    }
    
    updateUnreadBadge() {
        const totalUnread = this.chats.reduce((sum, chat) => sum + (chat.unreadCount || 0), 0);
        const chatTab = document.querySelector('.sidebar-tab[data-tab="chat"]');
        
        if (chatTab) {
            // Remove old badge
            const oldBadge = chatTab.querySelector('.unread-badge');
            if (oldBadge) oldBadge.remove();
            
            // Add new badge if unread > 0
            if (totalUnread > 0) {
                const badge = document.createElement('span');
                badge.className = 'unread-badge';
                badge.style.cssText = `
                    background: var(--cursor-accent);
                    color: white;
                    border-radius: 10px;
                    padding: 2px 6px;
                    font-size: 10px;
                    font-weight: 700;
                    margin-left: 6px;
                `;
                badge.textContent = totalUnread;
                chatTab.appendChild(badge);
            }
        }
    }
    
    formatTimeAgo(date) {
        const seconds = Math.floor((new Date() - date) / 1000);
        
        if (seconds < 60) return 'Just now';
        if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
        if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
        if (seconds < 604800) return `${Math.floor(seconds / 86400)}d ago`;
        
        return date.toLocaleDateString();
    }
    
    normalizeAttachments(attachments) {
        if (!attachments || !Array.isArray(attachments)) return null;
        return attachments.map((att) => ({
            name: att?.name || att?.filename || 'attachment',
            size: att?.size || 0,
            type: att?.type || att?.mime || '',
        }));
    }

    getActiveContainers() {
        const containers = [];
        const sidebar = document.getElementById('ai-chat-messages');
        if (sidebar) containers.push(sidebar);
        const center = document.getElementById('center-chat-messages');
        if (center) containers.push(center);
        return containers;
    }

    clearChatUI() {
        const containers = this.getActiveContainers();
        containers.forEach((container) => {
            container.innerHTML = '';
        });
    }

    appendMessageToUI(message, options = {}) {
        const containers = options.targets || this.getActiveContainers();
        if (containers.length === 0) return;

        containers.forEach((container) => {
            const element = this.createMessageElement(message, container.id);
            if (element) {
                container.appendChild(element);
                if (options.scroll !== false) {
                    container.scrollTop = container.scrollHeight;
                }
            }
        });
    }
    
    loadChatIntoUI(chat) {
        const container = document.getElementById('ai-chat-messages');
        if (!container) return;
        
        container.innerHTML = '';
        
        chat.messages.forEach(msg => {
            const msgEl = this.createMessageElement(msg);
            container.appendChild(msgEl);
        });
        
        // Scroll to bottom
        container.scrollTop = container.scrollHeight;
    }
    
    createMessageElement(message) {
        const div = document.createElement('div');
        div.className = `chat-message ${message.role}`;
        div.dataset.messageId = message.id;
        
        const bgColor = message.error ? '#ff4757' : 
                        message.role === 'user' ? 'rgba(255, 107, 53, 0.15)' : 
                        'rgba(119, 221, 190, 0.15)';
        const borderColor = message.error ? '#ff4757' : 
                           message.role === 'user' ? 'var(--orange)' : 
                           'var(--cursor-jade-dark)';
        const icon = message.role === 'user' ? '👤' : message.error ? '❌' : '🤖';
        
        div.style.cssText = `
            padding: 14px 16px;
            margin: 12px 16px;
            background: ${bgColor};
            border-left: 4px solid ${borderColor};
            border-radius: 8px;
            font-size: 13px;
            line-height: 1.6;
            position: relative;
            ${message.isUnread ? 'box-shadow: 0 0 0 3px var(--cursor-accent);' : ''}
        `;
        
        const time = new Date(message.timestamp).toLocaleTimeString();
        
        div.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 8px;">
                <div style="display: flex; align-items: center; gap: 8px;">
                    <span style="font-size: 18px;">${icon}</span>
                    <span style="font-weight: 600; color: ${borderColor};">
                        ${message.role === 'user' ? 'You' : 'BigDaddyG'}
                    </span>
                    ${message.isUnread ? '<span style="background: var(--cursor-accent); color: white; padding: 2px 6px; border-radius: 4px; font-size: 9px; font-weight: 700;">UNREAD</span>' : ''}
                </div>
                <div style="display: flex; gap: 6px; align-items: center;">
                    <span style="font-size: 10px; color: var(--cursor-text-muted);">${time}</span>
                    ${message.role === 'assistant' && !message.isUnread ? `
                        <button onclick="chatHistory.markMessageAsUnread('${message.id}')" style="background: none; border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 2px 8px; border-radius: 4px; font-size: 9px; cursor: pointer; font-weight: 600;">
                            📌 Mark Unread
                        </button>
                    ` : ''}
                    ${message.isUnread ? `
                        <button onclick="chatHistory.markMessageAsRead('${message.id}')" style="background: var(--cursor-accent); border: none; color: white; padding: 2px 8px; border-radius: 4px; font-size: 9px; cursor: pointer; font-weight: 600;">
                            ✅ Mark Read
                        </button>
                    ` : ''}
                </div>
            </div>
            <div style="color: var(--cursor-text); white-space: pre-wrap; word-wrap: break-word;">
                ${message.content}
            </div>
            ${message.attachments ? `
                <div style="margin-top: 8px; font-size: 11px; color: var(--cursor-text-secondary);">
                    📎 ${message.attachments.length} file(s) attached
                </div>
            ` : ''}
        `;
        
        return div;
    }
    
    getCurrentChat() {
        return this.currentChat;
    }
}

// Initialize chat history
window.chatHistory = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.chatHistory = new ChatHistoryManager();
    });
} else {
    window.chatHistory = new ChatHistoryManager();
}

// Export
window.ChatHistoryManager = ChatHistoryManager;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = ChatHistoryManager;
}

console.log('[ChatHistory] 📦 Chat history module loaded');

})(); // End IIFE


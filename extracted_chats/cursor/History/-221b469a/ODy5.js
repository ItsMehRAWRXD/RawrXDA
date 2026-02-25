/**
 * Browser Panel - Integrated Web Browser for BigDaddyG IDE
 * Combines BrowserView and webview for maximum compatibility
 */

class BrowserPanel {
    constructor() {
        this.isVisible = false;
        this.currentUrl = 'https://www.google.com';
        this.history = [];
        this.bookmarks = this.loadBookmarks();
        this.tabs = [];
        this.activeTabId = null;
        this.mediaState = {
            isPlaying: false,
            isYouTube: false,
            title: ''
        };
        this.playPauseBtn = null;
        this.youtubeBtn = null;
        this.pipBtn = null;
        
        console.log('[BrowserPanel] 🌐 Initializing browser panel...');
        this.init();
    }
    
    init() {
        this.createBrowserPanel();
        this.setupEventListeners();
        this.loadBookmarks();

        if (window.electron?.browser?.onMediaState) {
            window.electron.browser.onMediaState((state) => {
                if (!state) return;
                this.updateVideoState({
                    isPlaying: !!state.isPlaying,
                    isYouTube: !!state.isYouTube,
                    title: state.title || this.mediaState.title
                });
            });
        }

        if (window.electron?.browser?.onLoaded) {
            window.electron.browser.onLoaded(({ url, title }) => {
                this.applyEmbeddedLocation(url, title);
            });
        }

        if (window.electron?.browser?.onLoading) {
            window.electron.browser.onLoading(({ url }) => {
                if (this.statusBar) {
                    this.statusBar.textContent = url ? `Loading ${url}...` : 'Loading...';
                }
            });
        }

        if (window.electron?.browser?.onError) {
            window.electron.browser.onError(({ error }) => {
                if (this.statusBar) {
                    this.statusBar.textContent = error ? `Error: ${error}` : 'Browser error';
                }
            });
        }

        this.syncEmbeddedBrowserState();
        
        // Register keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.shiftKey && e.key === 'B') {
                e.preventDefault();
                this.toggle();
            }
            if (e.key === 'Escape' && this.isVisible) {
                e.preventDefault();
                this.hide();
            }
        });
        
        console.log('[BrowserPanel] ✅ Browser panel ready!');
    }
    
    createBrowserPanel() {
        // Create browser panel container
        const panel = document.createElement('div');
        panel.id = 'browser-panel';
        panel.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: #1e1e1e;
            z-index: 99999;
            display: none;
            flex-direction: column;
        `;
        
        // Browser toolbar
        const toolbar = document.createElement('div');
        toolbar.className = 'browser-toolbar';
        toolbar.style.cssText = `
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 12px;
            background: #2d2d2d;
            border-bottom: 1px solid #404040;
            min-height: 50px;
        `;
        
        // Navigation controls
        const navControls = document.createElement('div');
        navControls.style.cssText = 'display: flex; gap: 4px;';
        
        const backBtn = this.createButton('←', 'Go back', () => this.goBack());
        const forwardBtn = this.createButton('→', 'Go forward', () => this.goForward());
        const refreshBtn = this.createButton('⟳', 'Refresh', () => this.refresh());
        const homeBtn = this.createButton('🏠', 'Home', () => this.goHome());
        
        navControls.appendChild(backBtn);
        navControls.appendChild(forwardBtn);
        navControls.appendChild(refreshBtn);
        navControls.appendChild(homeBtn);
        
        // Address bar
        const addressBar = document.createElement('input');
        addressBar.id = 'browser-address-bar';
        addressBar.type = 'text';
        addressBar.placeholder = 'Enter URL or search...';
        addressBar.value = this.currentUrl;
        addressBar.style.cssText = `
            flex: 1;
            padding: 8px 12px;
            background: #3c3c3c;
            border: 1px solid #555;
            border-radius: 4px;
            color: #fff;
            font-size: 14px;
            outline: none;
            margin: 0 8px;
        `;
        
        addressBar.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                this.navigate(addressBar.value);
            }
        });
        
        // Browser controls
        const controls = document.createElement('div');
        controls.style.cssText = 'display: flex; gap: 4px;';
        
        const newTabBtn = this.createButton('+', 'New Tab', () => this.newTab());
        const bookmarkBtn = this.createButton('⭐', 'Bookmark', () => this.addBookmark());
        const devToolsBtn = this.createButton('🔧', 'DevTools', () => this.toggleDevTools());
        const closeBtn = this.createButton('✕', 'Close Browser', () => this.hide());
        
        controls.appendChild(newTabBtn);
        controls.appendChild(bookmarkBtn);
        controls.appendChild(devToolsBtn);
        controls.appendChild(closeBtn);

        const mediaControls = document.createElement('div');
        mediaControls.style.cssText = 'display: flex; gap: 4px;';

        const youtubeBtn = this.createButton('📺', 'Open YouTube', () => this.openYouTube());
        const playPauseBtn = this.createButton('▶️', 'Play/Pause Video', () => this.togglePlayback());
        const pipBtn = this.createButton('📌', 'Picture-in-Picture', () => this.enterPictureInPicture());
        const playFirstBtn = this.createButton('🎬', 'Play First Result', () => this.playFirstVideo());

        mediaControls.appendChild(youtubeBtn);
        mediaControls.appendChild(playPauseBtn);
        mediaControls.appendChild(pipBtn);
        mediaControls.appendChild(playFirstBtn);

        this.youtubeBtn = youtubeBtn;
        this.playPauseBtn = playPauseBtn;
        this.pipBtn = pipBtn;
        this.playFirstBtn = playFirstBtn;
        
        toolbar.appendChild(navControls);
        toolbar.appendChild(addressBar);
        toolbar.appendChild(mediaControls);
        toolbar.appendChild(controls);
        
        // Tab bar
        const tabBar = document.createElement('div');
        tabBar.id = 'browser-tab-bar';
        tabBar.style.cssText = `
            display: flex;
            background: #2d2d2d;
            border-bottom: 1px solid #404040;
            overflow-x: auto;
            min-height: 35px;
        `;
        
        // Browser content area
        const contentArea = document.createElement('div');
        contentArea.id = 'browser-content';
        contentArea.style.cssText = `
            flex: 1;
            position: relative;
            background: #fff;
        `;
        
        // Status bar
        const statusBar = document.createElement('div');
        statusBar.id = 'browser-status';
        statusBar.style.cssText = `
            height: 25px;
            background: #2d2d2d;
            border-top: 1px solid #404040;
            display: flex;
            align-items: center;
            padding: 0 12px;
            font-size: 12px;
            color: #ccc;
        `;
        statusBar.textContent = 'Ready';
        
        panel.appendChild(toolbar);
        panel.appendChild(tabBar);
        panel.appendChild(contentArea);
        panel.appendChild(statusBar);
        
        document.body.appendChild(panel);
        
        this.panel = panel;
        this.addressBar = addressBar;
        this.tabBar = tabBar;
        this.contentArea = contentArea;
        this.statusBar = statusBar;
        
        // Create initial tab
        this.newTab();
    }
    
    createButton(text, title, onclick) {
        const btn = document.createElement('button');
        btn.textContent = text;
        btn.title = title;
        btn.onclick = onclick;
        btn.style.cssText = `
            padding: 6px 12px;
            background: #404040;
            border: 1px solid #555;
            border-radius: 4px;
            color: #fff;
            cursor: pointer;
            font-size: 14px;
            transition: background 0.2s;
            min-width: 35px;
        `;
        
        btn.onmouseenter = () => btn.style.background = '#4a4a4a';
        btn.onmouseleave = () => btn.style.background = '#404040';
        
        return btn;
    }
    
    newTab(url = 'https://www.google.com') {
        const tabId = 'tab-' + Date.now();
        
        // Create tab button
        const tab = document.createElement('div');
        tab.className = 'browser-tab';
        tab.dataset.tabId = tabId;
        tab.style.cssText = `
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 16px;
            background: #404040;
            border-right: 1px solid #555;
            cursor: pointer;
            font-size: 13px;
            color: #ccc;
            min-width: 150px;
            max-width: 200px;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        `;
        
        const favicon = document.createElement('span');
        favicon.textContent = '🌐';
        favicon.style.fontSize = '12px';
        
        const title = document.createElement('span');
        title.textContent = 'New Tab';
        title.style.flex = '1';
        
        const closeBtn = document.createElement('span');
        closeBtn.textContent = '×';
        closeBtn.style.cssText = `
            color: #999;
            cursor: pointer;
            padding: 2px 4px;
            border-radius: 2px;
        `;
        closeBtn.onclick = (e) => {
            e.stopPropagation();
            this.closeTab(tabId);
        };
        
        tab.appendChild(favicon);
        tab.appendChild(title);
        tab.appendChild(closeBtn);
        
        tab.onclick = () => this.switchTab(tabId);
        
        // Create webview
        const webview = document.createElement('webview');
        webview.id = `webview-${tabId}`;
        webview.src = url;
        webview.style.cssText = `
            width: 100%;
            height: 100%;
            display: none;
        `;
        
        // Webview event handlers
        webview.addEventListener('did-start-loading', () => {
            this.statusBar.textContent = 'Loading...';
            favicon.textContent = '⏳';
        });
        
        webview.addEventListener('did-finish-load', () => {
            const pageUrl = this.getWebviewUrl(webview);
            const pageTitle = this.getWebviewTitle(webview) || 'Untitled';
            
            title.textContent = pageTitle;
            favicon.textContent = this.getFavicon(pageUrl);
            this.statusBar.textContent = 'Done';
            
            if (this.activeTabId === tabId) {
                this.addressBar.value = pageUrl;
                this.currentUrl = pageUrl;
            }
        });
        
        webview.addEventListener('page-title-updated', (e) => {
            title.textContent = e.title;
        });
        
        webview.addEventListener('did-fail-load', (e) => {
            this.statusBar.textContent = `Failed to load: ${e.errorDescription}`;
            favicon.textContent = '❌';
        });
        
        // Add to DOM
        this.tabBar.appendChild(tab);
        this.contentArea.appendChild(webview);
        
        // Store tab data
        this.tabs.push({
            id: tabId,
            tab: tab,
            webview: webview,
            url: url,
            title: 'New Tab'
        });
        
        // Switch to new tab
        this.switchTab(tabId);
        
        return tabId;
    }
    
    switchTab(tabId) {
        // Hide all webviews and deactivate tabs
        this.tabs.forEach(tabData => {
            tabData.webview.style.display = 'none';
            tabData.tab.style.background = '#404040';
            tabData.tab.style.color = '#ccc';
        });
        
        // Show active webview and activate tab
        const activeTab = this.tabs.find(t => t.id === tabId);
        if (activeTab) {
            activeTab.webview.style.display = 'block';
            activeTab.tab.style.background = '#555';
            activeTab.tab.style.color = '#fff';

            this.activeTabId = tabId;
            const currentUrl = this.getWebviewUrl(activeTab.webview) || activeTab.url;
            this.addressBar.value = currentUrl;
            this.currentUrl = currentUrl;
        }
    }
    
    closeTab(tabId) {
        const tabIndex = this.tabs.findIndex(t => t.id === tabId);
        if (tabIndex === -1) return;
        
        const tabData = this.tabs[tabIndex];
        
        // Remove from DOM
        tabData.tab.remove();
        tabData.webview.remove();
        
        // Remove from array
        this.tabs.splice(tabIndex, 1);
        
        // If this was the active tab, switch to another
        if (this.activeTabId === tabId) {
            if (this.tabs.length > 0) {
                const newActiveTab = this.tabs[Math.max(0, tabIndex - 1)];
                this.switchTab(newActiveTab.id);
            } else {
                // No tabs left, create a new one
                this.newTab();
            }
        }
    }
    
    navigate(urlOrSearch) {
        if (!this.activeTabId) return;
        
        let url = urlOrSearch.trim();
        
        // Add protocol if missing
        if (!url.includes('://')) {
            if (url.includes('.') && !url.includes(' ')) {
                url = 'https://' + url;
            } else {
                url = `https://www.google.com/search?q=${encodeURIComponent(url)}`;
            }
        }
        
        const activeTab = this.tabs.find(t => t.id === this.activeTabId);
        if (activeTab) {
            activeTab.webview.src = url;
            this.addressBar.value = url;
            this.currentUrl = url;
        }
    }
    
    goBack() {
        const activeTab = this.tabs.find(t => t.id === this.activeTabId);
        if (activeTab && activeTab.webview.canGoBack()) {
            activeTab.webview.goBack();
        }
    }
    
    goForward() {
        const activeTab = this.tabs.find(t => t.id === this.activeTabId);
        if (activeTab && activeTab.webview.canGoForward()) {
            activeTab.webview.goForward();
        }
    }
    
    refresh() {
        const activeTab = this.tabs.find(t => t.id === this.activeTabId);
        if (activeTab) {
            activeTab.webview.reload();
        }
    }
    
    goHome() {
        this.navigate('https://www.google.com');
    }
    
    toggleDevTools() {
        const activeTab = this.tabs.find(t => t.id === this.activeTabId);
        if (activeTab) {
            if (activeTab.webview.isDevToolsOpened()) {
                activeTab.webview.closeDevTools();
            } else {
                activeTab.webview.openDevTools();
            }
        }
    }
    
    addBookmark() {
        const activeTab = this.tabs.find(t => t.id === this.activeTabId);
        if (!activeTab) return;
        
        const url = this.getWebviewUrl(activeTab.webview);
        const title = this.getWebviewTitle(activeTab.webview) || url;
        
        if (!this.bookmarks.find(b => b.url === url)) {
            this.bookmarks.push({
                title,
                url,
                date: new Date().toISOString()
            });
            this.saveBookmarks();
            
            this.showNotification('✅ Bookmarked', title);
        }
    }
    
    getFavicon(url) {
        if (!url) return '🌐';
        
        const domain = new URL(url).hostname.toLowerCase();
        
        if (domain.includes('google')) return '🔍';
        if (domain.includes('github')) return '🐙';
        if (domain.includes('stackoverflow')) return '📚';
        if (domain.includes('youtube')) return '📺';
        if (domain.includes('twitter')) return '🐦';
        if (domain.includes('facebook')) return '📘';
        if (domain.includes('reddit')) return '🤖';
        if (domain.includes('wikipedia')) return '📖';
        if (domain.includes('amazon')) return '📦';
        if (domain.includes('netflix')) return '🎬';
        
        return '🌐';
    }

    getWebviewUrl(webview) {
        if (!webview) {
            return '';
        }
        try {
            if (typeof webview.getURL === 'function') {
                return webview.getURL();
            }
        } catch (error) {
            console.warn('[BrowserPanel] ⚠️ Failed to read webview URL via getURL:', error);
        }
        return webview.src || '';
    }

    getWebviewTitle(webview) {
        if (!webview) {
            return '';
        }
        try {
            if (typeof webview.getTitle === 'function') {
                return webview.getTitle();
            }
        } catch (error) {
            console.warn('[BrowserPanel] ⚠️ Failed to read webview title via getTitle:', error);
        }
        return webview.getAttribute?.('title') || '';
    }

    async syncEmbeddedBrowserState() {
        const mediaState = await this.callElectronBrowser('getMediaState');
        if (mediaState) {
            this.updateVideoState({
                isPlaying: !!mediaState.isPlaying,
                isYouTube: !!mediaState.isYouTube,
                title: mediaState.title || ''
            });
        }
    }

    updateVideoState(state = {}) {
        this.mediaState = {
            ...this.mediaState,
            ...state
        };

        if (this.playPauseBtn) {
            this.playPauseBtn.textContent = this.mediaState.isPlaying ? '⏸️' : '▶️';
        }

        if (this.youtubeBtn) {
            this.youtubeBtn.style.opacity = this.mediaState.isYouTube ? '1' : '0.6';
        }

        if (this.statusBar) {
            if (this.mediaState.title) {
                this.statusBar.textContent = this.mediaState.title;
            } else if (!this.statusBar.textContent) {
                this.statusBar.textContent = 'Ready';
            }
        }
    }

    updateMediaState(state = {}) {
        this.updateVideoState(state);
    }

    applyEmbeddedLocation(url, title) {
        if (!url) {
            return;
        }

        this.currentUrl = url;
        if (this.addressBar) {
            this.addressBar.value = url;
        }
        if (this.statusBar) {
            this.statusBar.textContent = title ? `Loaded: ${title}` : `Loaded: ${url}`;
        }
    }

    async callElectronBrowser(methodName, ...args) {
        const browserApi = window.electron?.browser;
        if (!browserApi) {
            return null;
        }

        const method = browserApi[methodName];
        if (typeof method !== 'function') {
            return null;
        }

        try {
            return await method(...args);
        } catch (error) {
            const message = error?.message || String(error);
            if (message && message.includes('Object has been destroyed')) {
                console.warn(`[BrowserPanel] Embedded browser unavailable (${methodName})`);
            } else {
                console.warn(`[BrowserPanel] ${methodName} failed:`, message);
            }
            return null;
        }
    }
    
    async show() {
        this.panel.style.display = 'flex';
        this.isVisible = true;
        this.callElectronBrowser('show');
        console.log('[BrowserPanel] 🌐 Browser shown');
    }
    
    async hide() {
        this.panel.style.display = 'none';
        this.isVisible = false;
        this.callElectronBrowser('hide');
        console.log('[BrowserPanel] 🌐 Browser hidden');
    }
    
    toggle() {
        if (this.isVisible) {
            this.hide();
        } else {
            this.show();
        }
    }
    
    loadBookmarks() {
        try {
            const saved = localStorage.getItem('browser-bookmarks');
            return saved ? JSON.parse(saved) : [];
        } catch (error) {
            return [];
        }
    }
    
    saveBookmarks() {
        try {
            localStorage.setItem('browser-bookmarks', JSON.stringify(this.bookmarks));
        } catch (error) {
            console.error('[BrowserPanel] Failed to save bookmarks:', error);
        }
    }
    
    showNotification(title, message) {
        // Simple notification
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: #2d2d2d;
            color: #fff;
            padding: 12px 20px;
            border-radius: 6px;
            border: 1px solid #555;
            z-index: 100000;
            font-size: 14px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        `;
        notification.innerHTML = `<strong>${title}</strong><br>${message}`;
        
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.remove();
        }, 3000);
    }
    
    setupEventListeners() {
        if (window.electron?.onMenuEvent) {
            window.electron.onMenuEvent((event) => {
                switch (event) {
                    case 'show-browser':
                        this.show();
                        break;
                    case 'browser-navigate': {
                        const url = prompt('Enter URL:');
                        if (url) this.navigate(url);
                        break;
                    }
                    case 'browser-back':
                        this.goBack();
                        break;
                    case 'browser-forward':
                        this.goForward();
                        break;
                    case 'browser-reload':
                        this.refresh();
                        break;
                    case 'browser-devtools':
                        this.toggleDevTools();
                        break;
                    default:
                        break;
                }
            });
        }
    }
}

// Initialize browser panel
window.browserPanel = new BrowserPanel();

console.log('[BrowserPanel] 🌐 Browser panel module loaded');
console.log('[BrowserPanel] 💡 Usage:');
console.log('  • Ctrl+Shift+B - Toggle browser');
console.log('  • browserPanel.show() - Show browser');
console.log('  • browserPanel.navigate(url) - Navigate to URL');
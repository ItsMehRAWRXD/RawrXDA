// ========================================================
// CLEAN PROFESSIONAL IDE - 300S TLS
// ========================================================
// Remove all resizable panes, fix overlaps, clean layout

(function cleanProfessionalIDE() {
    console.log('[CLEAN IDE] 🎨 Starting professional clean layout (300s TLS)...');

    // ========================================================
    // 1. REMOVE ALL RESIZABLE PANE CODE
    // ========================================================
    function removeResizablePanes() {
        // Remove all resize handles
        const resizeHandles = document.querySelectorAll('.pane-resize-handle, .resize-handle, [class*="resize"]');
        resizeHandles.forEach(handle => handle.remove());

        // Remove pane system references
        if (window.paneSystem) {
            delete window.paneSystem;
        }

        // Disable resize functionality
        const style = document.createElement('style');
        style.id = 'disable-resize';
        style.textContent = `
            /* Disable all resize functionality */
            .pane-resize-handle,
            .resize-handle,
            [class*="resize"] {
                display: none !important;
                pointer-events: none !important;
                cursor: default !important;
            }
            
            /* Prevent manual resizing */
            * {
                resize: none !important;
            }
            
            /* Remove cursor changes */
            *:hover {
                cursor: default !important;
            }
            
            button, a, input, textarea, select, [onclick], [role="button"] {
                cursor: pointer !important;
            }
        `;
        
        document.head.appendChild(style);
        console.log('[CLEAN IDE] ✅ All resizable panes removed');
    }

    // ========================================================
    // 2. CREATE PROFESSIONAL FIXED LAYOUT
    // ========================================================
    function createProfessionalLayout() {
        const layoutCSS = `
            /* PROFESSIONAL IDE LAYOUT - NO RESIZING */
            
            /* Base reset */
            * {
                box-sizing: border-box !important;
                margin: 0 !important;
                padding: 0 !important;
            }

            html, body {
                height: 100vh !important;
                width: 100vw !important;
                overflow: hidden !important;
                font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, sans-serif !important;
                font-size: 13px !important;
                line-height: 1.4 !important;
                background: #1e1e1e !important;
                color: #cccccc !important;
            }

            /* Main container */
            .main-content {
                display: flex !important;
                height: calc(100vh - 65px) !important;
                width: 100vw !important;
                overflow: hidden !important;
                position: relative !important;
            }

            /* Activity bar - fixed 48px */
            .activity-bar {
                width: 48px !important;
                min-width: 48px !important;
                max-width: 48px !important;
                height: 100% !important;
                background: #333333 !important;
                border-right: 1px solid #444444 !important;
                display: flex !important;
                flex-direction: column !important;
                align-items: center !important;
                padding: 8px 0 !important;
                flex-shrink: 0 !important;
                z-index: 100 !important;
            }

            .activity-icon {
                width: 40px !important;
                height: 40px !important;
                display: flex !important;
                align-items: center !important;
                justify-content: center !important;
                font-size: 18px !important;
                margin: 4px 0 !important;
                border-radius: 4px !important;
                cursor: pointer !important;
                transition: background 0.2s ease !important;
                color: #cccccc !important;
            }

            .activity-icon:hover {
                background: #464647 !important;
            }

            .activity-icon.active {
                background: #37373d !important;
                border-left: 2px solid #007acc !important;
            }

            /* Sidebar - fixed 280px */
            .sidebar {
                width: 280px !important;
                min-width: 280px !important;
                max-width: 280px !important;
                height: 100% !important;
                background: #252526 !important;
                border-right: 1px solid #444444 !important;
                display: flex !important;
                flex-direction: column !important;
                flex-shrink: 0 !important;
                overflow: hidden !important;
            }

            .sidebar.collapsed {
                width: 0 !important;
                min-width: 0 !important;
                max-width: 0 !important;
                border: none !important;
                overflow: hidden !important;
            }

            .sidebar-header {
                padding: 12px 16px !important;
                background: #2d2d2d !important;
                border-bottom: 1px solid #444444 !important;
                font-weight: 600 !important;
                font-size: 13px !important;
                color: #cccccc !important;
                display: flex !important;
                align-items: center !important;
                justify-content: space-between !important;
                flex-shrink: 0 !important;
            }

            .sidebar-content {
                flex: 1 !important;
                overflow-y: auto !important;
                overflow-x: hidden !important;
                padding: 8px 0 !important;
            }

            /* Editor area - flexible center */
            .editor-area {
                flex: 1 !important;
                height: 100% !important;
                background: #1e1e1e !important;
                display: flex !important;
                flex-direction: column !important;
                overflow: hidden !important;
                min-width: 400px !important;
            }

            .editor-header {
                padding: 8px 16px !important;
                background: #2d2d2d !important;
                border-bottom: 1px solid #444444 !important;
                font-weight: 500 !important;
                font-size: 13px !important;
                color: #cccccc !important;
                display: flex !important;
                align-items: center !important;
                justify-content: space-between !important;
                flex-shrink: 0 !important;
            }

            .editor-tabs {
                background: #2d2d2d !important;
                border-bottom: 1px solid #444444 !important;
                display: flex !important;
                align-items: center !important;
                padding: 0 8px !important;
                height: 36px !important;
                overflow-x: auto !important;
                overflow-y: hidden !important;
                flex-shrink: 0 !important;
            }

            .editor-tab {
                padding: 6px 16px !important;
                background: #2d2d2d !important;
                border: 1px solid #444444 !important;
                border-bottom: none !important;
                border-radius: 4px 4px 0 0 !important;
                margin-right: 2px !important;
                cursor: pointer !important;
                font-size: 13px !important;
                color: #cccccc !important;
                white-space: nowrap !important;
                display: flex !important;
                align-items: center !important;
                gap: 8px !important;
                transition: background 0.2s ease !important;
            }

            .editor-tab:hover {
                background: #37373d !important;
            }

            .editor-tab.active {
                background: #1e1e1e !important;
                border-color: #007acc !important;
                color: #ffffff !important;
            }

            .editor-wrapper {
                flex: 1 !important;
                overflow: hidden !important;
                display: flex !important;
                flex-direction: column !important;
            }

            .code-editor {
                flex: 1 !important;
                width: 100% !important;
                background: #1e1e1e !important;
                color: #d4d4d4 !important;
                border: none !important;
                padding: 16px !important;
                font-family: 'Consolas', 'Courier New', monospace !important;
                font-size: 14px !important;
                line-height: 1.6 !important;
                resize: none !important;
                outline: none !important;
                overflow: auto !important;
                white-space: pre !important;
                tab-size: 4 !important;
            }

            /* AI panel - fixed 360px */
            .ai-panel {
                width: 360px !important;
                min-width: 360px !important;
                max-width: 360px !important;
                height: 100% !important;
                background: #252526 !important;
                border-left: 1px solid #444444 !important;
                display: flex !important;
                flex-direction: column !important;
                flex-shrink: 0 !important;
                overflow: hidden !important;
            }

            .ai-panel.collapsed {
                width: 0 !important;
                min-width: 0 !important;
                max-width: 0 !important;
                border: none !important;
                overflow: hidden !important;
            }

            .ai-header {
                padding: 12px 16px !important;
                background: #2d2d2d !important;
                border-bottom: 1px solid #444444 !important;
                flex-shrink: 0 !important;
                max-height: 200px !important;
                overflow-y: auto !important;
            }

            .chat-messages {
                flex: 1 !important;
                overflow-y: auto !important;
                overflow-x: hidden !important;
                padding: 16px !important;
                display: flex !important;
                flex-direction: column !important;
                gap: 12px !important;
            }

            .chat-message {
                padding: 12px 16px !important;
                border-radius: 8px !important;
                max-width: 90% !important;
                word-wrap: break-word !important;
                line-height: 1.5 !important;
                font-size: 13px !important;
            }

            .chat-message.user {
                background: #0e639c !important;
                color: #ffffff !important;
                align-self: flex-end !important;
                margin-left: auto !important;
            }

            .chat-message.assistant {
                background: #2d2d2d !important;
                color: #cccccc !important;
                align-self: flex-start !important;
                margin-right: auto !important;
            }

            .ai-input-area {
                padding: 16px !important;
                background: #252526 !important;
                border-top: 1px solid #444444 !important;
                flex-shrink: 0 !important;
            }

            .ai-input-wrapper {
                display: flex !important;
                gap: 8px !important;
                align-items: flex-end !important;
            }

            .ai-input {
                flex: 1 !important;
                background: #2d2d2d !important;
                border: 1px solid #444444 !important;
                color: #cccccc !important;
                padding: 10px 12px !important;
                border-radius: 6px !important;
                font-size: 13px !important;
                font-family: inherit !important;
                resize: vertical !important;
                min-height: 38px !important;
                max-height: 120px !important;
                outline: none !important;
                transition: border-color 0.2s ease !important;
            }

            .ai-input:focus {
                border-color: #007acc !important;
                box-shadow: 0 0 0 1px #007acc !important;
            }

            /* Bottom panel - fixed 240px when visible */
            .bottom-panel {
                position: fixed !important;
                bottom: 0 !important;
                left: 0 !important;
                right: 0 !important;
                height: 240px !important;
                background: #1e1e1e !important;
                border-top: 1px solid #444444 !important;
                display: none !important;
                flex-direction: column !important;
                z-index: 200 !important;
            }

            .bottom-panel.visible {
                display: flex !important;
            }

            /* Status bar */
            .status-bar {
                position: fixed !important;
                bottom: 0 !important;
                left: 0 !important;
                right: 0 !important;
                height: 22px !important;
                background: #007acc !important;
                color: #ffffff !important;
                display: flex !important;
                align-items: center !important;
                padding: 0 16px !important;
                font-size: 12px !important;
                gap: 16px !important;
                z-index: 300 !important;
            }

            /* Buttons */
            button, .btn {
                background: #0e639c !important;
                border: 1px solid #007acc !important;
                color: #ffffff !important;
                padding: 6px 12px !important;
                border-radius: 4px !important;
                cursor: pointer !important;
                font-size: 12px !important;
                font-weight: 500 !important;
                transition: background 0.2s ease !important;
            }

            button:hover, .btn:hover {
                background: #1177bb !important;
            }

            button:active, .btn:active {
                background: #005a9e !important;
            }

            button:disabled, .btn:disabled {
                background: #464647 !important;
                border-color: #464647 !important;
                color: #969696 !important;
                cursor: not-allowed !important;
            }

            /* Inputs */
            input, textarea, select {
                background: #2d2d2d !important;
                border: 1px solid #444444 !important;
                color: #cccccc !important;
                padding: 6px 8px !important;
                border-radius: 4px !important;
                font-size: 13px !important;
                outline: none !important;
            }

            input:focus, textarea:focus, select:focus {
                border-color: #007acc !important;
                box-shadow: 0 0 0 1px #007acc !important;
            }

            /* Scrollbars */
            ::-webkit-scrollbar {
                width: 12px !important;
                height: 12px !important;
            }

            ::-webkit-scrollbar-track {
                background: #2d2d2d !important;
            }

            ::-webkit-scrollbar-thumb {
                background: #464647 !important;
                border-radius: 6px !important;
                border: 2px solid #2d2d2d !important;
            }

            ::-webkit-scrollbar-thumb:hover {
                background: #5a5a5a !important;
            }

            ::-webkit-scrollbar-corner {
                background: #2d2d2d !important;
            }

            /* File tree */
            .file-tree-item {
                padding: 4px 12px !important;
                cursor: pointer !important;
                color: #cccccc !important;
                font-size: 13px !important;
                display: flex !important;
                align-items: center !important;
                gap: 8px !important;
                transition: background 0.2s ease !important;
            }

            .file-tree-item:hover {
                background: #2d2d2d !important;
            }

            .file-tree-item.selected {
                background: #094771 !important;
                color: #ffffff !important;
            }

            /* Mode buttons */
            .mode-btn, .quality-btn {
                background: #2d2d2d !important;
                border: 1px solid #444444 !important;
                color: #cccccc !important;
                padding: 4px 8px !important;
                border-radius: 4px !important;
                cursor: pointer !important;
                font-size: 11px !important;
                transition: all 0.2s ease !important;
            }

            .mode-btn.active, .quality-btn.active {
                background: #0e639c !important;
                border-color: #007acc !important;
                color: #ffffff !important;
            }

            .mode-btn:hover, .quality-btn:hover {
                background: #37373d !important;
            }

            /* Remove all floating/overlay features */
            .ai-panel.floating,
            .floating,
            [class*="float"],
            [class*="overlay"],
            .modal,
            .dialog {
                position: static !important;
                top: auto !important;
                right: auto !important;
                left: auto !important;
                bottom: auto !important;
                transform: none !important;
                z-index: auto !important;
                box-shadow: none !important;
                border-radius: 0 !important;
                resize: none !important;
            }

            /* Disable problematic features */
            .draggable,
            .resizable,
            [draggable="true"] {
                pointer-events: none !important;
                user-select: none !important;
                -webkit-user-select: none !important;
                cursor: default !important;
            }

            /* Clean animations */
            * {
                transition: none !important;
                animation: none !important;
            }

            button, .btn, .mode-btn, .quality-btn, .activity-icon, .editor-tab, .file-tree-item {
                transition: background 0.2s ease !important;
            }
        `;

        const style = document.createElement('style');
        style.id = 'clean-professional-layout';
        style.textContent = layoutCSS;
        
        const old = document.getElementById('clean-professional-layout');
        if (old) old.remove();
        
        document.head.appendChild(style);
        console.log('[CLEAN IDE] ✅ Professional layout applied');
    }

    // ========================================================
    // 3. REMOVE PROBLEMATIC FEATURES
    // ========================================================
    function removeProblematicFeatures() {
        // Remove floating panels
        const floatingElements = document.querySelectorAll('.floating, .ai-panel.floating, [class*="float"]');
        floatingElements.forEach(el => {
            el.classList.remove('floating');
            el.style.position = 'static';
            el.style.top = 'auto';
            el.style.right = 'auto';
            el.style.transform = 'none';
            el.style.zIndex = 'auto';
        });

        // Remove drag functionality
        const draggableElements = document.querySelectorAll('[draggable="true"], .draggable');
        draggableElements.forEach(el => {
            el.draggable = false;
            el.classList.remove('draggable');
        });

        // Disable context menus that might cause issues
        document.addEventListener('contextmenu', function(e) {
            if (!e.target.matches('input, textarea')) {
                e.preventDefault();
            }
        });

        console.log('[CLEAN IDE] ✅ Problematic features removed');
    }

    // ========================================================
    // 4. ENSURE CLEAN STATE
    // ========================================================
    function ensureCleanState() {
        // Remove old fix stylesheets that might conflict
        const oldStyles = document.querySelectorAll('style[id*="fix"], style[id*="overhaul"], style[id*="resize"]');
        oldStyles.forEach(style => {
            if (style.id !== 'clean-professional-layout' && style.id !== 'disable-resize') {
                style.remove();
            }
        });

        // Reset panel states
        const panels = ['sidebar-panel', 'editor-panel', 'ai-panel'];
        panels.forEach(id => {
            const panel = document.getElementById(id);
            if (panel) {
                panel.style.width = '';
                panel.style.height = '';
                panel.style.minWidth = '';
                panel.style.maxWidth = '';
                panel.style.flex = '';
                panel.style.transform = '';
                panel.style.position = '';
                panel.style.top = '';
                panel.style.right = '';
                panel.style.left = '';
                panel.style.bottom = '';
                panel.style.zIndex = '';
            }
        });

        // Clean body
        document.body.style.margin = '0';
        document.body.style.padding = '0';
        document.body.style.overflow = 'hidden';

        console.log('[CLEAN IDE] ✅ Clean state ensured');
    }

    // ========================================================
    // 5. ADD PROFESSIONAL INTERACTIONS
    // ========================================================
    function addProfessionalInteractions() {
        // Toggle sidebar
        window.toggleSidebar = function() {
            const sidebar = document.querySelector('.sidebar');
            if (sidebar) {
                sidebar.classList.toggle('collapsed');
            }
        };

        // Toggle AI panel
        window.toggleAIPanel = function() {
            const panel = document.querySelector('.ai-panel');
            if (panel) {
                panel.classList.toggle('collapsed');
            }
        };

        // Toggle bottom panel
        window.toggleBottomPanel = function() {
            const panel = document.querySelector('.bottom-panel');
            if (panel) {
                panel.classList.toggle('visible');
            }
        };

        // Update activity bar clicks
        document.querySelectorAll('.activity-icon').forEach((icon, index) => {
            icon.onclick = function() {
                // Remove active from all
                document.querySelectorAll('.activity-icon').forEach(i => i.classList.remove('active'));
                // Add active to clicked
                this.classList.add('active');
                
                // Toggle appropriate panel
                switch(index) {
                    case 0: window.toggleSidebar(); break;
                    case 1: window.toggleBottomPanel(); break;
                    case 2: window.toggleAIPanel(); break;
                }
            };
        });

        console.log('[CLEAN IDE] ✅ Professional interactions added');
    }

    // ========================================================
    // 6. EXECUTE ALL CLEANUPS
    // ========================================================
    function executeAllCleanups() {
        removeResizablePanes();
        createProfessionalLayout();
        removeProblematicFeatures();
        ensureCleanState();
        addProfessionalInteractions();

        // Force layout recalculation
        document.body.style.display = 'none';
        document.body.offsetHeight;
        document.body.style.display = '';

        console.log('[CLEAN IDE] 🎉 CLEAN PROFESSIONAL IDE COMPLETE!');
        console.log('[CLEAN IDE] ✅ Fixed layout: 48px + 280px + flex + 360px');
        console.log('[CLEAN IDE] ✅ No overlaps, no resizing, professional');
        
        // Success notification
        setTimeout(() => {
            if (typeof showToast === 'function') {
                showToast('✅ Professional IDE Layout Applied', 'success', 3000);
            }
        }, 500);
    }

    // Run immediately
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', executeAllCleanups);
    } else {
        executeAllCleanups();
    }

    // Run multiple times to ensure cleanup
    setTimeout(executeAllCleanups, 100);
    setTimeout(executeAllCleanups, 500);
    setTimeout(executeAllCleanups, 1000);

    // Expose globally
    window.cleanProfessionalIDE = executeAllCleanups;

})();

console.log('%c🎨 CLEAN PROFESSIONAL IDE LOADED (300s TLS)', 'color: #007acc; font-size: 16px; font-weight: bold;');
console.log('%c✅ No resizing, no overlaps, clean professional layout', 'color: #4ec9b0; font-size: 12px;');
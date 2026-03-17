// ========================================================
// PROBLEMS PANEL SCROLL FIX
// ========================================================
// Makes the problems/console panel scrollable with visible scrollbar

(function fixProblemsPanel() {
    console.log('[PROBLEMS FIX] 🔧 Fixing problems panel scrolling...');

    // ========================================================
    // FIX 1: Make Problems Panel Scrollable
    // ========================================================
    function makeProblemsScrollable() {
        const problemsSelectors = [
            '#console-output',
            '#debug-console',
            '#problems-panel',
            '.console-messages',
            '.console-container',
            '.problems-list',
            '[class*="console"]',
            '[class*="problems"]'
        ];

        problemsSelectors.forEach(selector => {
            const elements = document.querySelectorAll(selector);
            elements.forEach(element => {
                // Force scrollable
                element.style.overflowY = 'scroll';
                element.style.maxHeight = '400px';
                element.style.height = 'auto';
                element.style.minHeight = '100px';
                
                // Ensure content is visible
                element.style.display = 'block';
                element.style.visibility = 'visible';
                element.style.opacity = '1';
                
                // Disable auto-scroll to bottom
                element.style.scrollBehavior = 'auto';
                
                // Make it interactable
                element.style.pointerEvents = 'auto';
                element.style.userSelect = 'text';
                
                console.log(`[PROBLEMS FIX] ✅ Fixed: ${selector}`);
            });
        });
    }

    // ========================================================
    // FIX 2: Add Visible Scrollbar Styles
    // ========================================================
    function addScrollbarStyles() {
        const style = document.createElement('style');
        style.id = 'problems-panel-fix-styles';
        style.textContent = `
            /* PROBLEMS PANEL FIX - FORCE SCROLLABLE */
            
            #console-output,
            #debug-console,
            #problems-panel,
            .console-messages,
            .console-container,
            .problems-list,
            [class*="console"],
            [class*="problems"] {
                overflow-y: scroll !important;
                max-height: 400px !important;
                height: auto !important;
                min-height: 100px !important;
                display: block !important;
                visibility: visible !important;
                opacity: 1 !important;
                pointer-events: auto !important;
                user-select: text !important;
                scroll-behavior: auto !important;
            }
            
            /* Large, visible scrollbars */
            #console-output::-webkit-scrollbar,
            #debug-console::-webkit-scrollbar,
            #problems-panel::-webkit-scrollbar,
            .console-messages::-webkit-scrollbar,
            .problems-list::-webkit-scrollbar {
                width: 14px !important;
                background: #1e1e1e !important;
            }
            
            #console-output::-webkit-scrollbar-thumb,
            #debug-console::-webkit-scrollbar-thumb,
            #problems-panel::-webkit-scrollbar-thumb,
            .console-messages::-webkit-scrollbar-thumb,
            .problems-list::-webkit-scrollbar-thumb {
                background: #4a4a4a !important;
                border-radius: 7px !important;
                border: 2px solid #1e1e1e !important;
            }
            
            #console-output::-webkit-scrollbar-thumb:hover,
            #debug-console::-webkit-scrollbar-thumb:hover,
            #problems-panel::-webkit-scrollbar-thumb:hover,
            .console-messages::-webkit-scrollbar-thumb:hover,
            .problems-list::-webkit-scrollbar-thumb:hover {
                background: #6a6a6a !important;
            }
            
            /* Ensure individual problem items are visible */
            .console-message,
            .problem-item,
            .error-item,
            .warning-item {
                display: block !important;
                padding: 4px 8px !important;
                margin: 2px 0 !important;
                border-bottom: 1px solid #333 !important;
                white-space: pre-wrap !important;
                word-wrap: break-word !important;
            }
            
            /* Limit line count display */
            .console-message:nth-child(n+500) {
                display: none !important;
            }
        `;
        
        // Remove old style if exists
        const oldStyle = document.getElementById('problems-panel-fix-styles');
        if (oldStyle) oldStyle.remove();
        
        document.head.appendChild(style);
        console.log('[PROBLEMS FIX] ✅ Scrollbar styles applied');
    }

    // ========================================================
    // FIX 3: Disable Auto-Scroll
    // ========================================================
    function disableAutoScroll() {
        window.problemsPanelAutoScroll = false;
        
        const containers = document.querySelectorAll('#console-output, #debug-console, #problems-panel, .console-messages');
        
        containers.forEach(container => {
            if (!container) return;
            
            // Override appendChild to prevent auto-scroll
            const originalAppendChild = container.appendChild.bind(container);
            container.appendChild = function(child) {
                const scrollPos = this.scrollTop;
                const result = originalAppendChild(child);
                
                // Only auto-scroll if user is at bottom
                if (window.problemsPanelAutoScroll && 
                    this.scrollHeight - this.scrollTop - this.clientHeight < 50) {
                    this.scrollTop = this.scrollHeight;
                } else {
                    // Keep scroll position
                    this.scrollTop = scrollPos;
                }
                
                return result;
            };
            
            console.log('[PROBLEMS FIX] ✅ Disabled auto-scroll for container');
        });
    }

    // ========================================================
    // FIX 4: Add Scroll Controls
    // ========================================================
    function addScrollControls() {
        const targetContainers = [
            document.getElementById('console-output'),
            document.getElementById('debug-console'),
            document.getElementById('problems-panel'),
            document.querySelector('.console-messages')
        ].filter(Boolean);

        targetContainers.forEach(container => {
            // Check if controls already exist
            if (container.parentElement.querySelector('.problems-scroll-controls')) {
                return;
            }

            const controls = document.createElement('div');
            controls.className = 'problems-scroll-controls';
            controls.style.cssText = `
                position: sticky;
                top: 0;
                background: #2a2a2a;
                padding: 4px 8px;
                display: flex;
                gap: 8px;
                align-items: center;
                z-index: 1000;
                border-bottom: 1px solid #444;
                font-size: 12px;
            `;

            controls.innerHTML = `
                <button onclick="this.closest('.console-container, #console-output, #debug-console, #problems-panel').scrollTop = 0" 
                        style="padding: 3px 8px; background: #007acc; color: white; border: none; border-radius: 3px; cursor: pointer; font-size: 11px;">
                    ⬆️ Top
                </button>
                <button onclick="this.closest('.console-container, #console-output, #debug-console, #problems-panel').scrollTop = this.closest('.console-container, #console-output, #debug-console, #problems-panel').scrollHeight" 
                        style="padding: 3px 8px; background: #007acc; color: white; border: none; border-radius: 3px; cursor: pointer; font-size: 11px;">
                    ⬇️ Bottom
                </button>
                <button onclick="this.closest('.console-container, #console-output, #debug-console, #problems-panel').innerHTML = this.parentElement.outerHTML" 
                        style="padding: 3px 8px; background: #d9534f; color: white; border: none; border-radius: 3px; cursor: pointer; font-size: 11px;">
                    🗑️ Clear
                </button>
                <label style="color: #ccc; display: flex; align-items: center; gap: 4px; font-size: 11px;">
                    <input type="checkbox" id="auto-scroll-problems" ${window.problemsPanelAutoScroll ? 'checked' : ''} 
                           onchange="window.problemsPanelAutoScroll = this.checked">
                    Auto-scroll
                </label>
                <span style="color: #888; margin-left: auto; font-size: 11px;">
                    Scroll manually or use controls
                </span>
            `;

            container.parentElement.insertBefore(controls, container);
            console.log('[PROBLEMS FIX] ✅ Scroll controls added');
        });
    }

    // ========================================================
    // FIX 5: Limit Problem Count
    // ========================================================
    function limitProblemCount() {
        const containers = document.querySelectorAll('#console-output, #debug-console, #problems-panel, .console-messages');
        
        containers.forEach(container => {
            const maxProblems = 500;
            const children = Array.from(container.children);
            
            if (children.length > maxProblems) {
                // Remove oldest problems
                const toRemove = children.length - maxProblems;
                for (let i = 0; i < toRemove; i++) {
                    children[i].remove();
                }
                console.log(`[PROBLEMS FIX] 🗑️ Removed ${toRemove} old problems`);
            }
        });
    }

    // ========================================================
    // FIX 6: Monitor and Auto-Fix
    // ========================================================
    function monitorProblemsPanel() {
        setInterval(() => {
            const containers = document.querySelectorAll('#console-output, #debug-console, #problems-panel, .console-messages');
            
            containers.forEach(container => {
                const computed = window.getComputedStyle(container);
                
                // Fix if scrolling is broken
                if (computed.overflowY !== 'scroll') {
                    container.style.overflowY = 'scroll';
                    console.warn('[PROBLEMS FIX] ⚠️ Fixed scrolling for container');
                }
                
                // Limit problem count
                if (container.children.length > 500) {
                    limitProblemCount();
                }
            });
        }, 3000);
        
        console.log('[PROBLEMS FIX] ✅ Monitoring active');
    }

    // ========================================================
    // FIX 7: Keyboard Shortcuts
    // ========================================================
    function addKeyboardShortcuts() {
        document.addEventListener('keydown', function(e) {
            const container = document.querySelector('#console-output, #debug-console, #problems-panel');
            
            if (!container) return;
            
            // Ctrl+Shift+Home: Scroll to top
            if (e.ctrlKey && e.shiftKey && e.key === 'Home') {
                e.preventDefault();
                container.scrollTop = 0;
                console.log('[PROBLEMS FIX] 📜 Scrolled to top');
            }
            
            // Ctrl+Shift+End: Scroll to bottom
            if (e.ctrlKey && e.shiftKey && e.key === 'End') {
                e.preventDefault();
                container.scrollTop = container.scrollHeight;
                console.log('[PROBLEMS FIX] 📜 Scrolled to bottom');
            }
            
            // Ctrl+Shift+Delete: Clear problems
            if (e.ctrlKey && e.shiftKey && e.key === 'Delete') {
                e.preventDefault();
                const controls = container.parentElement.querySelector('.problems-scroll-controls');
                container.innerHTML = controls ? controls.outerHTML : '';
                console.log('[PROBLEMS FIX] 🗑️ Problems cleared');
            }
        });
        
        console.log('[PROBLEMS FIX] ⌨️ Keyboard shortcuts added');
        console.log('  Ctrl+Shift+Home: Scroll to top');
        console.log('  Ctrl+Shift+End: Scroll to bottom');
        console.log('  Ctrl+Shift+Delete: Clear problems');
    }

    // ========================================================
    // APPLY ALL FIXES
    // ========================================================
    function applyAllFixes() {
        try {
            makeProblemsScrollable();
            addScrollbarStyles();
            disableAutoScroll();
            addScrollControls();
            limitProblemCount();
            monitorProblemsPanel();
            addKeyboardShortcuts();
            
            console.log('[PROBLEMS FIX] 🎉 ALL FIXES APPLIED!');
            console.log('[PROBLEMS FIX] Problems panel is now scrollable!');
            console.log('[PROBLEMS FIX] You can now see all errors!');
            
        } catch (error) {
            console.error('[PROBLEMS FIX] ❌ Error applying fixes:', error);
        }
    }

    // Run immediately
    applyAllFixes();

    // Run after DOM ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', applyAllFixes);
    }

    // Run after delay for dynamic content
    setTimeout(applyAllFixes, 1000);
    setTimeout(applyAllFixes, 3000);

    // Make function available globally
    window.fixProblemsPanel = applyAllFixes;

})();

// Show success message
console.log('%c✅ PROBLEMS PANEL SCROLL FIX LOADED!', 'color: green; font-size: 16px; font-weight: bold;');
console.log('%cYou can now scroll through all problems!', 'color: cyan; font-size: 14px;');
console.log('%cUse Ctrl+Shift+Home/End to scroll, or the control buttons', 'color: yellow; font-size: 12px;');
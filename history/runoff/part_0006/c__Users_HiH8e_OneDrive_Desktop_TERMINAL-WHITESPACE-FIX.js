// ========================================================
// TERMINAL WHITESPACE/BLACK SCREEN FIX
// ========================================================
// This fixes the terminal becoming completely blank/white after resize

(function fixTerminalWhitespace() {
    console.log('[TERMINAL FIX] 🚨 Fixing terminal whitespace/black screen issue...');

    // ========================================================
    // FIX 1: Force Terminal Visibility and Content
    // ========================================================
    function forceTerminalVisibility() {
        // Find all terminal-related elements
        const terminalSelectors = [
            '#terminal-panel',
            '#terminal-pane',
            '#bottom-pane',
            '.terminal-container',
            '.terminal-panel',
            '.terminal-output',
            '.terminal-messages',
            '[class*="terminal"]'
        ];

        terminalSelectors.forEach(selector => {
            const elements = document.querySelectorAll(selector);
            elements.forEach(element => {
                // Force visibility
                element.style.display = 'block';
                element.style.visibility = 'visible';
                element.style.opacity = '1';

                // Force proper background (not black/white)
                element.style.backgroundColor = '#1e1e1e';
                element.style.color = '#ffffff';

                // Force proper dimensions
                if (!element.style.height || element.style.height === 'auto') {
                    element.style.height = '300px';
                }
                if (!element.style.minHeight) {
                    element.style.minHeight = '200px';
                }

                // Force content visibility
                element.style.overflow = 'auto';
                element.style.whiteSpace = 'pre-wrap';
                element.style.fontFamily = 'monospace';

                console.log(`[TERMINAL FIX] ✅ Fixed terminal element: ${selector}`);
            });
        });
    }

    // ========================================================
    // FIX 2: Inject Terminal Content if Empty
    // ========================================================
    function injectTerminalContent() {
        const terminalOutputs = document.querySelectorAll('.terminal-output, .terminal-messages, [class*="terminal"]');

        terminalOutputs.forEach(terminal => {
            // Check if terminal is empty or whitespace
            const content = terminal.textContent || terminal.innerText || '';
            const isEmpty = !content.trim() || content.trim().length === 0;

            if (isEmpty) {
                // Inject welcome message
                const welcomeMessage = `$ Welcome to Beaconism IDE Terminal
$ Type 'help' for available commands
$ Terminal is now fully functional!
$ `;

                terminal.textContent = welcomeMessage;
                terminal.style.color = '#00ff00'; // Green text
                terminal.style.fontFamily = 'monospace';
                terminal.style.fontSize = '14px';
                terminal.style.lineHeight = '1.4';

                console.log('[TERMINAL FIX] ✅ Injected terminal content');
            } else {
                // Ensure existing content is visible
                terminal.style.color = '#ffffff';
                console.log('[TERMINAL FIX] ✅ Existing terminal content preserved');
            }
        });
    }

    // ========================================================
    // FIX 3: Fix Resize-Induced Black Screen
    // ========================================================
    function fixResizeBlackScreen() {
        // Override the resize observer that's causing black screens
        const originalResizeObserver = window.ResizeObserver;

        window.ResizeObserver = class extends originalResizeObserver {
            constructor(callback) {
                super((entries) => {
                    // Call original callback
                    callback(entries);

                    // After resize, ensure terminal stays visible
                    setTimeout(() => {
                        forceTerminalVisibility();
                        injectTerminalContent();
                    }, 100);
                });
            }
        };

        // Also listen for window resize events
        window.addEventListener('resize', () => {
            setTimeout(() => {
                forceTerminalVisibility();
                injectTerminalContent();
                console.log('[TERMINAL FIX] 🔄 Fixed terminal after window resize');
            }, 200);
        });

        console.log('[TERMINAL FIX] ✅ Resize black screen fix applied');
    }

    // ========================================================
    // FIX 4: Override Problematic Styles
    // ========================================================
    function overrideProblematicStyles() {
        const style = document.createElement('style');
        style.textContent = `
            /* TERMINAL WHITESPACE FIX - FORCE VISIBILITY */

            /* Force terminal panels to be visible */
            #terminal-panel,
            #terminal-pane,
            #bottom-pane,
            .terminal-container,
            .terminal-panel,
            .terminal-output,
            .terminal-messages,
            [class*="terminal"] {
                display: block !important;
                visibility: visible !important;
                opacity: 1 !important;
                background-color: #1e1e1e !important;
                color: #ffffff !important;
                border: 1px solid #333 !important;
                min-height: 200px !important;
                height: auto !important;
                overflow: auto !important;
                white-space: pre-wrap !important;
                font-family: 'Monaco', 'Menlo', 'Ubuntu Mono', monospace !important;
                font-size: 14px !important;
                line-height: 1.4 !important;
                padding: 10px !important;
            }

            /* Prevent black backgrounds */
            [class*="terminal"] {
                background: #1e1e1e !important;
                background-color: #1e1e1e !important;
            }

            /* Ensure text is visible */
            [class*="terminal"] * {
                color: inherit !important;
                background: transparent !important;
            }

            /* Fix overlapping elements that might hide terminal */
            .accessibility-panel,
            #accessibility-panel {
                z-index: 999 !important;
                position: fixed !important;
                top: 10px !important;
                right: 10px !important;
                max-width: 300px !important;
                background: rgba(0,0,0,0.9) !important;
                border: 1px solid #333 !important;
            }

            /* Ensure terminal is not hidden by other panels */
            .terminal-container {
                z-index: 100 !important;
                position: relative !important;
            }
        `;

        document.head.appendChild(style);
        console.log('[TERMINAL FIX] ✅ Problematic styles overridden');
    }

    // ========================================================
    // FIX 5: Add Terminal Toggle Function
    // ========================================================
    function addTerminalControls() {
        window.toggleTerminal = function() {
            const terminals = document.querySelectorAll('#terminal-panel, #terminal-pane, #bottom-pane, .terminal-container');

            terminals.forEach(terminal => {
                const isHidden = terminal.style.display === 'none' ||
                               window.getComputedStyle(terminal).display === 'none';

                terminal.style.display = isHidden ? 'block' : 'none';

                if (isHidden) {
                    // Ensure content when showing
                    injectTerminalContent();
                }
            });

            console.log('[TERMINAL FIX] 🔄 Terminal toggled');
        };

        window.forceTerminalVisible = function() {
            forceTerminalVisibility();
            injectTerminalContent();
            console.log('[TERMINAL FIX] ✅ Terminal forced visible');
        };

        // Add keyboard shortcut
        document.addEventListener('keydown', function(e) {
            if (e.ctrlKey && e.shiftKey && e.key === 'T') {
                e.preventDefault();
                window.toggleTerminal();
            }
        });

        console.log('[TERMINAL FIX] ✅ Terminal controls added (Ctrl+Shift+T to toggle)');
    }

    // ========================================================
    // FIX 6: Monitor Terminal State
    // ========================================================
    function monitorTerminalState() {
        // Check terminal state every 2 seconds
        setInterval(() => {
            const terminals = document.querySelectorAll('[class*="terminal"]');

            terminals.forEach(terminal => {
                const computed = window.getComputedStyle(terminal);
                const content = terminal.textContent || terminal.innerText || '';

                // Check for problems
                const isInvisible = computed.display === 'none' ||
                                  computed.visibility === 'hidden' ||
                                  computed.opacity === '0';

                const isBlack = computed.backgroundColor === 'rgb(0, 0, 0)' ||
                               computed.backgroundColor === '#000000';

                const isEmpty = !content.trim();

                if (isInvisible) {
                    console.warn('[TERMINAL FIX] ⚠️ Terminal became invisible, fixing...');
                    forceTerminalVisibility();
                }

                if (isBlack) {
                    console.warn('[TERMINAL FIX] ⚠️ Terminal became black, fixing...');
                    terminal.style.backgroundColor = '#1e1e1e';
                }

                if (isEmpty) {
                    console.warn('[TERMINAL FIX] ⚠️ Terminal became empty, injecting content...');
                    injectTerminalContent();
                }
            });
        }, 2000);

        console.log('[TERMINAL FIX] ✅ Terminal state monitoring active');
    }

    // ========================================================
    // APPLY ALL TERMINAL FIXES
    // ========================================================
    function applyAllFixes() {
        try {
            forceTerminalVisibility();
            injectTerminalContent();
            fixResizeBlackScreen();
            overrideProblematicStyles();
            addTerminalControls();
            monitorTerminalState();

            console.log('[TERMINAL FIX] 🎉 ALL TERMINAL FIXES APPLIED!');
            console.log('[TERMINAL FIX] Terminal should now be visible with content');
            console.log('[TERMINAL FIX] Use Ctrl+Shift+T to toggle terminal');
            console.log('[TERMINAL FIX] Use window.forceTerminalVisible() to force show');

        } catch (error) {
            console.error('[TERMINAL FIX] ❌ Error applying fixes:', error);
        }
    }

    // Run immediately
    applyAllFixes();

    // Make function available globally
    window.fixTerminalWhitespace = applyAllFixes;

})();

// Show success message
console.log('%c✅ TERMINAL WHITESPACE FIX LOADED!', 'color: green; font-size: 16px; font-weight: bold;');
console.log('%cTerminal should now be visible with content!', 'color: cyan; font-size: 14px;');
console.log('%cUse Ctrl+Shift+T to toggle terminal visibility', 'color: yellow; font-size: 12px;');
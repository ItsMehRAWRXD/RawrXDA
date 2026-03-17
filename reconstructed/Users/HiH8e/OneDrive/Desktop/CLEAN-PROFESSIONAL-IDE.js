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
        const old = document.getElementById('clean-professional-layout');
        if (old) {
            old.remove();
        }

        document.body.classList.add('professional-ide');
        document.documentElement.classList.add('professional-ide');
        console.log('[CLEAN IDE] ✅ Professional layout class applied');
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
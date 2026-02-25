/**
 * BigDaddyG IDE - Resizable Panes System
 * Make all panes resizable with draggable dividers
 * Supports: Left/Right sidebars, horizontal/vertical splits, persistent sizing
 */

(function() {
'use strict';

class ResizablePanes {
    constructor() {
        this.isDragging = false;
        this.currentDivider = null;
        this.dividers = new Map();
        this.minPaneSize = 150; // Minimum 150px
        this.maxPaneSize = 80; // Maximum 80% of viewport
        
        // Saved sizes (load from localStorage)
        this.savedSizes = this.loadSavedSizes();
        
        console.log('[ResizablePanes] 🎯 Initializing resizable panes...');
        this.init();
    }
    
    init() {
        // Wait for DOM to be ready
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.setup());
        } else {
            this.setup();
        }
    }
    
    setup() {
        // Create dividers for all panes
        this.createLeftDivider();
        this.createRightDivider();
        this.createHorizontalDivider();
        
        // Apply saved sizes
        this.applySavedSizes();
        
        // Register global mouse handlers
        this.registerGlobalHandlers();
        
        console.log('[ResizablePanes] ✅ Resizable panes ready');
        console.log('[ResizablePanes] 📏 Drag dividers to resize, double-click to reset');
    }
    
    createLeftDivider() {
        // Divider between left sidebar and center content
        const divider = document.createElement('div');
        divider.id = 'resize-divider-left';
        divider.className = 'resize-divider resize-divider-vertical';
        divider.style.cssText = `
            position: fixed;
            left: 280px;
            top: 0;
            width: 4px;
            height: 100vh;
            background: transparent;
            cursor: ew-resize;
            z-index: 10000;
            transition: background 0.2s;
        `;
        
        // Visual indicator on hover
        divider.addEventListener('mouseenter', () => {
            divider.style.background = 'var(--cursor-jade-dark)';
        });
        
        divider.addEventListener('mouseleave', () => {
            if (!this.isDragging) {
                divider.style.background = 'transparent';
            }
        });
        
        // Double-click to reset
        divider.addEventListener('dblclick', () => {
            this.resetLeftPane();
        });
        
        divider.addEventListener('mousedown', (e) => {
            e.preventDefault();
            this.startDragging('left', e);
        });
        
        document.body.appendChild(divider);
        this.dividers.set('left', divider);
        
        console.log('[ResizablePanes] ✅ Left divider created');
    }
    
    createRightDivider() {
        // Divider between center content and right editor
        const divider = document.createElement('div');
        divider.id = 'resize-divider-right';
        divider.className = 'resize-divider resize-divider-vertical';
        divider.style.cssText = `
            position: fixed;
            right: 50%;
            top: 0;
            width: 4px;
            height: 100vh;
            background: transparent;
            cursor: ew-resize;
            z-index: 10000;
            transition: background 0.2s;
        `;
        
        divider.addEventListener('mouseenter', () => {
            divider.style.background = 'var(--cursor-jade-dark)';
        });
        
        divider.addEventListener('mouseleave', () => {
            if (!this.isDragging) {
                divider.style.background = 'transparent';
            }
        });
        
        divider.addEventListener('dblclick', () => {
            this.resetRightPane();
        });
        
        divider.addEventListener('mousedown', (e) => {
            e.preventDefault();
            this.startDragging('right', e);
        });
        
        document.body.appendChild(divider);
        this.dividers.set('right', divider);
        
        console.log('[ResizablePanes] ✅ Right divider created');
    }
    
    createHorizontalDivider() {
        // Divider for horizontal splits (e.g., between code editor and terminal)
        const divider = document.createElement('div');
        divider.id = 'resize-divider-horizontal';
        divider.className = 'resize-divider resize-divider-horizontal';
        divider.style.cssText = `
            position: fixed;
            bottom: 300px;
            left: 0;
            width: 100%;
            height: 4px;
            background: transparent;
            cursor: ns-resize;
            z-index: 10000;
            transition: background 0.2s;
            display: none; /* Hidden by default, show when terminal is open */
        `;
        
        divider.addEventListener('mouseenter', () => {
            divider.style.background = 'var(--cursor-jade-dark)';
        });
        
        divider.addEventListener('mouseleave', () => {
            if (!this.isDragging) {
                divider.style.background = 'transparent';
            }
        });
        
        divider.addEventListener('dblclick', () => {
            this.resetHorizontalPane();
        });
        
        divider.addEventListener('mousedown', (e) => {
            e.preventDefault();
            this.startDragging('horizontal', e);
        });
        
        document.body.appendChild(divider);
        this.dividers.set('horizontal', divider);
        
        console.log('[ResizablePanes] ✅ Horizontal divider created');
    }
    
    startDragging(dividerType, e) {
        this.isDragging = true;
        this.currentDivider = dividerType;
        
        // Add dragging class to body
        document.body.classList.add('is-resizing');
        
        // Change cursor globally
        if (dividerType === 'horizontal') {
            document.body.style.cursor = 'ns-resize';
        } else {
            document.body.style.cursor = 'ew-resize';
        }
        
        // Highlight divider
        const divider = this.dividers.get(dividerType);
        if (divider) {
            divider.style.background = 'var(--cursor-accent)';
            divider.style.boxShadow = '0 0 10px rgba(119, 221, 190, 0.5)';
        }
        
        console.log(`[ResizablePanes] 🖱️ Started dragging: ${dividerType}`);
    }
    
    stopDragging() {
        if (!this.isDragging) return;
        
        this.isDragging = false;
        document.body.classList.remove('is-resizing');
        document.body.style.cursor = '';
        
        // Remove highlight
        const divider = this.dividers.get(this.currentDivider);
        if (divider) {
            divider.style.background = 'transparent';
            divider.style.boxShadow = 'none';
        }
        
        // Save sizes
        this.saveSizes();
        
        console.log(`[ResizablePanes] ✅ Stopped dragging: ${this.currentDivider}`);
        this.currentDivider = null;
    }
    
    registerGlobalHandlers() {
        // Global mousemove handler
        document.addEventListener('mousemove', (e) => {
            if (!this.isDragging) return;
            
            e.preventDefault();
            
            switch (this.currentDivider) {
                case 'left':
                    this.resizeLeftPane(e.clientX);
                    break;
                case 'right':
                    this.resizeRightPane(e.clientX);
                    break;
                case 'horizontal':
                    this.resizeHorizontalPane(e.clientY);
                    break;
            }
        });
        
        // Global mouseup handler
        document.addEventListener('mouseup', () => {
            this.stopDragging();
        });
        
        // Prevent text selection while dragging
        document.addEventListener('selectstart', (e) => {
            if (this.isDragging) {
                e.preventDefault();
            }
        });
    }
    
    resizeLeftPane(x) {
        // Constrain to min/max
        const minSize = this.minPaneSize;
        const maxSize = window.innerWidth * (this.maxPaneSize / 100);
        const newSize = Math.max(minSize, Math.min(maxSize, x));
        
        // Update left sidebar width
        const leftSidebar = document.querySelector('#conversation-history-sidebar, .left-sidebar, #file-explorer');
        if (leftSidebar) {
            leftSidebar.style.width = newSize + 'px';
        }
        
        // Update divider position
        const divider = this.dividers.get('left');
        if (divider) {
            divider.style.left = newSize + 'px';
        }
        
        // Update main content margin
        const mainContent = document.querySelector('.main-container, #orchestra-chat-stage');
        if (mainContent) {
            mainContent.style.marginLeft = newSize + 'px';
        }
        
        this.savedSizes.leftPane = newSize;
    }
    
    resizeRightPane(x) {
        // Calculate right pane width from right edge
        const rightWidth = window.innerWidth - x;
        
        // Constrain to min/max
        const minSize = this.minPaneSize;
        const maxSize = window.innerWidth * (this.maxPaneSize / 100);
        const newSize = Math.max(minSize, Math.min(maxSize, rightWidth));
        
        // Update right editor width
        const rightEditor = document.querySelector('#monaco-editor-container, .editor-container, .right-pane');
        if (rightEditor) {
            rightEditor.style.width = newSize + 'px';
        }
        
        // Update divider position
        const divider = this.dividers.get('right');
        if (divider) {
            divider.style.right = newSize + 'px';
        }
        
        this.savedSizes.rightPane = newSize;
    }
    
    resizeHorizontalPane(y) {
        // Calculate bottom pane height from bottom edge
        const bottomHeight = window.innerHeight - y;
        
        // Constrain to min/max
        const minSize = 100;
        const maxSize = window.innerHeight * 0.8;
        const newSize = Math.max(minSize, Math.min(maxSize, bottomHeight));
        
        // Update terminal/console height
        const bottomPane = document.querySelector('#console-panel, .terminal-panel, .bottom-pane');
        if (bottomPane) {
            bottomPane.style.height = newSize + 'px';
        }
        
        // Update divider position
        const divider = this.dividers.get('horizontal');
        if (divider) {
            divider.style.bottom = newSize + 'px';
        }
        
        this.savedSizes.bottomPane = newSize;
    }
    
    resetLeftPane() {
        const defaultSize = 280;
        this.resizeLeftPane(defaultSize);
        this.saveSizes();
        console.log('[ResizablePanes] 🔄 Left pane reset to default (280px)');
    }
    
    resetRightPane() {
        const defaultSize = window.innerWidth * 0.5;
        this.resizeRightPane(window.innerWidth - defaultSize);
        this.saveSizes();
        console.log('[ResizablePanes] 🔄 Right pane reset to default (50%)');
    }
    
    resetHorizontalPane() {
        const defaultSize = 300;
        this.resizeHorizontalPane(window.innerHeight - defaultSize);
        this.saveSizes();
        console.log('[ResizablePanes] 🔄 Horizontal pane reset to default (300px)');
    }
    
    saveSizes() {
        try {
            localStorage.setItem('bigdaddyg-pane-sizes', JSON.stringify(this.savedSizes));
            console.log('[ResizablePanes] 💾 Pane sizes saved:', this.savedSizes);
        } catch (error) {
            console.error('[ResizablePanes] ❌ Error saving sizes:', error);
        }
    }
    
    loadSavedSizes() {
        try {
            const saved = localStorage.getItem('bigdaddyg-pane-sizes');
            if (saved) {
                const sizes = JSON.parse(saved);
                console.log('[ResizablePanes] 📂 Loaded saved sizes:', sizes);
                return sizes;
            }
        } catch (error) {
            console.error('[ResizablePanes] ❌ Error loading sizes:', error);
        }
        
        // Defaults
        return {
            leftPane: 280,
            rightPane: window.innerWidth * 0.5,
            bottomPane: 300
        };
    }
    
    applySavedSizes() {
        if (this.savedSizes.leftPane) {
            this.resizeLeftPane(this.savedSizes.leftPane);
        }
        
        if (this.savedSizes.rightPane) {
            this.resizeRightPane(window.innerWidth - this.savedSizes.rightPane);
        }
        
        if (this.savedSizes.bottomPane) {
            this.resizeHorizontalPane(window.innerHeight - this.savedSizes.bottomPane);
        }
        
        console.log('[ResizablePanes] ✅ Saved sizes applied');
    }
    
    // API for external control
    showHorizontalDivider() {
        const divider = this.dividers.get('horizontal');
        if (divider) {
            divider.style.display = 'block';
        }
    }
    
    hideHorizontalDivider() {
        const divider = this.dividers.get('horizontal');
        if (divider) {
            divider.style.display = 'none';
        }
    }
    
    resetAllPanes() {
        this.resetLeftPane();
        this.resetRightPane();
        this.resetHorizontalPane();
        console.log('[ResizablePanes] 🔄 All panes reset to defaults');
    }
}

// Add global CSS for resizing
const style = document.createElement('style');
style.textContent = `
    /* Prevent text selection while resizing */
    body.is-resizing {
        user-select: none;
        -webkit-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
    }
    
    /* Resize divider styles */
    .resize-divider {
        position: relative;
    }
    
    .resize-divider::before {
        content: '';
        position: absolute;
        background: var(--cursor-jade-dark);
        opacity: 0;
        transition: opacity 0.2s;
    }
    
    .resize-divider-vertical::before {
        width: 2px;
        height: 100%;
        left: 1px;
    }
    
    .resize-divider-horizontal::before {
        width: 100%;
        height: 2px;
        top: 1px;
    }
    
    .resize-divider:hover::before {
        opacity: 0.5;
    }
    
    .resize-divider:active::before {
        opacity: 1;
    }
    
    /* Resize handle indicators */
    .resize-divider-vertical::after {
        content: '⋮';
        position: absolute;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        color: var(--cursor-text-secondary);
        font-size: 16px;
        opacity: 0;
        transition: opacity 0.2s;
        pointer-events: none;
    }
    
    .resize-divider-horizontal::after {
        content: '⋯';
        position: absolute;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        color: var(--cursor-text-secondary);
        font-size: 16px;
        opacity: 0;
        transition: opacity 0.2s;
        pointer-events: none;
    }
    
    .resize-divider:hover::after {
        opacity: 1;
    }
    
    /* Smooth transitions when not dragging */
    .left-sidebar,
    .right-pane,
    .bottom-pane,
    #conversation-history-sidebar,
    #monaco-editor-container,
    #console-panel {
        transition: width 0.1s ease-out, height 0.1s ease-out;
    }
    
    body.is-resizing .left-sidebar,
    body.is-resizing .right-pane,
    body.is-resizing .bottom-pane,
    body.is-resizing #conversation-history-sidebar,
    body.is-resizing #monaco-editor-container,
    body.is-resizing #console-panel {
        transition: none;
    }
`;
document.head.appendChild(style);

// Initialize
window.resizablePanes = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.resizablePanes = new ResizablePanes();
    });
} else {
    window.resizablePanes = new ResizablePanes();
}

// Export
window.ResizablePanes = ResizablePanes;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = ResizablePanes;
}

console.log('[ResizablePanes] 📦 Resizable panes module loaded');

})(); // End IIFE


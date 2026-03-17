/**
 * Universal Drag System
 * 
 * Makes ANY floating element draggable
 * - Floating panels
 * - Modals
 * - Dialogs
 * - Windows
 * - Popups
 */

(function() {
'use strict';

class UniversalDragSystem {
    constructor() {
        this.draggableElements = new Map();
        this.currentDragging = null;
        this.offset = { x: 0, y: 0 };
        
        console.log('[DragSystem] 🎯 Initializing Universal Drag System...');
        this.init();
    }
    
    init() {
        // Global mouse handlers
        document.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        document.addEventListener('mouseup', () => this.handleMouseUp());
        
        // Auto-detect and make floating elements draggable
        this.autoDetectFloatingElements();
        
        // Re-scan periodically for new floating elements
        setInterval(() => this.autoDetectFloatingElements(), 2000);
        
        console.log('[DragSystem] ✅ Universal Drag System ready!');
    }
    
    autoDetectFloatingElements() {
        // Find all elements with position: fixed
        const fixedElements = document.querySelectorAll('[style*="position: fixed"], [style*="position:fixed"]');
        
        fixedElements.forEach(element => {
            // Skip if already draggable
            if (this.draggableElements.has(element)) return;
            
            // Skip if too small (probably a button)
            const rect = element.getBoundingClientRect();
            if (rect.width < 200 || rect.height < 100) return;
            
            // Skip if it's the entire screen or body
            if (element === document.body || element === document.documentElement) return;
            
            // Skip main layout elements that should never be draggable
            const elementId = element.id || '';
            const skipIds = ['sidebar', 'editor-container', 'right-sidebar', 'main-container', 'app', 'title-bar'];
            if (skipIds.includes(elementId)) {
                console.log(`[DragSystem] ⏭️ Skipping main layout element: ${elementId}`);
                return;
            }
            
            // Try to make it draggable
            this.makeDraggable(element);
        });
    }
    
    makeDraggable(element, options = {}) {
        const {
            handle = null, // Specific drag handle (or null for entire element)
            constrainToViewport = true,
            onDragStart = null,
            onDragEnd = null,
            excludeSelectors = ['button', 'select', 'input', 'textarea', 'a']
        } = options;
        
        // Find drag handle (header, title bar, or entire element)
        const dragHandle = handle || this.findDragHandle(element);
        if (!dragHandle) return;
        
        // Mark as draggable
        dragHandle.style.cursor = 'move';
        dragHandle.dataset.draggable = 'true';
        
        // Add drag hint if it's a header
        if (dragHandle.tagName === 'DIV' && (dragHandle.id.includes('header') || dragHandle.className.includes('header'))) {
            const titleEl = dragHandle.querySelector('[style*="font-weight: 600"]') || 
                           dragHandle.querySelector('h1, h2, h3, h4, h5') ||
                           dragHandle;
            
            // Add subtle hint
            if (titleEl && !titleEl.textContent.includes('Drag')) {
                const hint = document.createElement('span');
                hint.style.cssText = 'font-size: 9px; opacity: 0.5; margin-left: 8px;';
                hint.textContent = '(Drag to move)';
                titleEl.appendChild(hint);
            }
        }
        
        // Mouse down handler
        dragHandle.addEventListener('mousedown', (e) => {
            // Don't drag if clicking excluded elements
            for (const selector of excludeSelectors) {
                if (e.target.matches(selector) || e.target.closest(selector)) {
                    return;
                }
            }
            
            e.preventDefault();
            
            this.currentDragging = element;
            
            // Get initial positions
            const rect = element.getBoundingClientRect();
            this.offset.x = e.clientX - rect.left;
            this.offset.y = e.clientY - rect.top;
            
            // Change cursor
            document.body.style.cursor = 'grabbing';
            dragHandle.style.cursor = 'grabbing';
            
            // Call callback
            if (onDragStart) onDragStart(element);
            
            // Add dragging class for visual feedback
            element.classList.add('dragging');
        });
        
        // Store element reference
        this.draggableElements.set(element, {
            handle: dragHandle,
            options,
            onDragEnd
        });
        
        const elementName = element.id || element.className || element.tagName.toLowerCase();
        if (elementName && elementName.trim()) {
            console.log(`[DragSystem] ✅ Made draggable: ${elementName}`);
        }
    }
    
    findDragHandle(element) {
        // Try to find a suitable drag handle within the element
        
        // Look for common header patterns
        const selectors = [
            '[id*="header"]',
            '[class*="header"]',
            '[id*="title"]',
            '[class*="title"]',
            'header',
            '.modal-header',
            '.panel-header',
            '.window-header'
        ];
        
        for (const selector of selectors) {
            const handle = element.querySelector(selector);
            if (handle) return handle;
        }
        
        // Fallback: use the element itself
        return element;
    }
    
    handleMouseMove(e) {
        if (!this.currentDragging) return;
        
        e.preventDefault();
        
        // Calculate new position
        let newX = e.clientX - this.offset.x;
        let newY = e.clientY - this.offset.y;
        
        // Get element data
        const data = this.draggableElements.get(this.currentDragging);
        if (!data) return;
        
        // Constrain to viewport if enabled
        if (data.options.constrainToViewport !== false) {
            const maxX = window.innerWidth - this.currentDragging.offsetWidth;
            const maxY = window.innerHeight - this.currentDragging.offsetHeight;
            
            newX = Math.max(0, Math.min(newX, maxX));
            newY = Math.max(0, Math.min(newY, maxY));
        }
        
        // Update position
        this.currentDragging.style.transform = 'none';
        this.currentDragging.style.left = newX + 'px';
        this.currentDragging.style.top = newY + 'px';
    }
    
    handleMouseUp() {
        if (!this.currentDragging) return;
        
        const data = this.draggableElements.get(this.currentDragging);
        
        // Reset cursors
        document.body.style.cursor = '';
        if (data && data.handle) {
            data.handle.style.cursor = 'move';
        }
        
        // Remove dragging class
        this.currentDragging.classList.remove('dragging');
        
        // Call callback
        if (data && data.onDragEnd) {
            data.onDragEnd(this.currentDragging);
        }
        
        this.currentDragging = null;
    }
    
    // Public API
    enable(element, options = {}) {
        this.makeDraggable(element, options);
    }
    
    disable(element) {
        if (this.draggableElements.has(element)) {
            const data = this.draggableElements.get(element);
            if (data.handle) {
                data.handle.style.cursor = '';
                data.handle.dataset.draggable = 'false';
            }
            this.draggableElements.delete(element);
            console.log('[DragSystem] ❌ Disabled dragging for element');
        }
    }
    
    isEnabled(element) {
        return this.draggableElements.has(element);
    }
}

// Initialize and expose globally
window.dragSystem = new UniversalDragSystem();

// Add global helper function
window.makeDraggable = (elementOrSelector, options = {}) => {
    const element = typeof elementOrSelector === 'string' 
        ? document.querySelector(elementOrSelector)
        : elementOrSelector;
    
    if (element) {
        window.dragSystem.enable(element, options);
        return true;
    }
    return false;
};

// Add CSS for dragging visual feedback
const style = document.createElement('style');
style.textContent = `
    [data-draggable="true"] {
        user-select: none;
        -webkit-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
    }
    
    .dragging {
        opacity: 0.9;
        box-shadow: 0 32px 64px rgba(0, 0, 0, 0.6) !important;
        transition: none !important;
    }
`;
document.head.appendChild(style);

console.log('[DragSystem] 📦 Universal Drag System loaded');
console.log('[DragSystem] 💡 Usage:');
console.log('  • makeDraggable(element) - Make any element draggable');
console.log('  • makeDraggable("#my-modal") - Use selector');
console.log('  • Auto-detects position: fixed elements');

})();


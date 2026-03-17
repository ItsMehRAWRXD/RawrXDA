/**
 * UI Polish & Enhancements
 * 
 * Professional animations, smooth transitions, and visual polish
 * to match or exceed VS Code, Cursor, and JetBrains quality
 */

(function() {
'use strict';

class UIEnhancer {
    constructor() {
        this.animationSpeed = 300; // ms
        this.init();
    }
    
    init() {
        console.log('[UIEnhancer] 🎨 Initializing UI enhancements...');
        
        this.addGlobalStyles();
        this.addAnimations();
        this.addHoverEffects();
        this.addLoadingStates();
        this.addNotificationSystem();
        this.addTooltips();
        this.addContextMenus();
        this.enhanceScrollbars();
        this.addFocusEffects();
        
        console.log('[UIEnhancer] ✅ UI enhancements ready');
    }
    
    // ========================================================================
    // GLOBAL STYLES
    // ========================================================================
    
    addGlobalStyles() {
        const style = document.createElement('style');
        style.id = 'ui-enhancer-styles';
        style.textContent = `
            /* ========================================
               SMOOTH ANIMATIONS & TRANSITIONS
               ======================================== */
            
            * {
                transition-timing-function: cubic-bezier(0.4, 0.0, 0.2, 1);
            }
            
            /* Smooth opacity transitions */
            .fade-in {
                animation: fadeIn 0.3s ease-in-out;
            }
            
            @keyframes fadeIn {
                from { opacity: 0; }
                to { opacity: 1; }
            }
            
            .fade-out {
                animation: fadeOut 0.3s ease-in-out;
            }
            
            @keyframes fadeOut {
                from { opacity: 1; }
                to { opacity: 0; }
            }
            
            /* Slide animations */
            .slide-in-right {
                animation: slideInRight 0.3s ease-out;
            }
            
            @keyframes slideInRight {
                from { 
                    transform: translateX(100%);
                    opacity: 0;
                }
                to { 
                    transform: translateX(0);
                    opacity: 1;
                }
            }
            
            .slide-in-left {
                animation: slideInLeft 0.3s ease-out;
            }
            
            @keyframes slideInLeft {
                from { 
                    transform: translateX(-100%);
                    opacity: 0;
                }
                to { 
                    transform: translateX(0);
                    opacity: 1;
                }
            }
            
            .slide-up {
                animation: slideUp 0.3s ease-out;
            }
            
            @keyframes slideUp {
                from { 
                    transform: translateY(20px);
                    opacity: 0;
                }
                to { 
                    transform: translateY(0);
                    opacity: 1;
                }
            }
            
            /* Scale animations */
            .scale-in {
                animation: scaleIn 0.2s ease-out;
            }
            
            @keyframes scaleIn {
                from { 
                    transform: scale(0.9);
                    opacity: 0;
                }
                to { 
                    transform: scale(1);
                    opacity: 1;
                }
            }
            
            /* Pulse animation */
            @keyframes pulse {
                0%, 100% { opacity: 1; }
                50% { opacity: 0.5; }
            }
            
            /* Glow effect */
            .glow {
                box-shadow: 0 0 20px var(--cyan);
                animation: glow 2s ease-in-out infinite;
            }
            
            @keyframes glow {
                0%, 100% { box-shadow: 0 0 10px var(--cyan); }
                50% { box-shadow: 0 0 30px var(--cyan); }
            }
            
            /* ========================================
               ENHANCED BUTTONS
               ======================================== */
            
            button, .btn {
                position: relative;
                overflow: hidden;
                transition: all 0.3s cubic-bezier(0.4, 0.0, 0.2, 1);
            }
            
            button:hover, .btn:hover {
                transform: translateY(-2px);
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
            }
            
            button:active, .btn:active {
                transform: translateY(0);
            }
            
            /* Ripple effect */
            button::after, .btn::after {
                content: '';
                position: absolute;
                top: 50%;
                left: 50%;
                width: 0;
                height: 0;
                border-radius: 50%;
                background: rgba(255, 255, 255, 0.5);
                transform: translate(-50%, -50%);
                transition: width 0.6s, height 0.6s;
            }
            
            button:active::after, .btn:active::after {
                width: 300px;
                height: 300px;
            }
            
            /* ========================================
               ENHANCED SCROLLBARS
               ======================================== */
            
            ::-webkit-scrollbar {
                width: 12px;
                height: 12px;
            }
            
            ::-webkit-scrollbar-track {
                background: var(--void);
                border-radius: 6px;
            }
            
            ::-webkit-scrollbar-thumb {
                background: rgba(0, 212, 255, 0.3);
                border-radius: 6px;
                border: 2px solid var(--void);
                transition: background 0.3s;
            }
            
            ::-webkit-scrollbar-thumb:hover {
                background: rgba(0, 212, 255, 0.5);
            }
            
            ::-webkit-scrollbar-thumb:active {
                background: rgba(0, 212, 255, 0.7);
            }
            
            /* ========================================
               ENHANCED PANELS & CARDS
               ======================================== */
            
            .panel, .card {
                transition: all 0.3s cubic-bezier(0.4, 0.0, 0.2, 1);
            }
            
            .panel:hover, .card:hover {
                transform: translateY(-2px);
                box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
            }
            
            /* ========================================
               LOADING STATES
               ======================================== */
            
            .loading {
                position: relative;
                pointer-events: none;
                opacity: 0.6;
            }
            
            .loading::before {
                content: '';
                position: absolute;
                top: 50%;
                left: 50%;
                width: 20px;
                height: 20px;
                margin: -10px 0 0 -10px;
                border: 2px solid var(--cyan);
                border-top-color: transparent;
                border-radius: 50%;
                animation: spin 0.8s linear infinite;
            }
            
            @keyframes spin {
                to { transform: rotate(360deg); }
            }
            
            /* ========================================
               FOCUS EFFECTS
               ======================================== */
            
            input:focus, textarea:focus, select:focus {
                outline: none;
                border-color: var(--cyan);
                box-shadow: 0 0 0 3px rgba(0, 212, 255, 0.2);
            }
            
            /* ========================================
               TOOLTIPS
               ======================================== */
            
            .tooltip {
                position: absolute;
                background: rgba(0, 0, 0, 0.9);
                color: #fff;
                padding: 8px 12px;
                border-radius: 6px;
                font-size: 12px;
                pointer-events: none;
                z-index: 10000;
                white-space: nowrap;
                opacity: 0;
                transition: opacity 0.2s;
            }
            
            .tooltip.show {
                opacity: 1;
            }
            
            .tooltip::before {
                content: '';
                position: absolute;
                bottom: -5px;
                left: 50%;
                margin-left: -5px;
                border-width: 5px 5px 0;
                border-style: solid;
                border-color: rgba(0, 0, 0, 0.9) transparent transparent;
            }
            
            /* ========================================
               CONTEXT MENUS
               ======================================== */
            
            .context-menu {
                position: fixed;
                background: rgba(10, 10, 30, 0.98);
                backdrop-filter: blur(10px);
                border: 1px solid var(--cyan);
                border-radius: 8px;
                padding: 8px 0;
                min-width: 200px;
                box-shadow: 0 10px 40px rgba(0, 0, 0, 0.5);
                z-index: 10001;
                display: none;
            }
            
            .context-menu.show {
                display: block;
                animation: scaleIn 0.15s ease-out;
            }
            
            .context-menu-item {
                padding: 10px 16px;
                cursor: pointer;
                font-size: 13px;
                color: #fff;
                display: flex;
                align-items: center;
                gap: 10px;
                transition: background 0.2s;
            }
            
            .context-menu-item:hover {
                background: rgba(0, 212, 255, 0.2);
            }
            
            .context-menu-item.danger:hover {
                background: rgba(255, 71, 87, 0.2);
                color: var(--red);
            }
            
            .context-menu-separator {
                height: 1px;
                background: rgba(0, 212, 255, 0.2);
                margin: 4px 0;
            }
            
            /* ========================================
               NOTIFICATION SYSTEM
               ======================================== */
            
            .notification {
                position: fixed;
                top: 20px;
                right: 20px;
                background: rgba(10, 10, 30, 0.98);
                backdrop-filter: blur(10px);
                border-left: 4px solid var(--cyan);
                border-radius: 8px;
                padding: 16px 20px;
                min-width: 300px;
                max-width: 400px;
                box-shadow: 0 10px 40px rgba(0, 0, 0, 0.5);
                z-index: 10002;
                animation: slideInRight 0.3s ease-out;
            }
            
            .notification.success {
                border-left-color: var(--green);
            }
            
            .notification.error {
                border-left-color: var(--red);
            }
            
            .notification.warning {
                border-left-color: var(--orange);
            }
            
            .notification-title {
                font-weight: 600;
                font-size: 14px;
                margin-bottom: 6px;
                color: var(--cyan);
            }
            
            .notification.success .notification-title {
                color: var(--green);
            }
            
            .notification.error .notification-title {
                color: var(--red);
            }
            
            .notification-message {
                font-size: 12px;
                color: #ccc;
                line-height: 1.5;
            }
            
            .notification-close {
                position: absolute;
                top: 10px;
                right: 10px;
                background: none;
                border: none;
                color: #888;
                cursor: pointer;
                font-size: 18px;
                padding: 0;
                width: 20px;
                height: 20px;
                transition: color 0.2s;
            }
            
            .notification-close:hover {
                color: #fff;
            }
        `;
        
        document.head.appendChild(style);
        console.log('[UIEnhancer] 🎨 Global styles added');
    }
    
    // ========================================================================
    // ANIMATIONS
    // ========================================================================
    
    addAnimations() {
        // Add entrance animations to existing elements
        document.querySelectorAll('.panel, .card').forEach(el => {
            el.classList.add('slide-up');
        });
        
        console.log('[UIEnhancer] ✨ Animations added');
    }
    
    // ========================================================================
    // HOVER EFFECTS
    // ========================================================================
    
    addHoverEffects() {
        // Add smooth hover effects to all interactive elements
        const interactiveElements = document.querySelectorAll('button, .btn, .file-item, .editor-tab, a');
        
        interactiveElements.forEach(el => {
            if (!el.dataset.hoverEnhanced) {
                el.dataset.hoverEnhanced = 'true';
            }
        });
        
        console.log('[UIEnhancer] 🎯 Hover effects added');
    }
    
    // ========================================================================
    // LOADING STATES
    // ========================================================================
    
    addLoadingStates() {
        window.showLoading = (element) => {
            if (element) {
                element.classList.add('loading');
            }
        };
        
        window.hideLoading = (element) => {
            if (element) {
                element.classList.remove('loading');
            }
        };
        
        console.log('[UIEnhancer] ⏳ Loading states added');
    }
    
    // ========================================================================
    // NOTIFICATION SYSTEM
    // ========================================================================
    
    addNotificationSystem() {
        window.showNotification = (title, message, type = 'info', duration = 5000) => {
            const notification = document.createElement('div');
            notification.className = `notification ${type}`;
            
            notification.innerHTML = `
                <button class="notification-close" onclick="this.parentElement.remove()">×</button>
                <div class="notification-title">${title}</div>
                <div class="notification-message">${message}</div>
            `;
            
            document.body.appendChild(notification);
            
            // Auto-remove after duration
            if (duration > 0) {
                setTimeout(() => {
                    notification.style.opacity = '0';
                    notification.style.transform = 'translateX(100%)';
                    setTimeout(() => notification.remove(), 300);
                }, duration);
            }
        };
        
        console.log('[UIEnhancer] 📢 Notification system ready');
    }
    
    // ========================================================================
    // TOOLTIPS
    // ========================================================================
    
    addTooltips() {
        let currentTooltip = null;
        
        document.addEventListener('mouseover', (e) => {
            const element = e.target;
            const tooltipText = element.getAttribute('data-tooltip') || element.getAttribute('title');
            
            if (tooltipText && !currentTooltip) {
                // Remove title to prevent default browser tooltip
                if (element.hasAttribute('title')) {
                    element.setAttribute('data-tooltip', tooltipText);
                    element.removeAttribute('title');
                }
                
                currentTooltip = document.createElement('div');
                currentTooltip.className = 'tooltip';
                currentTooltip.textContent = tooltipText;
                document.body.appendChild(currentTooltip);
                
                const rect = element.getBoundingClientRect();
                currentTooltip.style.left = rect.left + (rect.width / 2) - (currentTooltip.offsetWidth / 2) + 'px';
                currentTooltip.style.top = rect.top - currentTooltip.offsetHeight - 10 + 'px';
                
                setTimeout(() => currentTooltip.classList.add('show'), 10);
            }
        });
        
        document.addEventListener('mouseout', (e) => {
            if (currentTooltip) {
                currentTooltip.remove();
                currentTooltip = null;
            }
        });
        
        console.log('[UIEnhancer] 💬 Tooltips enabled');
    }
    
    // ========================================================================
    // CONTEXT MENUS
    // ========================================================================
    
    addContextMenus() {
        let currentContextMenu = null;
        
        window.showContextMenu = (x, y, items) => {
            // Remove existing context menu
            if (currentContextMenu) {
                currentContextMenu.remove();
            }
            
            const menu = document.createElement('div');
            menu.className = 'context-menu';
            
            items.forEach(item => {
                if (item === 'separator') {
                    const separator = document.createElement('div');
                    separator.className = 'context-menu-separator';
                    menu.appendChild(separator);
                } else {
                    const menuItem = document.createElement('div');
                    menuItem.className = `context-menu-item ${item.danger ? 'danger' : ''}`;
                    menuItem.innerHTML = `${item.icon || ''} ${item.label}`;
                    menuItem.onclick = () => {
                        if (item.action) item.action();
                        menu.remove();
                        currentContextMenu = null;
                    };
                    menu.appendChild(menuItem);
                }
            });
            
            menu.style.left = x + 'px';
            menu.style.top = y + 'px';
            
            document.body.appendChild(menu);
            currentContextMenu = menu;
            
            setTimeout(() => menu.classList.add('show'), 10);
        };
        
        // Close context menu on click outside
        document.addEventListener('click', () => {
            if (currentContextMenu) {
                currentContextMenu.remove();
                currentContextMenu = null;
            }
        });
        
        console.log('[UIEnhancer] 📋 Context menus ready');
    }
    
    // ========================================================================
    // ENHANCED SCROLLBARS
    // ========================================================================
    
    enhanceScrollbars() {
        // Scrollbars are styled via CSS
        console.log('[UIEnhancer] 📜 Enhanced scrollbars applied');
    }
    
    // ========================================================================
    // FOCUS EFFECTS
    // ========================================================================
    
    addFocusEffects() {
        // Focus effects are handled via CSS
        console.log('[UIEnhancer] 🎯 Focus effects applied');
    }
}

// ========================================================================
// GLOBAL EXPOSURE
// ========================================================================

window.uiEnhancer = new UIEnhancer();

console.log('[UIEnhancer] 🎨 UI Enhancer ready');

})();

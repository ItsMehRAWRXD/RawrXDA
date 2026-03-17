/**
 * BigDaddyG IDE - Context Menu Executor Integration
 * Wires context menu actions to agentic executor for true automation
 */

(function() {
'use strict';

class ContextMenuExecutor {
    constructor() {
        this.executor = null;
        this.menuActions = new Map();
        
        console.log('[ContextMenuExecutor] 🎯 Initializing...');
        this.init();
    }
    
    async init() {
        // Wait for agentic executor
        await this.waitForExecutor();
        
        // Register all menu actions
        this.registerActions();
        
        // Hook into existing context menus
        this.hookContextMenus();
        
        // Expose to window
        window.contextMenuExecutor = this;
        
        console.log('[ContextMenuExecutor] ✅ Ready');
    }
    
    async waitForExecutor() {
        return new Promise((resolve) => {
            const check = () => {
                if (window.getAgenticExecutor) {
                    this.executor = window.getAgenticExecutor();
                    console.log('[ContextMenuExecutor] Executor connected');
                    resolve();
                    return true;
                }
                return false;
            };
            
            if (!check()) {
                const interval = setInterval(() => {
                    if (check()) {
                        clearInterval(interval);
                    }
                }, 100);
            }
        });
    }
    
    registerActions() {
        // Auto Fix
        this.registerAction('autoFix', {
            name: 'Auto Fix',
            icon: '🔧',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                if (!code) {
                    throw new Error('No code selected');
                }
                
                return await this.executor.executeTask(
                    `Analyze and automatically fix any bugs, errors, or issues in this code:\n\n${code}\n\nApply fixes directly.`
                );
            }
        });
        
        // Explain Code
        this.registerAction('explainCode', {
            name: 'Explain Code',
            icon: '💡',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Explain what this code does in detail:\n\n${code}`
                );
            }
        });
        
        // Optimize
        this.registerAction('optimize', {
            name: 'Optimize',
            icon: '⚡',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Optimize this code for better performance and efficiency:\n\n${code}\n\nApply optimizations directly.`
                );
            }
        });
        
        // Add Tests
        this.registerAction('addTests', {
            name: 'Generate Tests',
            icon: '🧪',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Generate comprehensive unit tests for this code:\n\n${code}`
                );
            }
        });
        
        // Refactor
        this.registerAction('refactor', {
            name: 'Refactor',
            icon: '🔄',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Refactor this code to improve readability and maintainability:\n\n${code}\n\nApply refactoring directly.`
                );
            }
        });
        
        // Add Documentation
        this.registerAction('addDocs', {
            name: 'Add Documentation',
            icon: '📚',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Add comprehensive documentation to this code:\n\n${code}\n\nAdd JSDoc/comments directly.`
                );
            }
        });
        
        // Security Scan
        this.registerAction('securityScan', {
            name: 'Security Scan',
            icon: '🔒',
            handler: async (context) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Perform a security audit on this code and fix any vulnerabilities:\n\n${code}\n\nApply security fixes directly.`
                );
            }
        });
        
        // Convert To
        this.registerAction('convertTo', {
            name: 'Convert To...',
            icon: '🔀',
            handler: async (context, targetLanguage) => {
                const code = context.selectedText || context.currentFile;
                return await this.executor.executeTask(
                    `Convert this code to ${targetLanguage}:\n\n${code}`
                );
            }
        });
    }
    
    registerAction(id, config) {
        this.menuActions.set(id, config);
    }
    
    hookContextMenus() {
        // Override existing context menu functions
        const originalShowContextMenu = window.showContextMenu;
        
        window.showContextMenu = (x, y, items) => {
            // Add our executor-based actions
            const enhancedItems = this.enhanceMenuItems(items);
            
            // Call original
            if (originalShowContextMenu) {
                return originalShowContextMenu(x, y, enhancedItems);
            }
            
            // Fallback: create menu ourselves
            this.createContextMenu(x, y, enhancedItems);
        };
        
        // Hook editor right-click
        document.addEventListener('contextmenu', (e) => {
            if (e.target.closest('.monaco-editor, .editor-area, textarea.code-input')) {
                e.preventDefault();
                this.showEditorContextMenu(e.pageX, e.pageY);
            }
        });
    }
    
    enhanceMenuItems(items) {
        if (!items || !Array.isArray(items)) return items;
        
        return items.map(item => {
            // If item has an action that we can enhance, replace it
            if (item.action && typeof item.action === 'string') {
                const actionHandler = this.menuActions.get(item.action);
                if (actionHandler) {
                    return {
                        ...item,
                        action: () => this.executeAction(item.action)
                    };
                }
            }
            return item;
        });
    }
    
    async executeAction(actionId, additionalContext = {}) {
        const action = this.menuActions.get(actionId);
        if (!action) {
            console.error(`[ContextMenuExecutor] Action not found: ${actionId}`);
            return;
        }
        
        if (!this.executor) {
            window.showNotification?.('Agentic executor not available', '', 'error', 3000);
            return;
        }
        
        // Show progress
        window.showNotification?.(action.name, 'Processing...', 'info', 0);
        
        try {
            // Get context
            const context = this.getContext();
            
            // Execute action
            const result = await action.handler({ ...context, ...additionalContext });
            
            // Show success
            if (result.success) {
                window.showNotification?.(action.name, 'Completed successfully!', 'success', 3000);
                
                // Apply result if it's code
                if (result.code) {
                    this.applyResult(result.code, context);
                }
            } else {
                throw new Error(result.error || 'Action failed');
            }
            
        } catch (error) {
            console.error(`[ContextMenuExecutor] Action failed:`, error);
            window.showNotification?.(action.name, `Failed: ${error.message}`, 'error', 5000);
        }
    }
    
    getContext() {
        return {
            selectedText: this.getSelectedText(),
            currentFile: this.getCurrentFileContent(),
            fileName: this.getCurrentFileName(),
            language: this.getLanguage()
        };
    }
    
    getSelectedText() {
        // Try Monaco editor
        if (window.editor && window.editor.getSelection) {
            const selection = window.editor.getSelection();
            return window.editor.getModel().getValueInRange(selection);
        }
        
        // Try regular text selection
        const selection = window.getSelection();
        return selection ? selection.toString() : '';
    }
    
    getCurrentFileContent() {
        if (window.editor && window.editor.getValue) {
            return window.editor.getValue();
        }
        
        const activeEditor = document.querySelector('.monaco-editor textarea, textarea.code-input');
        return activeEditor?.value || '';
    }
    
    getCurrentFileName() {
        return window.currentFile || 'untitled';
    }
    
    getLanguage() {
        if (window.editor && window.editor.getModel) {
            return window.editor.getModel().getLanguageId();
        }
        
        const fileName = this.getCurrentFileName();
        const ext = fileName.split('.').pop();
        return ext || 'text';
    }
    
    applyResult(code, context) {
        if (window.editor && window.editor.setValue) {
            if (context.selectedText) {
                // Replace selection
                const selection = window.editor.getSelection();
                window.editor.executeEdits('context-menu', [{
                    range: selection,
                    text: code
                }]);
            } else {
                // Replace entire file
                window.editor.setValue(code);
            }
        }
    }
    
    showEditorContextMenu(x, y) {
        const items = [
            { id: 'autoFix', label: '🔧 Auto Fix', action: 'autoFix' },
            { id: 'explainCode', label: '💡 Explain Code', action: 'explainCode' },
            { id: 'optimize', label: '⚡ Optimize', action: 'optimize' },
            { type: 'separator' },
            { id: 'addTests', label: '🧪 Generate Tests', action: 'addTests' },
            { id: 'refactor', label: '🔄 Refactor', action: 'refactor' },
            { id: 'addDocs', label: '📚 Add Documentation', action: 'addDocs' },
            { type: 'separator' },
            { id: 'securityScan', label: '🔒 Security Scan', action: 'securityScan' },
            { 
                id: 'convertTo', 
                label: '🔀 Convert To...', 
                submenu: [
                    { id: 'convertToPython', label: 'Python', action: () => this.executeAction('convertTo', { targetLanguage: 'Python' }) },
                    { id: 'convertToJava', label: 'Java', action: () => this.executeAction('convertTo', { targetLanguage: 'Java' }) },
                    { id: 'convertToTypeScript', label: 'TypeScript', action: () => this.executeAction('convertTo', { targetLanguage: 'TypeScript' }) },
                    { id: 'convertToCpp', label: 'C++', action: () => this.executeAction('convertTo', { targetLanguage: 'C++' }) }
                ]
            }
        ];
        
        this.createContextMenu(x, y, items);
    }
    
    createContextMenu(x, y, items) {
        // Remove existing menu
        const existing = document.querySelector('.context-menu-executor');
        if (existing) existing.remove();
        
        const menu = document.createElement('div');
        menu.className = 'context-menu-executor';
        menu.style.cssText = `
            position: fixed;
            left: ${x}px;
            top: ${y}px;
            background: var(--vscode-menu-background, #2d2d30);
            border: 1px solid var(--vscode-menu-border, #454545);
            border-radius: 4px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 10000;
            min-width: 200px;
            padding: 4px 0;
            font-family: var(--vscode-font-family);
            font-size: 13px;
        `;
        
        items.forEach(item => {
            if (item.type === 'separator') {
                const sep = document.createElement('div');
                sep.style.cssText = `
                    height: 1px;
                    background: var(--vscode-menu-separatorBackground, #454545);
                    margin: 4px 0;
                `;
                menu.appendChild(sep);
                return;
            }
            
            const menuItem = document.createElement('div');
            menuItem.className = 'menu-item';
            menuItem.textContent = item.label;
            menuItem.style.cssText = `
                padding: 6px 12px;
                cursor: pointer;
                color: var(--vscode-menu-foreground, #cccccc);
                transition: background-color 0.1s;
            `;
            
            menuItem.addEventListener('mouseenter', () => {
                menuItem.style.backgroundColor = 'var(--vscode-menu-selectionBackground, #094771)';
            });
            
            menuItem.addEventListener('mouseleave', () => {
                menuItem.style.backgroundColor = 'transparent';
            });
            
            menuItem.addEventListener('click', () => {
                menu.remove();
                if (typeof item.action === 'string') {
                    this.executeAction(item.action);
                } else if (typeof item.action === 'function') {
                    item.action();
                }
            });
            
            menu.appendChild(menuItem);
        });
        
        document.body.appendChild(menu);
        
        // Close on click outside
        const closeHandler = (e) => {
            if (!menu.contains(e.target)) {
                menu.remove();
                document.removeEventListener('click', closeHandler);
            }
        };
        
        setTimeout(() => {
            document.addEventListener('click', closeHandler);
        }, 0);
    }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new ContextMenuExecutor();
    });
} else {
    new ContextMenuExecutor();
}

})();
    
    getCurrentFileContent() {
        if (window.editor && window.editor.getValue) {
            return window.editor.getValue();
        }
        return '';
    }
    
    getCurrentFileName() {
        // Try to get from tab system
        if (window.tabSystem && window.tabSystem.activeTab) {
            return window.tabSystem.activeTab.name;
        }
        return 'untitled';
    }
    
    getLanguage() {
        if (window.editor && window.editor.getModel) {
            const model = window.editor.getModel();
            if (model) {
                return model.getLanguageId();
            }
        }
        return 'javascript';
    }
    
    applyResult(code, context) {
        if (!window.editor) return;
        
        // If we had a selection, replace it
        if (context.selectedText && window.editor.getSelection) {
            const selection = window.editor.getSelection();
            window.editor.executeEdits('contextMenu', [{
                range: selection,
                text: code
            }]);
        } else {
            // Otherwise, insert at current position
            const position = window.editor.getPosition();
            window.editor.executeEdits('contextMenu', [{
                range: {
                    startLineNumber: position.lineNumber,
                    startColumn: position.column,
                    endLineNumber: position.lineNumber,
                    endColumn: position.column
                },
                text: '\n' + code + '\n'
            }]);
        }
    }
    
    showEditorContextMenu(x, y) {
        const context = this.getContext();
        const hasSelection = !!context.selectedText;
        
        const menuItems = [
            hasSelection && {
                label: '🔧 Auto Fix',
                action: () => this.executeAction('autoFix')
            },
            hasSelection && {
                label: '💡 Explain Code',
                action: () => this.executeAction('explainCode')
            },
            hasSelection && {
                label: '⚡ Optimize',
                action: () => this.executeAction('optimize')
            },
            { separator: true },
            {
                label: '🧪 Generate Tests',
                action: () => this.executeAction('addTests')
            },
            {
                label: '📚 Add Documentation',
                action: () => this.executeAction('addDocs')
            },
            { separator: true },
            {
                label: '🔒 Security Scan',
                action: () => this.executeAction('securityScan')
            },
            {
                label: '🔄 Refactor',
                action: () => this.executeAction('refactor')
            }
        ].filter(Boolean);
        
        if (window.showContextMenu) {
            window.showContextMenu(x, y, menuItems);
        }
    }
    
    createContextMenu(x, y, items) {
        // Remove existing menu
        const existing = document.getElementById('context-menu-executor');
        if (existing) existing.remove();
        
        const menu = document.createElement('div');
        menu.id = 'context-menu-executor';
        menu.style.cssText = `
            position: fixed;
            left: ${x}px;
            top: ${y}px;
            background: rgba(10, 10, 30, 0.98);
            backdrop-filter: blur(20px);
            border: 1px solid var(--cyan);
            border-radius: 8px;
            padding: 5px;
            z-index: 1000000;
            box-shadow: 0 5px 20px rgba(0,0,0,0.5);
            min-width: 200px;
        `;
        
        items.forEach(item => {
            if (item.separator) {
                const sep = document.createElement('div');
                sep.style.cssText = 'height: 1px; background: rgba(0,212,255,0.2); margin: 5px 0;';
                menu.appendChild(sep);
                return;
            }
            
            const menuItem = document.createElement('div');
            menuItem.textContent = item.label;
            menuItem.style.cssText = `
                padding: 8px 12px;
                cursor: pointer;
                color: #fff;
                font-size: 13px;
                border-radius: 4px;
                transition: all 0.2s;
            `;
            
            menuItem.onmouseenter = () => {
                menuItem.style.background = 'rgba(0,212,255,0.2)';
            };
            
            menuItem.onmouseleave = () => {
                menuItem.style.background = 'transparent';
            };
            
            menuItem.onclick = () => {
                if (item.action) item.action();
                menu.remove();
            };
            
            menu.appendChild(menuItem);
        });
        
        document.body.appendChild(menu);
        
        // Close on click outside
        const closeMenu = (e) => {
            if (!menu.contains(e.target)) {
                menu.remove();
                document.removeEventListener('click', closeMenu);
            }
        };
        setTimeout(() => document.addEventListener('click', closeMenu), 0);
    }
}

// Initialize on DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new ContextMenuExecutor();
    });
} else {
    new ContextMenuExecutor();
}

})();

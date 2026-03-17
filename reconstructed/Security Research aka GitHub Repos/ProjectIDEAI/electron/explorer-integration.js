/**
 * Enhanced File Explorer Integration Helper
 * 
 * Connects the enhanced file explorer to Monaco editor and tab system
 * Ensures seamless file tracking and operations
 */

(function() {
'use strict';

// Wait for all systems to load
window.addEventListener('load', () => {
    console.log('[ExplorerIntegration] 🔗 Setting up file explorer integration...');
    
    // ========================================================================
    // MONACO EDITOR INTEGRATION
    // ========================================================================
    
    // Hook into existing createNewTab function
    const originalCreateNewTab = window.createNewTab;
    if (originalCreateNewTab && typeof originalCreateNewTab === 'function') {
        window.createNewTab = function(filename, language, content, filePath) {
            // Call original function
            const result = originalCreateNewTab(filename, language, content, filePath);
            
            // Notify enhanced explorer
            if (filePath && window.enhancedFileExplorer) {
                window.enhancedFileExplorer.addOpenEditor({
                    path: filePath,
                    filename: filename,
                    language: language,
                    content: content
                });
            }
            
            return result;
        };
        console.log('[ExplorerIntegration] ✅ Hooked createNewTab');
    }
    
    // Hook into file save operations
    const originalSaveFile = window.saveFile;
    if (originalSaveFile && typeof originalSaveFile === 'function') {
        window.saveFile = function(path) {
            const result = originalSaveFile(path);
            
            // Notify enhanced explorer
            if (window.enhancedFileExplorer) {
                window.enhancedFileExplorer.updateOpenEditor(path, { modified: false });
            }
            
            return result;
        };
        console.log('[ExplorerIntegration] ✅ Hooked saveFile');
    }
    
    // Hook into editor close
    const originalCloseTab = window.closeTab;
    if (originalCloseTab && typeof originalCloseTab === 'function') {
        window.closeTab = function(tabId, filePath) {
            const result = originalCloseTab(tabId, filePath);
            
            // Notify enhanced explorer
            if (filePath && window.enhancedFileExplorer) {
                window.enhancedFileExplorer.removeOpenEditor(filePath);
            }
            
            return result;
        };
        console.log('[ExplorerIntegration] ✅ Hooked closeTab');
    }
    
    // ========================================================================
    // MONACO CHANGE DETECTION
    // ========================================================================
    
    // Listen for Monaco editor changes
    if (window.monaco && window.editor) {
        let changeTimeout;
        
        window.editor.onDidChangeModelContent(() => {
            clearTimeout(changeTimeout);
            
            changeTimeout = setTimeout(() => {
                // Get current file path
                const currentTab = getCurrentTab(); // Assumes this function exists
                
                if (currentTab && currentTab.filePath && window.enhancedFileExplorer) {
                    window.enhancedFileExplorer.updateOpenEditor(currentTab.filePath, {
                        modified: true
                    });
                }
            }, 500); // Debounce 500ms
        });
        
        console.log('[ExplorerIntegration] ✅ Monaco change detection active');
    }
    
    // ========================================================================
    // EVENT LISTENERS
    // ========================================================================
    
    // Focus editor event
    window.addEventListener('focus-editor', (event) => {
        const { path } = event.detail;
        console.log('[ExplorerIntegration] 🎯 Focusing editor:', path);
        
        // Find and switch to the tab with this file path
        if (window.switchToTab) {
            window.switchToTab(path);
        }
    });
    
    // Save file event
    window.addEventListener('save-file', (event) => {
        const { path } = event.detail;
        console.log('[ExplorerIntegration] 💾 Saving file:', path);
        
        if (window.saveFile) {
            window.saveFile(path);
        }
    });
    
    // Save file as event
    window.addEventListener('save-file-as', (event) => {
        const { path } = event.detail;
        console.log('[ExplorerIntegration] 💾 Save file as:', path);
        
        if (window.saveFileAs) {
            window.saveFileAs(path);
        }
    });
    
    // Close editor event
    window.addEventListener('close-editor', (event) => {
        const { path } = event.detail;
        console.log('[ExplorerIntegration] ✖️ Closing editor:', path);
        
        if (window.closeTabByPath) {
            window.closeTabByPath(path);
        }
    });
    
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    function getCurrentTab() {
        // Get current active tab from various tab system implementations
        
        // Try window.tabSystem first
        if (window.tabSystem && window.tabSystem.activeTab) {
            return window.tabSystem.activeTab;
        }
        
        // Try to find active Monaco editor tab
        const activeTabs = document.querySelectorAll('.editor-tab.active');
        if (activeTabs.length > 0) {
            const activeTab = activeTabs[0];
            return {
                filePath: activeTab.dataset.filePath || activeTab.getAttribute('data-file-path'),
                filename: activeTab.dataset.filename || activeTab.textContent.trim(),
                language: activeTab.dataset.language || 'plaintext'
            };
        }
        
        // Try to get from Monaco editor model
        if (window.editor && window.monaco) {
            const model = window.editor.getModel();
            if (model) {
                const uri = model.uri.toString();
                return {
                    filePath: uri,
                    filename: uri.split('/').pop() || 'untitled',
                    language: model.getLanguageId()
                };
            }
        }
        
        return null;
    }
    
    console.log('[ExplorerIntegration] ✅ File explorer integration complete');
});

})();

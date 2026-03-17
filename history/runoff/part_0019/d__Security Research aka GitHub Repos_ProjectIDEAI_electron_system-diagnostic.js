/**
 * BigDaddyG IDE - Comprehensive System Diagnostic
 * Checks ALL connections, wiring, and functionality
 */

(function() {
'use strict';

class SystemDiagnostic {
    constructor() {
        this.results = {
            critical: [],
            warnings: [],
            info: [],
            passed: 0,
            failed: 0
        };
    }
    
    async runFullDiagnostic() {
        console.log('%c╔══════════════════════════════════════════════════╗', 'color: cyan; font-weight: bold');
        console.log('%c║   🔍 BigDaddyG IDE System Diagnostic           ║', 'color: cyan; font-weight: bold');
        console.log('%c╚══════════════════════════════════════════════════╝', 'color: cyan; font-weight: bold');
        console.log('');
        
        // Run all diagnostics
        await this.checkCoreSystems();
        await this.checkBrowserSystems();
        await this.checkEditorSystems();
        await this.checkAISystems();
        await this.checkTerminalSystems();
        await this.checkFileSystems();
        await this.checkUISystems();
        await this.checkIntegrations();
        
        // Generate report
        this.generateReport();
        
        return this.results;
    }
    
    async checkCoreSystems() {
        console.log('%c\n━━━ 🔷 CORE SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check Electron
        this.check('Electron API', !!window.electron, 
            'window.electron available', 
            'Electron API not available - running in browser mode');
        
        // Check Monaco Editor
        this.check('Monaco Editor', !!window.monaco, 
            'Monaco editor loaded', 
            'Monaco editor not loaded');
        
        // Check window.editor instance
        this.check('Editor Instance', !!window.editor, 
            'Editor instance created', 
            'Editor instance not created yet');
        
        // Check tab system
        this.check('Tab System', !!window.tabSystem, 
            'Tab system initialized', 
            'Tab system not initialized');
        
        // Check hotkey manager
        this.check('Hotkey Manager', !!window.hotkeyManager, 
            'Hotkey manager active', 
            'Hotkey manager not loaded');
        
        // Check status manager
        this.check('Status Manager', !!window.statusManager, 
            'Centralized status manager active', 
            'Status manager not loaded');
        
        // Check model state manager
        this.check('Model State Manager', !!window.modelState, 
            'Model state manager active', 
            'Model state manager not loaded');
    }
    
    async checkBrowserSystems() {
        console.log('%c\n━━━ 🌐 BROWSER SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check web-browser.js
        this.check('WebBrowser Class', !!window.WebBrowser, 
            'WebBrowser class defined', 
            'WebBrowser class not found');
        
        this.check('WebBrowser Instance', !!window.webBrowser, 
            'webBrowser instance created', 
            'webBrowser instance not created');
        
        // Check browser-panel.js
        this.check('BrowserPanel Class', !!window.BrowserPanel, 
            'BrowserPanel class defined', 
            'BrowserPanel class not found');
        
        this.check('BrowserPanel Instance', !!window.browserPanel, 
            'browserPanel instance created', 
            'browserPanel instance not created');
        
        // Check if instances have required methods
        if (window.webBrowser) {
            this.check('WebBrowser.toggleBrowser()', 
                typeof window.webBrowser.toggleBrowser === 'function',
                'toggleBrowser method exists',
                'toggleBrowser method missing');
                
            this.check('WebBrowser.navigate()', 
                typeof window.webBrowser.navigate === 'function',
                'navigate method exists',
                'navigate method missing');
        }
        
        if (window.browserPanel) {
            this.check('BrowserPanel.toggle()', 
                typeof window.browserPanel.toggle === 'function',
                'toggle method exists',
                'toggle method missing');
                
            this.check('BrowserPanel.show()', 
                typeof window.browserPanel.show === 'function',
                'show method exists',
                'show method missing');
        }
        
        // Check browser UI elements
        const browserContainer = document.getElementById('web-browser-container');
        this.check('Browser UI Container', !!browserContainer, 
            'Browser container element exists', 
            'Browser container not found in DOM');
        
        const browserPanel = document.getElementById('browser-panel');
        this.check('Browser Panel Element', !!browserPanel, 
            'Browser panel element exists', 
            'Browser panel not found in DOM');
        
        // Check Electron browser integration
        if (window.electron) {
            this.check('Electron Browser API', !!window.electron.browser, 
                'Electron browser API available', 
                'Electron browser API not available');
        }
    }
    
    async checkEditorSystems() {
        console.log('%c\n━━━ 📝 EDITOR SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check Monaco editor methods
        if (window.editor) {
            this.check('editor.getValue()', 
                typeof window.editor.getValue === 'function',
                'getValue method works',
                'getValue method missing');
                
            this.check('editor.setValue()', 
                typeof window.editor.setValue === 'function',
                'setValue method works',
                'setValue method missing');
                
            this.check('editor.getModel()', 
                typeof window.editor.getModel === 'function',
                'getModel method works',
                'getModel method missing');
        }
        
        // Check file explorer
        this.check('File Explorer', !!window.fileExplorer || !!window.enhancedFileExplorer, 
            'File explorer initialized', 
            'File explorer not initialized');
        
        // Check agentic file browser
        this.check('Agentic File Browser', !!window.agenticBrowser, 
            'Agentic file browser loaded', 
            'Agentic file browser not loaded');
    }
    
    async checkAISystems() {
        console.log('%c\n━━━ 🤖 AI SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check agentic executor
        this.check('Agentic Executor Function', typeof window.getAgenticExecutor === 'function', 
            'getAgenticExecutor() available', 
            'getAgenticExecutor() not defined');
        
        if (typeof window.getAgenticExecutor === 'function') {
            const executor = window.getAgenticExecutor();
            this.check('Agentic Executor Instance', !!executor, 
                'Executor instance available', 
                'Executor instance is null');
                
            if (executor) {
                this.check('Executor.executeTask()', 
                    typeof executor.executeTask === 'function',
                    'executeTask method available',
                    'executeTask method missing');
            }
        }
        
        // Check command system
        this.check('Command System', !!window.commandSystem, 
            'Command system initialized', 
            'Command system not initialized');
        
        if (window.commandSystem) {
            this.check('Command System Commands', 
                window.commandSystem.commands && window.commandSystem.commands.size > 0,
                `${window.commandSystem.commands?.size || 0} commands registered`,
                'No commands registered');
        }
        
        // Check AI response handler
        this.check('AI Response Handler', !!window.aiResponseHandler, 
            'AI response handler loaded', 
            'AI response handler not loaded');
        
        // Check unified chat
        this.check('Unified Chat Handler', !!window.unifiedChat, 
            'Unified chat handler loaded', 
            'Unified chat handler not loaded');
        
        // Check context menu executor
        this.check('Context Menu Executor', !!window.contextMenuExecutor, 
            'Context menu executor loaded', 
            'Context menu executor not loaded');
        
        // Check quick actions executor
        this.check('Quick Actions Executor', !!window.quickActionsExecutor, 
            'Quick actions executor loaded', 
            'Quick actions executor not loaded');
        
        // Check Orchestra status
        if (window.statusManager) {
            const orchestraStatus = window.statusManager.getStatus('orchestra');
            this.check('Orchestra Server', 
                orchestraStatus?.running === true,
                `Orchestra server running (checked ${orchestraStatus?.lastCheck ? 'recently' : 'never'})`,
                'Orchestra server not running or not checked');
        }
    }
    
    async checkTerminalSystems() {
        console.log('%c\n━━━ 💻 TERMINAL SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check terminal panel
        this.check('Terminal Panel', !!window.terminalPanel || !!window.terminalPanelInstance, 
            'Terminal panel initialized', 
            'Terminal panel not initialized');
        
        // Check enhanced terminal
        this.check('Enhanced Terminal', !!window.enhancedTerminal, 
            'Enhanced terminal loaded', 
            'Enhanced terminal not loaded');
        
        // Check terminal toggle function
        this.check('Terminal Toggle', 
            typeof window.toggleTerminalPanel === 'function',
            'toggleTerminalPanel() available',
            'toggleTerminalPanel() not defined');
        
        // Check unified terminal toggle
        if (window.hotkeyManager) {
            this.check('Unified Terminal Toggle', 
                typeof window.hotkeyManager.toggleUnifiedTerminal === 'function',
                'Unified toggle method available',
                'Unified toggle method missing');
        }
        
        // Check console panel
        this.check('Console Panel', !!window.consolePanelInstance, 
            'Console panel initialized', 
            'Console panel not initialized');
    }
    
    async checkFileSystems() {
        console.log('%c\n━━━ 📁 FILE SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check file explorer types
        this.check('Enhanced File Explorer', !!window.enhancedFileExplorer, 
            'Enhanced file explorer loaded', 
            'Enhanced file explorer not loaded');
        
        this.check('Agentic File Browser', !!window.agenticBrowser, 
            'Agentic file browser loaded', 
            'Agentic file browser not loaded');
        
        // Check explorer integration
        this.check('Explorer Integration', !!window.explorerIntegration, 
            'Explorer integration loaded', 
            'Explorer integration not loaded');
        
        // Check if file operations work
        if (window.electron && window.electron.file) {
            this.check('Electron File API', true, 
                'Electron file operations available', 
                '');
        } else {
            this.check('Electron File API', false, 
                '', 
                'Electron file operations not available (browser mode)');
        }
    }
    
    async checkUISystems() {
        console.log('%c\n━━━ 🎨 UI SYSTEMS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check flexible layout
        this.check('Flexible Layout System', !!window.flexibleLayout, 
            'Flexible layout system loaded', 
            'Flexible layout system not loaded');
        
        // Check floating chat
        this.check('Floating Chat', !!window.floatingChat, 
            'Floating chat loaded', 
            'Floating chat not loaded');
        
        // Check plugin marketplace
        this.check('Plugin Marketplace', !!window.pluginMarketplace, 
            'Plugin marketplace loaded', 
            'Plugin marketplace not loaded');
        
        // Check global marketplace functions
        this.check('openMarketplace()', 
            typeof window.openMarketplace === 'function',
            'openMarketplace() available',
            'openMarketplace() not defined');
        
        this.check('openModelCatalog()', 
            typeof window.openModelCatalog === 'function',
            'openModelCatalog() available',
            'openModelCatalog() not defined');
        
        // Check command palette
        this.check('Command Palette', !!window.commandPalette, 
            'Command palette loaded', 
            'Command palette not loaded');
        
        // Check theme system
        this.check('Chameleon Theme', !!window.chameleonTheme, 
            'Theme system loaded', 
            'Theme system not loaded');
    }
    
    async checkIntegrations() {
        console.log('%c\n━━━ 🔗 INTEGRATIONS ━━━', 'color: cyan; font-weight: bold; font-size: 14px');
        
        // Check memory bridge
        this.check('Memory Bridge', !!window.memoryBridge, 
            'Memory bridge loaded', 
            'Memory bridge not loaded');
        
        if (window.memoryBridge) {
            this.check('Memory Bridge Available', 
                typeof window.memoryBridge.isAvailable === 'function',
                'Memory availability check available',
                'Memory availability check missing');
        }
        
        // Check GitHub integration
        this.check('GitHub Integration', !!window.githubIntegration, 
            'GitHub integration loaded', 
            'GitHub integration not loaded');
        
        // Check speech engine
        this.check('Offline Speech Engine', !!window.voiceCodingEngine, 
            'Voice coding engine loaded', 
            'Voice coding engine not loaded');
        
        // Check background agent manager
        this.check('Background Agent Manager', !!window.backgroundAgentManager, 
            'Background agent manager loaded', 
            'Background agent manager not loaded');
    }
    
    check(name, condition, passMsg, failMsg) {
        const result = {
            name,
            passed: !!condition,
            message: condition ? passMsg : failMsg
        };
        
        if (result.passed) {
            this.results.passed++;
            console.log(`%c✅ ${name}`, 'color: #00ff00', `- ${passMsg}`);
        } else {
            this.results.failed++;
            if (failMsg.includes('not available') || failMsg.includes('browser mode')) {
                this.results.warnings.push(result);
                console.log(`%c⚠️  ${name}`, 'color: #ff8800', `- ${failMsg}`);
            } else {
                this.results.critical.push(result);
                console.log(`%c❌ ${name}`, 'color: #ff0000', `- ${failMsg}`);
            }
        }
        
        return result;
    }
    
    generateReport() {
        console.log('%c\n╔══════════════════════════════════════════════════╗', 'color: cyan; font-weight: bold');
        console.log('%c║            📊 DIAGNOSTIC SUMMARY                ║', 'color: cyan; font-weight: bold');
        console.log('%c╚══════════════════════════════════════════════════╝', 'color: cyan; font-weight: bold');
        
        const total = this.results.passed + this.results.failed;
        const percentage = total > 0 ? Math.round((this.results.passed / total) * 100) : 0;
        
        console.log('');
        console.log(`%cTotal Checks: ${total}`, 'font-weight: bold; font-size: 13px');
        console.log(`%c✅ Passed: ${this.results.passed}`, 'color: #00ff00; font-weight: bold; font-size: 13px');
        console.log(`%c❌ Failed: ${this.results.critical.length}`, 'color: #ff0000; font-weight: bold; font-size: 13px');
        console.log(`%c⚠️  Warnings: ${this.results.warnings.length}`, 'color: #ff8800; font-weight: bold; font-size: 13px');
        console.log(`%c📈 Health Score: ${percentage}%`, 'color: cyan; font-weight: bold; font-size: 14px');
        
        console.log('');
        
        if (this.results.critical.length > 0) {
            console.log('%c🔴 CRITICAL ISSUES:', 'color: #ff0000; font-weight: bold; font-size: 13px');
            this.results.critical.forEach((issue, i) => {
                console.log(`  ${i + 1}. ${issue.name}: ${issue.message}`);
            });
            console.log('');
        }
        
        if (this.results.warnings.length > 0) {
            console.log('%c🟡 WARNINGS:', 'color: #ff8800; font-weight: bold; font-size: 13px');
            this.results.warnings.forEach((warning, i) => {
                console.log(`  ${i + 1}. ${warning.name}: ${warning.message}`);
            });
            console.log('');
        }
        
        // Recommendations
        console.log('%c💡 RECOMMENDATIONS:', 'color: cyan; font-weight: bold; font-size: 13px');
        
        if (!window.electron) {
            console.log('  • Running in browser mode - some features unavailable');
            console.log('  • Run with Electron for full functionality');
        }
        
        if (!window.statusManager?.getStatus('orchestra')?.running) {
            console.log('  • Start Orchestra server for AI features');
            console.log('  • Run: npm run orchestra:start');
        }
        
        if (this.results.critical.length > 0) {
            console.log('  • Fix critical issues first - they may break functionality');
        }
        
        if (!window.browserPanel && !window.webBrowser) {
            console.log('  • Browser systems not initialized - check script loading order');
        }
        
        console.log('');
        console.log('%c═══════════════════════════════════════════════════', 'color: cyan');
        
        // Store results globally
        window.diagnosticResults = this.results;
        
        return this.results;
    }
}

// Auto-run on page load
window.addEventListener('load', () => {
    setTimeout(async () => {
        const diagnostic = new SystemDiagnostic();
        await diagnostic.runFullDiagnostic();
    }, 3000); // Wait 3s for everything to initialize
});

// Expose for manual execution
window.runSystemDiagnostic = async function() {
    const diagnostic = new SystemDiagnostic();
    return await diagnostic.runFullDiagnostic();
};

console.log('%c✅ System Diagnostic loaded', 'color: #00ff00; font-weight: bold');
console.log('%cRun manually with: runSystemDiagnostic()', 'color: #888');

})();

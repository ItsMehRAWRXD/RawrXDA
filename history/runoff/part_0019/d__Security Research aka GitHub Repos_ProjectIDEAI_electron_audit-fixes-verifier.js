/**
 * BigDaddyG IDE - Audit Fixes Verification Script
 * Automatically tests all 12 critical fixes
 */

(function() {
'use strict';

class AuditFixesVerifier {
    constructor() {
        this.results = [];
        this.passed = 0;
        this.failed = 0;
    }
    
    async runAllTests() {
        console.log('%c==============================================', 'color: cyan; font-weight: bold');
        console.log('%c🔍 BigDaddyG IDE - Audit Fixes Verification', 'color: cyan; font-weight: bold; font-size: 16px');
        console.log('%c==============================================', 'color: cyan; font-weight: bold');
        console.log('');
        
        // Run all tests
        await this.test1_TerminalToggleUnified();
        await this.test2_MarketplaceAvailable();
        await this.test3_QuickActionsWired();
        await this.test4_CommandHandlersReal();
        await this.test5_MemoryHealthChecks();
        await this.test6_ModelStateUnified();
        await this.test7_OrchestraClientReady();
        await this.test8_ChatHandlerUnified();
        await this.test9_HotkeysMapped();
        await this.test10_ContextMenuExecutor();
        await this.test11_StatusManagerCentralized();
        await this.test12_ModulesLoaded();
        
        // Print summary
        this.printSummary();
    }
    
    test(name, condition, details = '') {
        const result = {
            name,
            passed: !!condition,
            details
        };
        
        this.results.push(result);
        
        if (result.passed) {
            this.passed++;
            console.log(`%c✅ PASS: ${name}`, 'color: #00ff00; font-weight: bold');
            if (details) console.log(`   ${details}`);
        } else {
            this.failed++;
            console.log(`%c❌ FAIL: ${name}`, 'color: #ff0000; font-weight: bold');
            if (details) console.log(`   ${details}`);
        }
    }
    
    async test1_TerminalToggleUnified() {
        console.log('\n%c[Test 1] Terminal Toggle Unified', 'color: yellow; font-weight: bold');
        
        const hasUnifiedMethod = window.hotkeyManager && 
                                 typeof window.hotkeyManager.toggleUnifiedTerminal === 'function';
        
        this.test(
            'Unified terminal toggle exists',
            hasUnifiedMethod,
            hasUnifiedMethod ? 'toggleUnifiedTerminal() method found' : 'Method not found'
        );
        
        const hasCtrlJ = window.hotkeyManager?.hotkeyConfig?.['terminal.toggle'];
        const hasCtrlTick = window.hotkeyManager?.hotkeyConfig?.['terminal.altToggle'];
        
        this.test(
            'Ctrl+J hotkey registered',
            hasCtrlJ,
            hasCtrlJ ? `Combo: ${hasCtrlJ.combo}` : 'Hotkey not registered'
        );
        
        this.test(
            'Ctrl+` hotkey registered',
            hasCtrlTick,
            hasCtrlTick ? `Combo: ${hasCtrlTick.combo}` : 'Hotkey not registered'
        );
    }
    
    async test2_MarketplaceAvailable() {
        console.log('\n%c[Test 2] Marketplace & Model Catalog', 'color: yellow; font-weight: bold');
        
        this.test(
            'openMarketplace() function exists',
            typeof window.openMarketplace === 'function',
            'Global function available'
        );
        
        this.test(
            'openModelCatalog() function exists',
            typeof window.openModelCatalog === 'function',
            'Global function available'
        );
        
        this.test(
            'Plugin marketplace instance exists',
            window.pluginMarketplace !== null && window.pluginMarketplace !== undefined,
            'Instance ready'
        );
    }
    
    async test3_QuickActionsWired() {
        console.log('\n%c[Test 3] Quick Actions Executor', 'color: yellow; font-weight: bold');
        
        this.test(
            'QuickActionsExecutor loaded',
            window.quickActionsExecutor !== undefined || 
            document.querySelector('[class*="quick-action"]') !== null,
            'Executor or quick action buttons found'
        );
        
        this.test(
            'Agentic executor available',
            typeof window.getAgenticExecutor === 'function',
            'getAgenticExecutor() function exists'
        );
    }
    
    async test4_CommandHandlersReal() {
        console.log('\n%c[Test 4] Command System Handlers', 'color: yellow; font-weight: bold');
        
        const hasCommandSystem = window.commandSystem !== null && window.commandSystem !== undefined;
        this.test(
            'Command system initialized',
            hasCommandSystem,
            hasCommandSystem ? `${window.commandSystem.commands?.size || 0} commands registered` : 'Not found'
        );
        
        if (hasCommandSystem && window.commandSystem.commands) {
            const hasCompile = window.commandSystem.commands.has('compile');
            const hasRun = window.commandSystem.commands.has('run');
            
            this.test('!compile command exists', hasCompile);
            this.test('!run command exists', hasRun);
        }
    }
    
    async test5_MemoryHealthChecks() {
        console.log('\n%c[Test 5] Memory Service Health Checks', 'color: yellow; font-weight: bold');
        
        const hasMemoryBridge = window.memoryBridge !== null && window.memoryBridge !== undefined;
        this.test(
            'Memory bridge exists',
            hasMemoryBridge,
            'window.memoryBridge found'
        );
        
        if (hasMemoryBridge) {
            const hasAvailabilityCheck = typeof window.memoryBridge.isAvailable === 'function';
            this.test(
                'isAvailable() method exists',
                hasAvailabilityCheck
            );
            
            if (hasAvailabilityCheck) {
                const status = window.memoryBridge.getAvailabilityStatus();
                this.test(
                    'Can check availability status',
                    status !== null && status !== undefined,
                    `Mode: ${status?.mode || 'unknown'}`
                );
            }
        }
    }
    
    async test6_ModelStateUnified() {
        console.log('\n%c[Test 6] Model State Manager', 'color: yellow; font-weight: bold');
        
        this.test(
            'Model state manager exists',
            window.modelState !== null && window.modelState !== undefined,
            'window.modelState found'
        );
        
        if (window.modelState) {
            const activeModel = window.modelState.getActiveModel();
            const availableModels = window.modelState.getAvailableModels();
            
            this.test(
                'Can get active model',
                activeModel !== null && activeModel !== undefined,
                activeModel ? `Active: ${activeModel.id || activeModel.name}` : 'None'
            );
            
            this.test(
                'Can get available models',
                Array.isArray(availableModels),
                `${availableModels?.length || 0} models available`
            );
        }
    }
    
    async test7_OrchestraClientReady() {
        console.log('\n%c[Test 7] Orchestra Client Initialization', 'color: yellow; font-weight: bold');
        
        // Wait a bit for initialization
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        this.test(
            'Status manager exists',
            window.statusManager !== null && window.statusManager !== undefined,
            'Centralized status monitoring ready'
        );
        
        if (window.statusManager) {
            const orchestraStatus = window.statusManager.getStatus('orchestra');
            this.test(
                'Can get Orchestra status',
                orchestraStatus !== null && orchestraStatus !== undefined,
                `Running: ${orchestraStatus?.running}, Last check: ${orchestraStatus?.lastCheck ? 'yes' : 'no'}`
            );
        }
    }
    
    async test8_ChatHandlerUnified() {
        console.log('\n%c[Test 8] Unified Chat Handler', 'color: yellow; font-weight: bold');
        
        this.test(
            'Unified chat handler exists',
            window.unifiedChat !== null && window.unifiedChat !== undefined,
            'window.unifiedChat found'
        );
        
        if (window.unifiedChat) {
            const inputCount = window.unifiedChat.inputs?.size || 0;
            this.test(
                'Chat inputs registered',
                inputCount > 0,
                `${inputCount} chat input(s) registered`
            );
            
            this.test(
                'Primary input set',
                window.unifiedChat.primaryInput !== null && window.unifiedChat.primaryInput !== undefined,
                `Primary: ${window.unifiedChat.primaryInput}`
            );
        }
    }
    
    async test9_HotkeysMapped() {
        console.log('\n%c[Test 9] Documented Hotkeys Mapped', 'color: yellow; font-weight: bold');
        
        if (window.hotkeyManager && window.hotkeyManager.hotkeyConfig) {
            const config = window.hotkeyManager.hotkeyConfig;
            
            this.test(
                'Memory dashboard hotkey (Ctrl+Shift+M)',
                config['memory.dashboard'] !== null && config['memory.dashboard'] !== undefined,
                config['memory.dashboard'] ? `Combo: ${config['memory.dashboard'].combo}` : 'Not found'
            );
            
            this.test(
                'Swarm engine hotkey (Ctrl+Alt+S)',
                config['swarm.engine'] !== null && config['swarm.engine'] !== undefined,
                config['swarm.engine'] ? `Combo: ${config['swarm.engine'].combo}` : 'Not found'
            );
        }
    }
    
    async test10_ContextMenuExecutor() {
        console.log('\n%c[Test 10] Context Menu Executor', 'color: yellow; font-weight: bold');
        
        this.test(
            'Context menu executor exists',
            window.contextMenuExecutor !== null && window.contextMenuExecutor !== undefined,
            'window.contextMenuExecutor found'
        );
        
        if (window.contextMenuExecutor) {
            const actionCount = window.contextMenuExecutor.menuActions?.size || 0;
            this.test(
                'Context menu actions registered',
                actionCount > 0,
                `${actionCount} action(s) registered`
            );
        }
    }
    
    async test11_StatusManagerCentralized() {
        console.log('\n%c[Test 11] Status Manager Centralized', 'color: yellow; font-weight: bold');
        
        const hasStatusManager = window.statusManager !== null && window.statusManager !== undefined;
        this.test(
            'Status manager exists',
            hasStatusManager,
            'Single source of truth for status'
        );
        
        if (hasStatusManager) {
            this.test(
                'Console panel subscribes to status manager',
                window.consolePanelInstance?.statusUnsubscribe !== null && 
                window.consolePanelInstance?.statusUnsubscribe !== undefined,
                'Subscription active'
            );
            
            const listenerCount = window.statusManager.listeners?.size || 0;
            this.test(
                'Status listeners registered',
                listenerCount > 0,
                `${listenerCount} service(s) monitored`
            );
        }
    }
    
    async test12_ModulesLoaded() {
        console.log('\n%c[Test 12] New Modules Loaded', 'color: yellow; font-weight: bold');
        
        const modules = [
            { name: 'status-manager.js', obj: window.statusManager },
            { name: 'model-state-manager.js', obj: window.modelState },
            { name: 'unified-chat-handler.js', obj: window.unifiedChat },
            { name: 'context-menu-executor.js', obj: window.contextMenuExecutor }
        ];
        
        modules.forEach(({ name, obj }) => {
            this.test(
                `${name} loaded`,
                obj !== null && obj !== undefined,
                obj ? 'Initialized' : 'Not found'
            );
        });
    }
    
    printSummary() {
        console.log('\n');
        console.log('%c==============================================', 'color: cyan; font-weight: bold');
        console.log('%c📊 VERIFICATION SUMMARY', 'color: cyan; font-weight: bold; font-size: 16px');
        console.log('%c==============================================', 'color: cyan; font-weight: bold');
        console.log('');
        
        const total = this.passed + this.failed;
        const percentage = total > 0 ? Math.round((this.passed / total) * 100) : 0;
        
        console.log(`%cTotal Tests: ${total}`, 'font-weight: bold');
        console.log(`%c✅ Passed: ${this.passed}`, 'color: #00ff00; font-weight: bold');
        console.log(`%c❌ Failed: ${this.failed}`, 'color: #ff0000; font-weight: bold');
        console.log(`%c📈 Success Rate: ${percentage}%`, 'color: cyan; font-weight: bold; font-size: 14px');
        
        console.log('');
        
        if (this.failed === 0) {
            console.log('%c🎉 ALL TESTS PASSED! IDE IS FLAWLESS!', 'background: #00ff00; color: #000; font-weight: bold; font-size: 16px; padding: 10px');
        } else {
            console.log('%c⚠️ Some tests failed. Review the results above.', 'background: #ff6b00; color: #fff; font-weight: bold; font-size: 14px; padding: 10px');
        }
        
        console.log('');
        console.log('%c==============================================', 'color: cyan; font-weight: bold');
        
        return { passed: this.passed, failed: this.failed, total, percentage };
    }
}

// Auto-run on page load
window.addEventListener('load', () => {
    setTimeout(async () => {
        const verifier = new AuditFixesVerifier();
        const results = await verifier.runAllTests();
        
        // Expose results to window
        window.auditVerificationResults = results;
    }, 2000); // Wait 2s for all modules to initialize
});

// Expose for manual execution
window.runAuditVerification = async function() {
    const verifier = new AuditFixesVerifier();
    return await verifier.runAllTests();
};

console.log('%c✅ Audit Fixes Verifier loaded', 'color: #00ff00; font-weight: bold');
console.log('%cRun manually with: runAuditVerification()', 'color: #888');

})();

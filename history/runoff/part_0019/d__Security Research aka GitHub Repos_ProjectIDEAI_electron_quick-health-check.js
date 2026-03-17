/**
 * Quick IDE Health Check
 * Run this in console: quickHealthCheck()
 */

window.quickHealthCheck = function() {
    console.clear();
    console.log('%c═══════════════════════════════════════', 'color: cyan; font-weight: bold');
    console.log('%c   🏥 QUICK HEALTH CHECK', 'color: cyan; font-weight: bold; font-size: 16px');
    console.log('%c═══════════════════════════════════════', 'color: cyan; font-weight: bold');
    console.log('');
    
    const checks = {
        '🌐 Browser (webBrowser)': !!window.webBrowser,
        '🌐 Browser Panel (browserPanel)': !!window.browserPanel,
        '💻 Terminal Panel': !!window.terminalPanelInstance,
        '📝 Monaco Editor': !!window.monaco,
        '✏️  Editor Instance': !!window.editor,
        '🤖 Agentic Executor': typeof window.getAgenticExecutor === 'function',
        '💬 Chat Handler': !!window.unifiedChat,
        '📁 File Explorer': !!window.enhancedFileExplorer,
        '⌨️  Hotkey Manager': !!window.hotkeyManager,
        '📊 Status Manager': !!window.statusManager,
        '🎨 Flexible Layout': !!window.flexibleLayout,
        '🛒 Marketplace': !!window.pluginMarketplace,
        '🎯 Context Menu Executor': !!window.contextMenuExecutor,
    };
    
    let passed = 0;
    let failed = 0;
    
    Object.entries(checks).forEach(([name, status]) => {
        if (status) {
            console.log(`%c✅ ${name}`, 'color: #00ff00');
            passed++;
        } else {
            console.log(`%c❌ ${name}`, 'color: #ff0000');
            failed++;
        }
    });
    
    console.log('');
    console.log(`%c📊 Score: ${passed}/${passed + failed} (${Math.round((passed / (passed + failed)) * 100)}%)`, 'font-weight: bold; font-size: 14px; color: cyan');
    
    // Quick fixes
    console.log('');
    console.log('%c🔧 Quick Fixes:', 'color: yellow; font-weight: bold');
    
    if (!window.webBrowser) {
        console.log('  💡 Browser not found - check web-browser.js loaded');
    }
    
    if (!window.editor) {
        console.log('  💡 Monaco editor not initialized - check console for errors');
    }
    
    if (!window.flexibleLayout) {
        console.log('  💡 Layout system failed - check flexible-layout-system.js');
    }
    
    if (!window.contextMenuExecutor) {
        console.log('  💡 Context menu not loaded - fixed context-menu-executor.js');
    }
    
    console.log('');
    console.log('%c═══════════════════════════════════════', 'color: cyan; font-weight: bold');
    
    return { passed, failed, percentage: Math.round((passed / (passed + failed)) * 100) };
};

console.log('%c✅ Quick health check loaded!', 'color: #00ff00; font-weight: bold');
console.log('%cRun: quickHealthCheck()', 'color: #888');

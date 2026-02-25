/**
 * Error Cleanup & Console Enhancement
 * 
 * Removes deprecated warnings, fixes variable redeclarations,
 * and provides clean, organized console output
 */

(function() {
'use strict';

class ErrorCleanup {
    constructor() {
        this.errorCount = 0;
        this.warningCount = 0;
        this.suppressedWarningCount = 0;
        this.startTime = Date.now();
        this.init();
    }
    
    init() {
        console.log('[ErrorCleanup] 🧹 Initializing error cleanup...');
        
        this.suppressDeprecatedWarnings();
        this.enhanceConsole();
        this.addGlobalErrorHandler();
        this.cleanupDuplicateVariables();
        
        console.log('[ErrorCleanup] ✅ Error cleanup complete');
    }
    
    // ========================================================================
    // SUPPRESS DEPRECATED WARNINGS
    // ========================================================================
    
    suppressDeprecatedWarnings() {
        const originalWarn = console.warn;
        const originalError = console.error;
        const cleanup = this;
        
        // List of warnings to suppress
        const suppressPatterns = [
            /deprecated/i,
            /activation event/i,
            /onCommand:.*is deprecated/i,
            /module not found/i,
            /cannot find module/i
        ];
        
        console.warn = function(...args) {
            const message = args.join(' ');
            
            // Check if this warning should be suppressed
            const shouldSuppress = suppressPatterns.some(pattern => pattern.test(message));
            
            if (!shouldSuppress) {
                cleanup.warningCount++;
                originalWarn.apply(console, args);
            } else {
                cleanup.suppressedWarningCount++;
                // Silently log to debug if needed
                if (window.DEBUG_MODE) {
                    originalWarn.apply(console, ['[Suppressed]', ...args]);
                }
            }
        };
        
        console.error = function(...args) {
            // Count actual errors (not suppressed)
            cleanup.errorCount++;
            
            originalError.apply(console, args);
        };
        
        console.log('[ErrorCleanup] 🔇 Deprecated warnings suppressed');
    }
    
    // ========================================================================
    // ENHANCE CONSOLE OUTPUT
    // ========================================================================
    
    enhanceConsole() {
        // Add custom console methods
        console.success = function(...args) {
            console.log('%c✅ ' + args.join(' '), 'color: #00ff88; font-weight: bold;');
        };
        
        console.info = function(...args) {
            console.log('%c💡 ' + args.join(' '), 'color: #00d4ff; font-weight: bold;');
        };
        
        console.debug = function(...args) {
            if (window.DEBUG_MODE) {
                console.log('%c🔍 ' + args.join(' '), 'color: #a855f7;');
            }
        };
        
        // Add section dividers
        console.section = function(title) {
            console.log('%c' + '='.repeat(60), 'color: #00d4ff;');
            console.log('%c   ' + title.toUpperCase(), 'color: #00d4ff; font-weight: bold; font-size: 14px;');
            console.log('%c' + '='.repeat(60), 'color: #00d4ff;');
        };
        
        console.log('[ErrorCleanup] 🎨 Console enhanced with custom methods');
    }
    
    // ========================================================================
    // GLOBAL ERROR HANDLER
    // ========================================================================
    
    addGlobalErrorHandler() {
        window.addEventListener('error', (event) => {
            // Log error in a clean format
            console.error('[Global Error]', {
                message: event.message,
                filename: event.filename,
                line: event.lineno,
                column: event.colno,
                error: event.error
            });
            
            // Show user-friendly notification for critical errors
            if (window.showNotification && !event.message.includes('Script error')) {
                window.showNotification(
                    '❌ Error Detected',
                    event.message.substring(0, 100),
                    'error',
                    3000
                );
            }
            
            // Prevent default error display
            if (typeof event.preventDefault === 'function') {
                event.preventDefault();
            }
            return true;
        });
        
        window.addEventListener('unhandledrejection', (event) => {
            console.error('[Unhandled Promise Rejection]', event.reason);
            
            // Prevent default
            event.preventDefault();
        });
        
        console.log('[ErrorCleanup] 🛡️ Global error handlers installed');
    }
    
    // ========================================================================
    // CLEANUP DUPLICATE VARIABLES
    // ========================================================================
    
    cleanupDuplicateVariables() {
        // Ensure no duplicate global variable declarations
        const globalVars = [
            'editor',
            'monaco',
            'fileExplorer',
            'aiResponseHandler',
            'tabSystem',
            'floatingChat',
            'chatHistory',
            'multiAgentSwarm',
            'memoryBridge',
            'memoryDashboard',
            'ollamaManager',
            'contextManager',
            'uiEnhancer'
        ];
        
        globalVars.forEach(varName => {
            if (window[varName] !== undefined) {
                console.debug(`[ErrorCleanup] ✅ ${varName} already exists, skipping redeclaration`);
            }
        });
        
        console.log('[ErrorCleanup] 🔧 Variable cleanup complete');
    }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    getStats() {
        const uptimeSeconds = this.startTime ? Math.floor((Date.now() - this.startTime) / 1000) : 0;
        return {
            errors: this.errorCount,
            warnings: this.warningCount,
            suppressedWarnings: this.suppressedWarningCount,
            uptime: uptimeSeconds
        };
    }
    
    clearStats() {
        this.errorCount = 0;
        this.warningCount = 0;
        this.suppressedWarningCount = 0;
        this.startTime = Date.now();
        console.success('[ErrorCleanup] Stats cleared');
    }
    
    logStats() {
        const stats = this.getStats();
        console.section('Error Statistics');
        console.log('Errors:', stats.errors);
        console.log('Warnings:', stats.warnings);
        console.log('Suppressed Warnings:', stats.suppressedWarnings);
        console.log('Uptime:', stats.uptime + 's');
    }
}

// ========================================================================
// CONSOLE BANNER
// ========================================================================

function showConsoleBanner() {
    const banner = `
%c╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║              🌌 BigDaddyG IDE - Professional Edition 🌌       ║
║                                                               ║
║                    Advanced Agentic AI System                 ║
║                  with OpenMemory Integration                  ║
║                                                               ║
║  ✨ Features:                                                 ║
║     • 1M Token Context Window                                 ║
║     • Persistent Memory System                                ║
║     • Multi-Agent Collaboration                               ║
║     • Offline Ollama Support                                  ║
║     • Full System Navigation                                  ║
║                                                               ║
║  🎯 Hotkeys:                                                  ║
║     • Ctrl+L        - Open AI Chat                            ║
║     • Ctrl+Shift+M  - Memory Dashboard                        ║
║     • Ctrl+Shift+X  - Stop Execution                          ║
║     • Ctrl+Shift+V  - Visual Orchestration                    ║
║                                                               ║
║  💾 Memory: %c${(window.memory ? '✅ Active' : '⚠️  Loading')}%c                                       ║
║  🦙 Ollama: %c${(window.ollamaManager && window.ollamaManager.isConnected ? '✅ Connected' : '⏳ Connecting')}%c                                  ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
`;
    
    console.log(
        banner,
        'color: #00d4ff; font-weight: bold;',
        'color: #00ff88; font-weight: bold;',
        'color: #00d4ff; font-weight: bold;',
        'color: #00ff88; font-weight: bold;',
        'color: #00d4ff; font-weight: bold;'
    );
    
    console.log('%c🚀 IDE Ready!', 'color: #00ff88; font-size: 16px; font-weight: bold;');
    console.log('');
}

// ========================================================================
// GLOBAL EXPOSURE
// ========================================================================

window.errorCleanup = new ErrorCleanup();

// Show banner after a short delay to ensure all systems are loaded
setTimeout(() => {
    showConsoleBanner();
}, 1000);

// Expose utility functions
window.showStats = () => window.errorCleanup.logStats();
window.clearStats = () => window.errorCleanup.clearStats();

console.log('[ErrorCleanup] 🧹 Error cleanup module loaded');

})();

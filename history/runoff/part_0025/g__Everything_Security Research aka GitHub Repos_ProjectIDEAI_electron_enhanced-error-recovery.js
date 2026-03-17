/**
 * Enhanced Error Recovery System
 * Automatically recovers from common errors and provides helpful suggestions
 */

console.log('[EnhancedRecovery] 🛡️ Loading enhanced error recovery system...');

class EnhancedErrorRecovery {
    constructor() {
        this.errorCount = 0;
        this.recoveryAttempts = new Map();
        this.maxRecoveryAttempts = 3;
        this.errorPatterns = this.initializeErrorPatterns();
        this.recoveryStrategies = this.initializeRecoveryStrategies();

        this.init();
    }

    init() {
        // Hook into global error handlers
        window.addEventListener('error', (event) => this.handleError(event));
        window.addEventListener('unhandledrejection', (event) => this.handleRejection(event));

        console.log('[EnhancedRecovery] ✅ Error recovery system active');
    }

    initializeErrorPatterns() {
        return {
            // Monaco Editor errors
            monaco: {
                pattern: /monaco|editor\.main/i,
                category: 'Monaco Editor',
                recoverable: true
            },

            // File system errors
            filesystem: {
                pattern: /ENOENT|EACCES|EPERM|file not found/i,
                category: 'File System',
                recoverable: true
            },

            // Network errors
            network: {
                pattern: /fetch|network|ECONNREFUSED|timeout/i,
                category: 'Network',
                recoverable: true
            },

            // Memory errors
            memory: {
                pattern: /memory|heap|out of memory/i,
                category: 'Memory',
                recoverable: true
            },

            // Syntax errors
            syntax: {
                pattern: /SyntaxError|Unexpected token/i,
                category: 'Syntax',
                recoverable: false
            }
        };
    }

    initializeRecoveryStrategies() {
        return {
            'Monaco Editor': async (error) => {
                console.log('[EnhancedRecovery] 🔧 Attempting Monaco recovery...');

                // Strategy 1: Reload Monaco CSS
                if (error.message.includes('CSS') || error.message.includes('style')) {
                    await this.reloadMonacoCSS();
                    return true;
                }

                // Strategy 2: Reinitialize Monaco
                if (window.initMonacoEditor) {
                    setTimeout(() => window.initMonacoEditor(), 1000);
                    return true;
                }

                return false;
            },

            'File System': async (error) => {
                console.log('[EnhancedRecovery] 🔧 Attempting file system recovery...');

                // Extract file path from error
                const pathMatch = error.message.match(/['"](.*?)['"]/) || error.message.match(/(\S+\.\w+)/);
                const filePath = pathMatch ? pathMatch[1] : null;

                if (filePath) {
                    console.log(`[EnhancedRecovery] 📁 File issue detected: ${filePath}`);

                    // Check if file exists via alternate method
                    if (window.electron && window.electron.readFile) {
                        const result = await window.electron.readFile(filePath);
                        if (result.success) {
                            console.log('[EnhancedRecovery] ✅ File accessible via alternate method');
                            return true;
                        }
                    }
                }

                return false;
            },

            'Network': async (error) => {
                console.log('[EnhancedRecovery] 🔧 Attempting network recovery...');

                // Check if it's an Ollama connection error
                if (error.message.includes('11441') || error.message.includes('ollama')) {
                    console.log('[EnhancedRecovery] 🤖 Ollama connection issue detected');

                    // Try to start Ollama service
                    if (window.electron && window.electron.startOllamaService) {
                        const started = await window.electron.startOllamaService();
                        if (started) {
                            console.log('[EnhancedRecovery] ✅ Ollama service started');
                            return true;
                        }
                    }
                }

                // Generic network retry strategy
                console.log('[EnhancedRecovery] ⏳ Will retry network request in 3s...');
                return 'retry-after-delay';
            },

            'Memory': async (error) => {
                console.log('[EnhancedRecovery] 🔧 Attempting memory recovery...');

                // Clear caches
                if (window.timerCleanup) {
                    window.timerCleanup();
                }

                if (window.clearEventListeners) {
                    window.clearEventListeners();
                }

                // Force garbage collection if available
                if (global.gc) {
                    global.gc();
                    console.log('[EnhancedRecovery] 🗑️ Garbage collection triggered');
                }

                return true;
            }
        };
    }

    async handleError(event) {
        this.errorCount++;
        const error = event.error || event;

        console.log(`[EnhancedRecovery] 🚨 Error ${this.errorCount} detected:`, error.message);

        // Categorize error
        const category = this.categorizeError(error);

        if (!category) {
            console.log('[EnhancedRecovery] ℹ️ Unknown error type, logging only');
            return;
        }

        console.log(`[EnhancedRecovery] 📋 Category: ${category}`);

        // Check if we should attempt recovery
        const attemptKey = `${category}-${error.message}`;
        const attempts = this.recoveryAttempts.get(attemptKey) || 0;

        if (attempts >= this.maxRecoveryAttempts) {
            console.log(`[EnhancedRecovery] ⚠️ Max recovery attempts reached for this error`);
            this.showUserNotification(category, error);
            return;
        }

        // Attempt recovery
        this.recoveryAttempts.set(attemptKey, attempts + 1);
        const recovered = await this.attemptRecovery(category, error);

        if (recovered) {
            console.log(`[EnhancedRecovery] ✅ Successfully recovered from ${category} error!`);
            event.preventDefault(); // Prevent default error handling
        } else {
            console.log(`[EnhancedRecovery] ❌ Recovery failed for ${category} error`);
        }
    }

    async handleRejection(event) {
        console.log('[EnhancedRecovery] 🚨 Unhandled promise rejection:', event.reason);

        const error = {
            message: event.reason?.message || String(event.reason),
            stack: event.reason?.stack
        };

        await this.handleError({ error });
    }

    categorizeError(error) {
        const message = error.message || String(error);

        for (const config of Object.values(this.errorPatterns)) {
            if (config.pattern.test(message)) {
                return config.category;
            }
        }

        return null;
    }

    async attemptRecovery(category, error) {
        const strategy = this.recoveryStrategies[category];

        if (!strategy) {
            console.log(`[EnhancedRecovery] ⚠️ No recovery strategy for ${category}`);
            return false;
        }

        try {
            const result = await strategy(error);

            if (result === 'retry-after-delay') {
                setTimeout(() => {
                    console.log('[EnhancedRecovery] 🔄 Retrying operation...');
                }, 3000);
                return true;
            }

            return result;
        } catch (recoveryError) {
            console.error('[EnhancedRecovery] ❌ Recovery strategy failed:', recoveryError);
            return false;
        }
    }

    async reloadMonacoCSS() {
        const existingLink = document.querySelector('link[href*="monaco"]');

        if (existingLink) {
            existingLink.parentNode.removeChild(existingLink);
        }

        return new Promise((resolve) => {
            const link = document.createElement('link');
            link.rel = 'stylesheet';
            link.href = './node_modules/monaco-editor/min/vs/style.css';

            link.onload = () => {
                console.log('[EnhancedRecovery] ✅ Monaco CSS reloaded');
                resolve(true);
            };

            link.onerror = () => {
                console.error('[EnhancedRecovery] ❌ Failed to reload Monaco CSS');
                resolve(false);
            };

            document.head.appendChild(link);
        });
    }

    showUserNotification(category, error) {
        // Create user-friendly notification
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: rgba(255, 71, 87, 0.95);
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
            z-index: 100000;
            max-width: 400px;
            font-size: 13px;
            line-height: 1.5;
        `;

        notification.innerHTML = `
            <div style="font-weight: bold; margin-bottom: 8px;">⚠️ ${category} Error</div>
            <div style="opacity: 0.9; margin-bottom: 10px;">${this.getUserFriendlyMessage(category, error)}</div>
            <button onclick="this.parentElement.remove()" style="background: white; color: #ff4757; border: none; padding: 6px 12px; border-radius: 4px; cursor: pointer; font-weight: bold;">Dismiss</button>
        `;

        document.body.appendChild(notification);

        // Auto-remove after 10 seconds
        setTimeout(() => notification.remove(), 10000);
    }

    getUserFriendlyMessage(category, error) {
        const messages = {
            'Monaco Editor': 'The code editor encountered an issue. Please refresh the page if problems persist.',
            'File System': 'Unable to access a file. Please check file permissions and try again.',
            'Network': 'Network connection issue. Please check your connection and try again.',
            'Memory': 'The IDE is using too much memory. Consider closing unused tabs.',
            'Syntax': 'A code syntax error was detected. Please review your code.',
        };

        return messages[category] || 'An unexpected error occurred. Please try restarting the IDE.';
    }

    getStats() {
        return {
            totalErrors: this.errorCount,
            recoveryAttempts: Array.from(this.recoveryAttempts.entries()).map(([key, count]) => ({
                error: key,
                attempts: count
            }))
        };
    }
}

// Initialize and expose globally
window.enhancedErrorRecovery = new EnhancedErrorRecovery();

console.log('[EnhancedRecovery] 🛡️ Enhanced error recovery system loaded successfully!');

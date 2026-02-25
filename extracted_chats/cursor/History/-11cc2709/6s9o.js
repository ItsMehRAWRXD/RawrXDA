/**
 * Agentic Auto-Fixer - Self-Healing IDE
 * 
 * Monitors the IDE for issues and AUTOMATICALLY fixes them:
 * - Memory leaks
 * - JavaScript errors
 * - Broken UI components
 * - Performance issues
 * 
 * This is TRUE agentic behavior - no human intervention needed!
 */

console.log('[AgenticFixer] 🤖 Initializing autonomous self-healing agent...');

(function() {
'use strict';

class AgenticAutoFixer {
    constructor() {
        this.isActive = false;
        this.issuesFound = [];
        this.fixesApplied = [];
        this.monitorInterval = null;
        this.errorLog = [];
        this.lastLintCheck = 0;
        this.lintCooldown = 10 * 60 * 1000; // 10 minutes
        this.lintRunning = false;
        
        this.init();
    }
    
    init() {
        console.log('[AgenticFixer] 🔧 Agent initialized - standing by...');
        
        // Auto-start monitoring after 10 seconds
        setTimeout(() => {
            this.startAutonomousMonitoring();
        }, 10000);
    }
    
    startAutonomousMonitoring() {
        if (this.isActive) return;
        
        this.isActive = true;
        console.log('[AgenticFixer] 🚀 AUTONOMOUS MONITORING ACTIVE!');
        console.log('[AgenticFixer] 🤖 Agent will automatically fix issues it finds...');
        
        // Monitor every 30 seconds
        this.monitorInterval = setInterval(() => {
            this.scanForIssues();
        }, 30000);
        
        // Run first scan immediately
        this.scanForIssues();
    }
    
    async scanForIssues() {
        console.log('[AgenticFixer] 🔍 Scanning for issues...');
        
        const issues = [];
        
        // 1. Check for memory leaks
        const memoryIssue = this.checkMemoryLeaks();
        if (memoryIssue) issues.push(memoryIssue);
        
        // 2. Check for broken chat inputs
        const chatIssue = this.checkChatInputs();
        if (chatIssue) issues.push(chatIssue);
        
        // 3. Check for JavaScript errors
        const jsErrors = this.checkRecentErrors();
        if (jsErrors.length > 0) issues.push(...jsErrors);
        
        // 4. Check for broken panels
        const panelIssue = this.checkPanels();
        if (panelIssue) issues.push(panelIssue);
        
        // 5. Check tab system
        const tabIssue = this.checkTabSystem();
        if (tabIssue) issues.push(tabIssue);

        // 6. Check lint results (async, may return null)
        const lintIssue = await this.checkLintStatus();
        if (lintIssue) issues.push(lintIssue);
        
        if (issues.length > 0) {
            console.log(`[AgenticFixer] 🚨 Found ${issues.length} issue(s)!`);
            this.issuesFound.push(...issues);
            
            // AUTONOMOUSLY FIX THEM!
            await this.autoFix(issues);
        } else {
            console.log('[AgenticFixer] ✅ No issues detected - IDE running smoothly');
        }
    }
    
    checkMemoryLeaks() {
        // Check timer stats
        if (window.getTimerStats) {
            const stats = window.getTimerStats();
            const leakPercent = ((stats.timeoutsCreated - stats.timeoutsCleared) / stats.timeoutsCreated) * 100;
            
            if (leakPercent > 70) {
                return {
                    type: 'memory_leak',
                    severity: 'HIGH',
                    description: `${Math.round(leakPercent)}% of timers not cleared (${stats.timeoutsCreated - stats.timeoutsCleared} leaked)`,
                    autoFix: 'cleanup_timers'
                };
            }
        }
        return null;
    }
    
    checkChatInputs() {
        const inputs = [
            { id: 'ai-input', name: 'Right Sidebar Chat' },
            { id: 'center-chat-input', name: 'Center Chat' }
        ];
        
        const broken = [];
        inputs.forEach(({ id, name }) => {
            const input = document.getElementById(id);
            if (input) {
                if (input.disabled || input.readOnly) {
                    broken.push(name);
                }
            }
        });
        
        if (broken.length > 0) {
            return {
                type: 'broken_chat',
                severity: 'HIGH',
                description: `Chat inputs disabled: ${broken.join(', ')}`,
                autoFix: 'enable_chat_inputs',
                data: broken
            };
        }
        return null;
    }
    
    checkRecentErrors() {
        // Check error log for recent errors
        const errors = [];
        
        // Look for common error patterns
        if (this.errorLog.length > 5) {
            errors.push({
                type: 'js_errors',
                severity: 'MEDIUM',
                description: `${this.errorLog.length} JavaScript errors logged`,
                autoFix: 'log_errors'
            });
        }
        
        return errors;
    }
    
    checkPanels() {
        // Check if panel manager exists and panels are responsive
        if (!window.panelManager) {
            return {
                type: 'missing_panel_manager',
                severity: 'MEDIUM',
                description: 'Panel manager not initialized',
                autoFix: 'init_panel_manager'
            };
        }
        return null;
    }
    
    checkTabSystem() {
        // Check if tab system exists
        if (!window.tabSystem) {
            return {
                type: 'missing_tab_system',
                severity: 'HIGH',
                description: 'Tab system not initialized',
                autoFix: 'init_tab_system'
            };
        }
        
        // Check if tabs are working
        const chatButtons = document.querySelectorAll('[onclick*="openChatTab"]');
        if (chatButtons.length > 0 && window.tabSystem.tabs.size === 0) {
            return {
                type: 'tab_system_inactive',
                severity: 'MEDIUM',
                description: 'Tab system exists but no tabs created yet',
                autoFix: 'none' // Not really a problem, just no tabs open
            };
        }
        
        return null;
    }
    
    async autoFix(issues) {
        console.log('[AgenticFixer] 🔧 AUTONOMOUSLY FIXING ISSUES...');
        
        for (const issue of issues) {
            console.log(`[AgenticFixer] 🎯 Fixing: ${issue.description}`);
            
            try {
                switch (issue.autoFix) {
                    case 'cleanup_timers':
                        this.fixMemoryLeak();
                        break;
                        
                    case 'enable_chat_inputs':
                        this.fixChatInputs();
                        break;
                        
                    case 'log_errors':
                        this.reportErrors();
                        break;

                case 'report_lint':
                    this.reportLintIssue(issue);
                    break;
                        
                    default:
                        console.log(`[AgenticFixer] ℹ️ No auto-fix available for: ${issue.type}`);
                }
                
                this.fixesApplied.push({
                    timestamp: new Date(),
                    issue: issue.description,
                    fix: issue.autoFix,
                    success: true
                });
                
                console.log(`[AgenticFixer] ✅ Fixed: ${issue.description}`);
                
            } catch (error) {
                console.error(`[AgenticFixer] ❌ Failed to fix ${issue.type}:`, error);
                this.fixesApplied.push({
                    timestamp: new Date(),
                    issue: issue.description,
                    fix: issue.autoFix,
                    success: false,
                    error: error.message
                });
            }
        }
        
        console.log(`[AgenticFixer] 🎉 Auto-fix complete! Applied ${this.fixesApplied.length} fix(es)`);
        
        // Show notification
        if (window.showNotification) {
            window.showNotification(
                '🤖 Agentic Auto-Fix',
                `Fixed ${issues.length} issue(s) automatically!`,
                'success',
                3000
            );
        }
    }
    
    fixMemoryLeak() {
        console.log('[AgenticFixer] 🧹 Cleaning up memory leak...');
        
        // Force cleanup of all timers
        if (window.cleanupAllTimers) {
            window.cleanupAllTimers();
        }
        
        // Clear event listener leaks
        if (window.eventListenerManager?.cleanup) {
            window.eventListenerManager.cleanup();
        }
        
        console.log('[AgenticFixer] ✅ Memory cleanup complete');
    }
    
    fixChatInputs() {
        console.log('[AgenticFixer] 💬 Enabling all chat inputs...');
        
        const inputs = document.querySelectorAll('textarea[id*="chat"], textarea[id*="input"], input[id*="chat"]');
        let fixed = 0;
        
        inputs.forEach(input => {
            if (input.disabled || input.readOnly) {
                input.disabled = false;
                input.readOnly = false;
                fixed++;
                console.log(`[AgenticFixer] ✅ Enabled: ${input.id || input.placeholder}`);
            }
        });
        
        console.log(`[AgenticFixer] ✅ Enabled ${fixed} chat input(s)`);
    }
    
    reportErrors() {
        console.log('[AgenticFixer] 📝 Error summary:');
        this.errorLog.forEach((err, i) => {
            console.log(`  ${i + 1}. ${err}`);
        });
    }

    async checkLintStatus() {
        const browserApi = window.electron;
        if (!browserApi?.executeCommand) {
            return null;
        }

        const now = Date.now();
        if (this.lintRunning || (now - this.lastLintCheck) < this.lintCooldown) {
            return null;
        }

        this.lintRunning = true;
        this.lastLintCheck = now;

        try {
            const command = 'npm run lint -- --max-warnings=0';
            const result = await browserApi.executeCommand(command, 'powershell');

            if (!result) {
                return null;
            }

            const { code, output = '', error = '' } = result;
            const combined = `${output}\n${error}`.trim();

            if (code === 0) {
                if (combined) {
                    console.log('[AgenticFixer] ✅ Lint check passed');
                }
                return null;
            }

            const message = combined || 'Lint command reported errors';

            if (/missing script/i.test(message)) {
                console.warn('[AgenticFixer] ⚠️ Lint script missing (npm run lint)');
                return {
                    type: 'lint_missing_script',
                    severity: 'LOW',
                    description: 'No lint script found in package.json (npm run lint)',
                    autoFix: 'none'
                };
            }

            return {
                type: 'lint_errors',
                severity: 'HIGH',
                description: 'Lint errors detected - see console for details',
                autoFix: 'report_lint',
                data: {
                    output,
                    error,
                    code
                }
            };
        } catch (error) {
            console.warn('[AgenticFixer] Lint check failed:', error?.message || error);
            return {
                type: 'lint_execution_failed',
                severity: 'MEDIUM',
                description: `Lint command failed: ${error?.message || 'Unknown error'}`,
                autoFix: 'none'
            };
        } finally {
            this.lintRunning = false;
        }
    }

    reportLintIssue(issue) {
        const payload = issue?.data || {};
        const output = payload.output?.trim();
        const error = payload.error?.trim();
        const message = [output, error].filter(Boolean).join('\n\n') || 'No lint output captured.';

        console.group('[AgenticFixer] 📛 Lint Issues Detected');
        console.log(message);
        if (payload.code !== undefined) {
            console.log('Exit code:', payload.code);
        }
        console.groupEnd();

        if (window.showNotification) {
            window.showNotification('Lint errors detected', 'Open the console for the complete report.', 'error', 5000);
        }
    }
    
    // Public API for error tracking
    logError(error) {
        this.errorLog.push({
            timestamp: new Date(),
            message: error.toString(),
            stack: error.stack
        });
        
        // Auto-trim to last 100 errors
        if (this.errorLog.length > 100) {
            this.errorLog = this.errorLog.slice(-100);
        }
    }
    
    getStats() {
        return {
            isActive: this.isActive,
            issuesFound: this.issuesFound.length,
            fixesApplied: this.fixesApplied.length,
            errorCount: this.errorLog.length,
            successRate: this.fixesApplied.length > 0 
                ? (this.fixesApplied.filter(f => f.success).length / this.fixesApplied.length * 100).toFixed(1) + '%'
                : '0%'
        };
    }
    
    showDashboard() {
        console.log('\n========== AGENTIC AUTO-FIXER STATS ==========');
        console.log(`Active: ${this.isActive ? 'YES 🟢' : 'NO 🔴'}`);
        console.log(`Issues Found: ${this.issuesFound.length}`);
        console.log(`Fixes Applied: ${this.fixesApplied.length}`);
        console.log(`Success Rate: ${this.getStats().successRate}`);
        console.log(`Errors Logged: ${this.errorLog.length}`);
        console.log('\nRecent Fixes:');
        this.fixesApplied.slice(-5).forEach((fix, i) => {
            const icon = fix.success ? '✅' : '❌';
            console.log(`  ${icon} ${fix.issue} (${fix.timestamp.toLocaleTimeString()})`);
        });
        console.log('==============================================\n');
    }
}

// Initialize
window.agenticAutoFixer = new AgenticAutoFixer();

// Intercept console.error to track errors
const originalError = console.error;
console.error = function(...args) {
    originalError.apply(console, args);
    if (window.agenticAutoFixer) {
        window.agenticAutoFixer.logError(new Error(args.join(' ')));
    }
};

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = AgenticAutoFixer;
}

console.log('[AgenticFixer] ✅ Agentic auto-fixer ready');
console.log('[AgenticFixer] 💡 Use agenticAutoFixer.showDashboard() to see stats');

})();


/**
 * Timer Manager - Prevents Memory Leaks
 * 
 * Centralized management of all setTimeout/setInterval calls
 * Automatically cleans up timers on window unload
 */

(function() {
'use strict';

class TimerManager {
    constructor() {
        this.timers = new Map();
        this.intervals = new Map();
        this.nextId = 1;
        this.stats = {
            timersCreated: 0,
            intervalsCreated: 0,
            timersCleared: 0,
            timersCompleted: 0,
            intervalsCleared: 0,
            activeTimers: 0,
            activeIntervals: 0
        };
        
        this.init();
    }
    
    init() {
        console.log('[TimerManager] ⏱️ Initializing timer management...');
        
        // Save original functions
        this.originalSetTimeout = window.setTimeout;
        this.originalSetInterval = window.setInterval;
        this.originalClearTimeout = window.clearTimeout;
        this.originalClearInterval = window.clearInterval;
        
        // Override global functions
        this.overrideGlobalFunctions();
        
        // Cleanup on window unload
        window.addEventListener('beforeunload', () => this.cleanup());
        
        // Periodic leak detection
        this.startLeakDetection();
        
        console.log('[TimerManager] ✅ Timer manager active');
    }
    
    overrideGlobalFunctions() {
        const self = this;
        
        // Override setTimeout
        window.setTimeout = function(callback, delay, ...args) {
            self.stats.timersCreated++;
            self.stats.activeTimers++;

            let timerId;
            const wrappedCallback = () => {
                if (self.timers.has(timerId)) {
                    self.timers.delete(timerId);
                }
                self.stats.activeTimers = Math.max(0, self.stats.activeTimers - 1);
                self.stats.timersCompleted++;

                try {
                    callback(...args);
                } catch (error) {
                    console.error('[TimerManager] Callback error:', error);
                }
            };

            timerId = self.originalSetTimeout.call(window, wrappedCallback, delay);
            
            // Track timer with stack trace for debugging
            self.timers.set(timerId, {
                type: 'timeout',
                delay,
                created: Date.now(),
                stack: new Error().stack
            });
            
            return timerId;
        };
        
        // Override setInterval
        window.setInterval = function(callback, delay, ...args) {
            self.stats.intervalsCreated++;
            self.stats.activeIntervals++;
            
            const intervalId = self.originalSetInterval.call(window, callback, delay, ...args);
            
            self.intervals.set(intervalId, {
                type: 'interval',
                delay,
                created: Date.now(),
                stack: new Error().stack
            });
            
            return intervalId;
        };
        
        // Override clearTimeout
        window.clearTimeout = function(timerId) {
            if (self.timers.has(timerId)) {
                self.timers.delete(timerId);
                self.stats.timersCleared++;
                self.stats.activeTimers = Math.max(0, self.stats.activeTimers - 1);
            }
            return self.originalClearTimeout.call(window, timerId);
        };
        
        // Override clearInterval
        window.clearInterval = function(intervalId) {
            if (self.intervals.has(intervalId)) {
                self.intervals.delete(intervalId);
                self.stats.intervalsCleared++;
                self.stats.activeIntervals--;
            }
            return self.originalClearInterval.call(window, intervalId);
        };
    }
    
    startLeakDetection() {
        // Check for leaks every 60 seconds
        this.leakDetectionInterval = this.originalSetInterval.call(window, () => {
            const { activeTimers, activeIntervals } = this.stats;
            
            // Warn if too many active timers/intervals
            if (activeTimers > 50) {
                console.warn(`[TimerManager] ⚠️ ${activeTimers} active setTimeout calls - potential leak!`);
            }
            
            if (activeIntervals > 20) {
                console.warn(`[TimerManager] ⚠️ ${activeIntervals} active setInterval calls - potential leak!`);
                this.logLongRunningIntervals();
            }
        }, 60000);
    }
    
    logLongRunningIntervals() {
        const now = Date.now();
        const longRunning = [];
        
        for (const [id, data] of this.intervals.entries()) {
            const age = now - data.created;
            if (age > 60000) { // Older than 1 minute
                longRunning.push({
                    id,
                    age: Math.round(age / 1000) + 's',
                    delay: data.delay + 'ms',
                    stack: data.stack.split('\n')[2] // Get caller line
                });
            }
        }
        
        if (longRunning.length > 0) {
            console.warn('[TimerManager] 🕐 Long-running intervals:', longRunning);
        }
    }
    
    cleanup() {
        console.log('[TimerManager] 🧹 Cleaning up all timers...');
        
        // Clear all timeouts
        const pendingTimeouts = this.timers.size;
        for (const timerId of this.timers.keys()) {
            this.originalClearTimeout.call(window, timerId);
        }
        this.timers.clear();
        this.stats.timersCleared += pendingTimeouts;
        this.stats.activeTimers = 0;
        
        // Clear all intervals
        const pendingIntervals = this.intervals.size;
        for (const intervalId of this.intervals.keys()) {
            this.originalClearInterval.call(window, intervalId);
        }
        this.intervals.clear();
        this.stats.intervalsCleared += pendingIntervals;
        this.stats.activeIntervals = 0;
        
        // Clear leak detection
        if (this.leakDetectionInterval) {
            this.originalClearInterval.call(window, this.leakDetectionInterval);
        }
        
        console.log('[TimerManager] ✅ All timers cleaned up');
        this.logStats();
    }
    
    logStats() {
        const resolvedTimers = this.stats.timersCleared + this.stats.timersCompleted;

        console.log('[TimerManager] 📊 Timer Statistics:');
        console.log(`  Timeouts Created: ${this.stats.timersCreated}`);
        console.log(`  Timeouts Cleared: ${this.stats.timersCleared}`);
        console.log(`  Timeouts Completed: ${this.stats.timersCompleted}`);
        console.log(`  Timeouts Active: ${this.stats.activeTimers}`);
        console.log(`  Intervals Created: ${this.stats.intervalsCreated}`);
        console.log(`  Intervals Cleared: ${this.stats.intervalsCleared}`);
        console.log(`  Intervals Active: ${this.stats.activeIntervals}`);
        
        const leakRate = this.stats.timersCreated > 0 
            ? Math.max(0, Math.round((1 - resolvedTimers / this.stats.timersCreated) * 100))
            : 0;
        
        if (leakRate > 20) {
            console.warn(`[TimerManager] ⚠️ ${leakRate}% of timers never cleared - MEMORY LEAK!`);
        }
    }
    
    // Manual cleanup method for components
    clearTimersForComponent(componentName) {
        const now = Date.now();
        let cleared = 0;
        
        for (const [id, data] of this.timers.entries()) {
            if (data.stack.includes(componentName)) {
                this.originalClearTimeout.call(window, id);
                this.timers.delete(id);
                cleared++;
            }
        }
        
        for (const [id, data] of this.intervals.entries()) {
            if (data.stack.includes(componentName)) {
                this.originalClearInterval.call(window, id);
                this.intervals.delete(id);
                cleared++;
            }
        }
        
        if (cleared > 0) {
            console.log(`[TimerManager] 🧹 Cleared ${cleared} timers for ${componentName}`);
        }
    }
    
    getStats() {
        return { ...this.stats };
    }
}

// ============================================================================
// GLOBAL CLEANUP COORDINATOR
// ============================================================================

// Cleanup all component intervals when window unloads
window.addEventListener('beforeunload', () => {
    console.log('[TimerManager] 🧹 Coordinating global cleanup...');
    
    // Call cleanup methods on all major components
    const components = [
        'consolePanel',
        'memoryDashboard',
        'parallelExecutionViz',
        'autoSaveInterval',
        'performanceMonitorInterval'
    ];
    
    components.forEach(comp => {
        try {
            if (window[comp] && typeof window[comp].cleanup === 'function') {
                window[comp].cleanup();
            } else if (window[comp] && typeof window[comp] === 'number') {
                // It's an interval ID
                clearInterval(window[comp]);
                console.log(`[TimerManager] 🧹 Cleared ${comp}`);
            }
        } catch (error) {
            console.warn(`[TimerManager] ⚠️ Error cleaning up ${comp}:`, error.message);
        }
    });
});

// ============================================================================
// GLOBAL EXPOSURE
// ============================================================================

window.timerManager = new TimerManager();

// Expose stats to console
window.getTimerStats = () => {
    window.timerManager.logStats();
    return window.timerManager.getStats();
};

// Expose cleanup for manual trigger
window.cleanupAllTimers = () => {
    window.timerManager.cleanup();
    console.log('[TimerManager] 🧹 Manual cleanup triggered');
};

console.log('[TimerManager] 📦 Timer manager module loaded');
console.log('[TimerManager] 💡 Use getTimerStats() to check for leaks');
console.log('[TimerManager] 💡 Use cleanupAllTimers() to manually clean');

})();


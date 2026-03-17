/**
 * BigDaddyG IDE - Remote Logger
 * Logs ALL events to remote terminal for debugging
 * Works even on remote PCs via WebSocket
 */

(function() {
'use strict';

class RemoteLogger {
    constructor() {
        this.logServer = null;
        this.ws = null;
        this.logBuffer = [];
        this.sessionId = `session-${Date.now()}`;
        this.remoteHost = 'localhost'; // Change to your IP for remote logging
        this.remotePort = 11442; // Remote logging port
        
        console.log('[RemoteLogger] 📡 Initializing remote logger...');
    }
    
    async init() {
        // Try to connect to remote log server
        await this.connectToLogServer();
        
        // Capture all console methods
        this.interceptConsole();
        
        // Capture all errors
        this.interceptErrors();
        
        // Capture all events
        this.captureAllEvents();
        
        // Send periodic status
        this.startStatusReporting();
        
        console.log('[RemoteLogger] ✅ Remote logger ready!');
        this.log('info', 'Remote Logger', 'Initialized successfully');
    }
    
    async connectToLogServer() {
        try {
            const wsUrl = `ws://${this.remoteHost}:${this.remotePort}`;
            console.log(`[RemoteLogger] 🔌 Connecting to: ${wsUrl}`);
            
            this.ws = new WebSocket(wsUrl);
            
            this.ws.onopen = () => {
                console.log('[RemoteLogger] ✅ Connected to remote log server');
                this.log('info', 'RemoteLogger', 'Connected to log server');
                
                // Send session info
                this.sendLog({
                    type: 'session_start',
                    sessionId: this.sessionId,
                    timestamp: Date.now(),
                    userAgent: navigator.userAgent,
                    platform: navigator.platform,
                    url: window.location.href
                });
                
                // Flush buffered logs
                this.flushBuffer();
            };
            
            this.ws.onerror = (error) => {
                console.log('[RemoteLogger] ⚠️ WebSocket error (offline logging)', error);
            };
            
            this.ws.onclose = () => {
                console.log('[RemoteLogger] 🔌 Disconnected from log server');
                // Try to reconnect after 5 seconds
                setTimeout(() => this.connectToLogServer(), 5000);
            };
            
        } catch (error) {
            console.log('[RemoteLogger] ℹ️ Remote logging unavailable (local only)');
        }
    }
    
    interceptConsole() {
        const self = this;
        
        ['log', 'warn', 'error', 'info', 'debug'].forEach(method => {
            const original = console[method];
            
            console[method] = function(...args) {
                // Check if logging is globally disabled due to pipe errors
                if (window.__consoleLoggingDisabled) {
                    return original.apply(console, args);
                }
                
                // Call original
                original.apply(console, args);
                
                // Send to remote
                self.log(method, 'Console', args.map(a => 
                    typeof a === 'object' ? JSON.stringify(a) : String(a)
                ).join(' '));
            };
        });
        
        console.log('[RemoteLogger] ✅ Console intercepted');
    }
    
    interceptErrors() {
        window.addEventListener('error', (event) => {
            if (!window.__consoleLoggingDisabled) {
                this.log('error', 'UncaughtError', {
                    message: event.message,
                    filename: event.filename,
                    lineno: event.lineno,
                    colno: event.colno,
                    error: event.error?.stack || event.error?.toString()
                });
            }
        });
        
        window.addEventListener('unhandledrejection', (event) => {
            if (!window.__consoleLoggingDisabled) {
                this.log('error', 'UnhandledPromise', {
                    reason: event.reason?.message || event.reason,
                    stack: event.reason?.stack
                });
            }
        });
        
        console.log('[RemoteLogger] ✅ Error handlers installed');
    }
    
    captureAllEvents() {
        // Mouse events
        ['click', 'dblclick', 'mousedown', 'mouseup', 'mousemove'].forEach(eventType => {
            document.addEventListener(eventType, (e) => {
                if (eventType === 'mousemove' && Math.random() > 0.1) return; // Sample 10%
                
                this.log('event', eventType, {
                    x: e.clientX,
                    y: e.clientY,
                    target: e.target.tagName,
                    id: e.target.id,
                    class: e.target.className
                });
            });
        });
        
        // Keyboard events
        ['keydown', 'keyup', 'keypress'].forEach(eventType => {
            document.addEventListener(eventType, (e) => {
                this.log('event', eventType, {
                    key: e.key,
                    code: e.code,
                    ctrl: e.ctrlKey,
                    shift: e.shiftKey,
                    alt: e.altKey
                });
            });
        });
        
        console.log('[RemoteLogger] ✅ Event capture enabled');
    }
    
    startStatusReporting() {
        // Send status every 10 seconds
        setInterval(() => {
            this.log('status', 'PeriodicStatus', {
                uptime: performance.now(),
                memory: performance.memory ? {
                    used: Math.round(performance.memory.usedJSHeapSize / 1024 / 1024),
                    total: Math.round(performance.memory.totalJSHeapSize / 1024 / 1024),
                    limit: Math.round(performance.memory.jsHeapSizeLimit / 1024 / 1024)
                } : null,
                orchestraStatus: document.getElementById('orchestra-status-text')?.textContent || 'unknown',
                openTabs: window.openTabs?.length || 0
            });
        }, 10000);
    }
    
    log(level, category, data) {
        const logEntry = {
            sessionId: this.sessionId,
            timestamp: Date.now(),
            level,
            category,
            data: typeof data === 'object' ? data : { message: data }
        };
        
        // Send to remote or buffer
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.sendLog(logEntry);
        } else {
            this.logBuffer.push(logEntry);
            
            // Limit buffer size
            if (this.logBuffer.length > 1000) {
                this.logBuffer.shift();
            }
        }
    }
    
    sendLog(logEntry) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            try {
                this.ws.send(JSON.stringify(logEntry));
            } catch (error) {
                // Handle pipe errors gracefully
                if (error.code === 'EPIPE' || error.message?.includes('EPIPE') ||
                    error.message?.includes('broken pipe') || error.message?.includes('write after end')) {
                    console.warn('[RemoteLogger] 🔇 WebSocket pipe broken, disabling remote logging');
                    this.ws.close();
                    window.__consoleLoggingDisabled = true;
                    return;
                }
                console.error('[RemoteLogger] Failed to send log:', error);
            }
        }
    }
    
    flushBuffer() {
        if (this.logBuffer.length > 0) {
            console.log(`[RemoteLogger] 📤 Flushing ${this.logBuffer.length} buffered logs`);
            
            this.logBuffer.forEach(entry => this.sendLog(entry));
            this.logBuffer = [];
        }
    }
}

// Initialize
window.remoteLogger = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.remoteLogger = new RemoteLogger();
        window.remoteLogger.init();
    });
} else {
    window.remoteLogger = new RemoteLogger();
    window.remoteLogger.init();
}

// Export
window.RemoteLogger = RemoteLogger;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RemoteLogger;
}

console.log('[RemoteLogger] 📦 Remote logger module loaded');

})(); // End IIFE


// Chatterbox Agent Helper - Monitor and communicate script status, especially requestID errors
const EventEmitter = require('events');
const { logger } = require('./logger');
const { reverseTracer } = require('./reverseTracer');
const { dataIntegrityValidator } = require('./dataIntegrity');

class ChatterboxAgent extends EventEmitter {
    constructor() {
        super();
        this.activeScripts = new Map();
        this.errorLog = [];
        this.communicationChannels = new Map();
        this.heartbeatInterval = null;
        this.requestIdErrors = new Map();
        this.stuckScripts = new Map();
        
        this.initializeChatterbox();
    }

    initializeChatterbox() {
        logger.info('[CHAT] Chatterbox Agent initialized - monitoring all scripts');
        
        // Set up communication channels
        this.setupCommunicationChannels();
        
        // Start heartbeat monitoring
        this.startHeartbeatMonitoring();
        
        // Monitor for requestID errors
        this.monitorRequestIdErrors();
        
        // Set up process monitoring
        this.setupProcessMonitoring();
    }

    // Register a script for monitoring
    registerScript(scriptId, scriptInfo) {
        const script = {
            id: scriptId,
            name: scriptInfo.name || scriptId,
            type: scriptInfo.type || 'unknown',
            status: 'starting',
            startTime: Date.now(),
            lastHeartbeat: Date.now(),
            requestCount: 0,
            errorCount: 0,
            lastError: null,
            metadata: scriptInfo.metadata || {},
            communicationChannel: scriptInfo.channel || 'default'
        };

        this.activeScripts.set(scriptId, script);
        
        // Set up communication channel if needed
        if (!this.communicationChannels.has(script.communicationChannel)) {
            this.setupCommunicationChannel(script.communicationChannel);
        }

        logger.info(`[CHAT] Script registered: ${scriptId} (${script.name})`);
        this.broadcastStatus('script_registered', { scriptId, script });
        this.emit('script_registered', { scriptId, script });
        
        return scriptId;
    }

    // Update script status
    updateScriptStatus(scriptId, status, data = {}) {
        const script = this.activeScripts.get(scriptId);
        if (!script) {
            logger.warn(`[CHAT] Attempted to update unknown script: ${scriptId}`);
            return false;
        }

        const oldStatus = script.status;
        script.status = status;
        script.lastHeartbeat = Date.now();
        script.lastUpdate = Date.now();

        // Update metadata
        Object.assign(script.metadata, data);

        // Track status changes
        if (oldStatus !== status) {
            logger.info(`[CHAT] Script status change: ${scriptId} (${script.name}) - ${oldStatus} [CHAR] ${status}`);
            this.broadcastStatus('status_change', { scriptId, script, oldStatus, newStatus: status });
            this.emit('status_change', { scriptId, script, oldStatus, newStatus: status });
        }

        // Check for stuck scripts
        this.checkForStuckScripts();

        return true;
    }

    // Record script error
    recordScriptError(scriptId, error, context = {}) {
        const script = this.activeScripts.get(scriptId);
        if (!script) {
            logger.warn(`[CHAT] Attempted to record error for unknown script: ${scriptId}`);
            return false;
        }

        script.errorCount++;
        script.lastError = {
            message: error.message,
            stack: error.stack,
            timestamp: Date.now(),
            context
        };

        const errorRecord = {
            scriptId,
            scriptName: script.name,
            error: error.message,
            stack: error.stack,
            timestamp: Date.now(),
            context,
            requestId: context.requestId || null
        };

        this.errorLog.push(errorRecord);

        // Track requestID errors specifically
        if (context.requestId) {
            this.trackRequestIdError(context.requestId, errorRecord);
        }

        logger.error(`[CHAT] Script error recorded: ${scriptId} (${script.name}) - ${error.message}`);
        this.broadcastStatus('script_error', errorRecord);
        this.emit('script_error', errorRecord);

        // Record in reverse tracer
        reverseTracer.recordCorruption(
            'scriptError',
            `Script ${scriptId} error: ${error.message}`,
            error.stack,
            'SCRIPT_ERROR'
        );

        return true;
    }

    // Track requestID errors
    trackRequestIdError(requestId, errorRecord) {
        if (!this.requestIdErrors.has(requestId)) {
            this.requestIdErrors.set(requestId, []);
        }
        
        this.requestIdErrors.get(requestId).push(errorRecord);
        
        logger.error(`[CHAT] RequestID Error tracked: ${requestId} - ${errorRecord.error}`);
        
        // Broadcast requestID error
        const requestIdErrorData = {
            requestId,
            error: errorRecord.error,
            scriptId: errorRecord.scriptId,
            timestamp: errorRecord.timestamp
        };
        this.broadcastStatus('requestid_error', requestIdErrorData);
        this.emit('requestid_error', requestIdErrorData);
    }

    // Check for stuck scripts
    checkForStuckScripts() {
        const now = Date.now();
        const stuckThreshold = 30000; // 30 seconds

        for (const [scriptId, script] of this.activeScripts) {
            const timeSinceHeartbeat = now - script.lastHeartbeat;
            
            if (timeSinceHeartbeat > stuckThreshold && script.status !== 'completed' && script.status !== 'failed') {
                if (!this.stuckScripts.has(scriptId)) {
                    this.stuckScripts.set(scriptId, {
                        scriptId,
                        scriptName: script.name,
                        stuckSince: script.lastHeartbeat,
                        duration: timeSinceHeartbeat,
                        lastStatus: script.status
                    });

                    logger.error(`[CHAT] Script appears stuck: ${scriptId} (${script.name}) - ${timeSinceHeartbeat}ms since last heartbeat`);
                    const stuckData = {
                        scriptId,
                        scriptName: script.name,
                        duration: timeSinceHeartbeat,
                        lastStatus: script.status
                    };
                    this.broadcastStatus('script_stuck', stuckData);
                    this.emit('script_stuck', stuckData);
                }
            } else if (this.stuckScripts.has(scriptId)) {
                // Script is no longer stuck
                this.stuckScripts.delete(scriptId);
                logger.info(`[CHAT] Script no longer stuck: ${scriptId} (${script.name})`);
                const unstuckData = { scriptId, scriptName: script.name };
                this.broadcastStatus('script_unstuck', unstuckData);
                this.emit('script_unstuck', unstuckData);
            }
        }
    }

    // Setup communication channels
    setupCommunicationChannels() {
        // Default channel
        this.setupCommunicationChannel('default');
        
        // Database channel
        this.setupCommunicationChannel('database');
        
        // API channel
        this.setupCommunicationChannel('api');
        
        // File system channel
        this.setupCommunicationChannel('filesystem');
    }

    // Setup individual communication channel
    setupCommunicationChannel(channelName) {
        const channel = {
            name: channelName,
            subscribers: [],
            messageQueue: [],
            lastActivity: Date.now()
        };

        this.communicationChannels.set(channelName, channel);
        logger.info(`[CHAT] Communication channel setup: ${channelName}`);
    }

    // Broadcast status to all channels
    broadcastStatus(eventType, data) {
        const message = {
            eventType,
            data,
            timestamp: Date.now(),
            source: 'chatterbox'
        };

        for (const [channelName, channel] of this.communicationChannels) {
            channel.messageQueue.push(message);
            channel.lastActivity = Date.now();
        }

        // Also log to database if available
        this.logToDatabase(message);
    }

    // Log to database
    async logToDatabase(message) {
        try {
            // This would integrate with your MongoDB or other database
            // For now, we'll just log it
            logger.info(`[CHAT] Database log: ${message.eventType} - ${JSON.stringify(message.data)}`);
        } catch (error) {
            logger.error(`[CHAT] Failed to log to database: ${error.message}`);
        }
    }

    // Start heartbeat monitoring
    startHeartbeatMonitoring() {
        this.heartbeatInterval = setInterval(() => {
            this.performHeartbeatCheck();
        }, 10000); // Every 10 seconds

        logger.info('[CHAT] Heartbeat monitoring started');
    }

    // Perform heartbeat check
    performHeartbeatCheck() {
        const now = Date.now();
        const heartbeatThreshold = 60000; // 1 minute

        for (const [scriptId, script] of this.activeScripts) {
            const timeSinceHeartbeat = now - script.lastHeartbeat;
            
            if (timeSinceHeartbeat > heartbeatThreshold) {
                logger.warn(`[CHAT] Script heartbeat overdue: ${scriptId} (${script.name}) - ${timeSinceHeartbeat}ms`);
                const heartbeatData = {
                    scriptId,
                    scriptName: script.name,
                    timeSinceHeartbeat
                };
                this.broadcastStatus('heartbeat_overdue', heartbeatData);
                this.emit('heartbeat_overdue', heartbeatData);
            }
        }

        // Clean up old data
        this.cleanupOldData();
    }

    // Monitor for requestID errors
    monitorRequestIdErrors() {
        // Override console methods to catch requestID errors
        const originalError = console.error;
        const originalLog = console.log;

        console.error = (...args) => {
            const message = args.join(' ');
            if (message.includes('Request ID:') && message.includes('ERROR_BAD_REQUEST')) {
                this.handleRequestIdError(message, args);
            }
            return originalError.apply(console, args);
        };

        console.log = (...args) => {
            const message = args.join(' ');
            if (message.includes('Request ID:') && message.includes('ERROR_BAD_REQUEST')) {
                this.handleRequestIdError(message, args);
            }
            return originalLog.apply(console, args);
        };
    }

    // Handle requestID error
    handleRequestIdError(message, args) {
        const requestIdMatch = message.match(/Request ID: ([a-f0-9-]+)/);
        const requestId = requestIdMatch ? requestIdMatch[1] : 'unknown';

        const errorRecord = {
            requestId,
            message,
            timestamp: Date.now(),
            source: 'console_monitor',
            fullArgs: args
        };

        this.trackRequestIdError(requestId, errorRecord);
        
        logger.error(`[CHAT] RequestID Error detected: ${requestId}`);
        this.broadcastStatus('requestid_error_detected', errorRecord);
    }

    // Setup process monitoring
    setupProcessMonitoring() {
        process.on('uncaughtException', (error) => {
            this.handleProcessError('uncaughtException', error);
        });

        process.on('unhandledRejection', (reason) => {
            this.handleProcessError('unhandledRejection', reason);
        });

        process.on('SIGTERM', () => {
            this.broadcastStatus('process_terminating', { signal: 'SIGTERM' });
        });

        process.on('SIGINT', () => {
            this.broadcastStatus('process_interrupted', { signal: 'SIGINT' });
        });
    }

    // Handle process errors
    handleProcessError(type, error) {
        const errorRecord = {
            type,
            error: error.message || error.toString(),
            stack: error.stack,
            timestamp: Date.now()
        };

        this.errorLog.push(errorRecord);
        
        logger.error(`[CHAT] Process error: ${type} - ${errorRecord.error}`);
        this.broadcastStatus('process_error', errorRecord);
        this.emit('process_error', errorRecord);

        // Record in reverse tracer
        reverseTracer.recordCorruption(
            'processError',
            `Process ${type}: ${errorRecord.error}`,
            error.stack,
            'PROCESS_ERROR'
        );
    }

    // Get script status
    getScriptStatus(scriptId) {
        return this.activeScripts.get(scriptId);
    }

    // Get all script statuses
    getAllScriptStatuses() {
        return Array.from(this.activeScripts.values());
    }

    // Get stuck scripts
    getStuckScripts() {
        return Array.from(this.stuckScripts.values());
    }

    // Get requestID errors
    getRequestIdErrors(requestId = null) {
        if (requestId) {
            return this.requestIdErrors.get(requestId) || [];
        }
        return Array.from(this.requestIdErrors.entries());
    }

    // Get error log
    getErrorLog(limit = 100) {
        return this.errorLog.slice(-limit);
    }

    // Cleanup old data
    cleanupOldData() {
        const cutoff = Date.now() - 300000; // 5 minutes

        // Cleanup error log
        this.errorLog = this.errorLog.filter(error => error.timestamp > cutoff);

        // Cleanup requestID errors
        for (const [requestId, errors] of this.requestIdErrors) {
            const recentErrors = errors.filter(error => error.timestamp > cutoff);
            if (recentErrors.length === 0) {
                this.requestIdErrors.delete(requestId);
            } else {
                this.requestIdErrors.set(requestId, recentErrors);
            }
        }

        // Cleanup communication channels
        for (const [channelName, channel] of this.communicationChannels) {
            channel.messageQueue = channel.messageQueue.filter(msg => msg.timestamp > cutoff);
        }
    }

    // Get comprehensive status report
    getStatusReport() {
        return {
            timestamp: Date.now(),
            activeScripts: this.activeScripts.size,
            stuckScripts: this.stuckScripts.size,
            requestIdErrors: this.requestIdErrors.size,
            totalErrors: this.errorLog.length,
            communicationChannels: this.communicationChannels.size,
            scripts: Array.from(this.activeScripts.values()),
            stuck: Array.from(this.stuckScripts.values()),
            recentErrors: this.errorLog.slice(-10),
            health: this.getHealthStatus()
        };
    }

    // Get health status
    getHealthStatus() {
        const now = Date.now();
        const healthyScripts = Array.from(this.activeScripts.values())
            .filter(script => now - script.lastHeartbeat < 60000).length;
        
        const totalScripts = this.activeScripts.size;
        const healthRatio = totalScripts > 0 ? healthyScripts / totalScripts : 1;

        return {
            status: healthRatio > 0.8 ? 'healthy' : healthRatio > 0.5 ? 'warning' : 'critical',
            healthyScripts,
            totalScripts,
            healthRatio: Math.round(healthRatio * 100),
            stuckScripts: this.stuckScripts.size,
            recentErrors: this.errorLog.filter(error => now - error.timestamp < 300000).length
        };
    }

    // Shutdown chatterbox
    shutdown() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
        }
        
        this.broadcastStatus('chatterbox_shutdown', { timestamp: Date.now() });
        logger.info('[CHAT] Chatterbox Agent shutdown');
    }
}

// Create singleton instance
const chatterbox = new ChatterboxAgent();

// Graceful shutdown
process.on('SIGTERM', () => {
    chatterbox.shutdown();
});

process.on('SIGINT', () => {
    chatterbox.shutdown();
});

module.exports = {
    ChatterboxAgent,
    chatterbox
};

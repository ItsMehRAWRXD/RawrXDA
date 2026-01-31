// RawrZ Safe Startup - Prevents malformation during system startup
const { standardizedInitializer } = require('./standardizedInit');
const { logger } = require('../utils/logger');
const { dataIntegrityValidator } = require('../utils/dataIntegrity');

class SafeStartup {
    constructor() {
        this.startupState = {
            started: false,
            starting: false,
            failed: false,
            startTime: null,
            endTime: null,
            errors: [],
            warnings: []
        };
        
        this.setupProcessHandlers();
    }
    
    // Setup process event handlers
    setupProcessHandlers() {
        process.on('SIGTERM', () => {
            this.handleShutdown('SIGTERM');
        });
        
        process.on('SIGINT', () => {
            this.handleShutdown('SIGINT');
        });
        
        process.on('uncaughtException', (error) => {
            this.handleStartupError('uncaughtException', error);
        });
        
        process.on('unhandledRejection', (reason) => {
            this.handleStartupError('unhandledRejection', reason);
        });
    }
    
    // Start the system safely
    async start() {
        if (this.startupState.started) {
            console.log('[SAFE_STARTUP] System already started');
            return true;
        }
        
        if (this.startupState.starting) {
            throw new Error('Startup already in progress');
        }
        
        try {
            this.startupState.starting = true;
            this.startupState.startTime = new Date().toISOString();
            
            console.log('[SAFE_STARTUP] Starting RawrZ Security Platform safely...');
            
            // Pre-startup validation
            await this.preStartupValidation();
            
            // Initialize all components
            console.log('[SAFE_STARTUP] Initializing components...');
            const initialized = await standardizedInitializer.initialize();
            
            if (!initialized) {
                throw new Error('Component initialization failed');
            }
            
            // Post-initialization validation
            await this.postInitializationValidation();
            
            // Mark startup as complete
            this.startupState.started = true;
            this.startupState.starting = false;
            this.startupState.endTime = new Date().toISOString();
            
            const duration = new Date(this.startupState.endTime) - new Date(this.startupState.startTime);
            
            console.log(`[SAFE_STARTUP] RawrZ Security Platform started successfully in ${duration}ms`);
            console.log('[SAFE_STARTUP] All components initialized and validated');
            
            // Log startup report
            const report = standardizedInitializer.getReport();
            console.log('[SAFE_STARTUP] Startup Report:');
            console.log(`  - Total Components: ${report.summary.totalComponents}`);
            console.log(`  - Initialized: ${report.summary.initializedComponents}`);
            console.log(`  - Failed: ${report.summary.failedComponents}`);
            console.log(`  - Required: ${report.summary.requiredComponents}`);
            console.log(`  - Optional: ${report.summary.optionalComponents}`);
            
            if (report.recommendations.length > 0) {
                console.log('[SAFE_STARTUP] Recommendations:');
                report.recommendations.forEach(rec => console.log(`  - ${rec}`));
            }
            
            return true;
            
        } catch (error) {
            this.startupState.failed = true;
            this.startupState.starting = false;
            this.startupState.endTime = new Date().toISOString();
            this.startupState.errors.push(error.message);
            
            console.error(`[SAFE_STARTUP] Startup failed: ${error.message}`);
            console.error('[SAFE_STARTUP] System will not start due to initialization failure');
            
            throw error;
        }
    }
    
    // Pre-startup validation
    async preStartupValidation() {
        console.log('[SAFE_STARTUP] Running pre-startup validation...');
        
        // Check Node.js version
        const nodeVersion = process.version;
        const majorVersion = parseInt(nodeVersion.slice(1).split('.')[0]);
        
        if (majorVersion < 14) {
            throw new Error(`Node.js version ${nodeVersion} is not supported. Minimum version is 14.x`);
        }
        
        console.log(`[SAFE_STARTUP] Node.js version: ${nodeVersion} `);
        
        // Check memory availability
        const memUsage = process.memoryUsage();
        const totalMem = memUsage.heapTotal + memUsage.external;
        const maxMem = 1024 * 1024 * 1024; // 1GB
        
        if (totalMem > maxMem) {
            this.startupState.warnings.push(`High memory usage detected: ${Math.round(totalMem / 1024 / 1024)}MB`);
        }
        
        console.log(`[SAFE_STARTUP] Memory usage: ${Math.round(totalMem / 1024 / 1024)}MB `);
        
        // Check required directories
        const requiredDirs = ['src', 'src/engines', 'src/utils', 'src/init'];
        const fs = require('fs');
        
        for (const dir of requiredDirs) {
            if (!fs.existsSync(dir)) {
                throw new Error(`Required directory missing: ${dir}`);
            }
        }
        
        console.log('[SAFE_STARTUP] Directory structure validation ');
        
        // Check for malformed files
        await this.checkForMalformedFiles();
        
        console.log('[SAFE_STARTUP] Pre-startup validation completed ');
    }
    
    // Check for malformed files
    async checkForMalformedFiles() {
        console.log('[SAFE_STARTUP] Checking for malformed files...');
        
        const fs = require('fs');
        const path = require('path');
        
        const filesToCheck = [
            'src/utils/logger.js',
            'src/utils/dataIntegrity.js',
            'src/utils/chatterbox.js',
            'src/utils/reverseTracer.js',
            'src/utils/builtinDatabase.js'
        ];
        
        for (const filePath of filesToCheck) {
            try {
                if (fs.existsSync(filePath)) {
                    const content = fs.readFileSync(filePath, 'utf8');
                    
                    // Check for UTF-8 violations
                    const utf8Validation = dataIntegrityValidator.enforceUTF8Only(content, filePath);
                    if (!utf8Validation.valid) {
                        this.startupState.warnings.push(`UTF-8 violations in ${filePath}: ${utf8Validation.violations.length} issues`);
                    }
                    
                    // Check for basic syntax issues
                    if (content.includes('\x00')) {
                        this.startupState.warnings.push(`Null bytes detected in ${filePath}`);
                    }
                    
                    // Check for suspicious patterns
                    if (content.includes('eval(') || content.includes('Function(')) {
                        this.startupState.warnings.push(`Suspicious code patterns in ${filePath}`);
                    }
                    
                } else {
                    this.startupState.warnings.push(`File not found: ${filePath}`);
                }
            } catch (error) {
                this.startupState.warnings.push(`Error checking ${filePath}: ${error.message}`);
            }
        }
        
        console.log('[SAFE_STARTUP] File validation completed ');
    }
    
    // Post-initialization validation
    async postInitializationValidation() {
        console.log('[SAFE_STARTUP] Running post-initialization validation...');
        
        // Check that all required components are initialized
        const status = standardizedInitializer.getStatus();
        const failedRequired = status.components.filter(c => c.required && !c.initialized);
        
        if (failedRequired.length > 0) {
            throw new Error(`Required components failed to initialize: ${failedRequired.map(c => c.name).join(', ')}`);
        }
        
        // Validate component functionality
        const logger = standardizedInitializer.getComponent('logger');
        if (logger && typeof logger.info === 'function') {
            logger.info('[SAFE_STARTUP] Logger validation successful');
        }
        
        const dataIntegrity = standardizedInitializer.getComponent('dataIntegrity');
        if (dataIntegrity && typeof dataIntegrity.enforceUTF8Only === 'function') {
            const testValidation = dataIntegrity.enforceUTF8Only('test', 'startup_validation');
            if (!testValidation.valid) {
                this.startupState.warnings.push('Data integrity validator test failed');
            }
        }
        
        console.log('[SAFE_STARTUP] Post-initialization validation completed ');
    }
    
    // Handle startup errors
    handleStartupError(type, error) {
        const errorInfo = {
            type,
            message: error.message || error.toString(),
            stack: error.stack,
            timestamp: new Date().toISOString()
        };
        
        this.startupState.errors.push(errorInfo);
        
        console.error(`[SAFE_STARTUP] ${type}: ${errorInfo.message}`);
        
        // Don't exit on unhandled rejections during startup
        if (type === 'unhandledRejection' && this.startupState.starting) {
            console.error('[SAFE_STARTUP] Unhandled rejection during startup - continuing...');
            return;
        }
        
        // Exit on critical errors
        if (type === 'uncaughtException') {
            console.error('[SAFE_STARTUP] Critical error - shutting down');
            process.exit(1);
        }
    }
    
    // Handle shutdown
    async handleShutdown(signal) {
        console.log(`[SAFE_STARTUP] Received ${signal}, shutting down gracefully...`);
        
        try {
            await standardizedInitializer.shutdown();
            console.log('[SAFE_STARTUP] Graceful shutdown completed');
            process.exit(0);
        } catch (error) {
            console.error(`[SAFE_STARTUP] Error during shutdown: ${error.message}`);
            process.exit(1);
        }
    }
    
    // Get startup status
    getStatus() {
        return {
            ...this.startupState,
            components: standardizedInitializer.getStatus()
        };
    }
    
    // Get startup report
    getReport() {
        return {
            startup: this.startupState,
            components: standardizedInitializer.getReport()
        };
    }
}

// Create singleton instance
const safeStartup = new SafeStartup();

// Export both class and instance
module.exports = {
    SafeStartup,
    safeStartup
};

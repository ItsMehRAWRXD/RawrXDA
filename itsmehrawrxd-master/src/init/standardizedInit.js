// RawrZ Standardized Initialization - Safe component initialization without malformation
const { safeInitializer } = require('../utils/safeInitializer');
const { logger } = require('../utils/logger');
const { dataIntegrityValidator } = require('../utils/dataIntegrity');
const { chatterbox } = require('../utils/chatterbox');
const { reverseTracer } = require('../utils/reverseTracer');
const { builtinDatabase } = require('../utils/builtinDatabase');

class StandardizedInitializer {
    constructor() {
        this.initialized = false;
        this.components = new Map();
    }
    
    // Initialize all RawrZ components safely
    async initialize() {
        if (this.initialized) {
            logger.info('[STD_INIT] System already initialized');
            return true;
        }
        
        try {
            logger.info('[STD_INIT] Starting standardized initialization process');
            
            // Register core components
            await this.registerCoreComponents();
            
            // Register engine components
            await this.registerEngineComponents();
            
            // Initialize all components
            const result = await safeInitializer.initializeAll();
            
            if (result.initialized) {
                this.initialized = true;
                logger.info('[STD_INIT] Standardized initialization completed successfully');
                
                // Generate and log initialization report
                const report = safeInitializer.generateReport();
                logger.info('[STD_INIT] Initialization Report:', report);
                
                return true;
            } else {
                throw new Error('Initialization failed');
            }
            
        } catch (error) {
            logger.error(`[STD_INIT] Initialization failed: ${error.message}`);
            throw error;
        }
    }
    
    // Register core utility components
    async registerCoreComponents() {
        logger.info('[STD_INIT] Registering core components');
        
        // Register logger (already loaded)
        safeInitializer.registerComponent('logger', logger, {
            required: true,
            order: 0,
            validate: true
        });
        
        // Register data integrity validator
        safeInitializer.registerComponent('dataIntegrity', dataIntegrityValidator, {
            required: true,
            order: 1,
            dependencies: ['logger'],
            validate: true
        });
        
        // Register chatterbox
        safeInitializer.registerComponent('chatterbox', chatterbox, {
            required: true,
            order: 2,
            dependencies: ['logger', 'dataIntegrity'],
            validate: true
        });
        
        // Register reverse tracer
        safeInitializer.registerComponent('reverseTracer', reverseTracer, {
            required: true,
            order: 3,
            dependencies: ['logger', 'dataIntegrity'],
            validate: true
        });
        
        // Register builtin database
        safeInitializer.registerComponent('builtinDatabase', builtinDatabase, {
            required: false,
            order: 4,
            dependencies: ['logger', 'dataIntegrity'],
            validate: true
        });
    }
    
    // Register engine components with lazy loading
    async registerEngineComponents() {
        logger.info('[STD_INIT] Registering engine components');
        
        // Register RawrZ Engine with lazy loading
        const rawrzEngine = await this.loadEngineSafely('rawrz-engine');
        if (rawrzEngine) {
            safeInitializer.registerComponent('rawrzEngine', rawrzEngine, {
                required: true,
                order: 5,
                dependencies: ['logger', 'dataIntegrity', 'chatterbox'],
                validate: true
            });
        }
        
        // Register individual engines with lazy loading
        const engineModules = [
            'advanced-crypto',
            'anti-analysis',
            'backup-system',
            'compression-engine',
            'digital-forensics',
            'dual-crypto-engine',
            'dual-generators',
            'full-assembly',
            'hot-patchers',
            'malware-analysis',
            'memory-manager',
            'mobile-tools',
            'network-tools',
            'polymorphic-engine',
            'reverse-engineering',
            'stealth-engine',
            'stub-generator'
        ];
        
        for (const engineName of engineModules) {
            try {
                const engine = await this.loadEngineSafely(engineName);
                if (engine) {
                    safeInitializer.registerComponent(engineName, engine, {
                        required: false,
                        order: 6,
                        dependencies: ['logger', 'dataIntegrity'],
                        validate: true
                    });
                }
            } catch (error) {
                logger.warn(`[STD_INIT] Failed to load engine ${engineName}: ${error.message}`);
            }
        }
    }
    
    // Load engine safely with error handling
    async loadEngineSafely(engineName) {
        try {
            logger.info(`[STD_INIT] Loading engine: ${engineName}`);
            
            // Validate engine name to prevent path traversal
            if (!/^[a-zA-Z0-9-_]+$/.test(engineName)) {
                throw new Error(`Invalid engine name: ${engineName}`);
            }
            
            const enginePath = `../engines/${engineName}`;
            const engine = require(enginePath);
            
            // Validate engine structure
            if (!engine || typeof engine !== 'object') {
                throw new Error(`Engine ${engineName} is not a valid object`);
            }
            
            // Check for required methods
            if (typeof engine.initialize !== 'function') {
                logger.warn(`[STD_INIT] Engine ${engineName} missing initialize method`);
            }
            
            logger.info(`[STD_INIT] Engine ${engineName} loaded successfully`);
            return engine;
            
        } catch (error) {
            logger.error(`[STD_INIT] Failed to load engine ${engineName}: ${error.message}`);
            return null;
        }
    }
    
    // Get component by name
    getComponent(name) {
        return safeInitializer.getComponent(name);
    }
    
    // Check if component is initialized
    isComponentInitialized(name) {
        return safeInitializer.isComponentInitialized(name);
    }
    
    // Get initialization status
    getStatus() {
        return safeInitializer.getStatus();
    }
    
    // Get initialization report
    getReport() {
        return safeInitializer.generateReport();
    }
    
    // Shutdown all components gracefully
    async shutdown() {
        logger.info('[STD_INIT] Starting graceful shutdown');
        
        try {
            // Shutdown components in reverse order
            const components = Array.from(safeInitializer.componentRegistry.values())
                .sort((a, b) => b.options.order - a.options.order);
            
            for (const componentInfo of components) {
                if (componentInfo.initialized && typeof componentInfo.component.shutdown === 'function') {
                    try {
                        logger.info(`[STD_INIT] Shutting down component: ${componentInfo.name}`);
                        await componentInfo.component.shutdown();
                    } catch (error) {
                        logger.error(`[STD_INIT] Error shutting down component ${componentInfo.name}: ${error.message}`);
                    }
                }
            }
            
            this.initialized = false;
            logger.info('[STD_INIT] Graceful shutdown completed');
            
        } catch (error) {
            logger.error(`[STD_INIT] Error during shutdown: ${error.message}`);
        }
    }
}

// Create singleton instance
const standardizedInitializer = new StandardizedInitializer();

// Export both class and instance
module.exports = {
    StandardizedInitializer,
    standardizedInitializer
};

// RawrZ Safe Initialization System - Prevents malformation and standardizes component initialization
const EventEmitter = require('events');
const { logger } = require('./logger');
const { dataIntegrityValidator } = require('./dataIntegrity');
const { chatterbox } = require('./chatterbox');

class SafeInitializer extends EventEmitter {
    constructor() {
        super();
        
        // Prevent duplicate initialization
        if (SafeInitializer.instance) {
            return SafeInitializer.instance;
        }
        
        this.initializationState = {
            initialized: false,
            initializing: false,
            failed: false,
            startTime: null,
            endTime: null,
            components: new Map(),
            errors: [],
            warnings: []
        };
        
        this.initializationOrder = [
            'logger',
            'dataIntegrity',
            'chatterbox',
            'reverseTracer',
            'builtinDatabase',
            'rawrzEngine',
            'engines'
        ];
        
        this.componentRegistry = new Map();
        this.validationRules = new Map();
        
        // Set singleton instance
        SafeInitializer.instance = this;
        
        this.setupValidationRules();
        this.setupErrorHandling();
    }
    
    // Setup validation rules for each component
    setupValidationRules() {
        // Logger validation
        this.validationRules.set('logger', {
            required: ['info', 'error', 'warn', 'debug'],
            validate: (component) => {
                const errors = [];
                if (!component || typeof component !== 'object') {
                    errors.push('Logger must be an object');
                } else {
                    this.validationRules.get('logger').required.forEach(method => {
                        if (typeof component[method] !== 'function') {
                            errors.push(`Logger missing required method: ${method}`);
                        }
                    });
                }
                return errors;
            }
        });
        
        // Data Integrity validation
        this.validationRules.set('dataIntegrity', {
            required: ['validateBeforeEncryption', 'validateAfterEncryption', 'enforceUTF8Only'],
            validate: (component) => {
                const errors = [];
                if (!component || typeof component !== 'object') {
                    errors.push('DataIntegrity must be an object');
                } else {
                    this.validationRules.get('dataIntegrity').required.forEach(method => {
                        if (typeof component[method] !== 'function') {
                            errors.push(`DataIntegrity missing required method: ${method}`);
                        }
                    });
                }
                return errors;
            }
        });
        
        // Chatterbox validation
        this.validationRules.set('chatterbox', {
            required: ['registerScript', 'updateScriptStatus', 'recordScriptError'],
            validate: (component) => {
                const errors = [];
                if (!component || typeof component !== 'object') {
                    errors.push('Chatterbox must be an object');
                } else {
                    this.validationRules.get('chatterbox').required.forEach(method => {
                        if (typeof component[method] !== 'function') {
                            errors.push(`Chatterbox missing required method: ${method}`);
                        }
                    });
                }
                return errors;
            }
        });
        
        // Engine validation
        this.validationRules.set('engine', {
            required: ['initialize', 'getStatus'],
            validate: (component) => {
                const errors = [];
                if (!component || typeof component !== 'object') {
                    errors.push('Engine must be an object');
                } else {
                    this.validationRules.get('engine').required.forEach(method => {
                        if (typeof component[method] !== 'function') {
                            errors.push(`Engine missing required method: ${method}`);
                        }
                    });
                }
                return errors;
            }
        });
    }
    
    // Setup error handling
    setupErrorHandling() {
        process.on('uncaughtException', (error) => {
            this.handleInitializationError('uncaughtException', error);
        });
        
        process.on('unhandledRejection', (reason) => {
            this.handleInitializationError('unhandledRejection', reason);
        });
    }
    
    // Register a component for initialization
    registerComponent(name, component, options = {}) {
        if (this.initializationState.initialized) {
            throw new Error(`Cannot register component ${name} after initialization is complete`);
        }
        
        const componentInfo = {
            name,
            component,
            options: {
                required: options.required || false,
                validate: options.validate || true,
                order: options.order || this.initializationOrder.indexOf(name),
                dependencies: options.dependencies || [],
                ...options
            },
            status: 'registered',
            errors: [],
            warnings: [],
            initialized: false
        };
        
        this.componentRegistry.set(name, componentInfo);
        logger.info(`[SAFE_INIT] Component registered: ${name}`);
        
        return componentInfo;
    }
    
    // Validate a component before initialization
    validateComponent(name, component) {
        const componentInfo = this.componentRegistry.get(name);
        if (!componentInfo) {
            return { valid: false, errors: [`Component ${name} not registered`] };
        }
        
        const errors = [];
        const warnings = [];
        
        // Check if component exists
        if (!component) {
            errors.push(`Component ${name} is null or undefined`);
            return { valid: false, errors, warnings };
        }
        
        // Run validation rules if enabled
        if (componentInfo.options.validate) {
            const rule = this.validationRules.get(name) || this.validationRules.get('engine');
            if (rule) {
                const validationErrors = rule.validate(component);
                errors.push(...validationErrors);
            }
        }
        
        // Check dependencies
        for (const dep of componentInfo.options.dependencies) {
            const depInfo = this.componentRegistry.get(dep);
            if (!depInfo || !depInfo.initialized) {
                errors.push(`Dependency ${dep} not initialized for component ${name}`);
            }
        }
        
        // UTF-8 validation for string components
        if (typeof component === 'string') {
            const utf8Validation = dataIntegrityValidator.enforceUTF8Only(component, `component_${name}`);
            if (!utf8Validation.valid) {
                errors.push(`UTF-8 validation failed for component ${name}: ${utf8Validation.violations.length} violations`);
            }
        }
        
        return {
            valid: errors.length === 0,
            errors,
            warnings
        };
    }
    
    // Initialize a single component safely
    async initializeComponent(name) {
        const componentInfo = this.componentRegistry.get(name);
        if (!componentInfo) {
            throw new Error(`Component ${name} not registered`);
        }
        
        if (componentInfo.initialized) {
            logger.info(`[SAFE_INIT] Component ${name} already initialized, skipping`);
            return componentInfo;
        }
        
        try {
            logger.info(`[SAFE_INIT] Initializing component: ${name}`);
            componentInfo.status = 'initializing';
            
            // Validate component before initialization
            const validation = this.validateComponent(name, componentInfo.component);
            if (!validation.valid) {
                componentInfo.errors.push(...validation.errors);
                componentInfo.warnings.push(...validation.warnings);
                throw new Error(`Validation failed for component ${name}: ${validation.errors.join(', ')}`);
            }
            
            // Initialize component if it has an initialize method
            if (typeof componentInfo.component.initialize === 'function') {
                logger.info(`[SAFE_INIT] Calling initialize() for component: ${name}`);
                const initResult = await componentInfo.component.initialize();
                
                if (initResult && initResult.success === false) {
                    throw new Error(`Component ${name} initialization failed: ${initResult.message || 'Unknown error'}`);
                }
            }
            
            // Mark as initialized
            componentInfo.initialized = true;
            componentInfo.status = 'initialized';
            componentInfo.initializedAt = new Date().toISOString();
            
            logger.info(`[SAFE_INIT] Component ${name} initialized successfully`);
            this.emit('component_initialized', { name, componentInfo });
            
            return componentInfo;
            
        } catch (error) {
            componentInfo.status = 'failed';
            componentInfo.errors.push(error.message);
            componentInfo.failedAt = new Date().toISOString();
            
            logger.error(`[SAFE_INIT] Failed to initialize component ${name}: ${error.message}`);
            this.emit('component_failed', { name, error: error.message, componentInfo });
            
            if (componentInfo.options.required) {
                throw error;
            } else {
                this.initializationState.warnings.push(`Optional component ${name} failed to initialize: ${error.message}`);
            }
            
            return componentInfo;
        }
    }
    
    // Initialize all components in the correct order
    async initializeAll() {
        if (this.initializationState.initialized) {
            logger.info('[SAFE_INIT] System already initialized, skipping');
            return this.initializationState;
        }
        
        if (this.initializationState.initializing) {
            throw new Error('Initialization already in progress');
        }
        
        try {
            this.initializationState.initializing = true;
            this.initializationState.startTime = new Date().toISOString();
            
            logger.info('[SAFE_INIT] Starting safe initialization process');
            
            // Sort components by initialization order
            const sortedComponents = Array.from(this.componentRegistry.values())
                .sort((a, b) => a.options.order - b.options.order);
            
            // Initialize components in order
            for (const componentInfo of sortedComponents) {
                await this.initializeComponent(componentInfo.name);
            }
            
            // Mark initialization as complete
            this.initializationState.initialized = true;
            this.initializationState.initializing = false;
            this.initializationState.endTime = new Date().toISOString();
            
            const duration = new Date(this.initializationState.endTime) - new Date(this.initializationState.startTime);
            
            logger.info(`[SAFE_INIT] Initialization completed successfully in ${duration}ms`);
            this.emit('initialization_complete', this.initializationState);
            
            return this.initializationState;
            
        } catch (error) {
            this.initializationState.failed = true;
            this.initializationState.initializing = false;
            this.initializationState.endTime = new Date().toISOString();
            this.initializationState.errors.push(error.message);
            
            logger.error(`[SAFE_INIT] Initialization failed: ${error.message}`);
            this.emit('initialization_failed', { error: error.message, state: this.initializationState });
            
            throw error;
        }
    }
    
    // Handle initialization errors
    handleInitializationError(type, error) {
        const errorInfo = {
            type,
            message: error.message || error.toString(),
            stack: error.stack,
            timestamp: new Date().toISOString()
        };
        
        this.initializationState.errors.push(errorInfo);
        
        logger.error(`[SAFE_INIT] ${type}: ${errorInfo.message}`);
        this.emit('initialization_error', errorInfo);
        
        // Record in chatterbox if available
        if (this.componentRegistry.has('chatterbox') && this.componentRegistry.get('chatterbox').initialized) {
            chatterbox.recordScriptError('safeInitializer', error, { type, context: 'initialization' });
        }
    }
    
    // Get initialization status
    getStatus() {
        return {
            ...this.initializationState,
            components: Array.from(this.componentRegistry.values()).map(info => ({
                name: info.name,
                status: info.status,
                initialized: info.initialized,
                errors: info.errors,
                warnings: info.warnings,
                required: info.options.required
            }))
        };
    }
    
    // Get component by name
    getComponent(name) {
        const componentInfo = this.componentRegistry.get(name);
        return componentInfo ? componentInfo.component : null;
    }
    
    // Check if component is initialized
    isComponentInitialized(name) {
        const componentInfo = this.componentRegistry.get(name);
        return componentInfo ? componentInfo.initialized : false;
    }
    
    // Reset initialization state (for testing)
    reset() {
        this.initializationState = {
            initialized: false,
            initializing: false,
            failed: false,
            startTime: null,
            endTime: null,
            components: new Map(),
            errors: [],
            warnings: []
        };
        
        // Reset component states
        for (const [name, componentInfo] of this.componentRegistry) {
            componentInfo.status = 'registered';
            componentInfo.initialized = false;
            componentInfo.errors = [];
            componentInfo.warnings = [];
            delete componentInfo.initializedAt;
            delete componentInfo.failedAt;
        }
        
        logger.info('[SAFE_INIT] Initialization state reset');
    }
    
    // Generate initialization report
    generateReport() {
        const status = this.getStatus();
        const report = {
            timestamp: new Date().toISOString(),
            summary: {
                totalComponents: this.componentRegistry.size,
                initializedComponents: status.components.filter(c => c.initialized).length,
                failedComponents: status.components.filter(c => c.status === 'failed').length,
                requiredComponents: status.components.filter(c => c.required).length,
                optionalComponents: status.components.filter(c => !c.required).length
            },
            status: status,
            recommendations: this.generateRecommendations(status)
        };
        
        return report;
    }
    
    // Generate recommendations based on initialization status
    generateRecommendations(status) {
        const recommendations = [];
        
        const failedRequired = status.components.filter(c => c.required && c.status === 'failed');
        if (failedRequired.length > 0) {
            recommendations.push(`CRITICAL: ${failedRequired.length} required components failed to initialize`);
        }
        
        const failedOptional = status.components.filter(c => !c.required && c.status === 'failed');
        if (failedOptional.length > 0) {
            recommendations.push(`WARNING: ${failedOptional.length} optional components failed to initialize`);
        }
        
        if (status.errors.length > 0) {
            recommendations.push(`ERROR: ${status.errors.length} initialization errors occurred`);
        }
        
        if (status.warnings.length > 0) {
            recommendations.push(`WARNING: ${status.warnings.length} initialization warnings occurred`);
        }
        
        return recommendations;
    }
}

// Create singleton instance
const safeInitializer = new SafeInitializer();

module.exports = {
    SafeInitializer,
    safeInitializer
};

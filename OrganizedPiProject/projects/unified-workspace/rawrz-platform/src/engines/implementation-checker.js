// RawrZ Implementation Checker - Comprehensive system health and implementation verification
const EventEmitter = require('events');
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { logger } = require('../utils/logger');

class ImplementationChecker extends EventEmitter {
    // Performance monitoring
    static performance = {
        monitor: (fn) => {
            const start = process.hrtime.bigint();
            const result = fn();
            const end = process.hrtime.bigint();
            const duration = Number(end - start) / 1000000; // Convert to milliseconds
            if (duration > 100) { // Log slow operations
                logger.warn(`[PERF] Slow operation: ${duration.toFixed(2)}ms`);
            }
            return result;
        }
    };
    constructor() {
        super();
        this.name = 'ImplementationChecker';
        this.version = '1.0.0';
        this.memoryManager = new Map();
        this.checks = new Map();
        this.healthStatus = new Map();
        this.moduleRegistry = new Map();
        this.checkHistory = [];
        this.autoUpdateInterval = null;
        this.checkInterval = 30000; // 30 seconds
        this.lastUpdate = null;
        this.initialized = false;
        
        // Define all expected modules and their requirements
        this.expectedModules = {
            // Core Engines
            'stub-generator': {
                requiredMethods: ['generateStub', 'compileJavaScript', 'checkPackingStatus'],
                requiredProperties: ['stubTypes', 'encryptionMethods'],
                type: 'instance'
            },
            'advanced-crypto': {
                requiredMethods: ['encrypt', 'decrypt', 'getAlgorithms', 'applyFUDEncryption'],
                requiredProperties: ['algorithms', 'metadata'],
                type: 'instance'
            },
            'dual-crypto-engine': {
                requiredMethods: ['encrypt', 'decrypt', 'getSupportedAlgorithms'],
                requiredProperties: ['engines'],
                type: 'instance'
            },
            'camellia-assembly': {
                requiredMethods: ['encrypt', 'decrypt', 'getKeySize'],
                requiredProperties: ['keySizes'],
                type: 'instance'
            },
            'compression-engine': {
                requiredMethods: ['compress', 'decompress', 'getSupportedFormats'],
                requiredProperties: ['formats'],
                type: 'instance'
            },
            'stealth-engine': {
                requiredMethods: ['enableStealth', 'getStatus', 'runDetectionScan'],
                requiredProperties: ['stealthModes', 'detectionMethods'],
                type: 'instance'
            },
            'dual-generators': {
                requiredMethods: ['generateDual', 'getGeneratorStatus', 'getGenerationStats'],
                requiredProperties: ['generators'],
                type: 'instance'
            },
            'hot-patchers': {
                requiredMethods: ['applyPatch', 'revertPatch', 'getPatchStatus'],
                requiredProperties: ['patchTypes'],
                type: 'instance'
            },
            'full-assembly': {
                requiredMethods: ['compileAssembly', 'assemble', 'getSupportedArchitectures'],
                requiredProperties: ['architectures'],
                type: 'instance'
            },
            'polymorphic-engine': {
                requiredMethods: ['transform', 'polymorphize', 'getSupportedMutationTypes'],
                requiredProperties: ['mutationTypes'],
                type: 'instance'
            },
            'anti-analysis': {
                requiredMethods: ['enableAntiAnalysis', 'obfuscateCode', 'checkForDebugging'],
                requiredProperties: ['techniques', 'obfuscationMethods'],
                type: 'instance'
            },
            'memory-manager': {
                requiredMethods: ['manageMemory', 'allocateMemory', 'getMemoryStats'],
                requiredProperties: ['memoryStats', 'memoryPools'],
                type: 'instance'
            },
            'backup-system': {
                requiredMethods: ['createBackup', 'restoreBackup', 'listBackups'],
                requiredProperties: ['backupPolicies', 'storageLocations'],
                type: 'instance'
            },
            'mobile-tools': {
                requiredMethods: ['analyzeMobile', 'analyzeApp', 'deviceSecurityScan'],
                requiredProperties: ['deviceProfiles', 'malwareSignatures'],
                type: 'instance'
            },
            'network-tools': {
                requiredMethods: ['scanPorts', 'analyzeNetwork', 'getNetworkInfo'],
                requiredProperties: ['scanTypes', 'protocols'],
                type: 'instance'
            },
            'reverse-engineering': {
                requiredMethods: ['analyzeBinary', 'disassemble', 'getAnalysisReport'],
                requiredProperties: ['analysisTypes', 'architectures'],
                type: 'instance'
            },
            'digital-forensics': {
                requiredMethods: ['analyzeEvidence', 'extractData', 'generateReport'],
                requiredProperties: ['evidenceTypes', 'extractionMethods'],
                type: 'instance'
            },
            'malware-analysis': {
                requiredMethods: ['analyzeMalware', 'extractIOCs', 'generateReport'],
                requiredProperties: ['analysisTypes', 'iocTypes'],
                type: 'instance'
            },
            'private-virus-scanner': {
                requiredMethods: ['scanFile', 'scanMultipleFiles', 'getEngineInfo'],
                requiredProperties: ['engines', 'scanQueue'],
                type: 'instance'
            },
            'jotti-scanner': {
                requiredMethods: ['scanFile', 'scanMultipleFiles', 'getScannerInfo'],
                requiredProperties: ['scanners', 'apiUrl'],
                type: 'instance'
            },
            'irc-bot-generator': {
                requiredMethods: ['generateBot', 'testBot', 'compileBot', 'generateBotAsStub'],
                requiredProperties: ['templates', 'features'],
                type: 'instance'
            },
            'advanced-fud-engine': {
                requiredMethods: ['makeCodeFUD', 'applyStaticAnalysisEvasion', 'getFUDFeatures'],
                requiredProperties: ['evasionTechniques', 'polymorphicMethods'],
                type: 'instance'
            },
            'mutex-engine': {
                requiredMethods: ['generateMutex', 'applyMutex', 'getMutexOptions'],
                requiredProperties: ['mutexPatterns', 'languages'],
                type: 'class'
            },
            'burner-encryption-engine': {
                requiredMethods: ['encrypt', 'decrypt', 'getSupportedAlgorithms'],
                requiredProperties: ['algorithms', 'keyManagement'],
                type: 'instance'
            },
            'template-generator': {
                requiredMethods: ['generateTemplate', 'getTemplates', 'validateTemplate'],
                requiredProperties: ['templateTypes', 'validationRules'],
                type: 'instance'
            }
        };
    }

    async initialize() {
        try {
            await this.loadModuleRegistry();
            await this.performImplementationCheck();
            await this.startAutoUpdate();
            this.initialized = true;
            this.emit('initialized', { checker: this.name, version: this.version });
            logger.info('Implementation Checker initialized successfully');
            return { success: true, message: 'Implementation Checker initialized' };
        } catch (error) {
            this.emit('error', { checker: this.name, error: error.message });
            logger.error('Failed to initialize Implementation Checker:', error);
            return { success: false, error: error.message };
        }
    }

    // Load module registry from rawrz-engine
    async loadModuleRegistry() {
        try {
            const rawrzEngine = require('./rawrz-engine');
            const modules = rawrzEngine.getModuleList();
            
            for (const moduleName of modules) {
                // Skip implementation-checker to prevent circular dependency
                if (moduleName === 'implementation-checker') {
                    continue;
                }
                
                try {
                    const module = await rawrzEngine.loadModule(moduleName);
                    this.moduleRegistry.set(moduleName, {
                        name: moduleName,
                        moduleLoaded: true,
                        moduleType: typeof module,
                        loaded: true,
                        lastChecked: Date.now(),
                        errors: []
                    });
                } catch (error) {
                    this.moduleRegistry.set(moduleName, {
                        name: moduleName,
                        moduleLoaded: false,
                        moduleType: 'error',
                        loaded: false,
                        lastChecked: Date.now(),
                        errors: [error.message]
                    });
                }
            }
            
            logger.info("Loaded " + this.moduleRegistry.size + " modules into registry");
            return { success: true, modules: this.moduleRegistry.size };
        } catch (error) {
            logger.error('Failed to load module registry:', error);
            return { success: false, error: error.message };
        }
    }

    // Perform comprehensive implementation check
    async performImplementationCheck() {
        const checkId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            logger.info(`Starting implementation check: ${checkId}`);
            
            const checkResult = {
                id: checkId,
                timestamp: Date.now(),
                startTime,
                modules: {},
                summary: {
                    total: 0,
                    passed: 0,
                    failed: 0,
                    warnings: 0,
                    errors: 0
                },
                recommendations: [],
                healthScore: 0
            };

            // Check each expected module
            for (const [moduleName, requirements] of Object.entries(this.expectedModules)) {
                const moduleCheck = await this.checkModule(moduleName, requirements);
                checkResult.modules[moduleName] = moduleCheck;
                
                // Update summary
                checkResult.summary.total++;
                if (moduleCheck.status === 'passed') {
                    checkResult.summary.passed++;
                } else if (moduleCheck.status === 'failed') {
                    checkResult.summary.failed++;
                } else if (moduleCheck.status === 'warning') {
                    checkResult.summary.warnings++;
                }
                
                checkResult.summary.errors += moduleCheck.errors.length;
            }

            // Calculate health score
            checkResult.healthScore = this.calculateHealthScore(checkResult);
            
            // Generate recommendations
            checkResult.recommendations = this.generateRecommendations(checkResult);
            
            // Store check result
            checkResult.endTime = Date.now();
            checkResult.duration = checkResult.endTime - checkResult.startTime;
            this.checks.set(checkId, checkResult);
            this.checkHistory.push(checkResult);
            
            // Keep only last 100 checks
            if (this.checkHistory.length > 100) {
                this.checkHistory = this.checkHistory.slice(-100);
            }
            
            this.lastUpdate = Date.now();
            this.emit('checkCompleted', checkResult);
            
            logger.info(`Implementation check completed: ${checkId}`, {
                duration: checkResult.duration,
                healthScore: checkResult.healthScore,
                passed: checkResult.summary.passed,
                failed: checkResult.summary.failed
            });
            
            return checkResult;
            
        } catch (error) {
            logger.error(`Implementation check failed: ${checkId}`, error);
            this.emit('checkFailed', { checkId, error: error.message });
            throw error;
        }
    }

    // Check individual module
    async checkModule(moduleName, requirements) {
        const moduleCheck = {
            name: moduleName,
            status: 'unknown',
            loaded: false,
            methods: {},
            properties: {},
            errors: [],
            warnings: [],
            score: 0
        };

        try {
            const moduleInfo = this.moduleRegistry.get(moduleName);
            
            if (!moduleInfo || !moduleInfo.loaded) {
                moduleCheck.status = 'failed';
                moduleCheck.errors.push("Module " + moduleName + " not loaded");
                return moduleCheck;
            }

            // Load the module fresh to avoid circular references
            const rawrzEngine = require('./rawrz-engine');
            const module = await rawrzEngine.loadModule(moduleName);
            moduleCheck.loaded = true;

            // Check if module is instance or class
            if (requirements.type === 'class') {
                // For classes, check if it can be instantiated
                try {
                    const instance = new module();
                    moduleCheck.instanceCreated = true;
                } catch (error) {
                    moduleCheck.errors.push(`Failed to instantiate class: ${error.message}`);
                }
            } else {
                // For instances, use directly
                moduleCheck.instanceAvailable = true;
            }

            // Check required methods
            for (const methodName of requirements.requiredMethods) {
                const instanceToCheck = requirements.type === 'class' ? new module() : module;
                const methodCheck = await this.checkMethod(instanceToCheck, methodName);
                moduleCheck.methods[methodName] = methodCheck;
                
                if (!methodCheck.exists) {
                    moduleCheck.errors.push("Required method " + methodName + " not found");
                } else if (!methodCheck.callable) {
                    moduleCheck.errors.push("Method " + methodName + " is not callable");
                } else if (methodCheck.error) {
                    moduleCheck.warnings.push(`Method ${methodName} error: methodCheck.error`);
                }
            }

            // Check required properties
            for (const propName of requirements.requiredProperties) {
                const instanceToCheck = requirements.type === 'class' ? new module() : module;
                const propCheck = this.checkProperty(instanceToCheck, propName);
                moduleCheck.properties[propName] = propCheck;
                
                if (!propCheck.exists) {
                    moduleCheck.errors.push("Required property " + propName + " not found");
                } else if (propCheck.empty) {
                    moduleCheck.warnings.push("Property " + propName + " is empty");
                }
            }

            // Determine overall status
            if (moduleCheck.errors.length === 0 && moduleCheck.warnings.length === 0) {
                moduleCheck.status = 'passed';
                moduleCheck.score = 100;
            } else if (moduleCheck.errors.length === 0) {
                moduleCheck.status = 'warning';
                moduleCheck.score = 80;
            } else {
                moduleCheck.status = 'failed';
                moduleCheck.score = Math.max(0, 100 - (moduleCheck.errors.length * 20));
            }

            return moduleCheck;

        } catch (error) {
            moduleCheck.status = 'failed';
            moduleCheck.errors.push(`Module check failed: ${error.message}`);
            return moduleCheck;
        }
    }

    // Check if method exists and is callable
    async checkMethod(instance, methodName) {
        const methodCheck = {
            name: methodName,
            exists: false,
            callable: false,
            error: null,
            testResult: null
        };

        try {
            if (instance && typeof instance[methodName] === 'function') {
                methodCheck.exists = true;
                methodCheck.callable = true;
                
                // Try to call the method with safe parameters
                try {
                    const testParams = this.getTestParameters(methodName);
                    const result = await instance[methodName](...testParams);
                    methodCheck.testResult = { 
                        success: true, 
                        resultType: typeof result,
                        hasResult: result !== null && result !== undefined
                    };
                } catch (error) {
                    methodCheck.error = error.message;
                }
            } else if (instance && instance[methodName] !== undefined) {
                methodCheck.exists = true;
                methodCheck.error = 'Property exists but is not a function';
            }
        } catch (error) {
            methodCheck.error = error.message;
        }

        return methodCheck;
    }

    // Check if property exists and has content
    checkProperty(instance, propName) {
        const propCheck = {
            name: propName,
            exists: false,
            empty: false,
            type: null,
            value: null
        };

        try {
            if (instance && instance[propName] !== undefined) {
                propCheck.exists = true;
                propCheck.type = typeof instance[propName];
                
                if (Array.isArray(instance[propName])) {
                    propCheck.empty = instance[propName].length === 0;
                    propCheck.value = "Array(" + instance[propName].length + ")";
                } else if (typeof instance[propName] === 'object' && instance[propName] !== null) {
                    propCheck.empty = Object.keys(instance[propName]).length === 0;
                    propCheck.value = "Object(" + Object.keys(instance[propName]).length + " keys)";
                } else {
                    propCheck.empty = !instance[propName];
                    propCheck.value = String(instance[propName]).substring(0, 100);
                }
            }
        } catch (error) {
            propCheck.error = error.message;
        }

        return propCheck;
    }

    // Get safe test parameters for method calls
    getTestParameters(methodName) {
        const safeParams = {
            'generateStub': ['test', { type: 'cpp' }],
            'generateTemplate': ['stub', { template: 'basic_cpp' }],
            'validateTemplate': [{ type: 'stub', template: 'basic_cpp', code: '#include <iostream>\nint main() { return 0; }', name: 'Basic C++ Stub', language: 'cpp' }],
            'encrypt': ['test data', 'aes-256-gcm'],
            'decrypt': ['encrypted data', 'aes-256-gcm'],
            'compress': ['test data'],
            'decompress': ['compressed data'],
            'scanPorts': ['localhost', [80, 443]],
            'analyzeMalware': ['api-server.js'],
            'createBackup': ['api-server.js'],
            'generateBot': [{ server: 'test', channel: '#test' }, [], []],
            'testBot': ['api-server.js'],
            'compileBot': ['api-server.js'],
            'generateMutex': ['test'],
            'applyMutex': ['api-server.js', 'test'],
            'scanFile': ['api-server.js'],
            'scanMultipleFiles': [['api-server.js', 'package-lock.json']],
            'makeCodeFUD': ['test code'],
            'enableStealth': ['standard'],
            'enableAntiAnalysis': ['full'],
            'manageMemory': ['allocate', { size: 1024 }],
            'generateDual': ['test', {}],
            'applyPatch': ['api-server.js', { type: 'file', data: Buffer.from('test') }],
            'revertPatch': ['test-patch-id'],
            'compileAssembly': ['mov rax, 1', { architecture: 'x64' }],
            'assemble': ['mov rax, 1', 'x64', {}],
            'transform': ['test code', {}],
            'analyzeMobile': ['test.apk', { type: 'app' }],
            'analyzeApp': ['test.apk', 'Android'],
            'deviceSecurityScan': [],
            'analyzeBinary': ['api-server.js'],
            'disassemble': ['api-server.js'],
            'analyzeEvidence': ['api-server.js'],
            'extractData': ['api-server.js'],
            'generateReport': [],
            'extractIOCs': ['api-server.js'],
            'getEngineInfo': [],
            'getScannerInfo': [],
            'getTemplates': [],
            'getAvailableFeatures': [],
            'getMutexOptions': [],
            'getFUDFeatures': [],
            'getSupportedAlgorithms': [],
            'getSupportedFormats': [],
            'getSupportedArchitectures': [],
            'getSupportedMutationTypes': [],
            'getMemoryStats': [],
            'getGeneratorStatus': [],
            'getGenerationStats': [],
            'getPatchStatus': ['test-patch-id'],
            'getStatus': [],
            'getNetworkInfo': [],
            'getAnalysisReport': [],
            'getBackupReport': [],
            'getMobileReport': []
        };

        return safeParams[methodName] || [];
    }

    // Calculate overall health score
    calculateHealthScore(checkResult) {
        if (checkResult.summary.total === 0) return 0;
        
        const totalScore = Object.values(checkResult.modules).reduce((sum, module) => {
            return sum + (module.score || 0);
        }, 0);
        
        return Math.round(totalScore / checkResult.summary.total);
    }

    // Generate recommendations based on check results
    generateRecommendations(checkResult) {
        const recommendations = [];

        // Failed modules
        const failedModules = Object.entries(checkResult.modules)
            .filter(([_, module]) => module.status === 'failed')
            .map(([name, _]) => name);

        if (failedModules.length > 0) {
            recommendations.push({
                type: 'critical',
                message: "Critical: " + failedModules.length + " modules failed implementation check",
                modules: failedModules,
                action: 'Fix failed modules immediately'
            });
        }

        // Warning modules
        const warningModules = Object.entries(checkResult.modules)
            .filter(([_, module]) => module.status === 'warning')
            .map(([name, _]) => name);

        if (warningModules.length > 0) {
            recommendations.push({
                type: 'warning',
                message: "Warning: " + warningModules.length + " modules have implementation warnings",
                modules: warningModules,
                action: 'Review and fix warnings'
            });
        }

        // Missing modules
        const expectedModules = Object.keys(this.expectedModules);
        const checkedModules = Object.keys(checkResult.modules);
        const missingModules = expectedModules.filter(name => !checkedModules.includes(name));

        if (missingModules.length > 0) {
            recommendations.push({
                type: 'info',
                message: "Info: " + missingModules.length + " expected modules not found",
                modules: missingModules,
                action: 'Implement missing modules or update expectations'
            });
        }

        // Health score recommendations
        if (checkResult.healthScore < 50) {
            recommendations.push({
                type: 'critical',
                message: 'Critical: System health score is very low',
                action: 'Immediate attention required'
            });
        } else if (checkResult.healthScore < 80) {
            recommendations.push({
                type: 'warning',
                message: 'Warning: System health score is below optimal',
                action: 'Review and improve module implementations'
            });
        }

        return recommendations;
    }

    // Start auto-update system
    async startAutoUpdate() {
        if (this.autoUpdateInterval) {
            clearInterval(this.autoUpdateInterval);
        }

        this.autoUpdateInterval = setInterval(async () => {
            try {
                await this.performImplementationCheck();
                await this.updateModuleRegistry();
            } catch (error) {
                logger.error('Auto-update check failed:', error);
            }
        }, this.checkInterval);

        logger.info("Auto-update started with " + this.checkInterval + "ms interval");
    }

    // Update module registry
    async updateModuleRegistry() {
        try {
            const rawrzEngine = require('./rawrz-engine');
            const currentModules = Array.from(this.moduleRegistry.keys());
            const availableModules = rawrzEngine.getModuleList();
            
            // Check for new modules
            for (const moduleName of availableModules) {
                if (!this.moduleRegistry.has(moduleName)) {
                    try {
                        const module = await rawrzEngine.loadModule(moduleName);
                        this.moduleRegistry.set(moduleName, {
                            name: moduleName,
                            module: module,
                            loaded: true,
                            lastChecked: Date.now(),
                            errors: []
                        });
                        logger.info(`New module discovered: ${moduleName}`);
                    } catch (error) {
                        logger.warn("Failed to load new module " + moduleName + ":", error.message);
                    }
                }
            }
            
            // Update last checked time for existing modules
            for (const [moduleName, moduleInfo] of this.moduleRegistry) {
                moduleInfo.lastChecked = Date.now();
            }
            
        } catch (error) {
            logger.error('Failed to update module registry:', error);
        }
    }

    // Get current health status
    getHealthStatus() {
        const latestCheck = this.checkHistory[this.checkHistory.length - 1];
        
        return {
            initialized: this.initialized,
            lastUpdate: this.lastUpdate,
            autoUpdateEnabled: !!this.autoUpdateInterval,
            checkInterval: this.checkInterval,
            totalModules: this.moduleRegistry.size,
            latestCheck: latestCheck ? {
                id: latestCheck.id,
                timestamp: latestCheck.timestamp,
                healthScore: latestCheck.healthScore,
                summary: latestCheck.summary
            } : null,
            recommendations: latestCheck ? latestCheck.recommendations : []
        };
    }

    // Get detailed check results
    getCheckResults(checkId = null) {
        if (checkId) {
            const result = this.checks.get(checkId);
            return result ? this.sanitizeForJSON(result) : null;
        }
        
        const results = Array.from(this.checks.values()).sort((a, b) => b.timestamp - a.timestamp);
        return results.map(result => this.sanitizeForJSON(result));
    }

    // Sanitize objects for JSON serialization to avoid circular references
    sanitizeForJSON(obj) {
        if (obj === null || typeof obj !== 'object') {
            return obj;
        }
        
        if (Array.isArray(obj)) {
            return obj.map(item => this.sanitizeForJSON(item));
        }
        
        const sanitized = {};
        for (const [key, value] of Object.entries(obj)) {
            // Skip functions, circular references, and complex objects
            if (typeof value === 'function' || 
                key === '_events' || 
                key === '_eventsCount' ||
                key === '_maxListeners' ||
                key === 'domain' ||
                key === '_idlePrev' ||
                key === '_idleNext' ||
                key === '_idleStart' ||
                key === '_idleTimeout' ||
                key === '_destroyed' ||
                key === 'constructor') {
                continue;
            }
            
            if (value && typeof value === 'object') {
                try {
                    sanitized[key] = this.sanitizeForJSON(value);
                } catch (e) {
                    sanitized[key] = '[Circular Reference]';
                }
            } else {
                sanitized[key] = value;
            }
        }
        
        return sanitized;
    }

    // Get module status
    getModuleStatus(moduleName = null) {
        if (moduleName) {
            const moduleInfo = this.moduleRegistry.get(moduleName);
            const latestCheck = this.checkHistory[this.checkHistory.length - 1];
            return {
                module: moduleInfo,
                checkResult: latestCheck ? latestCheck.modules[moduleName] : null
            };
        }
        
        const status = {};
        for (const [name, moduleInfo] of this.moduleRegistry) {
            const latestCheck = this.checkHistory[this.checkHistory.length - 1];
            status[name] = {
                module: moduleInfo,
                checkResult: latestCheck ? latestCheck.modules[name] : null
            };
        }
        
        return status;
    }

    // Force immediate check
    async forceCheck() {
        logger.info('Forcing immediate implementation check');
        return await this.performImplementationCheck();
    }

    // Update check interval
    updateCheckInterval(newInterval) {
        this.checkInterval = newInterval;
        if (this.autoUpdateInterval) {
            this.startAutoUpdate();
        }
        logger.info("Check interval updated to " + newInterval + "ms");
    }

    // Get modules information
    async getModules() {
        try {
            const modules = Array.from(this.moduleRegistry.entries()).map(([name, module]) => ({
                name,
                status: this.healthStatus.get(name)?.healthy ? 'healthy' : 'unhealthy',
                lastCheck: this.healthStatus.get(name)?.lastCheck,
                requirements: this.expectedModules[name] || {},
                methods: module.methods || [],
                properties: module.properties || [],
                type: module.type || 'unknown'
            }));

            return {
                total: modules.length,
                healthy: modules.filter(m => m.status === 'healthy').length,
                unhealthy: modules.filter(m => m.status === 'unhealthy').length,
                modules: modules
            };
        } catch (error) {
            logger.error('Failed to get modules:', error);
            throw error;
        }
    }

    // Cleanup and shutdown
    async shutdown() {
        if (this.autoUpdateInterval) {
            clearInterval(this.autoUpdateInterval);
            this.autoUpdateInterval = null;
        }
        
        this.initialized = false;
        this.emit('shutdown', { checker: this.name });
        logger.info('Implementation Checker shutdown complete');
    }

    // Panel Integration Methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: this.description || 'RawrZ Engine',
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }
    
    getAvailableEndpoints() {
        return [
            { method: 'GET', path: '/api/' + this.name + '/status', description: 'Get engine status' },
            { method: 'POST', path: '/api/' + this.name + '/initialize', description: 'Initialize engine' },
            { method: 'POST', path: '/api/' + this.name + '/start', description: 'Start engine' },
            { method: 'POST', path: '/api/' + this.name + '/stop', description: 'Stop engine' }
        ];
    }
    
    getSettings() {
        return {
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            config: this.config || {}
        };
    }
    
    // CLI Integration Methods
    async getCLICommands() {
        return [
            {
                command: this.name + ' status',
                description: 'Get engine status',
                action: async () => {
                    const status = this.getStatus();
                    
                    return status;
                }
            },
            {
                command: this.name + ' start',
                description: 'Start engine',
                action: async () => {
                    const result = await this.start();
                    
                    return result;
                }
            },
            {
                command: this.name + ' stop',
                description: 'Stop engine',
                action: async () => {
                    const result = await this.stop();
                    
                    return result;
                }
            },
            {
                command: this.name + ' config',
                description: 'Get engine configuration',
                action: async () => {
                    const config = this.getConfig();
                    
                    return config;
                }
            }
        ];
    }
    
    getConfig() {
        return {
            name: this.name,
            version: this.version,
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            settings: this.settings || {}
        };
    }

    async getStatus() {
        return {
            name: this.name,
            version: this.version,
            status: this.initialized ? 'active' : 'inactive',
            initialized: this.initialized,
            checks: this.checks.size,
            healthStatus: this.healthStatus.size,
            moduleRegistry: this.moduleRegistry.size,
            checkHistory: this.checkHistory.length,
            lastUpdate: this.lastUpdate
        };
    }

}

// Create and export instance
const implementationChecker = new ImplementationChecker();

module.exports = implementationChecker;

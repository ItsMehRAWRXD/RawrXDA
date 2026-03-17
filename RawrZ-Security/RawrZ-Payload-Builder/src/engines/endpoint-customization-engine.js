// RawrZ Endpoint Customization Engine - Dynamic API Endpoint Generation
// Part of the paneling refactor for endpoint generation and customization
const crypto = require('crypto');
const fs = require('fs').promises;
const path = require('path');
const { logger } = require('../utils/logger');
const { EventEmitter } = require('events');

class EndpointCustomizationEngine extends EventEmitter {
    constructor() {
        super();
        this.name = 'Endpoint Customization Engine';
        this.version = '1.0.0';
        this.description = 'Dynamic API endpoint generation and customization system';
        this.initialized = false;
        
        // Endpoint templates and generators
        this.endpointTemplates = new Map();
        this.customEndpoints = new Map();
        this.endpointGroups = new Map();
        
        // Customization options
        this.customizationOptions = {
            authentication: {
                types: ['none', 'basic', 'bearer', 'api_key', 'custom'],
                default: 'bearer'
            },
            rate_limiting: {
                enabled: true,
                window_ms: 60000,
                max_requests: 100
            },
            cors: {
                enabled: true,
                origins: ['*'],
                methods: ['GET', 'POST', 'PUT', 'DELETE'],
                headers: ['Content-Type', 'Authorization']
            },
            validation: {
                enabled: true,
                strict_mode: false,
                sanitization: true
            },
            logging: {
                enabled: true,
                level: 'info',
                format: 'json'
            },
            caching: {
                enabled: true,
                ttl: 300,
                strategy: 'memory'
            }
        };

        // Generated endpoints registry
        this.generatedEndpoints = new Map();
        this.endpointStatistics = {
            totalGenerated: 0,
            activeEndpoints: 0,
            customizationsApplied: 0,
            lastGeneration: null
        };
    }

    async initialize(config = {}) {
        this.config = {
            autoRegister: true,
            endpointPrefix: '/api/custom',
            maxEndpoints: 1000,
            enableHotReload: true,
            ...config
        };

        await this.loadEndpointTemplates();
        await this.initializeCustomizationSystem();
        await this.setupHotReload();

        this.initialized = true;
        logger.info('Endpoint Customization Engine initialized - ready for dynamic endpoint generation');
    }

    // Load predefined endpoint templates
    async loadEndpointTemplates() {
        const templates = {
            'crud': {
                name: 'CRUD Operations',
                description: 'Create, Read, Update, Delete endpoints',
                endpoints: [
                    { method: 'GET', path: '/{resource}', handler: 'list' },
                    { method: 'GET', path: '/{resource}/{id}', handler: 'get' },
                    { method: 'POST', path: '/{resource}', handler: 'create' },
                    { method: 'PUT', path: '/{resource}/{id}', handler: 'update' },
                    { method: 'DELETE', path: '/{resource}/{id}', handler: 'delete' }
                ]
            },
            'auth': {
                name: 'Authentication',
                description: 'Authentication and authorization endpoints',
                endpoints: [
                    { method: 'POST', path: '/login', handler: 'login' },
                    { method: 'POST', path: '/logout', handler: 'logout' },
                    { method: 'GET', path: '/profile', handler: 'profile' },
                    { method: 'POST', path: '/refresh', handler: 'refresh' },
                    { method: 'POST', path: '/register', handler: 'register' }
                ]
            },
            'file': {
                name: 'File Operations',
                description: 'File upload, download, and management',
                endpoints: [
                    { method: 'POST', path: '/upload', handler: 'upload' },
                    { method: 'GET', path: '/download/{id}', handler: 'download' },
                    { method: 'GET', path: '/files', handler: 'list' },
                    { method: 'DELETE', path: '/files/{id}', handler: 'delete' },
                    { method: 'GET', path: '/files/{id}/info', handler: 'info' }
                ]
            },
            'data': {
                name: 'Data Processing',
                description: 'Data manipulation and processing endpoints',
                endpoints: [
                    { method: 'POST', path: '/process', handler: 'process' },
                    { method: 'GET', path: '/status/{id}', handler: 'status' },
                    { method: 'POST', path: '/transform', handler: 'transform' },
                    { method: 'GET', path: '/export/{format}', handler: 'export' },
                    { method: 'POST', path: '/validate', handler: 'validate' }
                ]
            },
            'webhook': {
                name: 'Webhook Management',
                description: 'Webhook registration and management',
                endpoints: [
                    { method: 'POST', path: '/webhooks', handler: 'create' },
                    { method: 'GET', path: '/webhooks', handler: 'list' },
                    { method: 'PUT', path: '/webhooks/{id}', handler: 'update' },
                    { method: 'DELETE', path: '/webhooks/{id}', handler: 'delete' },
                    { method: 'POST', path: '/webhooks/{id}/test', handler: 'test' }
                ]
            }
        };

        for (const [key, template] of Object.entries(templates)) {
            this.endpointTemplates.set(key, template);
        }

        logger.info(`Loaded ${Object.keys(templates).length} endpoint templates`);
    }

    // Generate custom endpoint from template
    async generateEndpointFromTemplate(templateName, customizations = {}) {
        const template = this.endpointTemplates.get(templateName);
        if (!template) {
            throw new Error(`Template not found: ${templateName}`);
        }

        const endpointId = this.generateEndpointId();
        const customEndpoint = {
            id: endpointId,
            template: templateName,
            name: customizations.name || template.name,
            description: customizations.description || template.description,
            basePath: customizations.basePath || `${this.config.endpointPrefix}/${templateName}`,
            endpoints: [],
            customizations: customizations,
            createdAt: new Date().toISOString(),
            status: 'generated'
        };

        // Generate individual endpoints
        for (const endpointTemplate of template.endpoints) {
            const customEndpointDef = await this.customizeEndpoint(endpointTemplate, customizations);
            customEndpoint.endpoints.push(customEndpointDef);
        }

        // Apply global customizations
        await this.applyGlobalCustomizations(customEndpoint, customizations);

        // Register the endpoint
        this.customEndpoints.set(endpointId, customEndpoint);
        this.endpointStatistics.totalGenerated++;
        this.endpointStatistics.activeEndpoints++;
        this.endpointStatistics.lastGeneration = new Date().toISOString();

        logger.info(`Generated custom endpoint from template: ${templateName} (${endpointId})`);
        return customEndpoint;
    }

    // Customize individual endpoint
    async customizeEndpoint(endpointTemplate, customizations) {
        const endpoint = {
            method: endpointTemplate.method,
            path: endpointTemplate.path,
            handler: endpointTemplate.handler,
            middleware: [],
            validation: {},
            authentication: null,
            rateLimiting: null,
            caching: null,
            logging: null
        };

        // Apply authentication customization
        if (customizations.authentication) {
            endpoint.authentication = this.customizeAuthentication(customizations.authentication);
        }

        // Apply rate limiting
        if (customizations.rate_limiting) {
            endpoint.rateLimiting = this.customizeRateLimiting(customizations.rate_limiting);
        }

        // Apply validation
        if (customizations.validation) {
            endpoint.validation = this.customizeValidation(customizations.validation);
        }

        // Apply caching
        if (customizations.caching) {
            endpoint.caching = this.customizeCaching(customizations.caching);
        }

        // Apply logging
        if (customizations.logging) {
            endpoint.logging = this.customizeLogging(customizations.logging);
        }

        // Add custom middleware
        if (customizations.middleware) {
            endpoint.middleware = customizations.middleware;
        }

        return endpoint;
    }

    // Customize authentication
    customizeAuthentication(authConfig) {
        const auth = {
            type: authConfig.type || this.customizationOptions.authentication.default,
            required: authConfig.required !== false,
            roles: authConfig.roles || [],
            permissions: authConfig.permissions || []
        };

        if (authConfig.custom) {
            auth.custom = authConfig.custom;
        }

        return auth;
    }

    // Customize rate limiting
    customizeRateLimiting(rateLimitConfig) {
        return {
            enabled: rateLimitConfig.enabled !== false,
            window_ms: rateLimitConfig.window_ms || this.customizationOptions.rate_limiting.window_ms,
            max_requests: rateLimitConfig.max_requests || this.customizationOptions.rate_limiting.max_requests,
            skip_successful: rateLimitConfig.skip_successful || false,
            skip_failed: rateLimitConfig.skip_failed || false
        };
    }

    // Customize validation
    customizeValidation(validationConfig) {
        return {
            enabled: validationConfig.enabled !== false,
            strict_mode: validationConfig.strict_mode || false,
            sanitization: validationConfig.sanitization !== false,
            schemas: validationConfig.schemas || {},
            custom_validators: validationConfig.custom_validators || []
        };
    }

    // Customize caching
    customizeCaching(cachingConfig) {
        return {
            enabled: cachingConfig.enabled !== false,
            ttl: cachingConfig.ttl || this.customizationOptions.caching.ttl,
            strategy: cachingConfig.strategy || this.customizationOptions.caching.strategy,
            key_generator: cachingConfig.key_generator || null,
            condition: cachingConfig.condition || null
        };
    }

    // Customize logging
    customizeLogging(loggingConfig) {
        return {
            enabled: loggingConfig.enabled !== false,
            level: loggingConfig.level || this.customizationOptions.logging.level,
            format: loggingConfig.format || this.customizationOptions.logging.format,
            include_request: loggingConfig.include_request || true,
            include_response: loggingConfig.include_response || true,
            custom_fields: loggingConfig.custom_fields || {}
        };
    }

    // Apply global customizations
    async applyGlobalCustomizations(customEndpoint, customizations) {
        // Add global middleware
        if (customizations.globalMiddleware) {
            customEndpoint.globalMiddleware = customizations.globalMiddleware;
        }

        // Add global error handling
        if (customizations.errorHandling) {
            customEndpoint.errorHandling = customizations.errorHandling;
        }

        // Add global response formatting
        if (customizations.responseFormat) {
            customEndpoint.responseFormat = customizations.responseFormat;
        }

        // Add security headers
        if (customizations.securityHeaders) {
            customEndpoint.securityHeaders = customizations.securityHeaders;
        }
    }

    // Generate endpoint ID
    generateEndpointId() {
        return `endpoint_${Date.now()}_${crypto.randomBytes(8).toString('hex')}`;
    }

    // Create endpoint group
    async createEndpointGroup(groupName, endpoints) {
        const group = {
            name: groupName,
            endpoints: endpoints,
            createdAt: new Date().toISOString(),
            status: 'active'
        };

        this.endpointGroups.set(groupName, group);
        logger.info(`Created endpoint group: ${groupName}`);
        return group;
    }

    // Setup hot reload for endpoints
    async setupHotReload() {
        if (!this.config.enableHotReload) return;

        // Watch for configuration changes
        const configWatcher = require('chokidar');
        const configPath = path.join(__dirname, '../../config/endpoints');
        
        try {
            await fs.mkdir(configPath, { recursive: true });
            
            const watcher = configWatcher.watch(configPath, {
                ignored: /(^|[\/\\])\../,
                persistent: true
            });

            watcher.on('change', async (filePath) => {
                await this.reloadEndpointFromFile(filePath);
            });

            logger.info('Hot reload enabled for endpoint configurations');
        } catch (error) {
            logger.warn('Failed to setup hot reload:', error.message);
        }
    }

    // Reload endpoint from file
    async reloadEndpointFromFile(filePath) {
        try {
            const config = JSON.parse(await fs.readFile(filePath, 'utf8'));
            const endpointId = config.id;
            
            if (this.customEndpoints.has(endpointId)) {
                this.customEndpoints.set(endpointId, config);
                logger.info(`Reloaded endpoint from file: ${endpointId}`);
                this.emit('endpointReloaded', endpointId);
            }
        } catch (error) {
            logger.error(`Failed to reload endpoint from ${filePath}:`, error.message);
        }
    }

    // Get endpoint by ID
    getEndpoint(endpointId) {
        return this.customEndpoints.get(endpointId);
    }

    // Get all endpoints
    getAllEndpoints() {
        return Array.from(this.customEndpoints.values());
    }

    // Get endpoints by template
    getEndpointsByTemplate(templateName) {
        return this.getAllEndpoints().filter(ep => ep.template === templateName);
    }

    // Delete endpoint
    async deleteEndpoint(endpointId) {
        if (this.customEndpoints.has(endpointId)) {
            this.customEndpoints.delete(endpointId);
            this.endpointStatistics.activeEndpoints--;
            logger.info(`Deleted endpoint: ${endpointId}`);
            return true;
        }
        return false;
    }

    // Panel integration methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: this.description,
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }

    getAvailableEndpoints() {
        return [
            { method: 'POST', path: '/api/endpoint-customization/generate', description: 'Generate custom endpoint' },
            { method: 'GET', path: '/api/endpoint-customization/endpoints', description: 'Get all endpoints' },
            { method: 'GET', path: '/api/endpoint-customization/templates', description: 'Get available templates' },
            { method: 'DELETE', path: '/api/endpoint-customization/endpoint/{id}', description: 'Delete endpoint' }
        ];
    }

    getSettings() {
        return {
            templates: Array.from(this.endpointTemplates.keys()),
            customizationOptions: this.customizationOptions,
            maxEndpoints: this.config.maxEndpoints,
            endpointPrefix: this.config.endpointPrefix,
            hotReload: this.config.enableHotReload
        };
    }

    getStatus() {
        return {
            initialized: this.initialized,
            totalEndpoints: this.endpointStatistics.totalGenerated,
            activeEndpoints: this.endpointStatistics.activeEndpoints,
            endpointGroups: this.endpointGroups.size,
            statistics: this.endpointStatistics,
            uptime: process.uptime(),
            timestamp: new Date().toISOString()
        };
    }

    // Get endpoint statistics
    getEndpointStatistics() {
        return {
            ...this.endpointStatistics,
            templates: this.endpointTemplates.size,
            endpointGroups: this.endpointGroups.size,
            customizationOptions: Object.keys(this.customizationOptions)
        };
    }
}

module.exports = EndpointCustomizationEngine;

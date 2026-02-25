const AgenticBrowser = require('./agent');
const ProductionBrowserAgent = require('./production-browser-agent');
const TextProcessor = require('./text-processor');
const BulkTextHandler = require('./bulk-handler');
const ModelManager = require('./model-manager');
const { subscriptionConfigs, organizationSettings, browserSettings } = require('./config');
const chalk = require('chalk');

class EnhancedOrchestrator {
    constructor() {
        this.agents = new Map([
            ['browser',     { name: 'Browser Agent',    status: 'idle', ttl: 120, exec: this.browserTask.bind(this)     }],
            ['production_browser', { name: 'Production Browser', status: 'idle', ttl: 90, exec: this.productionBrowserTask.bind(this) }],
            ['text',        { name: 'Text Processor',   status: 'idle', ttl: 60,  exec: this.textTask.bind(this)      }],
            ['bulk',        { name: 'Bulk Handler',     status: 'idle', ttl: 60,  exec: this.bulkTask.bind(this)       }],
            ['download',    { name: 'Download Manager', status: 'idle', ttl: 60,  exec: this.downloadTask.bind(this)  }],
            ['organize',    { name: 'File Organizer',   status: 'idle', ttl: 60,  exec: this.organizeTask.bind(this)    }],
            ['security',     { name: 'Security Scan',  status: 'idle', ttl: 60,  exec: this.securityTask.bind(this)    }],
            ['performance', { name: 'Performance',      status: 'idle', ttl: 60,  exec: this.performanceTask.bind(this) }],
            ['training',   { name: 'Model Training',   status: 'idle', ttl: 300, exec: this.trainingTask.bind(this)    }],
            ['inference',  { name: 'Model Inference',   status: 'idle', ttl: 60,  exec: this.inferenceTask.bind(this)  }],
            ['orchestrator',{ name: 'Orchestrator',     status: 'idle', ttl: 60,  exec: this.orchestrateTask.bind(this) }]
        ]);
        
        this.browser = null;
        this.productionBrowser = null;
        this.textProcessor = new TextProcessor();
        this.bulkHandler = new BulkTextHandler();
        this.modelManager = new ModelManager();
    }

    async initialize() {
        console.log(chalk.blue.bold('=== ENHANCED BIGDADDYG ORCHESTRATOR ==='));
        console.log(chalk.yellow('Multi-agent system for web automation and bulk processing\n'));
        
        await this.textProcessor.initialize();
        await this.bulkHandler.initialize();
        await this.modelManager.initialize();
        
        this.browser = new AgenticBrowser({
            downloadPath: organizationSettings.downloadPath,
            organizeByType: organizationSettings.organizeByType,
            organizeByDate: organizationSettings.organizeByDate
        });
    }

    async browserTask(task) {
        try {
            this.agents.get('browser').status = 'active';
            
            if (!this.browser) {
                await this.browser.initialize();
            }
            
            const { action, url, selector, options } = task;
            
            switch (action) {
                case 'navigate':
                    return await this.browser.navigateTo(url, options);
                case 'click':
                    return await this.browser.performAction('click', selector, options);
                case 'type':
                    return await this.browser.performAction('type', selector, options);
                case 'download':
                    return await this.browser.performAction('download', selector, options);
                case 'scrape':
                    return await this.browser.page.evaluate(() => document.body.innerText);
                default:
                    throw new Error(`Unknown browser action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Browser task failed: ${error.message}`);
        } finally {
            this.agents.get('browser').status = 'idle';
        }
    }

    async productionBrowserTask(task) {
        try {
            this.agents.get('production_browser').status = 'active';
            
            if (!this.productionBrowser) {
                this.productionBrowser = new ProductionBrowserAgent({
                    downloadPath: organizationSettings.downloadPath,
                    ttl: 90
                });
                await this.productionBrowser.launch();
            }
            
            // Check if agent is expired
            if (this.productionBrowser.isExpired()) {
                await this.productionBrowser.close();
                this.productionBrowser = new ProductionBrowserAgent({
                    downloadPath: organizationSettings.downloadPath,
                    ttl: 90
                });
                await this.productionBrowser.launch();
            }
            
            const result = await this.productionBrowser.orchestrate(task);
            return result;
            
        } catch (error) {
            throw new Error(`Production browser task failed: ${error.message}`);
        } finally {
            this.agents.get('production_browser').status = 'idle';
        }
    }

    async textTask(task) {
        try {
            this.agents.get('text').status = 'active';
            
            const { action, content, options } = task;
            
            switch (action) {
                case 'process':
                    return await this.textProcessor.processBulkText(content, options);
                case 'organize':
                    return await this.textProcessor.organizeByContent(content, options);
                case 'categorize':
                    return this.textProcessor.categorizeContent(content);
                case 'chunk':
                    return this.textProcessor.createChunks(content.split('\n'), options.chunkSize || 10000);
                default:
                    throw new Error(`Unknown text action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Text task failed: ${error.message}`);
        } finally {
            this.agents.get('text').status = 'idle';
        }
    }

    async bulkTask(task) {
        try {
            this.agents.get('bulk').status = 'active';
            
            const { action, data, options } = task;
            
            switch (action) {
                case 'process_paste':
                    return await this.bulkHandler.handleDirectPaste();
                case 'process_file':
                    return await this.bulkHandler.handleFileLoad();
                case 'process_web':
                    return await this.bulkHandler.handleWebScraping();
                case 'process_clipboard':
                    return await this.bulkHandler.handleClipboard();
                default:
                    throw new Error(`Unknown bulk action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Bulk task failed: ${error.message}`);
        } finally {
            this.agents.get('bulk').status = 'idle';
        }
    }

    async downloadTask(task) {
        try {
            this.agents.get('download').status = 'active';
            
            const { action, url, options } = task;
            
            if (!this.browser) {
                await this.browser.initialize();
            }
            
            switch (action) {
                case 'single':
                    return await this.browser.handleDownload(url, options);
                case 'batch':
                    const results = [];
                    for (const downloadUrl of url) {
                        results.push(await this.browser.handleDownload(downloadUrl, options));
                    }
                    return results;
                case 'subscription':
                    return await this.browser.executeWebSubscription(options);
                default:
                    throw new Error(`Unknown download action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Download task failed: ${error.message}`);
        } finally {
            this.agents.get('download').status = 'idle';
        }
    }

    async organizeTask(task) {
        try {
            this.agents.get('organize').status = 'active';
            
            const { action, files, options } = task;
            
            switch (action) {
                case 'by_type':
                    return await this.textProcessor.organizeByContent(files, options);
                case 'by_date':
                    return await this.browser.organizeDownloads();
                case 'create_index':
                    return await this.textProcessor.createIndexFile(files, options);
                case 'create_summary':
                    return await this.textProcessor.createSummary(files);
                default:
                    throw new Error(`Unknown organize action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Organize task failed: ${error.message}`);
        } finally {
            this.agents.get('organize').status = 'idle';
        }
    }

    async securityTask(task) {
        try {
            this.agents.get('security').status = 'active';
            
            const { action, content, options } = task;
            
            switch (action) {
                case 'scan_text':
                    return this.scanForSecrets(content);
                case 'sanitize':
                    return this.sanitizeContent(content);
                case 'validate_urls':
                    return this.validateUrls(content);
                default:
                    throw new Error(`Unknown security action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Security task failed: ${error.message}`);
        } finally {
            this.agents.get('security').status = 'idle';
        }
    }

    async performanceTask(task) {
        try {
            this.agents.get('performance').status = 'active';
            
            const { action, data, options } = task;
            
            switch (action) {
                case 'measure':
                    return this.measurePerformance(data);
                case 'optimize':
                    return this.optimizeContent(data);
                case 'compress':
                    return this.compressData(data);
                default:
                    throw new Error(`Unknown performance action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Performance task failed: ${error.message}`);
        } finally {
            this.agents.get('performance').status = 'idle';
        }
    }

    async trainingTask(task) {
        try {
            this.agents.get('training').status = 'active';
            
            const { action, scriptName, config } = task;
            
            switch (action) {
                case 'create_script':
                    return await this.modelManager.createCustomTrainingScript(scriptName, config);
                case 'run_training':
                    return await this.modelManager.runCustomTraining(scriptName);
                case 'create_template':
                    return await this.modelManager.createModelTemplate(scriptName, config);
                case 'list_models':
                    return await this.modelManager.listAvailableModels();
                default:
                    throw new Error(`Unknown training action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Training task failed: ${error.message}`);
        } finally {
            this.agents.get('training').status = 'idle';
        }
    }

    async inferenceTask(task) {
        try {
            this.agents.get('inference').status = 'active';
            
            const { action, modelName, input } = task;
            
            switch (action) {
                case 'load_model':
                    return await this.modelManager.loadOfflineModel(modelName);
                case 'predict':
                    return await this.modelManager.predictWithModel(modelName, input);
                case 'download_model':
                    return await this.modelManager.downloadModel(modelName, task.targetPath);
                default:
                    throw new Error(`Unknown inference action: ${action}`);
            }
        } catch (error) {
            throw new Error(`Inference task failed: ${error.message}`);
        } finally {
            this.agents.get('inference').status = 'idle';
        }
    }

    async orchestrateTask(task) {
        try {
            this.agents.get('orchestrator').status = 'active';
            
            const pipeline = ['browser', 'text', 'bulk', 'download', 'organize', 'security', 'performance', 'training', 'inference'];
            let payload = task;
            const report = [];
            
            for (const agentId of pipeline) {
                const agent = this.agents.get(agentId);
                if (agent && agent.exec) {
                    try {
                        payload = await agent.exec(payload);
                        report.push(`${agent.name}: OK`);
                    } catch (error) {
                        report.push(`${agent.name}: ERR ${error.message}`);
                    }
                }
            }
            
            return { result: payload, report };
        } catch (error) {
            throw new Error(`Orchestration failed: ${error.message}`);
        } finally {
            this.agents.get('orchestrator').status = 'idle';
        }
    }

    // Utility methods
    scanForSecrets(content) {
        const secrets = [];
        const patterns = [
            { name: 'API Key', pattern: /[a-zA-Z0-9]{32,}/g },
            { name: 'Password', pattern: /password\s*[:=]\s*['"][^'"]+['"]/gi },
            { name: 'Token', pattern: /token\s*[:=]\s*['"][^'"]+['"]/gi },
            { name: 'Secret', pattern: /secret\s*[:=]\s*['"][^'"]+['"]/gi }
        ];
        
        patterns.forEach(({ name, pattern }) => {
            const matches = content.match(pattern);
            if (matches) {
                secrets.push({ type: name, count: matches.length, matches: matches.slice(0, 5) });
            }
        });
        
        return { secrets, riskLevel: secrets.length > 0 ? 'HIGH' : 'LOW' };
    }

    sanitizeContent(content) {
        return content
            .replace(/password\s*[:=]\s*['"][^'"]+['"]/gi, 'password="****"')
            .replace(/token\s*[:=]\s*['"][^'"]+['"]/gi, 'token="****"')
            .replace(/secret\s*[:=]\s*['"][^'"]+['"]/gi, 'secret="****"');
    }

    validateUrls(content) {
        const urlPattern = /https?:\/\/[^\s]+/g;
        const urls = content.match(urlPattern) || [];
        
        return urls.map(url => ({
            url,
            valid: this.isValidUrl(url),
            domain: this.extractDomain(url)
        }));
    }

    isValidUrl(string) {
        try {
            new URL(string);
            return true;
        } catch (_) {
            return false;
        }
    }

    extractDomain(url) {
        try {
            return new URL(url).hostname;
        } catch (_) {
            return null;
        }
    }

    measurePerformance(data) {
        const start = Date.now();
        const size = JSON.stringify(data).length;
        const end = Date.now();
        
        return {
            processingTime: end - start,
            dataSize: size,
            throughput: size / (end - start),
            timestamp: new Date().toISOString()
        };
    }

    optimizeContent(data) {
        if (typeof data === 'string') {
            return data
                .replace(/\s+/g, ' ')
                .replace(/\n\s*\n/g, '\n')
                .trim();
        }
        return data;
    }

    compressData(data) {
        const original = JSON.stringify(data);
        const compressed = original
            .replace(/\s+/g, ' ')
            .replace(/"/g, "'");
        
        return {
            original: original.length,
            compressed: compressed.length,
            ratio: compressed.length / original.length,
            savings: original.length - compressed.length
        };
    }

    // Live status board
    render() {
        console.clear();
        console.log(chalk.blue.bold('🧠 Enhanced BigDaddyG Orchestrator – Agent Status'));
        console.table([...this.agents.entries()].map(([k,v]) => ({ 
            id: k, 
            name: v.name, 
            status: v.status, 
            ttl: v.ttl 
        })));
    }

    // Auto-refresh status
    startStatusBoard() {
        const interval = setInterval(() => this.render(), 5000);
        this.render();
        return () => clearInterval(interval);
    }

    async cleanup() {
        if (this.browser) {
            await this.browser.close();
        }
        console.log(chalk.blue('[ORCHESTRATOR] Cleanup completed'));
    }
}

module.exports = EnhancedOrchestrator;

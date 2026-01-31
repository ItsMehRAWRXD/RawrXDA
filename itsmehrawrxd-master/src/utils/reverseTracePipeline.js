// Reverse Trace Pipeline - Comprehensive data flow monitoring and malformity tracing
const { logger } = require('./logger');
const { reverseTracer } = require('./reverseTracer');
const { dataIntegrityValidator } = require('./dataIntegrity');

class ReverseTracePipeline {
    constructor() {
        this.pipeline = new Map();
        this.dataFlow = [];
        this.corruptionChain = [];
        this.operationGraph = new Map();
        this.malformitySources = new Map();
        this.traceDepth = 0;
        this.maxTraceDepth = 10;
        
        this.initializePipeline();
    }

    initializePipeline() {
        logger.info('[SEARCH] Reverse Trace Pipeline initialized');
        
        // Monitor all critical data flows
        this.monitorDataFlows();
        
        // Set up automatic tracing
        this.setupAutomaticTracing();
        
        // Start periodic analysis
        this.startPeriodicAnalysis();
    }

    // Monitor all critical data flows
    monitorDataFlows() {
        // Monitor encryption pipeline
        this.addPipelineStage('ENCRYPTION_PIPELINE', {
            stages: ['input_validation', 'pre_encryption', 'encryption', 'post_encryption', 'output_validation'],
            criticalPoints: ['pre_encryption', 'post_encryption'],
            malformityChecks: ['ROE_FONT_CORRUPTION', 'ENCODING_CORRUPTION', 'MENU_CORRUPTION']
        });

        // Monitor UI rendering pipeline
        this.addPipelineStage('UI_RENDERING_PIPELINE', {
            stages: ['data_preparation', 'template_rendering', 'font_processing', 'menu_generation', 'output_rendering'],
            criticalPoints: ['font_processing', 'menu_generation'],
            malformityChecks: ['ROE_FONT_CORRUPTION', 'MENU_CORRUPTION', 'ENCODING_CORRUPTION']
        });

        // Monitor API response pipeline
        this.addPipelineStage('API_RESPONSE_PIPELINE', {
            stages: ['data_processing', 'json_serialization', 'response_formatting', 'cors_headers', 'output_streaming'],
            criticalPoints: ['json_serialization', 'response_formatting'],
            malformityChecks: ['JSON_MALFORMED', 'ENCODING_CORRUPTION']
        });

        // Monitor file processing pipeline
        this.addPipelineStage('FILE_PROCESSING_PIPELINE', {
            stages: ['file_read', 'content_validation', 'processing', 'output_validation', 'file_write'],
            criticalPoints: ['content_validation', 'output_validation'],
            malformityChecks: ['ENCODING_CORRUPTION', 'NULL_BYTES', 'CONTROL_CHARACTERS']
        });
    }

    // Add pipeline stage
    addPipelineStage(name, config) {
        this.pipeline.set(name, {
            ...config,
            operations: [],
            malformities: [],
            lastActivity: null,
            traceHistory: []
        });
    }

    // Track operation through pipeline
    trackOperation(pipelineName, stage, operationId, data, metadata = {}) {
        const pipeline = this.pipeline.get(pipelineName);
        if (!pipeline) {
            logger.warn(`Unknown pipeline: ${pipelineName}`);
            return;
        }

        // Handle encryption result objects properly
        let dataToProcess = data;
        if (data && typeof data === 'object' && data.data) {
            // If it's an encryption result object, use the encrypted data field
            dataToProcess = data.data;
        }

        const operation = {
            id: operationId,
            stage,
            timestamp: Date.now(),
            data: dataToProcess ? dataToProcess.toString().substring(0, 1000) : null,
            metadata,
            checksums: dataToProcess ? dataIntegrityValidator.calculateChecksums(dataToProcess) : null,
            malformities: [] // DISABLED: Malformity detection disabled to prevent memory leaks
        };

        // Add to pipeline operations
        pipeline.operations.push(operation);
        
        // Keep only last 100 operations per pipeline
        if (pipeline.operations.length > 100) {
            pipeline.operations.shift();
        }

        // Track in data flow
        this.dataFlow.push({
            pipeline: pipelineName,
            stage,
            operationId,
            timestamp: operation.timestamp,
            dataLength: dataToProcess ? dataToProcess.length : 0,
            malformities: operation.malformities.length
        });

        // Check for malformities
        if (operation.malformities.length > 0) {
            this.handleMalformity(pipelineName, stage, operation);
        }

        // Update operation graph
        this.updateOperationGraph(pipelineName, stage, operation);

        // Deep trace if critical point
        if (pipeline.criticalPoints.includes(stage)) {
            this.deepTrace(pipelineName, stage, operation);
        }

        pipeline.lastActivity = Date.now();
    }

    // Handle malformity detection
    handleMalformity(pipelineName, stage, operation) {
        const malformity = {
            pipeline: pipelineName,
            stage,
            operationId: operation.id,
            timestamp: operation.timestamp,
            malformities: operation.malformities,
            data: operation.data,
            checksums: operation.checksums
        };

        // Add to corruption chain
        this.corruptionChain.push(malformity);

        // Track malformity source
        const sourceKey = `${pipelineName}_${stage}`;
        if (!this.malformitySources.has(sourceKey)) {
            this.malformitySources.set(sourceKey, []);
        }
        this.malformitySources.get(sourceKey).push(malformity);

        // Log critical malformities
        operation.malformities.forEach(m => {
            if (m.severity === 'CRITICAL' || m.severity === 'HIGH') {
                logger.error(`[ALERT] CRITICAL MALFORMITY: ${m.type} in ${pipelineName}:${stage}`);
                logger.error(`[ALERT] Description: ${m.description}`);
                logger.error(`[ALERT] Operation ID: ${operation.id}`);
                
                // Record in reverse tracer
                reverseTracer.recordCorruption(
                    `${pipelineName}:${stage}`,
                    m.description,
                    null,
                    m.type
                );
            }
        });

        // Trigger reverse trace
        this.reverseTrace(pipelineName, stage, operation);
    }

    // Reverse trace to find source
    reverseTrace(pipelineName, stage, operation) {
        logger.info(`[SEARCH] REVERSE TRACING: ${pipelineName}:${stage} - Operation ${operation.id}`);
        
        const pipeline = this.pipeline.get(pipelineName);
        if (!pipeline) return;

        // Find previous operations in same pipeline
        const previousOps = pipeline.operations
            .filter(op => op.timestamp < operation.timestamp)
            .slice(-5); // Last 5 operations

        // Check for malformity propagation
        for (const prevOp of previousOps) {
            if (this.hasMalformityPropagation(prevOp, operation)) {
                logger.error(`[TARGET] MALFORMITY PROPAGATION DETECTED: ${prevOp.id} -> ${operation.id}`);
                this.traceMalformityPropagation(prevOp, operation);
            }
        }

        // Check cross-pipeline contamination
        this.checkCrossPipelineContamination(pipelineName, operation);

        // Analyze data transformation chain
        this.analyzeDataTransformationChain(pipelineName, operation);
    }

    // Check for malformity propagation
    hasMalformityPropagation(sourceOp, targetOp) {
        if (!sourceOp.malformities.length || !targetOp.malformities.length) {
            return false;
        }

        // Check if malformity types match
        const sourceTypes = sourceOp.malformities.map(m => m.type);
        const targetTypes = targetOp.malformities.map(m => m.type);
        
        return sourceTypes.some(type => targetTypes.includes(type));
    }

    // Trace malformity propagation
    traceMalformityPropagation(sourceOp, targetOp) {
        const propagation = {
            source: {
                operationId: sourceOp.id,
                timestamp: sourceOp.timestamp,
                malformities: sourceOp.malformities
            },
            target: {
                operationId: targetOp.id,
                timestamp: targetOp.timestamp,
                malformities: targetOp.malformities
            },
            propagationTime: targetOp.timestamp - sourceOp.timestamp,
            timestamp: Date.now()
        };

        logger.error(`[TARGET] MALFORMITY PROPAGATION CHAIN:`);
        logger.error(`  Source: ${sourceOp.id} (${sourceOp.malformities.map(m => m.type).join(', ')})`);
        logger.error(`  Target: ${targetOp.id} (${targetOp.malformities.map(m => m.type).join(', ')})`);
        logger.error(`  Propagation Time: ${propagation.propagationTime}ms`);

        // Record in reverse tracer
        reverseTracer.recordCorruption(
            'malformityPropagation',
            `Malformity propagated from ${sourceOp.id} to ${targetOp.id}`,
            null,
            'MALFORMITY_PROPAGATION'
        );
    }

    // Check cross-pipeline contamination
    checkCrossPipelineContamination(pipelineName, operation) {
        for (const [otherPipelineName, otherPipeline] of this.pipeline) {
            if (otherPipelineName === pipelineName) continue;

            const recentOps = otherPipeline.operations
                .filter(op => Math.abs(op.timestamp - operation.timestamp) < 5000) // Within 5 seconds
                .slice(-3);

            for (const otherOp of recentOps) {
                if (this.hasDataContamination(otherOp, operation)) {
                    logger.error(`[TARGET] CROSS-PIPELINE CONTAMINATION: ${otherPipelineName} -> ${pipelineName}`);
                    this.traceCrossPipelineContamination(otherOp, operation, otherPipelineName, pipelineName);
                }
            }
        }
    }

    // Check for data contamination
    hasDataContamination(sourceOp, targetOp) {
        if (!sourceOp.checksums || !targetOp.checksums) return false;

        // Check for similar checksums (data reuse)
        const sourceCRC = sourceOp.checksums.crc32;
        const targetCRC = targetOp.checksums.crc32;

        return sourceCRC === targetCRC;
    }

    // Trace cross-pipeline contamination
    traceCrossPipelineContamination(sourceOp, targetOp, sourcePipeline, targetPipeline) {
        logger.error(`[TARGET] CROSS-PIPELINE CONTAMINATION DETECTED:`);
        logger.error(`  Source Pipeline: ${sourcePipeline} (${sourceOp.id})`);
        logger.error(`  Target Pipeline: ${targetPipeline} (${targetOp.id})`);
        logger.error(`  Data CRC Match: ${sourceOp.checksums.crc32}`);

        reverseTracer.recordCorruption(
            'crossPipelineContamination',
            `Data contamination from ${sourcePipeline} to ${targetPipeline}`,
            null,
            'CROSS_PIPELINE_CONTAMINATION'
        );
    }

    // Analyze data transformation chain
    analyzeDataTransformationChain(pipelineName, operation) {
        const pipeline = this.pipeline.get(pipelineName);
        if (!pipeline) return;

        // Find transformation chain
        const chain = pipeline.operations
            .filter(op => op.timestamp <= operation.timestamp)
            .slice(-10); // Last 10 operations

        // Analyze transformations
        for (let i = 1; i < chain.length; i++) {
            const prev = chain[i - 1];
            const curr = chain[i];

            if (this.hasTransformationIssue(prev, curr)) {
                logger.error(`[TARGET] TRANSFORMATION ISSUE: ${prev.id} -> ${curr.id}`);
                this.analyzeTransformationIssue(prev, curr);
            }
        }
    }

    // Check for transformation issues
    hasTransformationIssue(sourceOp, targetOp) {
        if (!sourceOp.checksums || !targetOp.checksums) return false;

        // Check for unexpected size changes
        const sizeRatio = targetOp.checksums.size / sourceOp.checksums.size;
        if (sizeRatio < 0.1 || sizeRatio > 10) {
            return true;
        }

        // Check for new malformities introduced
        if (sourceOp.malformities.length === 0 && targetOp.malformities.length > 0) {
            return true;
        }

        return false;
    }

    // Analyze transformation issue
    analyzeTransformationIssue(sourceOp, targetOp) {
        const issue = {
            source: sourceOp.id,
            target: targetOp.id,
            sizeChange: targetOp.checksums.size / sourceOp.checksums.size,
            newMalformities: targetOp.malformities.length - sourceOp.malformities.length,
            timestamp: Date.now()
        };

        logger.error(`[TARGET] TRANSFORMATION ISSUE ANALYSIS:`);
        logger.error(`  Size Change Ratio: ${issue.sizeChange.toFixed(2)}`);
        logger.error(`  New Malformities: ${issue.newMalformities}`);

        reverseTracer.recordCorruption(
            'transformationIssue',
            `Transformation issue: size ratio ${issue.sizeChange.toFixed(2)}, new malformities: ${issue.newMalformities}`,
            null,
            'TRANSFORMATION_ISSUE'
        );
    }

    // Update operation graph
    updateOperationGraph(pipelineName, stage, operation) {
        const nodeId = `${pipelineName}:${stage}:${operation.id}`;
        
        if (!this.operationGraph.has(nodeId)) {
            this.operationGraph.set(nodeId, {
                pipeline: pipelineName,
                stage,
                operationId: operation.id,
                timestamp: operation.timestamp,
                connections: [],
                malformities: operation.malformities
            });
        }

        // Add connections to recent operations
        const recentNodes = Array.from(this.operationGraph.entries())
            .filter(([id, node]) => 
                Math.abs(node.timestamp - operation.timestamp) < 10000 && // Within 10 seconds
                id !== nodeId
            )
            .slice(-5);

        for (const [otherId, otherNode] of recentNodes) {
            this.operationGraph.get(nodeId).connections.push(otherId);
        }
    }

    // Deep trace for critical points
    deepTrace(pipelineName, stage, operation) {
        logger.info(`[SEARCH] DEEP TRACE: ${pipelineName}:${stage} - Operation ${operation.id}`);
        
        // Analyze data in detail
        if (operation.data) {
            this.analyzeDataInDetail(operation.data, operation.id);
        }

        // Check for patterns
        this.checkForPatterns(pipelineName, operation);

        // Validate against known good states
        this.validateAgainstKnownGoodStates(pipelineName, stage, operation);
    }

    // Analyze data in detail
    analyzeDataInDetail(data, operationId) {
        const analysis = {
            operationId,
            length: data.length,
            encoding: this.detectEncoding(data),
            entropy: this.calculateEntropy(data),
            patterns: this.detectPatterns(data),
            suspicious: []
        };

        // Check for suspicious patterns
        if (analysis.entropy < 2.0) {
            analysis.suspicious.push('Low entropy - possible encoding issue');
        }

        if (data.includes('ROE')) {
            analysis.suspicious.push('ROE pattern detected');
        }

        if (data.includes('font')) {
            analysis.suspicious.push('Font-related content detected');
        }

        if (analysis.suspicious.length > 0) {
            logger.warn(`[SEARCH] DATA ANALYSIS SUSPICIOUS: ${operationId}`);
            analysis.suspicious.forEach(s => logger.warn(`  - ${s}`));
        }
    }

    // Detect encoding
    detectEncoding(data) {
        try {
            const buffer = Buffer.from(data, 'utf8');
            const roundTrip = buffer.toString('utf8');
            return roundTrip === data ? 'utf8' : 'unknown';
        } catch (error) {
            return 'error';
        }
    }

    // Calculate entropy
    calculateEntropy(data) {
        const buffer = Buffer.from(data, 'utf8');
        const frequencies = {};
        
        for (let i = 0; i < buffer.length; i++) {
            const byte = buffer[i];
            frequencies[byte] = (frequencies[byte] || 0) + 1;
        }

        let entropy = 0;
        const length = buffer.length;
        
        for (const freq of Object.values(frequencies)) {
            const probability = freq / length;
            entropy -= probability * Math.log2(probability);
        }

        return entropy;
    }

    // Detect patterns
    detectPatterns(data) {
        const patterns = [];
        
        if (/ROE.*font|font.*ROE/i.test(data)) {
            patterns.push('ROE_FONT_PATTERN');
        }
        
        if (/menu.*corrupt|corrupt.*menu/i.test(data)) {
            patterns.push('MENU_CORRUPTION_PATTERN');
        }
        
        if (/[^\x00-\x7F]/.test(data)) {
            patterns.push('NON_ASCII_CHARACTERS');
        }

        return patterns;
    }

    // Check for patterns
    checkForPatterns(pipelineName, operation) {
        const patterns = this.detectPatterns(operation.data || '');
        
        if (patterns.length > 0) {
            logger.warn(`[SEARCH] PATTERNS DETECTED in ${pipelineName}: ${patterns.join(', ')}`);
            
            patterns.forEach(pattern => {
                reverseTracer.recordCorruption(
                    'patternDetection',
                    `Pattern detected: ${pattern}`,
                    null,
                    pattern
                );
            });
        }
    }

    // Validate against known good states
    validateAgainstKnownGoodStates(pipelineName, stage, operation) {
        // This would compare against known good states
        // For now, we'll just log the validation attempt
        logger.info(`[SEARCH] VALIDATION: ${pipelineName}:${stage} against known good states`);
    }

    // Setup automatic tracing
    setupAutomaticTracing() {
        // Monitor console output for malformities
        const originalLog = console.log;
        const originalError = console.error;

        console.log = (...args) => {
            this.monitorConsoleOutput('log', args);
            return originalLog.apply(console, args);
        };

        console.error = (...args) => {
            this.monitorConsoleOutput('error', args);
            return originalError.apply(console, args);
        };
    }

    // Monitor console output
    monitorConsoleOutput(level, args) {
        const output = args.join(' ');
        
        // Check for malformity indicators
        if (/ROE.*font|font.*ROE|menu.*corrupt|corrupt.*menu/i.test(output)) {
            this.trackOperation('CONSOLE_OUTPUT_PIPELINE', 'console_output', `console_${Date.now()}`, output, { level });
        }
    }

    // Start periodic analysis
    startPeriodicAnalysis() {
        setInterval(() => {
            this.analyzePipelineHealth();
            this.cleanupOldData();
        }, 30000); // Every 30 seconds
    }

    // Analyze pipeline health
    analyzePipelineHealth() {
        const health = {
            timestamp: Date.now(),
            pipelines: {},
            overallHealth: 'healthy',
            issues: []
        };

        for (const [name, pipeline] of this.pipeline) {
            const pipelineHealth = {
                operations: pipeline.operations.length,
                malformities: pipeline.malformities.length,
                lastActivity: pipeline.lastActivity,
                health: 'healthy'
            };

            // Check for issues
            if (pipeline.malformities.length > 10) {
                pipelineHealth.health = 'critical';
                health.overallHealth = 'critical';
                health.issues.push(`High malformity count in ${name}: ${pipeline.malformities.length}`);
            } else if (pipeline.malformities.length > 5) {
                pipelineHealth.health = 'warning';
                if (health.overallHealth === 'healthy') {
                    health.overallHealth = 'warning';
                }
                health.issues.push(`Elevated malformity count in ${name}: ${pipeline.malformities.length}`);
            }

            health.pipelines[name] = pipelineHealth;
        }

        if (health.issues.length > 0) {
            logger.warn(`[SEARCH] PIPELINE HEALTH ANALYSIS: ${health.overallHealth}`);
            health.issues.forEach(issue => logger.warn(`  - ${issue}`));
        }
    }

    // Cleanup old data
    cleanupOldData() {
        const cutoff = Date.now() - 300000; // 5 minutes

        // Cleanup data flow
        this.dataFlow = this.dataFlow.filter(flow => flow.timestamp > cutoff);

        // Cleanup corruption chain
        this.corruptionChain = this.corruptionChain.filter(corruption => corruption.timestamp > cutoff);

        // Cleanup operation graph
        for (const [nodeId, node] of this.operationGraph) {
            if (node.timestamp < cutoff) {
                this.operationGraph.delete(nodeId);
            }
        }
    }

    // Get comprehensive trace report
    getTraceReport() {
        return {
            timestamp: Date.now(),
            pipelines: Object.fromEntries(this.pipeline),
            dataFlow: this.dataFlow.slice(-50), // Last 50 flows
            corruptionChain: this.corruptionChain.slice(-20), // Last 20 corruptions
            operationGraph: Object.fromEntries(this.operationGraph),
            malformitySources: Object.fromEntries(this.malformitySources),
            health: this.getPipelineHealth()
        };
    }

    // Get pipeline health
    getPipelineHealth() {
        const health = {
            overall: 'healthy',
            pipelines: {},
            issues: []
        };

        for (const [name, pipeline] of this.pipeline) {
            const pipelineHealth = {
                operations: pipeline.operations.length,
                malformities: pipeline.malformities.length,
                lastActivity: pipeline.lastActivity,
                health: 'healthy'
            };

            if (pipeline.malformities.length > 10) {
                pipelineHealth.health = 'critical';
                health.overall = 'critical';
            } else if (pipeline.malformities.length > 5) {
                pipelineHealth.health = 'warning';
                if (health.overall === 'healthy') {
                    health.overall = 'warning';
                }
            }

            health.pipelines[name] = pipelineHealth;
        }

        return health;
    }
}

// Create singleton instance
const reverseTracePipeline = new ReverseTracePipeline();

module.exports = {
    ReverseTracePipeline,
    reverseTracePipeline
};

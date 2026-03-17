#!/usr/bin/env node
/**
 * RawrZ Separated Encryption Workflow Demo
 * Demonstrates the burn-and-reuse strategy with separated encryption
 */

const EncryptionWorkflowController = require('./engines/encryption-workflow-controller');
const path = require('path');
const fs = require('fs').promises;

class SeparatedEncryptionDemo {
    constructor() {
        this.controller = new EncryptionWorkflowController();
    }

    async runDemo() {
        console.log('================================================================');
        console.log('  RawrZ Separated Encryption Workflow - DEMO');
        console.log('  Burn-and-Reuse Strategy for Maximum Efficiency');
        console.log('================================================================\n');

        try {
            // Create a sample file for demonstration
            await this.createSampleFile();

            // Demo 1: Complete workflow (your strategy)
            console.log('DEMO 1: Complete Burn-and-Reuse Workflow');
            console.log('=========================================');
            const workflowResult = await this.controller.executeCompleteWorkflow('./sample-payload.txt', {
                encryptionMethod: 'aes-256-gcm',
                stubType: 'cpp-loader'
            });

            // Demo 2: Quick encrypt-only
            console.log('\nDEMO 2: Quick Encrypt-Only (No Stub)');
            console.log('=====================================');
            const quickResult = await this.controller.quickEncryptOnly('./sample-payload.txt', 'chacha20');

            // Demo 3: Reuse encrypted payload
            console.log('\nDEMO 3: Reuse Encrypted Payload');
            console.log('================================');
            const reuseResult = await this.controller.reuseEncryptedPayload(quickResult.payloadId, 'asm-inject');

            // Demo 4: Reuse existing stub
            console.log('\nDEMO 4: Reuse Existing Stub (Your Efficiency Strategy)');
            console.log('======================================================');
            const stubReuseResult = await this.controller.reuseExistingStub(workflowResult.boundStub.id);

            // Show statistics
            console.log('\nWORKFLOW STATISTICS');
            console.log('===================');
            const stats = this.controller.getWorkflowStats();
            console.log('Total Workflows:', stats.totalWorkflows);
            console.log('Successful:', stats.successfulWorkflows);
            console.log('Stored Payloads:', stats.encryptor.totalPayloadsStored);
            console.log('Reuseable Stubs:', stats.stubManager.reuseableStubs);
            console.log('Efficiency Rating:', stats.stubManager.efficiency);
            console.log('Average Time:', stats.efficiency.avgWorkflowTime);

            // Show reuseable resources
            console.log('\nREUSEABLE RESOURCES');
            console.log('===================');
            const resources = this.controller.listReuseableResources();
            console.log(`Encrypted Payloads: ${resources.encryptedPayloads.length}`);
            console.log(`Reuseable Stubs: ${resources.reuseableStubs.length}`);
            
            resources.reuseableStubs.forEach(stub => {
                console.log(`  - Stub ${stub.id}: ${stub.reuseCount}/${stub.maxReuse} uses`);
            });

            console.log('\n================================================================');
            console.log('  DEMO COMPLETE - Separated Encryption Workflow Ready!');
            console.log('  Your burn-and-reuse strategy is now fully implemented.');
            console.log('================================================================');

        } catch (error) {
            console.error('Demo failed:', error.message);
        } finally {
            // Cleanup
            try {
                await fs.unlink('./sample-payload.txt');
            } catch (e) {
                // Ignore cleanup errors
            }
        }
    }

    async createSampleFile() {
        const sampleContent = `
// Sample payload for encryption testing
const payload = {
    message: "RawrZ Professional Encryption Test",
    timestamp: "${new Date().toISOString()}",
    data: "This is sample data for testing separated encryption workflow",
    size: 1024
};

console.log("Payload executed successfully");
        `;
        
        await fs.writeFile('./sample-payload.txt', sampleContent.trim());
        console.log('[SETUP] Created sample payload file for demo\n');
    }
}

// Run demo if executed directly
if (require.main === module) {
    const demo = new SeparatedEncryptionDemo();
    demo.runDemo().catch(console.error);
}

module.exports = SeparatedEncryptionDemo;
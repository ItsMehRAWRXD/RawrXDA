/**
 * Model Resolution System
 * Handles model aliases, capability negotiation, and offline fallback chains
 * Compatible with BigDaddyG:Latest and all model formats
 */

import registry from '../config/models.registry.json';
import { LLMError } from './errors.js';

/**
 * Model Resolution System for Offline-First Operation
 * Handles aliases like "BigDaddyG:Latest" and maps them to local models
 */
export class ModelResolver {
    constructor() {
        this.aliases = registry.aliases;
        this.capabilities = registry.capabilities;
        this.fallbacks = registry.fallbacks;
        this.policies = registry.policies;
        this.installed = registry.installed || [];
        this.initialized = false;
    }

    async initialize() {
        if (this.initialized) return;

        console.log('🔧 Initializing Model Resolver...');

        // Load installed models from cache
        await this.loadInstalledModels();

        // Validate model registry
        await this.validateRegistry();

        this.initialized = true;
        console.log('✅ Model Resolver initialized');
    }

    async loadInstalledModels() {
        try {
            // Scan models directory for installed models
            const modelFiles = await this.scanModelsDirectory();
            this.installed = modelFiles.map(file => this.parseModelFile(file));
            console.log(`📚 Found ${this.installed.length} installed models`);
        } catch (error) {
            console.warn('⚠️ Failed to load installed models:', error);
        }
    }

    async scanModelsDirectory() {
        // In browser environment, scan IndexedDB or file system
        try {
            if (typeof window !== 'undefined' && window.runtimeAPI) {
                return await window.runtimeAPI.listFiles('/models');
            }
        } catch (error) {
            console.warn('⚠️ Failed to scan models directory:', error);
        }
        return [];
    }

    parseModelFile(filePath) {
        // Parse model file to extract metadata
        const fileName = filePath.split('/').pop();
        const extension = fileName.split('.').pop().toLowerCase();

        // Determine model format
        let format = 'unknown';
        if (extension === 'gguf') format = 'gguf';
        else if (extension === 'onnx') format = 'onnx';
        else if (extension === 'bin') format = 'bin';
        else if (extension === 'safetensors') format = 'safetensors';

        // Extract model ID from path
        const modelId = fileName.replace(/\.(gguf|onnx|bin|safetensors)$/, '');

        return {
            id: `local:${modelId}.${extension}`,
            path: filePath,
            format,
            size: 0, // Would be calculated from file size
            verified: true,
            timestamp: Date.now()
        };
    }

    async validateRegistry() {
        // Validate registry integrity
        const requiredKeys = ['aliases', 'capabilities', 'fallbacks', 'policies'];
        const missingKeys = requiredKeys.filter(key => !registry[key]);

        if (missingKeys.length > 0) {
            throw new Error(`Registry missing required sections: ${missingKeys.join(', ')}`);
        }

        console.log('✅ Registry validation complete');
    }

    /**
     * Resolve model alias to actual model implementation
     * Handles BigDaddyG:Latest and other aliases with offline fallbacks
     */
    async resolve(input) {
        await this.initialize();

        const normalizedInput = this.normalizeInput(input);
        console.log(`🔍 Resolving model: ${input} -> ${normalizedInput}`);

        try {
            // Step 1: Check direct alias mapping
            let candidate = this.findAliasMatch(normalizedInput);

            // Step 2: Check installed models
            if (!candidate) {
                candidate = this.findInstalledMatch(normalizedInput);
            }

            // Step 3: Check fallback chains
            if (!candidate) {
                candidate = this.findFallbackMatch(normalizedInput);
            }

            // Step 4: Validate candidate
            if (!candidate) {
                throw new LLMError(
                    'MODEL_NOT_FOUND',
                    `Model "${input}" not available. Check model registry or install locally.`
                );
            }

            // Step 5: Apply policy constraints
            this.applyPolicies(candidate);

            // Step 6: Select best backend
            const backend = this.selectBackend(candidate);

            // Step 7: Create resolved model
            const resolved = {
                modelId: candidate.id,
                path: candidate.path,
                backend,
                format: candidate.format,
                capabilities: this.capabilities[candidate.id] || [],
                metadata: candidate,
                resolvedFrom: input,
                timestamp: Date.now()
            };

            console.log(`✅ Model resolved: ${input} -> ${resolved.modelId}`);
            return resolved;

        } catch (error) {
            console.error(`❌ Model resolution failed: ${input}`, error);
            throw error;
        }
    }

    normalizeInput(input) {
        // Normalize input for consistent matching
        return input.trim().toLowerCase().replace(/\s+/g, '').replace(/@/g, ':');
    }

    findAliasMatch(normalizedInput) {
        // Check exact alias match
        if (this.aliases[normalizedInput]) {
            const aliasTarget = this.aliases[normalizedInput];
            return this.findInstalledModel(aliasTarget);
        }

        // Check partial matches (e.g., "bigdaddyg" matches "bigdaddyg:latest")
        for (const [alias, target] of Object.entries(this.aliases)) {
            if (alias.includes(normalizedInput) || normalizedInput.includes(alias)) {
                const model = this.findInstalledModel(target);
                if (model) return model;
            }
        }

        return null;
    }

    findInstalledMatch(normalizedInput) {
        // Direct match with installed models
        const model = this.installed.find(m => m.id === normalizedInput);
        if (model) return model;

        // Partial match with installed models
        return this.installed.find(m =>
            m.id.toLowerCase().includes(normalizedInput) ||
            normalizedInput.includes(m.id.toLowerCase())
        );
    }

    findFallbackMatch(normalizedInput) {
        // Determine fallback category based on input
        const category = this.determineCategory(normalizedInput);

        if (this.fallbacks[category]) {
            for (const fallbackId of this.fallbacks[category]) {
                const model = this.findInstalledModel(fallbackId);
                if (model) return model;
            }
        }

        return null;
    }

    determineCategory(normalizedInput) {
        if (normalizedInput.includes('chat') || normalizedInput.includes('gpt') || normalizedInput.includes('claude')) {
            return 'chat-default';
        }
        if (normalizedInput.includes('code') || normalizedInput.includes('supernova') || normalizedInput.includes('codex')) {
            return 'code-default';
        }
        if (normalizedInput.includes('completion') || normalizedInput.includes('text')) {
            return 'completion-default';
        }
        return 'chat-default'; // Default fallback
    }

    findInstalledModel(modelId) {
        return this.installed.find(m => m.id === modelId);
    }

    applyPolicies(candidate) {
        // Apply policy constraints
        if (this.policies.offline_only && candidate.id.startsWith('remote:')) {
            throw new LLMError(
                'OFFLINE_REQUIRED',
                'Remote models not allowed in offline-only mode'
            );
        }

        if (this.policies.max_vram_mb) {
            const requiredVRAM = this.estimateVRAM(candidate);
            if (requiredVRAM > this.policies.max_vram_mb) {
                throw new LLMError(
                    'INSUFFICIENT_VRAM',
                    `Model requires ${requiredVRAM}MB VRAM, limit is ${this.policies.max_vram_mb}MB`
                );
            }
        }

        if (this.policies.require_verification && !candidate.verified) {
            throw new LLMError(
                'UNVERIFIED_MODEL',
                'Model verification required but not verified'
            );
        }
    }

    estimateVRAM(candidate) {
        // Estimate VRAM requirements based on model parameters
        const baseVRAM = 1024; // 1GB base

        if (candidate.format === 'gguf') {
            return baseVRAM + (candidate.paramsB || 7) * 128; // MB per billion parameters
        }

        if (candidate.format === 'onnx') {
            return baseVRAM * 0.5; // ONNX typically more efficient
        }

        return baseVRAM;
    }

    selectBackend(candidate) {
        // Select best available backend for the model
        const backends = this.getAvailableBackends();

        // Prefer GPU backends for larger models
        if (backends.includes('webgpu') && this.isGPUModel(candidate)) {
            return 'webgpu';
        }

        if (backends.includes('wasm')) {
            return 'wasm';
        }

        if (backends.includes('onnx')) {
            return 'onnx';
        }

        throw new LLMError(
            'NO_BACKEND',
            'No compatible backend available for this model'
        );
    }

    getAvailableBackends() {
        const backends = [];

        // Check WebGPU availability
        if (typeof navigator !== 'undefined' && navigator.gpu) {
            backends.push('webgpu');
        }

        // Check WASM availability
        if (typeof WebAssembly === 'object') {
            backends.push('wasm');
        }

        // Check ONNX availability
        if (typeof window !== 'undefined' && window.ort) {
            backends.push('onnx');
        }

        return backends;
    }

    isGPUModel(candidate) {
        // Determine if model should prefer GPU
        return candidate.format === 'gguf' && (candidate.paramsB || 0) > 3;
    }

    /**
     * List all available models with their capabilities
     */
    async listModels() {
        await this.initialize();

        const models = [];

        // Add installed models
        for (const model of this.installed) {
            models.push({
                id: model.id,
                name: model.id.split(':')[1] || model.id,
                format: model.format,
                capabilities: this.capabilities[model.id] || [],
                size: model.size || 0,
                verified: model.verified,
                local: true
            });
        }

        // Add alias mappings
        for (const [alias, target] of Object.entries(this.aliases)) {
            const model = this.findInstalledModel(target);
            if (model) {
                models.push({
                    id: alias,
                    name: alias,
                    aliasFor: target,
                    format: model.format,
                    capabilities: this.capabilities[target] || [],
                    local: true
                });
            }
        }

        return models;
    }

    /**
     * Check if a model is available (installed and verified)
     */
    async isModelAvailable(modelId) {
        try {
            const resolved = await this.resolve(modelId);
            return resolved && resolved.metadata.verified;
        } catch (error) {
            return false;
        }
    }

    /**
     * Get model capabilities without full resolution
     */
    getModelCapabilities(modelId) {
        const normalizedId = this.normalizeInput(modelId);
        let targetId = this.aliases[normalizedId];

        if (!targetId) {
            targetId = normalizedId;
        }

        return this.capabilities[targetId] || [];
    }

    /**
     * Add new model alias
     */
    async addAlias(alias, targetModel) {
        this.aliases[alias] = targetModel;
        await this.saveRegistry();
        console.log(`✅ Added alias: ${alias} -> ${targetModel}`);
    }

    /**
     * Install model from remote source
     */
    async installModel(modelConfig) {
        console.log(`📥 Installing model: ${modelConfig.name}`);

        try {
            // Download model
            const modelBuffer = await this.downloadModel(modelConfig.url);

            // Verify integrity
            const actualHash = await this.calculateHash(modelBuffer);
            if (modelConfig.hash && actualHash !== modelConfig.hash) {
                throw new Error('Model integrity check failed');
            }

            // Save to models directory
            await this.saveModel(modelConfig.name, modelBuffer);

            // Add to installed models
            this.installed.push({
                id: `local:${modelConfig.name}`,
                path: `/models/${modelConfig.name}`,
                format: modelConfig.format,
                size: modelBuffer.byteLength,
                verified: true,
                timestamp: Date.now()
            });

            console.log(`✅ Model installed: ${modelConfig.name}`);
            return true;

        } catch (error) {
            console.error(`❌ Model installation failed: ${modelConfig.name}`, error);
            throw error;
        }
    }

    async downloadModel(url) {
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`Failed to download model: ${response.status}`);
        }
        return response.arrayBuffer();
    }

    async saveModel(name, buffer) {
        // Save model to persistent storage
        if (typeof window !== 'undefined' && window.runtimeAPI) {
            await window.runtimeAPI.writeFile(`/models/${name}`, buffer);
        } else {
            // Fallback for Node.js environment
            const fs = await import('fs/promises');
            await fs.writeFile(`models/${name}`, Buffer.from(buffer));
        }
    }

    async calculateHash(buffer) {
        const hashBuffer = await crypto.subtle.digest('SHA-256', buffer);
        const hashArray = Array.from(new Uint8Array(hashBuffer));
        return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
    }

    async saveRegistry() {
        // Save updated registry to persistent storage
        if (typeof window !== 'undefined' && window.runtimeAPI) {
            await window.runtimeAPI.writeFile('/config/models.registry.json', JSON.stringify({
                aliases: this.aliases,
                capabilities: this.capabilities,
                fallbacks: this.fallbacks,
                policies: this.policies,
                installed: this.installed
            }, null, 2));
        } else {
            const fs = await import('fs/promises');
            await fs.writeFile('config/models.registry.json', JSON.stringify({
                aliases: this.aliases,
                capabilities: this.capabilities,
                fallbacks: this.fallbacks,
                policies: this.policies,
                installed: this.installed
            }, null, 2));
        }
    }

    /**
     * Get resolution statistics and diagnostics
     */
    getDiagnostics() {
        return {
            initialized: this.initialized,
            aliases: Object.keys(this.aliases).length,
            installed: this.installed.length,
            backends: this.getAvailableBackends(),
            policies: this.policies,
            fallbackChains: Object.keys(this.fallbacks).length
        };
    }

    /**
     * Test model resolution without loading
     */
    async testResolution(modelId) {
        try {
            const startTime = Date.now();
            const resolved = await this.resolve(modelId);
            const resolutionTime = Date.now() - startTime;

            return {
                success: true,
                modelId,
                resolved: resolved.modelId,
                backend: resolved.backend,
                resolutionTime,
                capabilities: resolved.capabilities
            };
        } catch (error) {
            return {
                success: false,
                modelId,
                error: error.message,
                errorCode: error.code
            };
        }
    }
}

// Global instance
export const modelResolver = new ModelResolver();

// Convenience function for quick resolution
export async function resolveModel(input) {
    return modelResolver.resolve(input);
}

// Convenience function for model listing
export async function listModels() {
    return modelResolver.listModels();
}

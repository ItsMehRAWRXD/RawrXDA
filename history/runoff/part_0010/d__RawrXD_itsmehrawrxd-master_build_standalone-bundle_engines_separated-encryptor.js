/**
 * RawrZ Separated Encryptor - Encryption Only (No Stub Generation)
 * Handles pure encryption without creating stubs - designed for efficiency
 */

const crypto = require('crypto');
const fs = require('fs').promises;
const path = require('path');
const { WindowsFormatEngine } = require('./windows-format-engine');
const { MultiLanguageStubGenerator } = require('./multi-language-stub-generator');

class SeparatedEncryptor {
    constructor() {
        this.name = 'SeparatedEncryptor';
        this.version = '2.0.0';
        this.windowsFormatEngine = new WindowsFormatEngine();
        this.multiLangStubGen = new MultiLanguageStubGenerator();
        
        this.encryptionMethods = {
            'aes-256-gcm': {
                name: 'AES-256-GCM',
                keySize: 32,
                ivSize: 12,
                security: 'high',
                performance: 'medium'
            },
            'aes-256-cbc': {
                name: 'AES-256-CBC',
                keySize: 32,
                ivSize: 16,
                security: 'high',
                performance: 'medium'
            },
            'chacha20': {
                name: 'ChaCha20',
                keySize: 32,
                ivSize: 12,
                security: 'high',
                performance: 'high'
            },
            'camellia-256-cbc': {
                name: 'Camellia-256-CBC',
                keySize: 32,
                ivSize: 16,
                security: 'high',
                performance: 'medium'
            },
            'hybrid': {
                name: 'Hybrid Multi-Layer',
                keySize: 32,
                ivSize: 16,
                security: 'custom',
                performance: 'high'
            }
        };

        this.encryptedPayloads = new Map(); // Store encrypted payloads for reuse
        this.encryptionCache = new Map(); // Cache for performance
    }

    /**
     * Pure encryption - no stub generation
     * @param {Buffer|string} data - Data to encrypt
     * @param {Object} options - Encryption options
     * @returns {Object} Encrypted result with metadata
     */
    async encryptOnly(data, options = {}) {
        const method = options.method || 'aes-256-gcm';
        const encryptionConfig = this.encryptionMethods[method];
        
        if (!encryptionConfig) {
            throw new Error(`Unsupported encryption method: ${method}`);
        }

        console.log(`[ENCRYPT] Using ${encryptionConfig.name} encryption`);
        console.log('[ENCRYPT] Pure encryption mode - no stub generation');

        let inputBuffer = Buffer.isBuffer(data) ? data : Buffer.from(data, 'utf8');
        
        let encryptedData;
        let encryptionKey;
        let iv;
        let authTag = null;

        switch (method) {
            case 'aes-256-gcm':
                encryptedData = await this.encryptAES256GCM(inputBuffer);
                break;
            case 'aes-256-cbc':
                encryptedData = await this.encryptAES256CBC(inputBuffer);
                break;
            case 'chacha20':
                encryptedData = await this.encryptChaCha20(inputBuffer);
                break;
            case 'camellia-256-cbc':
                encryptedData = await this.encryptCamellia256(inputBuffer);
                break;
            case 'hybrid':
                encryptedData = await this.encryptHybrid(inputBuffer);
                break;
            default:
                throw new Error(`Encryption method ${method} not implemented`);
        }

        const payloadId = crypto.randomUUID();
        const result = {
            payloadId: payloadId,
            method: method,
            encryptedData: encryptedData.encrypted,
            key: encryptedData.key,
            iv: encryptedData.iv,
            authTag: encryptedData.authTag,
            size: encryptedData.encrypted.length,
            timestamp: new Date().toISOString(),
            checksum: crypto.createHash('sha256').update(encryptedData.encrypted).digest('hex')
        };

        // Store for reuse
        this.encryptedPayloads.set(payloadId, result);
        console.log(`[STORED] Encrypted payload stored with ID: ${payloadId}`);
        console.log(`[READY] Payload ready for stub binding`);

        return result;
    }

    /**
     * Encrypt and format for specific Windows format
     */
    async encryptForWindowsFormat(data, method, formatType, options = {}) {
        try {
            console.log(`[ENCRYPT+FORMAT] Encrypting for ${formatType} format using ${method}`);
            
            // First encrypt the payload
            const encrypted = await this.encryptOnly(data, method);
            
            // Initialize Windows Format Engine if not done
            if (!this.windowsFormatEngine.initialized) {
                await this.windowsFormatEngine.initialize();
            }
            
            // Generate the specific Windows format
            const formatted = await this.windowsFormatEngine.generateFormat(
                formatType, 
                encrypted.encryptedData, 
                {
                    ...options,
                    encryptionMetadata: {
                        method: encrypted.method,
                        payloadId: encrypted.payloadId,
                        key: encrypted.key,
                        iv: encrypted.iv,
                        authTag: encrypted.authTag
                    }
                }
            );

            const result = {
                payloadId: encrypted.payloadId,
                method: encrypted.method,
                formatType: formatType,
                extension: formatted.extension,
                mimeType: formatted.mimeType,
                data: formatted.data,
                filename: formatted.filename,
                metadata: formatted.metadata,
                encryptionInfo: {
                    key: encrypted.key,
                    iv: encrypted.iv,
                    authTag: encrypted.authTag,
                    checksum: encrypted.checksum
                },
                timestamp: new Date().toISOString()
            };

            // Store formatted result
            this.encryptedPayloads.set(`${encrypted.payloadId}_${formatType}`, result);
            
            console.log(`[SUCCESS] Generated ${formatType} format for payload ${encrypted.payloadId}`);
            console.log(`[FORMAT] File: ${formatted.filename}, Size: ${formatted.data.length} bytes`);
            
            return result;
        } catch (error) {
            console.error(`[ERROR] Failed to encrypt for ${formatType} format:`, error.message);
            throw error;
        }
    }

    /**
     * Batch encrypt for multiple Windows formats
     */
    async encryptForMultipleFormats(data, method, formatTypes, options = {}) {
        try {
            console.log(`[BATCH] Encrypting for ${formatTypes.length} formats: ${formatTypes.join(', ')}`);
            
            const results = [];
            
            for (const formatType of formatTypes) {
                try {
                    const result = await this.encryptForWindowsFormat(data, method, formatType, options);
                    results.push(result);
                } catch (error) {
                    console.error(`[ERROR] Failed to generate ${formatType}:`, error.message);
                    results.push({
                        formatType: formatType,
                        error: error.message,
                        success: false
                    });
                }
            }
            
            const successful = results.filter(r => !r.error);
            const failed = results.filter(r => r.error);
            
            console.log(`[BATCH COMPLETE] ${successful.length} successful, ${failed.length} failed`);
            
            return {
                successful: successful,
                failed: failed,
                total: formatTypes.length,
                summary: {
                    successCount: successful.length,
                    failureCount: failed.length,
                    formats: formatTypes
                }
            };
        } catch (error) {
            console.error('[BATCH ERROR] Batch encryption failed:', error.message);
            throw error;
        }
    }

    /**
     * Get all supported Windows formats
     */
    getSupportedWindowsFormats() {
        if (!this.windowsFormatEngine.initialized) {
            return [];
        }
        return this.windowsFormatEngine.getSupportedFormats();
    }

    /**
     * Get Windows formats by category
     */
    getWindowsFormatsByCategory(category) {
        if (!this.windowsFormatEngine.initialized) {
            return [];
        }
        return this.windowsFormatEngine.getFormatsByCategory(category);
    }

    /**
     * Get stealth Windows formats (low detection)
     */
    getStealthWindowsFormats() {
        if (!this.windowsFormatEngine.initialized) {
            return [];
        }
        return this.windowsFormatEngine.getStealthFormats();
    }

    /**
     * Generate multi-language stubs for encrypted payload
     */
    async generateLanguageStubs(data, method, languages, options = {}) {
        try {
            console.log(`[MULTI-LANG] Generating stubs for ${languages.length} languages: ${languages.join(', ')}`);
            
            // First encrypt the payload
            const encrypted = await this.encryptOnly(data, method);
            
            // Initialize multi-language generator if not done
            if (!this.multiLangStubGen.initialized) {
                await this.multiLangStubGen.initialize();
            }
            
            // Generate stubs for all requested languages
            const result = await this.multiLangStubGen.generateMultiLanguageStubs(
                encrypted.encryptedData.toString('base64'), // Convert to base64 for templates
                languages,
                {
                    ...options,
                    decryptionKey: encrypted.key,
                    iv: encrypted.iv,
                    authTag: encrypted.authTag,
                    baseClassName: options.className || 'SecureStub'
                }
            );

            // Store results with encryption metadata
            const enhancedResult = {
                payloadId: encrypted.payloadId,
                encryptionMethod: encrypted.method,
                languages: languages,
                successful: result.successful.map(stub => ({
                    ...stub,
                    encryptionInfo: {
                        payloadId: encrypted.payloadId,
                        method: encrypted.method,
                        key: encrypted.key,
                        iv: encrypted.iv,
                        authTag: encrypted.authTag,
                        checksum: encrypted.checksum
                    }
                })),
                failed: result.failed,
                summary: result.summary,
                timestamp: new Date().toISOString()
            };

            // Store enhanced results
            this.encryptedPayloads.set(`${encrypted.payloadId}_multi_lang`, enhancedResult);

            console.log(`[MULTI-LANG] Generated ${result.successful.length} successful stubs, ${result.failed.length} failed`);
            
            return enhancedResult;
        } catch (error) {
            console.error('[MULTI-LANG ERROR] Failed to generate language stubs:', error.message);
            throw error;
        }
    }

    /**
     * Generate stub for specific language and Windows format combination
     */
    async generateLanguageFormatCombination(data, method, language, windowsFormat, options = {}) {
        try {
            console.log(`[COMBO] Generating ${language} stub in ${windowsFormat} format`);
            
            // First generate the language stub
            const langResult = await this.generateLanguageStubs(data, method, [language], options);
            
            if (langResult.successful.length === 0) {
                throw new Error(`Failed to generate ${language} stub`);
            }

            const langStub = langResult.successful[0];

            // Then format it for Windows
            const formatted = await this.encryptForWindowsFormat(
                langStub.sourceCode, 
                'aes-256-gcm', // Re-encrypt the source code
                windowsFormat,
                {
                    ...options,
                    displayName: `${langStub.className}_${windowsFormat}`,
                    targetPath: this.getExecutorForLanguage(language),
                    originalLanguage: language,
                    originalStub: langStub
                }
            );

            console.log(`[COMBO] Successfully generated ${language}+${windowsFormat} combination`);
            
            return {
                language: language,
                windowsFormat: windowsFormat,
                combination: formatted,
                originalStub: langStub,
                timestamp: new Date().toISOString()
            };
        } catch (error) {
            console.error(`[COMBO ERROR] Failed to generate ${language}+${windowsFormat}:`, error.message);
            throw error;
        }
    }

    /**
     * Get appropriate executor/interpreter for each language
     */
    getExecutorForLanguage(language) {
        const executors = {
            'python': 'python.exe',
            'javascript': 'node.exe', 
            'powershell': 'powershell.exe',
            'batch': 'cmd.exe',
            'vbscript': 'wscript.exe',
            'perl': 'perl.exe',
            'php': 'php.exe',
            'ruby': 'ruby.exe'
        };
        
        return executors[language] || 'cmd.exe';
    }

    /**
     * Get all supported languages
     */
    getSupportedLanguages() {
        if (!this.multiLangStubGen.initialized) {
            return [];
        }
        return this.multiLangStubGen.getSupportedLanguages();
    }

    /**
     * Get languages by runtime type
     */
    getLanguagesByRuntime(runtime) {
        if (!this.multiLangStubGen.initialized) {
            return [];
        }
        return this.multiLangStubGen.getLanguagesByRuntime(runtime);
    }

    /**
     * Get stealth languages (low detection)
     */
    getStealthLanguages() {
        if (!this.multiLangStubGen.initialized) {
            return [];
        }
        return this.multiLangStubGen.getStealthLanguages();
    }

    /**
     * Generate comprehensive payload matrix (all languages × all formats)
     */
    async generatePayloadMatrix(data, method, options = {}) {
        try {
            console.log('[MATRIX] Generating comprehensive payload matrix...');
            
            const languages = options.languages || ['cpp', 'python', 'javascript', 'powershell'];
            const formats = options.formats || ['exe', 'dll', 'ps1', 'hta', 'lnk', 'doc'];
            
            const results = {
                payloadId: crypto.randomUUID(),
                totalCombinations: languages.length * formats.length,
                successful: [],
                failed: [],
                matrix: {},
                timestamp: new Date().toISOString()
            };

            // Generate language stubs first
            const langResults = await this.generateLanguageStubs(data, method, languages, options);
            
            // Then format each successful language into each Windows format
            for (const langStub of langResults.successful) {
                results.matrix[langStub.language] = {};
                
                for (const format of formats) {
                    try {
                        const combo = await this.generateLanguageFormatCombination(
                            langStub.sourceCode, 
                            method,
                            langStub.language,
                            format,
                            options
                        );
                        
                        results.matrix[langStub.language][format] = combo;
                        results.successful.push(`${langStub.language}+${format}`);
                        
                    } catch (error) {
                        results.matrix[langStub.language][format] = { error: error.message };
                        results.failed.push(`${langStub.language}+${format}`);
                    }
                }
            }

            console.log(`[MATRIX] Generated ${results.successful.length} successful combinations, ${results.failed.length} failed`);
            
            return results;
        } catch (error) {
            console.error('[MATRIX ERROR] Failed to generate payload matrix:', error.message);
            throw error;
        }
    }

    /**
     * AES-256-GCM encryption
     */
    async encryptAES256GCM(data) {
        const key = crypto.randomBytes(32);
        const iv = crypto.randomBytes(12);
        
        const cipher = crypto.createCipher('aes-256-gcm', key);
        cipher.setAAD(Buffer.from('RawrZ-Auth', 'utf8'));
        
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);
        const authTag = cipher.getAuthTag();

        return {
            encrypted: encrypted,
            key: key.toString('hex'),
            iv: iv.toString('hex'),
            authTag: authTag.toString('hex')
        };
    }

    /**
     * AES-256-CBC encryption
     */
    async encryptAES256CBC(data) {
        const key = crypto.randomBytes(32);
        const iv = crypto.randomBytes(16);
        
        const cipher = crypto.createCipher('aes-256-cbc', key);
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);

        return {
            encrypted: encrypted,
            key: key.toString('hex'),
            iv: iv.toString('hex'),
            authTag: null
        };
    }

    /**
     * ChaCha20 encryption
     */
    async encryptChaCha20(data) {
        const key = crypto.randomBytes(32);
        const iv = crypto.randomBytes(12);
        
        const cipher = crypto.createCipher('chacha20', key);
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);

        return {
            encrypted: encrypted,
            key: key.toString('hex'),
            iv: iv.toString('hex'),
            authTag: null
        };
    }

    /**
     * Camellia-256-CBC encryption
     */
    async encryptCamellia256(data) {
        const key = crypto.randomBytes(32);
        const iv = crypto.randomBytes(16);
        
        const cipher = crypto.createCipher('camellia-256-cbc', key);
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);

        return {
            encrypted: encrypted,
            key: key.toString('hex'),
            iv: iv.toString('hex'),
            authTag: null
        };
    }

    /**
     * Hybrid multi-layer encryption
     */
    async encryptHybrid(data) {
        const key = crypto.randomBytes(32);
        const iv = crypto.randomBytes(16);
        
        // Layer 1: XOR with key
        let processed = Buffer.from(data);
        for (let i = 0; i < processed.length; i++) {
            processed[i] ^= key[i % key.length];
        }

        // Layer 2: Byte rotation
        for (let i = 0; i < processed.length; i++) {
            processed[i] = ((processed[i] << 3) | (processed[i] >> 5)) & 0xFF;
        }

        // Layer 3: AES encryption
        const cipher = crypto.createCipher('aes-256-cbc', key);
        let encrypted = cipher.update(processed);
        encrypted = Buffer.concat([encrypted, cipher.final()]);

        return {
            encrypted: encrypted,
            key: key.toString('hex'),
            iv: iv.toString('hex'),
            authTag: null
        };
    }

    /**
     * Encrypt file directly - pure encryption
     */
    async encryptFile(filePath, options = {}) {
        console.log(`[ENCRYPT] Processing file: ${path.basename(filePath)}`);
        
        try {
            const fileData = await fs.readFile(filePath);
            console.log(`[LOADED] File size: ${fileData.length} bytes`);
            
            const result = await this.encryptOnly(fileData, options);
            
            // Save encrypted payload if output path specified
            if (options.outputPath) {
                const encryptedPath = options.outputPath;
                await fs.writeFile(encryptedPath, result.encryptedData);
                console.log(`[SAVED] Encrypted payload saved to: ${encryptedPath}`);
            }

            return result;
        } catch (error) {
            console.error(`[ERROR] File encryption failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * Get stored encrypted payload for reuse
     */
    getStoredPayload(payloadId) {
        const payload = this.encryptedPayloads.get(payloadId);
        if (payload) {
            console.log(`[REUSE] Retrieved stored payload: ${payloadId}`);
            return payload;
        } else {
            console.log(`[ERROR] Payload not found: ${payloadId}`);
            return null;
        }
    }

    /**
     * List all stored encrypted payloads
     */
    listStoredPayloads() {
        const payloads = Array.from(this.encryptedPayloads.entries()).map(([id, payload]) => ({
            id: id,
            method: payload.method,
            size: payload.size,
            timestamp: payload.timestamp,
            checksum: payload.checksum.substring(0, 16) + '...'
        }));

        console.log(`[STORAGE] ${payloads.length} encrypted payload(s) available for reuse`);
        return payloads;
    }

    /**
     * Clear old payloads to save memory
     */
    clearOldPayloads(maxAge = 24 * 60 * 60 * 1000) { // 24 hours default
        const now = Date.now();
        let cleared = 0;

        for (const [id, payload] of this.encryptedPayloads.entries()) {
            const age = now - new Date(payload.timestamp).getTime();
            if (age > maxAge) {
                this.encryptedPayloads.delete(id);
                cleared++;
            }
        }

        console.log(`[CLEANUP] Cleared ${cleared} old payload(s)`);
        return cleared;
    }

    /**
     * Get supported encryption methods
     */
    getSupportedMethods() {
        return Object.entries(this.encryptionMethods).map(([key, config]) => ({
            method: key,
            name: config.name,
            security: config.security,
            performance: config.performance
        }));
    }

    /**
     * Get encryption statistics
     */
    getStats() {
        return {
            totalPayloadsStored: this.encryptedPayloads.size,
            supportedMethods: Object.keys(this.encryptionMethods).length,
            cacheSize: this.encryptionCache.size,
            version: this.version
        };
    }
}

module.exports = SeparatedEncryptor;
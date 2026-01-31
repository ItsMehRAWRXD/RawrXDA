// RawrZ Compression Engine - Advanced compression with multiple algorithms
const zlib = require('zlib');
const { promisify } = require('util');
const crypto = require('crypto');
const { logger } = require('../utils/logger');

class CompressionEngine {
    constructor() {
        this.algorithms = {
            gzip: {
                compress: promisify(zlib.gzip),
                decompress: promisify(zlib.gunzip),
                level: 6
            },
            deflate: {
                compress: promisify(zlib.deflate),
                decompress: promisify(zlib.inflate),
                level: 6
            },
            brotli: {
                compress: promisify(zlib.brotliCompress),
                decompress: promisify(zlib.brotliDecompress),
                level: 6
            }
        };
        
        this.compressionStats = {
            totalCompressed: 0,
            totalDecompressed: 0,
            totalSavings: 0,
            operations: 0
        };
    }

    async initialize(config) {
        this.config = config.compression || {};
        logger.info('Compression Engine initialized');
    }

    // Compress data with specified algorithm
    async compress(data, algorithm = 'gzip', options = {}) {
        const startTime = Date.now();
        
        try {
            // Convert data to buffer if needed
            const inputBuffer = Buffer.isBuffer(data) ? data : Buffer.from(data);
            const originalSize = inputBuffer.length;
            
            // Get compression function
            const compressionFunc = this.algorithms[algorithm]?.compress;
            if (!compressionFunc) {
                throw new Error(`Unsupported compression algorithm: ${algorithm}`);
            }
            
            // Prepare options
            const compressOptions = {
                level: options.level || this.algorithms[algorithm].level,
                ...options
            };
            
            // Compress data
            const compressedBuffer = await compressionFunc(inputBuffer, compressOptions);
            const compressedSize = compressedBuffer.length;
            const compressionRatio = ((originalSize - compressedSize) / originalSize * 100).toFixed(2);
            
            // Update stats
            this.compressionStats.totalCompressed += originalSize;
            this.compressionStats.totalSavings += (originalSize - compressedSize);
            this.compressionStats.operations++;
            
            const result = {
                algorithm,
                originalSize,
                compressedSize,
                compressionRatio: `${compressionRatio}%`,
                data: compressedBuffer,
                metadata: {
                    timestamp: new Date().toISOString(),
                    algorithm,
                    options: compressOptions
                }
            };
            
            logger.info(`Compression completed: ${algorithm}`, {
                originalSize,
                compressedSize,
                compressionRatio: `${compressionRatio}%`,
                duration: Date.now() - startTime
            });
            
            return result;
            
        } catch (error) {
            logger.error('Compression failed:', error);
            throw error;
        }
    }

    // Decompress data
    async decompress(compressedData, algorithm = 'gzip', options = {}) {
        const startTime = Date.now();
        
        try {
            // Convert data to buffer if needed
            const inputBuffer = Buffer.isBuffer(compressedData) ? compressedData : Buffer.from(compressedData);
            const compressedSize = inputBuffer.length;
            
            // Get decompression function
            const decompressionFunc = this.algorithms[algorithm]?.decompress;
            if (!decompressionFunc) {
                throw new Error(`Unsupported decompression algorithm: ${algorithm}`);
            }
            
            // Decompress data
            const decompressedBuffer = await decompressionFunc(inputBuffer, options);
            const originalSize = decompressedBuffer.length;
            
            // Update stats
            this.compressionStats.totalDecompressed += originalSize;
            this.compressionStats.operations++;
            
            const result = {
                algorithm,
                compressedSize,
                originalSize,
                data: decompressedBuffer,
                metadata: {
                    timestamp: new Date().toISOString(),
                    algorithm,
                    options
                }
            };
            
            logger.info(`Decompression completed: ${algorithm}`, {
                compressedSize,
                originalSize,
                duration: Date.now() - startTime
            });
            
            return result;
            
        } catch (error) {
            logger.error('Decompression failed:', error);
            throw error;
        }
    }

    // Compress with multiple algorithms and return best result
    async compressOptimal(data, algorithms = ['gzip', 'deflate', 'brotli']) {
        const startTime = Date.now();
        const results = [];
        
        try {
            // Test all algorithms
            for (const algorithm of algorithms) {
                try {
                    const result = await this.compress(data, algorithm);
                    results.push(result);
                } catch (error) {
                    logger.warn(`Compression failed for ${algorithm}:`, error.message);
                }
            }
            
            if (results.length === 0) {
                throw new Error('All compression algorithms failed');
            }
            
            // Find best compression ratio
            const bestResult = results.reduce((best, current) => {
                const bestRatio = parseFloat(best.compressionRatio);
                const currentRatio = parseFloat(current.compressionRatio);
                return currentRatio > bestRatio ? current : best;
            });
            
            logger.info(`Optimal compression found: ${bestResult.algorithm}`, {
                compressionRatio: bestResult.compressionRatio,
                duration: Date.now() - startTime,
                algorithmsTested: algorithms.length
            });
            
            return {
                ...bestResult,
                allResults: results,
                metadata: {
                    ...bestResult.metadata,
                    algorithmsTested: algorithms,
                    optimalAlgorithm: bestResult.algorithm
                }
            };
            
        } catch (error) {
            logger.error('Optimal compression failed:', error);
            throw error;
        }
    }

    // Compress file
    async compressFile(filePath, outputPath = null, algorithm = 'gzip') {
        const fs = require('fs').promises;
        const path = require('path');
        
        try {
            // Read file
            const fileData = await fs.readFile(filePath);
            
            // Compress data
            const compressed = await this.compress(fileData, algorithm);
            
            // Determine output path
            const output = outputPath || `${filePath}.${algorithm}`;
            
            // Write compressed file
            await fs.writeFile(output, compressed.data);
            
            logger.info(`File compressed: ${filePath} -> ${output}`, {
                algorithm,
                originalSize: compressed.originalSize,
                compressedSize: compressed.compressedSize,
                compressionRatio: compressed.compressionRatio
            });
            
            return {
                inputFile: filePath,
                outputFile: output,
                ...compressed
            };
            
        } catch (error) {
            logger.error(`File compression failed: ${filePath}`, error);
            throw error;
        }
    }

    // Decompress file
    async decompressFile(filePath, outputPath = null, algorithm = 'gzip') {
        const fs = require('fs').promises;
        const path = require('path');
        
        try {
            // Read compressed file
            const compressedData = await fs.readFile(filePath);
            
            // Decompress data
            const decompressed = await this.decompress(compressedData, algorithm);
            
            // Determine output path
            const output = outputPath || filePath.replace(`.${algorithm}`, '');
            
            // Write decompressed file
            await fs.writeFile(output, decompressed.data);
            
            logger.info(`File decompressed: ${filePath} -> ${output}`, {
                algorithm,
                compressedSize: decompressed.compressedSize,
                originalSize: decompressed.originalSize
            });
            
            return {
                inputFile: filePath,
                outputFile: output,
                ...decompressed
            };
            
        } catch (error) {
            logger.error(`File decompression failed: ${filePath}`, error);
            throw error;
        }
    }

    // Stream compression
    createCompressStream(algorithm = 'gzip', options = {}) {
        const compressionFunc = this.algorithms[algorithm];
        if (!compressionFunc) {
            throw new Error(`Unsupported compression algorithm: ${algorithm}`);
        }
        
        return zlib.createGzip({
            level: options.level || compressionFunc.level,
            ...options
        });
    }

    // Stream decompression
    createDecompressStream(algorithm = 'gzip', options = {}) {
        const decompressionFunc = this.algorithms[algorithm];
        if (!decompressionFunc) {
            throw new Error(`Unsupported decompression algorithm: ${algorithm}`);
        }
        
        return zlib.createGunzip(options);
    }

    // Get compression statistics
    getStats() {
        const totalProcessed = this.compressionStats.totalCompressed + this.compressionStats.totalDecompressed;
        const averageSavings = this.compressionStats.operations > 0 
            ? (this.compressionStats.totalSavings / this.compressionStats.operations).toFixed(2)
            : 0;
        
        return {
            ...this.compressionStats,
            totalProcessed,
            averageSavings,
            supportedAlgorithms: Object.keys(this.algorithms),
            uptime: Date.now() - this.startTime
        };
    }

    // Reset statistics
    resetStats() {
        this.compressionStats = {
            totalCompressed: 0,
            totalDecompressed: 0,
            totalSavings: 0,
            operations: 0
        };
        logger.info('Compression statistics reset');
    }

    // Get supported algorithms
    getSupportedAlgorithms() {
        return Object.keys(this.algorithms);
    }

    // Test compression algorithm
    async testAlgorithm(algorithm, testData = null) {
        try {
            const data = testData || Buffer.from('RawrZ Compression Engine Test Data - ' + Date.now());
            const compressed = await this.compress(data, algorithm);
            const decompressed = await this.decompress(compressed.data, algorithm);
            
            const isValid = data.equals(decompressed.data);
            
            return {
                algorithm,
                supported: true,
                valid: isValid,
                compressionRatio: compressed.compressionRatio,
                testDataSize: data.length
            };
            
        } catch (error) {
            return {
                algorithm,
                supported: false,
                error: error.message
            };
        }
    }

    // Test all algorithms
    async testAllAlgorithms(testData = null) {
        const results = [];
        
        for (const algorithm of Object.keys(this.algorithms)) {
            const result = await this.testAlgorithm(algorithm, testData);
            results.push(result);
        }
        
        return results;
    }

    // Get engine status
    getStatus() {
        return {
            name: 'Compression Engine',
            version: '1.0.0',
            initialized: true,
            algorithms: Object.keys(this.algorithms),
            compressionStats: this.compressionStats,
            status: 'ready',
            timestamp: new Date().toISOString()
        };
    }

    // Cleanup
    async cleanup() {
        logger.info('Compression Engine cleanup completed');
    }
}

// Create and export instance
const compressionEngine = new CompressionEngine();

module.exports = compressionEngine;

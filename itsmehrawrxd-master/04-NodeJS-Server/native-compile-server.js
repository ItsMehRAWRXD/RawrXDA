const express = require('express');
const multer = require('multer');
const { exec } = require('child_process');
const { promisify } = require('util');
const fs = require('fs').promises;
const path = require('path');

const execAsync = promisify(exec);
const app = express();
const PORT = 8080;

// Middleware
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));

// Multer for file uploads
const upload = multer({ 
    storage: multer.memoryStorage(),
    limits: { fileSize: 50 * 1024 * 1024 } // 50MB limit
});

// Health check
app.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        service: 'RawrZ Native Compilation Server',
        timestamp: new Date().toISOString(),
        version: '1.0.0'
    });
});

// Compile C/C++ source to native executable
app.post('/compile', async (req, res) => {
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

        console.log(`Compiling ${target} executable with ${optimization} optimization...`);

        // Set environment variables for compilation
        const env = {
            ...process.env,
            TARGET_ARCH: target,
            OPTIMIZATION: optimization
        };

        // Execute compilation script
        const { stdout, stderr } = await execAsync(
            'echo "$SRC" | /usr/local/bin/native-compile.sh',
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
        console.error('Compilation error:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            stderr: error.stderr
        });
    }
});

// Compile from file upload
app.post('/compile-file', upload.single('source'), async (req, res) => {
    try {
        if (!req.file) {
            return res.status(400).json({
                success: false,
                error: 'Source file is required'
            });
        }

        const source = req.file.buffer.toString('utf8');
        const { 
            target = 'linux', 
            optimization = 'release',
            filename = path.parse(req.file.originalname).name
        } = req.body;

        console.log(`Compiling uploaded file to ${target} executable...`);

        // Set environment variables
        const env = {
            ...process.env,
            TARGET_ARCH: target,
            OPTIMIZATION: optimization
        };

        // Execute compilation
        const { stdout, stderr } = await execAsync(
            'echo "$SRC" | /usr/local/bin/native-compile.sh',
            {
                env: { ...env, SRC: source },
                maxBuffer: 50 * 1024 * 1024
            }
        );

        if (stderr) {
            console.error('Compilation stderr:', stderr);
        }

        // Set response headers
        const extension = target === 'windows' ? '.exe' : '';
        const contentType = target === 'windows' ? 'application/x-msdownload' : 'application/octet-stream';
        
        res.set({
            'Content-Type': contentType,
            'Content-Disposition': `attachment; filename="${filename}${extension}"`,
            'X-Compilation-Target': target,
            'X-Optimization-Level': optimization,
            'X-Original-Filename': req.file.originalname,
            'X-Compilation-Time': new Date().toISOString()
        });

        res.send(Buffer.from(stdout, 'binary'));

    } catch (error) {
        console.error('File compilation error:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            stderr: error.stderr
        });
    }
});

// Cross-compile for multiple targets
app.post('/cross-compile', async (req, res) => {
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
                    'echo "$SRC" | /usr/local/bin/native-compile.sh',
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

// Get compilation info
app.get('/info', (req, res) => {
    res.json({
        service: 'RawrZ Native Compilation Server',
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
                'Multiple architectures'
            ]
        },
        endpoints: {
            'POST /compile': 'Compile source to native executable',
            'POST /compile-file': 'Compile uploaded file',
            'POST /cross-compile': 'Cross-compile for multiple targets',
            'GET /info': 'Get service information',
            'GET /health': 'Health check'
        }
    });
});

// Error handling middleware
app.use((error, req, res, next) => {
    console.error('Server error:', error);
    res.status(500).json({
        success: false,
        error: 'Internal server error',
        message: error.message
    });
});

// Start server
app.listen(PORT, '0.0.0.0', () => {
    console.log(`RawrZ Native Compilation Server running on port ${PORT}`);
    console.log('Available endpoints:');
    console.log('  POST /compile - Compile source to native executable');
    console.log('  POST /compile-file - Compile uploaded file');
    console.log('  POST /cross-compile - Cross-compile for multiple targets');
    console.log('  GET /info - Get service information');
    console.log('  GET /health - Health check');
});

module.exports = app;

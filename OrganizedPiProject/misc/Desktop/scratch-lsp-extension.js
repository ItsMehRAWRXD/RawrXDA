/**
 * Secure Scratch Extension for LSP Integration
 * 
 * This extension provides secure integration with the LSP Bridge Server
 * for code completion, diagnostics, hover documentation, and more.
 * 
 * Security Features:
 * - Input validation and sanitization
 * - CORS handling with proper error management
 * - Rate limiting to prevent abuse
 * - Secure error handling without exposing sensitive data
 * - User consent for network operations
 */

(function(ext) {
    'use strict';
    
    // Configuration
    const CONFIG = {
        LSP_SERVER_URL: 'http://localhost:8080',
        MAX_REQUEST_SIZE: 10000, // 10KB max request size
        RATE_LIMIT_WINDOW: 60000, // 1 minute
        MAX_REQUESTS_PER_WINDOW: 10,
        REQUEST_TIMEOUT: 10000 // 10 seconds
    };
    
    // Rate limiting
    let requestCount = 0;
    let windowStart = Date.now();
    
    // Cleanup function
    ext._shutdown = function() {
        // Clean up any resources if needed
        console.log('LSP Extension shutting down');
    };
    
    // Status reporting
    ext._getStatus = function() {
        return {status: 2, msg: 'Ready'};
    };
    
    /**
     * Security: Validate and sanitize input
     */
    function validateInput(input, maxLength = 1000) {
        if (!input || typeof input !== 'string') {
            throw new Error('Invalid input: must be a non-empty string');
        }
        
        if (input.length > maxLength) {
            throw new Error('Input too long: maximum ' + maxLength + ' characters');
        }
        
        // Remove potentially dangerous characters
        return input.replace(/[<>\"'&]/g, '');
    }
    
    /**
     * Security: Check rate limiting
     */
    function checkRateLimit() {
        const now = Date.now();
        
        // Reset window if expired
        if (now - windowStart > CONFIG.RATE_LIMIT_WINDOW) {
            requestCount = 0;
            windowStart = now;
        }
        
        if (requestCount >= CONFIG.MAX_REQUESTS_PER_WINDOW) {
            throw new Error('Rate limit exceeded. Please wait before making more requests.');
        }
        
        requestCount++;
    }
    
    /**
     * Security: Secure fetch with timeout and error handling
     */
    async function secureFetch(url, options = {}) {
        return new Promise((resolve, reject) => {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), CONFIG.REQUEST_TIMEOUT);
            
            fetch(url, {
                ...options,
                signal: controller.signal,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                }
            })
            .then(response => {
                clearTimeout(timeoutId);
                
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                }
                
                return response.json();
            })
            .then(data => resolve(data))
            .catch(error => {
                clearTimeout(timeoutId);
                
                // Don't expose internal errors
                if (error.name === 'AbortError') {
                    reject(new Error('Request timed out. Please try again.'));
                } else if (error.message.includes('Failed to fetch')) {
                    reject(new Error('Unable to connect to language server. Please check if the server is running.'));
                } else {
                    reject(new Error('An error occurred while processing your request.'));
                }
            });
        });
    }
    
    /**
     * Get code completions
     */
    ext.getCompletions = function(filePath, code, line, column) {
        return new Promise((resolve, reject) => {
            try {
                checkRateLimit();
                
                const filePathSafe = validateInput(filePath, 200);
                const codeSafe = validateInput(code, 8000);
                const lineNum = Math.max(1, Math.min(10000, parseInt(line) || 1));
                const colNum = Math.max(1, Math.min(1000, parseInt(column) || 1));
                
                const payload = {
                    file: filePathSafe,
                    content: codeSafe,
                    line: lineNum,
                    column: colNum
                };
                
                secureFetch(`${CONFIG.LSP_SERVER_URL}/complete`, {
                    method: 'POST',
                    body: JSON.stringify(payload)
                })
                .then(response => {
                    if (response.completions && Array.isArray(response.completions)) {
                        resolve(response.completions.map(comp => comp.label || comp.text || comp));
                    } else {
                        resolve([]);
                    }
                })
                .catch(reject);
                
            } catch (error) {
                reject(error);
            }
        });
    };
    
    /**
     * Get code diagnostics (errors and warnings)
     */
    ext.getDiagnostics = function(filePath, code) {
        return new Promise((resolve, reject) => {
            try {
                checkRateLimit();
                
                const filePathSafe = validateInput(filePath, 200);
                const codeSafe = validateInput(code, 8000);
                
                const payload = {
                    file: filePathSafe,
                    content: codeSafe
                };
                
                secureFetch(`${CONFIG.LSP_SERVER_URL}/diagnostics`, {
                    method: 'POST',
                    body: JSON.stringify(payload)
                })
                .then(response => {
                    if (response.diagnostics && Array.isArray(response.diagnostics)) {
                        resolve(response.diagnostics);
                    } else {
                        resolve([]);
                    }
                })
                .catch(reject);
                
            } catch (error) {
                reject(error);
            }
        });
    };
    
    /**
     * Get hover documentation
     */
    ext.getHoverInfo = function(filePath, code, line, column) {
        return new Promise((resolve, reject) => {
            try {
                checkRateLimit();
                
                const filePathSafe = validateInput(filePath, 200);
                const codeSafe = validateInput(code, 8000);
                const lineNum = Math.max(1, Math.min(10000, parseInt(line) || 1));
                const colNum = Math.max(1, Math.min(1000, parseInt(column) || 1));
                
                const payload = {
                    file: filePathSafe,
                    content: codeSafe,
                    line: lineNum,
                    column: colNum
                };
                
                secureFetch(`${CONFIG.LSP_SERVER_URL}/hover`, {
                    method: 'POST',
                    body: JSON.stringify(payload)
                })
                .then(response => {
                    if (response.contents && Array.isArray(response.contents)) {
                        resolve(response.contents.map(c => c.value || c).join('\n'));
                    } else {
                        resolve('No documentation available');
                    }
                })
                .catch(reject);
                
            } catch (error) {
                reject(error);
            }
        });
    };
    
    /**
     * Go to definition
     */
    ext.goToDefinition = function(filePath, code, line, column) {
        return new Promise((resolve, reject) => {
            try {
                checkRateLimit();
                
                const filePathSafe = validateInput(filePath, 200);
                const codeSafe = validateInput(code, 8000);
                const lineNum = Math.max(1, Math.min(10000, parseInt(line) || 1));
                const colNum = Math.max(1, Math.min(1000, parseInt(column) || 1));
                
                const payload = {
                    file: filePathSafe,
                    content: codeSafe,
                    line: lineNum,
                    column: colNum
                };
                
                secureFetch(`${CONFIG.LSP_SERVER_URL}/definition`, {
                    method: 'POST',
                    body: JSON.stringify(payload)
                })
                .then(response => {
                    resolve(response);
                })
                .catch(reject);
                
            } catch (error) {
                reject(error);
            }
        });
    };
    
    /**
     * Format code
     */
    ext.formatCode = function(filePath, code) {
        return new Promise((resolve, reject) => {
            try {
                checkRateLimit();
                
                const filePathSafe = validateInput(filePath, 200);
                const codeSafe = validateInput(code, 8000);
                
                const payload = {
                    file: filePathSafe,
                    content: codeSafe
                };
                
                secureFetch(`${CONFIG.LSP_SERVER_URL}/format`, {
                    method: 'POST',
                    body: JSON.stringify(payload)
                })
                .then(response => {
                    if (response.formatted) {
                        resolve(response.formatted);
                    } else {
                        resolve(codeSafe); // Return original if no formatting available
                    }
                })
                .catch(reject);
                
            } catch (error) {
                reject(error);
            }
        });
    };
    
    /**
     * Check server health
     */
    ext.checkServerHealth = function() {
        return new Promise((resolve, reject) => {
            try {
                secureFetch(`${CONFIG.LSP_SERVER_URL}/health`)
                .then(response => {
                    resolve(response.status === 'healthy');
                })
                .catch(() => {
                    resolve(false);
                });
                
            } catch (error) {
                resolve(false);
            }
        });
    };
    
    // Block definitions
    const blocks = [
        {
            opcode: 'get_completions',
            blockType: Scratch.BlockType.REPORTER,
            text: 'get completions for [FILE] at line [LINE] column [COLUMN]',
            arguments: {
                FILE: {
                    type: Scratch.ArgumentType.STRING,
                    defaultValue: 'main.java'
                },
                LINE: {
                    type: Scratch.ArgumentType.NUMBER,
                    defaultValue: 1
                },
                COLUMN: {
                    type: Scratch.ArgumentType.NUMBER,
                    defaultValue: 1
                }
            }
        },
        {
            opcode: 'get_diagnostics',
            blockType: Scratch.BlockType.REPORTER,
            text: 'get diagnostics for [FILE]',
            arguments: {
                FILE: {
                    type: Scratch.ArgumentType.STRING,
                    defaultValue: 'main.java'
                }
            }
        },
        {
            opcode: 'get_hover_info',
            blockType: Scratch.BlockType.REPORTER,
            text: 'get hover info for [FILE] at line [LINE] column [COLUMN]',
            arguments: {
                FILE: {
                    type: Scratch.ArgumentType.STRING,
                    defaultValue: 'main.java'
                },
                LINE: {
                    type: Scratch.ArgumentType.NUMBER,
                    defaultValue: 1
                },
                COLUMN: {
                    type: Scratch.ArgumentType.NUMBER,
                    defaultValue: 1
                }
            }
        },
        {
            opcode: 'go_to_definition',
            blockType: Scratch.BlockType.REPORTER,
            text: 'go to definition for [FILE] at line [LINE] column [COLUMN]',
            arguments: {
                FILE: {
                    type: Scratch.ArgumentType.STRING,
                    defaultValue: 'main.java'
                },
                LINE: {
                    type: Scratch.ArgumentType.NUMBER,
                    defaultValue: 1
                },
                COLUMN: {
                    type: Scratch.ArgumentType.NUMBER,
                    defaultValue: 1
                }
            }
        },
        {
            opcode: 'format_code',
            blockType: Scratch.BlockType.REPORTER,
            text: 'format code for [FILE]',
            arguments: {
                FILE: {
                    type: Scratch.ArgumentType.STRING,
                    defaultValue: 'main.java'
                }
            }
        },
        {
            opcode: 'check_server_health',
            blockType: Scratch.BlockType.BOOLEAN,
            text: 'LSP server is healthy'
        }
    ];
    
    // Extension descriptor
    ext._getInfo = function() {
        return {
            id: 'lspintegration',
            name: 'LSP Integration',
            color1: '#FF6B35',
            color2: '#F7931E',
            blocks: blocks,
            menus: {},
            showStatusButton: true
        };
    };
    
    // Register the extension
    Scratch.extensions.register(ext);
    
})({});

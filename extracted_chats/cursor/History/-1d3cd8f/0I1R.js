const http = require('http');
const url = require('url');

// BigDaddyG Assembly Model Server
// This server provides AI model services for the BigDaddyG Assembly project

const PORT = process.env.MODEL_PORT_ASSEMBLY || 11441;
const HOST = 'localhost';

// Simple in-memory storage for conversation history
let conversationHistory = [];

// CORS headers for web requests
const corsHeaders = {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    'Content-Type': 'application/json'
};

// Health check endpoint
const healthCheck = (res) => {
    res.writeHead(200, corsHeaders);
    res.end(JSON.stringify({ 
        status: 'healthy', 
        service: 'BigDaddyG Assembly Model Server',
        port: PORT,
        timestamp: new Date().toISOString()
    }));
};

// Chat completion endpoint
const handleChatCompletion = (req, res) => {
    let body = '';
    
    req.on('data', chunk => {
        body += chunk.toString();
    });
    
    req.on('end', () => {
        try {
            const data = JSON.parse(body);
            const { messages, model = 'BigDaddyG:Latest', temperature = 0.7 } = data;
            
            // Add to conversation history
            conversationHistory.push(...messages);
            
            // Extract emotional state from system message (if provided)
            let emotionalState = 'CALM';
            const systemMsg = messages.find(m => m.role === 'system');
            if (systemMsg && systemMsg.content) {
                const stateMatch = systemMsg.content.match(/\[Emotional State: (\w+)\]/);
                if (stateMatch) {
                    emotionalState = stateMatch[1];
                }
            }
            
            // Get user's message
            const lastMessage = messages[messages.length - 1];
            const userInput = lastMessage.content;
            
            // Route to appropriate model
            let response;
            const modelLower = model.toLowerCase();
            
            console.log(`[BigDaddyG] 🤖 Model: ${model} | State: ${emotionalState} | Query: ${userInput.substring(0, 50)}...`);
            
            if (modelLower.includes('code')) {
                response = generateCodeResponse(userInput, emotionalState);
            } else if (modelLower.includes('debug')) {
                response = generateDebugResponse(userInput, emotionalState);
            } else if (modelLower.includes('crypto') || modelLower.includes('security')) {
                response = generateCryptoResponse(userInput, emotionalState);
            } else if (modelLower.includes('assembly')) {
                response = generateResponse(userInput, emotionalState);
            } else {
                // Default: BigDaddyG:Latest
                response = generateResponse(userInput, emotionalState);
            }
            
            // Calculate token usage (rough estimate)
            const promptTokens = Math.ceil(userInput.length / 4);
            const completionTokens = Math.ceil(response.length / 4);
            
            const completion = {
                id: `chatcmpl-${Date.now()}`,
                object: 'chat.completion',
                created: Math.floor(Date.now() / 1000),
                model: model,
                choices: [{
                    index: 0,
                    message: {
                        role: 'assistant',
                        content: response
                    },
                    finish_reason: 'stop'
                }],
                usage: {
                    prompt_tokens: promptTokens,
                    completion_tokens: completionTokens,
                    total_tokens: promptTokens + completionTokens
                }
            };
            
            res.writeHead(200, corsHeaders);
            res.end(JSON.stringify(completion));
            
        } catch (error) {
            console.error('[BigDaddyG] ❌ Error:', error);
            res.writeHead(400, corsHeaders);
            res.end(JSON.stringify({ error: 'Invalid JSON in request body', details: error.message }));
        }
    });
};

// ========== SPECIALIZED MODEL RESPONSE GENERATORS ==========

// BigDaddyG:Code - Code Generation Specialist
const generateCodeResponse = (input, emotionalState = 'CALM') => {
    const inputLower = input.toLowerCase();
    
    // Detect programming language
    let language = 'javascript';
    if (inputLower.includes('python')) language = 'python';
    if (inputLower.includes('c++') || inputLower.includes('cpp')) language = 'cpp';
    if (inputLower.includes('assembly') || inputLower.includes('asm')) language = 'assembly';
    if (inputLower.includes('rust')) language = 'rust';
    if (inputLower.includes('go')) language = 'go';
    
    // Code generation templates
    const codeTemplates = {
        javascript: {
            function: `// Generated by BigDaddyG:Code\nfunction processData(input) {\n    // TODO: Implement logic\n    return input;\n}`,
            class: `class DataProcessor {\n    constructor() {\n        this.data = [];\n    }\n    \n    process(input) {\n        // Implementation\n        return input;\n    }\n}`,
            async: `async function fetchData(url) {\n    try {\n        const response = await fetch(url);\n        return await response.json();\n    } catch (error) {\n        console.error('Error:', error);\n    }\n}`
        },
        python: {
            function: `# Generated by BigDaddyG:Code\ndef process_data(input_data):\n    """Process the input data"""\n    # TODO: Implement logic\n    return input_data`,
            class: `class DataProcessor:\n    def __init__(self):\n        self.data = []\n    \n    def process(self, input_data):\n        """Process input"""\n        return input_data`,
            async: `import asyncio\nimport aiohttp\n\nasync def fetch_data(url):\n    async with aiohttp.ClientSession() as session:\n        async with session.get(url) as response:\n            return await response.json()`
        },
        cpp: {
            function: `// Generated by BigDaddyG:Code\nint processData(int input) {\n    // TODO: Implement logic\n    return input;\n}`,
            class: `class DataProcessor {\nprivate:\n    std::vector<int> data;\n    \npublic:\n    DataProcessor() {}\n    \n    int process(int input) {\n        return input;\n    }\n};`,
            template: `template<typename T>\nclass Processor {\npublic:\n    T process(T input) {\n        return input;\n    }\n};`
        }
    };
    
    // Select appropriate template
    let template = codeTemplates[language]?.function || codeTemplates.javascript.function;
    
    if (inputLower.includes('class')) template = codeTemplates[language]?.class;
    if (inputLower.includes('async')) template = codeTemplates[language]?.async;
    
    // Emotional context adjustments
    let explanation = '';
    if (emotionalState === 'FOCUSED') {
        explanation = '\n\n// Focused mode: Code is concise and well-structured';
    } else if (emotionalState === 'INTENSE') {
        explanation = '\n\n// Intense mode: Optimized for speed and debugging';
    } else if (emotionalState === 'OVERWHELMED') {
        explanation = '\n\n// Calm mode: Code includes helpful comments and examples';
        template += '\n\n// Example usage:\n// const result = processData(yourInput);';
    }
    
    return `I'll help you with that code. Here's a ${language} solution:\n\n${template}${explanation}\n\nAdditional notes about your request "${input}":\n- Language detected: ${language}\n- Generated with emotional context: ${emotionalState}\n- Ready for customization and extension`;
};

// BigDaddyG:Debug - Debugging and Error Analysis Specialist
const generateDebugResponse = (input, emotionalState = 'CALM') => {
    const inputLower = input.toLowerCase();
    
    // Detect debug scenario
    let debugType = 'general';
    if (inputLower.includes('error') || inputLower.includes('exception')) debugType = 'error';
    if (inputLower.includes('crash') || inputLower.includes('segfault')) debugType = 'crash';
    if (inputLower.includes('memory') || inputLower.includes('leak')) debugType = 'memory';
    if (inputLower.includes('performance') || inputLower.includes('slow')) debugType = 'performance';
    if (inputLower.includes('logic') || inputLower.includes('bug')) debugType = 'logic';
    
    const debugStrategies = {
        error: {
            steps: [
                '1. Check the error message and stack trace',
                '2. Verify input data types and values',
                '3. Look for null/undefined references',
                '4. Check API endpoints and network calls',
                '5. Review recent code changes'
            ],
            tools: ['console.log', 'debugger', 'try-catch blocks', 'error logging'],
            tip: 'Most errors are caused by unexpected input or null values'
        },
        crash: {
            steps: [
                '1. Check for memory access violations',
                '2. Review pointer dereferencing',
                '3. Look for buffer overflows',
                '4. Check array bounds',
                '5. Verify resource cleanup (file handles, sockets)'
            ],
            tools: ['valgrind', 'address sanitizer', 'debugger breakpoints'],
            tip: 'Crashes often indicate memory corruption or resource issues'
        },
        memory: {
            steps: [
                '1. Profile memory usage over time',
                '2. Check for unclosed resources',
                '3. Look for circular references',
                '4. Review object lifecycle management',
                '5. Use memory profiling tools'
            ],
            tools: ['heap profiler', 'memory analyzer', 'resource monitors'],
            tip: 'Memory leaks accumulate over time - test with extended runs'
        },
        performance: {
            steps: [
                '1. Profile execution time with tools',
                '2. Identify hotspots and bottlenecks',
                '3. Check for unnecessary loops or recursion',
                '4. Review database query efficiency',
                '5. Consider caching strategies'
            ],
            tools: ['profiler', 'performance.now()', 'chrome devtools'],
            tip: 'Optimization rule: measure first, then optimize'
        },
        logic: {
            steps: [
                '1. Add console.log at key decision points',
                '2. Verify conditional logic (if/else)',
                '3. Check loop conditions and boundaries',
                '4. Review state management',
                '5. Test edge cases'
            ],
            tools: ['console logging', 'debugger stepping', 'unit tests'],
            tip: 'Break complex logic into smaller, testable functions'
        }
    };
    
    const strategy = debugStrategies[debugType] || debugStrategies.general;
    
    // Emotional context adjustments
    let tone = '';
    if (emotionalState === 'OVERWHELMED') {
        tone = '\n\n💡 Take a breath. Debugging is a systematic process. Let\'s break this down step by step.';
    } else if (emotionalState === 'INTENSE') {
        tone = '\n\n⚡ Quick debugging protocol activated. Focus on the critical path.';
    } else if (emotionalState === 'FOCUSED') {
        tone = '\n\n🎯 Systematic debugging approach engaged.';
    }
    
    return `BigDaddyG:Debug Analysis for: "${input}"

🔍 Debug Type: ${debugType.toUpperCase()}
📋 Emotional State: ${emotionalState}

DEBUGGING STRATEGY:
${strategy.steps.join('\n')}

RECOMMENDED TOOLS:
${strategy.tools.map(t => '• ' + t).join('\n')}

💡 PRO TIP: ${strategy.tip}${tone}

NEXT STEPS:
1. Apply the strategy above systematically
2. Document what you find at each step
3. Test your fix thoroughly
4. Consider adding unit tests to prevent regression

Would you like me to provide more specific guidance for any step?`;
};

// BigDaddyG:Crypto - Encryption and Security Specialist
const generateCryptoResponse = (input, emotionalState = 'CALM') => {
    const inputLower = input.toLowerCase();
    
    // Detect crypto topic
    let cryptoType = 'general';
    if (inputLower.includes('encrypt') || inputLower.includes('cipher')) cryptoType = 'encryption';
    if (inputLower.includes('decrypt')) cryptoType = 'decryption';
    if (inputLower.includes('hash') || inputLower.includes('sha') || inputLower.includes('md5')) cryptoType = 'hashing';
    if (inputLower.includes('key') || inputLower.includes('rsa') || inputLower.includes('aes')) cryptoType = 'keys';
    if (inputLower.includes('secure') || inputLower.includes('security')) cryptoType = 'security';
    if (inputLower.includes('sign') || inputLower.includes('verify')) cryptoType = 'signature';
    
    const cryptoKnowledge = {
        encryption: {
            algorithms: ['AES-256-GCM', 'ChaCha20-Poly1305', 'RSA-OAEP'],
            code: `// AES-256-GCM Encryption Example
const crypto = require('crypto');

function encrypt(text, password) {
    const algorithm = 'aes-256-gcm';
    const salt = crypto.randomBytes(16);
    const key = crypto.pbkdf2Sync(password, salt, 100000, 32, 'sha256');
    const iv = crypto.randomBytes(16);
    
    const cipher = crypto.createCipheriv(algorithm, key, iv);
    let encrypted = cipher.update(text, 'utf8', 'hex');
    encrypted += cipher.final('hex');
    
    const authTag = cipher.getAuthTag();
    
    return {
        encrypted,
        salt: salt.toString('hex'),
        iv: iv.toString('hex'),
        authTag: authTag.toString('hex')
    };
}`,
            best_practices: [
                'Always use authenticated encryption (GCM mode)',
                'Never reuse IVs with the same key',
                'Use PBKDF2 or Argon2 for key derivation',
                'Store salt and IV with encrypted data',
                'Use at least 256-bit keys for AES'
            ]
        },
        decryption: {
            algorithms: ['AES-256-GCM', 'ChaCha20-Poly1305'],
            code: `function decrypt(encrypted, password, salt, iv, authTag) {
    const algorithm = 'aes-256-gcm';
    const saltBuf = Buffer.from(salt, 'hex');
    const key = crypto.pbkdf2Sync(password, saltBuf, 100000, 32, 'sha256');
    const ivBuf = Buffer.from(iv, 'hex');
    const authTagBuf = Buffer.from(authTag, 'hex');
    
    const decipher = crypto.createDecipheriv(algorithm, key, ivBuf);
    decipher.setAuthTag(authTagBuf);
    
    let decrypted = decipher.update(encrypted, 'hex', 'utf8');
    decrypted += decipher.final('utf8');
    
    return decrypted;
}`,
            best_practices: [
                'Verify authentication tags before decrypting',
                'Handle decryption errors gracefully',
                'Never expose plaintext in error messages',
                'Use constant-time comparison for MACs',
                'Clear sensitive data from memory after use'
            ]
        },
        hashing: {
            algorithms: ['SHA-256', 'SHA-512', 'BLAKE2b', 'Argon2'],
            code: `// Secure Password Hashing
const crypto = require('crypto');

function hashPassword(password) {
    const salt = crypto.randomBytes(16);
    const hash = crypto.pbkdf2Sync(password, salt, 100000, 64, 'sha512');
    
    return {
        salt: salt.toString('hex'),
        hash: hash.toString('hex')
    };
}

function verifyPassword(password, salt, hash) {
    const saltBuf = Buffer.from(salt, 'hex');
    const hashBuf = crypto.pbkdf2Sync(password, saltBuf, 100000, 64, 'sha512');
    
    return crypto.timingSafeEqual(hashBuf, Buffer.from(hash, 'hex'));
}`,
            best_practices: [
                'Use SHA-256 or SHA-512 for hashing',
                'Never use MD5 or SHA-1 (broken)',
                'Add salt to prevent rainbow tables',
                'Use many iterations (100,000+) for passwords',
                'Use Argon2 for password hashing if available'
            ]
        },
        keys: {
            algorithms: ['RSA-2048', 'RSA-4096', 'ECDSA', 'Ed25519'],
            code: `// RSA Key Generation
const crypto = require('crypto');

function generateKeyPair() {
    const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', {
        modulusLength: 4096,
        publicKeyEncoding: {
            type: 'spki',
            format: 'pem'
        },
        privateKeyEncoding: {
            type: 'pkcs8',
            format: 'pem',
            cipher: 'aes-256-cbc',
            passphrase: 'your-secure-passphrase'
        }
    });
    
    return { publicKey, privateKey };
}`,
            best_practices: [
                'Use at least 2048-bit RSA keys (4096 preferred)',
                'Store private keys encrypted',
                'Never share private keys',
                'Use separate keys for signing vs encryption',
                'Rotate keys periodically'
            ]
        },
        security: {
            principles: [
                'Defense in depth - multiple layers',
                'Principle of least privilege',
                'Never trust user input',
                'Fail securely (default deny)',
                'Keep secrets out of code'
            ],
            code: `// Secure API Request Example
const crypto = require('crypto');

function makeSecureRequest(data, apiKey) {
    // 1. Validate input
    if (!data || typeof data !== 'object') {
        throw new Error('Invalid input');
    }
    
    // 2. Create HMAC signature
    const timestamp = Date.now();
    const payload = JSON.stringify({ data, timestamp });
    const signature = crypto
        .createHmac('sha256', apiKey)
        .update(payload)
        .digest('hex');
    
    // 3. Send with headers
    return fetch('https://api.example.com/endpoint', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'X-Signature': signature,
            'X-Timestamp': timestamp
        },
        body: payload
    });
}`,
            best_practices: [
                'Always validate and sanitize input',
                'Use HTTPS for all network communication',
                'Implement rate limiting',
                'Log security events',
                'Keep dependencies updated'
            ]
        },
        signature: {
            algorithms: ['RSA-PSS', 'ECDSA', 'Ed25519'],
            code: `// Digital Signature
const crypto = require('crypto');

function signData(data, privateKey) {
    const sign = crypto.createSign('SHA256');
    sign.update(data);
    sign.end();
    
    return sign.sign(privateKey, 'hex');
}

function verifySignature(data, signature, publicKey) {
    const verify = crypto.createVerify('SHA256');
    verify.update(data);
    verify.end();
    
    return verify.verify(publicKey, signature, 'hex');
}`,
            best_practices: [
                'Sign data before sending',
                'Verify signatures before trusting data',
                'Use SHA-256 or better for signing',
                'Keep private signing keys secure',
                'Include timestamps to prevent replay attacks'
            ]
        }
    };
    
    const knowledge = cryptoKnowledge[cryptoType] || cryptoKnowledge.encryption;
    
    let response = `BigDaddyG:Crypto - Security & Encryption Analysis

🔐 Topic: ${cryptoType.toUpperCase()}
🧠 Emotional State: ${emotionalState}
📊 Security Level: ${emotionalState === 'INTENSE' ? 'Maximum' : 'Standard'}

`;
    
    if (knowledge.algorithms) {
        response += `RECOMMENDED ALGORITHMS:\n${knowledge.algorithms.map(a => '• ' + a).join('\n')}\n\n`;
    }
    
    if (knowledge.code) {
        response += `SECURE IMPLEMENTATION:\n\`\`\`\n${knowledge.code}\n\`\`\`\n\n`;
    }
    
    if (knowledge.best_practices) {
        response += `SECURITY BEST PRACTICES:\n${knowledge.best_practices.map(b => '✅ ' + b).join('\n')}\n\n`;
    }
    
    if (knowledge.principles) {
        response += `SECURITY PRINCIPLES:\n${knowledge.principles.map(p => '🔒 ' + p).join('\n')}\n\n`;
    }
    
    // Emotional adjustments
    if (emotionalState === 'OVERWHELMED') {
        response += '\n💡 SIMPLIFIED: Start with the code above, test it thoroughly, then add complexity incrementally.\n';
    } else if (emotionalState === 'INTENSE') {
        response += '\n⚡ URGENT: Focus on the implementation above. Security first, optimization later.\n';
    }
    
    response += `\nFor your specific query "${input}", this approach provides:\n`;
    response += `• Strong security guarantees\n`;
    response += `• Industry-standard algorithms\n`;
    response += `• Protection against common attacks\n`;
    response += `• Clear implementation path\n`;
    
    return response;
};

// BigDaddyG:Latest - General Purpose (Original)
const generateResponse = (input, emotionalState = 'CALM') => {
    const responses = [
        `I understand you're asking about: "${input}". This is a response from the BigDaddyG Assembly model server.`,
        `Assembly language is powerful for low-level programming. Your question "${input}" is noted.`,
        `The BigDaddyG project is designed for efficient assembly programming. Regarding "${input}", here's my response.`,
        `Assembly programming requires precision. Your query "${input}" has been processed by the BigDaddyG model.`
    ];
    
    // Add emotional context
    let emotionalSuffix = '';
    if (emotionalState === 'FOCUSED') {
        emotionalSuffix = '\n\n[FOCUSED MODE]: I\'m providing detailed, concentrated analysis.';
    } else if (emotionalState === 'INTENSE') {
        emotionalSuffix = '\n\n[INTENSE MODE]: Quick, direct response optimized for speed.';
    } else if (emotionalState === 'OVERWHELMED') {
        emotionalSuffix = '\n\n[CALM MODE]: Taking it slow. Let me know if you need clarification.';
    }
    
    return responses[Math.floor(Math.random() * responses.length)] + emotionalSuffix;
};

// Main server handler
const server = http.createServer((req, res) => {
    const parsedUrl = url.parse(req.url, true);
    const path = parsedUrl.pathname;
    const method = req.method;
    
    // Handle CORS preflight
    if (method === 'OPTIONS') {
        res.writeHead(200, corsHeaders);
        res.end();
        return;
    }
    
    // Route handling
    switch (path) {
        case '/health':
        case '/status':
            healthCheck(res);
            break;
            
        case '/v1/chat/completions':
            if (method === 'POST') {
                handleChatCompletion(req, res);
            } else {
                res.writeHead(405, corsHeaders);
                res.end(JSON.stringify({ error: 'Method not allowed' }));
            }
            break;
            
        case '/':
            res.writeHead(200, corsHeaders);
            res.end(JSON.stringify({ 
                message: 'BigDaddyG Assembly Model Server',
                version: '1.0.0',
                endpoints: ['/health', '/v1/chat/completions']
            }));
            break;
            
        default:
            res.writeHead(404, corsHeaders);
            res.end(JSON.stringify({ error: 'Not found' }));
    }
});

// Error handling
server.on('error', (err) => {
    console.error('Server error:', err);
});

// Start server
server.listen(PORT, HOST, () => {
    console.log(`BigDaddyG Assembly Model Server running on http://${HOST}:${PORT}`);
    console.log('Available endpoints:');
    console.log('  GET  /health - Health check');
    console.log('  POST /v1/chat/completions - Chat completions');
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\nShutting down BigDaddyG Assembly Model Server...');
    server.close(() => {
        console.log('Server closed');
        process.exit(0);
    });
});

module.exports = server;

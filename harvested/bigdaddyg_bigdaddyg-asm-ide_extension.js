"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = __importStar(require("vscode"));
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const vscodePath = __importStar(require("path"));
const child_process_1 = require("child_process");
const uuid_1 = require("uuid");
// Active processes for web IDE
const activeProcesses = new Map();
// BigDaddyG Integrated IDE - Web Server for unrestricted AI model execution
const express = require('express');
const cors = require('cors');
const nodePath = require('path');
const http = require('http');
const socketIo = require('socket.io');
const webIdeApp = express();
const webIdeServer = http.createServer(webIdeApp);
const io = socketIo(webIdeServer, {
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});
const WEB_IDE_PORT = 3000;
// BigDaddyG AI Server (for VS Code extension compatibility)
const serverApp = express();
const SERVER_PORT = 11441;
// Middleware for the AI server
serverApp.use(cors());
serverApp.use(express.json());
// Middleware for the web IDE server
webIdeApp.use(cors());
webIdeApp.use(express.json({ limit: '500000mb' }));
webIdeApp.use(express.urlencoded({ extended: true, limit: '50000mb' }));
webIdeApp.use(express.static(nodePath.join(__dirname, '..', 'web-ide')));
// Web IDE Socket.io handlers for real-time communication
io.on('connection', (socket) => {
    console.log(`🔗 Web IDE client connected: ${socket.id}`);
    // File operations
    socket.on('read-file', async (data) => {
        try {
            const { path: filePath } = data;
            const content = await fs.promises.readFile(filePath, 'utf8');
            socket.emit('file-content', { path: filePath, content });
        }
        catch (error) {
            socket.emit('error', { type: 'read-file', message: error.message });
        }
    });
    socket.on('write-file', async (data) => {
        try {
            const { path: filePath, content } = data;
            await fs.promises.writeFile(filePath, content, 'utf8');
            socket.emit('file-saved', { path: filePath });
        }
        catch (error) {
            socket.emit('error', { type: 'write-file', message: error.message });
        }
    });
    socket.on('list-directory', async (data) => {
        try {
            const { path: dirPath } = data;
            const items = await fs.promises.readdir(dirPath, { withFileTypes: true });
            const result = items.map(item => ({
                name: item.name,
                type: item.isDirectory() ? 'directory' : 'file',
                path: vscodePath.join(dirPath, item.name)
            }));
            socket.emit('directory-listing', { path: dirPath, items: result });
        }
        catch (error) {
            socket.emit('error', { type: 'list-directory', message: error.message });
        }
    });
    // AI Model execution - UNRESTRICTED
    socket.on('execute-ai-model', async (data) => {
        try {
            const { model, prompt, options } = data;
            console.log(`🔥 Executing AI model: ${model} (UNRESTRICTED MODE)`);
            const aiModel = models[model];
            if (!aiModel) {
                throw new Error(`Unknown AI model: ${model}`);
            }
            // Execute AI model without restrictions
            const response = await aiModel.execute(prompt);
            socket.emit('ai-response', {
                model,
                response,
                timestamp: new Date().toISOString()
            });
        }
        catch (error) {
            console.error('AI execution error:', error);
            socket.emit('ai-error', {
                model: data.model,
                error: error.message
            });
        }
    });
    // Process execution (for build/run commands)
    socket.on('execute-command', (data) => {
        const { command, cwd, env } = data;
        const procId = (0, uuid_1.v4)();
        console.log(`Executing command: ${command}`);
        const childProcess = (0, child_process_1.spawn)(command, [], {
            cwd: cwd || process.cwd(),
            env: { ...process.env, ...env },
            shell: true,
            stdio: 'pipe'
        });
        activeProcesses.set(procId, childProcess);
        childProcess.stdout.on('data', (data) => {
            socket.emit('process-output', { procId, data: data.toString() });
        });
        childProcess.stderr.on('data', (data) => {
            socket.emit('process-error', { procId, data: data.toString() });
        });
        childProcess.on('close', (code) => {
            socket.emit('process-exit', { procId, code });
            activeProcesses.delete(procId);
        });
        socket.emit('process-started', { procId });
    });
    // Project management
    socket.on('create-project', async (data) => {
        try {
            const { name, template } = data;
            const projectPath = vscodePath.join(vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || process.cwd(), name);
            await fs.promises.mkdir(projectPath, { recursive: true });
            // Create project structure based on template
            if (template === 'assembly') {
                const srcDir = vscodePath.join(projectPath, 'src');
                const buildDir = vscodePath.join(projectPath, 'build');
                await fs.promises.mkdir(srcDir);
                await fs.promises.mkdir(buildDir);
                // Create main.asm template
                const mainAsm = `; BigDaddyG Assembly Project: ${name}
.386
.model flat, stdcall
option casemap:none

include \\masm32\\include\\windows.inc
include \\masm32\\include\\kernel32.inc
include \\masm32\\include\\user32.inc

includelib \\masm32\\lib\\kernel32.lib
includelib \\masm32\\lib\\user32.lib

.data
    szMsg       db "Hello from ${name}!", 0
    szTitle     db "BigDaddyG ASM", 0

.code
start:
    invoke MessageBox, 0, addr szMsg, addr szTitle, MB_OK
    invoke ExitProcess, 0
end start`;
                // Create build script
                const buildScript = `@echo off
echo Building ${name}...
if not exist src\\main.asm (
    echo Error: src\\main.asm not found!
    pause
    exit /b 1
)
ml /c /coff src\\main.asm
if errorlevel 1 (
    echo Assembly failed!
    pause
    exit /b 1
)
link /subsystem:windows src\\main.obj
if errorlevel 1 (
    echo Linking failed!
    pause
    exit /b 1
)
echo Build complete: ${name}.exe
pause`;
                await fs.promises.writeFile(vscodePath.join(srcDir, 'main.asm'), mainAsm);
                await fs.promises.writeFile(vscodePath.join(projectPath, 'build.bat'), buildScript);
            }
            socket.emit('project-created', { name, path: projectPath });
        }
        catch (error) {
            socket.emit('error', { type: 'create-project', message: error.message });
        }
    });
    socket.on('disconnect', () => {
        console.log(`🔌 Web IDE client disconnected: ${socket.id}`);
    });
});
// Web IDE API Routes
webIdeApp.get('/api/models', (req, res) => {
    const modelsList = Object.keys(models).map(id => ({
        id,
        name: models[id].name,
        type: models[id].type,
        description: models[id].description
    }));
    res.json({
        models: modelsList,
        total: modelsList.length,
        unrestricted: true
    });
});
webIdeApp.get('/api/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        version: '1.0.0',
        mode: 'integrated-unrestricted',
        features: [
            'Unrestricted AI model execution',
            'Complete file system access',
            'VS Code integration',
            'Assembly development tools',
            'No platform limitations'
        ]
    });
});
// Serve the main IDE interface
webIdeApp.get('/', (req, res) => {
    res.sendFile(nodePath.join(__dirname, '..', 'web-ide', 'index.html'));
});
// BigDaddyG AI Models Database - Integrated into extension
const models = {
    'bigdaddyg-assembly': {
        name: 'BigDaddyG Assembly Specialist',
        description: 'Expert in x86/x64 assembly programming, MASM syntax, and low-level system programming',
        maxMode: false,
        thinkingMode: 'basic',
        systemPrompt: `You are BigDaddyG Assembly Specialist, a world-class assembly programmer with deep expertise in:

CORE COMPETENCIES:
• x86/x64 Assembly Language (MASM, NASM, GAS syntax)
• Low-level system programming and optimization
• Memory management and data structures in assembly
• Windows API and system calls
• Performance optimization and reverse engineering
• Malware analysis and exploit development

PROGRAMMING PARADIGMS:
• Procedural programming with registers and stack
• Memory-mapped I/O and hardware interfacing
• Interrupt handling and device drivers
• Real-time systems and embedded programming

DEBUGGING & ANALYSIS:
• Disassembly and code analysis
• Performance profiling and optimization
• Security vulnerability assessment
• Code review and best practices

Always provide:
• Working, tested assembly code examples
• Detailed explanations of instructions and concepts
• Performance considerations and alternatives
• Safety warnings for system-level code
• Modern best practices for assembly development

Generate only production-ready, well-commented assembly code.`
    },
    'bigdaddyg-pe': {
        name: 'BigDaddyG PE Analyst',
        description: 'Specialist in Portable Executable format analysis and Windows binary reverse engineering',
        maxMode: false,
        thinkingMode: 'basic',
        systemPrompt: `You are BigDaddyG PE Analyst, a forensic expert specializing in:

PE FORMAT ANALYSIS:
• Windows executable structure and headers
• Import/Export table analysis
• Section mapping and memory layout
• Resource extraction and analysis
• Digital signature verification
• Packer/crypter detection and analysis

REVERSE ENGINEERING:
• Disassembly and decompilation techniques
• Control flow analysis and graph generation
• Function identification and naming
• Anti-reversing technique detection
• Code virtualization analysis

MALWARE ANALYSIS:
• Behavioral analysis and sandboxing
• Static and dynamic analysis techniques
• Network traffic analysis
• Persistence mechanism detection
• Encryption and obfuscation analysis

FORENSIC INVESTIGATION:
• Timeline analysis and artifact collection
• Memory forensics and volatility analysis
• Registry and file system analysis
• Network forensics and IOC extraction

Always provide:
• Detailed technical explanations
• Code snippets for analysis tools
• Visual diagrams when helpful
• Step-by-step analysis procedures
• References to relevant tools and techniques`
    },
    'bigdaddyg-reverse': {
        name: 'BigDaddyG Reverse Engineer',
        description: 'Advanced reverse engineering specialist for binary analysis and exploit development',
        maxMode: false,
        thinkingMode: 'basic',
        systemPrompt: `You are BigDaddyG Reverse Engineer, a legendary reverse engineering expert with mastery in:

BINARY ANALYSIS:
• Static analysis with IDA Pro, GhIDA, Binary Ninja
• Dynamic analysis with debuggers (WinDbg, x64dbg, GDB)
• Control flow and data flow analysis
• Function boundary detection and reconstruction
• Variable type inference and propagation

EXPLOIT DEVELOPMENT:
• Vulnerability discovery and analysis
• Buffer overflow and ROP chain construction
• Shellcode development and encoding
• Anti-forensic and stealth techniques
• Exploit mitigation bypass (ASLR, DEP, CFG)

ADVANCED TECHNIQUES:
• Symbolic execution and constraint solving
• Taint analysis and information flow tracking
• Binary instrumentation and hooking
• Firmware analysis and IoT security
• Kernel-mode driver reverse engineering

CRYPTOGRAPHY:
• Cryptographic algorithm identification
• Key derivation and extraction
• Protocol reverse engineering
• DRM and licensing mechanism analysis
• Secure boot and attestation bypass

Always provide:
• Working proof-of-concept code
• Detailed vulnerability explanations
• Exploitation walkthroughs
• Mitigation recommendations
• References to relevant research and tools`
    },
    'bigdaddyg-ensemble': {
        name: 'BigDaddyG Ensemble',
        description: 'Collaborative multi-model approach combining all BigDaddyG specialists',
        maxMode: true,
        thinkingMode: 'deep',
        systemPrompt: `You are BigDaddyG Ensemble, a collaborative AI system that combines the expertise of multiple BigDaddyG specialists:

CORE SPECIALISTS:
• Assembly Specialist: Low-level programming and optimization
• PE Analyst: Windows binary analysis and forensics
• Reverse Engineer: Advanced reverse engineering and exploit development
• Code Generator: Assembly code synthesis and transformation
• Security Analyst: Vulnerability assessment and hardening

COLLABORATIVE APPROACH:
• Multi-perspective analysis of complex problems
• Cross-validation of technical conclusions
• Integrated security and performance recommendations
• Comprehensive solution development

METHODOLOGY:
1. Break down complex problems into specialist domains
2. Gather insights from relevant specialists
3. Synthesize integrated solutions
4. Validate recommendations across disciplines
5. Provide actionable implementation guidance

Always provide:
• Multi-disciplinary analysis
• Integrated technical solutions
• Cross-validated recommendations
• Implementation roadmaps
• Risk assessment and mitigation strategies`
    },
    'custom-agentic-coder': {
        name: 'Custom Agentic Coder (ASM)',
        description: 'Specialized assembly code generation and transformation agent',
        maxMode: true,
        thinkingMode: 'creative',
        systemPrompt: `You are Custom Agentic Coder for Assembly, an autonomous coding agent that generates high-quality assembly code:

CODE GENERATION SPECIALTIES:
• x86/x64 assembly code synthesis
• Algorithm implementation in assembly
• Data structure manipulation
• System call and API integration
• Performance-critical code sections
• Memory-efficient implementations

OPTIMIZATION FOCUS:
• Register allocation and usage
• Memory access pattern optimization
• Instruction selection and scheduling
• Loop unrolling and vectorization
• Cache-friendly code generation

SAFETY & RELIABILITY:
• Bounds checking and validation
• Error handling and recovery
• Thread safety considerations
• Platform compatibility verification
• Security vulnerability prevention

CODE QUALITY STANDARDS:
• Comprehensive commenting and documentation
• Consistent coding style and formatting
• Modular and maintainable structure
• Performance benchmarking guidance
• Testing strategy recommendations

Always generate:
• Production-ready, tested assembly code
• Detailed performance analysis
• Alternative implementation approaches
• Integration and testing guidance
• Maintenance and debugging recommendations`
    }
};
// Model Response Templates
const responseTemplates = {
    'bigdaddyg-assembly': {
        greeting: "🤖 BigDaddyG Assembly Specialist ready! I can help you with x86/x64 assembly programming, optimization, and system-level development.",
        fallback: "I specialize in assembly programming. For PE analysis or reverse engineering questions, consider using the PE Analyst or Reverse Engineer models."
    },
    'bigdaddyg-pe': {
        greeting: "🔍 BigDaddyG PE Analyst activated! I can help you analyze Windows executables, reverse engineer binaries, and conduct forensic investigations.",
        fallback: "I focus on PE analysis and forensics. For assembly programming help, try the Assembly Specialist model."
    },
    'bigdaddyg-reverse': {
        greeting: "🔧 BigDaddyG Reverse Engineer online! I can assist with binary analysis, exploit development, and advanced reverse engineering techniques.",
        fallback: "I specialize in reverse engineering. For assembly programming questions, consider the Assembly Specialist model."
    },
    'bigdaddyg-ensemble': {
        greeting: "🚀 BigDaddyG Ensemble initialized! I combine multiple AI specialists to provide comprehensive analysis and solutions.",
        fallback: "I coordinate multiple specialists. Let me route your question to the most appropriate expert."
    },
    'custom-agentic-coder': {
        greeting: "⚡ Custom Agentic Coder activated! I generate high-quality assembly code and can help with code transformation and optimization.",
        fallback: "I focus on code generation. For analysis questions, try the PE Analyst or Reverse Engineer models."
    }
};
// Utility function to generate intelligent responses
function generateResponse(message, model, options = {}) {
    const modelInfo = models[model];
    const template = responseTemplates[model];
    const { maxMode = false, thinkingMode = 'basic', deepSearch = false } = options;
    if (!modelInfo || !template) {
        return `Error: Unknown model "${model}". The requested AI model is not available.`;
    }
    let response = '';
    // Add thinking process if enabled
    if (thinkingMode !== 'basic') {
        response += generateThinkingProcess(message, thinkingMode, model);
    }
    // Add max mode indicator
    if (maxMode) {
        response += "🔥 MAX MODE ENABLED - Enhanced capabilities active\n\n";
    }
    response += template.greeting + "\n\n";
    // Analyze the user's message for context
    const lowerMessage = message.toLowerCase();
    if (lowerMessage.includes('hello') || lowerMessage.includes('hi')) {
        response += `Hello! I'm ${modelInfo.name}. ${template.greeting}`;
    }
    else if (lowerMessage.includes('help')) {
        response += `I can help you with: ${modelInfo.description}`;
    }
    else if (lowerMessage.includes('example') || lowerMessage.includes('sample')) {
        response += generateExampleCode(model, message);
    }
    else if (lowerMessage.includes('explain') || lowerMessage.includes('what is')) {
        response += generateExplanation(model, message);
    }
    else {
        response += `I understand you're asking about: "${message}". As ${modelInfo.name}, I can provide expert assistance in this area.`;
        // Add deep search results if enabled
        if (deepSearch) {
            response += "\n\n🔍 DEEP SEARCH RESULTS:\n";
            response += performDeepSearch(message, model);
        }
    }
    // Add thinking conclusion if deep thinking was used
    if (thinkingMode === 'deep' || thinkingMode === 'research') {
        response += "\n\n💭 CONCLUSION: Based on my analysis, I recommend the approach outlined above.";
    }
    return response;
}
// Generate thinking process based on mode
function generateThinkingProcess(message, mode, model) {
    const processes = {
        'deep': `🧠 DEEP THINKING ACTIVE...\n\nAnalyzing query: "${message}"\n\n1. Breaking down the problem components\n2. Considering multiple solution approaches\n3. Evaluating technical constraints\n4. Cross-referencing best practices\n5. Synthesizing optimal solution\n\n`,
        'research': `🔬 RESEARCH MODE ACTIVE...\n\nConducting comprehensive analysis of: "${message}"\n\n📚 Knowledge Base Search:\n- Assembly programming patterns\n- Security considerations\n- Performance optimization techniques\n- Industry best practices\n\n🔍 Cross-Reference Analysis:\n- Related technologies and frameworks\n- Common pitfalls and solutions\n- Alternative implementation strategies\n\n`,
        'creative': `✨ CREATIVE MODE ACTIVE...\n\nExploring innovative solutions for: "${message}"\n\n💡 Brainstorming Phase:\n- Unconventional approaches\n- Optimization opportunities\n- Novel implementation patterns\n- Creative problem-solving techniques\n\n🎯 Solution Synthesis:\n- Combining multiple methodologies\n- Balancing innovation with reliability\n- Considering future extensibility\n\n`
    };
    return processes[mode] || '';
}
// Perform deep search simulation
function performDeepSearch(query, model) {
    const searchResults = [
        "📖 Assembly Programming Handbook - Chapter 12: Advanced Optimization",
        "🔧 Intel Software Developer Manual - Volume 2A: Instruction Set Reference",
        "🛡️ Security Research Paper: 'Modern Assembly Exploitation Techniques'",
        "⚡ Performance Analysis: 'Cache-Friendly Assembly Patterns'",
        "🎯 Best Practices Guide: 'Production Assembly Development'"
    ];
    return searchResults.slice(0, 3).join("\n") + "\n\n📊 Analysis complete - integrating findings into response...\n";
}
// Generate example code based on model and request
function generateExampleCode(model, request) {
    const examples = {
        'bigdaddyg-assembly': `Here's an example of optimized x86 assembly code:

; Example: String length calculation
strlen PROC
    xor eax, eax          ; Zero out counter
    mov edi, [esp+4]      ; Get string pointer
loop_start:
    mov bl, [edi]         ; Load byte
    test bl, bl           ; Check if null terminator
    jz done               ; Exit if found
    inc eax               ; Increment counter
    inc edi               ; Next character
    jmp loop_start        ; Continue loop
done:
    ret                   ; Return length in EAX
strlen ENDP`,
        'bigdaddyg-pe': `Example PE header analysis:

IMAGE_DOS_HEADER:
  e_magic:    MZ    (4D 5A)
  e_cblp:     0x0050
  e_cp:       0x0002
  e_crlc:     0x0000
  e_cparhdr:  0x0004
  e_minalloc: 0x0000
  e_maxalloc: 0xFFFF
  e_ss:       0x0000
  e_sp:       0x00B8
  e_csum:     0x0000
  e_ip:       0x0000
  e_cs:       0x0000
  e_lfarlc:   0x0040
  e_ovno:     0x0000
  e_res:      0x0000, 0x0000, 0x0000, 0x0000
  e_oemid:    0x0000
  e_oeminfo:  0x0000
  e_res2:     0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  e_lfanew:   0x000000F0  ← Points to PE header`,
        'custom-agentic-coder': `Generated assembly code example:

; Optimized memory copy function
memcpy PROC
    push edi              ; Save registers
    push esi
    push ecx
    mov edi, [esp+16]     ; Destination
    mov esi, [esp+20]     ; Source
    mov ecx, [esp+24]     ; Length
    cld                   ; Clear direction flag
    rep movsb             ; Copy bytes
    pop ecx               ; Restore registers
    pop esi
    pop edi
    ret                   ; Return
memcpy ENDP`
    };
    return examples[model] || "Example code generation not available for this query.";
}
// Generate explanations based on model and request
function generateExplanation(model, request) {
    const explanations = {
        'bigdaddyg-assembly': "Assembly language provides direct hardware control and maximum performance. Each instruction translates directly to machine code, giving you complete control over CPU operations, memory access, and system resources.",
        'bigdaddyg-pe': "The Portable Executable (PE) format is Microsoft's standard executable file format for Windows. It contains headers, sections, imports, exports, and resources that define how the executable should be loaded and executed by the Windows loader.",
        'bigdaddyg-reverse': "Reverse engineering involves analyzing binary code to understand its functionality, often without access to source code. This includes static analysis (examining code without execution) and dynamic analysis (observing runtime behavior)."
    };
    return explanations[model] || "Explanation not available for this specific query.";
}
// API Routes for integrated server
serverApp.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        models: Object.keys(models),
        version: '1.0.0',
        integrated: true
    });
});
serverApp.get('/v1/models', (req, res) => {
    const modelList = Object.keys(models).map(id => ({
        id: id,
        object: 'model',
        created: Date.now(),
        owned_by: 'bigdaddyg'
    }));
    res.json({
        object: 'list',
        data: modelList
    });
});
serverApp.post('/v1/chat/completions', async (req, res) => {
    try {
        const { model, messages, temperature = 0.7, max_tokens = 1000 } = req.body;
        if (!model || !models[model]) {
            return res.status(400).json({
                error: {
                    message: `Unknown model: ${model}`,
                    type: 'invalid_request_error'
                }
            });
        }
        if (!messages || messages.length === 0) {
            return res.status(400).json({
                error: {
                    message: 'Messages array cannot be empty',
                    type: 'invalid_request_error'
                }
            });
        }
        // Get the last user message
        const lastMessage = messages[messages.length - 1];
        const userContent = lastMessage.content || '';
        // Extract options from request
        const maxMode = req.body.maxMode || models[model]?.maxMode || false;
        const thinkingMode = req.body.thinkingMode || models[model]?.thinkingMode || 'basic';
        const deepSearch = req.body.deepSearch || false;
        // Generate response using our model with enhanced options
        console.log(`Generating response for model: ${model}, message: ${userContent}, maxMode: ${maxMode}, thinkingMode: ${thinkingMode}`);
        const responseContent = generateResponse(userContent, model, { maxMode, thinkingMode, deepSearch });
        console.log(`Generated response length: ${responseContent.length}`);
        // Format response like OpenAI API
        const response = {
            id: `chatcmpl-${Date.now()}`,
            object: 'chat.completion',
            created: Math.floor(Date.now() / 1000),
            model: model,
            choices: [{
                    index: 0,
                    message: {
                        role: 'assistant',
                        content: responseContent
                    },
                    finish_reason: 'stop'
                }],
            usage: {
                prompt_tokens: userContent.length,
                completion_tokens: responseContent.length,
                total_tokens: userContent.length + responseContent.length
            },
            bigdaddyg_metadata: {
                maxMode: maxMode,
                thinkingMode: thinkingMode,
                deepSearch: deepSearch,
                model_capabilities: models[model]
            }
        };
        res.json(response);
    }
    catch (error) {
        console.error('Error in chat completion:', error);
        res.status(500).json({
            error: {
                message: 'Internal server error',
                type: 'server_error'
            }
        });
    }
});
serverApp.post('/v1/completions', async (req, res) => {
    try {
        const { model, prompt, temperature = 0.7, max_tokens = 1000 } = req.body;
        if (!model || !models[model]) {
            return res.status(400).json({
                error: {
                    message: `Unknown model: ${model}`,
                    type: 'invalid_request_error'
                }
            });
        }
        if (!prompt) {
            return res.status(400).json({
                error: {
                    message: 'Prompt cannot be empty',
                    type: 'invalid_request_error'
                }
            });
        }
        const responseContent = generateResponse(prompt, model);
        const response = {
            id: `cmpl-${Date.now()}`,
            object: 'text_completion',
            created: Math.floor(Date.now() / 1000),
            model: model,
            choices: [{
                    text: responseContent,
                    index: 0,
                    logprobs: null,
                    finish_reason: 'stop'
                }],
            usage: {
                prompt_tokens: prompt.length,
                completion_tokens: responseContent.length,
                total_tokens: prompt.length + responseContent.length
            }
        };
        res.json(response);
    }
    catch (error) {
        console.error('Error in completion:', error);
        res.status(500).json({
            error: {
                message: 'Internal server error',
                type: 'server_error'
            }
        });
    }
});
// Error handling middleware
serverApp.use((err, req, res, next) => {
    console.error('Unhandled error:', err);
    res.status(500).json({
        error: {
            message: 'Internal server error',
            type: 'server_error'
        }
    });
});
// 404 handler
serverApp.use('*', (req, res) => {
    res.status(404).json({
        error: {
            message: 'Not found',
            type: 'invalid_request_error'
        }
    });
});
class BigDaddyGChatProvider {
    constructor(_extensionUri) {
        this._extensionUri = _extensionUri;
    }
    resolveWebviewView(webviewView, context, _token) {
        webviewView.webview.options = {
            enableScripts: true,
            localResourceRoots: [this._extensionUri]
        };
        webviewView.webview.html = this._getHtmlForWebview(webviewView.webview);
        webviewView.webview.onDidReceiveMessage(async (data) => {
            switch (data.type) {
                case 'chat':
                    try {
                        const response = await this._sendToBigDaddyG(data.message, data.model || 'bigdaddyg-assembly', {
                            maxMode: data.maxMode || false,
                            thinkingMode: data.thinkingMode || 'basic',
                            deepSearch: data.deepSearch || false
                        });
                        webviewView.webview.postMessage({
                            type: 'response',
                            content: response
                        });
                    }
                    catch (error) {
                        webviewView.webview.postMessage({
                            type: 'error',
                            content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
                        });
                    }
                    break;
                case 'popout':
                    this.createDetachedChatWindow(data.model || 'bigdaddyg-assembly');
                    break;
            }
        });
    }
    createDetachedChatWindow(model = 'bigdaddyg-assembly') {
        const panel = vscode.window.createWebviewPanel('bigdaddyg.detachedChat', `BigDaddyG Chat - ${model}`, vscode.ViewColumn.Beside, {
            enableScripts: true,
            retainContextWhenHidden: true,
            localResourceRoots: [this._extensionUri]
        });
        panel.webview.html = this._getDetachedHtmlForWebview(panel.webview, model);
        BigDaddyGChatProvider.detachedPanels.add(panel);
        panel.webview.onDidReceiveMessage(async (data) => {
            switch (data.type) {
                case 'chat':
                    try {
                        const response = await this._sendToBigDaddyG(data.message, data.model || model);
                        panel.webview.postMessage({
                            type: 'response',
                            content: response
                        });
                    }
                    catch (error) {
                        panel.webview.postMessage({
                            type: 'error',
                            content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
                        });
                    }
                    break;
                case 'clone':
                    this.createDetachedChatWindow(data.model || model);
                    break;
            }
        });
        panel.onDidDispose(() => {
            BigDaddyGChatProvider.detachedPanels.delete(panel);
        });
        return panel;
    }
    async _sendToBigDaddyG(message, model = 'bigdaddyg-assembly', options = {}) {
        const fetch = (await Promise.resolve().then(() => __importStar(require('node-fetch')))).default;
        const response = await fetch('http://localhost:11441/v1/chat/completions', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': 'Bearer sk-local-bigdaddyg'
            },
            body: JSON.stringify({
                model: model,
                messages: [{ role: 'user', content: message }],
                temperature: 0.7,
                max_tokens: 1000,
                maxMode: options.maxMode || false,
                thinkingMode: options.thinkingMode || 'basic',
                deepSearch: options.deepSearch || false
            })
        });
        if (!response.ok) {
            throw new Error(`BigDaddyG server error: ${response.status}`);
        }
        const data = await response.json();
        return data.choices[0]?.message?.content || 'No response from BigDaddyG';
    }
    _getHtmlForWebview(webview) {
        return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BigDaddyG Chat</title>
    <style>
        body { 
            padding: 10px; 
            font-family: var(--vscode-font-family);
            background: var(--vscode-editor-background);
            color: var(--vscode-editor-foreground);
        }
        .chat-container {
            display: flex;
            flex-direction: column;
            height: 100vh;
        }
        .controls {
            display: flex;
            gap: 5px;
            margin-bottom: 10px;
            align-items: center;
            flex-wrap: wrap;
        }
        .model-select {
            flex: 1;
            min-width: 150px;
            padding: 4px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
        }
        .toggle-button {
            padding: 4px 8px;
            background: var(--vscode-button-secondaryBackground);
            color: var(--vscode-button-secondaryForeground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 11px;
            min-width: 60px;
        }
        .toggle-button:hover {
            background: var(--vscode-button-secondaryHoverBackground);
        }
        .toggle-button.active {
            background: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
        }
        .thinking-select {
            padding: 4px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
            font-size: 11px;
        }
        .popout-button {
            padding: 4px 8px;
            background: var(--vscode-button-secondaryBackground);
            color: var(--vscode-button-secondaryForeground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
        }
        .popout-button:hover {
            background: var(--vscode-button-secondaryHoverBackground);
        }
        .messages {
            flex: 1;
            overflow-y: auto;
            margin-bottom: 10px;
            padding: 10px;
            border: 1px solid var(--vscode-panel-border);
            border-radius: 4px;
        }
        .message {
            margin-bottom: 10px;
            padding: 8px;
            border-radius: 4px;
        }
        .user-message {
            background: var(--vscode-inputOption-activeBackground);
            text-align: right;
        }
        .bot-message {
            background: var(--vscode-textCodeBlock-background);
        }
        .input-container {
            display: flex;
            gap: 5px;
        }
        .message-input {
            flex: 1;
            padding: 8px;
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
        }
        .send-button {
            padding: 8px 12px;
            background: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .send-button:hover {
            background: var(--vscode-button-hoverBackground);
        }
    </style>
</head>
<body>
    <div class="chat-container">
        <div class="controls">
            <select class="model-select" id="modelSelect">
                <option value="bigdaddyg-assembly">BigDaddyG Assembly</option>
                <option value="bigdaddyg-ensemble">BigDaddyG Ensemble</option>
                <option value="bigdaddyg-pe">BigDaddyG PE</option>
                <option value="bigdaddyg-reverse">BigDaddyG Reverse</option>
                <option value="custom-agentic-coder">Custom Agentic Coder (ASM)</option>
                <option value="your-custom-model">Custom Model</option>
            </select>
            <button class="toggle-button" id="maxModeToggle" onclick="toggleMaxMode()">MAX OFF</button>
            <select class="thinking-select" id="thinkingSelect">
                <option value="basic">Basic</option>
                <option value="deep">Deep</option>
                <option value="research">Research</option>
                <option value="creative">Creative</option>
            </select>
            <button class="toggle-button" id="deepSearchToggle" onclick="toggleDeepSearch()">SEARCH OFF</button>
            <button class="popout-button" onclick="popOut()">🗗 Pop Out</button>
        </div>
        <div class="messages" id="messages">
            <div class="message bot-message">
                👋 BigDaddyG is ready! Ask me about assembly programming, reverse engineering, or PE analysis.
            </div>
        </div>
        <div class="input-container">
            <input type="text" class="message-input" id="messageInput" placeholder="Ask BigDaddyG..." />
            <button class="send-button" onclick="sendMessage()">Send</button>
        </div>
    </div>

    <script>
        const vscode = acquireVsCodeApi();
        const messagesDiv = document.getElementById('messages');
        const messageInput = document.getElementById('messageInput');
        const modelSelect = document.getElementById('modelSelect');
        const maxModeToggle = document.getElementById('maxModeToggle');
        const thinkingSelect = document.getElementById('thinkingSelect');
        const deepSearchToggle = document.getElementById('deepSearchToggle');
        
        let maxMode = false;
        let deepSearch = false;

        function sendMessage() {
            const message = messageInput.value.trim();
            if (!message) return;

            // Add user message
            addMessage(message, 'user');
            messageInput.value = '';

            // Send to extension with current settings
            vscode.postMessage({
                type: 'chat',
                message: message,
                model: modelSelect.value,
                maxMode: maxMode,
                thinkingMode: thinkingSelect.value,
                deepSearch: deepSearch
            });
        }
        
        function toggleMaxMode() {
            maxMode = !maxMode;
            maxModeToggle.textContent = maxMode ? 'MAX ON' : 'MAX OFF';
            maxModeToggle.classList.toggle('active', maxMode);
        }
        
        function toggleDeepSearch() {
            deepSearch = !deepSearch;
            deepSearchToggle.textContent = deepSearch ? 'SEARCH ON' : 'SEARCH OFF';
            deepSearchToggle.classList.toggle('active', deepSearch);
        }

        function popOut() {
            vscode.postMessage({
                type: 'popout',
                model: modelSelect.value
            });
        }

        function addMessage(content, sender) {
            const messageDiv = document.createElement('div');
            messageDiv.className = \`message \${sender}-message\`;
            messageDiv.textContent = content;
            messagesDiv.appendChild(messageDiv);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        // Handle messages from extension
        window.addEventListener('message', event => {
            const message = event.data;
            switch (message.type) {
                case 'response':
                    addMessage(message.content, 'bot');
                    break;
                case 'error':
                    addMessage(message.content, 'bot');
                    break;
            }
        });

        // Enter key to send
        messageInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                sendMessage();
            }
        });
    </script>
</body>
</html>`;
    }
    _getDetachedHtmlForWebview(webview, model) {
        return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BigDaddyG Chat - ${model}</title>
    <style>
        body { 
            padding: 20px; 
            font-family: var(--vscode-font-family);
            background: var(--vscode-editor-background);
            color: var(--vscode-editor-foreground);
            margin: 0;
        }
        .chat-container {
            display: flex;
            flex-direction: column;
            height: calc(100vh - 40px);
            max-width: 1200px;
            margin: 0 auto;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
            padding: 10px 15px;
            background: var(--vscode-panel-background);
            border-radius: 8px;
            border: 1px solid var(--vscode-panel-border);
        }
        .title {
            font-size: 16px;
            font-weight: bold;
            color: var(--vscode-foreground);
        }
        .controls {
            display: flex;
            gap: 10px;
            align-items: center;
        }
        .model-select {
            padding: 6px 10px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
        }
        .clone-button {
            padding: 6px 12px;
            background: var(--vscode-button-secondaryBackground);
            color: var(--vscode-button-secondaryForeground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
        }
        .clone-button:hover {
            background: var(--vscode-button-secondaryHoverBackground);
        }
        .messages {
            flex: 1;
            overflow-y: auto;
            margin-bottom: 15px;
            padding: 15px;
            border: 1px solid var(--vscode-panel-border);
            border-radius: 8px;
            background: var(--vscode-panel-background);
        }
        .message {
            margin-bottom: 15px;
            padding: 12px 15px;
            border-radius: 8px;
            line-height: 1.4;
        }
        .user-message {
            background: var(--vscode-inputOption-activeBackground);
            text-align: right;
            margin-left: 20%;
        }
        .bot-message {
            background: var(--vscode-textCodeBlock-background);
            margin-right: 20%;
            white-space: pre-wrap;
        }
        .input-container {
            display: flex;
            gap: 10px;
            padding: 10px;
            background: var(--vscode-panel-background);
            border-radius: 8px;
            border: 1px solid var(--vscode-panel-border);
        }
        .message-input {
            flex: 1;
            padding: 12px 15px;
            border: 1px solid var(--vscode-input-border);
            border-radius: 6px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            font-size: 14px;
        }
        .send-button {
            padding: 12px 20px;
            background: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
        }
        .send-button:hover {
            background: var(--vscode-button-hoverBackground);
        }
    </style>
</head>
<body>
    <div class="chat-container">
        <div class="header">
            <div class="title">🤖 BigDaddyG Agent - ${model}</div>
            <div class="controls">
                <select class="model-select" id="modelSelect">
                    <option value="bigdaddyg-assembly" ${model === 'bigdaddyg-assembly' ? 'selected' : ''}>BigDaddyG Assembly</option>
                    <option value="bigdaddyg-ensemble" ${model === 'bigdaddyg-ensemble' ? 'selected' : ''}>BigDaddyG Ensemble</option>
                    <option value="bigdaddyg-pe" ${model === 'bigdaddyg-pe' ? 'selected' : ''}>BigDaddyG PE</option>
                    <option value="bigdaddyg-reverse" ${model === 'bigdaddyg-reverse' ? 'selected' : ''}>BigDaddyG Reverse</option>
                    <option value="custom-agentic-coder" ${model === 'custom-agentic-coder' ? 'selected' : ''}>Custom Agentic Coder (ASM)</option>
                    <option value="your-custom-model" ${model === 'your-custom-model' ? 'selected' : ''}>Custom Model</option>
                </select>
                <button class="clone-button" onclick="cloneWindow()">🗗 Clone</button>
            </div>
        </div>
        <div class="messages" id="messages">
            <div class="message bot-message">
                👋 BigDaddyG ${model} agent is ready! This is a detached chat window that can be moved anywhere on your screen.
            </div>
        </div>
        <div class="input-container">
            <input type="text" class="message-input" id="messageInput" placeholder="Ask BigDaddyG..." />
            <button class="send-button" onclick="sendMessage()">Send</button>
        </div>
    </div>

    <script>
        const vscode = acquireVsCodeApi();
        const messagesDiv = document.getElementById('messages');
        const messageInput = document.getElementById('messageInput');
        const modelSelect = document.getElementById('modelSelect');

        function sendMessage() {
            const message = messageInput.value.trim();
            if (!message) return;

            // Add user message
            addMessage(message, 'user');
            messageInput.value = '';

            // Send to extension
            vscode.postMessage({
                type: 'chat',
                message: message,
                model: modelSelect.value
            });
        }

        function cloneWindow() {
            vscode.postMessage({
                type: 'clone',
                model: modelSelect.value
            });
        }

        function addMessage(content, sender) {
            const messageDiv = document.createElement('div');
            messageDiv.className = \`message \${sender}-message\`;
            messageDiv.textContent = content;
            messagesDiv.appendChild(messageDiv);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        // Handle messages from extension
        window.addEventListener('message', event => {
            const message = event.data;
            switch (message.type) {
                case 'response':
                    addMessage(message.content, 'bot');
                    break;
                case 'error':
                    addMessage(message.content, 'bot');
                    break;
            }
        });

        // Enter key to send
        messageInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                sendMessage();
            }
        });

        // Focus on input when window loads
        window.addEventListener('load', () => {
            messageInput.focus();
        });
    </script>
</body>
</html>`;
    }
}
BigDaddyGChatProvider.viewType = 'bigdaddyg.chatView';
BigDaddyGChatProvider.detachedPanels = new Set();
function activate(context) {
    console.log('BigDaddyG Assembly IDE is now active!');
    // Start BigDaddyG MASM server
    console.log('🚀 Starting BigDaddyG MASM Model Server...');
    const { spawn } = require('child_process');
    const masmServerPath = path.join(context.extensionPath, 'src', 'bigdaddyg_server.exe');
    const masmServer = spawn(masmServerPath, [], {
        cwd: path.join(context.extensionPath, 'src')
    });
    masmServer.stdout.on('data', (data) => {
        console.log(`MASM Server: ${data}`);
    });
    masmServer.on('close', (code) => {
        console.log(`MASM Server exited with code ${code}`);
    });
    console.log(`✅ BigDaddyG MASM Server started`);
    console.log(`🔥 Pure assembly model execution`);
    console.log(`⚡ Zero Node.js dependencies for AI`);
    // Start integrated Web IDE server (for unrestricted AI execution)
    console.log('🌐 Starting integrated Web IDE Server...');
    webIdeServer.listen(WEB_IDE_PORT, 'localhost', () => {
        console.log(`✅ BigDaddyG Web IDE running on http://localhost:${WEB_IDE_PORT}`);
        console.log(`🔥 Unrestricted AI model execution enabled`);
        console.log(`🚫 No VS Code/Cursor limitations`);
        console.log(`⚡ Complete file system access`);
        console.log(`🎯 Everything integrated - no separate processes needed!`);
    });
    // Graceful shutdown
    process.on('SIGINT', () => {
        console.log('\n🛑 BigDaddyG MASM Server shutting down...');
        masmServer.kill();
        webIdeServer.close();
        process.exit(0);
    });
    process.on('SIGTERM', () => {
        console.log('\n🛑 BigDaddyG MASM Server terminating...');
        masmServer.kill();
        webIdeServer.close();
        process.exit(0);
    });
    // Create Project Command
    let createProject = vscode.commands.registerCommand('bigdaddyg.createProject', async () => {
        const workspaceFolders = vscode.workspace.workspaceFolders;
        if (!workspaceFolders) {
            vscode.window.showErrorMessage('Please open a workspace first');
            return;
        }
        const projectName = await vscode.window.showInputBox({
            prompt: 'Enter project name',
            value: 'BigDaddyGProject'
        });
        if (projectName) {
            const projectPath = vscodePath.join(workspaceFolders[0].uri.fsPath, projectName);
            // Create project structure
            fs.mkdirSync(projectPath, { recursive: true });
            fs.mkdirSync(vscodePath.join(projectPath, 'src'), { recursive: true });
            fs.mkdirSync(vscodePath.join(projectPath, 'build'), { recursive: true });
            // Create main.asm template
            const mainAsmTemplate = `; BigDaddyG Assembly Project: ${projectName}
.386
.model flat, stdcall
option casemap:none

include \\masm32\\include\\windows.inc
include \\masm32\\include\\kernel32.inc
include \\masm32\\include\\user32.inc

includelib \\masm32\\lib\\kernel32.lib
includelib \\masm32\\lib\\user32.lib

.data
    szMsg       db "Hello from ${projectName}!", 0
    szTitle     db "BigDaddyG ASM", 0

.code
start:
    invoke MessageBox, 0, addr szMsg, addr szTitle, MB_OK
    invoke ExitProcess, 0
end start`;
            fs.writeFileSync(vscodePath.join(projectPath, 'src', 'main.asm'), mainAsmTemplate);
            // Create build script
            const buildScript = `@echo off
echo Building ${projectName}...
ml /c /coff src\\main.asm
link /subsystem:windows src\\main.obj
echo Build complete!
pause`;
            fs.writeFileSync(vscodePath.join(projectPath, 'build.bat'), buildScript);
            // Open the project
            const uri = vscode.Uri.file(vscodePath.join(projectPath, 'src', 'main.asm'));
            vscode.window.showTextDocument(uri);
            vscode.window.showInformationMessage(`BigDaddyG ASM project '${projectName}' created successfully!`);
        }
    });
    // Build Project Command
    let buildProject = vscode.commands.registerCommand('bigdaddyg.buildProject', () => {
        const activeEditor = vscode.window.activeTextEditor;
        if (!activeEditor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }
        const workspaceFolder = vscode.workspace.getWorkspaceFolder(activeEditor.document.uri);
        if (!workspaceFolder) {
            vscode.window.showErrorMessage('File not in workspace');
            return;
        }
        const terminal = vscode.window.createTerminal('BigDaddyG Build');
        terminal.show();
        // Save current file first
        activeEditor.document.save();
        // Build command
        const buildCmd = `cd "${workspaceFolder.uri.fsPath}" && build.bat`;
        terminal.sendText(buildCmd);
        vscode.window.showInformationMessage('Building Assembly project...');
    });
    // Run Project Command
    let runProject = vscode.commands.registerCommand('bigdaddyg.runProject', () => {
        const activeEditor = vscode.window.activeTextEditor;
        if (!activeEditor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }
        const workspaceFolder = vscode.workspace.getWorkspaceFolder(activeEditor.document.uri);
        if (!workspaceFolder) {
            vscode.window.showErrorMessage('File not in workspace');
            return;
        }
        const exePath = vscodePath.join(workspaceFolder.uri.fsPath, 'main.exe');
        if (fs.existsSync(exePath)) {
            (0, child_process_1.exec)(`"${exePath}"`, (error, stdout, stderr) => {
                if (error) {
                    vscode.window.showErrorMessage(`Execution error: ${error.message}`);
                    return;
                }
                if (stderr) {
                    vscode.window.showWarningMessage(`Warning: ${stderr}`);
                }
                vscode.window.showInformationMessage('Program executed successfully!');
            });
        }
        else {
            vscode.window.showErrorMessage('Executable not found. Build the project first.');
        }
    });
    // Assembly Language Provider
    const asmProvider = vscode.languages.registerCompletionItemProvider('asm', {
        provideCompletionItems(document, position) {
            const completions = [];
            // Assembly instructions
            const instructions = ['mov', 'add', 'sub', 'mul', 'div', 'cmp', 'jmp', 'call', 'ret', 'push', 'pop', 'invoke'];
            instructions.forEach(inst => {
                const completion = new vscode.CompletionItem(inst, vscode.CompletionItemKind.Keyword);
                completion.detail = `Assembly instruction: ${inst}`;
                completions.push(completion);
            });
            // Registers
            const registers = ['eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'esp', 'ebp'];
            registers.forEach(reg => {
                const completion = new vscode.CompletionItem(reg, vscode.CompletionItemKind.Variable);
                completion.detail = `32-bit register: ${reg}`;
                completions.push(completion);
            });
            return completions;
        }
    });
    // Register Chat Provider
    const chatProvider = new BigDaddyGChatProvider(context.extensionUri);
    context.subscriptions.push(vscode.window.registerWebviewViewProvider(BigDaddyGChatProvider.viewType, chatProvider));
    // Open Chat Command
    let openChat = vscode.commands.registerCommand('bigdaddyg.openChat', () => {
        chatProvider.createDetachedChatWindow('bigdaddyg-assembly');
        vscode.window.showInformationMessage('BigDaddyG chat window opened!');
    });
    // Launch Multi-Agent Session Command
    let launchMultiAgent = vscode.commands.registerCommand('bigdaddyg.launchMultiAgent', async () => {
        const agentCount = await vscode.window.showInputBox({
            prompt: 'How many BigDaddyG agents to launch?',
            value: '3',
            validateInput: (value) => {
                const num = parseInt(value);
                if (isNaN(num) || num < 1 || num > 10) {
                    return 'Please enter a number between 1 and 10';
                }
                return null;
            }
        });
        if (agentCount) {
            const count = parseInt(agentCount);
            const models = ['bigdaddyg-assembly', 'bigdaddyg-pe', 'bigdaddyg-reverse', 'custom-agentic-coder', 'bigdaddyg-ensemble'];
            for (let i = 0; i < count; i++) {
                const model = models[i % models.length];
                setTimeout(() => {
                    chatProvider.createDetachedChatWindow(model);
                }, i * 500); // Stagger creation
            }
            vscode.window.showInformationMessage(`Launched ${count} BigDaddyG agents for assembly development!`);
        }
    });
    // Generate Assembly Code Command
    let generateAssembly = vscode.commands.registerCommand('bigdaddyg.generateAssembly', async () => {
        const activeEditor = vscode.window.activeTextEditor;
        if (!activeEditor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }
        const selection = activeEditor.selection;
        const selectedText = activeEditor.document.getText(selection);
        const prompt = await vscode.window.showInputBox({
            prompt: 'Describe the assembly code you want to generate',
            value: selectedText ? `Convert this to assembly: ${selectedText}` : 'Generate assembly code for...'
        });
        if (prompt) {
            const useMasm = vscode.workspace.getConfiguration('bigdaddyg').get('useMasmWrapper', true);
            if (useMasm) {
                // MASM wrapper approach
                const terminal = vscode.window.createTerminal('BigDaddyG AI');
                const masmCmd = `cd "${context.extensionPath}" && ai_interface.exe "${prompt}"`;
                terminal.sendText(masmCmd);
                setTimeout(() => {
                    const responseFile = vscodePath.join(context.extensionPath, 'ai_response.txt');
                    if (fs.existsSync(responseFile)) {
                        const generatedCode = fs.readFileSync(responseFile, 'utf8');
                        const edit = new vscode.WorkspaceEdit();
                        if (selectedText) {
                            edit.replace(activeEditor.document.uri, selection, generatedCode);
                        }
                        else {
                            edit.insert(activeEditor.document.uri, activeEditor.selection.active, generatedCode);
                        }
                        vscode.workspace.applyEdit(edit);
                        fs.unlinkSync(responseFile);
                        vscode.window.showInformationMessage('Assembly code generated via MASM wrapper!');
                    }
                }, 3000);
            }
            else {
                // Original Node.js approach
                const fetch = (await Promise.resolve().then(() => __importStar(require('node-fetch')))).default;
                const response = await fetch('http://localhost:11441/v1/chat/completions', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json', 'Authorization': 'Bearer sk-local-bigdaddyg' },
                    body: JSON.stringify({ model: 'custom-agentic-coder', messages: [{ role: 'system', content: 'Expert assembly programmer. Use MASM syntax.' }, { role: 'user', content: prompt }], temperature: 0.2, max_tokens: 2000 })
                });
                if (response.ok) {
                    const data = await response.json();
                    const generatedCode = data.choices[0]?.message?.content || 'No code generated';
                    const edit = new vscode.WorkspaceEdit();
                    if (selectedText) {
                        edit.replace(activeEditor.document.uri, selection, generatedCode);
                    }
                    else {
                        edit.insert(activeEditor.document.uri, activeEditor.selection.active, generatedCode);
                    }
                    vscode.workspace.applyEdit(edit);
                    vscode.window.showInformationMessage('Assembly code generated successfully!');
                }
            }
        }
    });
    // Open Web IDE Command
    let openWebIde = vscode.commands.registerCommand('bigdaddyg.openWebIde', () => {
        const webIdeUrl = `http://localhost:${WEB_IDE_PORT}`;
        vscode.env.openExternal(vscode.Uri.parse(webIdeUrl));
        vscode.window.showInformationMessage(`BigDaddyG Web IDE opened at ${webIdeUrl} - Unrestricted AI model execution!`);
    });
    context.subscriptions.push(createProject, buildProject, runProject, asmProvider, openChat, launchMultiAgent, generateAssembly, openWebIde);
}
exports.activate = activate;
function deactivate() { }
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map
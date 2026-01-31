// IRC Bot for RawrZ Security Platform - Clean Implementation
const { chatterbox } = require('./chatterbox');
const { logger } = require('./logger');
const { databaseIntegration } = require('./databaseIntegration');

class IRCBot {
    constructor(config = {}) {
        this.config = {
            server: config.server || 'irc.rizon.net',
            port: config.port || 6667,
            channels: config.channels || ['#rawr'],
            nick: config.nick || 'RawrZBot',
            username: config.username || 'bibbles11',
            realname: config.realname || 'RawrZ Security Platform Monitor',
            password: config.password || 'bibbles11',
            ...config
        };
        
        this.connected = false;
        this.socket = null;
        this.channels = new Set(this.config.channels);
        this.messageQueue = [];
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.channelsJoined = false;
        this.messageQueue = [];
        this.rateLimitDelay = 500; // 500ms between messages
        this.lastMessageTime = 0;
        
        this.initializeIRC();
        this.initializeDatabase();
    }

    initializeIRC() {
        logger.info('[BOT] IRC Bot initializing...');
        this.setupChatterboxListeners();
        this.connect();
    }

    setupChatterboxListeners() {
        chatterbox.on('script_registered', (data) => {
            this.sendToIRC(`[LOG] Script registered: ${data.scriptId} (${data.script.name})`);
        });

        chatterbox.on('status_change', (data) => {
            this.sendToIRC(`[INFO] ${data.script.name}: ${data.oldStatus} [CHAR] ${data.newStatus}`);
        });

        chatterbox.on('script_error', (data) => {
            this.sendToIRC(`[ERROR] ERROR in ${data.scriptName}: ${data.error}`);
            if (data.requestId) {
                this.sendToIRC(`[SEARCH] RequestID: ${data.requestId}`);
            }
        });

        chatterbox.on('heartbeat_overdue', (data) => {
            this.sendToIRC(`[HEARTBEAT] Heartbeat overdue: ${data.scriptName} (${Math.round(data.timeSinceHeartbeat / 1000)}s)`);
        });

        // Start heartbeat monitoring if not already started
        if (!chatterbox.heartbeatInterval) {
            chatterbox.startHeartbeatMonitoring();
        }
    }

    async initializeDatabase() {
        try {
            await databaseIntegration.initialize();
            if (databaseIntegration.enabled) {
                logger.info('[BOT] Database integration enabled');
            } else {
                logger.info('[BOT] Database integration disabled - running in standalone mode');
            }
        } catch (error) {
            logger.error('[BOT] Failed to initialize database integration:', error);
        }
    }

    async logCommandToDatabase(command, args, userId, channel, response, executionTime, status = 'success', error = null) {
        try {
            await databaseIntegration.logCommandHistory({
                command,
                args,
                userId,
                channel,
                response,
                executionTime,
                status,
                error,
                metadata: {
                    timestamp: new Date().toISOString(),
                    botVersion: '1.0.0'
                }
            });
        } catch (dbError) {
            // Don't let database errors affect command execution
            logger.error('[BOT] Failed to log command to database:', dbError);
        }
    }

    connect() {
            const net = require('net');
        
            this.socket = new net.Socket();
            
            this.socket.connect(this.config.port, this.config.server, () => {
                logger.info(`[BOT] Connected to IRC server: ${this.config.server}:${this.config.port}`);
                this.connected = true;
                this.reconnectAttempts = 0;
            
            // Send IRC registration
            this.sendRaw(`NICK ${this.config.nick}`);
            this.sendRaw(`USER ${this.config.username} 0 * :${this.config.realname}`);
            
            // Send queued messages
            while (this.messageQueue.length > 0) {
                const message = this.messageQueue.shift();
                this.sendToIRC(message);
            }
            });

            this.socket.on('data', (data) => {
            const lines = data.toString().split('\r\n');
            for (const line of lines) {
                if (line.trim()) {
                    this.handleIRCLine(line);
                }
            }
            });

            this.socket.on('close', () => {
            logger.info('[BOT] IRC connection closed');
                this.connected = false;
                this.reconnect();
            });

            this.socket.on('error', (error) => {
            logger.error('[BOT] IRC connection error:', error);
                this.connected = false;
                this.reconnect();
            });
    }

    handleIRCLine(line) {
        // Handle PING
        if (line.startsWith('PING')) {
            const server = line.split(' ')[1];
            this.sendRaw(`PONG ${server}`);
            return;
        }

        // Handle authentication
        if (line.includes('NickServ') && line.includes('IDENTIFY')) {
            this.sendRaw(`PRIVMSG NickServ :IDENTIFY ${this.config.password}`);
            logger.info('[BOT] IRC: Identifying with NickServ');
            return;
        }

        // Also try to identify immediately after connection
        if (line.includes('376') || line.includes('End of /MOTD command')) {
            logger.info('[BOT] IRC: MOTD received, attempting NickServ identification');
            setTimeout(() => {
                this.sendRaw(`PRIVMSG NickServ :IDENTIFY ${this.config.password}`);
                logger.info('[BOT] IRC: Sending NickServ identify command');
            }, 1000);
        }

        // Handle successful authentication (multiple possible messages)
        if (line.includes('You are now identified') || 
            line.includes('Password accepted') ||
            line.includes('You are successfully identified')) {
            logger.info('[BOT] IRC: Successfully authenticated');
            setTimeout(() => {
                for (const channel of this.channels) {
                    this.sendRaw(`JOIN ${channel}`);
                    logger.info(`[BOT] IRC: Joining channel ${channel}`);
                }
            }, 1000); // Wait 1 second before joining channels
            return;
        }

        // Handle MOTD end (fallback for channels that don't require auth)
        if (line.includes('End of /MOTD command') || line.includes('376')) {
            if (!this.channelsJoined) {
                logger.info('[BOT] IRC: MOTD received, joining channels');
                this.channelsJoined = true;
                setTimeout(() => {
        for (const channel of this.channels) {
            this.sendRaw(`JOIN ${channel}`);
            logger.info(`[BOT] IRC: Joining channel ${channel}`);
                    }
                }, 2000); // Wait 2 seconds after MOTD
            }
            return;
        }

        // Handle channel messages
        this.handleChannelMessage(line);
    }

    handleChannelMessage(line) {
        const match = line.match(/:([^!]+)![^@]+@[^ ]+ PRIVMSG ([^ ]+) :(.+)/);
        if (!match) return;

        const [, nick, channel, message] = match;
        
        if (message.startsWith('!')) {
            this.handleCommand(nick, channel, message);
        }
    }

    async handleCommand(nick, channel, message) {
        const startTime = Date.now();
        const args = message.split(' ');
        const command = args[0].toLowerCase();
        let response = '';
        let status = 'success';
        let error = null;
        
        try {
            switch (command) {
            case '!status':
                this.sendSystemStatus();
                break;
            case '!scripts':
                this.sendScriptStatus();
                break;
            case '!errors':
                this.sendRecentErrors();
                break;
            case '!stuck':
                this.sendStuckScripts();
                break;
            case '!requestid':
                this.sendRequestIdErrors();
                break;
            case '!encrypt':
                this.handleEncrypt(nick, channel, args.slice(1));
                break;
            case '!decrypt':
                this.handleDecrypt(nick, channel, args.slice(1));
                break;
            case '!algorithms':
                this.handleAlgorithms(nick, channel);
                break;
            case '!upload':
                this.handleUpload(nick, channel, args.slice(1));
                break;
            case '!files':
                this.handleListFiles(nick, channel);
                break;
            case '!convert':
                this.handleConvertFile(nick, channel, args.slice(1));
                break;
            case '!simpleenc':
                this.handleSimpleEncrypt(nick, channel, args.slice(1));
                break;
            case '!stub':
                this.handleStubGeneration(nick, channel, args.slice(1));
                break;
            case '!compress':
                this.handleCompression(nick, channel, args.slice(1));
                break;
            case '!obfuscate':
                this.handleObfuscation(nick, channel, args.slice(1));
                break;
            case '!compile':
                this.handleCompileStub(nick, channel, args.slice(1));
                break;
            case '!hotpatch':
                this.handleHotPatch(nick, channel, args.slice(1));
                break;
            case '!polymorph':
                this.handlePolymorph(nick, channel, args.slice(1));
                break;
            case '!antianalysis':
                this.handleAntiAnalysis(nick, channel, args.slice(1));
                break;
            case '!reverse':
                this.handleReverseEngineering(nick, channel, args.slice(1));
                break;
            case '!mobile':
                this.handleMobileAnalysis(nick, channel, args.slice(1));
                break;
            case '!network':
                this.handleNetworkAnalysis(nick, channel, args.slice(1));
                break;
            case '!forensics':
                this.handleDigitalForensics(nick, channel, args.slice(1));
                break;
            case '!malware':
                this.handleMalwareAnalysis(nick, channel, args.slice(1));
                break;
            case '!stealth':
                this.handleStealth(nick, channel, args.slice(1));
                break;
            case '!memory':
                this.handleMemoryOptimization(nick, channel, args.slice(1));
                break;
            case '!backup':
                this.handleBackup(nick, channel, args.slice(1));
                break;
            case '!assemble':
                this.handleAssembly(nick, channel, args.slice(1));
                break;
            case '!dualgen':
                this.handleDualGenerators(nick, channel, args.slice(1));
                break;
            case '!apistatus':
                this.handleAPIStatus(nick, channel, args.slice(1));
                break;
            case '!help':
                this.sendHelp(args.slice(1));
                break;
            case '!dbstats':
                this.handleDatabaseStats(nick, channel);
                break;
            default:
                this.sendToIRC(`${nick}: Unknown command. Use !help for available commands.`);
                status = 'error';
                error = 'Unknown command';
        }
        
        // Log command to database
        const executionTime = Date.now() - startTime;
        await this.logCommandToDatabase(
            command, 
            args.slice(1), 
            nick, 
            channel, 
            response, 
            executionTime, 
            status, 
            error
        );
        
        } catch (error) {
            // Log error to database
            const executionTime = Date.now() - startTime;
            await this.logCommandToDatabase(
                command, 
                args.slice(1), 
                nick, 
                channel, 
                'Command execution failed', 
                executionTime, 
                'error', 
                error.message
            );
            this.sendToIRC(`${nick}: Command execution failed: ${error.message}`);
        }
    }

    // Clean encryption handler
    async handleEncrypt(nick, channel, args) {
        if (args.length < 2) {
            this.sendToIRC(`${nick}: Usage: !encrypt <algorithm> <file_url_or_text>`);
            this.sendToIRC(`${nick}: Example: !encrypt aes-256-gcm https://example.com/file.txt`);
            return;
        }

        let algorithm = args[0];
        const input = args.slice(1).join(' ');

        // Handle common algorithm name variations
        const algorithmMap = {
            // Camellia variations
            'cam-256-cbc': 'camellia-256-cbc',
            'cam-192-cbc': 'camellia-192-cbc',
            'cam-128-cbc': 'camellia-128-cbc',
            'cam-256-ctr': 'camellia-256-ctr',
            'cam-192-ctr': 'camellia-192-ctr',
            'cam-128-ctr': 'camellia-128-ctr',
            'camellia-256-gcm': 'camellia-256-gcm',
            'camellia-192-gcm': 'camellia-192-gcm',
            'camellia-128-gcm': 'camellia-128-gcm',
            'cam-256-gcm': 'camellia-256-gcm',
            'cam-192-gcm': 'camellia-192-gcm',
            'cam-128-gcm': 'camellia-128-gcm',
            
            // AES variations
            'aes256gcm': 'aes-256-gcm',
            'aes192gcm': 'aes-192-gcm',
            'aes128gcm': 'aes-128-gcm',
            'aes256cbc': 'aes-256-cbc',
            'aes192cbc': 'aes-192-cbc',
            'aes128cbc': 'aes-128-cbc',
            'aes-256': 'aes-256-gcm',
            'aes-192': 'aes-192-gcm',
            'aes-128': 'aes-128-gcm',
            
            // ARIA variations
            'aria256gcm': 'aria-256-gcm',
            'aria192gcm': 'aria-192-gcm',
            'aria128gcm': 'aria-128-gcm',
            'aria-256': 'aria-256-gcm',
            'aria-192': 'aria-192-gcm',
            'aria-128': 'aria-128-gcm',
            
            // ChaCha20 variations
            'chacha': 'chacha20',
            'chacha20-poly1305': 'chacha20',
            
            // RSA variations
            'rsa': 'rsa-4096',
            'rsa4096': 'rsa-4096'
        };

        if (algorithmMap[algorithm.toLowerCase()]) {
            algorithm = algorithmMap[algorithm.toLowerCase()];
            this.sendToIRC(`${nick}: Using algorithm: ${algorithm}`);
        }

        try {
            // Get RawrZ engine instance
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();

            let dataToEncrypt;
            let inputType = 'text';

            // Handle different input types
            if (input.startsWith('http://') || input.startsWith('https://') || input.startsWith('ftp://')) {
                // Download from URL
                dataToEncrypt = await this.downloadFile(input);
                inputType = 'file';
                this.sendToIRC(`${nick}: File downloaded (${dataToEncrypt.length} bytes)`);
            } else if (input.startsWith('file:')) {
                // Read local file
                const filename = input.slice(5);
                dataToEncrypt = await this.readLocalFile(filename);
                inputType = 'file';
                this.sendToIRC(`${nick}: Local file read (${dataToEncrypt.length} bytes)`);
            } else {
                // Treat as text
                dataToEncrypt = input;
            }

            // Encrypt the data
            const result = await engine.encryptAdvanced(dataToEncrypt, { algorithm });
            
            // Use suggested extension from encryption result
            const fileExtension = result.data.suggestedExtension || '.enc';
            
            // Save encrypted file with proper extension
            const filename = await this.saveEncryptedFile(result, algorithm, fileExtension);
            
            // Send results
            this.sendToIRC(`${nick}: Encryption successful!`);
            this.sendToIRC(`${nick}: Type: ${inputType} | Algorithm: ${algorithm}`);
            
            // Handle different result structures
            const encryptedSize = result.metadata?.encryptedSize || result.data?.length || 'unknown';
            this.sendToIRC(`${nick}: Original: ${dataToEncrypt.length} bytes | Encrypted: ${encryptedSize} bytes`);
            this.sendToIRC(`${nick}: File: ${filename} (preserves original extension)`);
            this.sendToIRC(`${nick}: Key: ${result.key || 'Generated'}`);
            this.sendToIRC(`${nick}: IV: ${result.iv || 'Generated'}`);
            if (result.authTag) {
                this.sendToIRC(`${nick}: Auth Tag: ${result.authTag}`);
            }
            
            // Provide download options
            this.sendToIRC(`${nick}: Download options:`);
            this.sendToIRC(`${nick}: 1. File saved locally: ${filename}`);
            this.sendToIRC(`${nick}: 2. Use !upload command to re-upload with custom name`);
            this.sendToIRC(`${nick}: 3. Access via API: POST /api/upload with filename and base64 data`);
            
            // Provide the encrypted data in a manageable format
            if (result.data) {
                try {
                    const base64Data = Buffer.from(result.data, 'hex').toString('base64');
                    if (base64Data.length > 200) {
                        this.sendToIRC(`${nick}: Encrypted data (first 200 chars): ${base64Data.substring(0, 200)}...`);
                        this.sendToIRC(`${nick}: Full data available via !upload command or API`);
                    } else {
                        this.sendToIRC(`${nick}: Encrypted data (base64): ${base64Data}`);
                    }
                } catch (bufferError) {
                    this.sendToIRC(`${nick}: Encrypted data saved to file: ${filename}`);
                    this.sendToIRC(`${nick}: Use !decrypt command with key/IV to decrypt`);
                }
            } else {
                this.sendToIRC(`${nick}: Encrypted data saved to file: ${filename}`);
                this.sendToIRC(`${nick}: Use !decrypt command with key/IV to decrypt`);
            }

        } catch (error) {
            logger.error('IRC Encryption error:', error);
            this.sendToIRC(`${nick}: Encryption failed: ${error.message}`);
        }
    }

    // Clean decryption handler
    // Decrypt handler - works with ALL encryption methods and converted files
    async handleDecrypt(nick, channel, args) {
        if (args.length < 4) {
            this.sendToIRC(`${nick}: Usage: !decrypt <algorithm> <encrypted_data_or_filename> <key> <iv> [auth_tag]`);
            this.sendToIRC(`${nick}: Example: !decrypt aes-256-gcm <encrypted_data> <key> <iv>`);
            this.sendToIRC(`${nick}: Example: !decrypt cam-256-cbc encrypted_file.enc <key> <iv>`);
            this.sendToIRC(`${nick}: Example: !decrypt aria-256-gcm converted_file.pdf <key> <iv>`);
            return;
        }

        const [algorithm, encryptedInput, key, iv, authTag] = args;

        try {
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();

            let encryptedData;
            let isFile = false;
            let isConvertedFile = false;

            // Check if input is a filename (contains .enc or other extensions)
            if (encryptedInput.includes('.') && !encryptedInput.startsWith('data:')) {
                isFile = true;
                const fs = require('fs').promises;
                const path = require('path');
                
                const uploadsDir = path.join(__dirname, '../../uploads');
                const filePath = path.join(uploadsDir, encryptedInput);
                
                try {
                    await fs.access(filePath);
                    const fileContent = await fs.readFile(filePath, 'utf8');
                    const parsedData = JSON.parse(fileContent);
                    
                    // Check if this is a converted file
                    if (parsedData.isConverted) {
                        isConvertedFile = true;
                        this.sendToIRC(`${nick}: Detected converted file with extension: ${parsedData.convertedExtension}`);
                        this.sendToIRC(`${nick}: Original encrypted file: ${parsedData.originalEncryptedFilename}`);
                    }
                    
                    encryptedData = parsedData;
                } catch (error) {
                    this.sendToIRC(`${nick}: File not found: ${encryptedInput}`);
                    return;
                }
            } else {
                // Direct encrypted data
                try {
                    encryptedData = JSON.parse(encryptedInput);
                } catch (error) {
                    // If not JSON, treat as raw encrypted data
                    encryptedData = {
                        data: encryptedInput,
                        algorithm: algorithm
                    };
                }
            }

            const result = await engine.decryptAdvanced(encryptedData, {
                algorithm,
                key,
                iv,
                authTag: authTag || null
            });
            
            this.sendToIRC(`${nick}: Decryption successful!`);
            this.sendToIRC(`${nick}: Algorithm: ${algorithm}`);
            this.sendToIRC(`${nick}: Method: ${isFile ? (isConvertedFile ? 'Converted File' : 'Encrypted File') : 'Direct Data'}`);
            
            if (result.data && result.data.length > 0) {
                const preview = result.data.substring(0, 200);
                this.sendToIRC(`${nick}: Decrypted data: ${preview}${result.data.length > 200 ? '...' : ''}`);
                this.sendToIRC(`${nick}: Data length: ${result.data.length} characters`);
            } else {
                this.sendToIRC(`${nick}: Decrypted data is empty or invalid`);
            }
            
            // Show supported algorithms for reference
            if (isConvertedFile) {
                this.sendToIRC(`${nick}: Note: Converted files work with all encryption methods`);
            }

        } catch (error) {
            logger.error('IRC Decryption error:', error);
            this.sendToIRC(`${nick}: Decryption failed: ${error.message}`);
            this.sendToIRC(`${nick}: Supported algorithms: AES, Camellia, ARIA, ChaCha20, RSA`);
        }
    }

    // List available algorithms
    handleAlgorithms(nick, channel) {
        const algorithms = [
            'aes-256-gcm', 'aes-192-gcm', 'aes-128-gcm',
            'aes-256-cbc', 'aes-192-cbc', 'aes-128-cbc',
            'camellia-256-cbc', 'camellia-192-cbc', 'camellia-128-cbc',
            'camellia-256-ctr', 'camellia-192-ctr', 'camellia-128-ctr',
            'aria-256-gcm', 'aria-192-gcm', 'aria-128-gcm',
            'chacha20', 'rsa-4096'
        ];

        this.sendToIRC(`${nick}: Available algorithms (${algorithms.length}):`);
        
        // Split into chunks for IRC message limits
        const chunkSize = 5;
        for (let i = 0; i < algorithms.length; i += chunkSize) {
            const chunk = algorithms.slice(i, i + chunkSize);
            this.sendToIRC(`${nick}: ${chunk.join(', ')}`);
        }
        
        this.sendToIRC(`${nick}: Common shortcuts: cam-256-cbc, aes-256, aria-256, chacha, rsa`);
        this.sendToIRC(`${nick}: Note: Camellia GCM modes use CBC instead (GCM not supported)`);
    }

    // File upload handler
    async handleUpload(nick, channel, args) {
        if (args.length < 2) {
            this.sendToIRC(`${nick}: Usage: !upload <filename> <base64_data>`);
            return;
        }

        const filename = args[0];
        const base64Data = args.slice(1).join(' ');

        try {
            const data = Buffer.from(base64Data, 'base64');
            
            // Check file size limit (100MB)
            const maxSize = 100 * 1024 * 1024;
            if (data.length > maxSize) {
                throw new Error(`File too large: ${data.length} bytes (max: ${maxSize} bytes)`);
            }
            
            // Sanitize filename to prevent path traversal and invalid characters
            const sanitizedFilename = filename
                .replace(/[<>:"/\\|?*]/g, '_')  // Replace invalid characters
                .replace(/\.\./g, '_')          // Prevent path traversal
                .substring(0, 255);             // Limit length
            
            // Save file
            const fs = require('fs').promises;
            const path = require('path');
            
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const filePath = path.join(uploadsDir, sanitizedFilename);
            await fs.writeFile(filePath, data);
            
            const downloadUrl = `http://localhost:3002/api/download/${filename}`;
            
            this.sendToIRC(`${nick}: File uploaded successfully!`);
            this.sendToIRC(`${nick}: Original filename: ${filename}`);
            this.sendToIRC(`${nick}: Sanitized filename: ${sanitizedFilename}`);
            this.sendToIRC(`${nick}: Size: ${data.length} bytes`);
            this.sendToIRC(`${nick}: File saved locally: ${sanitizedFilename}`);
            this.sendToIRC(`${nick}: Ready for encryption: !encrypt <algorithm> file:${sanitizedFilename}`);
            this.sendToIRC(`${nick}: Note: Use !files to list all available files`);

        } catch (error) {
            logger.error('IRC Upload error:', error);
            this.sendToIRC(`${nick}: Upload failed: ${error.message}`);
        }
    }

    // Simple encryption with auto-conversion to user's chosen extension
    async handleSimpleEncrypt(nick, channel, args) {
        if (args.length < 3) {
            this.sendToIRC(`${nick}: Usage: !simpleenc <algorithm> <file_url_or_text> <target_extension>`);
            this.sendToIRC(`${nick}: Example: !simpleenc aes-256-gcm https://example.com/file.exe .pdf`);
            this.sendToIRC(`${nick}: Example: !simpleenc cam-256-cbc Hello World .txt`);
            this.sendToIRC(`${nick}: Example: !simpleenc aria-256-gcm file:document.pdf .dll`);
            return;
        }

        const algorithm = args[0];
        const input = args.slice(1, -1).join(' '); // Everything except last argument
        const targetExtension = args[args.length - 1]; // Last argument is the extension

        // Validate extension format
        const cleanExtension = targetExtension.startsWith('.') ? targetExtension : '.' + targetExtension;

        try {
            // First, perform the encryption using the existing handleEncrypt logic
            this.sendToIRC(`${nick}: Starting simple encryption with auto-conversion...`);
            
            // Get the normalized algorithm
            const algorithmMap = {
                // Camellia variations
                'cam-256-cbc': 'camellia-256-cbc',
                'cam-192-cbc': 'camellia-192-cbc',
                'cam-128-cbc': 'camellia-128-cbc',
                'cam-256-ctr': 'camellia-256-ctr',
                'cam-192-ctr': 'camellia-192-ctr',
                'cam-128-ctr': 'camellia-128-ctr',
                'camellia-256-gcm': 'camellia-256-gcm',
                'camellia-192-gcm': 'camellia-192-gcm',
                'camellia-128-gcm': 'camellia-128-gcm',
                'cam-256-gcm': 'camellia-256-gcm',
                'cam-192-gcm': 'camellia-192-gcm',
                'cam-128-gcm': 'camellia-128-gcm',
                
                // AES variations
                'aes256gcm': 'aes-256-gcm',
                'aes192gcm': 'aes-192-gcm',
                'aes128gcm': 'aes-128-gcm',
                'aes256cbc': 'aes-256-cbc',
                'aes192cbc': 'aes-192-cbc',
                'aes128cbc': 'aes-128-cbc',
                'aes-256': 'aes-256-gcm',
                'aes-192': 'aes-192-gcm',
                'aes-128': 'aes-128-gcm',
                
                // ARIA variations
                'aria-256-gcm': 'aria-256-gcm',
                'aria-192-gcm': 'aria-192-gcm',
                'aria-128-gcm': 'aria-128-gcm',
                'aria-256': 'aria-256-gcm',
                'aria-192': 'aria-192-gcm',
                'aria-128': 'aria-128-gcm',
                
                // ChaCha20 variations
                'chacha20': 'chacha20',
                'chacha': 'chacha20',
                
                // RSA variations
                'rsa-4096': 'rsa-4096',
                'rsa': 'rsa-4096'
            };

            let normalizedAlgorithm = algorithm;
            if (algorithmMap[algorithm.toLowerCase()]) {
                normalizedAlgorithm = algorithmMap[algorithm.toLowerCase()];
                this.sendToIRC(`${nick}: Using algorithm: ${normalizedAlgorithm}`);
            }

            // Get RawrZ engine instance
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();

            let dataToEncrypt;
            let inputType = 'text';

            // Handle different input types
            if (input.startsWith('http://') || input.startsWith('https://') || input.startsWith('ftp://')) {
                // Download from URL
                dataToEncrypt = await this.downloadFile(input);
                inputType = 'file';
                this.sendToIRC(`${nick}: File downloaded (${dataToEncrypt.length} bytes)`);
            } else if (input.startsWith('file:')) {
                // Read local file
                const filename = input.slice(5);
                const fs = require('fs').promises;
                const path = require('path');
                
                const uploadsDir = path.join(__dirname, '../../uploads');
                const filePath = path.join(uploadsDir, filename);
                
                try {
                    dataToEncrypt = await fs.readFile(filePath);
                    inputType = 'file';
                    this.sendToIRC(`${nick}: Local file read (${dataToEncrypt.length} bytes)`);
                } catch (error) {
                    this.sendToIRC(`${nick}: Local file not found: ${filename}`);
            return;
                }
            } else {
                // Treat as text
                dataToEncrypt = input;
                inputType = 'text';
            }

            // Check file size limit (100MB)
            const maxSize = 100 * 1024 * 1024;
            if (dataToEncrypt.length > maxSize) {
                this.sendToIRC(`${nick}: File too large. Maximum size: 100MB`);
                return;
            }

            // Perform encryption
            const encryptionOptions = {
                algorithm: normalizedAlgorithm,
                dataType: inputType === 'file' ? 'binary' : 'text',
                outputFormat: 'hex'
            };

            const result = await engine.encryptAdvanced(dataToEncrypt, encryptionOptions);
            
            // Create the encrypted file with .enc extension first
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const tempFilename = `encrypted_${normalizedAlgorithm.replace(/[^a-zA-Z0-9]/g, '_')}_${timestamp}.enc`;
            
            const fs = require('fs').promises;
            const path = require('path');
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const tempFilePath = path.join(uploadsDir, tempFilename);
            const encryptedContent = JSON.stringify({
                algorithm: normalizedAlgorithm,
                data: result.data,
                key: result.key,
                iv: result.iv,
                authTag: result.authTag,
                metadata: result.metadata,
                timestamp: new Date().toISOString()
            }, null, 2);
            
            await fs.writeFile(tempFilePath, encryptedContent, 'utf8');
            
            // Now automatically convert to the user's chosen extension
            const finalFilename = `encrypted_${normalizedAlgorithm.replace(/[^a-zA-Z0-9]/g, '_')}_${timestamp}${cleanExtension}`;
            const finalFilePath = path.join(uploadsDir, finalFilename);
            
            const convertedContent = JSON.stringify({
                algorithm: normalizedAlgorithm,
                data: result.data,
                key: result.key,
                iv: result.iv,
                authTag: result.authTag,
                metadata: result.metadata,
                timestamp: new Date().toISOString(),
                convertedExtension: cleanExtension,
                originalEncryptedFilename: tempFilename,
                conversionTimestamp: new Date().toISOString(),
                isConverted: true
            }, null, 2);
            
            await fs.writeFile(finalFilePath, convertedContent, 'utf8');
            
            // Clean up the temporary .enc file
            try {
                await fs.unlink(tempFilePath);
            } catch (error) {
                // Ignore cleanup errors
            }
            
            // Send results
            this.sendToIRC(`${nick}: Simple encryption with auto-conversion completed!`);
            this.sendToIRC(`${nick}: Type: ${inputType} | Algorithm: ${normalizedAlgorithm}`);
            this.sendToIRC(`${nick}: Original: ${dataToEncrypt.length} bytes | Encrypted: ${result.data?.length || 'unknown'} bytes`);
            this.sendToIRC(`${nick}: File: ${finalFilename} (appears as ${cleanExtension})`);
            this.sendToIRC(`${nick}: Key: ${result.key || 'Generated'}`);
            this.sendToIRC(`${nick}: IV: ${result.iv || 'Generated'}`);
            if (result.authTag) {
                this.sendToIRC(`${nick}: Auth Tag: ${result.authTag}`);
            }
            this.sendToIRC(`${nick}: Note: File appears as ${cleanExtension} but contains encrypted data`);
            this.sendToIRC(`${nick}: Use !decrypt with the same key/IV to decrypt the file`);
            this.sendToIRC(`${nick}: Supported for ALL encryption methods: AES, Camellia, ARIA, ChaCha20, RSA`);
            
        } catch (error) {
            logger.error('IRC Simple encryption error:', error);
            this.sendToIRC(`${nick}: Simple encryption failed: ${error.message}`);
        }
    }

    // Stub generation handler - full RawrZ Engine capabilities
    async handleStubGeneration(nick, channel, args) {
        if (args.length < 2) {
            this.sendToIRC(`${nick}: Usage: !stub <algorithm> <target> [stub_type] [executable_type]`);
            this.sendToIRC(`${nick}: Example: !stub aes-256-gcm https://example.com/file.exe console`);
            this.sendToIRC(`${nick}: Example: !stub cam-256-cbc file:document.pdf gui`);
            this.sendToIRC(`${nick}: Example: !stub aria-256-gcm "Hello World" service`);
            return;
        }

        const algorithm = args[0];
        const target = args.slice(1, -1).join(' '); // Everything except last argument
        const executableType = args[args.length - 1] || 'console'; // Last argument is executable type

        try {
            this.sendToIRC(`${nick}: Starting stub generation...`);
            
            // Get the normalized algorithm
            const algorithmMap = {
                // Camellia variations
                'cam-256-cbc': 'camellia-256-cbc',
                'cam-192-cbc': 'camellia-192-cbc',
                'cam-128-cbc': 'camellia-128-cbc',
                'cam-256-ctr': 'camellia-256-ctr',
                'cam-192-ctr': 'camellia-192-ctr',
                'cam-128-ctr': 'camellia-128-ctr',
                'camellia-256-gcm': 'camellia-256-gcm',
                'camellia-192-gcm': 'camellia-192-gcm',
                'camellia-128-gcm': 'camellia-128-gcm',
                'cam-256-gcm': 'camellia-256-gcm',
                'cam-192-gcm': 'camellia-192-gcm',
                'cam-128-gcm': 'camellia-128-gcm',
                
                // AES variations
                'aes256gcm': 'aes-256-gcm',
                'aes192gcm': 'aes-192-gcm',
                'aes128gcm': 'aes-128-gcm',
                'aes256cbc': 'aes-256-cbc',
                'aes192cbc': 'aes-192-cbc',
                'aes128cbc': 'aes-128-cbc',
                'aes-256': 'aes-256-gcm',
                'aes-192': 'aes-192-gcm',
                'aes-128': 'aes-128-gcm',
                
                // ARIA variations
                'aria-256-gcm': 'aria-256-gcm',
                'aria-192-gcm': 'aria-192-gcm',
                'aria-128-gcm': 'aria-128-gcm',
                'aria-256': 'aria-256-gcm',
                'aria-192': 'aria-192-gcm',
                'aria-128': 'aria-128-gcm',
                
                // ChaCha20 variations
                'chacha20': 'chacha20',
                'chacha': 'chacha20',
                
                // RSA variations
                'rsa-4096': 'rsa-4096',
                'rsa': 'rsa-4096'
            };

            let normalizedAlgorithm = algorithm;
            if (algorithmMap[algorithm.toLowerCase()]) {
                normalizedAlgorithm = algorithmMap[algorithm.toLowerCase()];
                this.sendToIRC(`${nick}: Using algorithm: ${normalizedAlgorithm}`);
            }

            // Get RawrZ engine instance
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();

            // Prepare stub generation options
            const stubOptions = {
                algorithm: normalizedAlgorithm,
                executableType: executableType,
                stubFormat: 'executable'
            };

            const result = await engine.generateStub(target, stubOptions);
            
            // Save the generated stub
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `stub_${normalizedAlgorithm.replace(/[^a-zA-Z0-9]/g, '_')}_${executableType}_${timestamp}.exe`;
            
            const fs = require('fs').promises;
            const path = require('path');
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const filePath = path.join(uploadsDir, filename);
            await fs.writeFile(filePath, result.data, 'binary');
            
            this.sendToIRC(`${nick}: Stub generation completed!`);
            this.sendToIRC(`${nick}: Algorithm: ${normalizedAlgorithm}`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Executable Type: ${executableType}`);
            this.sendToIRC(`${nick}: File: ${filename}`);
            this.sendToIRC(`${nick}: Size: ${result.data.length} bytes`);
            this.sendToIRC(`${nick}: Stub saved locally: ${filename}`);
            this.sendToIRC(`${nick}: Note: Stub contains encrypted payload with ${normalizedAlgorithm}`);
            
        } catch (error) {
            logger.error('IRC Stub generation error:', error);
            this.sendToIRC(`${nick}: Stub generation failed: ${error.message}`);
        }
    }

    // Compression handler - full RawrZ Engine capabilities
    async handleCompression(nick, channel, args) {
        if (args.length < 2) {
            this.sendToIRC(`${nick}: Usage: !compress <algorithm> <data_or_url> [compression_level]`);
            this.sendToIRC(`${nick}: Example: !compress gzip https://example.com/file.txt`);
            this.sendToIRC(`${nick}: Example: !compress deflate "Hello World" 9`);
            this.sendToIRC(`${nick}: Example: !compress brotli file:document.pdf`);
            return;
        }

        const algorithm = args[0];
        const input = args.slice(1, -1).join(' '); // Everything except last argument
        const compressionLevel = parseInt(args[args.length - 1]) || 6; // Last argument is compression level

        try {
            this.sendToIRC(`${nick}: Starting compression...`);
            
            // Get RawrZ engine instance
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();

            let dataToCompress;
            let inputType = 'text';

            // Handle different input types
            if (input.startsWith('http://') || input.startsWith('https://') || input.startsWith('ftp://')) {
                // Download from URL
                dataToCompress = await this.downloadFile(input);
                inputType = 'file';
                this.sendToIRC(`${nick}: File downloaded (${dataToCompress.length} bytes)`);
            } else if (input.startsWith('file:')) {
                // Read local file
                const filename = input.slice(5);
                const fs = require('fs').promises;
                const path = require('path');
                
                const uploadsDir = path.join(__dirname, '../../uploads');
                const filePath = path.join(uploadsDir, filename);
                
                try {
                    dataToCompress = await fs.readFile(filePath);
                    inputType = 'file';
                    this.sendToIRC(`${nick}: Local file read (${dataToCompress.length} bytes)`);
                } catch (error) {
                    this.sendToIRC(`${nick}: Local file not found: ${filename}`);
                    return;
                }
            } else {
                // Treat as text
                dataToCompress = input;
                inputType = 'text';
            }

            // Perform compression
            const compressionOptions = {
                algorithm: algorithm,
                level: compressionLevel
            };

            const result = await engine.compress(dataToCompress, compressionOptions);
            
            // Save the compressed data
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `compressed_${algorithm}_${timestamp}.${algorithm}`;
            
            const fs = require('fs').promises;
            const path = require('path');
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const filePath = path.join(uploadsDir, filename);
            await fs.writeFile(filePath, result.data, 'binary');
            
            this.sendToIRC(`${nick}: Compression completed!`);
            this.sendToIRC(`${nick}: Algorithm: ${algorithm}`);
            this.sendToIRC(`${nick}: Type: ${inputType}`);
            this.sendToIRC(`${nick}: Original: ${dataToCompress.length} bytes`);
            this.sendToIRC(`${nick}: Compressed: ${result.data.length} bytes`);
            this.sendToIRC(`${nick}: Ratio: ${((1 - result.data.length / dataToCompress.length) * 100).toFixed(2)}%`);
            this.sendToIRC(`${nick}: File: ${filename}`);
            this.sendToIRC(`${nick}: Compressed file saved locally: ${filename}`);
            
        } catch (error) {
            logger.error('IRC Compression error:', error);
            this.sendToIRC(`${nick}: Compression failed: ${error.message}`);
        }
    }

    // Obfuscation handler - full RawrZ Engine capabilities
    async handleObfuscation(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !obfuscate <data_or_url> [obfuscation_type]`);
            this.sendToIRC(`${nick}: Example: !obfuscate https://example.com/file.txt`);
            this.sendToIRC(`${nick}: Example: !obfuscate "Hello World" xor`);
            this.sendToIRC(`${nick}: Example: !obfuscate file:document.pdf base64`);
            return;
        }

        const input = args.slice(0, -1).join(' '); // Everything except last argument
        const obfuscationType = args[args.length - 1] || 'xor'; // Last argument is obfuscation type

        try {
            this.sendToIRC(`${nick}: Starting obfuscation...`);
            
            let dataToObfuscate;
            let inputType = 'text';

            // Handle different input types
            if (input.startsWith('http://') || input.startsWith('https://') || input.startsWith('ftp://')) {
                // Download from URL
                dataToObfuscate = await this.downloadFile(input);
                inputType = 'file';
                this.sendToIRC(`${nick}: File downloaded (${dataToObfuscate.length} bytes)`);
            } else if (input.startsWith('file:')) {
                // Read local file
                const filename = input.slice(5);
                const fs = require('fs').promises;
                const path = require('path');
                
                const uploadsDir = path.join(__dirname, '../../uploads');
                const filePath = path.join(uploadsDir, filename);
                
                try {
                    dataToObfuscate = await fs.readFile(filePath);
                    inputType = 'file';
                    this.sendToIRC(`${nick}: Local file read (${dataToObfuscate.length} bytes)`);
                } catch (error) {
                    this.sendToIRC(`${nick}: Local file not found: ${filename}`);
                    return;
                }
            } else {
                // Treat as text
                dataToObfuscate = input;
                inputType = 'text';
            }

            // Perform obfuscation using advanced crypto engine
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const cryptoModule = await engine.loadModule('advanced-crypto');
            const obfuscatedData = cryptoModule.obfuscateData(dataToObfuscate);
            
            // Save the obfuscated data
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `obfuscated_${obfuscationType}_${timestamp}.bin`;
            
            const fs = require('fs').promises;
            const path = require('path');
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const filePath = path.join(uploadsDir, filename);
            await fs.writeFile(filePath, obfuscatedData, 'binary');
            
            this.sendToIRC(`${nick}: Obfuscation completed!`);
            this.sendToIRC(`${nick}: Type: ${obfuscationType}`);
            this.sendToIRC(`${nick}: Input: ${inputType}`);
            this.sendToIRC(`${nick}: Original: ${dataToObfuscate.length} bytes`);
            this.sendToIRC(`${nick}: Obfuscated: ${obfuscatedData.length} bytes`);
            this.sendToIRC(`${nick}: File: ${filename}`);
            this.sendToIRC(`${nick}: Obfuscated file saved locally: ${filename}`);
            
        } catch (error) {
            logger.error('IRC Obfuscation error:', error);
            this.sendToIRC(`${nick}: Obfuscation failed: ${error.message}`);
        }
    }

    // Convert file extension handler - works with ALL encryption methods
    async handleConvertFile(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !convert <encrypted_filename> [new_extension]`);
            this.sendToIRC(`${nick}: Example: !convert encrypted_aes_256_gcm_2025-09-09T06-18-35-124Z.pdf.enc`);
            this.sendToIRC(`${nick}: Example: !convert encrypted_cam_256_cbc_2025-09-09T06-18-35-124Z.exe.enc .pdf`);
            this.sendToIRC(`${nick}: Example: !convert encrypted_aria_256_gcm_2025-09-09T06-18-35-124Z.zip.enc .dll`);
            return;
        }

        const encryptedFilename = args[0];
        const newExtension = args[1] || null;

        try {
            const fs = require('fs').promises;
            const path = require('path');
            
            const uploadsDir = path.join(__dirname, '../../uploads');
            const encryptedFilePath = path.join(uploadsDir, encryptedFilename);
            
            // Check if encrypted file exists
            try {
                await fs.access(encryptedFilePath);
            } catch (error) {
                this.sendToIRC(`${nick}: Encrypted file not found: ${encryptedFilename}`);
                return;
            }
            
            // Read the encrypted file
            const encryptedContent = await fs.readFile(encryptedFilePath, 'utf8');
            const encryptedData = JSON.parse(encryptedContent);
            
            // Validate that this is an encrypted file with proper structure
            if (!encryptedData.algorithm || !encryptedData.data) {
                this.sendToIRC(`${nick}: Invalid encrypted file format. File must contain algorithm and data fields.`);
                return;
            }
            
            // Extract original extension from filename or use provided extension
            let targetExtension;
            if (newExtension) {
                targetExtension = newExtension.startsWith('.') ? newExtension : '.' + newExtension;
            } else {
                // Extract original extension from the encrypted filename
                const match = encryptedFilename.match(/\.([^.]+)\.enc$/);
                if (match) {
                    targetExtension = '.' + match[1];
                } else {
                    targetExtension = '.enc';
                }
            }
            
            // Create new filename with target extension
            const baseFilename = encryptedFilename.replace(/\.enc$/, '');
            const newFilename = baseFilename + targetExtension;
            const newFilePath = path.join(uploadsDir, newFilename);
            
            // Create the converted file with original extension but encrypted content
            // Preserve all encryption metadata for proper decryption
            const convertedContent = JSON.stringify({
                ...encryptedData,
                convertedExtension: targetExtension,
                originalEncryptedFilename: encryptedFilename,
                conversionTimestamp: new Date().toISOString(),
                isConverted: true
            }, null, 2);
            
            await fs.writeFile(newFilePath, convertedContent, 'utf8');
            
            this.sendToIRC(`${nick}: File converted successfully!`);
            this.sendToIRC(`${nick}: Original: ${encryptedFilename}`);
            this.sendToIRC(`${nick}: Converted: ${newFilename}`);
            this.sendToIRC(`${nick}: Extension: ${targetExtension}`);
            this.sendToIRC(`${nick}: Algorithm: ${encryptedData.algorithm}`);
            this.sendToIRC(`${nick}: Key Length: ${encryptedData.key ? encryptedData.key.length : 'N/A'}`);
            this.sendToIRC(`${nick}: IV Length: ${encryptedData.iv ? encryptedData.iv.length : 'N/A'}`);
            this.sendToIRC(`${nick}: Note: File appears as ${targetExtension} but contains encrypted data`);
            this.sendToIRC(`${nick}: Use !decrypt with the same key/IV to decrypt the converted file`);
            this.sendToIRC(`${nick}: Supported for ALL encryption methods: AES, Camellia, ARIA, ChaCha20, RSA`);
            
        } catch (error) {
            logger.error('IRC Convert file error:', error);
            this.sendToIRC(`${nick}: Convert failed: ${error.message}`);
        }
    }

    // List files handler
    async handleListFiles(nick, channel) {
        try {
            const fs = require('fs').promises;
            const path = require('path');
            
            const uploadsDir = path.join(__dirname, '../../uploads');
            
            try {
                await fs.access(uploadsDir);
            } catch (error) {
                this.sendToIRC(`${nick}: No files found.`);
                return;
            }
            
            const files = await fs.readdir(uploadsDir);
            
            if (files.length === 0) {
                this.sendToIRC(`${nick}: No files found.`);
                return;
            }
            
            this.sendToIRC(`${nick}: Available files (${files.length}):`);
            
            for (const file of files.slice(0, 10)) { // Limit to 10 files
                const filePath = path.join(uploadsDir, file);
                const stats = await fs.stat(filePath);
                const size = stats.size;
                const modified = stats.mtime.toISOString().split('T')[0];
                
                this.sendToIRC(`${nick}: ${file} (${size} bytes, ${modified})`);
                this.sendToIRC(`${nick}: File available locally: ${file}`);
            }
            
            if (files.length > 10) {
                this.sendToIRC(`${nick}: ... and ${files.length - 10} more files`);
            }

        } catch (error) {
            logger.error('IRC List files error:', error);
            this.sendToIRC(`${nick}: Failed to list files: ${error.message}`);
        }
    }

    // Hot Patch handler
    async handleHotPatch(nick, channel, args) {
        if (args.length < 2) {
            this.sendToIRC(`${nick}: Usage: !hotpatch <target> <patch_data>`);
            this.sendToIRC(`${nick}: Example: !hotpatch process.exe "NOP 0x401000"`);
            this.sendToIRC(`${nick}: Example: !hotpatch memory 0x401000 "90 90 90"`);
            return;
        }

        const target = args[0];
        const patchData = args.slice(1).join(' ');

        try {
            this.sendToIRC(`${nick}: Applying hot patch...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.applyHotPatch(target, patchData);
            
            this.sendToIRC(`${nick}: Hot patch applied successfully!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Patch: ${patchData}`);
            this.sendToIRC(`${nick}: Result: ${result.status || 'Success'}`);
            
        } catch (error) {
            logger.error('IRC Hot patch error:', error);
            this.sendToIRC(`${nick}: Hot patch failed: ${error.message}`);
        }
    }

    // Polymorphic Engine handler
    async handlePolymorph(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !polymorph <code_or_file> [mutation_type]`);
            this.sendToIRC(`${nick}: Example: !polymorph "mov eax, 1" instruction-substitution`);
            this.sendToIRC(`${nick}: Example: !polymorph file:code.asm junk-code-injection`);
            this.sendToIRC(`${nick}: Available types: instruction-substitution, register-reallocation, code-reordering, junk-code-injection, control-flow-flattening, string-encryption`);
            return;
        }

        const input = args.slice(0, -1).join(' ');
        const mutationType = args[args.length - 1] || 'instruction-substitution';

        try {
            this.sendToIRC(`${nick}: Starting polymorphic transformation...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            let codeToTransform;
            let inputType = 'text';

            // Handle different input types
            if (input.startsWith('file:')) {
                const filename = input.slice(5);
                const fs = require('fs').promises;
                const path = require('path');
                
                const uploadsDir = path.join(__dirname, '../../uploads');
                const filePath = path.join(uploadsDir, filename);
                
                try {
                    codeToTransform = await fs.readFile(filePath, 'utf8');
                    inputType = 'file';
                    this.sendToIRC(`${nick}: File read (${codeToTransform.length} bytes)`);
                } catch (error) {
                    this.sendToIRC(`${nick}: File not found: ${filename}`);
                    return;
                }
            } else {
                codeToTransform = input;
                inputType = 'text';
            }

            const result = await engine.polymorphizeCode(codeToTransform, { mutationType });
            
            // Save the polymorphic result
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `polymorphic_${mutationType}_${timestamp}.asm`;
            
            const fs = require('fs').promises;
            const path = require('path');
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const filePath = path.join(uploadsDir, filename);
            await fs.writeFile(filePath, result.code, 'utf8');
            
            this.sendToIRC(`${nick}: Polymorphic transformation completed!`);
            this.sendToIRC(`${nick}: Type: ${mutationType}`);
            this.sendToIRC(`${nick}: Input: ${inputType}`);
            this.sendToIRC(`${nick}: Original: ${codeToTransform.length} bytes`);
            this.sendToIRC(`${nick}: Transformed: ${result.code.length} bytes`);
            this.sendToIRC(`${nick}: File: ${filename}`);
            this.sendToIRC(`${nick}: Mutations applied: ${result.mutations || 'Unknown'}`);
            
        } catch (error) {
            logger.error('IRC Polymorphic error:', error);
            this.sendToIRC(`${nick}: Polymorphic transformation failed: ${error.message}`);
        }
    }

    // Anti-Analysis handler
    async handleAntiAnalysis(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !antianalysis <target>`);
            this.sendToIRC(`${nick}: Example: !antianalysis process.exe`);
            this.sendToIRC(`${nick}: Example: !antianalysis file:malware.bin`);
            this.sendToIRC(`${nick}: Example: !antianalysis memory 0x401000`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Running anti-analysis detection...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.runAntiAnalysis(target);
            
            this.sendToIRC(`${nick}: Anti-analysis scan completed!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Analysis Type: ${result.type || 'General'}`);
            this.sendToIRC(`${nick}: Threats Detected: ${result.threats || 0}`);
            this.sendToIRC(`${nick}: Risk Level: ${result.riskLevel || 'Unknown'}`);
            
            if (result.detections && result.detections.length > 0) {
                this.sendToIRC(`${nick}: Detections:`);
                for (const detection of result.detections.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${detection.type}: ${detection.description}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC Anti-analysis error:', error);
            this.sendToIRC(`${nick}: Anti-analysis failed: ${error.message}`);
        }
    }

    // Reverse Engineering handler
    async handleReverseEngineering(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !reverse <target>`);
            this.sendToIRC(`${nick}: Example: !reverse file:binary.exe`);
            this.sendToIRC(`${nick}: Example: !reverse process.exe`);
            this.sendToIRC(`${nick}: Example: !reverse memory 0x401000`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Starting reverse engineering analysis...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.reverseEngineer(target);
            
            this.sendToIRC(`${nick}: Reverse engineering completed!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Architecture: ${result.architecture || 'Unknown'}`);
            this.sendToIRC(`${nick}: Entry Point: ${result.entryPoint || 'Unknown'}`);
            this.sendToIRC(`${nick}: Functions Found: ${result.functions || 0}`);
            this.sendToIRC(`${nick}: Strings Found: ${result.strings || 0}`);
            
            if (result.imports && result.imports.length > 0) {
                this.sendToIRC(`${nick}: Key Imports:`);
                for (const imp of result.imports.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${imp}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC Reverse engineering error:', error);
            this.sendToIRC(`${nick}: Reverse engineering failed: ${error.message}`);
        }
    }

    // Mobile Analysis handler
    async handleMobileAnalysis(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !mobile <target>`);
            this.sendToIRC(`${nick}: Example: !mobile file:app.apk`);
            this.sendToIRC(`${nick}: Example: !mobile file:app.ipa`);
            this.sendToIRC(`${nick}: Example: !mobile process:com.example.app`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Starting mobile analysis...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.analyzeMobile(target);
            
            this.sendToIRC(`${nick}: Mobile analysis completed!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Platform: ${result.platform || 'Unknown'}`);
            this.sendToIRC(`${nick}: Package: ${result.package || 'Unknown'}`);
            this.sendToIRC(`${nick}: Version: ${result.version || 'Unknown'}`);
            this.sendToIRC(`${nick}: Permissions: ${result.permissions || 0}`);
            
            if (result.permissions && result.permissions.length > 0) {
                this.sendToIRC(`${nick}: Key Permissions:`);
                for (const perm of result.permissions.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${perm}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC Mobile analysis error:', error);
            this.sendToIRC(`${nick}: Mobile analysis failed: ${error.message}`);
        }
    }

    // Network Analysis handler
    async handleNetworkAnalysis(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !network <target>`);
            this.sendToIRC(`${nick}: Example: !network 192.168.1.1`);
            this.sendToIRC(`${nick}: Example: !network example.com`);
            this.sendToIRC(`${nick}: Example: !network file:network.pcap`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Starting network analysis...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.analyzeNetwork(target);
            
            this.sendToIRC(`${nick}: Network analysis completed!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Type: ${result.type || 'Unknown'}`);
            this.sendToIRC(`${nick}: Status: ${result.status || 'Unknown'}`);
            this.sendToIRC(`${nick}: Ports Open: ${result.openPorts || 0}`);
            this.sendToIRC(`${nick}: Services: ${result.services || 0}`);
            
            if (result.services && result.services.length > 0) {
                this.sendToIRC(`${nick}: Services Found:`);
                for (const service of result.services.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${service.port}: ${service.name}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC Network analysis error:', error);
            this.sendToIRC(`${nick}: Network analysis failed: ${error.message}`);
        }
    }

    // Digital Forensics handler
    async handleDigitalForensics(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !forensics <target>`);
            this.sendToIRC(`${nick}: Example: !forensics file:disk.img`);
            this.sendToIRC(`${nick}: Example: !forensics file:memory.dmp`);
            this.sendToIRC(`${nick}: Example: !forensics process:malware.exe`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Starting digital forensics analysis...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.performForensics(target);
            
            this.sendToIRC(`${nick}: Digital forensics completed!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Type: ${result.type || 'Unknown'}`);
            this.sendToIRC(`${nick}: Evidence Found: ${result.evidence || 0}`);
            this.sendToIRC(`${nick}: Artifacts: ${result.artifacts || 0}`);
            this.sendToIRC(`${nick}: Timeline: ${result.timeline || 'Unknown'}`);
            
            if (result.artifacts && result.artifacts.length > 0) {
                this.sendToIRC(`${nick}: Key Artifacts:`);
                for (const artifact of result.artifacts.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${artifact.type}: ${artifact.description}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC Digital forensics error:', error);
            this.sendToIRC(`${nick}: Digital forensics failed: ${error.message}`);
        }
    }

    // Malware Analysis handler
    async handleMalwareAnalysis(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !malware <target>`);
            this.sendToIRC(`${nick}: Example: !malware file:suspicious.exe`);
            this.sendToIRC(`${nick}: Example: !malware file:malware.bin`);
            this.sendToIRC(`${nick}: Example: !malware process:malware.exe`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Starting malware analysis...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.analyzeMalware(target);
            
            this.sendToIRC(`${nick}: Malware analysis completed!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Family: ${result.family || 'Unknown'}`);
            this.sendToIRC(`${nick}: Type: ${result.type || 'Unknown'}`);
            this.sendToIRC(`${nick}: Risk Level: ${result.riskLevel || 'Unknown'}`);
            this.sendToIRC(`${nick}: Behaviors: ${result.behaviors || 0}`);
            
            if (result.behaviors && result.behaviors.length > 0) {
                this.sendToIRC(`${nick}: Key Behaviors:`);
                for (const behavior of result.behaviors.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${behavior.type}: ${behavior.description}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC Malware analysis error:', error);
            this.sendToIRC(`${nick}: Malware analysis failed: ${error.message}`);
        }
    }

    // Stealth Engine handler
    async handleStealth(nick, channel, args) {
        const mode = args[0] || 'full';

        try {
            this.sendToIRC(`${nick}: Enabling stealth mode...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.enableStealth(mode);
            
            this.sendToIRC(`${nick}: Stealth mode enabled!`);
            this.sendToIRC(`${nick}: Mode: ${mode}`);
            this.sendToIRC(`${nick}: Status: ${result.status || 'Active'}`);
            this.sendToIRC(`${nick}: Features: ${result.features || 'All enabled'}`);
            this.sendToIRC(`${nick}: Available modes: basic, standard, full, maximum`);
            
        } catch (error) {
            logger.error('IRC Stealth error:', error);
            this.sendToIRC(`${nick}: Stealth activation failed: ${error.message}`);
        }
    }

    // Memory Optimization handler
    async handleMemoryOptimization(nick, channel, args) {
        try {
            this.sendToIRC(`${nick}: Optimizing memory...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.optimizeMemory();
            
            this.sendToIRC(`${nick}: Memory optimization completed!`);
            this.sendToIRC(`${nick}: Status: ${result.status || 'Optimized'}`);
            this.sendToIRC(`${nick}: Memory freed: ${result.freed || 'Unknown'} bytes`);
            this.sendToIRC(`${nick}: Heap size: ${result.heapSize || 'Unknown'}`);
            this.sendToIRC(`${nick}: GC threshold: ${result.gcThreshold || 'Unknown'}%`);
            
        } catch (error) {
            logger.error('IRC Memory optimization error:', error);
            this.sendToIRC(`${nick}: Memory optimization failed: ${error.message}`);
        }
    }

    // Backup System handler
    async handleBackup(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !backup <target> [options]`);
            this.sendToIRC(`${nick}: Example: !backup file:important.txt`);
            this.sendToIRC(`${nick}: Example: !backup process:malware.exe`);
            this.sendToIRC(`${nick}: Example: !backup memory 0x401000`);
            return;
        }

        const target = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Creating backup...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.createBackup(target);
            
            this.sendToIRC(`${nick}: Backup created successfully!`);
            this.sendToIRC(`${nick}: Target: ${target}`);
            this.sendToIRC(`${nick}: Backup file: ${result.filename || 'Unknown'}`);
            this.sendToIRC(`${nick}: Size: ${result.size || 'Unknown'} bytes`);
            this.sendToIRC(`${nick}: Location: ${result.location || 'uploads/'}`);
            
        } catch (error) {
            logger.error('IRC Backup error:', error);
            this.sendToIRC(`${nick}: Backup failed: ${error.message}`);
        }
    }

    // Assembly handler
    async handleAssembly(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !assemble <code> [architecture]`);
            this.sendToIRC(`${nick}: Example: !assemble "mov eax, 1" x64`);
            this.sendToIRC(`${nick}: Example: !assemble "push ebp; mov ebp, esp" x86`);
            this.sendToIRC(`${nick}: Available architectures: x86, x64, arm, arm64`);
            return;
        }

        const code = args.slice(0, -1).join(' ');
        const architecture = args[args.length - 1] || 'x64';

        try {
            this.sendToIRC(`${nick}: Assembling code...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.assembleCode(code, architecture);
            
            // Save the assembled code
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `assembled_${architecture}_${timestamp}.bin`;
            
            const fs = require('fs').promises;
            const path = require('path');
            const uploadsDir = path.join(__dirname, '../../uploads');
            await fs.mkdir(uploadsDir, { recursive: true });
            
            const filePath = path.join(uploadsDir, filename);
            await fs.writeFile(filePath, result.binary, 'binary');
            
            this.sendToIRC(`${nick}: Assembly completed!`);
            this.sendToIRC(`${nick}: Architecture: ${architecture}`);
            this.sendToIRC(`${nick}: Code length: ${code.length} characters`);
            this.sendToIRC(`${nick}: Binary size: ${result.binary.length} bytes`);
            this.sendToIRC(`${nick}: File: ${filename}`);
            this.sendToIRC(`${nick}: Entry point: ${result.entryPoint || 'Unknown'}`);
            
        } catch (error) {
            logger.error('IRC Assembly error:', error);
            this.sendToIRC(`${nick}: Assembly failed: ${error.message}`);
        }
    }

    // Dual Generators handler
    async handleDualGenerators(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !dualgen <config>`);
            this.sendToIRC(`${nick}: Example: !dualgen "aes,camellia"`);
            this.sendToIRC(`${nick}: Example: !dualgen "chacha,rsa"`);
            this.sendToIRC(`${nick}: Available generators: aes, camellia, chacha, rsa, aria`);
            return;
        }

        const config = args.join(' ');

        try {
            this.sendToIRC(`${nick}: Running dual generators...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.runDualGenerators(config);
            
            this.sendToIRC(`${nick}: Dual generators completed!`);
            this.sendToIRC(`${nick}: Configuration: ${config}`);
            this.sendToIRC(`${nick}: Generators: ${result.generators || 'Unknown'}`);
            this.sendToIRC(`${nick}: Keys generated: ${result.keys || 0}`);
            this.sendToIRC(`${nick}: Status: ${result.status || 'Success'}`);
            
        } catch (error) {
            logger.error('IRC Dual generators error:', error);
            this.sendToIRC(`${nick}: Dual generators failed: ${error.message}`);
        }
    }

    // API Status handler
    async handleAPIStatus(nick, channel, args) {
        try {
            this.sendToIRC(`${nick}: Checking API status...`);
            
            const engine = require('../engines/rawrz-engine');
            await engine.initializeModules();
            
            const result = await engine.getAPIStatus();
            
            this.sendToIRC(`${nick}: API Status Report:`);
            this.sendToIRC(`${nick}: Overall: ${result.overall || 'Unknown'}`);
            this.sendToIRC(`${nick}: APIs Active: ${result.active || 0}`);
            this.sendToIRC(`${nick}: APIs Down: ${result.down || 0}`);
            this.sendToIRC(`${nick}: Response Time: ${result.responseTime || 'Unknown'}ms`);
            
            if (result.apis && result.apis.length > 0) {
                this.sendToIRC(`${nick}: API Details:`);
                for (const api of result.apis.slice(0, 3)) {
                    this.sendToIRC(`${nick}: - ${api.name}: ${api.status}`);
                }
            }
            
        } catch (error) {
            logger.error('IRC API status error:', error);
            this.sendToIRC(`${nick}: API status check failed: ${error.message}`);
        }
    }

    // Helper methods
    async downloadFile(url, maxRedirects = 5) {
        const https = require('https');
        const http = require('http');
        const urlModule = require('url');
        
        return new Promise((resolve, reject) => {
            const download = (currentUrl, redirectCount = 0) => {
                if (redirectCount > maxRedirects) {
                    reject(new Error('Too many redirects'));
                    return;
                }
                
                const parsedUrl = new urlModule.URL(currentUrl);
                const client = parsedUrl.protocol === 'https:' ? https : http;
                
                const req = client.get(currentUrl, (res) => {
                    // Handle redirects
                    if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                        const redirectUrl = new urlModule.URL(res.headers.location, currentUrl).href;
                        this.sendToIRC(`Following redirect to: ${redirectUrl}`);
                        download(redirectUrl, redirectCount + 1);
                        return;
                    }
                    
                    if (res.statusCode !== 200) {
                        reject(new Error(`HTTP ${res.statusCode}: ${res.statusMessage}`));
                        return;
                    }
                    
                    const chunks = [];
                    res.on('data', chunk => chunks.push(chunk));
                    res.on('end', () => {
                        const buffer = Buffer.concat(chunks);
                        
                        // Check file size limit (100MB)
                        const maxSize = 100 * 1024 * 1024;
                        if (buffer.length > maxSize) {
                            reject(new Error(`File too large: ${buffer.length} bytes (max: ${maxSize} bytes)`));
                            return;
                        }
                        
                        resolve(buffer);
                    });
                    res.on('error', reject);
                });
                
                req.on('error', reject);
                req.setTimeout(300000, () => { // 5 minutes
                    req.destroy();
                    reject(new Error('Download timeout'));
                });
            };
            
            download(url);
        });
    }

    async readLocalFile(filename) {
        const fs = require('fs').promises;
        const path = require('path');
        
        const uploadsDir = path.join(__dirname, '../../uploads');
        const filePath = path.join(uploadsDir, filename);
        
        const data = await fs.readFile(filePath);
        
        // Check file size limit (100MB)
        const maxSize = 100 * 1024 * 1024;
        if (data.length > maxSize) {
            throw new Error(`File too large: ${data.length} bytes (max: ${maxSize} bytes)`);
        }
        
        return data;
    }

    async saveEncryptedFile(result, algorithm, fileExtension = '.enc') {
        const fs = require('fs').promises;
        const path = require('path');
        
        const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
        const filename = `encrypted_${algorithm.replace(/[^a-zA-Z0-9]/g, '_')}_${timestamp}${fileExtension}`;
        
        const uploadsDir = path.join(__dirname, '../../uploads');
        await fs.mkdir(uploadsDir, { recursive: true });
        
        const filePath = path.join(uploadsDir, filename);
        const encryptedContent = JSON.stringify({
            algorithm: algorithm,
            data: result.data,
            key: result.key,
            iv: result.iv,
            authTag: result.authTag,
            metadata: result.metadata,
            timestamp: new Date().toISOString()
        }, null, 2);
        
        await fs.writeFile(filePath, encryptedContent, 'utf8');
        return filename;
    }

    // Database statistics handler
    async handleDatabaseStats(nick, channel) {
        try {
            const stats = await databaseIntegration.getSystemStats();
            
            if (!stats.enabled) {
                this.sendToIRC(`${nick}: Database integration is disabled`);
                this.sendToIRC(`${nick}: Connection Status: ${stats.connectionStatus}`);
                return;
            }
            
            this.sendToIRC(`${nick}: [DB] Database Integration: Active`);
            this.sendToIRC(`${nick}: [DB] Connection: ${stats.connectionStatus}`);
            this.sendToIRC(`${nick}: [DB] Total Operations: ${stats.stats.totalOperations}`);
            this.sendToIRC(`${nick}: [DB] Successful: ${stats.stats.successfulOperations} | Failed: ${stats.stats.failedOperations}`);
            
            if (stats.databaseStats) {
                this.sendToIRC(`${nick}: [DB] Encryptions: ${stats.databaseStats.totalEncryptions}`);
                this.sendToIRC(`${nick}: [DB] Stubs Generated: ${stats.databaseStats.totalStubs}`);
                this.sendToIRC(`${nick}: [DB] Polymorphic Ops: ${stats.databaseStats.totalPolymorphic}`);
                this.sendToIRC(`${nick}: [DB] Commands Logged: ${stats.databaseStats.totalCommands}`);
                this.sendToIRC(`${nick}: [DB] Last 24h: ${stats.databaseStats.last24Hours.commands} commands`);
            }
            
        } catch (error) {
            this.sendToIRC(`${nick}: Failed to get database stats: ${error.message}`);
        }
    }

    // System status methods (keeping existing functionality)
    sendSystemStatus() {
        const status = chatterbox.getStatusReport();
        this.sendToIRC(`[STATUS] System Health: ${status.health.status}`);
        this.sendToIRC(`[STATUS] Active Scripts: ${status.activeScripts} | Errors: ${status.recentErrors.length} | Stuck: ${status.stuckScripts}`);
        this.sendToIRC(`[HEARTBEAT] Monitoring: ${chatterbox.heartbeatInterval ? 'Active' : 'Inactive'}`);
    }

    sendScriptStatus() {
        const scripts = chatterbox.getAllScripts();
        this.sendToIRC(`[SCRIPTS] Active Scripts (${scripts.length}):`);
        for (const script of scripts.slice(0, 5)) {
            this.sendToIRC(`  [CHAR] ${script.name}: ${script.status}`);
        }
    }

    sendRecentErrors() {
        const errors = chatterbox.getRecentErrors();
        this.sendToIRC(`[ERRORS] Recent Errors (${errors.length}):`);
        for (const error of errors.slice(0, 3)) {
            this.sendToIRC(`  [CHAR] ${error.scriptName}: ${error.error}`);
        }
    }

    sendStuckScripts() {
        const stuck = chatterbox.getStuckScripts();
        this.sendToIRC(`[STUCK] Stuck Scripts (${stuck.length}):`);
        for (const script of stuck.slice(0, 3)) {
            this.sendToIRC(`  [CHAR] ${script.name}: ${script.status} (${Math.round((Date.now() - script.lastHeartbeat) / 1000)}s)`);
        }
    }

    sendRequestIdErrors() {
        const requestIdErrors = chatterbox.getRequestIdErrors();
        this.sendToIRC(`[ALERT] RequestID Errors (${requestIdErrors.length}):`);
        for (const [requestId, errors] of requestIdErrors.slice(0, 3)) {
            const latestError = errors[errors.length - 1];
            const timeAgo = Math.round((Date.now() - latestError.timestamp) / 1000);
            this.sendToIRC(`  [CHAR] ${requestId}: ${latestError.error} (${timeAgo}s ago)`);
        }
    }

    sendHelp(category = null) {
        if (!category || category.length === 0) {
            this.sendToIRC('[BOT] RawrZ Security Platform - 34 Commands Available');
            this.sendToIRC('[CORE] !encrypt !decrypt !algorithms !upload !files !convert !simpleenc');
            this.sendToIRC('[ADVANCED] !stub !compile !compress !obfuscate !hotpatch !polymorph');
            this.sendToIRC('[ANALYSIS] !antianalysis !reverse !mobile !network !forensics !malware');
            this.sendToIRC('[SYSTEM] !stealth !memory !backup !assemble !dualgen !apistatus');
            this.sendToIRC('[MONITOR] !status !scripts !errors !stuck !requestid !dbstats');
            this.sendToIRC('[HELP] Use !help <category> for details (core/advanced/analysis/system/monitor)');
            return;
        }

        const cat = category[0].toLowerCase();
        
        switch (cat) {
            case 'core':
                this.sendToIRC('[CORE COMMANDS]');
                this.sendToIRC('  !encrypt <alg> <data> - Encrypt files/text');
                this.sendToIRC('  !decrypt <alg> <data> <key> <iv> - Decrypt data');
                this.sendToIRC('  !algorithms - List encryption algorithms');
                this.sendToIRC('  !upload <file> <base64> - Upload files');
                this.sendToIRC('  !files - List available files');
                this.sendToIRC('  !convert <file> [ext] - Convert file extensions');
                this.sendToIRC('  !simpleenc <alg> <data> <ext> - Encrypt & convert');
                break;
                
            case 'advanced':
                this.sendToIRC('[ADVANCED COMMANDS]');
                this.sendToIRC('  !stub <alg> <target> <type> - Generate encrypted stubs');
                this.sendToIRC('  !compile <cpp_file> - Compile C++ to executable');
                this.sendToIRC('  !compress <alg> <data> - Compress files');
                this.sendToIRC('  !obfuscate <data> [type] - Obfuscate data');
                this.sendToIRC('  !hotpatch <target> <patch> - Apply hot patches');
                this.sendToIRC('  !polymorph <code> [type] - Polymorphic transformation');
                break;
                
            case 'analysis':
                this.sendToIRC('[ANALYSIS COMMANDS]');
                this.sendToIRC('  !antianalysis <target> - Anti-analysis detection');
                this.sendToIRC('  !reverse <target> - Reverse engineering');
                this.sendToIRC('  !mobile <target> - Mobile app analysis');
                this.sendToIRC('  !network <target> - Network analysis');
                this.sendToIRC('  !forensics <target> - Digital forensics');
                this.sendToIRC('  !malware <target> - Malware analysis');
                break;
                
            case 'system':
                this.sendToIRC('[SYSTEM COMMANDS]');
                this.sendToIRC('  !stealth [mode] - Enable stealth modes');
                this.sendToIRC('  !memory - Memory optimization');
                this.sendToIRC('  !backup <target> - Create backups');
                this.sendToIRC('  !assemble <code> [arch] - Assemble code');
                this.sendToIRC('  !dualgen <config> - Dual crypto generators');
                this.sendToIRC('  !apistatus - API status check');
                break;
                
            case 'monitor':
                this.sendToIRC('[MONITOR COMMANDS]');
                this.sendToIRC('  !status - System health status');
                this.sendToIRC('  !scripts - Active scripts monitoring');
                this.sendToIRC('  !errors - Recent error reports');
                this.sendToIRC('  !stuck - Stuck scripts detection');
                this.sendToIRC('  !requestid - RequestID error tracking');
                this.sendToIRC('  !dbstats - Database statistics and health');
                break;
                
            default:
                this.sendToIRC(`Unknown category: ${cat}. Use: core, advanced, analysis, system, monitor`);
        }
    }

    async sendToIRC(message) {
        if (!this.connected || !this.socket) {
            this.messageQueue.push(message);
            return;
        }

        // Rate limiting to prevent flooding
        const now = Date.now();
        const timeSinceLastMessage = now - this.lastMessageTime;
        
        if (timeSinceLastMessage < this.rateLimitDelay) {
            const delay = this.rateLimitDelay - timeSinceLastMessage;
            await new Promise(resolve => setTimeout(resolve, delay));
        }
        
        this.lastMessageTime = Date.now();

        for (const channel of this.channels) {
            this.sendRaw(`PRIVMSG ${channel} :${message}`);
        }
    }

    sendRaw(command) {
        if (this.socket && this.connected) {
            this.socket.write(command + '\r\n');
        }
    }

    reconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            logger.error('[BOT] Max reconnection attempts reached, giving up');
            return;
        }

        this.reconnectAttempts++;
        const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts), 30000);
        
        logger.info(`[BOT] Reconnecting to IRC in ${delay}ms (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
        
        setTimeout(() => {
            this.connect();
        }, delay);
    }

    // Compile C++ stub to executable using native-roslyn
    async handleCompileStub(nick, channel, args) {
        if (args.length < 1) {
            this.sendToIRC(`${nick}: Usage: !compile <cpp_filename>`);
            this.sendToIRC(`${nick}: Example: !compile data_aes-256-gcm_stub.cpp`);
            this.sendToIRC(`${nick}: This compiles C++ stubs to native executables using Clang/LLVM`);
            return;
        }

        const cppFilename = args[0];
        
        try {
            const fs = require('fs').promises;
            const path = require('path');
            const { exec } = require('child_process');
            const { promisify } = require('util');
            const execAsync = promisify(exec);
            
            const uploadsDir = path.join(__dirname, '../../uploads');
            const cppFilePath = path.join(uploadsDir, cppFilename);
            
            // Check if C++ file exists
            try {
                await fs.access(cppFilePath);
            } catch (error) {
                this.sendToIRC(`${nick}: C++ file not found: ${cppFilename}`);
                return;
            }
            
            this.sendToIRC(`${nick}: Compiling ${cppFilename} to native executable...`);
            
            // Read the C++ source
            const cppSource = await fs.readFile(cppFilePath, 'utf8');
            
            // Check if native-roslyn container is running
            try {
                await execAsync('docker ps --filter "name=native-build" --format "{{.Names}}"');
            } catch (error) {
                this.sendToIRC(`${nick}: Native-roslyn container not found. Please start it with:`);
                this.sendToIRC(`${nick}: docker run -d --name native-build -p 8080:8080 native-roslyn`);
                return;
            }
            
            // Compile using native-roslyn container
            const outputFilename = cppFilename.replace('.cpp', '.exe');
            const outputPath = path.join(uploadsDir, outputFilename);
            
            // Pipe C++ source to native-roslyn container and save executable
            const compileCommand = `echo "${cppSource.replace(/"/g, '\\"')}" | docker exec -i native-build /usr/local/bin/compile.sh > "${outputPath}"`;
            
            await execAsync(compileCommand);
            
            // Check if compilation was successful
            try {
                await fs.access(outputPath);
                const stats = await fs.stat(outputPath);
                
                this.sendToIRC(`${nick}: ✅ Compilation successful!`);
                this.sendToIRC(`${nick}: 📁 Executable saved: ${outputFilename}`);
                this.sendToIRC(`${nick}: 📊 Size: ${(stats.size / 1024).toFixed(2)} KB`);
                this.sendToIRC(`${nick}: 🚀 Ready to execute: ${outputFilename}`);
                
            } catch (error) {
                this.sendToIRC(`${nick}: ❌ Compilation failed. Check C++ syntax and dependencies.`);
                this.sendToIRC(`${nick}: 💡 Make sure your C++ code is valid and includes necessary headers.`);
            }
            
        } catch (error) {
            this.sendToIRC(`${nick}: ❌ Compilation error: ${error.message}`);
            logger.error('Compilation error:', error);
        }
    }

    startPeriodicUpdates() {
        setInterval(() => {
            if (this.connected) {
                const status = chatterbox.getStatusReport();
                const health = status.health;
                
                if (health.status !== 'healthy' || status.stuckScripts > 0 || status.recentErrors.length > 0) {
                    this.sendSystemStatus();
                }
            }
        }, 300000); // Every 5 minutes
    }

    addChannel(channel) {
        if (!channel.startsWith('#')) {
            channel = '#' + channel;
        }
        
        this.channels.add(channel);
        if (this.connected) {
            this.sendRaw(`JOIN ${channel}`);
            logger.info(`[BOT] IRC: Added channel ${channel}`);
        }
    }

    removeChannel(channel) {
        if (!channel.startsWith('#')) {
            channel = '#' + channel;
        }
        
        this.channels.delete(channel);
        if (this.connected) {
            this.sendRaw(`PART ${channel}`);
            logger.info(`[BOT] IRC: Removed channel ${channel}`);
        }
    }

    disconnect() {
        if (this.connected && this.socket) {
            this.sendToIRC('[BOT] RawrZ Monitor disconnecting...');
            this.sendRaw('QUIT :RawrZ Security Platform Monitor shutting down');
            this.socket.end();
        }
    }
}

// Create IRC bot instance
const ircBot = new IRCBot({
    server: process.env.IRC_SERVER || 'irc.rizon.net',
    port: parseInt(process.env.IRC_PORT) || 6667,
    channels: (process.env.IRC_CHANNELS || '#rawr').split(','),
    nick: process.env.IRC_NICK || 'RawrZBot',
    username: process.env.IRC_USERNAME || 'bibbles11',
    realname: process.env.IRC_REALNAME || 'RawrZ Security Platform Monitor',
    password: process.env.IRC_PASSWORD || 'bibbles11'
});

// Start periodic updates
ircBot.startPeriodicUpdates();

// Graceful shutdown
process.on('SIGTERM', () => {
    ircBot.disconnect();
});

process.on('SIGINT', () => {
    ircBot.disconnect();
});

module.exports = {
    IRCBot,
    ircBot
};

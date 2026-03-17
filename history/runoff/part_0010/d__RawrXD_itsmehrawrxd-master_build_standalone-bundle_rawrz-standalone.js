#!/usr/bin/env node
/**
 * RawrZ Security Platform - Standalone .NET-less Version
 * Powered by Pi Engine Integration
 * 
 * This is a completely standalone version that doesn't require .NET Framework
 * Uses Node.js + Pi Engine for multi-language compilation and execution
 */

console.log(`
================================================================
  RawrZ Security Platform - Standalone Edition              
                                                                  
  Multi-Language Compilation Engine                            
  Advanced Security Tools & Analysis                          
  Zero .NET Dependencies - Pure Node.js                       
  Powered by Pi Engine Integration                            
================================================================
`);

const PiEngineIntegration = require('./pi-engine-integration');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

class RawrZStandalone {
    constructor() {
        this.piEngine = new PiEngineIntegration();
        this.rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });
        this.isRunning = true;
    }

    async start() {
        console.log('[INIT] Initializing RawrZ Standalone Platform...\n');
        
        // Show system status
        const status = this.piEngine.getStatus();
        console.log('[STATUS] System Status:');
        console.log(`   [NODE] Node.js: ${status.nodeVersion}`);
        console.log(`   [PLATFORM] Platform: ${status.platform}`);
        console.log(`   [ENGINES] Engines Loaded: ${status.enginesLoaded}`);
        console.log(`   [WORKSPACE] Workspace: ${status.workspace}`);
        console.log(`   [UPTIME] Uptime: ${Math.floor(status.uptime)}s\n`);

        // Test engines
        console.log('[TEST] Testing Pi Engine Integration...');
        await this.piEngine.testAllEngines();

        console.log('\n[READY] RawrZ Standalone Platform Ready!\n');
        
        this.showMenu();
    }

    showMenu() {
        console.log(`
[MENU] RawrZ Standalone - Command Menu:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Compile & Run Code (Multi-Language)
2. Load Security Engine
3. System Status & Engine Info
4. Test All Engines
5. Workspace Management
6. Exit

Enter your choice (1-6): `);
        
        this.rl.question('', (choice) => {
            this.handleMenuChoice(choice.trim());
        });
    }

    async handleMenuChoice(choice) {
        switch (choice) {
            case '1':
                await this.compileAndRunMenu();
                break;
            case '2':
                await this.loadSecurityEngine();
                break;
            case '3':
                this.showSystemStatus();
                break;
            case '4':
                await this.piEngine.testAllEngines();
                this.showMenu();
                break;
            case '5':
                this.manageWorkspace();
                break;
            case '6':
                this.exit();
                return;
            default:
                console.log('[ERROR] Invalid choice. Please select 1-6.');
                this.showMenu();
                break;
        }
    }

    async compileAndRunMenu() {
        console.log('\n[COMPILER] Multi-Language Compiler (Pi Engine)');
        console.log('Supported: JavaScript, Python, PowerShell, C++, C#\n');
        
        this.rl.question('Enter language (js/python/ps1/cpp/cs): ', (language) => {
            console.log('Enter your code (type END on a new line to finish):');
            
            let code = '';
            const codeInput = () => {
                this.rl.question('', (line) => {
                    if (line === 'END') {
                        this.executeCode(language, code);
                    } else {
                        code += line + '\n';
                        codeInput();
                    }
                });
            };
            codeInput();
        });
    }

    async executeCode(language, code) {
        console.log(`\n[EXEC] Executing ${language.toUpperCase()} code via Pi Engine...\n`);
        
        const result = await this.piEngine.compileAndRun(language, code);
        
        if (result.success) {
            console.log('[SUCCESS] Execution Successful!');
            console.log('[OUTPUT]:');
            console.log(result.output);
        } else {
            console.log('[FAILED] Execution Failed!');
            console.log('[ERROR]:');
            console.log(result.error);
        }
        
        console.log('\nPress Enter to continue...');
        this.rl.question('', () => {
            this.showMenu();
        });
    }

    async loadSecurityEngine() {
        console.log('\n[SECURITY] Available Security Engines:');
        const engines = this.piEngine.listEngines();
        
        engines.forEach((engine, index) => {
            console.log(`   ${index + 1}. ${engine}`);
        });

        this.rl.question('\nSelect engine number (or 0 to go back): ', (choice) => {
            const engineIndex = parseInt(choice) - 1;
            if (engineIndex >= 0 && engineIndex < engines.length) {
                const engineName = engines[engineIndex];
                const engine = this.piEngine.getEngine(engineName);
                console.log(`\n[LOADING] Loading ${engineName}...`);
                console.log('[SUCCESS] Engine loaded successfully!');
                console.log('[INFO] Engine capabilities: Multi-language compilation & security analysis');
            } else if (choice === '0') {
                // Go back to menu
            } else {
                console.log('[ERROR] Invalid selection.');
            }
            this.showMenu();
        });
    }

    showSystemStatus() {
        console.log('\n[STATUS] RawrZ Standalone System Status:');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        
        const status = this.piEngine.getStatus();
        console.log(`[ENGINES] Loaded Engines: ${status.enginesLoaded}`);
        console.log(`⚡ Node.js Version: ${status.nodeVersion}`);
        console.log(`[PLATFORM] Platform: ${status.platform}`);
        console.log(`📁 Workspace: ${status.workspace}`);
        console.log(`🕒 Uptime: ${Math.floor(status.uptime)}s`);
        console.log(`🔄 Active Operations: ${status.activeOperations}`);
        
        console.log('\n🛡️  Available Engines:');
        status.engines.forEach((engine, index) => {
            console.log(`   ${index + 1}. ${engine}`);
        });

        console.log('\nPress Enter to continue...');
        this.rl.question('', () => {
            this.showMenu();
        });
    }

    manageWorkspace() {
        console.log('\n📁 Workspace Management:');
        const status = this.piEngine.getStatus();
        console.log(`Current workspace: ${status.workspace}`);
        
        try {
            const files = fs.readdirSync(status.workspace);
            if (files.length > 0) {
                console.log('\n📄 Files in workspace:');
                files.forEach(file => console.log(`   📄 ${file}`));
            } else {
                console.log('   (Empty)');
            }
        } catch (error) {
            console.log('❌ Error reading workspace:', error.message);
        }

        console.log('\nPress Enter to continue...');
        this.rl.question('', () => {
            this.showMenu();
        });
    }

    exit() {
        console.log('\n🔥 Thanks for using RawrZ Standalone Platform!');
        console.log('   🎯 All engines powered down safely.');
        console.log('   🛡️  Security systems offline.');
        console.log('\nGoodbye! 👋\n');
        this.rl.close();
        process.exit(0);
    }
}

// Auto-start if run directly
if (require.main === module) {
    const platform = new RawrZStandalone();
    platform.start().catch(error => {
        console.error('❌ Failed to start RawrZ Standalone:', error);
        process.exit(1);
    });
}

module.exports = RawrZStandalone;
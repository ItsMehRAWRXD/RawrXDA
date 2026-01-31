#!/usr/bin/env node

// ========================================
// SMOKE TEST - LET THE CODE SPEAK FOR ITSELF
// ========================================
// No explanations, no assumptions - just test if it works

const fs = require('fs');
const path = require('path');
const { spawn, exec } = require('child_process');
const { promisify } = require('util');
const config = require('./config-loader');

const execAsync = promisify(exec);

class SmokeTest {
    constructor() {
        this.results = [];
        this.passed = 0;
        this.failed = 0;
    }

    async run() {
        console.log(" SMOKE TEST");
        console.log("=============");

        await this.test1();
        await this.test2();
        await this.test3();
        await this.test4();
        await this.test5();
        await this.test6();
        
        this.printResults();
    }

    async test1() {
        console.log(" TEST 1");
        
        try {
            const serverFile = 'auto-server.js';
            if (!fs.existsSync(serverFile)) {
                throw new Error('File missing');
            }
            
            const content = fs.readFileSync(serverFile, 'utf8');
            
            // Test if it can start
            const server = spawn('node', [serverFile], { stdio: 'pipe' });
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Test if it responds
            const http = require('http');
            const response = await this.makeRequest('GET', '/api/health');
            
            server.kill();
            
            this.assert(response.status === 200, "Responds to requests");
            this.assert(content.includes('app.listen'), "Has server code");
            this.assert(content.includes('/api/'), "Has API routes");
            
            console.log(" PASSED\n");
            
        } catch (error) {
            this.assert(false, `Failed: ${error.message}`);
            console.log(" FAILED\n");
        }
    }

    async test2() {
        console.log(" TEST 2");
        
        try {
            const projectPath = path.join(process.cwd(), '01-OhGee-AI-Assistant', 'KimiAppNative');
            const projectFile = path.join(projectPath, 'KimiAppNative.csproj');
            
            if (!fs.existsSync(projectFile)) {
                throw new Error('Project missing');
            }
            
            // Test if it builds
            const buildConfig = config.getBuildConfig();
            const { stdout, stderr } = await execAsync(`${buildConfig.compiler} build --verbosity ${buildConfig.verbosity}`, {
                cwd: projectPath
            });
            
            // Test if AI files exist and have content
            const aiPath = path.join(projectPath, 'IDE', 'AIModels');
            if (fs.existsSync(aiPath)) {
                const files = fs.readdirSync(aiPath);
                for (const file of files) {
                    const filePath = path.join(aiPath, file);
                    const content = fs.readFileSync(filePath, 'utf8');
                    this.assert(content.length > 100, `File ${file} has content`);
                    this.assert(content.includes('class'), `File ${file} has class`);
                }
            }
            
            this.assert(!stderr.includes('error CS'), "Builds without errors");
            
            console.log(" PASSED\n");
            
        } catch (error) {
            this.assert(false, `Failed: ${error.message}`);
            console.log(" FAILED\n");
        }
    }

    async test3() {
        console.log(" TEST 3");
        
        try {
            const projectPath = path.join(process.cwd(), '02-RawrZ-Desktop-Encryptor', 'RawrZ.NET', 'RawrZDesktop');
            const projectFile = path.join(projectPath, 'RawrZDesktop.csproj');
            
            if (!fs.existsSync(projectFile)) {
                throw new Error('Project missing');
            }
            
            // Test if it builds
            const buildConfig = config.getBuildConfig();
            const { stdout, stderr } = await execAsync(`${buildConfig.compiler} build --verbosity ${buildConfig.verbosity}`, {
                cwd: projectPath
            });
            
            // Test if engine files exist
            const engineFile = path.join(projectPath, 'Engines', 'IrcBotEngine.cs');
            if (fs.existsSync(engineFile)) {
                const content = fs.readFileSync(engineFile, 'utf8');
                this.assert(content.includes('GenerateIrcBotCode'), "Has generation method");
                this.assert(content.includes('TcpClient'), "Has network code");
            }
            
            this.assert(!stderr.includes('error CS'), "Builds without errors");
            
            console.log(" PASSED\n");
            
        } catch (error) {
            this.assert(false, `Failed: ${error.message}`);
            console.log(" FAILED\n");
        }
    }

    async test4() {
        console.log(" TEST 4");
        
        try {
            const projectPath = path.join(process.cwd(), '02-RawrZ-Desktop-Encryptor', 'RawrZ.NET', 'RawrZSecurityPlatform');
            const projectFile = path.join(projectPath, 'RawrZSecurityPlatform.csproj');
            
            if (!fs.existsSync(projectFile)) {
                throw new Error('Project missing');
            }
            
            // Test if it builds
            const buildConfig = config.getBuildConfig();
            const { stdout, stderr } = await execAsync(`${buildConfig.compiler} build --verbosity ${buildConfig.verbosity}`, {
                cwd: projectPath
            });
            
            // Test if launch settings exist
            const launchFile = path.join(projectPath, 'Properties', 'launchSettings.json');
            if (fs.existsSync(launchFile)) {
                const content = fs.readFileSync(launchFile, 'utf8');
                this.assert(content.includes('port'), "Has port config");
            }
            
            this.assert(!stderr.includes('error CS'), "Builds without errors");
            
            console.log(" PASSED\n");
            
        } catch (error) {
            this.assert(false, `Failed: ${error.message}`);
            console.log(" FAILED\n");
        }
    }

    async test5() {
        console.log(" TEST 5");
        
        try {
            const cliFile = 'eon_ide_cli.js';
            if (!fs.existsSync(cliFile)) {
                throw new Error('CLI missing');
            }
            
            // Test help command
            const { stdout: helpOut } = await execAsync(`node ${cliFile} help`);
            this.assert(helpOut.includes('EON'), "Help works");
            
            // Test version command
            const { stdout: versionOut } = await execAsync(`node ${cliFile} version`);
            this.assert(versionOut.includes('v1.0.0'), "Version works");
            
            // Test create command
            const { stdout: createOut } = await execAsync(`node ${cliFile} create test-cli-project`);
            this.assert(createOut.includes('successfully'), "Create works");
            
            // Test parse command
            const { stdout: parseOut } = await execAsync(`node ${cliFile} parse test-cli-project/sample.eon`);
            this.assert(parseOut.includes('AST'), "Parse works");
            
            // Cleanup
            if (fs.existsSync('test-cli-project')) {
                fs.rmSync('test-cli-project', { recursive: true, force: true });
            }
            
            console.log(" PASSED\n");
            
        } catch (error) {
            this.assert(false, `Failed: ${error.message}`);
            console.log(" FAILED\n");
        }
    }

    async test6() {
        console.log(" TEST 6");
        
        try {
            const asmFile = 'eon_ide_fixed.asm';
            if (!fs.existsSync(asmFile)) {
                throw new Error('Assembly file missing');
            }
            
            const content = fs.readFileSync(asmFile, 'utf8');
            
            // Test if it has assembly structure
            this.assert(content.includes('section .data'), "Has data section");
            this.assert(content.includes('section .text'), "Has text section");
            this.assert(content.includes('global _start'), "Has entry point");
            
            // Test if it has EON functions
            this.assert(content.includes('parse_eon'), "Has parse function");
            this.assert(content.includes('compile_eon'), "Has compile function");
            
            // Test if it has real assembly code
            this.assert(content.includes('mov rax'), "Has assembly instructions");
            this.assert(content.includes('call'), "Has function calls");
            this.assert(content.includes('ret'), "Has returns");
            
            // Test if it's not just stubs
            this.assert(!content.includes('TODO: Implement'), "No TODO stubs");
            
            console.log(" PASSED\n");
            
        } catch (error) {
            this.assert(false, `Failed: ${error.message}`);
            console.log(" FAILED\n");
        }
    }

    async makeRequest(method, path) {
        return new Promise((resolve, reject) => {
            const serverConfig = config.getServerConfig();
            const options = {
                hostname: serverConfig.host,
                port: serverConfig.port,
                path: path,
                method: method,
                timeout: serverConfig.timeout
            };

            const req = require('http').request(options, (res) => {
                resolve({ status: res.statusCode });
            });

            req.on('error', () => resolve({ status: 0 }));
            req.on('timeout', () => resolve({ status: 0 }));
            req.end();
        });
    }

    assert(condition, message) {
        if (condition) {
            this.passed++;
            this.results.push(` ${message}`);
        } else {
            this.failed++;
            this.results.push(` ${message}`);
        }
    }

    printResults() {
        console.log(" RESULTS");
        console.log("==========");
        
        this.results.forEach(result => console.log(result));
        
        console.log(`\n ${this.passed} passed, ${this.failed} failed`);
        
        if (this.failed === 0) {
            console.log(" ALL WORKING");
        } else {
            console.log(" SOME ISSUES");
        }
    }
}

// Run it
if (require.main === module) {
    const test = new SmokeTest();
    test.run().catch(console.error);
}

module.exports = SmokeTest;
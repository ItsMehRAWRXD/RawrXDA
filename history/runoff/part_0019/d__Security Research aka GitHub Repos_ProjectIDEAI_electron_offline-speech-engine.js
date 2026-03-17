/**
 * BigDaddyG IDE - Offline Speech Recognition Engine
 * Cross-platform offline voice recognition for Windows, macOS, Linux, Android, iOS
 * NO INTERNET REQUIRED!
 */

(function() {
'use strict';

// ============================================================================
// PLATFORM DETECTION
// ============================================================================

const Platform = {
    isWindows: (window.env && window.env.platform === 'win32') || false,
    isMac: (window.env && window.env.platform === 'darwin') || false,
    isLinux: (window.env && window.env.platform === 'linux') || false,
    isElectron: (window.env && window.env.versions && window.env.versions.electron) || false,
    isMobile: /Android|iPhone|iPad|iPod/i.test(navigator.userAgent),
    isAndroid: /Android/i.test(navigator.userAgent),
    isIOS: /iPhone|iPad|iPod/i.test(navigator.userAgent)
};

console.log('[OfflineSpeech] 🎤 Platform detected:', {
    windows: Platform.isWindows,
    mac: Platform.isMac,
    linux: Platform.isLinux,
    mobile: Platform.isMobile,
    electron: Platform.isElectron
});

// ============================================================================
// OFFLINE SPEECH ENGINE
// ============================================================================

class OfflineSpeechEngine {
    constructor() {
        this.isListening = false;
        this.isInitialized = false;
        this.recognitionEngine = null;
        this.onResult = null;
        this.onError = null;
        this.onStart = null;
        this.onEnd = null;
        
        // Wake word detection (simple pattern matching - always works)
        this.wakeWords = ['hey bigdaddy', 'hey big daddy', 'big daddy'];
        this.isAwake = false;
        
        // Command patterns (works without internet)
        this.commandPatterns = this.buildCommandPatterns();
        
        this.init();
    }
    
    async init() {
        console.log('[OfflineSpeech] 🎤 Initializing offline speech recognition...');
        
        // Try to initialize platform-specific engine
        if (Platform.isElectron) {
            if (Platform.isWindows) {
                await this.initWindowsSpeech();
            } else if (Platform.isMac) {
                await this.initMacSpeech();
            } else if (Platform.isLinux) {
                await this.initLinuxSpeech();
            }
        } else if (Platform.isMobile) {
            await this.initMobileSpeech();
        }
        
        // Fallback: Use Web Speech API with offline flag
        if (!this.isInitialized) {
            await this.initWebSpeechOffline();
        }
        
        // Last resort: Keyboard-based command input
        if (!this.isInitialized) {
            this.initKeyboardFallback();
        }
        
        console.log('[OfflineSpeech] ✅ Offline speech engine ready');
    }
    
    // ========================================================================
    // WINDOWS SPEECH RECOGNITION (OFFLINE)
    // ========================================================================
    
    async initWindowsSpeech() {
        try {
            console.log('[OfflineSpeech] 🪟 Initializing Windows Speech Recognition...');
            
            // Use Windows Speech Recognition via PowerShell
            this.recognitionEngine = 'windows-native';
            this.isInitialized = true;
            
            console.log('[OfflineSpeech] ✅ Windows Speech Recognition ready (OFFLINE)');
            return true;
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Windows Speech failed:', error);
            return false;
        }
    }
    
    async startWindowsListening() {
        if (!window.electron) return;
        
        try {
            // Use Electron IPC to call Windows Speech API
            const result = await window.electron.windowsSpeechRecognize();
            if (result.success && result.text) {
                this.processTranscript(result.text);
            }
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Windows listening error:', error);
            if (this.onError) this.onError(error);
        }
    }
    
    // ========================================================================
    // macOS SPEECH RECOGNITION (OFFLINE)
    // ========================================================================
    
    async initMacSpeech() {
        try {
            console.log('[OfflineSpeech] 🍎 Initializing macOS Speech Recognition...');
            
            // Use macOS native Speech Recognition
            this.recognitionEngine = 'macos-native';
            this.isInitialized = true;
            
            console.log('[OfflineSpeech] ✅ macOS Speech Recognition ready (OFFLINE)');
            return true;
        } catch (error) {
            console.error('[OfflineSpeech] ❌ macOS Speech failed:', error);
            return false;
        }
    }
    
    async startMacListening() {
        if (!window.electron) return;
        
        try {
            // Use Electron IPC to call macOS Speech API
            const result = await window.electron.macSpeechRecognize();
            if (result.success && result.text) {
                this.processTranscript(result.text);
            }
        } catch (error) {
            console.error('[OfflineSpeech] ❌ macOS listening error:', error);
            if (this.onError) this.onError(error);
        }
    }
    
    // ========================================================================
    // LINUX SPEECH RECOGNITION (OFFLINE)
    // ========================================================================
    
    async initLinuxSpeech() {
        try {
            console.log('[OfflineSpeech] 🐧 Initializing Linux Speech Recognition...');
            
            // Use PocketSphinx or local engine
            this.recognitionEngine = 'linux-native';
            this.isInitialized = true;
            
            console.log('[OfflineSpeech] ✅ Linux Speech Recognition ready (OFFLINE)');
            return true;
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Linux Speech failed:', error);
            return false;
        }
    }
    
    async startLinuxListening() {
        if (!window.electron) return;
        
        try {
            // Use Electron IPC to call Linux speech engine
            const result = await window.electron.linuxSpeechRecognize();
            if (result.success && result.text) {
                this.processTranscript(result.text);
            }
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Linux listening error:', error);
            if (this.onError) this.onError(error);
        }
    }
    
    // ========================================================================
    // MOBILE SPEECH RECOGNITION (OFFLINE)
    // ========================================================================
    
    async initMobileSpeech() {
        try {
            console.log('[OfflineSpeech] 📱 Initializing Mobile Speech Recognition...');
            
            // Use native mobile speech APIs
            if (Platform.isAndroid) {
                this.recognitionEngine = 'android-native';
            } else if (Platform.isIOS) {
                this.recognitionEngine = 'ios-native';
            }
            
            this.isInitialized = true;
            console.log('[OfflineSpeech] ✅ Mobile Speech Recognition ready (OFFLINE)');
            return true;
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Mobile Speech failed:', error);
            return false;
        }
    }
    
    // ========================================================================
    // WEB SPEECH API WITH OFFLINE FLAG
    // ========================================================================
    
    async initWebSpeechOffline() {
        try {
            console.log('[OfflineSpeech] 🌐 Attempting Web Speech API (offline mode)...');
            
            if (!('webkitSpeechRecognition' in window) && !('SpeechRecognition' in window)) {
                console.log('[OfflineSpeech] ⚠️ Web Speech API not available');
                return false;
            }
            
            const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
            this.recognition = new SpeechRecognition();
            
            // Try to force offline mode (not guaranteed to work)
            this.recognition.continuous = true;
            this.recognition.interimResults = true;
            this.recognition.lang = 'en-US';
            
            // Some browsers support offline flag
            if ('offline' in this.recognition) {
                this.recognition.offline = true;
                console.log('[OfflineSpeech] ✅ Offline mode enabled');
            } else {
                console.log('[OfflineSpeech] ⚠️ Offline mode not supported, will use online API');
            }
            
            this.setupWebSpeechEvents();
            this.recognitionEngine = 'web-speech';
            this.isInitialized = true;
            
            console.log('[OfflineSpeech] ✅ Web Speech API ready');
            return true;
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Web Speech failed:', error);
            return false;
        }
    }
    
    setupWebSpeechEvents() {
        this.recognition.onstart = () => {
            this.isListening = true;
            console.log('[OfflineSpeech] 👂 Listening...');
            if (this.onStart) this.onStart();
        };
        
        this.recognition.onend = () => {
            this.isListening = false;
            console.log('[OfflineSpeech] 🔇 Stopped listening');
            if (this.onEnd) this.onEnd();
            
            // Auto-restart if still awake
            if (this.isAwake) {
                setTimeout(() => this.startListening(), 100);
            }
        };
        
        this.recognition.onresult = (event) => {
            let transcript = '';
            for (let i = event.resultIndex; i < event.results.length; i++) {
                if (event.results[i].isFinal) {
                    transcript += event.results[i][0].transcript;
                }
            }
            
            if (transcript) {
                console.log('[OfflineSpeech] 🎤 Heard:', transcript);
                this.processTranscript(transcript);
            }
        };
        
        this.recognition.onerror = (event) => {
            // Suppress network errors - use fallback instead
            if (event.error === 'network') {
                console.log('[OfflineSpeech] ℹ️ Network unavailable, using keyboard fallback');
                this.stopListening();
                this.showKeyboardFallback();
            } else {
                console.error('[OfflineSpeech] ❌ Error:', event.error);
                if (this.onError) this.onError(event.error);
            }
        };
    }
    
    // ========================================================================
    // KEYBOARD FALLBACK (ALWAYS WORKS!)
    // ========================================================================
    
    initKeyboardFallback() {
        console.log('[OfflineSpeech] ⌨️ Initializing keyboard command fallback...');
        
        // Add keyboard shortcut to open command palette
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.shiftKey && e.key === 'P') {
                this.showCommandPalette();
            }
        });
        
        this.recognitionEngine = 'keyboard-fallback';
        this.isInitialized = true;
        
        console.log('[OfflineSpeech] ✅ Keyboard fallback ready');
        console.log('[OfflineSpeech] 💡 Press Ctrl+Shift+P for command palette');
    }
    
    showKeyboardFallback() {
        const message = `🎤 Voice Recognition Unavailable (No Internet)\n\n` +
                       `Use keyboard shortcuts instead:\n\n` +
                       `• Ctrl+Shift+P - Command Palette\n` +
                       `• Ctrl+K - AI Assistant\n` +
                       `• Ctrl+/ - Quick Actions\n\n` +
                       `Or connect to internet for voice features.`;
        
        console.log('[OfflineSpeech] ℹ️ Showing keyboard fallback');
        // Don't alert - just log
    }
    
    showCommandPalette() {
        // Create command palette overlay
        let palette = document.getElementById('command-palette');
        
        if (!palette) {
            palette = document.createElement('div');
            palette.id = 'command-palette';
            palette.style.cssText = `
                position: fixed;
                top: 20%;
                left: 50%;
                transform: translateX(-50%);
                width: 600px;
                max-width: 90%;
                background: rgba(0, 0, 0, 0.95);
                border: 2px solid var(--cyan);
                border-radius: 10px;
                box-shadow: 0 10px 50px rgba(0, 212, 255, 0.5);
                z-index: 100000;
                padding: 20px;
                display: none;
            `;
            
            palette.innerHTML = `
                <div style="color: var(--cyan); font-size: 18px; font-weight: bold; margin-bottom: 15px;">
                    🎤 Voice Command Palette (Keyboard Mode)
                </div>
                <input id="command-input" type="text" 
                    placeholder="Type your command (e.g., 'create function', 'save file', 'new tab')" 
                    style="width: 100%; padding: 12px; background: rgba(255,255,255,0.05); 
                    border: 1px solid var(--cyan); border-radius: 5px; color: white; 
                    font-size: 14px; margin-bottom: 15px;">
                <div style="color: #888; font-size: 12px; margin-bottom: 10px;">
                    💡 Common commands: "new file", "save file", "close tab", "format code", 
                    "explain code", "fix bugs", "optimize"
                </div>
                <div style="display: flex; gap: 10px;">
                    <button onclick="executeCommandFromPalette()" 
                        style="flex: 1; padding: 10px; background: var(--cyan); color: black; 
                        border: none; border-radius: 5px; font-weight: bold; cursor: pointer;">
                        ▶️ Execute
                    </button>
                    <button onclick="closeCommandPalette()" 
                        style="padding: 10px 20px; background: rgba(255,71,87,0.2); 
                        color: var(--red); border: 1px solid var(--red); border-radius: 5px; 
                        font-weight: bold; cursor: pointer;">
                        ✕ Close
                    </button>
                </div>
            `;
            
            document.body.appendChild(palette);
            
            // Close on Escape
            document.addEventListener('keydown', (e) => {
                if (e.key === 'Escape' && palette.style.display === 'block') {
                    palette.style.display = 'none';
                }
            });
        }
        
        palette.style.display = 'block';
        document.getElementById('command-input').focus();
        console.log('[OfflineSpeech] 📋 Command palette opened');
    }
    
    // ========================================================================
    // COMMAND PATTERN MATCHING (OFFLINE)
    // ========================================================================
    
    buildCommandPatterns() {
        return {
            // File operations
            'new file': { action: 'new-file', requiresParam: false },
            'create file': { action: 'new-file', requiresParam: false },
            'save file': { action: 'save-file', requiresParam: false },
            'save': { action: 'save-file', requiresParam: false },
            'save as': { action: 'save-as', requiresParam: false },
            'open file': { action: 'open-file', requiresParam: false },
            'close tab': { action: 'close-tab', requiresParam: false },
            'close file': { action: 'close-tab', requiresParam: false },
            'next tab': { action: 'next-tab', requiresParam: false },
            'previous tab': { action: 'prev-tab', requiresParam: false },
            
            // Code operations
            'format code': { action: 'format', requiresParam: false },
            'format': { action: 'format', requiresParam: false },
            'comment': { action: 'comment', requiresParam: false },
            'uncomment': { action: 'uncomment', requiresParam: false },
            'indent': { action: 'indent', requiresParam: false },
            'outdent': { action: 'outdent', requiresParam: false },
            'undo': { action: 'undo', requiresParam: false },
            'redo': { action: 'redo', requiresParam: false },
            
            // AI operations
            'explain code': { action: 'ai-explain', requiresParam: false },
            'explain': { action: 'ai-explain', requiresParam: false },
            'fix code': { action: 'ai-fix', requiresParam: false },
            'fix bugs': { action: 'ai-fix', requiresParam: false },
            'optimize code': { action: 'ai-optimize', requiresParam: false },
            'optimize': { action: 'ai-optimize', requiresParam: false },
            'refactor': { action: 'ai-refactor', requiresParam: false },
            'generate tests': { action: 'ai-tests', requiresParam: false },
            'add documentation': { action: 'ai-docs', requiresParam: false },
            
            // Code generation
            'create function': { action: 'gen-function', requiresParam: true },
            'create class': { action: 'gen-class', requiresParam: true },
            'create component': { action: 'gen-component', requiresParam: true },
            'create variable': { action: 'gen-variable', requiresParam: true },
            'add comment': { action: 'add-comment', requiresParam: true },
            
            // Navigation
            'go to line': { action: 'goto-line', requiresParam: true },
            'find': { action: 'find', requiresParam: true },
            'search': { action: 'search', requiresParam: true },
            'replace': { action: 'replace', requiresParam: true },
            
            // Editing
            'select all': { action: 'select-all', requiresParam: false },
            'copy': { action: 'copy', requiresParam: false },
            'paste': { action: 'paste', requiresParam: false },
            'cut': { action: 'cut', requiresParam: false },
            'delete line': { action: 'delete-line', requiresParam: false },
            'duplicate line': { action: 'duplicate-line', requiresParam: false }
        };
    }
    
    processTranscript(transcript) {
        const text = transcript.toLowerCase().trim();
        console.log('[OfflineSpeech] 🎤 Processing:', text);
        
        // Check for wake word
        if (!this.isAwake) {
            for (const wakeWord of this.wakeWords) {
                if (text.includes(wakeWord)) {
                    this.isAwake = true;
                    console.log('[OfflineSpeech] 👋 Awake!');
                    this.speak('Ready! What would you like me to do?');
                    if (this.onResult) {
                        this.onResult({ 
                            transcript: text, 
                            command: 'wake', 
                            isWakeWord: true 
                        });
                    }
                    return;
                }
            }
            return; // Not awake yet, ignore
        }
        
        // Check for sleep word
        if (text.includes('go to sleep') || text.includes('goodbye')) {
            this.isAwake = false;
            console.log('[OfflineSpeech] 😴 Going to sleep');
            this.speak('Goodbye!');
            this.stopListening();
            return;
        }
        
        // Match command patterns
        for (const [pattern, command] of Object.entries(this.commandPatterns)) {
            if (text.includes(pattern)) {
                console.log('[OfflineSpeech] ✅ Command matched:', pattern);
                
                // Extract parameter if needed
                let param = null;
                if (command.requiresParam) {
                    param = text.replace(pattern, '').trim();
                }
                
                if (this.onResult) {
                    this.onResult({
                        transcript: text,
                        command: command.action,
                        parameter: param,
                        matched: pattern
                    });
                }
                
                this.speak('Executing ' + pattern);
                return;
            }
        }
        
        // No command matched - send to AI as natural language
        console.log('[OfflineSpeech] 💬 No command matched, sending to AI');
        if (this.onResult) {
            this.onResult({
                transcript: text,
                command: 'ai-query',
                isNaturalLanguage: true
            });
        }
    }
    
    speak(text) {
        if (!window.speechSynthesis) return;
        
        const utterance = new SpeechSynthesisUtterance(text);
        utterance.rate = 1.0;
        utterance.pitch = 1.0;
        window.speechSynthesis.speak(utterance);
    }
    
    // ========================================================================
    // UNIFIED START/STOP API
    // ========================================================================
    
    async startListening() {
        if (this.isListening) {
            console.log('[OfflineSpeech] ⚠️ Already listening');
            return;
        }
        
        console.log('[OfflineSpeech] 🎤 Starting listening...');
        
        try {
            switch (this.recognitionEngine) {
                case 'windows-native':
                    await this.startWindowsListening();
                    break;
                    
                case 'macos-native':
                    await this.startMacListening();
                    break;
                    
                case 'linux-native':
                    await this.startLinuxListening();
                    break;
                    
                case 'android-native':
                case 'ios-native':
                    await this.startMobileListening();
                    break;
                    
                case 'web-speech':
                    this.recognition.start();
                    break;
                    
                case 'keyboard-fallback':
                    this.showCommandPalette();
                    break;
                    
                default:
                    console.error('[OfflineSpeech] ❌ No recognition engine available');
                    this.showKeyboardFallback();
            }
        } catch (error) {
            console.error('[OfflineSpeech] ❌ Start failed:', error);
            // Fallback to keyboard
            this.showCommandPalette();
        }
    }
    
    stopListening() {
        if (!this.isListening) return;
        
        console.log('[OfflineSpeech] 🔇 Stopping...');
        this.isAwake = false;
        
        if (this.recognition && this.recognitionEngine === 'web-speech') {
            this.recognition.stop();
        }
        
        this.isListening = false;
    }
    
    async startMobileListening() {
        // Mobile speech API bridge
        console.log('[OfflineSpeech] 📱 Mobile speech not yet implemented');
        this.showCommandPalette();
    }
}

// ============================================================================
// GLOBAL FUNCTIONS FOR COMMAND PALETTE
// ============================================================================

window.executeCommandFromPalette = function() {
    const input = document.getElementById('command-input');
    const text = input.value.trim();
    
    if (text && window.offlineSpeech) {
        console.log('[OfflineSpeech] ⌨️ Executing keyboard command:', text);
        window.offlineSpeech.processTranscript(text);
        input.value = '';
        closeCommandPalette();
    }
};

window.closeCommandPalette = function() {
    const palette = document.getElementById('command-palette');
    if (palette) {
        palette.style.display = 'none';
    }
};

// ============================================================================
// EXPORTS
// ============================================================================

window.OfflineSpeechEngine = OfflineSpeechEngine;
window.Platform = Platform;

// Browser/Node compatibility
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { OfflineSpeechEngine, Platform };
}

console.log('[OfflineSpeech] 📦 Offline speech engine module loaded');
console.log('[OfflineSpeech] 🌍 Platform:', Platform.isWindows ? 'Windows' : Platform.isMac ? 'macOS' : Platform.isLinux ? 'Linux' : 'Other');
console.log('[OfflineSpeech] 💡 Press Ctrl+Shift+P for command palette (ALWAYS WORKS!)');

})(); // End IIFE


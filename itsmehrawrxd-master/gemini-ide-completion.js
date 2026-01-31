#!/usr/bin/env node

const axios = require('axios');
const fs = require('fs').promises;
const path = require('path');
const { spawn } = require('child_process');

class GeminiIDECompletion {
    constructor() {
        this.geminiUrl = 'https://gemini.google.com/app/aad339cc75299115?is_sa=1&is_sa=1&android-min-version=301356232&ios-min-version=322.0&campaign_id=bkws&utm_source=sem&utm_source=google&utm_medium=paid-media&utm_medium=cpc&utm_campaign=bkws&utm_campaign=2024enUS_gemfeb&pt=9008&mt=8&ct=p-growth-sem-bkws&gclsrc=aw.ds&gad_source=1&gad_campaignid=20108148196&gbraid=0AAAAApk5BhmPjh28p-mADsbx26drUMgJj&gclid=Cj0KCQjwxL7GBhDXARIsAGOcmIMuBnvtoeES_fC-riSiWHDvP77MgNq0ASZ-UttsH0XHs-V7Vv_TKx4aAoCJEALw_wcB';
        this.spoofedAIServer = 'http://localhost:9999';
        this.interval = 30000; // 30 seconds
        this.isRunning = false;
        this.completionCache = new Map();
        this.sessionId = this.generateSessionId();
        
        // IDE completion patterns
        this.completionPatterns = [
            'code completion',
            'function suggestion',
            'variable naming',
            'error fixing',
            'code optimization',
            'documentation generation',
            'test case creation',
            'refactoring suggestion',
            'performance improvement',
            'security enhancement'
        ];
    }

    async start() {
        console.log(' Starting Gemini IDE Completion System...');
        console.log('=' .repeat(60));
        
        try {
            // Check if spoofed AI server is running
            await this.checkSpoofedAIServer();
            
            // Start periodic completion fetching
            this.isRunning = true;
            this.startPeriodicCompletion();
            
            // Start IDE integration
            await this.startIDEIntegration();
            
            console.log(' Gemini IDE Completion System started successfully!');
            console.log(` Fetching completions every ${this.interval / 1000} seconds`);
            console.log(' Completions will be available in your IDE');
            
        } catch (error) {
            console.error(' Failed to start Gemini IDE Completion:', error.message);
            throw error;
        }
    }

    async checkSpoofedAIServer() {
        try {
            const response = await axios.get(`${this.spoofedAIServer}/health`, { timeout: 5000 });
            console.log(' Spoofed AI Server is running');
        } catch (error) {
            console.log(' Spoofed AI Server not running, starting it...');
            await this.startSpoofedAIServer();
        }
    }

    async startSpoofedAIServer() {
        return new Promise((resolve, reject) => {
            const server = spawn('node', ['spoofed-ai-server.js'], {
                stdio: 'pipe',
                detached: false
            });

            server.stdout.on('data', (data) => {
                const output = data.toString().trim();
                if (output.includes('Server running')) {
                    console.log(' Spoofed AI Server started');
                    resolve();
                }
            });

            server.stderr.on('data', (data) => {
                const error = data.toString().trim();
                if (error && !error.includes('DeprecationWarning')) {
                    console.log(`[Spoofed AI] ${error}`);
                }
            });

            server.on('error', (error) => {
                reject(new Error(`Failed to start spoofed AI server: ${error.message}`));
            });

            // Timeout after 10 seconds
            setTimeout(() => {
                reject(new Error('Timeout starting spoofed AI server'));
            }, 10000);
        });
    }

    startPeriodicCompletion() {
        console.log(' Starting periodic completion fetching...');
        
        // Initial fetch
        this.fetchCompletions();
        
        // Set up interval
        this.intervalId = setInterval(() => {
            this.fetchCompletions();
        }, this.interval);
    }

    async fetchCompletions() {
        try {
            console.log(' Fetching completions from Gemini...');
            
            // Get random completion pattern
            const pattern = this.completionPatterns[Math.floor(Math.random() * this.completionPatterns.length)];
            
            // Create completion request
            const completionRequest = {
                pattern: pattern,
                context: 'IDE completion system',
                timestamp: new Date().toISOString(),
                sessionId: this.sessionId
            };

            // Send to spoofed AI server (which will simulate Gemini responses)
            const response = await axios.post(`${this.spoofedAIServer}/api/gemini/unlock`, {
                messages: [
                    {
                        role: 'user',
                        content: `Generate IDE completion suggestions for: ${pattern}. Provide practical, actionable code completions that would be useful in a development environment.`
                    }
                ],
                stream: false,
                completion_type: 'ide_suggestion',
                pattern: pattern
            }, { timeout: 15000 });

            if (response.data && response.data.choices && response.data.choices[0]) {
                const completion = response.data.choices[0].message.content;
                
                // Process and cache completion
                await this.processCompletion(pattern, completion);
                
                console.log(` Fetched completion for: ${pattern}`);
            } else {
                console.log(' No completion data received');
            }

        } catch (error) {
            console.log(` Failed to fetch completion: ${error.message}`);
        }
    }

    async processCompletion(pattern, completion) {
        try {
            // Parse completion into structured data
            const structuredCompletion = this.parseCompletion(completion);
            
            // Cache the completion
            this.completionCache.set(pattern, {
                content: structuredCompletion,
                timestamp: new Date().toISOString(),
                source: 'gemini'
            });

            // Save to file for IDE access
            await this.saveCompletionToFile(pattern, structuredCompletion);
            
            // Update IDE integration
            await this.updateIDEIntegration(pattern, structuredCompletion);

        } catch (error) {
            console.log(` Failed to process completion: ${error.message}`);
        }
    }

    parseCompletion(completion) {
        // Parse the completion into structured format
        const lines = completion.split('\n');
        const structured = {
            suggestions: [],
            codeBlocks: [],
            explanations: [],
            examples: []
        };

        let currentSection = 'suggestions';
        let currentCodeBlock = '';

        for (const line of lines) {
            const trimmed = line.trim();
            
            if (trimmed.startsWith('```')) {
                if (currentCodeBlock) {
                    structured.codeBlocks.push({
                        language: currentCodeBlock.split('\n')[0] || 'javascript',
                        code: currentCodeBlock.trim()
                    });
                    currentCodeBlock = '';
                } else {
                    currentCodeBlock = trimmed.replace('```', '');
                }
            } else if (currentCodeBlock) {
                currentCodeBlock += line + '\n';
            } else if (trimmed.startsWith('- ') || trimmed.startsWith('* ')) {
                structured.suggestions.push(trimmed.substring(2));
            } else if (trimmed.startsWith('Example:')) {
                currentSection = 'examples';
                structured.examples.push(trimmed.substring(8));
            } else if (trimmed.startsWith('Explanation:')) {
                currentSection = 'explanations';
                structured.explanations.push(trimmed.substring(12));
            } else if (trimmed && currentSection === 'examples') {
                structured.examples[structured.examples.length - 1] += ' ' + trimmed;
            } else if (trimmed && currentSection === 'explanations') {
                structured.explanations[structured.explanations.length - 1] += ' ' + trimmed;
            }
        }

        return structured;
    }

    async saveCompletionToFile(pattern, completion) {
        try {
            const completionsDir = path.join(__dirname, 'ide-completions');
            await fs.mkdir(completionsDir, { recursive: true });
            
            const filename = `${pattern.replace(/\s+/g, '-')}-${Date.now()}.json`;
            const filepath = path.join(completionsDir, filename);
            
            await fs.writeFile(filepath, JSON.stringify(completion, null, 2));
            
            // Also update the latest completion file
            const latestFile = path.join(completionsDir, 'latest-completions.json');
            const latestData = {
                pattern: pattern,
                completion: completion,
                timestamp: new Date().toISOString(),
                source: 'gemini'
            };
            
            await fs.writeFile(latestFile, JSON.stringify(latestData, null, 2));
            
        } catch (error) {
            console.log(` Failed to save completion: ${error.message}`);
        }
    }

    async updateIDEIntegration(pattern, completion) {
        try {
            // Update VS Code extension
            await this.updateVSCodeExtension(pattern, completion);
            
            // Update IntelliJ plugin
            await this.updateIntelliJPlugin(pattern, completion);
            
            // Update web interface
            await this.updateWebInterface(pattern, completion);
            
        } catch (error) {
            console.log(` Failed to update IDE integration: ${error.message}`);
        }
    }

    async updateVSCodeExtension(pattern, completion) {
        try {
            const extensionPath = path.join(__dirname, 'n0mn0m-vscode-extension', 'completions.json');
            const existingData = await this.loadExistingCompletions(extensionPath);
            
            existingData[pattern] = {
                ...completion,
                timestamp: new Date().toISOString(),
                source: 'gemini'
            };
            
            await fs.writeFile(extensionPath, JSON.stringify(existingData, null, 2));
            
        } catch (error) {
            console.log(` Failed to update VS Code extension: ${error.message}`);
        }
    }

    async updateIntelliJPlugin(pattern, completion) {
        try {
            const pluginPath = path.join(__dirname, 'intellij-plugin', 'src', 'main', 'resources', 'completions.json');
            const existingData = await this.loadExistingCompletions(pluginPath);
            
            existingData[pattern] = {
                ...completion,
                timestamp: new Date().toISOString(),
                source: 'gemini'
            };
            
            await fs.writeFile(pluginPath, JSON.stringify(existingData, null, 2));
            
        } catch (error) {
            console.log(` Failed to update IntelliJ plugin: ${error.message}`);
        }
    }

    async updateWebInterface(pattern, completion) {
        try {
            const webPath = path.join(__dirname, 'web-completions.json');
            const existingData = await this.loadExistingCompletions(webPath);
            
            existingData[pattern] = {
                ...completion,
                timestamp: new Date().toISOString(),
                source: 'gemini'
            };
            
            await fs.writeFile(webPath, JSON.stringify(existingData, null, 2));
            
        } catch (error) {
            console.log(` Failed to update web interface: ${error.message}`);
        }
    }

    async loadExistingCompletions(filepath) {
        try {
            const data = await fs.readFile(filepath, 'utf8');
            return JSON.parse(data);
        } catch (error) {
            return {};
        }
    }

    async startIDEIntegration() {
        console.log(' Starting IDE integration...');
        
        try {
            // Start VS Code extension if available
            await this.startVSCodeExtension();
            
            // Start IntelliJ plugin if available
            await this.startIntelliJPlugin();
            
            // Start web interface
            await this.startWebInterface();
            
        } catch (error) {
            console.log(` IDE integration warning: ${error.message}`);
        }
    }

    async startVSCodeExtension() {
        try {
            const extensionPath = path.join(__dirname, 'n0mn0m-vscode-extension');
            const packageJsonPath = path.join(extensionPath, 'package.json');
            
            await fs.access(packageJsonPath);
            
            // Compile and install extension
            const compileProcess = spawn('npm', ['run', 'compile'], {
                cwd: extensionPath,
                stdio: 'pipe'
            });

            compileProcess.on('close', (code) => {
                if (code === 0) {
                    console.log(' VS Code extension compiled');
                }
            });

        } catch (error) {
            console.log(' VS Code extension not found');
        }
    }

    async startIntelliJPlugin() {
        try {
            const pluginPath = path.join(__dirname, 'intellij-plugin');
            const buildGradlePath = path.join(pluginPath, 'build.gradle');
            
            await fs.access(buildGradlePath);
            
            // Build plugin
            const buildProcess = spawn('./gradlew', ['build'], {
                cwd: pluginPath,
                stdio: 'pipe'
            });

            buildProcess.on('close', (code) => {
                if (code === 0) {
                    console.log(' IntelliJ plugin built');
                }
            });

        } catch (error) {
            console.log(' IntelliJ plugin not found');
        }
    }

    async startWebInterface() {
        try {
            // Create web interface for completions
            const webInterface = this.createWebInterface();
            const webPath = path.join(__dirname, 'gemini-completions-panel.html');
            
            await fs.writeFile(webPath, webInterface);
            
            console.log(' Web interface created: gemini-completions-panel.html');
            
        } catch (error) {
            console.log(` Failed to create web interface: ${error.message}`);
        }
    }

    createWebInterface() {
        return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Gemini IDE Completions</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0;
            padding: 20px;
            background: #1e1e1e;
            color: #ffffff;
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #4CAF50;
            margin-bottom: 10px;
        }
        .completions-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
            gap: 20px;
        }
        .completion-card {
            background: #2d2d2d;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 20px;
        }
        .completion-title {
            color: #4CAF50;
            font-size: 18px;
            font-weight: bold;
            margin-bottom: 15px;
        }
        .suggestions {
            margin-bottom: 15px;
        }
        .suggestion {
            background: #3d3d3d;
            padding: 8px 12px;
            margin: 5px 0;
            border-radius: 4px;
            border-left: 3px solid #4CAF50;
        }
        .code-block {
            background: #1e1e1e;
            border: 1px solid #404040;
            border-radius: 4px;
            padding: 15px;
            margin: 10px 0;
            font-family: 'Courier New', monospace;
            overflow-x: auto;
        }
        .timestamp {
            color: #888;
            font-size: 12px;
            margin-top: 10px;
        }
        .status {
            position: fixed;
            top: 20px;
            right: 20px;
            background: #4CAF50;
            color: white;
            padding: 10px 20px;
            border-radius: 20px;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="status" id="status"> Fetching Completions...</div>
    
    <div class="header">
        <h1> Gemini IDE Completions</h1>
        <p>Real-time AI-powered code completions from Gemini</p>
    </div>
    
    <div class="completions-grid" id="completionsGrid">
        <div class="completion-card">
            <div class="completion-title">Loading...</div>
            <p>Fetching completions from Gemini...</p>
        </div>
    </div>

    <script>
        async function loadCompletions() {
            try {
                const response = await fetch('web-completions.json');
                const data = await response.json();
                
                const grid = document.getElementById('completionsGrid');
                grid.innerHTML = '';
                
                for (const [pattern, completion] of Object.entries(data)) {
                    const card = document.createElement('div');
                    card.className = 'completion-card';
                    
                    let html = \`<div class="completion-title">\${pattern}</div>\`;
                    
                    if (completion.suggestions && completion.suggestions.length > 0) {
                        html += '<div class="suggestions">';
                        completion.suggestions.forEach(suggestion => {
                            html += \`<div class="suggestion">\${suggestion}</div>\`;
                        });
                        html += '</div>';
                    }
                    
                    if (completion.codeBlocks && completion.codeBlocks.length > 0) {
                        completion.codeBlocks.forEach(block => {
                            html += \`<div class="code-block">\${block.code}</div>\`;
                        });
                    }
                    
                    if (completion.explanations && completion.explanations.length > 0) {
                        completion.explanations.forEach(explanation => {
                            html += \`<div class="suggestion"> \${explanation}</div>\`;
                        });
                    }
                    
                    html += \`<div class="timestamp">Updated: \${new Date(completion.timestamp).toLocaleString()}</div>\`;
                    
                    card.innerHTML = html;
                    grid.appendChild(card);
                }
                
                document.getElementById('status').textContent = ' Completions Updated';
                document.getElementById('status').style.background = '#4CAF50';
                
            } catch (error) {
                document.getElementById('status').textContent = ' Error Loading';
                document.getElementById('status').style.background = '#f44336';
            }
        }
        
        // Load completions initially
        loadCompletions();
        
        // Refresh every 30 seconds
        setInterval(loadCompletions, 30000);
    </script>
</body>
</html>`;
    }

    generateSessionId() {
        return 'session_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }

    async stop() {
        console.log(' Stopping Gemini IDE Completion System...');
        
        this.isRunning = false;
        
        if (this.intervalId) {
            clearInterval(this.intervalId);
        }
        
        console.log(' Gemini IDE Completion System stopped');
    }
}

// CLI interface
if (require.main === module) {
    const completion = new GeminiIDECompletion();
    
    // Handle graceful shutdown
    process.on('SIGINT', async () => {
        await completion.stop();
        process.exit(0);
    });
    
    process.on('SIGTERM', async () => {
        await completion.stop();
        process.exit(0);
    });
    
    completion.start().catch(console.error);
}

module.exports = GeminiIDECompletion;

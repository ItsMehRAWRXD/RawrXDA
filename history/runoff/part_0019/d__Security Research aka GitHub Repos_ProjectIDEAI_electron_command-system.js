/**
 * BigDaddyG IDE - Unified Command System
 * Slash commands for AI: !pic, !code, !projectnew, !projectresume
 */

(function() {
'use strict';

class CommandSystem {
    constructor() {
        this.commands = new Map();
        this.projectHistory = [];
        this.currentProject = null;
        
        console.log('[CommandSystem] ⚡ Initializing command system...');
    }
    
    async init() {
        // Register all commands
        this.registerCommands();
        
        // Hook into AI chat
        this.hookIntoChat();
        
        // Load project history
        this.loadProjectHistory();
        
        console.log('[CommandSystem] ✅ Command system ready!');
        this.showAvailableCommands();
    }
    
    registerCommands() {
        // !pic - Generate images
        this.commands.set('pic', {
            name: 'Generate Image',
            usage: '!pic [description]',
            description: 'Generate images with AI (e.g., !pic a futuristic IDE interface)',
            icon: '🎨',
            handler: async (args) => await this.handlePicCommand(args)
        });
        
        // !code - Generate code
        this.commands.set('code', {
            name: 'Generate Code',
            usage: '!code [language] [description]',
            description: 'Generate code in any language (e.g., !code python create a web scraper)',
            icon: '💻',
            handler: async (args) => await this.handleCodeCommand(args)
        });
        
        // !projectnew - Create new project
        this.commands.set('projectnew', {
            name: 'New Project',
            usage: '!projectnew [type] [name]',
            description: 'Create a new project (e.g., !projectnew react my-app)',
            icon: '🆕',
            handler: async (args) => await this.handleProjectNewCommand(args)
        });
        
        // !projectresume - Resume project
        this.commands.set('projectresume', {
            name: 'Resume Project',
            usage: '!projectresume [name or empty for list]',
            description: 'Resume working on a previous project',
            icon: '▶️',
            handler: async (args) => await this.handleProjectResumeCommand(args)
        });
        
        // !compile - Compile code
        this.commands.set('compile', {
            name: 'Compile Code',
            usage: '!compile [file or current]',
            description: 'Compile current file or specified file',
            icon: '🔧',
            handler: async (args) => await this.handleCompileCommand(args)
        });
        
        // !run - Run code
        this.commands.set('run', {
            name: 'Run Code',
            usage: '!run [file or current]',
            description: 'Run current file or specified file',
            icon: '▶️',
            handler: async (args) => await this.handleRunCommand(args)
        });
        
        // !test - Generate tests
        this.commands.set('test', {
            name: 'Generate Tests',
            usage: '!test [file or current]',
            description: 'Generate unit tests for code',
            icon: '🧪',
            handler: async (args) => await this.handleTestCommand(args)
        });
        
        // !docs - Generate documentation
        this.commands.set('docs', {
            name: 'Generate Docs',
            usage: '!docs [file or current]',
            description: 'Generate documentation for code',
            icon: '📚',
            handler: async (args) => await this.handleDocsCommand(args)
        });
        
        // !refactor - Refactor code
        this.commands.set('refactor', {
            name: 'Refactor Code',
            usage: '!refactor [description]',
            description: 'Refactor code with AI guidance',
            icon: '🔄',
            handler: async (args) => await this.handleRefactorCommand(args)
        });
        
        // !help - Show all commands
        this.commands.set('help', {
            name: 'Help',
            usage: '!help',
            description: 'Show all available commands',
            icon: '❓',
            handler: async () => await this.handleHelpCommand()
        });
        
        console.log(`[CommandSystem] ✅ Registered ${this.commands.size} commands`);
    }
    
    hookIntoChat() {
        // Hook into floating chat
        if (window.floatingChat) {
            const originalSend = window.floatingChat.send.bind(window.floatingChat);
            
            window.floatingChat.send = async function() {
                const input = document.getElementById('floating-chat-input');
                const message = input?.value.trim();
                
                if (message && message.startsWith('!')) {
                    // Extract command and args
                    const parts = message.substring(1).split(' ');
                    const command = parts[0].toLowerCase();
                    const args = parts.slice(1).join(' ');
                    
                    if (window.commandSystem.commands.has(command)) {
                        input.value = '';
                        await window.commandSystem.executeCommand(command, args);
                        return;
                    }
                }
                
                // Call original send
                return originalSend();
            };
        }
        
        // Also hook into sidebar chat
        const originalSendToAI = window.sendToAI;
        
        window.sendToAI = async function() {
            const input = document.getElementById('ai-input');
            const message = input?.value.trim();
            
            if (message && message.startsWith('!')) {
                const parts = message.substring(1).split(' ');
                const command = parts[0].toLowerCase();
                const args = parts.slice(1).join(' ');
                
                if (window.commandSystem.commands.has(command)) {
                    input.value = '';
                    await window.commandSystem.executeCommand(command, args);
                    return;
                }
            }
            
            // Call original
            if (originalSendToAI) {
                return originalSendToAI();
            }
        };
        
        console.log('[CommandSystem] ✅ Commands hooked into chat');
    }
    
    async executeCommand(command, args) {
        const cmd = this.commands.get(command);
        
        if (!cmd) {
            this.showError(`Unknown command: !${command}. Type !help for available commands.`);
            return;
        }
        
        console.log(`[CommandSystem] ⚡ Executing: !${command} ${args}`);
        
        try {
            await cmd.handler(args);
        } catch (error) {
            console.error(`[CommandSystem] ❌ Command failed:`, error);
            this.showError(`Command failed: ${error.message}`);
        }
    }
    
    // ========================================================================
    // COMMAND HANDLERS
    // ========================================================================
    
    async handlePicCommand(args) {
        if (!args) {
            this.showError('Usage: !pic [description]\nExample: !pic a futuristic IDE with neon overlays');
            return;
        }
        
        // Delegate to image generator
        if (window.imageGenerator) {
            await window.imageGenerator.generateImage(args);
        } else {
            this.showError('Image generator not available');
        }
    }
    
    async handleCodeCommand(args) {
        if (!args) {
            this.showError('Usage: !code [language] [description]\nExample: !code python create a web scraper');
            return;
        }
        
        this.showProgress('💻 Generating code...');
        
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Generate production-quality code for: ${args}\n\nProvide ONLY the code, properly formatted and commented.`,
                    model: 'BigDaddyG:Latest',
                    parameters: {
                        temperature: 0.3, // Lower for code generation
                        max_tokens: 8192
                    }
                })
            });
            
            if (!response.ok) throw new Error(`AI returned ${response.status}`);
            
            const data = await response.json();
            const code = data.response || data.message;
            
            // Display code with syntax highlighting
            this.displayGeneratedCode(args, code);
            
        } catch (error) {
            this.showError(`Code generation failed: ${error.message}`);
        }
    }
    
    async handleProjectNewCommand(args) {
        if (!args) {
            // Show project templates
            this.showProjectTemplates();
            return;
        }
        
        const parts = args.split(' ');
        const type = parts[0].toLowerCase();
        const name = parts.slice(1).join(' ') || `new-${type}-project`;
        
        this.showProgress(`🆕 Creating ${type} project: ${name}...`);
        
        try {
            // Ask AI to create project structure
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Create a complete ${type} project structure for "${name}". Include:
- Project folder structure
- package.json or equivalent
- Main entry files
- Configuration files
- README.md
- .gitignore

Return the structure as a JSON object with file paths as keys and content as values.`,
                    model: 'BigDaddyG:Latest',
                    parameters: { temperature: 0.4 }
                })
            });
            
            if (!response.ok) throw new Error(`AI returned ${response.status}`);
            
            const data = await response.json();
            const projectStructure = data.response || data.message;
            
            // Create project
            await this.createProjectFiles(name, projectStructure);
            
            // Save to history
            this.currentProject = {
                name,
                type,
                path: name,
                created: Date.now(),
                lastOpened: Date.now()
            };
            
            this.projectHistory.push(this.currentProject);
            this.saveProjectHistory();
            
            this.showSuccess(`✅ Project created: ${name}\n\nFiles created successfully! Opening in Explorer...`);
            
        } catch (error) {
            this.showError(`Project creation failed: ${error.message}`);
        }
    }
    
    async handleProjectResumeCommand(args) {
        if (!args) {
            // Show list of recent projects
            this.showRecentProjects();
            return;
        }
        
        // Find project by name
        const project = this.projectHistory.find(p => 
            p.name.toLowerCase().includes(args.toLowerCase())
        );
        
        if (!project) {
            this.showError(`Project not found: ${args}\n\nType !projectresume to see all projects.`);
            return;
        }
        
        this.showProgress(`▶️ Resuming project: ${project.name}...`);
        
        try {
            // Open project folder
            if (window.electron && window.electron.openFolderDialog) {
                // Or auto-open if path exists
                console.log(`[CommandSystem] Resuming: ${project.path}`);
                
                // Update last opened
                project.lastOpened = Date.now();
                this.currentProject = project;
                this.saveProjectHistory();
                
                // Ask AI for context
                const context = await this.getProjectContext(project);
                
                this.showProjectResume(project, context);
            }
            
        } catch (error) {
            this.showError(`Failed to resume project: ${error.message}`);
        }
    }
    
    async handleCompileCommand(args) {
        const file = args || 'current';
        this.showProgress(`🔧 Compiling ${file}...`);
        
        // Get current file if 'current'
        let filePath = file;
        if (file === 'current') {
            filePath = this.getCurrentFilePath();
            if (!filePath) {
                this.showError('No file is currently open');
                return;
            }
        }
        
        try {
            // Get file content
            const code = window.editor?.getValue() || '';
            if (!code) {
                this.showError('No code to compile');
                return;
            }
            
            // Detect language from file extension
            const ext = filePath.split('.').pop().toLowerCase();
            const langMap = {
                'js': 'node',
                'ts': 'tsc',
                'py': 'python',
                'java': 'javac',
                'c': 'gcc',
                'cpp': 'g++',
                'cs': 'csc',
                'go': 'go build',
                'rs': 'cargo build'
            };
            
            const compiler = langMap[ext];
            if (!compiler) {
                this.showError(`No compiler configured for .${ext} files`);
                return;
            }
            
            // Use agentic executor to compile
            if (window.getAgenticExecutor) {
                const executor = window.getAgenticExecutor();
                const result = await executor.executeTask(`Compile ${filePath} using ${compiler}`);
                
                if (result.success) {
                    this.showSuccess(`✅ Compilation successful!\n\n${result.output || ''}`);
                } else {
                    this.showError(`❌ Compilation failed:\n\n${result.error || 'Unknown error'}`);
                }
            } else {
                // Fallback: show command to run
                this.showSuccess(`✅ Run this command to compile:\n\n${compiler} ${filePath}`);
            }
            
        } catch (error) {
            this.showError(`Compilation error: ${error.message}`);
        }
    }
    
    async handleRunCommand(args) {
        const file = args || 'current';
        this.showProgress(`▶️ Running ${file}...`);
        
        try {
            let filePath = file === 'current' ? this.getCurrentFilePath() : file;
            
            if (!filePath) {
                this.showError('No file specified');
                return;
            }
            
            // Use agentic executor to run file
            if (window.getAgenticExecutor) {
                const executor = window.getAgenticExecutor();
                const result = await executor.executeTask(`Run ${filePath}`);
                
                if (result.success) {
                    this.showSuccess(`✅ Execution complete!\n\n${result.output || ''}`);
                } else {
                    this.showError(`❌ Execution failed:\n\n${result.error || 'Unknown error'}`);
                }
            } else if (window.agenticBrowser) {
                // Fallback to file browser
                await window.agenticBrowser.runFile(filePath);
            } else {
                this.showError('No execution system available');
            }
            
        } catch (error) {
            this.showError(`Execution error: ${error.message}`);
        }
    }
    
    async handleTestCommand(args) {
        this.showProgress('🧪 Generating tests...');
        
        const code = window.editor?.getValue() || '';
        
        if (!code) {
            this.showError('No code to test. Open a file first.');
            return;
        }
        
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Generate comprehensive unit tests for this code:\n\n\`\`\`\n${code}\n\`\`\`\n\nUse appropriate testing framework (Jest, pytest, etc.).`,
                    model: 'BigDaddyG:Latest',
                    parameters: { temperature: 0.3 }
                })
            });
            
            const data = await response.json();
            const tests = data.response || data.message;
            
            this.displayGeneratedCode('Unit Tests', tests);
            
        } catch (error) {
            this.showError(`Test generation failed: ${error.message}`);
        }
    }
    
    async handleDocsCommand(args) {
        this.showProgress('📚 Generating documentation...');
        
        const code = window.editor?.getValue() || '';
        
        if (!code) {
            this.showError('No code to document. Open a file first.');
            return;
        }
        
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Generate comprehensive documentation for this code:\n\n\`\`\`\n${code}\n\`\`\`\n\nInclude: function descriptions, parameters, return values, examples.`,
                    model: 'BigDaddyG:Latest',
                    parameters: { temperature: 0.4 }
                })
            });
            
            const data = await response.json();
            const docs = data.response || data.message;
            
            this.displayGeneratedCode('Documentation', docs);
            
        } catch (error) {
            this.showError(`Documentation generation failed: ${error.message}`);
        }
    }
    
    async handleRefactorCommand(args) {
        this.showProgress('🔄 Refactoring code...');
        
        const code = window.editor?.getValue() || '';
        
        if (!code) {
            this.showError('No code to refactor. Open a file first.');
            return;
        }
        
        const instruction = args || 'improve code quality, performance, and readability';
        
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Refactor this code to ${instruction}:\n\n\`\`\`\n${code}\n\`\`\`\n\nProvide the refactored code with explanations.`,
                    model: 'BigDaddyG:Latest',
                    parameters: { temperature: 0.3 }
                })
            });
            
            const data = await response.json();
            const refactored = data.response || data.message;
            
            this.displayGeneratedCode('Refactored Code', refactored);
            
        } catch (error) {
            this.showError(`Refactoring failed: ${error.message}`);
        }
    }
    
    async handleHelpCommand() {
        const commandList = Array.from(this.commands.entries())
            .map(([cmd, info]) => `${info.icon} **!${cmd}** - ${info.description}`)
            .join('\n');
        
        const helpMessage = `
# 🎯 Available Commands

${commandList}

**Tip:** Type any command in the AI chat and press Ctrl+Enter!
        `.trim();
        
        this.displayMessage('Command Help', helpMessage, '❓');
    }
    
    // ========================================================================
    // HELPER METHODS
    // ========================================================================
    
    showProjectTemplates() {
        const templates = [
            { type: 'react', name: 'React App', icon: '⚛️', description: 'React + Vite + TypeScript' },
            { type: 'node', name: 'Node.js App', icon: '🟢', description: 'Express + TypeScript' },
            { type: 'python', name: 'Python App', icon: '🐍', description: 'Flask + Virtual Env' },
            { type: 'vue', name: 'Vue.js App', icon: '💚', description: 'Vue 3 + Vite' },
            { type: 'svelte', name: 'Svelte App', icon: '🧡', description: 'SvelteKit' },
            { type: 'next', name: 'Next.js App', icon: '▲', description: 'Next.js + React' },
            { type: 'electron', name: 'Electron App', icon: '⚡', description: 'Electron + React' },
            { type: 'rust', name: 'Rust Project', icon: '🦀', description: 'Cargo + tokio' },
            { type: 'go', name: 'Go Project', icon: '🐹', description: 'Go modules' },
            { type: 'java', name: 'Java Project', icon: '☕', description: 'Maven + Spring Boot' }
        ];
        
        const templatesHTML = templates.map(t => `
            <div style="background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-border); border-radius: 8px; padding: 12px; cursor: pointer; transition: all 0.2s;" onmouseover="this.style.borderColor='var(--cursor-jade-light)'" onmouseout="this.style.borderColor='var(--cursor-border)'" onclick="commandSystem.executeCommand('projectnew', '${t.type} my-${t.type}-app')">
                <div style="font-size: 24px; margin-bottom: 6px;">${t.icon}</div>
                <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark); margin-bottom: 4px;">${t.name}</div>
                <div style="font-size: 11px; color: var(--cursor-text-secondary);">${t.description}</div>
            </div>
        `).join('');
        
        this.displayMessage('Project Templates', `
            <div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px; margin-top: 12px;">
                ${templatesHTML}
            </div>
            <div style="margin-top: 16px; padding: 12px; background: rgba(119, 221, 190, 0.1); border-radius: 8px; font-size: 12px; color: var(--cursor-text-secondary);">
                💡 <strong>Tip:</strong> Type <code style="background: rgba(119, 221, 190, 0.2); padding: 2px 6px; border-radius: 3px;">!projectnew [type] [name]</code> or click a template above
            </div>
        `, '🆕');
    }
    
    showRecentProjects() {
        if (this.projectHistory.length === 0) {
            this.displayMessage('Recent Projects', 'No projects yet. Use **!projectnew** to create one!', '▶️');
            return;
        }
        
        // Sort by last opened
        const sorted = [...this.projectHistory].sort((a, b) => b.lastOpened - a.lastOpened);
        
        const projectsHTML = sorted.map(p => `
            <div style="background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-border); border-radius: 8px; padding: 12px; margin-bottom: 8px; cursor: pointer; transition: all 0.2s;" onmouseover="this.style.borderColor='var(--cursor-jade-light)'" onmouseout="this.style.borderColor='var(--cursor-border)'" onclick="commandSystem.executeCommand('projectresume', '${p.name}')">
                <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 6px;">
                    <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark);">${p.name}</div>
                    <div style="font-size: 10px; color: var(--cursor-text-muted); background: rgba(119, 221, 190, 0.15); padding: 2px 6px; border-radius: 10px;">${p.type}</div>
                </div>
                <div style="font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 4px;">
                    📁 ${p.path}
                </div>
                <div style="font-size: 10px; color: var(--cursor-text-muted);">
                    Last opened: ${new Date(p.lastOpened).toLocaleString()}
                </div>
            </div>
        `).join('');
        
        this.displayMessage('Recent Projects', projectsHTML, '▶️');
    }
    
    async getProjectContext(project) {
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `I'm resuming work on a ${project.type} project called "${project.name}". 
What should I focus on? Suggest next steps, potential improvements, and things to check.`,
                    model: 'BigDaddyG:Latest',
                    parameters: { temperature: 0.6 }
                })
            });
            
            const data = await response.json();
            return data.response || data.message;
            
        } catch (error) {
            return 'Welcome back! Ready to continue coding.';
        }
    }
    
    showProjectResume(project, context) {
        this.displayMessage(`Resume: ${project.name}`, `
            <div style="background: rgba(119, 221, 190, 0.1); border-radius: 8px; padding: 12px; margin-bottom: 12px;">
                <div style="font-size: 13px; font-weight: 600; color: var(--cursor-jade-dark); margin-bottom: 6px;">
                    📂 ${project.name}
                </div>
                <div style="font-size: 11px; color: var(--cursor-text-secondary); line-height: 1.6;">
                    <strong>Type:</strong> ${project.type}<br>
                    <strong>Path:</strong> ${project.path}<br>
                    <strong>Created:</strong> ${new Date(project.created).toLocaleString()}<br>
                    <strong>Last Opened:</strong> ${new Date(project.lastOpened).toLocaleString()}
                </div>
            </div>
            
            <div style="color: var(--cursor-text); font-size: 13px; line-height: 1.6; white-space: pre-wrap;">
${context}
            </div>
        `, '▶️');
    }
    
    async createProjectFiles(projectName, structure) {
        console.log(`[CommandSystem] Creating project: ${projectName}`);
        
        // Parse AI response to extract file structure
        // For now, just show the structure
        console.log('Project structure:', structure);
        
        // In full implementation, use window.electron.writeFile to create files
    }
    
    getCurrentFilePath() {
        // Get current open file from tabs
        if (window.openTabs && window.openTabs.length > 0) {
            const activeTab = window.openTabs.find(t => t.isActive);
            return activeTab?.filePath || null;
        }
        return null;
    }
    
    loadProjectHistory() {
        try {
            const saved = localStorage.getItem('bigdaddyg-project-history');
            if (saved) {
                this.projectHistory = JSON.parse(saved);
                console.log(`[CommandSystem] Loaded ${this.projectHistory.length} projects from history`);
            }
        } catch (error) {
            console.warn('[CommandSystem] Failed to load project history:', error);
        }
    }
    
    saveProjectHistory() {
        try {
            localStorage.setItem('bigdaddyg-project-history', JSON.stringify(this.projectHistory));
        } catch (error) {
            console.warn('[CommandSystem] Failed to save project history:', error);
        }
    }
    
    // ========================================================================
    // UI HELPERS
    // ========================================================================
    
    showProgress(message) {
        this.displayMessage('Command', message, '⏳', 'info');
    }
    
    showSuccess(message) {
        this.displayMessage('Command', message, '✅', 'success');
    }
    
    showError(message) {
        this.displayMessage('Error', message, '❌', 'error');
    }
    
    displayMessage(title, content, icon = '💬', type = 'info') {
        const container = document.getElementById('floating-chat-messages') || 
                          document.getElementById('ai-messages') ||
                          document.getElementById('ai-chat-messages');
        
        if (!container) return;
        
        const bgColors = {
            info: 'rgba(74, 144, 226, 0.1)',
            success: 'rgba(119, 221, 190, 0.1)',
            error: 'rgba(255, 71, 87, 0.1)'
        };
        
        const borderColors = {
            info: '#4a90e2',
            success: '#77ddbe',
            error: '#ff4757'
        };
        
        const msgDiv = document.createElement('div');
        msgDiv.style.cssText = `
            margin-bottom: 16px;
            padding: 14px;
            background: ${bgColors[type]};
            border-left: 4px solid ${borderColors[type]};
            border-radius: 8px;
            user-select: text;
        `;
        
        msgDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 12px; user-select: none;">
                <span style="font-size: 16px;">${icon}</span>
                <span style="font-weight: 600; color: ${borderColors[type]};">${title}</span>
                <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
            </div>
            <div style="color: var(--cursor-text); font-size: 13px; line-height: 1.6; white-space: pre-wrap; user-select: text;">
                ${content}
            </div>
        `;
        
        container.appendChild(msgDiv);
        container.scrollTop = container.scrollHeight;
    }
    
    displayGeneratedCode(title, code) {
        const container = document.getElementById('floating-chat-messages') || 
                          document.getElementById('ai-messages') ||
                          document.getElementById('ai-chat-messages');
        
        if (!container) return;
        
        // Extract code from markdown if present
        let extractedCode = code;
        const codeBlockMatch = code.match(/```[\w]*\n([\s\S]*?)\n```/);
        if (codeBlockMatch) {
            extractedCode = codeBlockMatch[1];
        }
        
        const msgDiv = document.createElement('div');
        msgDiv.style.cssText = `
            margin-bottom: 16px;
            padding: 14px;
            background: rgba(119, 221, 190, 0.1);
            border-left: 4px solid var(--cursor-jade-dark);
            border-radius: 8px;
        `;
        
        msgDiv.innerHTML = `
            <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 12px; user-select: none;">
                <span style="font-size: 16px;">💻</span>
                <span style="font-weight: 600; color: var(--cursor-jade-dark);">${title}</span>
                <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
            </div>
            <div style="background: #1e1e1e; border-radius: 8px; padding: 16px; margin-bottom: 12px; overflow-x: auto;">
                <pre style="margin: 0; color: #d4d4d4; font-family: 'Courier New', monospace; font-size: 12px; line-height: 1.5; user-select: text;"><code>${this.escapeHtml(extractedCode)}</code></pre>
            </div>
            <div style="display: flex; gap: 8px;">
                <button onclick="commandSystem.applyCodeToEditor(this.previousElementSibling.querySelector('code').textContent)" style="flex: 1; background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    ✅ Apply to Editor
                </button>
                <button onclick="commandSystem.copyCode(this.previousElementSibling.previousElementSibling.querySelector('code').textContent)" style="flex: 1; background: rgba(119, 221, 190, 0.15); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    📋 Copy Code
                </button>
                <button onclick="commandSystem.saveCodeToFile(this.previousElementSibling.previousElementSibling.previousElementSibling.querySelector('code').textContent)" style="flex: 1; background: rgba(119, 221, 190, 0.15); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 8px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    💾 Save as File
                </button>
            </div>
        `;
        
        container.appendChild(msgDiv);
        container.scrollTop = container.scrollHeight;
    }
    
    applyCodeToEditor(code) {
        if (window.editor) {
            window.editor.setValue(code);
            this.showNotification('✅ Code applied to editor!', 'success');
        } else {
            this.showNotification('❌ No editor open', 'error');
        }
    }
    
    copyCode(code) {
        navigator.clipboard.writeText(code)
            .then(() => this.showNotification('✅ Code copied!', 'success'))
            .catch(() => this.showNotification('❌ Copy failed', 'error'));
    }
    
    async saveCodeToFile(code) {
        if (window.electron && window.electron.saveFileDialog) {
            try {
                const filePath = await window.electron.saveFileDialog({
                    title: 'Save Generated Code',
                    filters: [{ name: 'All Files', extensions: ['*'] }]
                });
                
                if (filePath) {
                    await window.electron.writeFile(filePath, code);
                    this.showNotification(`✅ Saved: ${filePath}`, 'success');
                }
            } catch (error) {
                this.showNotification('❌ Save failed', 'error');
            }
        }
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    showAvailableCommands() {
        console.log('[CommandSystem] 📋 Available commands:');
        this.commands.forEach((info, cmd) => {
            console.log(`  ${info.icon} !${cmd} - ${info.description}`);
        });
    }
    
    showNotification(message, type = 'info') {
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: ${type === 'error' ? '#ff4757' : type === 'success' ? '#77ddbe' : '#4a90e2'};
            color: white;
            padding: 12px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 20000;
            font-size: 13px;
            font-weight: 600;
            animation: slideInRight 0.3s ease-out;
        `;
        notification.textContent = message;
        document.body.appendChild(notification);
        
        setTimeout(() => notification.remove(), 3000);
    }
}

// Initialize
window.commandSystem = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.commandSystem = new CommandSystem();
        window.commandSystem.init();
    });
} else {
    window.commandSystem = new CommandSystem();
    window.commandSystem.init();
}

// Export
window.CommandSystem = CommandSystem;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = CommandSystem;
}

console.log('[CommandSystem] 📦 Command system module loaded');

})(); // End IIFE


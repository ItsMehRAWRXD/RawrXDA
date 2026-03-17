/**
 * BigDaddyG IDE - Agentic Executor
 * Allows AI to execute commands, compile code, and iterate autonomously
 * Like Cursor + Devin - full autonomous development loop
 */

(function () {
    if (typeof window !== 'undefined' && window.AgenticExecutorDefined) {
        console.warn('[AgenticExecutor] Already loaded, skipping re-init');
        return;
    }
    if (typeof window !== 'undefined') {
        window.AgenticExecutorDefined = true;
    }

    // Use window.require if available (Electron), otherwise skip Node.js modules
    var agenticSpawn, agenticFs, agenticPath, AgenticSecurityHardening;
    if (typeof window !== 'undefined' && typeof window.require === 'function') {
        const childProcess = window.require('child_process');
        agenticSpawn = childProcess.spawn;
        agenticFs = window.require('fs').promises;
        agenticPath = window.require('path');
        try {
            AgenticSecurityHardening = window.require('./agentic-security-hardening').AgenticSecurityHardening;
        } catch (e) {
            console.warn('[AgenticExecutor] Security hardening not available:', e.message);
        }
    }

    // ============================================================================
    // AGENTIC EXECUTOR CONFIGURATION
    // ============================================================================

    var AgenticConfig = (typeof window !== 'undefined' && window.AgenticConfig) ? window.AgenticConfig : {
        // Safety levels (matches agentic-safety.js)
        safetyLevel: 'BALANCED', // SAFE, BALANCED, AGGRESSIVE, YOLO

        // What AI can do at each level
        permissions: {
            SAFE: {
                readFiles: true,
                writeFiles: false,
                executeCommands: false,
                compileCode: false,
                installPackages: false,
                modifySystem: false
            },
            BALANCED: {
                readFiles: true,
                writeFiles: true,
                executeCommands: true,      // With confirmation
                compileCode: true,
                installPackages: false,
                modifySystem: false
            },
            AGGRESSIVE: {
                readFiles: true,
                writeFiles: true,
                executeCommands: true,       // Auto-execute safe commands
                compileCode: true,
                installPackages: true,        // With confirmation
                modifySystem: false
            },
            YOLO: {
                readFiles: true,
                writeFiles: true,
                executeCommands: true,       // Auto-execute everything
                compileCode: true,
                installPackages: true,        // Auto-install
                modifySystem: true           // Full system access
            }
        },

        // Command whitelist (safe commands that can auto-execute)
        safeCommands: [
            'clang', 'gcc', 'g++',          // Compilers
            'node', 'python', 'cargo',       // Interpreters
            'make', 'cmake', 'ninja',        // Build systems
            'git status', 'git diff',        // Safe git commands
            'ls', 'dir', 'cat', 'type',      // File viewing
            'echo', 'pwd', 'cd'              // Basic commands
        ],

        // Dangerous commands (need confirmation even in YOLO mode)
        dangerousCommands: [
            'rm -rf', 'del /f', 'format',    // File deletion
            'dd', 'fdisk', 'mkfs',           // Disk operations
            'chmod 777', 'chown',            // Permission changes
            'sudo', 'su',                    // Privilege escalation
            'curl | sh', 'wget | sh'         // Remote execution
        ],

        // Auto-retry settings
        maxRetries: 3,
        retryDelay: 1000, // ms

        // Execution timeout
        commandTimeout: 30000, // 30 seconds

        // Logging
        logAllCommands: true,
        logToFile: true
    };

    // ============================================================================
    // AGENTIC EXECUTOR CLASS
    // ============================================================================

    class AgenticExecutor {
        constructor() {
            this.executionHistory = [];
            this.currentSession = {
                task: null,
                steps: [],
                startTime: null,
                endTime: null
            };
            this.workingDirectory = (typeof process !== 'undefined' && process.cwd) ? process.cwd() : '/';

            // Initialize security hardening
            this.security = new AgenticSecurityHardening(AgenticConfig.safetyLevel);

            console.log('[Agentic Executor] 🤖 Initialized');
            console.log(`[Agentic Executor] 🛡️ Safety Level: ${AgenticConfig.safetyLevel}`);
            console.log('[Agentic Executor] 🔒 Security hardening ACTIVE');
        }

        // ========================================================================
        // CORE EXECUTION METHODS
        // ========================================================================

        /**
         * Execute a full agentic task
         * Example: "Create a C program that prints Hello World and compile it"
         */
        async executeTask(task, onProgress) {
            console.log(`[Agentic Executor] 🎯 Starting task: ${task}`);

            this.currentSession = {
                task: task,
                steps: [],
                startTime: Date.now(),
                endTime: null
            };

            try {
                // Step 1: Plan the task
                const plan = await this.planTask(task);
                onProgress({ type: 'plan', plan });

                // Step 2: Execute each step
                for (const step of plan.steps) {
                    const result = await this.executeStep(step, onProgress);

                    // If step failed, try to fix
                    if (!result.success) {
                        const fixed = await this.fixAndRetry(step, result, onProgress);
                        if (!fixed) {
                            throw new Error(`Failed to complete step: ${step.description}`);
                        }
                    }
                }

                // Step 3: Verify completion
                const verification = await this.verifyTask(task, onProgress);

                this.currentSession.endTime = Date.now();
                this.currentSession.success = verification.success;

                console.log('[Agentic Executor] ✅ Task completed successfully');
                return {
                    success: true,
                    session: this.currentSession,
                    verification
                };

            } catch (error) {
                console.error('[Agentic Executor] ❌ Task failed:', error);
                this.currentSession.endTime = Date.now();
                this.currentSession.success = false;
                this.currentSession.error = error.message;

                return {
                    success: false,
                    error: error.message,
                    session: this.currentSession
                };
            }
        }

        /**
         * Plan a task into executable steps
         */
        async planTask(task) {
            // Query BigDaddyG to create a plan
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Break down this task into executable steps. Be specific about files to create and commands to run:\n\n${task}`,
                    model: 'BigDaddyG:Latest',
                    mode: 'Plan'
                })
            });

            const data = await response.json();

            // Parse the plan
            const steps = this.parsePlan(data.response);

            return {
                task,
                steps,
                timestamp: Date.now()
            };
        }

        /**
         * Parse AI response into executable steps
         */
        parsePlan(aiResponse) {
            const steps = [];
            const lines = aiResponse.split('\n');

            let currentStep = null;

            for (const line of lines) {
                // Detect step markers: "1.", "Step 1:", etc.
                if (/^\d+\.|^Step \d+:/i.test(line.trim())) {
                    if (currentStep) {
                        steps.push(currentStep);
                    }
                    currentStep = {
                        description: line.replace(/^\d+\.|^Step \d+:/i, '').trim(),
                        type: this.detectStepType(line),
                        actions: []
                    };
                }
                // Detect commands: lines starting with $ or >
                else if (/^[$>]/.test(line.trim())) {
                    const command = line.replace(/^[$>]/, '').trim();
                    if (currentStep) {
                        currentStep.actions.push({
                            type: 'command',
                            command: command
                        });
                    }
                }
                // Detect file creation: "Create file.c:"
                else if (/create.*\.[\w]+/i.test(line)) {
                    const match = line.match(/[\w.-]+\.[\w]+/);
                    if (match && currentStep) {
                        currentStep.actions.push({
                            type: 'createFile',
                            filename: match[0]
                        });
                    }
                }
            }

            if (currentStep) {
                steps.push(currentStep);
            }

            return steps;
        }

        /**
         * Detect what type of step this is
         */
        detectStepType(description) {
            const lower = description.toLowerCase();
            if (lower.includes('create') || lower.includes('write')) return 'create';
            if (lower.includes('compile') || lower.includes('build')) return 'compile';
            if (lower.includes('run') || lower.includes('execute')) return 'execute';
            if (lower.includes('test')) return 'test';
            if (lower.includes('fix') || lower.includes('debug')) return 'debug';
            return 'general';
        }

        /**
         * Execute a single step
         */
        async executeStep(step, onProgress) {
            console.log(`[Agentic Executor] 🔧 Executing: ${step.description}`);

            const stepResult = {
                step: step.description,
                startTime: Date.now(),
                endTime: null,
                success: false,
                output: '',
                error: null
            };

            onProgress({ type: 'step_start', step });

            try {
                for (const action of step.actions) {
                    if (action.type === 'command') {
                        const result = await this.executeCommand(action.command, onProgress);
                        stepResult.output += result.output;
                        if (!result.success) {
                            stepResult.error = result.error;
                            throw new Error(result.error);
                        }
                    }
                    else if (action.type === 'createFile') {
                        // Get file content from AI
                        const content = await this.generateFileContent(action.filename, step.description);
                        await this.writeFile(action.filename, content);
                        stepResult.output += `Created ${action.filename}\n`;
                        onProgress({ type: 'file_created', filename: action.filename });
                    }
                }

                stepResult.success = true;
                stepResult.endTime = Date.now();
                this.currentSession.steps.push(stepResult);

                onProgress({ type: 'step_complete', step, result: stepResult });

                return stepResult;

            } catch (error) {
                stepResult.endTime = Date.now();
                stepResult.error = error.message;
                this.currentSession.steps.push(stepResult);

                onProgress({ type: 'step_error', step, error: error.message });

                return stepResult;
            }
        }

        /**
         * Execute a terminal command
         */
        async executeCommand(command, onProgress) {
            console.log(`[Agentic Executor] 💻 Command: ${command}`);

            // SECURITY: Validate command with hardening layer
            try {
                command = await this.security.validateCommand(command);
            } catch (error) {
                console.error(`[Agentic Executor] 🛡️ Security blocked: ${error.message}`);
                return {
                    success: false,
                    error: `Security check failed: ${error.message}`,
                    output: ''
                };
            }

            // Check for blocked binaries
            if (this.security.isBlockedBinary(command)) {
                console.error(`[Agentic Executor] 🚫 Blocked binary: ${command}`);
                return {
                    success: false,
                    error: 'Binary blocked by security policy',
                    output: ''
                };
            }

            // Safety check
            if (!this.isSafeToExecute(command)) {
                const confirmed = await this.requestPermission(command);
                if (!confirmed) {
                    return {
                        success: false,
                        error: 'User denied permission to execute command',
                        output: ''
                    };
                }
            }

            // Log command (with secret scrubbing)
            this.logCommand(command);

            onProgress({ type: 'command', command });

            return new Promise((resolve) => {
                const parts = command.split(' ');
                const cmd = parts[0];
                const args = parts.slice(1);

                const proc = agenticSpawn(cmd, args, {
                    cwd: this.workingDirectory,
                    shell: true,
                    timeout: AgenticConfig.commandTimeout
                });

                let stdout = '';
                let stderr = '';

                proc.stdout.on('data', (data) => {
                    const text = data.toString();
                    stdout += text;
                    onProgress({ type: 'output', data: text });
                });

                proc.stderr.on('data', (data) => {
                    const text = data.toString();
                    stderr += text;
                    onProgress({ type: 'error_output', data: text });
                });

                proc.on('close', (code) => {
                    if (code === 0) {
                        resolve({
                            success: true,
                            output: stdout,
                            error: null,
                            exitCode: code
                        });
                    } else {
                        resolve({
                            success: false,
                            output: stdout,
                            error: stderr || `Command exited with code ${code}`,
                            exitCode: code
                        });
                    }
                });

                proc.on('error', (error) => {
                    resolve({
                        success: false,
                        output: stdout,
                        error: error.message,
                        exitCode: -1
                    });
                });
            });
        }

        /**
         * Check if command is safe to auto-execute
         */
        isSafeToExecute(command) {
            const level = AgenticConfig.safetyLevel;
            const permissions = AgenticConfig.permissions[level];

            // YOLO mode - execute everything (except truly dangerous)
            if (level === 'YOLO') {
                return !this.isDangerous(command);
            }

            // Check if command is in safe list
            const isSafe = AgenticConfig.safeCommands.some(safe =>
                command.toLowerCase().startsWith(safe.toLowerCase())
            );

            // AGGRESSIVE - auto-execute safe commands
            if (level === 'AGGRESSIVE' && isSafe) {
                return true;
            }

            // BALANCED - need permission for all commands
            if (level === 'BALANCED' && permissions.executeCommands) {
                return false; // Request permission
            }

            // SAFE - no command execution
            return false;
        }

        /**
         * Check if command is dangerous
         */
        isDangerous(command) {
            return AgenticConfig.dangerousCommands.some(dangerous =>
                command.toLowerCase().includes(dangerous.toLowerCase())
            );
        }

        /**
         * Request user permission to execute command
         */
        async requestPermission(command) {
            // In Electron, show dialog
            if (window.electron) {
                return await window.electron.showMessageBox({
                    type: 'question',
                    buttons: ['Allow', 'Deny'],
                    defaultId: 1,
                    title: 'Agentic Execution',
                    message: 'Allow AI to execute this command?',
                    detail: command
                });
            }

            // Fallback to confirm
            return confirm(`Allow AI to execute this command?\n\n${command}`);
        }

        /**
         * Generate file content using AI
         */
        async generateFileContent(filename, context) {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Generate the complete content for ${filename}. Context: ${context}\n\nOnly output the raw file content, no explanations.`,
                    model: 'BigDaddyG:Code'
                })
            });

            const data = await response.json();

            // Extract code block if present
            const codeMatch = data.response.match(/```[\w]*\n([\s\S]*?)```/);
            if (codeMatch) {
                return codeMatch[1];
            }

            return data.response;
        }

        /**
         * Write file to disk
         */
        async writeFile(filename, content) {
            const fullPath = agenticPath.join(this.workingDirectory, filename);
            await agenticFs.writeFile(fullPath, content, 'utf8');
            console.log(`[Agentic Executor] 📝 Created file: ${filename}`);
        }

        /**
         * Fix and retry a failed step
         */
        async fixAndRetry(step, failedResult, onProgress) {
            console.log(`[Agentic Executor] 🔧 Attempting to fix: ${step.description}`);

            onProgress({ type: 'fixing', step, error: failedResult.error });

            // Ask BigDaddyG to fix the issue
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `This step failed:\n\nStep: ${step.description}\nError: ${failedResult.error}\nOutput: ${failedResult.output}\n\nHow do I fix this? Provide the corrected command or code.`,
                    model: 'BigDaddyG:Latest',
                    mode: 'Debug'
                })
            });

            const data = await response.json();

            // Try to extract and execute fix
            const fixedCommand = this.extractCommand(data.response);
            if (fixedCommand) {
                const result = await this.executeCommand(fixedCommand, onProgress);
                return result.success;
            }

            return false;
        }

        /**
         * Extract command from AI response
         */
        extractCommand(text) {
            // Look for command markers
            const match = text.match(/[$>]\s*(.+)/);
            if (match) {
                return match[1].trim();
            }

            // Look for code blocks
            const codeMatch = text.match(/```(?:bash|sh|shell)?\n(.+?)\n```/s);
            if (codeMatch) {
                return codeMatch[1].trim();
            }

            return null;
        }

        /**
         * Verify task completion
         */
        async verifyTask(task, onProgress) {
            onProgress({ type: 'verifying' });

            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Verify if this task was completed successfully:\n\nTask: ${task}\n\nSteps completed:\n${this.currentSession.steps.map(s => `- ${s.step}: ${s.success ? 'SUCCESS' : 'FAILED'}`).join('\n')}\n\nWas the task completed? Answer YES or NO and explain.`,
                    model: 'BigDaddyG:Latest'
                })
            });

            const data = await response.json();
            const success = /yes/i.test(data.response);

            return {
                success,
                explanation: data.response
            };
        }

        /**
         * Log command execution
         */
        logCommand(command) {
            // SECURITY: Scrub secrets before logging
            const scrubbedCommand = this.security.scrubSecrets(command);

            const entry = {
                command: scrubbedCommand,
                timestamp: Date.now(),
                safetyLevel: AgenticConfig.safetyLevel,
                workingDirectory: this.workingDirectory
            };

            this.executionHistory.push(entry);

            if (AgenticConfig.logAllCommands) {
                console.log(`[Agentic Executor] 📋 ${scrubbedCommand}`);
            }
        }

        // ========================================================================
        // UTILITY METHODS
        // ========================================================================

        setSafetyLevel(level) {
            if (!AgenticConfig.permissions[level]) {
                throw new Error(`Invalid safety level: ${level}`);
            }
            AgenticConfig.safetyLevel = level;
            console.log(`[Agentic Executor] 🛡️ Safety level set to: ${level}`);
        }

        getSafetyLevel() {
            return AgenticConfig.safetyLevel;
        }

        getPermissions() {
            return AgenticConfig.permissions[AgenticConfig.safetyLevel];
        }

        getExecutionHistory() {
            return this.executionHistory;
        }

        getCurrentSession() {
            return this.currentSession;
        }

        setWorkingDirectory(dir) {
            this.workingDirectory = dir;
            console.log(`[Agentic Executor] 📁 Working directory: ${dir}`);
        }
    }

    // ============================================================================
    // SINGLETON
    // ============================================================================

    var agenticExecutor = null;

    function getAgenticExecutor() {
        if (!agenticExecutor) {
            agenticExecutor = new AgenticExecutor();
        }
        return agenticExecutor;
    }

    // ============================================================================
    // EXPORTS
    // ============================================================================

    if (typeof window !== 'undefined') {
        window.AgenticExecutor = AgenticExecutor;
        window.getAgenticExecutor = getAgenticExecutor;
        window.AgenticConfig = AgenticConfig;
    }

    if (typeof module !== 'undefined' && module.exports) {
        module.exports = {
            AgenticExecutor,
            getAgenticExecutor,
            AgenticConfig
        };
    }

})();


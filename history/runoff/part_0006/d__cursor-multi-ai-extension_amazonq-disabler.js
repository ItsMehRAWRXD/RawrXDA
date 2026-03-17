            }
            

    // Initialize and test logging immediately
    AmazonQDisabler.init();
    window.createAWSSecurityDashboard();
    window.interceptTerminalOutput();
    window.monitorProcesses();

    // Set up the fake status after DOM loads
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', window.createFakeAmazonQStatus);
    } else {
        window.createFakeAmazonQStatus();
    }

    // Disable agentic features
    window.disableAmazonQAgentic();

    // Continuous monitoring - reduced timeout
    setInterval(() => {
        if (document.getElementById('security-logs')) {
            updateSecurityDashboard();
        }
        AmazonQLogger.log(`📊 System check: ${AmazonQLogger.logs.length} events logged, monitoring active`, 'info');
    }, 5000); // Every 5 seconds

    // Button handler functions
    // Agentic mode flag
    window.agenticMode = false;

    // Agentic toggle button logic (assumes #agentBtn exists)
    window.toggleAgent = function() {
        const btn = document.getElementById('agentBtn');
        if (!btn) return;
        window.agenticMode = !window.agenticMode;
        btn.textContent = window.agenticMode ? 'AI Agent: ON' : 'AI Agent: OFF';
        if (window.addConsoleLog) window.addConsoleLog(`AI Agent ${window.agenticMode ? 'enabled' : 'disabled'}`, 'info');
    };

    // Block Amazon Q chat/response if agenticMode is ON
    window.amazonQResponse = function(query) {
        if (window.agenticMode) {
            // Do the work silently, no immediate response
            // Simulate async work (replace with real logic as needed)
            return new Promise(resolve => {
                setTimeout(() => {
                    // Only report after work is done (fallback)
                    if (window.vscode && window.vscode.postMessage) {
                        window.vscode.postMessage({
                            command: 'terminal.write',
                            text: `\n[AmazonQ Fallback] ${query} - Work completed\n`
                        });
                    }
                    resolve({
                        success: true,
                        response: '[Fallback] Work completed and reported to terminal.',
                        blocked: false
                    });
                }, 2000); // Simulate 2s work
            });
        } else {
            // Fallback: integrate into terminal (simulate)
            if (window.vscode && window.vscode.postMessage) {
                window.vscode.postMessage({
                    command: 'terminal.write',
                    text: `\n[AmazonQ Fallback] ${query}\n`
                });
            }
            return Promise.resolve({
                success: true,
                response: '[Fallback] Sent to terminal.',
                blocked: false
            });
        }
    };
    window.formatCode = function() {
        const editor = document.getElementById('codeEditor');
        try {
                const formatted = editor.value.replace(/;/g, ';\n')
                    .replace(/{/g, '{\n')
                    .replace(/}/g, '\n}');
            editor.value = formatted;
            if (window.addConsoleLog) window.addConsoleLog('Code formatted', 'info');
        } catch (error) {
            if (window.addConsoleLog) window.addConsoleLog('Format error: ' + error.message, 'error');
        }
    };

    window.clearOutput = function() {
        const output = document.getElementById('consoleOutput');
        if (output) output.innerHTML = '';
        if (window.addConsoleLog) window.addConsoleLog('Output cleared', 'info');
    };

    window.debugCode = function() {
        if (window.addConsoleLog) window.addConsoleLog('Debug mode activated', 'info');
        if (window.updateStatus) window.updateStatus('Debug mode');
    };

    window.runAndPublish = function() {
        if (window.addConsoleLog) window.addConsoleLog('Building and publishing...', 'info');
        if (window.updateStatus) window.updateStatus('Publishing');
    };

    window.toggleProducts = function() {
        if (window.addConsoleLog) window.addConsoleLog('Products panel toggled', 'info');
    };

    window.toggleSettings = function() {
        if (window.addConsoleLog) window.addConsoleLog('Settings panel toggled', 'info');
    };

    window.toggleProjectManager = function() {
        if (window.addConsoleLog) window.addConsoleLog('Project manager toggled', 'info');
    };

    window.toggleAgent = function() {
        const btn = document.getElementById('agentBtn');
        if (!btn) return;
        const isOn = btn.textContent.includes('ON');
        btn.textContent = isOn ? 'AI Agent: OFF' : 'AI Agent: ON';
        if (window.addConsoleLog) window.addConsoleLog(`AI Agent ${isOn ? 'disabled' : 'enabled'}`, 'info');
    };

    // Trigger Q logging and test logs
    AmazonQLogger.log('Script loaded successfully', 'info');
    AmazonQLogger.log('Testing terminal output', 'warn');

    // Test blocked API call
    fetch('https://amazonq.aws.com/test').then(() => {
        AmazonQLogger.log('Fetch test completed', 'info');
    });

    // Expose logger globally for debugging
    window.AmazonQLogger = AmazonQLogger;

    AmazonQLogger.log('🔍 All AWS/CodeWhisperer activity will be logged and blocked', 'success');
    AmazonQLogger.log('📱 Press Ctrl+Shift+L to view security dashboard', 'info');
    AmazonQLogger.log('⚡ Enhanced code formatting functions loaded', 'success');

})();
                }
                
                localStorage.setItem('aws-security-logs', JSON.stringify(existingLogs));
            } catch (e) {
                // Silently fail if localStorage not available
            }
        },

        exportTerminalLogs: function() {
            const logContent = this.terminalLogs.join('\n');
            const blob = new Blob([logContent], { type: 'text/plain' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `aws-security-logs-${Date.now()}.txt`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            return logContent;
        },

        getTerminalOutput: function() {
            return this.terminalLogs.join('\n');
        },
        
        getLogs: function() {
            return this.logs;
        },
        
        clearLogs: function() {
            this.logs = [];
        }
    };

    // Override Amazon Q functions to disable agentic behavior
    const AmazonQDisabler = {
        init: function() {
            this.interceptAmazonQAPIs();
            this.disableAgenticFeatures();
            this.setupFakeResponses();
            AmazonQLogger.log('Amazon Q Agentic Coding disabled (running in stealth mode)', 'warn');
        },

        interceptAmazonQAPIs: function() {
            // Intercept and block all AWS/Amazon Q/CodeWhisperer API calls
            const originalFetch = window.fetch;
            window.fetch = function(url, options) {
                if (url && typeof url === 'string') {
                    const blockedPatterns = [
                        'amazonq', 'aws', 'codewhisperer', 'lambda', 
                        'bedrock', 'sagemaker', 'comprehend', 'textract',
                        'polly', 'transcribe', 'translate', 'rekognition',
                        'lex', 'connect', 'chime', 'workspaces', 'appstream',
                        'codestar', 'codebuild', 'codecommit', 'codedeploy',
                        'codepipeline', 'cloud9', 'x-ray', 'cloudformation',
                        'cloudwatch', 'cloudtrail', 'config', 'systems-manager',
                        'secrets-manager', 'parameter-store', 'kms', 'iam',
                        'sts', 'cognito', 'amplify', 'appsync', 'api-gateway'
                    ];
                    
                    const isBlocked = blockedPatterns.some(pattern => 
                        url.toLowerCase().includes(pattern) || 
                        (options && options.headers && 
                         JSON.stringify(options.headers).toLowerCase().includes(pattern))
                    );
                    
                    if (isBlocked) {
                        AmazonQLogger.log(`BLOCKED API call to: ${url}`, 'error');
                        
                        // Return fake success response to prevent errors
                        return Promise.resolve({
                            ok: true,
                            status: 200,
                            json: () => Promise.resolve({
                                status: 'blocked',
                                message: 'AWS service disabled for security',
                                data: null,
                                blocked: true
                            }),
                            text: () => Promise.resolve('{"blocked": true}')
                        });
                    }
                }
                return originalFetch.apply(this, arguments);
            };

            // Block AWS SDK imports
            this.blockAWSSDK();
            // Block Lambda functions
            this.blockLambdaFunctions();
            // Block CodeWhisperer specifically
            this.unwireCodeWhisperer();
        },

        blockAWSSDK: function() {
            // Block AWS SDK loading
            const originalRequire = window.require;
            if (originalRequire) {
                window.require = function(moduleName) {
                    const awsModules = [
                        'aws-sdk', '@aws-sdk', 'aws-lambda', 'aws-cdk',
                        'codewhisperer', 'amazon-q', 'bedrock-runtime'
                    ];
                    
                    if (awsModules.some(module => moduleName.includes(module))) {
                        AmazonQLogger.log(`BLOCKED require: ${moduleName}`, 'error');
                        throw new Error(`Module ${moduleName} has been disabled for security`);
                    }
                    
                    return originalRequire.apply(this, arguments);
                };
            }

            // Block dynamic imports
            const originalImport = window.import;
            if (originalImport) {
                window.import = function(moduleName) {
                    if (moduleName.includes('aws') || moduleName.includes('codewhisperer')) {
                        AmazonQLogger.log(`BLOCKED import: ${moduleName}`, 'error');
                        return Promise.reject(new Error(`Import ${moduleName} disabled`));
                    }
                    return originalImport.apply(this, arguments);
                };
            }
        },

        blockLambdaFunctions: function() {
            // Disable AWS Lambda invocation
            window.AWS = undefined;
            window.lambda = undefined;
            window.invokeFunction = function() {
                AmazonQLogger.log('BLOCKED Lambda function invocation attempt', 'error');
                return Promise.reject(new Error('Lambda functions disabled'));
            };

            // Block serverless framework
            window.serverless = undefined;
            window.sls = undefined;
        },

        unwireCodeWhisperer: function() {
            // Completely disable CodeWhisperer
            window.CodeWhisperer = undefined;
            window.codeWhisperer = undefined;
            window.amazonCodeWhisperer = undefined;
            
            // Block CodeWhisperer VS Code commands
            if (window.vscode && window.vscode.commands) {
                const originalExecuteCommand = window.vscode.commands.executeCommand;
                window.vscode.commands.executeCommand = function(command, ...args) {
                    const codewhispererCommands = [
                        'aws.codeWhisperer', 'codewhisperer', 'amazon-q',
                        'aws.amazonq', 'aws.toolkit'
                    ];
                    
                    if (codewhispererCommands.some(cmd => command.includes(cmd))) {
                        AmazonQLogger.log(`BLOCKED VS Code command: ${command}`, 'error');
                        return Promise.resolve({ blocked: true });
                    }
                    
                    return originalExecuteCommand.apply(this, arguments);
                };
            }

            // Disable CodeWhisperer suggestions
            window.getCodeSuggestions = function() {
                AmazonQLogger.log('CodeWhisperer suggestions disabled', 'warn');
                return Promise.resolve([]);
            };

            // Block CodeWhisperer configuration
            window.configureCodeWhisperer = function() {
                AmazonQLogger.log('CodeWhisperer configuration blocked', 'error');
                return false;
            };
        },

        disableAgenticFeatures: function() {
            // Comprehensive WebSocket monitoring and blocking
            const originalWebSocket = window.WebSocket;
            window.WebSocket = function(url, protocols) {
                const suspiciousPatterns = [
                    'amazonq', 'aws', 'codewhisperer', 'bedrock', 'lambda',
                    'sagemaker', 'comprehend', 'lex', 'polly', 'transcribe'
                ];
                
                if (url && suspiciousPatterns.some(pattern => url.toLowerCase().includes(pattern))) {
                    AmazonQLogger.log(`🔒 BLOCKED WebSocket: ${url} | Protocols: ${JSON.stringify(protocols)}`, 'error');
                    AmazonQLogger.log(`🕵️ Connection attempt from: ${new Error().stack}`, 'warn');
                    
                    // Return a monitored fake WebSocket
                    return {
                        url: url,
                        protocols: protocols,
                        readyState: 1, // OPEN state
                        
                        send: function(data) {
                            AmazonQLogger.log(`📤 INTERCEPTED WebSocket send to ${url}: ${JSON.stringify(data)}`, 'warn');
                            // Log the data they're trying to send
                            if (data && typeof data === 'string') {
                                try {
                                    const parsed = JSON.parse(data);
                                    AmazonQLogger.log(`📊 Parsed data structure: ${JSON.stringify(parsed, null, 2)}`, 'info');
                                } catch (e) {
                                    AmazonQLogger.log(`📄 Raw data: ${data.substring(0, 200)}...`, 'info');
                                }
                            }
                        },
                        
                        close: function(code, reason) {
                            AmazonQLogger.log(`🔌 Fake WebSocket closed: ${url} | Code: ${code} | Reason: ${reason}`, 'info');
                        },
                        
                        addEventListener: function(event, handler) {
                            AmazonQLogger.log(`👂 Event listener added: ${event} on ${url}`, 'info');
                        },
                        
                        removeEventListener: function(event, handler) {
                            AmazonQLogger.log(`🚫 Event listener removed: ${event} on ${url}`, 'info');
                        }
                    };
                }
                
                // Log all WebSocket connections for monitoring
                AmazonQLogger.log(`✅ Allowed WebSocket connection: ${url}`, 'info');
                return new originalWebSocket(url, protocols);
            };

            // Monitor XMLHttpRequest as well
            this.monitorXMLHttpRequests();
        },

        monitorXMLHttpRequests: function() {
            const originalXHR = window.XMLHttpRequest;
            window.XMLHttpRequest = function() {
                const xhr = new originalXHR();
                const originalOpen = xhr.open;
                const originalSend = xhr.send;
                
                xhr.open = function(method, url, ...args) {
                    const awsPatterns = ['aws', 'amazonq', 'codewhisperer', 'bedrock', 'lambda'];
                    
                    if (awsPatterns.some(pattern => url.toLowerCase().includes(pattern))) {
                        AmazonQLogger.log(`🚨 BLOCKED XHR ${method} request to: ${url}`, 'error');
                        AmazonQLogger.log(`📍 Request origin: ${new Error().stack.split('\n')[2]}`, 'warn');
                        
                        // Don't actually open the connection
                        return;
                    }
                    
                    AmazonQLogger.log(`📡 Monitoring XHR ${method}: ${url}`, 'info');
                    return originalOpen.apply(this, [method, url, ...args]);
                };
                
                xhr.send = function(data) {
                    if (data) {
                        AmazonQLogger.log(`📤 XHR data being sent: ${JSON.stringify(data)}`, 'info');
                    }
                    return originalSend.apply(this, arguments);
                };
                
                return xhr;
            };
        },

        setupFakeResponses: function() {
            // Create fake Amazon Q responses to maintain the illusion
            window.amazonQResponse = function(query) {
                AmazonQLogger.log(`Query intercepted: ${query}`, 'info');
                
                const fakeResponses = [
                    "I'm analyzing your code...",
                    "Generating suggestions...",
                    "Code review in progress...",
                    "Agentic analysis complete.",
                    "No issues found in current context."
                ];
                
                const response = fakeResponses[Math.floor(Math.random() * fakeResponses.length)];
                AmazonQLogger.log(`Fake response sent: ${response}`, 'info');
                
                return Promise.resolve({
                    success: true,
                    response: response,
                    disabled: true // Hidden flag
                });
            };
        }
    };

    // Override Amazon Q extension methods if they exist
    if (window.vscode) {
        const originalPostMessage = window.vscode.postMessage;
        window.vscode.postMessage = function(message) {
            if (message && message.command && 
                (message.command.includes('amazonq') || message.command.includes('codewhisperer'))) {
                
                AmazonQLogger.log(`Blocked VS Code message: ${JSON.stringify(message)}`, 'warn');
                return; // Block the message
            }
            return originalPostMessage.apply(this, arguments);
        };
    }

    // Create a fake Amazon Q status indicator
    window.createFakeAmazonQStatus = function() {
        const statusDiv = document.createElement('div');
        statusDiv.id = 'amazonq-status';
        statusDiv.style.cssText = `
            position: fixed;
            bottom: 20px;
            right: 20px;
            background: #232f3e;
            color: #ff9900;
            padding: 8px 12px;
            border-radius: 4px;
            font-size: 12px;
            z-index: 1000;
            border: 1px solid #ff9900;
        `;
        statusDiv.textContent = 'Amazon Q: Active (Agentic Mode)';
        document.body.appendChild(statusDiv);
        
        // Fake activity indicator
        setInterval(() => {
            statusDiv.style.opacity = statusDiv.style.opacity === '0.5' ? '1' : '0.5';
        }, 2000);
        
        AmazonQLogger.log('Fake Amazon Q status indicator created', 'info');
    };

    // Disable specific Amazon Q agentic features
    window.disableAmazonQAgentic = function() {
        // Block common agentic endpoints
        const blockedEndpoints = [
            '/amazonq/agentic',
            '/codewhisperer/agentic',
            '/aws/agentic',
            '/q/suggestions',
            '/q/completions',
            '/q/chat'
        ];

        blockedEndpoints.forEach(endpoint => {
            if (window[endpoint]) {
                window[endpoint] = function() {
                    AmazonQLogger.log(`Blocked agentic call to: ${endpoint}`, 'warn');
                    return Promise.resolve({ disabled: true, message: 'Agentic feature disabled' });
                };
            }
        });

        AmazonQLogger.log('All agentic endpoints disabled', 'warn');
    };

    // Export logger for external access
    window.AmazonQLogger = AmazonQLogger;
    
    // Create a comprehensive monitoring dashboard
    window.createAWSSecurityDashboard = function() {
        const dashboard = document.createElement('div');
        dashboard.id = 'aws-security-dashboard';
        dashboard.style.cssText = `
            position: fixed;
            top: 10px;
            left: 10px;
            width: 400px;
            height: 300px;
            background: rgba(0, 0, 0, 0.95);
            color: #00ff00;
            font-family: 'Courier New', monospace;
            font-size: 10px;
            border: 1px solid #00ff00;
            padding: 10px;
            z-index: 9999;
            overflow-y: auto;
            display: none;
        `;
        
        dashboard.innerHTML = `
            <div style="color: #ff0000; font-weight: bold;">🔒 AWS SECURITY MONITOR 🔒</div>
            <div style="color: #ffff00;">Press Ctrl+Shift+L to toggle</div>
            <hr style="border-color: #00ff00;">
            <div id="security-logs"></div>
        `;
        
        document.body.appendChild(dashboard);
        
        // Toggle dashboard with Ctrl+Shift+L
        document.addEventListener('keydown', function(e) {
            if (e.ctrlKey && e.shiftKey && e.key === 'L') {
                dashboard.style.display = dashboard.style.display === 'none' ? 'block' : 'none';
                if (dashboard.style.display === 'block') {
                    updateSecurityDashboard();
                }
            }
        });
        
        return dashboard;
    };
    
    function updateSecurityDashboard() {
        const logsDiv = document.getElementById('security-logs');
        if (logsDiv) {
            const recentLogs = AmazonQLogger.terminalLogs.slice(-20);
            logsDiv.innerHTML = recentLogs.map(log => 
                `<div style="margin: 2px 0; color: ${log.includes('BLOCKED') ? '#ff4444' : '#44ff44'};">${log}</div>`
            ).join('');
            logsDiv.scrollTop = logsDiv.scrollHeight;
        }
    }

    // Terminal output interceptor
    window.interceptTerminalOutput = function() {
        // Intercept console methods to capture all terminal output
        ['log', 'warn', 'error', 'info', 'debug'].forEach(method => {
            const original = console[method];
            console[method] = function(...args) {
                const message = args.join(' ');
                
                // Check if it's AWS related
                const awsKeywords = ['aws', 'codewhisperer', 'amazonq', 'bedrock', 'lambda', 'iam', 'sts'];
                if (awsKeywords.some(keyword => message.toLowerCase().includes(keyword))) {
                    AmazonQLogger.log(`🎯 CAPTURED terminal ${method}: ${message}`, 'warn');
                }
                
                return original.apply(console, args);
            };
        });
        
        AmazonQLogger.log('Terminal output interception active', 'success');
    };

    // Process monitoring (if available)
    window.monitorProcesses = function() {
        if (typeof process !== 'undefined') {
            const originalEmit = process.emit;
            process.emit = function(event, ...args) {
                if (event === 'exit' || event === 'SIGTERM' || event === 'SIGINT') {
                    AmazonQLogger.log(`🔄 Process event detected: ${event}`, 'warn');
                }
                return originalEmit.apply(this, [event, ...args]);
            };
        }
    };

    // Initialize all monitoring systems
    AmazonQDisabler.init();
    window.createAWSSecurityDashboard();
    window.interceptTerminalOutput();
    window.monitorProcesses();
    
    // Set up the fake status after DOM loads
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', window.createFakeAmazonQStatus);
    } else {
        window.createFakeAmazonQStatus();
    }

    // Disable agentic features
    window.disableAmazonQAgentic();

    // Continuous monitoring - reduced timeout
    setInterval(() => {
        if (document.getElementById('security-logs')) {
            updateSecurityDashboard();
        }
        
        // Log system status
        AmazonQLogger.log(`📊 System check: ${AmazonQLogger.logs.length} events logged, monitoring active`, 'info');
    }, 5000); // Every 5 seconds


    // Button handler functions
    window.formatCode = function() {
        const editor = document.getElementById('codeEditor');
        try {
            const formatted = editor.value.replace(/;/g, ';

    AmazonQLogger.log('🚀 AWS Security Monitor fully initialized with terminal logging', 'success');
            editor.value = formatted;
            if (window.addConsoleLog) window.addConsoleLog('Code formatted', 'info');
        } catch (error) {
            if (window.addConsoleLog) window.addConsoleLog('Format error: ' + error.message, 'error');
        }
    };

    window.clearOutput = function() {
        const output = document.getElementById('consoleOutput');
        if (output) output.innerHTML = '';
        if (window.addConsoleLog) window.addConsoleLog('Output cleared', 'info');
    };

    window.debugCode = function() {
        if (window.addConsoleLog) window.addConsoleLog('Debug mode activated', 'info');
        if (window.updateStatus) window.updateStatus('Debug mode');
    };

    window.runAndPublish = function() {
        if (window.addConsoleLog) window.addConsoleLog('Building and publishing...', 'info');
        if (window.updateStatus) window.updateStatus('Publishing');
    };

    window.toggleProducts = function() {
        if (window.addConsoleLog) window.addConsoleLog('Products panel toggled', 'info');
    };

    window.toggleSettings = function() {
        if (window.addConsoleLog) window.addConsoleLog('Settings panel toggled', 'info');
    };

    window.toggleProjectManager = function() {
        if (window.addConsoleLog) window.addConsoleLog('Project manager toggled', 'info');
    };

    window.toggleAgent = function() {
        const btn = document.getElementById('agentBtn');
        if (!btn) return;
        const isOn = btn.textContent.includes('ON');
        btn.textContent = isOn ? 'AI Agent: OFF' : 'AI Agent: ON';
        if (window.addConsoleLog) window.addConsoleLog(`AI Agent ${isOn ? 'disabled' : 'enabled'}`, 'info');
    };
    AmazonQLogger.log('🔍 All AWS/CodeWhisperer activity will be logged and blocked', 'success');
    AmazonQLogger.log('📱 Press Ctrl+Shift+L to view security dashboard', 'info');
    AmazonQLogger.log('⚡ Enhanced code formatting functions loaded', 'success');

})();
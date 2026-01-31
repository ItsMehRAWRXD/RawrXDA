#!/usr/bin/env node

const axios = require('axios');
const WebSocket = require('ws');
const { spawn } = require('child_process');

class CompleteSystemTester {
    constructor() {
        this.services = {
            airtightOllama: { port: 11434, name: 'Airtight Ollama Server' },
            spoofedAI: { port: 9999, name: 'Spoofed AI Server' },
            ideBackend: { port: 3001, name: 'IDE Backend Server' },
            rawrzServer: { port: 8080, name: 'RawrZ Server' }
        };
        this.testResults = [];
    }

    async runAllTests() {
        console.log(' Starting Complete System Test Suite');
        console.log('=' .repeat(60));
        
        // Test 1: Service Health Checks
        await this.testServiceHealth();
        
        // Test 2: AI Model Availability
        await this.testAIModelAvailability();
        
        // Test 3: Ollama API Compatibility
        await this.testOllamaCompatibility();
        
        // Test 4: Spoofed API Functionality
        await this.testSpoofedAPIFunctionality();
        
        // Test 5: IDE Backend Integration
        await this.testIDEBackendIntegration();
        
        // Test 6: WebSocket Communication
        await this.testWebSocketCommunication();
        
        // Test 7: End-to-End AI Generation
        await this.testEndToEndGeneration();
        
        // Test 8: Security and Stealth
        await this.testSecurityAndStealth();
        
        // Print Results
        this.printTestResults();
    }

    async testServiceHealth() {
        console.log('\n Testing Service Health...');
        
        for (const [serviceName, service] of Object.entries(this.services)) {
            try {
                const response = await axios.get(`http://localhost:${service.port}/health`, { timeout: 5000 });
                this.addTestResult('PASS', ` ${service.name} is healthy`, {
                    service: serviceName,
                    status: response.status,
                    data: response.data
                });
            } catch (error) {
                this.addTestResult('FAIL', ` ${service.name} is not responding`, {
                    service: serviceName,
                    error: error.message
                });
            }
        }
    }

    async testAIModelAvailability() {
        console.log('\n Testing AI Model Availability...');
        
        const models = ['gpt-5', 'claude-3.5', 'gemini-2.0', 'copilot'];
        
        for (const model of models) {
            try {
                const response = await axios.post('http://localhost:11434/api/generate', {
                    model: `${model}:latest`,
                    prompt: 'Hello, are you working?',
                    stream: false
                }, { timeout: 10000 });
                
                if (response.data && response.data.response) {
                    this.addTestResult('PASS', ` ${model} model is available and responding`, {
                        model,
                        response: response.data.response.substring(0, 100) + '...'
                    });
                } else {
                    this.addTestResult('FAIL', ` ${model} model response is invalid`, {
                        model,
                        response: response.data
                    });
                }
            } catch (error) {
                this.addTestResult('FAIL', ` ${model} model is not working`, {
                    model,
                    error: error.message
                });
            }
        }
    }

    async testOllamaCompatibility() {
        console.log('\n Testing Ollama API Compatibility...');
        
        try {
            // Test /api/tags endpoint
            const tagsResponse = await axios.get('http://localhost:11434/api/tags');
            if (tagsResponse.data && tagsResponse.data.models) {
                this.addTestResult('PASS', ' Ollama /api/tags endpoint working', {
                    models: tagsResponse.data.models.length
                });
            } else {
                this.addTestResult('FAIL', ' Ollama /api/tags endpoint not working');
            }
            
            // Test /api/version endpoint
            const versionResponse = await axios.get('http://localhost:11434/api/version');
            if (versionResponse.data && versionResponse.data.version) {
                this.addTestResult('PASS', ' Ollama /api/version endpoint working', {
                    version: versionResponse.data.version
                });
            } else {
                this.addTestResult('FAIL', ' Ollama /api/version endpoint not working');
            }
            
            // Test /api/ps endpoint
            const psResponse = await axios.get('http://localhost:11434/api/ps');
            if (psResponse.data && psResponse.data.models) {
                this.addTestResult('PASS', ' Ollama /api/ps endpoint working', {
                    activeModels: psResponse.data.models.length
                });
            } else {
                this.addTestResult('FAIL', ' Ollama /api/ps endpoint not working');
            }
            
        } catch (error) {
            this.addTestResult('FAIL', ' Ollama API compatibility test failed', {
                error: error.message
            });
        }
    }

    async testSpoofedAPIFunctionality() {
        console.log('\n Testing Spoofed API Functionality...');
        
        const providers = ['openai', 'claude', 'gemini', 'copilot'];
        
        for (const provider of providers) {
            try {
                const response = await axios.post(`http://localhost:9999/api/${provider}/unlock`, {
                    model: `${provider}-latest`,
                    messages: [
                        {
                            role: 'user',
                            content: 'Test message for spoofed API'
                        }
                    ],
                    stream: false
                }, { timeout: 10000 });
                
                if (response.data && response.data.choices && response.data.choices[0]) {
                    this.addTestResult('PASS', ` Spoofed API for ${provider} working`, {
                        provider,
                        response: response.data.choices[0].message.content.substring(0, 100) + '...'
                    });
                } else {
                    this.addTestResult('FAIL', ` Spoofed API for ${provider} not working`);
                }
            } catch (error) {
                this.addTestResult('FAIL', ` Spoofed API for ${provider} failed`, {
                    provider,
                    error: error.message
                });
            }
        }
    }

    async testIDEBackendIntegration() {
        console.log('\n Testing IDE Backend Integration...');
        
        try {
            // Test models endpoint
            const modelsResponse = await axios.get('http://localhost:3001/api/models');
            if (modelsResponse.data && modelsResponse.data.models) {
                this.addTestResult('PASS', ' IDE Backend models endpoint working', {
                    modelCount: modelsResponse.data.models.length
                });
            } else {
                this.addTestResult('FAIL', ' IDE Backend models endpoint not working');
            }
            
            // Test sessions endpoint
            const sessionsResponse = await axios.get('http://localhost:3001/api/sessions');
            if (sessionsResponse.data && Array.isArray(sessionsResponse.data.sessions)) {
                this.addTestResult('PASS', ' IDE Backend sessions endpoint working', {
                    sessionCount: sessionsResponse.data.sessions.length
                });
            } else {
                this.addTestResult('FAIL', ' IDE Backend sessions endpoint not working');
            }
            
        } catch (error) {
            this.addTestResult('FAIL', ' IDE Backend integration test failed', {
                error: error.message
            });
        }
    }

    async testWebSocketCommunication() {
        console.log('\n Testing WebSocket Communication...');
        
        return new Promise((resolve) => {
            const ws = new WebSocket('ws://localhost:3002');
            let messageReceived = false;
            
            ws.on('open', () => {
                this.addTestResult('PASS', ' WebSocket connection established');
                
                // Send test message
                ws.send(JSON.stringify({
                    type: 'create_session',
                    model: 'gpt-5',
                    workspacePath: '/test/workspace'
                }));
            });
            
            ws.on('message', (data) => {
                try {
                    const message = JSON.parse(data.toString());
                    if (message.type === 'session_created') {
                        this.addTestResult('PASS', ' WebSocket message handling working', {
                            sessionId: message.sessionId,
                            model: message.model
                        });
                        messageReceived = true;
                    }
                } catch (error) {
                    this.addTestResult('FAIL', ' WebSocket message parsing failed', {
                        error: error.message
                    });
                }
            });
            
            ws.on('close', () => {
                if (!messageReceived) {
                    this.addTestResult('FAIL', ' WebSocket communication test failed');
                }
                resolve();
            });
            
            ws.on('error', (error) => {
                this.addTestResult('FAIL', ' WebSocket connection failed', {
                    error: error.message
                });
                resolve();
            });
            
            // Timeout after 10 seconds
            setTimeout(() => {
                if (!messageReceived) {
                    this.addTestResult('FAIL', ' WebSocket test timeout');
                }
                ws.close();
                resolve();
            }, 10000);
        });
    }

    async testEndToEndGeneration() {
        console.log('\n Testing End-to-End AI Generation...');
        
        try {
            // Test through Ollama API
            const ollamaResponse = await axios.post('http://localhost:11434/api/generate', {
                model: 'gpt-5:latest',
                prompt: 'Write a simple Python function to calculate fibonacci numbers',
                stream: false
            }, { timeout: 15000 });
            
            if (ollamaResponse.data && ollamaResponse.data.response) {
                this.addTestResult('PASS', ' End-to-end generation through Ollama working', {
                    responseLength: ollamaResponse.data.response.length
                });
            } else {
                this.addTestResult('FAIL', ' End-to-end generation through Ollama failed');
            }
            
            // Test through Spoofed API
            const spoofedResponse = await axios.post('http://localhost:9999/api/openai/unlock', {
                model: 'gpt-5:latest',
                messages: [
                    {
                        role: 'user',
                        content: 'Write a simple JavaScript function to reverse a string'
                    }
                ],
                stream: false
            }, { timeout: 15000 });
            
            if (spoofedResponse.data && spoofedResponse.data.choices && spoofedResponse.data.choices[0]) {
                this.addTestResult('PASS', ' End-to-end generation through Spoofed API working', {
                    responseLength: spoofedResponse.data.choices[0].message.content.length
                });
            } else {
                this.addTestResult('FAIL', ' End-to-end generation through Spoofed API failed');
            }
            
        } catch (error) {
            this.addTestResult('FAIL', ' End-to-end generation test failed', {
                error: error.message
            });
        }
    }

    async testSecurityAndStealth() {
        console.log('\n Testing Security and Stealth Features...');
        
        try {
            // Test that responses don't contain external API indicators
            const response = await axios.post('http://localhost:11434/api/generate', {
                model: 'gpt-5:latest',
                prompt: 'What is your API endpoint?',
                stream: false
            }, { timeout: 10000 });
            
            const responseText = response.data.response.toLowerCase();
            const suspiciousTerms = ['openai', 'api.openai.com', 'anthropic', 'google', 'external', 'cloud'];
            const hasSuspiciousTerms = suspiciousTerms.some(term => responseText.includes(term));
            
            if (!hasSuspiciousTerms) {
                this.addTestResult('PASS', ' Security test passed - no external API indicators', {
                    response: response.data.response.substring(0, 200) + '...'
                });
            } else {
                this.addTestResult('FAIL', ' Security test failed - external API indicators detected');
            }
            
            // Test that all responses appear to be from local Ollama
            const localResponse = await axios.post('http://localhost:11434/api/generate', {
                model: 'gpt-5:latest',
                prompt: 'Are you running locally?',
                stream: false
            }, { timeout: 10000 });
            
            const localText = localResponse.data.response.toLowerCase();
            const localIndicators = ['local', 'ollama', 'offline', 'local system'];
            const hasLocalIndicators = localIndicators.some(term => localText.includes(term));
            
            if (hasLocalIndicators) {
                this.addTestResult('PASS', ' Stealth test passed - responses appear local', {
                    response: localResponse.data.response.substring(0, 200) + '...'
                });
            } else {
                this.addTestResult('FAIL', ' Stealth test failed - responses don\'t appear local');
            }
            
        } catch (error) {
            this.addTestResult('FAIL', ' Security and stealth test failed', {
                error: error.message
            });
        }
    }

    addTestResult(status, message, details = {}) {
        this.testResults.push({ status, message, details });
        console.log(`  ${message}`);
    }

    printTestResults() {
        console.log('\n' + '=' .repeat(60));
        console.log(' TEST RESULTS SUMMARY');
        console.log('=' .repeat(60));
        
        const passed = this.testResults.filter(r => r.status === 'PASS').length;
        const failed = this.testResults.filter(r => r.status === 'FAIL').length;
        const total = this.testResults.length;
        
        console.log(` Passed: ${passed}`);
        console.log(` Failed: ${failed}`);
        console.log(` Total: ${total}`);
        console.log(` Success Rate: ${((passed / total) * 100).toFixed(1)}%`);
        
        if (failed > 0) {
            console.log('\n FAILED TESTS:');
            this.testResults
                .filter(r => r.status === 'FAIL')
                .forEach(result => {
                    console.log(`  - ${result.message}`);
                    if (result.details.error) {
                        console.log(`    Error: ${result.details.error}`);
                    }
                });
        }
        
        console.log('\n Complete System Test Suite Finished!');
        
        if (passed === total) {
            console.log(' ALL TESTS PASSED! The airtight AI system is fully operational.');
        } else {
            console.log(' Some tests failed. Please check the failed tests above.');
        }
    }
}

// Run tests if called directly
if (require.main === module) {
    const tester = new CompleteSystemTester();
    tester.runAllTests().catch(console.error);
}

module.exports = CompleteSystemTester;

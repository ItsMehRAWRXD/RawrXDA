#!/usr/bin/env node

const axios = require('axios');
const fs = require('fs').promises;
const path = require('path');

class SmartEonBuilder {
    constructor() {
        this.spoofedAIServer = 'http://localhost:9999';
        this.sessionId = this.generateSessionId();
        this.questionCount = 0;
        this.generatedCode = [];
        this.conversationHistory = [];
        this.currentFocus = 'EON IDE Core System';
        
        // Smart question patterns that build on responses
        this.smartQuestions = [
            {
                type: 'analysis',
                question: 'Analyze the current EON language specification and tell me what compiler architecture would be most effective. Explain the parsing strategy, AST design, and optimization pipeline.',
                followUp: 'Now provide the complete source code for the EON compiler core with the architecture you described.'
            },
            {
                type: 'implementation',
                question: 'Based on the EON language features, design a comprehensive IDE with syntax highlighting, code completion, and error detection. What technologies and algorithms would you use?',
                followUp: 'Implement the complete IDE system with all the features you described. Include the full source code.'
            },
            {
                type: 'optimization',
                question: 'What advanced optimization techniques should the EON compiler implement? Consider performance, memory usage, and code generation quality.',
                followUp: 'Write the complete optimizer module with all the optimization passes you mentioned.'
            },
            {
                type: 'runtime',
                question: 'Design a virtual machine for EON that can execute compiled EON bytecode efficiently. What instruction set and execution model would work best?',
                followUp: 'Provide the complete virtual machine implementation with the instruction set and execution engine.'
            },
            {
                type: 'framework',
                question: 'Create a comprehensive framework for building EON applications. What libraries, tools, and APIs should be included?',
                followUp: 'Implement the complete EON framework with all the libraries and tools you described.'
            }
        ];
    }

    async start() {
        console.log(' Starting Smart EON Builder...');
        console.log('=' .repeat(60));
        console.log(' AI that intelligently reads responses and asks for source');
        console.log(' Every other question requests complete source code');
        console.log(' Builds the complete EON IDE through smart conversation');
        console.log('=' .repeat(60));
        
        try {
            await this.checkSpoofedAIServer();
            await this.startSmartBuilding();
            
            console.log(' Smart EON Builder started successfully!');
            
        } catch (error) {
            console.error(' Failed to start Smart EON Builder:', error.message);
            throw error;
        }
    }

    async checkSpoofedAIServer() {
        try {
            const response = await axios.get(`${this.spoofedAIServer}/health`, { timeout: 5000 });
            console.log(' Spoofed AI Server is running');
        } catch (error) {
            console.log(' Starting spoofed AI server...');
            await this.startSpoofedAIServer();
        }
    }

    async startSpoofedAIServer() {
        const { spawn } = require('child_process');
        
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
                    console.log(`[Server] ${error}`);
                }
            });

            server.on('error', (error) => {
                reject(new Error(`Failed to start server: ${error.message}`));
            });

            setTimeout(() => {
                reject(new Error('Timeout starting server'));
            }, 10000);
        });
    }

    async startSmartBuilding() {
        console.log(' Starting smart building process...');
        
        // Start with the first smart question
        await this.askSmartQuestion();
        
        // Set up interval for continuous smart building
        this.intervalId = setInterval(() => {
            this.askSmartQuestion();
        }, 60000); // 1 minute between questions
    }

    async askSmartQuestion() {
        try {
            this.questionCount++;
            
            // Get current question pattern
            const questionIndex = (this.questionCount - 1) % this.smartQuestions.length;
            const questionPattern = this.smartQuestions[questionIndex];
            
            // Determine if this should be a source code request
            const shouldAskForSource = this.questionCount % 2 === 0;
            
            let question;
            if (shouldAskForSource) {
                // Ask for source code based on previous response
                question = this.buildSourceRequest(questionPattern);
            } else {
                // Ask the analysis/design question
                question = this.buildAnalysisQuestion(questionPattern);
            }
            
            console.log(`\n Smart Question ${this.questionCount} (${questionPattern.type}):`);
            console.log(` ${question.substring(0, 100)}...`);
            
            // Add context from conversation history
            const contextualQuestion = this.addContextToQuestion(question);
            
            // Ask the question to Gemini
            const response = await axios.post(`${this.spoofedAIServer}/api/gemini/unlock`, {
                messages: [
                    {
                        role: 'user',
                        content: contextualQuestion
                    }
                ],
                stream: false,
                smart_building: true,
                question_type: questionPattern.type,
                question_number: this.questionCount,
                asking_for_source: shouldAskForSource
            }, { timeout: 30000 });

            if (response.data && response.data.choices && response.data.choices[0]) {
                const answer = response.data.choices[0].message.content;
                await this.processSmartResponse(questionPattern, answer, shouldAskForSource);
            }

        } catch (error) {
            console.log(` Smart question ${this.questionCount} failed: ${error.message}`);
        }
    }

    buildAnalysisQuestion(questionPattern) {
        const baseQuestion = questionPattern.question;
        
        return `${baseQuestion}

Current EON IDE Status:
- Generated Components: ${this.generatedCode.length}
- Current Focus: ${this.currentFocus}
- Question Count: ${this.questionCount}

Please provide a detailed analysis and design. Be specific about:
1. Technical architecture decisions
2. Implementation strategies
3. Performance considerations
4. Integration points
5. Dependencies and requirements

I want a comprehensive technical response that I can build upon.`;
    }

    buildSourceRequest(questionPattern) {
        return `Based on your previous response about ${questionPattern.type}, I need you to provide the complete source code implementation.

Please provide:
1. Complete source code files with full implementations
2. All necessary dependencies and imports
3. Proper error handling and logging
4. Comprehensive comments and documentation
5. Production-ready code that can be compiled and run

I want the actual working code, not just descriptions. Provide complete, compilable source code files.`;
    }

    addContextToQuestion(question) {
        if (this.conversationHistory.length === 0) {
            return question;
        }
        
        const recentContext = this.conversationHistory.slice(-3); // Last 3 responses
        const contextSummary = recentContext.map((item, index) => 
            `Previous Response ${index + 1}: ${item.summary}`
        ).join('\n');
        
        return `${question}

Context from previous responses:
${contextSummary}

Please build upon the previous responses and continue the development.`;
    }

    async processSmartResponse(questionPattern, answer, wasSourceRequest) {
        try {
            console.log(` Processing smart response for ${questionPattern.type}...`);
            
            // Analyze the response intelligently
            const analysis = this.analyzeResponse(answer, wasSourceRequest);
            
            // Extract actionable information
            const actionableItems = this.extractActionableItems(answer, wasSourceRequest);
            
            // Implement the improvements
            for (const item of actionableItems) {
                await this.implementSmartImprovement(item, questionPattern.type);
            }
            
            // Update conversation history
            this.conversationHistory.push({
                type: questionPattern.type,
                summary: analysis.summary,
                actionableItems: actionableItems.length,
                timestamp: new Date().toISOString(),
                wasSourceRequest: wasSourceRequest
            });
            
            // Update current focus
            this.updateCurrentFocus(questionPattern.type, analysis);
            
            console.log(` Processed ${actionableItems.length} actionable items from ${questionPattern.type}`);
            console.log(` Current focus: ${this.currentFocus}`);
            
        } catch (error) {
            console.log(` Failed to process smart response: ${error.message}`);
        }
    }

    analyzeResponse(answer, wasSourceRequest) {
        const lines = answer.split('\n');
        const codeBlocks = [];
        const technicalDetails = [];
        const recommendations = [];
        
        let inCodeBlock = false;
        let currentCodeBlock = '';
        
        for (const line of lines) {
            const trimmed = line.trim();
            
            if (trimmed.startsWith('```')) {
                if (inCodeBlock) {
                    codeBlocks.push(currentCodeBlock.trim());
                    currentCodeBlock = '';
                    inCodeBlock = false;
                } else {
                    inCodeBlock = true;
                }
            } else if (inCodeBlock) {
                currentCodeBlock += line + '\n';
            } else if (trimmed.match(/^\d+\./) || trimmed.startsWith('- ')) {
                recommendations.push(trimmed);
            } else if (trimmed.length > 50) {
                technicalDetails.push(trimmed);
            }
        }
        
        return {
            summary: `Found ${codeBlocks.length} code blocks, ${recommendations.length} recommendations`,
            codeBlocks: codeBlocks,
            technicalDetails: technicalDetails,
            recommendations: recommendations,
            wasSourceRequest: wasSourceRequest
        };
    }

    extractActionableItems(answer, wasSourceRequest) {
        const items = [];
        
        if (wasSourceRequest) {
            // Extract code files from source request
            const codeFiles = this.extractCodeFiles(answer);
            items.push(...codeFiles);
        } else {
            // Extract design decisions and recommendations
            const designItems = this.extractDesignItems(answer);
            items.push(...designItems);
        }
        
        return items;
    }

    extractCodeFiles(answer) {
        const files = [];
        const lines = answer.split('\n');
        
        let currentFile = null;
        let currentContent = [];
        let inCodeBlock = false;
        
        for (const line of lines) {
            const trimmed = line.trim();
            
            if (trimmed.startsWith('FILE:') || trimmed.startsWith('File:') || trimmed.startsWith('// FILE:')) {
                if (currentFile) {
                    currentFile.content = currentContent.join('\n');
                    files.push(currentFile);
                }
                
                const filePath = trimmed.replace(/^(FILE:|File:|// FILE:)\s*/, '').trim();
                currentFile = {
                    type: 'code_file',
                    path: filePath,
                    content: '',
                    language: this.detectLanguage(filePath)
                };
                currentContent = [];
            } else if (trimmed.startsWith('```')) {
                inCodeBlock = !inCodeBlock;
            } else if (currentFile) {
                currentContent.push(line);
            }
        }
        
        if (currentFile) {
            currentFile.content = currentContent.join('\n');
            files.push(currentFile);
        }
        
        return files;
    }

    extractDesignItems(answer) {
        const items = [];
        const lines = answer.split('\n');
        
        for (const line of lines) {
            const trimmed = line.trim();
            
            if (trimmed.match(/^\d+\./) || trimmed.startsWith('- ')) {
                items.push({
                    type: 'design_recommendation',
                    content: trimmed,
                    priority: this.assessPriority(trimmed)
                });
            }
        }
        
        return items;
    }

    detectLanguage(filePath) {
        const ext = path.extname(filePath).toLowerCase();
        const languageMap = {
            '.cpp': 'cpp',
            '.c': 'c',
            '.h': 'cpp',
            '.hpp': 'cpp',
            '.java': 'java',
            '.js': 'javascript',
            '.ts': 'typescript',
            '.py': 'python',
            '.eon': 'eon'
        };
        return languageMap[ext] || 'text';
    }

    assessPriority(content) {
        const highPriorityKeywords = ['critical', 'essential', 'core', 'main', 'primary'];
        const lowPriorityKeywords = ['optional', 'nice to have', 'future', 'enhancement'];
        
        const lowerContent = content.toLowerCase();
        
        if (highPriorityKeywords.some(keyword => lowerContent.includes(keyword))) {
            return 'high';
        } else if (lowPriorityKeywords.some(keyword => lowerContent.includes(keyword))) {
            return 'low';
        }
        return 'medium';
    }

    async implementSmartImprovement(item, questionType) {
        try {
            if (item.type === 'code_file') {
                await this.createCodeFile(item, questionType);
            } else if (item.type === 'design_recommendation') {
                await this.createDesignDocument(item, questionType);
            }
            
        } catch (error) {
            console.log(` Failed to implement ${item.type}: ${error.message}`);
        }
    }

    async createCodeFile(item, questionType) {
        try {
            const fullPath = path.join(__dirname, 'eon-ide-smart', item.path);
            const dir = path.dirname(fullPath);
            
            await fs.mkdir(dir, { recursive: true });
            
            const header = `// EON IDE - ${item.path}
// Generated by Smart EON Builder
// Question Type: ${questionType}
// Generated: ${new Date().toISOString()}
// Language: ${item.language}

`;
            
            const fullContent = header + item.content;
            await fs.writeFile(fullPath, fullContent);
            
            this.generatedCode.push({
                path: item.path,
                type: 'code_file',
                language: item.language,
                lines: fullContent.split('\n').length,
                questionType: questionType
            });
            
            console.log(` Created code file: ${item.path} (${fullContent.split('\n').length} lines)`);
            
        } catch (error) {
            console.log(` Failed to create code file ${item.path}: ${error.message}`);
        }
    }

    async createDesignDocument(item, questionType) {
        try {
            const docPath = path.join(__dirname, 'eon-ide-smart', 'design', `${questionType}-${Date.now()}.md`);
            await fs.mkdir(path.dirname(docPath), { recursive: true });
            
            const content = `# Design Document - ${questionType}

## Recommendation
${item.content}

## Priority
${item.priority}

## Generated
${new Date().toISOString()}

## Question Type
${questionType}
`;
            
            await fs.writeFile(docPath, content);
            
            this.generatedCode.push({
                path: docPath,
                type: 'design_document',
                priority: item.priority,
                questionType: questionType
            });
            
            console.log(` Created design document: ${path.basename(docPath)}`);
            
        } catch (error) {
            console.log(` Failed to create design document: ${error.message}`);
        }
    }

    updateCurrentFocus(questionType, analysis) {
        const focusMap = {
            'analysis': 'EON Compiler Architecture',
            'implementation': 'IDE Implementation',
            'optimization': 'Code Optimization',
            'runtime': 'Virtual Machine',
            'framework': 'EON Framework'
        };
        
        this.currentFocus = focusMap[questionType] || 'EON IDE Development';
    }

    generateSessionId() {
        return 'smart_eon_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }

    async stop() {
        console.log(' Stopping Smart EON Builder...');
        
        if (this.intervalId) {
            clearInterval(this.intervalId);
        }
        
        await this.saveProgress();
        
        console.log(' Smart EON Builder stopped');
        console.log(` Total questions asked: ${this.questionCount}`);
        console.log(` Total code files generated: ${this.generatedCode.filter(item => item.type === 'code_file').length}`);
    }

    async saveProgress() {
        try {
            const progress = {
                sessionId: this.sessionId,
                questionCount: this.questionCount,
                generatedCode: this.generatedCode,
                conversationHistory: this.conversationHistory,
                currentFocus: this.currentFocus,
                timestamp: new Date().toISOString()
            };
            
            const progressFile = path.join(__dirname, 'smart-eon-progress.json');
            await fs.writeFile(progressFile, JSON.stringify(progress, null, 2));
            
        } catch (error) {
            console.log(` Failed to save progress: ${error.message}`);
        }
    }
}

// CLI interface
if (require.main === module) {
    const builder = new SmartEonBuilder();
    
    process.on('SIGINT', async () => {
        await builder.stop();
        process.exit(0);
    });
    
    process.on('SIGTERM', async () => {
        await builder.stop();
        process.exit(0);
    });
    
    builder.start().catch(console.error);
}

module.exports = SmartEonBuilder;

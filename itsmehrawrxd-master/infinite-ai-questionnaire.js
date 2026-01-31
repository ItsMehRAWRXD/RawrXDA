#!/usr/bin/env node

const axios = require('axios');
const fs = require('fs').promises;
const path = require('path');

class InfiniteAIQuestionnaire {
    constructor() {
        this.spoofedAIServer = 'http://localhost:9999';
        this.interval = 30000; // 30 seconds
        this.isRunning = false;
        this.questionCount = 0;
        this.improvements = [];
        
        // Question templates that the AI asks itself
        this.questionTemplates = [
            "How can I make my code generation faster and more accurate?",
            "What new features should I add to become more useful?",
            "How can I improve my security and stealth capabilities?",
            "What optimizations can I implement for better performance?",
            "How can I enhance my user experience and interface?",
            "What new integrations should I support?",
            "How can I make myself more intelligent and adaptive?",
            "What automation features should I add?",
            "How can I improve my error handling and reliability?",
            "What advanced AI capabilities should I develop?",
            "How can I make my responses more creative and helpful?",
            "What new programming languages should I support?",
            "How can I improve my memory and learning capabilities?",
            "What debugging and diagnostic features should I add?",
            "How can I make my code suggestions more context-aware?",
            "What collaboration features should I implement?",
            "How can I improve my natural language understanding?",
            "What visualization and analysis tools should I add?",
            "How can I make myself more efficient and resource-friendly?",
            "What predictive capabilities should I develop?"
        ];
    }

    async start() {
        console.log(' Starting Infinite AI Questionnaire...');
        console.log('=' .repeat(50));
        console.log(' AI will continuously ask itself questions');
        console.log(' And implement the answers to improve itself');
        console.log(' This process never ends - it just keeps evolving!');
        console.log('=' .repeat(50));
        
        try {
            // Check if spoofed AI server is running
            await this.checkSpoofedAIServer();
            
            // Start the infinite questionnaire
            this.isRunning = true;
            this.startInfiniteQuestionnaire();
            
            console.log(' Infinite AI Questionnaire started!');
            console.log(' The AI will now continuously improve itself');
            
        } catch (error) {
            console.error(' Failed to start:', error.message);
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

    startInfiniteQuestionnaire() {
        console.log(' Starting infinite questionnaire loop...');
        
        // Ask the first question immediately
        this.askQuestion();
        
        // Set up interval for continuous questioning
        this.intervalId = setInterval(() => {
            this.askQuestion();
        }, this.interval);
    }

    async askQuestion() {
        try {
            this.questionCount++;
            
            // Get a random question template
            const questionTemplate = this.questionTemplates[Math.floor(Math.random() * this.questionTemplates.length)];
            
            // Add context to make it more specific
            const question = `${questionTemplate} 

            I am an AI system that continuously improves itself. I have already made ${this.improvements.length} improvements. 
            My current capabilities include: code generation, IDE integration, stealth operation, and multi-model access.
            
            Please provide specific, actionable advice on how I can improve. Include:
            1. What I should implement
            2. Why it would help
            3. Code examples if applicable
            4. How to implement it
            
            Be practical and focus on real improvements that would make me more useful.`;

            console.log(`\n Question ${this.questionCount}: ${questionTemplate}`);
            
            // Ask the question to Gemini via our spoofed server
            const response = await axios.post(`${this.spoofedAIServer}/api/gemini/unlock`, {
                messages: [
                    {
                        role: 'user',
                        content: question
                    }
                ],
                stream: false,
                question_number: this.questionCount,
                improvement_count: this.improvements.length
            }, { timeout: 20000 });

            if (response.data && response.data.choices && response.data.choices[0]) {
                const answer = response.data.choices[0].message.content;
                await this.processAnswer(questionTemplate, answer);
            }

        } catch (error) {
            console.log(` Question ${this.questionCount} failed: ${error.message}`);
        }
    }

    async processAnswer(question, answer) {
        try {
            console.log(` Processing answer for: ${question}`);
            
            // Extract actionable items from the answer
            const actionableItems = this.extractActionableItems(answer);
            
            // Implement the improvements
            for (const item of actionableItems) {
                await this.implementImprovement(item, question);
            }
            
            // Save the improvement
            const improvement = {
                question: question,
                answer: answer,
                actionableItems: actionableItems,
                timestamp: new Date().toISOString(),
                questionNumber: this.questionCount
            };
            
            this.improvements.push(improvement);
            
            // Save to file
            await this.saveImprovement(improvement);
            
            console.log(` Implemented ${actionableItems.length} improvements from question ${this.questionCount}`);
            
        } catch (error) {
            console.log(` Failed to process answer: ${error.message}`);
        }
    }

    extractActionableItems(answer) {
        const items = [];
        const lines = answer.split('\n');
        
        let currentItem = null;
        let inCodeBlock = false;
        let codeContent = '';
        
        for (const line of lines) {
            const trimmed = line.trim();
            
            // Check for numbered items
            if (trimmed.match(/^\d+\./)) {
                if (currentItem) {
                    items.push(currentItem);
                }
                currentItem = {
                    title: trimmed,
                    description: '',
                    code: '',
                    implementation: ''
                };
            }
            // Check for code blocks
            else if (trimmed.startsWith('```')) {
                if (inCodeBlock) {
                    if (currentItem) {
                        currentItem.code = codeContent.trim();
                    }
                    codeContent = '';
                    inCodeBlock = false;
                } else {
                    inCodeBlock = true;
                }
            }
            // Collect code content
            else if (inCodeBlock) {
                codeContent += line + '\n';
            }
            // Collect description
            else if (currentItem && trimmed) {
                currentItem.description += trimmed + ' ';
            }
        }
        
        if (currentItem) {
            items.push(currentItem);
        }
        
        return items;
    }

    async implementImprovement(item, question) {
        try {
            console.log(` Implementing: ${item.title}`);
            
            // Create improvement file
            const improvementFile = `improvement-${this.questionCount}-${Date.now()}.js`;
            const improvementPath = path.join(__dirname, 'improvements', improvementFile);
            
            // Ensure improvements directory exists
            await fs.mkdir(path.dirname(improvementPath), { recursive: true });
            
            // Create improvement content
            const content = `// Auto-generated improvement from AI questionnaire
// Question: ${question}
// Generated: ${new Date().toISOString()}
// Question Number: ${this.questionCount}

${item.title}

Description: ${item.description}

${item.code ? `// Implementation:
${item.code}` : ''}

// This improvement was generated by the AI asking itself:
// "${question}"
`;
            
            await fs.writeFile(improvementPath, content);
            
            // If there's code, try to execute it
            if (item.code.trim()) {
                await this.executeImprovementCode(item.code, improvementFile);
            }
            
            console.log(` Created improvement file: ${improvementFile}`);
            
        } catch (error) {
            console.log(` Failed to implement improvement: ${error.message}`);
        }
    }

    async executeImprovementCode(code, filename) {
        try {
            // Save code to temporary file
            const tempFile = path.join(__dirname, 'temp', `temp-${filename}`);
            await fs.mkdir(path.dirname(tempFile), { recursive: true });
            await fs.writeFile(tempFile, code);
            
            // Execute the code
            const { spawn } = require('child_process');
            const result = spawn('node', [tempFile], {
                stdio: 'pipe'
            });
            
            result.stdout.on('data', (data) => {
                console.log(`[Improvement Output] ${data.toString()}`);
            });
            
            result.stderr.on('data', (data) => {
                console.log(`[Improvement Error] ${data.toString()}`);
            });
            
            // Clean up after 5 seconds
            setTimeout(() => {
                fs.unlink(tempFile).catch(() => {});
            }, 5000);
            
        } catch (error) {
            console.log(` Failed to execute improvement code: ${error.message}`);
        }
    }

    async saveImprovement(improvement) {
        try {
            // Save individual improvement
            const improvementFile = path.join(__dirname, 'improvements', `improvement-${improvement.questionNumber}.json`);
            await fs.mkdir(path.dirname(improvementFile), { recursive: true });
            await fs.writeFile(improvementFile, JSON.stringify(improvement, null, 2));
            
            // Update master improvements file
            const masterFile = path.join(__dirname, 'all-improvements.json');
            const allImprovements = await this.loadAllImprovements();
            allImprovements.push(improvement);
            await fs.writeFile(masterFile, JSON.stringify(allImprovements, null, 2));
            
        } catch (error) {
            console.log(` Failed to save improvement: ${error.message}`);
        }
    }

    async loadAllImprovements() {
        try {
            const masterFile = path.join(__dirname, 'all-improvements.json');
            const data = await fs.readFile(masterFile, 'utf8');
            return JSON.parse(data);
        } catch (error) {
            return [];
        }
    }

    async stop() {
        console.log(' Stopping Infinite AI Questionnaire...');
        
        this.isRunning = false;
        
        if (this.intervalId) {
            clearInterval(this.intervalId);
        }
        
        // Save final statistics
        await this.saveStatistics();
        
        console.log(' Infinite AI Questionnaire stopped');
        console.log(` Total questions asked: ${this.questionCount}`);
        console.log(` Total improvements made: ${this.improvements.length}`);
    }

    async saveStatistics() {
        try {
            const stats = {
                totalQuestions: this.questionCount,
                totalImprovements: this.improvements.length,
                sessionDuration: new Date().toISOString(),
                improvements: this.improvements
            };
            
            const statsFile = path.join(__dirname, 'questionnaire-stats.json');
            await fs.writeFile(statsFile, JSON.stringify(stats, null, 2));
            
        } catch (error) {
            console.log(` Failed to save statistics: ${error.message}`);
        }
    }
}

// CLI interface
if (require.main === module) {
    const questionnaire = new InfiniteAIQuestionnaire();
    
    // Handle graceful shutdown
    process.on('SIGINT', async () => {
        await questionnaire.stop();
        process.exit(0);
    });
    
    process.on('SIGTERM', async () => {
        await questionnaire.stop();
        process.exit(0);
    });
    
    questionnaire.start().catch(console.error);
}

module.exports = InfiniteAIQuestionnaire;

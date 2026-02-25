// agentic_copilot.js - Enhanced Agentic AI System

class AgenticCopilot {
    constructor(apiKeys = {}) {
        this.apiKeys = apiKeys;
        this.tools = {};
        this.thinkingEnabled = true; // Default state for agent thinking
        this.searchEnabled = true;   // Default state for agent search
        this.planningEnabled = true; // Default state for agent planning
        this.selfReflectionEnabled = true; // Default state for self-reflection
        this.learningEnabled = true; // Default state for learning from feedback
        this.memory = []; // Store conversation history and learnings
        this.currentPlan = null;
        this.executionHistory = [];
    }

    // Register a tool that the agent can use
    registerTool(name, func, description = '') {
        this.tools[name] = {
            func: func,
            description: description,
            usageCount: 0,
            successRate: 1.0
        };
        console.log(`AgenticCopilot: Tool '${name}' registered`);
    }

    // Method to toggle agent thinking
    toggleThinking(enable) {
        this.thinkingEnabled = enable;
        console.log(`Agentic thinking ${this.thinkingEnabled ? 'enabled' : 'disabled'}`);
        this.updateUI('thinking', enable);
    }

    // Method to toggle agent search
    toggleSearch(enable) {
        this.searchEnabled = enable;
        console.log(`Agentic search ${this.searchEnabled ? 'enabled' : 'disabled'}`);
        this.updateUI('search', enable);
    }

    // Method to toggle agent planning
    togglePlanning(enable) {
        this.planningEnabled = enable;
        console.log(`Agentic planning ${this.planningEnabled ? 'enabled' : 'disabled'}`);
        this.updateUI('planning', enable);
    }

    // Method to toggle self-reflection
    toggleSelfReflection(enable) {
        this.selfReflectionEnabled = enable;
        console.log(`Agentic self-reflection ${this.selfReflectionEnabled ? 'enabled' : 'disabled'}`);
        this.updateUI('selfReflection', enable);
    }

    // Method to toggle learning
    toggleLearning(enable) {
        this.learningEnabled = enable;
        console.log(`Agentic learning ${this.learningEnabled ? 'enabled' : 'disabled'}`);
        this.updateUI('learning', enable);
    }

    // Update UI elements based on toggle states
    updateUI(feature, enabled) {
        const indicator = document.getElementById(`agentic-${feature}-indicator`);
        if (indicator) {
            indicator.textContent = enabled ? '🟢' : '🔴';
            indicator.title = `${feature} ${enabled ? 'enabled' : 'disabled'}`;
        }
    }

    // Enhanced agentic thinking process
    async agenticThinking(query, context) {
        if (!this.thinkingEnabled) return null;

        console.log('AgenticCopilot: 🤔 Agent is thinking...');
        
        // Simulate complex thinking process
        const thinkingSteps = [
            'Analyzing query intent...',
            'Evaluating available tools...',
            'Considering context relevance...',
            'Formulating execution plan...',
            'Assessing potential outcomes...'
        ];

        for (const step of thinkingSteps) {
            console.log(`AgenticCopilot: ${step}`);
            await new Promise(resolve => setTimeout(resolve, 200));
        }

        return {
            intent: this.analyzeIntent(query),
            toolRecommendations: this.recommendTools(query),
            confidence: this.calculateConfidence(query, context),
            reasoning: 'Based on query analysis and available tools'
        };
    }

    // Analyze the intent of the query
    analyzeIntent(query) {
        const lowerQuery = query.toLowerCase();
        
        if (lowerQuery.includes('generate') || lowerQuery.includes('create') || lowerQuery.includes('write')) {
            return 'code_generation';
        } else if (lowerQuery.includes('debug') || lowerQuery.includes('fix') || lowerQuery.includes('error')) {
            return 'debugging';
        } else if (lowerQuery.includes('explain') || lowerQuery.includes('how') || lowerQuery.includes('what')) {
            return 'explanation';
        } else if (lowerQuery.includes('optimize') || lowerQuery.includes('improve') || lowerQuery.includes('performance')) {
            return 'optimization';
        } else if (lowerQuery.includes('test') || lowerQuery.includes('testing')) {
            return 'testing';
        } else if (lowerQuery.includes('search') || lowerQuery.includes('find')) {
            return 'search';
        } else {
            return 'general';
        }
    }

    // Recommend tools based on query intent
    recommendTools(query) {
        const intent = this.analyzeIntent(query);
        const recommendations = [];

        switch (intent) {
            case 'code_generation':
                recommendations.push('generateCode', 'createFunction', 'buildAPI');
                break;
            case 'debugging':
                recommendations.push('analyzeError', 'debugCode', 'fixIssues');
                break;
            case 'explanation':
                recommendations.push('explainCode', 'documentCode', 'createTutorial');
                break;
            case 'optimization':
                recommendations.push('optimizeCode', 'analyzePerformance', 'suggestImprovements');
                break;
            case 'testing':
                recommendations.push('generateTests', 'createTestSuite', 'validateCode');
                break;
            case 'search':
                if (this.searchEnabled) {
                    recommendations.push('webSearch', 'codebaseSearch', 'findReferences');
                }
                break;
            default:
                recommendations.push('generalResponse', 'simulateResponse');
        }

        return recommendations.filter(tool => this.tools[tool]);
    }

    // Calculate confidence based on query and context
    calculateConfidence(query, context) {
        let confidence = 0.5; // Base confidence

        // Increase confidence based on context availability
        if (context && context.length > 50) confidence += 0.2;
        
        // Increase confidence based on query specificity
        if (query.length > 20) confidence += 0.1;
        
        // Increase confidence if we have relevant tools
        const intent = this.analyzeIntent(query);
        const availableTools = this.recommendTools(query);
        if (availableTools.length > 0) confidence += 0.2;

        return Math.min(confidence, 0.95);
    }

    // Create execution plan
    async createPlan(query, context, thinkingResult) {
        if (!this.planningEnabled) return null;

        console.log('AgenticCopilot: 📋 Creating execution plan...');
        
        const plan = {
            id: Date.now(),
            query: query,
            context: context,
            intent: thinkingResult?.intent || 'general',
            steps: [],
            estimatedTime: 0,
            confidence: thinkingResult?.confidence || 0.5
        };

        // Plan steps based on intent and available tools
        const toolRecommendations = thinkingResult?.toolRecommendations || this.recommendTools(query);
        
        for (const toolName of toolRecommendations) {
            if (this.tools[toolName]) {
                plan.steps.push({
                    tool: toolName,
                    description: this.tools[toolName].description,
                    estimatedTime: 1000 // 1 second default
                });
                plan.estimatedTime += 1000;
            }
        }

        // Add fallback step if no tools available
        if (plan.steps.length === 0) {
            plan.steps.push({
                tool: 'simulateResponse',
                description: 'Generate simulated response',
                estimatedTime: 500
            });
            plan.estimatedTime += 500;
        }

        this.currentPlan = plan;
        console.log('AgenticCopilot: Plan created:', plan);
        return plan;
    }

    // Execute the plan
    async executePlan(plan) {
        console.log('AgenticCopilot: 🚀 Executing plan...');
        
        const results = [];
        
        for (const step of plan.steps) {
            try {
                console.log(`AgenticCopilot: Executing step: ${step.tool}`);
                
                if (this.tools[step.tool]) {
                    const tool = this.tools[step.tool];
                    const result = await tool.func(plan.query, plan.context);
                    
                    // Update tool usage statistics
                    tool.usageCount++;
                    
                    results.push({
                        step: step.tool,
                        result: result,
                        success: true,
                        timestamp: Date.now()
                    });
                } else {
                    results.push({
                        step: step.tool,
                        result: { content: `Tool ${step.tool} not available`, source: 'error' },
                        success: false,
                        timestamp: Date.now()
                    });
                }
            } catch (error) {
                console.error(`AgenticCopilot: Error executing step ${step.tool}:`, error);
                results.push({
                    step: step.tool,
                    result: { content: `Error: ${error.message}`, source: 'error' },
                    success: false,
                    timestamp: Date.now()
                });
            }
        }

        this.executionHistory.push({
            plan: plan,
            results: results,
            timestamp: Date.now()
        });

        return results;
    }

    // Self-reflection on execution results
    async selfReflect(plan, results) {
        if (!this.selfReflectionEnabled) return null;

        console.log('AgenticCopilot: 🪞 Self-reflecting on execution...');
        
        const reflection = {
            planId: plan.id,
            successRate: results.filter(r => r.success).length / results.length,
            totalTime: Date.now() - plan.id,
            insights: [],
            improvements: []
        };

        // Analyze results
        for (const result of results) {
            if (result.success) {
                reflection.insights.push(`Successfully executed ${result.step}`);
            } else {
                reflection.improvements.push(`Failed to execute ${result.step}: ${result.result.content}`);
            }
        }

        // Update tool success rates
        for (const result of results) {
            if (this.tools[result.step]) {
                const tool = this.tools[result.step];
                const currentSuccesses = tool.successRate * tool.usageCount;
                const newSuccesses = currentSuccesses + (result.success ? 1 : 0);
                tool.successRate = newSuccesses / (tool.usageCount + 1);
            }
        }

        console.log('AgenticCopilot: Self-reflection complete:', reflection);
        return reflection;
    }

    // Learn from feedback and update memory
    async learnFromFeedback(feedback) {
        if (!this.learningEnabled) return;

        console.log('AgenticCopilot: 🧠 Learning from feedback...');
        
        this.memory.push({
            type: 'feedback',
            content: feedback,
            timestamp: Date.now()
        });

        // Keep only last 100 memories
        if (this.memory.length > 100) {
            this.memory = this.memory.slice(-100);
        }
    }

    // The main agentic loop that processes queries
    async processAgenticQuery(query, context) {
        console.log('AgenticCopilot: 🎯 Starting agentic query processing...', { query, context });

        const startTime = Date.now();

        try {
            // Step 1: Agentic Thinking
            const thinkingResult = await this.agenticThinking(query, context);

            // Step 2: Create Execution Plan
            const plan = await this.createPlan(query, context, thinkingResult);

            // Step 3: Execute Plan
            const results = await plan ? await this.executePlan(plan) : [];

            // Step 4: Self-Reflection
            const reflection = plan ? await this.selfReflect(plan, results) : null;

            // Step 5: Synthesize Final Response
            const finalResponse = this.synthesizeResponse(query, context, results, reflection);

            const processingTime = ((Date.now() - startTime) / 1000).toFixed(3);

            console.log('AgenticCopilot: ✅ Query processing complete', {
                processingTime,
                stepsExecuted: results.length,
                successRate: reflection?.successRate || 1.0
            });

            return {
                content: finalResponse,
                source: 'agentic',
                confidence: reflection?.successRate || 0.8,
                processingTime: processingTime,
                metadata: {
                    plan: plan,
                    results: results,
                    reflection: reflection,
                    thinking: thinkingResult
                }
            };

        } catch (error) {
            console.error('AgenticCopilot: ❌ Error in agentic processing:', error);
            
            return {
                content: `Agentic processing failed: ${error.message}. Falling back to basic response.`,
                source: 'agentic_error',
                confidence: 0.1,
                processingTime: ((Date.now() - startTime) / 1000).toFixed(3),
                error: error.message
            };
        }
    }

    // Synthesize final response from execution results
    synthesizeResponse(query, context, results, reflection) {
        let response = `# 🤖 Agentic AI Response\n\n`;
        response += `**Query:** ${query}\n\n`;

        if (reflection) {
            response += `**Execution Summary:**\n`;
            response += `- Steps executed: ${results.length}\n`;
            response += `- Success rate: ${(reflection.successRate * 100).toFixed(1)}%\n`;
            response += `- Processing time: ${reflection.totalTime}ms\n\n`;
        }

        response += `## 📋 Results:\n\n`;

        for (const result of results) {
            response += `### ${result.step} ${result.success ? '✅' : '❌'}\n`;
            if (result.result && result.result.content) {
                response += `${result.result.content}\n\n`;
            }
        }

        if (reflection && reflection.improvements.length > 0) {
            response += `## 🔧 Areas for Improvement:\n`;
            for (const improvement of reflection.improvements) {
                response += `- ${improvement}\n`;
            }
            response += `\n`;
        }

        return response;
    }

    // Get agentic status
    getStatus() {
        return {
            thinkingEnabled: this.thinkingEnabled,
            searchEnabled: this.searchEnabled,
            planningEnabled: this.planningEnabled,
            selfReflectionEnabled: this.selfReflectionEnabled,
            learningEnabled: this.learningEnabled,
            toolsCount: Object.keys(this.tools).length,
            memorySize: this.memory.length,
            executionHistorySize: this.executionHistory.length
        };
    }

    // Reset agentic state
    reset() {
        this.memory = [];
        this.currentPlan = null;
        this.executionHistory = [];
        console.log('AgenticCopilot: 🔄 State reset');
    }
}

// Expose for Node, window, and ES modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = AgenticCopilot;
}
if (typeof window !== 'undefined') {
    window.AgenticCopilot = AgenticCopilot;
}
// ES module default export
export default AgenticCopilot;

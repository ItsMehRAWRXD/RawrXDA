import * as vscode from 'vscode';
import axios, { AxiosInstance } from 'axios';
import { Logger } from '../utils/Logger';
import { WebAuthManager } from './WebAuthManager';

export class AICopilotProvider {
    private logger: Logger;
    private context: vscode.ExtensionContext;
    private webAuthManager: WebAuthManager;
    private apiClient: AxiosInstance;
    private requestTimeout = 30000; // 30 seconds

    constructor(context: vscode.ExtensionContext, webAuthManager: WebAuthManager) {
        this.logger = new Logger('AICopilotProvider');
        this.context = context;
        this.webAuthManager = webAuthManager;
        this.apiClient = axios.create();
    }

    private async getApiConfig() {
        const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
        const token = await this.webAuthManager.getToken();
        
        return {
            apiEndpoint: config.get<string>('apiEndpoint', 'https://api.openai.com/v1'),
            token: token,
            model: config.get<string>('model', 'gpt-4'),
            temperature: config.get<number>('temperature', 0.7),
            maxTokens: config.get<number>('maxTokens', 2048),
            timeout: this.requestTimeout
        };
    }

    async explainCode(code: string, languageId: string): Promise<string> {
        try {
            this.logger.debug(`Explaining code (${languageId}): ${code.substring(0, 50)}...`);
            const startTime = Date.now();

            const apiConfig = await this.getApiConfig();
            if (!apiConfig.token) {
                throw new Error('Not authenticated. Please authenticate first.');
            }

            const response = await this.apiClient.post(
                `${apiConfig.apiEndpoint}/chat/completions`,
                {
                    model: apiConfig.model,
                    messages: [
                        {
                            role: 'system',
                            content: 'You are an expert code analyst. Provide clear, concise explanations of code.'
                        },
                        {
                            role: 'user',
                            content: `Explain the following ${languageId} code:\n\`\`\`\n${code}\n\`\`\``
                        }
                    ],
                    temperature: apiConfig.temperature,
                    max_tokens: apiConfig.maxTokens
                },
                {
                    headers: {
                        'Authorization': `Bearer ${apiConfig.token}`,
                        'Content-Type': 'application/json'
                    },
                    timeout: apiConfig.timeout
                }
            );

            const explanation = response.data.choices[0].message.content;
            const duration = Date.now() - startTime;
            this.logger.info(`Code explanation completed in ${duration}ms`);
            
            return explanation;
        } catch (error) {
            this.logger.error('Error explaining code:', error);
            throw error;
        }
    }

    async refactorCode(code: string, languageId: string): Promise<string> {
        try {
            this.logger.debug(`Refactoring code (${languageId}): ${code.substring(0, 50)}...`);
            const startTime = Date.now();

            const apiConfig = await this.getApiConfig();
            if (!apiConfig.token) {
                throw new Error('Not authenticated. Please authenticate first.');
            }

            const response = await this.apiClient.post(
                `${apiConfig.apiEndpoint}/chat/completions`,
                {
                    model: apiConfig.model,
                    messages: [
                        {
                            role: 'system',
                            content: 'You are an expert code refactorer. Improve code quality, readability, and performance. Return only the refactored code.'
                        },
                        {
                            role: 'user',
                            content: `Refactor the following ${languageId} code:\n\`\`\`\n${code}\n\`\`\``
                        }
                    ],
                    temperature: apiConfig.temperature,
                    max_tokens: apiConfig.maxTokens
                },
                {
                    headers: {
                        'Authorization': `Bearer ${apiConfig.token}`,
                        'Content-Type': 'application/json'
                    },
                    timeout: apiConfig.timeout
                }
            );

            const refactoredCode = response.data.choices[0].message.content;
            const duration = Date.now() - startTime;
            this.logger.info(`Code refactoring completed in ${duration}ms`);
            
            return refactoredCode;
        } catch (error) {
            this.logger.error('Error refactoring code:', error);
            throw error;
        }
    }

    async generateCode(prompt: string, languageId: string): Promise<string> {
        try {
            this.logger.debug(`Generating code (${languageId}): ${prompt}`);
            const startTime = Date.now();

            const apiConfig = await this.getApiConfig();
            if (!apiConfig.token) {
                throw new Error('Not authenticated. Please authenticate first.');
            }

            const response = await this.apiClient.post(
                `${apiConfig.apiEndpoint}/chat/completions`,
                {
                    model: apiConfig.model,
                    messages: [
                        {
                            role: 'system',
                            content: `You are an expert ${languageId} developer. Generate clean, well-structured code. Return only the code without explanations.`
                        },
                        {
                            role: 'user',
                            content: `Generate ${languageId} code for: ${prompt}`
                        }
                    ],
                    temperature: apiConfig.temperature,
                    max_tokens: apiConfig.maxTokens
                },
                {
                    headers: {
                        'Authorization': `Bearer ${apiConfig.token}`,
                        'Content-Type': 'application/json'
                    },
                    timeout: apiConfig.timeout
                }
            );

            const generatedCode = response.data.choices[0].message.content;
            const duration = Date.now() - startTime;
            this.logger.info(`Code generation completed in ${duration}ms`);
            
            return generatedCode;
        } catch (error) {
            this.logger.error('Error generating code:', error);
            throw error;
        }
    }

    async optimizeCode(code: string, languageId: string): Promise<string> {
        try {
            this.logger.debug(`Optimizing code (${languageId}): ${code.substring(0, 50)}...`);
            const startTime = Date.now();

            const apiConfig = await this.getApiConfig();
            if (!apiConfig.token) {
                throw new Error('Not authenticated. Please authenticate first.');
            }

            const response = await this.apiClient.post(
                `${apiConfig.apiEndpoint}/chat/completions`,
                {
                    model: apiConfig.model,
                    messages: [
                        {
                            role: 'system',
                            content: 'You are an expert code optimizer. Improve performance and efficiency. Return only the optimized code.'
                        },
                        {
                            role: 'user',
                            content: `Optimize the following ${languageId} code for performance:\n\`\`\`\n${code}\n\`\`\``
                        }
                    ],
                    temperature: apiConfig.temperature,
                    max_tokens: apiConfig.maxTokens
                },
                {
                    headers: {
                        'Authorization': `Bearer ${apiConfig.token}`,
                        'Content-Type': 'application/json'
                    },
                    timeout: apiConfig.timeout
                }
            );

            const optimizedCode = response.data.choices[0].message.content;
            const duration = Date.now() - startTime;
            this.logger.info(`Code optimization completed in ${duration}ms`);
            
            return optimizedCode;
        } catch (error) {
            this.logger.error('Error optimizing code:', error);
            throw error;
        }
    }

    async chat(messages: any[]): Promise<string> {
        try {
            this.logger.debug(`Chat with ${messages.length} messages`);
            const startTime = Date.now();

            const apiConfig = await this.getApiConfig();
            if (!apiConfig.token) {
                throw new Error('Not authenticated. Please authenticate first.');
            }

            const response = await this.apiClient.post(
                `${apiConfig.apiEndpoint}/chat/completions`,
                {
                    model: apiConfig.model,
                    messages: messages,
                    temperature: apiConfig.temperature,
                    max_tokens: apiConfig.maxTokens
                },
                {
                    headers: {
                        'Authorization': `Bearer ${apiConfig.token}`,
                        'Content-Type': 'application/json'
                    },
                    timeout: apiConfig.timeout
                }
            );

            const reply = response.data.choices[0].message.content;
            const duration = Date.now() - startTime;
            this.logger.info(`Chat response received in ${duration}ms`);
            
            return reply;
        } catch (error) {
            this.logger.error('Error in chat:', error);
            throw error;
        }
    }
}

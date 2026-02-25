import * as vscode from 'vscode';
import axios, { AxiosInstance } from 'axios';
import { OllamaProvider } from './ollamaProvider';

type SimpleProvider = 'proxy' | 'ollama';

export interface AISuiteConfig {
    serverUrl: string;
    authToken?: string;
    timeout: number;
    serviceSlug: string;
    provider: SimpleProvider;
    ollamaBaseUrl: string;
    ollamaModel: string;
}

export class AIService {
    constructor(private readonly context: vscode.ExtensionContext) {}

    private get config(): AISuiteConfig {
        const configuration = vscode.workspace.getConfiguration('aiSuite');
        return {
            serverUrl: configuration.get<string>('serverUrl', 'http://localhost:3003'),
            authToken: configuration.get<string>('authToken') || undefined,
            timeout: configuration.get<number>('timeout', 30000),
            serviceSlug: configuration.get<string>('simple.serviceSlug', 'custom'),
            provider: configuration.get<SimpleProvider>('simple.provider', 'ollama'),
            ollamaBaseUrl: configuration.get<string>('simple.ollamaBaseUrl', 'http://127.0.0.1:11434'),
            ollamaModel: configuration.get<string>('simple.ollamaModel', 'bigdaddyg:latest')
        };
    }

    private get proxyClient(): AxiosInstance {
        const { serverUrl, authToken, timeout } = this.config;
        return axios.create({
            baseURL: serverUrl,
            timeout,
            headers: authToken ? { Authorization: `Bearer ${authToken}` } : undefined
        });
    }

    private get ollama(): OllamaProvider {
        const { ollamaBaseUrl, ollamaModel, timeout } = this.config;
        return new OllamaProvider({
            baseUrl: ollamaBaseUrl,
            model: ollamaModel,
            timeout
        });
    }

    async askQuestion(question: string): Promise<string> {
        return this.executePrompt(`Answer the following question:\n\n${question}`);
    }

    async explainCode(code: string): Promise<string> {
        return this.executePrompt(`Explain the following code:\n\n${code}`);
    }

    async generateTests(code: string): Promise<string> {
        return this.executePrompt(`Write comprehensive unit tests for:\n\n${code}`);
    }

    private async executePrompt(prompt: string): Promise<string> {
        const config = this.config;

        if (config.provider === 'ollama') {
            return this.ollama.chat(prompt);
        }

        const client = this.proxyClient;
        const response = await client.post(`/chat/${config.serviceSlug}`, { message: prompt });
        const data = response.data;

        if (typeof data === 'string') {
            return data;
        }

        if (data?.response) {
            return data.response;
        }

        if (data?.message) {
            return data.message;
        }

        return JSON.stringify(data, null, 2);
    }
}


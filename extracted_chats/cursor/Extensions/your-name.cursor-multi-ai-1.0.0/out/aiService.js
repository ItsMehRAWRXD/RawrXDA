"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AIService = void 0;
const vscode = __importStar(require("vscode"));
const axios_1 = __importDefault(require("axios"));
const ollamaProvider_1 = require("./ollamaProvider");
class AIService {
    constructor(context) {
        this.context = context;
    }
    get config() {
        const configuration = vscode.workspace.getConfiguration('aiSuite');
        return {
            serverUrl: configuration.get('serverUrl', 'http://localhost:3003'),
            authToken: configuration.get('authToken') || undefined,
            timeout: configuration.get('timeout', 30000),
            serviceSlug: configuration.get('simple.serviceSlug', 'custom'),
            provider: configuration.get('simple.provider', 'ollama'),
            ollamaBaseUrl: configuration.get('simple.ollamaBaseUrl', 'http://127.0.0.1:11434'),
            ollamaModel: configuration.get('simple.ollamaModel', 'bigdaddyg:latest')
        };
    }
    get proxyClient() {
        const { serverUrl, authToken, timeout } = this.config;
        return axios_1.default.create({
            baseURL: serverUrl,
            timeout,
            headers: authToken ? { Authorization: `Bearer ${authToken}` } : undefined
        });
    }
    get ollama() {
        const { ollamaBaseUrl, ollamaModel, timeout } = this.config;
        return new ollamaProvider_1.OllamaProvider({
            baseUrl: ollamaBaseUrl,
            model: ollamaModel,
            timeout
        });
    }
    async askQuestion(question) {
        return this.executePrompt(`Answer the following question:\n\n${question}`);
    }
    async explainCode(code) {
        return this.executePrompt(`Explain the following code:\n\n${code}`);
    }
    async generateTests(code) {
        return this.executePrompt(`Write comprehensive unit tests for:\n\n${code}`);
    }
    async executePrompt(prompt) {
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
exports.AIService = AIService;
//# sourceMappingURL=aiService.js.map
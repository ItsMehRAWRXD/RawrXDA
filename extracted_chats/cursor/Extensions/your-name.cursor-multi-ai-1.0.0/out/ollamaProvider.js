"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.OllamaProvider = void 0;
const axios_1 = __importDefault(require("axios"));
class OllamaProvider {
    constructor(options) {
        this.options = options;
    }
    createClient() {
        return axios_1.default.create({
            baseURL: this.options.baseUrl,
            timeout: this.options.timeout
        });
    }
    async chat(prompt) {
        const client = this.createClient();
        const response = await client.post('/api/chat', {
            model: this.options.model,
            messages: [{ role: 'user', content: prompt }]
        });
        const data = response.data;
        if (typeof data === 'string') {
            return data;
        }
        if (data?.message?.content) {
            return data.message.content;
        }
        return JSON.stringify(data, null, 2);
    }
}
exports.OllamaProvider = OllamaProvider;
//# sourceMappingURL=ollamaProvider.js.map
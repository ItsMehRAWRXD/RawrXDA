import axios, { AxiosInstance } from 'axios';

export interface OllamaProviderOptions {
    baseUrl: string;
    model: string;
    timeout: number;
}

export class OllamaProvider {
    constructor(private readonly options: OllamaProviderOptions) {}

    private createClient(): AxiosInstance {
        return axios.create({
            baseURL: this.options.baseUrl,
            timeout: this.options.timeout
        });
    }

    async chat(prompt: string): Promise<string> {
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


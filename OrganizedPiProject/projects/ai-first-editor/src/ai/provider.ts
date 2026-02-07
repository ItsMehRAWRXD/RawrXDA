import * as vscode from 'vscode';
import * as https from 'https';

export interface ChatMessage {
  role: 'user' | 'assistant' | 'system';
  content: string;
}
export interface AIProvider {
  respond(input: { messages: ChatMessage[] }): Promise<string>;
}

class EchoProvider implements AIProvider {
  async respond(input: { messages: ChatMessage[] }): Promise<string> {
    const last = input.messages.filter(m => m.role === 'user').pop();
    return last ? `echo: ${last.content}` : 'echo: (no input)';
  }
}

class OpenAIProvider implements AIProvider {
  constructor(private apiKey: string) {}
  async respond(input: { messages: ChatMessage[] }): Promise<string> {
    const body = JSON.stringify({ model: 'gpt-3.5-turbo', messages: input.messages });
    return new Promise((resolve, reject) => {
      const req = https.request({
        method: 'POST',
        hostname: 'api.openai.com',
        path: '/v1/chat/completions',
        headers: {
          'content-type': 'application/json',
          'authorization': `Bearer ${this.apiKey}`
        }
      }, res => {
        const chunks: Buffer[] = [];
        res.on('data', d => chunks.push(d));
        res.on('end', () => {
          try {
            const data = JSON.parse(Buffer.concat(chunks).toString());
            resolve(data.choices?.[0]?.message?.content || 'No response');
          } catch (e) { reject(e); }
        });
      });
      req.on('error', reject);
      req.write(body);
      req.end();
    });
  }
}

class AnthropicProvider implements AIProvider {
  constructor(private apiKey: string) {}
  async respond(input: { messages: ChatMessage[] }): Promise<string> {
    const body = JSON.stringify({ model: 'claude-3-sonnet-20240229', max_tokens: 1000, messages: input.messages });
    return new Promise((resolve, reject) => {
      const req = https.request({
        method: 'POST',
        hostname: 'api.anthropic.com',
        path: '/v1/messages',
        headers: {
          'content-type': 'application/json',
          'x-api-key': this.apiKey,
          'anthropic-version': '2023-06-01'
        }
      }, res => {
        const chunks: Buffer[] = [];
        res.on('data', d => chunks.push(d));
        res.on('end', () => {
          try {
            const data = JSON.parse(Buffer.concat(chunks).toString());
            resolve(data.content?.[0]?.text || 'No response');
          } catch (e) { reject(e); }
        });
      });
      req.on('error', reject);
      req.write(body);
      req.end();
    });
  }
}

let currentProvider: string = 'echo';
let apiKeys: { [key: string]: string } = {};

export function setProvider(provider: string): void {
  currentProvider = provider;
}

export function setApiKey(provider: string, key: string): void {
  apiKeys[provider] = key;
}

export function getProviders(): string[] {
  return ['echo', 'openai', 'anthropic'];
}

export function createProvider(): AIProvider {
  switch (currentProvider) {
    case 'openai':
      if (!apiKeys.openai) throw new Error('OpenAI API key not set');
      return new OpenAIProvider(apiKeys.openai);
    case 'anthropic':
      if (!apiKeys.anthropic) throw new Error('Anthropic API key not set');
      return new AnthropicProvider(apiKeys.anthropic);
    default:
      return new EchoProvider();
  }
}
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

class HttpProvider implements AIProvider {
  constructor(private endpoint: string, private apiKey?: string) {}
  async respond(input: { messages: ChatMessage[] }): Promise<string> {
    const body = JSON.stringify(input);
    return new Promise((resolve, reject) => {
      const url = new URL(this.endpoint);
      const req = https.request({
        method: 'POST',
        protocol: url.protocol,
        hostname: url.hostname,
        path: url.pathname + url.search,
        port: url.port || (url.protocol === 'https:' ? 443 : 80),
        headers: {
          'content-type': 'application/json',
          'content-length': Buffer.byteLength(body),
          ...(this.apiKey ? { 'authorization': `Bearer ${this.apiKey}` } : {})
        }
      }, res => {
        const chunks: Buffer[] = [];
        res.on('data', d => chunks.push(d));
        res.on('end', () => {
          const text = Buffer.concat(chunks).toString('utf8');
          if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
            try {
              const json = JSON.parse(text);
              resolve(json.result || text);
            } catch {
              resolve(text);
            }
          } else {
            reject(new Error(`HTTP ${res.statusCode}: ${text}`));
          }
        });
      });
      req.on('error', reject);
      req.write(body);
      req.end();
    });
  }
}

export function createProvider(): AIProvider {
  const cfg = vscode.workspace.getConfiguration('aiFoundation');
  const kind = (cfg.get<string>('provider') || 'echo').toLowerCase();
  if (kind === 'http') {
    const endpoint = cfg.get<string>('http.endpoint') || '';
    const apiKeyEnv = cfg.get<string>('http.apiKeyEnv') || '';
    const apiKey = apiKeyEnv ? process.env[apiKeyEnv] : undefined;
    if (!endpoint) {
      vscode.window.showWarningMessage('AI provider is set to "http" but no endpoint configured. Falling back to echo.');
      return new EchoProvider();
    }
    return new HttpProvider(endpoint, apiKey);
  }
  return new EchoProvider();
}
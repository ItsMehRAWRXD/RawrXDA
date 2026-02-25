// BigDaddyGEngine/sim/connector.ts
// Real or Mock Ollama Connector

import { mockGenerate, mockChat } from './modelLogic';

// Global toggle for real vs mock Ollama
(window as any).useRealOllama = false;

export async function ollamaGenerate(model: string, prompt: string): Promise<any> {
  if (!(window as any).useRealOllama) {
    console.log(`🤖 Mock generate: ${model}`);
    return mockGenerate(model, prompt);
  }
  
  console.log(`🌐 Real Ollama generate: ${model}`);
  try {
    const response = await fetch('http://localhost:11434/api/generate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model, prompt })
    });
    
    if (!response.ok) {
      throw new Error(`Ollama request failed: ${response.statusText}`);
    }
    
    return await response.json();
  } catch (error) {
    console.error('Real Ollama request failed, falling back to mock:', error);
    return mockGenerate(model, prompt);
  }
}

export async function ollamaChat(model: string, messages: any[]): Promise<any> {
  if (!(window as any).useRealOllama) {
    console.log(`💬 Mock chat: ${model}`);
    return mockChat(model, messages);
  }
  
  console.log(`🌐 Real Ollama chat: ${model}`);
  try {
    const response = await fetch('http://localhost:11434/api/chat', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model, messages })
    });
    
    if (!response.ok) {
      throw new Error(`Ollama request failed: ${response.statusText}`);
    }
    
    return await response.json();
  } catch (error) {
    console.error('Real Ollama request failed, falling back to mock:', error);
    return mockChat(model, messages);
  }
}

export function toggleOllamaBackend(useReal: boolean): void {
  (window as any).useRealOllama = useReal;
  console.log(`🔄 Switched to ${useReal ? 'real' : 'mock'} Ollama backend`);
}

export function isUsingRealOllama(): boolean {
  return !!(window as any).useRealOllama;
}

export async function checkOllamaStatus(): Promise<boolean> {
  try {
    const response = await fetch('http://localhost:11434/api/version', {
      method: 'GET',
      timeout: 5000
    });
    return response.ok;
  } catch {
    return false;
  }
}

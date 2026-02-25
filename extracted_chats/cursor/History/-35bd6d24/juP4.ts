// BigDaddyGEngine/core/ui.ts
// Full LLM Interface Component

import { createElement } from './renderer';
import { useState } from './state';
import { ollamaGenerate, ollamaChat, toggleOllamaBackend, isUsingRealOllama, checkOllamaStatus } from '../sim/connector';
import { mockModels } from '../sim/modelLogic';
import { formatMetrics, generateFakeStats } from '../sim/metrics';

export function LLMInterface() {
  const [model, setModel] = useState(mockModels[0]);
  const [input, setInput] = useState('');
  const [history, setHistory] = useState<string[]>([]);
  const [stats, setStats] = useState('Ready.');
  const [isLoading, setIsLoading] = useState(false);
  const [ollamaStatus, setOllamaStatus] = useState<'unknown' | 'available' | 'unavailable'>('unknown');

  // Check Ollama status on component mount
  useState(() => {
    checkOllamaStatus().then(available => {
      setOllamaStatus(available ? 'available' : 'unavailable');
    });
  });

  async function sendPrompt() {
    if (!input.trim()) return;
    
    setIsLoading(true);
    setStats('Generating...');
    
    try {
      const result = await ollamaGenerate(model, input);
      const newHistory = [...history, `> ${input}`, result.response];
      setHistory(newHistory);
      setStats(formatMetrics(result.stats || generateFakeStats()));
      setInput('');
    } catch (error) {
      const errorMsg = `Error: ${error.message}`;
      setHistory([...history, `> ${input}`, errorMsg]);
      setStats('Generation failed');
    } finally {
      setIsLoading(false);
    }
  }

  async function sendChat() {
    if (!input.trim()) return;
    
    setIsLoading(true);
    setStats('Chatting...');
    
    try {
      const messages = [
        ...history.filter((_, i) => i % 2 === 0).map((msg, i) => ({
          role: i % 2 === 0 ? 'user' as const : 'assistant' as const,
          content: msg.replace(/^> |^You: |^AI: /, '')
        })),
        { role: 'user' as const, content: input }
      ];
      
      const result = await ollamaChat(model, messages);
      const newHistory = [...history, `You: ${input}`, `AI: ${result.message.content}`];
      setHistory(newHistory);
      setStats(formatMetrics(result.stats || generateFakeStats()));
      setInput('');
    } catch (error) {
      const errorMsg = `Error: ${error.message}`;
      setHistory([...history, `You: ${input}`, errorMsg]);
      setStats('Chat failed');
    } finally {
      setIsLoading(false);
    }
  }

  function toggleBackend() {
    const newMode = !isUsingRealOllama();
    toggleOllamaBackend(newMode);
    setStats(newMode ? 'Switched to Real Ollama' : 'Switched to Mock Mode');
  }

  function clearHistory() {
    setHistory([]);
    setStats('History cleared');
  }

  function exportHistory() {
    const historyText = history.join('\n');
    const blob = new Blob([historyText], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `llm-conversation-${new Date().toISOString().slice(0, 19)}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    setStats('History exported');
  }

  return createElement('div', { class: 'llm-interface' },
    // Header
    createElement('div', { class: 'header' },
      createElement('h2', null, '🧠 Local LLM Interface'),
      createElement('div', { class: 'status-indicators' },
        createElement('span', { 
          class: `status-indicator ${ollamaStatus}`,
          title: `Ollama: ${ollamaStatus}`
        }, `Ollama: ${ollamaStatus}`),
        createElement('span', { 
          class: `status-indicator ${isUsingRealOllama() ? 'real' : 'mock'}`,
          title: `Mode: ${isUsingRealOllama() ? 'Real Ollama' : 'Mock Mode'}`
        }, `Mode: ${isUsingRealOllama() ? 'Real' : 'Mock'}`)
      )
    ),

    // Controls
    createElement('div', { class: 'controls' },
      createElement('div', { class: 'model-selector' },
        createElement('label', null, 'Model:'),
        createElement('select', {
          value: model,
          onChange: (e: any) => setModel(e.target.value),
          disabled: isLoading
        }, ...mockModels.map(m => 
          createElement('option', { value: m }, m)
        ))
      ),
      
      createElement('div', { class: 'mode-toggle' },
        createElement('button', { 
          onClick: toggleBackend,
          class: `toggle-btn ${isUsingRealOllama() ? 'real' : 'mock'}`,
          disabled: isLoading || ollamaStatus === 'unavailable'
        }, isUsingRealOllama() ? '🌐 Real Ollama' : '🎭 Mock Mode'),
        createElement('small', null, 
          ollamaStatus === 'unavailable' ? '(Ollama not available)' : ''
        )
      )
    ),

    // Input area
    createElement('div', { class: 'input-area' },
      createElement('textarea', {
        placeholder: 'Enter prompt or message...',
        value: input,
        onInput: (e: any) => setInput(e.target.value),
        disabled: isLoading,
        rows: 4
      }),
      
      createElement('div', { class: 'input-buttons' },
        createElement('button', { 
          onClick: sendPrompt,
          disabled: isLoading || !input.trim(),
          class: 'action-btn generate-btn'
        }, isLoading ? '⏳ Generating...' : '🚀 Generate'),
        
        createElement('button', { 
          onClick: sendChat,
          disabled: isLoading || !input.trim(),
          class: 'action-btn chat-btn'
        }, isLoading ? '⏳ Chatting...' : '💬 Chat')
      )
    ),

    // Output area
    createElement('div', { class: 'output-area' },
      createElement('div', { class: 'output-header' },
        createElement('h3', null, 'Conversation'),
        createElement('div', { class: 'output-controls' },
          createElement('button', { 
            onClick: clearHistory,
            disabled: history.length === 0,
            class: 'control-btn clear-btn'
          }, '🗑️ Clear'),
          createElement('button', { 
            onClick: exportHistory,
            disabled: history.length === 0,
            class: 'control-btn export-btn'
          }, '📁 Export')
        )
      ),
      
      createElement('div', { class: 'output' },
        history.length === 0 ? 
          createElement('div', { class: 'empty-state' },
            createElement('p', null, 'No conversation yet. Start by typing a prompt above.'),
            createElement('p', null, '💡 Try: "Explain machine learning" or "Write a poem about coding"')
          ) :
          ...history.map((line, index) => 
            createElement('div', { 
              key: index,
              class: `output-line ${line.startsWith('>') ? 'prompt' : line.startsWith('You:') ? 'user' : line.startsWith('AI:') ? 'assistant' : 'response'}`
            }, line)
          )
      )
    ),

    // Status bar
    createElement('div', { class: 'status-bar' },
      createElement('small', null, stats),
      createElement('small', null, `Model: ${model} | Messages: ${history.length}`)
    )
  );
}

export function InsightReportViewer() {
  const [output, setOutput] = useState('Waiting...');
  const [prompt, setPrompt] = useState('Tell me about model drift and ROI analysis.');
  const [isLoading, setIsLoading] = useState(false);
  const [stats, setStats] = useState('Ready.');

  async function handleSend() {
    if (!prompt.trim()) return;
    
    setIsLoading(true);
    setStats('Analyzing...');
    
    try {
      const result = await ollamaGenerate('mock-llama', prompt);
      setOutput(result.response);
      setStats(formatMetrics(result.stats || generateFakeStats()));
    } catch (error) {
      setOutput(`Error: ${error.message}`);
      setStats('Analysis failed');
    } finally {
      setIsLoading(false);
    }
  }

  return createElement('div', { class: 'insight-viewer' },
    createElement('h3', null, '📊 Insight Report Viewer'),
    createElement('div', { class: 'input-section' },
      createElement('textarea', {
        value: prompt,
        onInput: (e: any) => setPrompt(e.target.value),
        disabled: isLoading,
        placeholder: 'Enter your analysis prompt...',
        rows: 3
      }),
      createElement('button', { 
        onClick: handleSend,
        disabled: isLoading || !prompt.trim(),
        class: 'analyze-btn'
      }, isLoading ? '⏳ Analyzing...' : '🔍 Analyze')
    ),
    createElement('div', { class: 'output-section' },
      createElement('pre', { class: 'output' }, output)
    ),
    createElement('div', { class: 'status' },
      createElement('small', null, stats)
    )
  );
}

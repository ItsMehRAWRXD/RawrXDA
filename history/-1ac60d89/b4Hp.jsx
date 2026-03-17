import React, { useState } from 'react';

export default function App() {
  const [prompt, setPrompt] = useState('');
  const [response, setResponse] = useState('');
  const [status, setStatus] = useState('idle');

  const handleSend = async () => {
    if (!prompt.trim()) return;
    setStatus('loading');
    setResponse('');
    try {
      // Placeholder: wire to Ollama HTTP API (e.g., localhost:11434)
      const res = await fetch('/api/ollama', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt }),
      });
      const data = await res.json();
      setResponse(data.output || '');
      setStatus('done');
    } catch (err) {
      setStatus('error');
      setResponse('Request failed. Configure /api/ollama proxy.');
    }
  };

  return (
    <div className="page">
      <header>
        <h1>Ollama React UI</h1>
        <p>Hook this front-end to your local Ollama model server.</p>
      </header>
      <main>
        <label>
          Prompt
          <textarea
            value={prompt}
            onChange={(e) => setPrompt(e.target.value)}
            placeholder="Ask the model..."
          />
        </label>
        <button onClick={handleSend} disabled={status === 'loading'}>
          {status === 'loading' ? 'Sending...' : 'Send'}
        </button>
        <section className="output">
          <h2>Response</h2>
          <pre>{response}</pre>
        </section>
      </main>
    </div>
  );
}

// BigDaddyGEngine/components/BigDaddyGBrowserPanel.tsx - React Browser Panel
import React, { useState, useCallback } from 'react';
import { useBigDaddyGEngine } from '../useEngine';
import { notifyInferenceStart, notifyInferenceComplete } from '../hooks';
import { StreamingRenderer } from './StreamingRenderer';
import { TokenHeatmap } from './TokenHeatmap';
import { useAnalytics, usePerformanceMonitor } from '../hooks';

export function BigDaddyGBrowserPanel() {
  const { execute, stream, streamWithLogits, status, tokens, trace, currentModel, memoryUsage, latency } = useBigDaddyGEngine();
  const [output, setOutput] = useState('');
  const [showHeatmap, setShowHeatmap] = useState(false);
  const [selectedModel, setSelectedModel] = useState('rawr');
  const { logEvent } = useAnalytics();
  const { metrics, startInferenceTimer, endInferenceTimer, updateMemoryUsage } = usePerformanceMonitor();

  const handleRun = useCallback(async () => {
    const prompt = (document.getElementById('promptInput') as HTMLInputElement)?.value || 'Hello, BigDaddyG!';
    const startTime = startInferenceTimer();
    
    notifyInferenceStart();
    logEvent('inference_start', { prompt, model: selectedModel });
    
    try {
      const result = await execute(prompt, `/models/${selectedModel}.gguf`);
      setOutput(result);
      
      endInferenceTimer(startTime, tokens.length);
      updateMemoryUsage(memoryUsage);
      notifyInferenceComplete(result);
      
      logEvent('inference_complete', { 
        outputLength: result.length, 
        tokensGenerated: tokens.length,
        latency: latency 
      });
    } catch (error) {
      console.error('Inference failed:', error);
    }
  }, [execute, selectedModel, startInferenceTimer, endInferenceTimer, updateMemoryUsage, logEvent, tokens.length, memoryUsage, latency]);

  const handleStream = useCallback(async () => {
    const prompt = (document.getElementById('promptInput') as HTMLInputElement)?.value || 'Hello, BigDaddyG!';
    const startTime = startInferenceTimer();
    
    notifyInferenceStart();
    logEvent('inference_start', { prompt, model: selectedModel, mode: 'streaming' });
    
    try {
      setOutput('');
      await stream(prompt, `/models/${selectedModel}.gguf`);
      
      const finalOutput = tokens.join('');
      setOutput(finalOutput);
      
      endInferenceTimer(startTime, tokens.length);
      updateMemoryUsage(memoryUsage);
      notifyInferenceComplete(finalOutput);
      
      logEvent('inference_complete', { 
        outputLength: finalOutput.length, 
        tokensGenerated: tokens.length,
        latency: latency,
        mode: 'streaming'
      });
    } catch (error) {
      console.error('Streaming failed:', error);
    }
  }, [stream, selectedModel, startInferenceTimer, endInferenceTimer, updateMemoryUsage, logEvent, tokens, memoryUsage, latency]);

  const handleStreamWithLogits = useCallback(async () => {
    const prompt = (document.getElementById('promptInput') as HTMLInputElement)?.value || 'Hello, BigDaddyG!';
    const startTime = startInferenceTimer();
    
    notifyInferenceStart();
    logEvent('inference_start', { prompt, model: selectedModel, mode: 'streaming_with_logits' });
    
    try {
      setOutput('');
      await streamWithLogits(prompt, `/models/${selectedModel}.gguf`);
      
      const finalOutput = tokens.join('');
      setOutput(finalOutput);
      
      endInferenceTimer(startTime, tokens.length);
      updateMemoryUsage(memoryUsage);
      notifyInferenceComplete(finalOutput);
      
      logEvent('inference_complete', { 
        outputLength: finalOutput.length, 
        tokensGenerated: tokens.length,
        latency: latency,
        mode: 'streaming_with_logits'
      });
    } catch (error) {
      console.error('Streaming with logits failed:', error);
    }
  }, [streamWithLogits, selectedModel, startInferenceTimer, endInferenceTimer, updateMemoryUsage, logEvent, tokens, memoryUsage, latency]);

  const handleModelChange = useCallback((model: string) => {
    setSelectedModel(model);
    logEvent('model_switch', { model });
  }, [logEvent]);

  return (
    <div className="browser-panel">
      <div className="panel-header">
        <h2>BigDaddyG Engine</h2>
        <div className="status-indicators">
          <span className={`status ${status}`}>
            {status === 'idle' ? '🟢 Ready' : 
             status === 'loading' ? '🟡 Loading' : 
             status === 'running' ? '🔵 Running' : 
             '🔴 Error'}
          </span>
          {currentModel && <span className="model">Model: {currentModel}</span>}
        </div>
      </div>

      <div className="controls">
        <div className="model-selector">
          <label htmlFor="modelSelect">Model:</label>
          <select 
            id="modelSelect" 
            value={selectedModel} 
            onChange={(e) => handleModelChange(e.target.value)}
            disabled={status === 'running'}
          >
            <option value="rawr">Rawr (1GB)</option>
            <option value="tinyllama">TinyLlama (512MB)</option>
          </select>
        </div>

        <div className="prompt-input">
          <input 
            type="text" 
            id="promptInput" 
            placeholder="Enter your prompt here..."
            defaultValue="Hello, BigDaddyG! How can you help me today?"
            disabled={status === 'running'}
          />
        </div>

        <div className="action-buttons">
          <button 
            onClick={handleRun} 
            disabled={status !== 'idle'}
            className="btn btn-primary"
          >
            Run Inference
          </button>
          
          <button 
            onClick={handleStream} 
            disabled={status !== 'idle'}
            className="btn btn-secondary"
          >
            Stream Tokens
          </button>
          
          <button 
            onClick={handleStreamWithLogits} 
            disabled={status !== 'idle'}
            className="btn btn-tertiary"
          >
            Stream + Logits
          </button>
        </div>
      </div>

      <div className="output-section">
        <div className="output-header">
          <h3>Output</h3>
          <div className="output-controls">
            <button 
              onClick={() => setShowHeatmap(!showHeatmap)}
              className="btn btn-small"
            >
              {showHeatmap ? 'Hide' : 'Show'} Heatmap
            </button>
            <span className="token-count">
              {tokens.length} tokens
            </span>
          </div>
        </div>

        <div className="output-content">
          {showHeatmap && trace.length > 0 ? (
            <TokenHeatmap trace={trace} />
          ) : (
            <StreamingRenderer 
              stream={output || tokens.join('')} 
              isStreaming={status === 'running'}
            />
          )}
        </div>
      </div>

      <div className="metrics">
        <div className="metric">
          <span className="label">Latency:</span>
          <span className="value">{latency.toFixed(2)}ms</span>
        </div>
        <div className="metric">
          <span className="label">Memory:</span>
          <span className="value">{(memoryUsage / 1024 / 1024).toFixed(2)}MB</span>
        </div>
        <div className="metric">
          <span className="label">Tokens/sec:</span>
          <span className="value">{metrics.tokensPerSecond.toFixed(2)}</span>
        </div>
        <div className="metric">
          <span className="label">Inference Time:</span>
          <span className="value">{metrics.inferenceTime.toFixed(2)}ms</span>
        </div>
      </div>

      <style jsx>{`
        .browser-panel {
          display: flex;
          flex-direction: column;
          height: 100%;
          background: linear-gradient(135deg, #0a0a0a 0%, #1a1a1a 100%);
          color: #00ff00;
          font-family: 'Courier New', monospace;
          padding: 20px;
          border: 2px solid #00ff00;
          border-radius: 10px;
        }

        .panel-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 20px;
          padding-bottom: 10px;
          border-bottom: 1px solid #00ff00;
        }

        .panel-header h2 {
          margin: 0;
          font-size: 24px;
          text-shadow: 0 0 10px #00ff00;
        }

        .status-indicators {
          display: flex;
          gap: 15px;
          align-items: center;
        }

        .status {
          padding: 5px 10px;
          border-radius: 5px;
          font-size: 12px;
          font-weight: bold;
        }

        .status.idle { background: rgba(0, 255, 0, 0.2); }
        .status.loading { background: rgba(255, 255, 0, 0.2); }
        .status.running { background: rgba(0, 150, 255, 0.2); }
        .status.error { background: rgba(255, 0, 0, 0.2); }

        .model {
          font-size: 12px;
          color: #00aa00;
        }

        .controls {
          display: flex;
          flex-direction: column;
          gap: 15px;
          margin-bottom: 20px;
        }

        .model-selector, .prompt-input {
          display: flex;
          align-items: center;
          gap: 10px;
        }

        .model-selector label {
          font-weight: bold;
          min-width: 60px;
        }

        select, input {
          flex: 1;
          background: rgba(0, 255, 0, 0.05);
          border: 1px solid #00ff00;
          border-radius: 5px;
          padding: 8px 12px;
          color: #00ff00;
          font-family: inherit;
        }

        select:focus, input:focus {
          outline: none;
          box-shadow: 0 0 10px rgba(0, 255, 0, 0.5);
        }

        .action-buttons {
          display: flex;
          gap: 10px;
        }

        .btn {
          padding: 10px 20px;
          border: 1px solid #00ff00;
          border-radius: 5px;
          background: rgba(0, 255, 0, 0.1);
          color: #00ff00;
          cursor: pointer;
          font-family: inherit;
          font-weight: bold;
          transition: all 0.3s;
        }

        .btn:hover:not(:disabled) {
          background: rgba(0, 255, 0, 0.2);
          box-shadow: 0 0 10px rgba(0, 255, 0, 0.5);
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        .btn-primary { background: rgba(0, 255, 0, 0.2); }
        .btn-secondary { background: rgba(0, 150, 255, 0.2); }
        .btn-tertiary { background: rgba(255, 165, 0, 0.2); }
        .btn-small { padding: 5px 10px; font-size: 12px; }

        .output-section {
          flex: 1;
          display: flex;
          flex-direction: column;
          min-height: 300px;
        }

        .output-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 10px;
        }

        .output-header h3 {
          margin: 0;
          font-size: 18px;
        }

        .output-controls {
          display: flex;
          gap: 10px;
          align-items: center;
        }

        .token-count {
          font-size: 12px;
          color: #00aa00;
        }

        .output-content {
          flex: 1;
          background: rgba(0, 0, 0, 0.5);
          border: 1px solid #00ff00;
          border-radius: 5px;
          padding: 15px;
          overflow-y: auto;
          min-height: 200px;
        }

        .metrics {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
          gap: 10px;
          margin-top: 15px;
          padding-top: 15px;
          border-top: 1px solid #00ff00;
        }

        .metric {
          display: flex;
          flex-direction: column;
          align-items: center;
          padding: 8px;
          background: rgba(0, 255, 0, 0.05);
          border-radius: 5px;
        }

        .metric .label {
          font-size: 10px;
          color: #00aa00;
          margin-bottom: 2px;
        }

        .metric .value {
          font-size: 14px;
          font-weight: bold;
          color: #00ff00;
        }
      `}</style>
    </div>
  );
}

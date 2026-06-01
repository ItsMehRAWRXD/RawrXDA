/**
 * InferencePanel.tsx
 * Day 3-4 implementation: token streaming UI bound to engine READY state.
 */

import React, { useEffect, useMemo, useRef, useState } from 'react';
import { EngineStatus, engineService } from '../engine/EngineService';
import { InferenceMetrics, InferenceMonitor } from './InferenceMonitor';
import { safetyInterceptor } from '../safety/SafetyInterceptor';
import './InferencePanel.css';

const DEFAULT_PROMPT = 'Implement a robust parser for a small JSON subset with clear error handling.';

export const InferencePanel: React.FC = () => {
  const [status, setStatus] = useState<EngineStatus | null>(engineService.getStatus());
  const [prompt, setPrompt] = useState(DEFAULT_PROMPT);
  const [output, setOutput] = useState('');
  const [isStreaming, setIsStreaming] = useState(false);
  const [metrics, setMetrics] = useState<InferenceMetrics>({ tokenCount: 0, tokensPerSec: 0, elapsedMs: 0 });
  const [error, setError] = useState('');
  const tokenBufferRef = useRef('');
  const flushIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  const flushBufferedTokens = () => {
    if (!tokenBufferRef.current) {
      return;
    }
    const chunk = tokenBufferRef.current;
    tokenBufferRef.current = '';
    setOutput((prev) => prev + chunk);
  };

  useEffect(() => {
    const unsubscribe = engineService.onStatusChange((next) => {
      setStatus(next);
    });

    return () => {
      unsubscribe();
      if (flushIntervalRef.current) {
        clearInterval(flushIntervalRef.current);
        flushIntervalRef.current = null;
      }
      flushBufferedTokens();
      engineService.stopInferenceStream();
    };
  }, []);

  useEffect(() => {
    if (isStreaming) {
      if (flushIntervalRef.current) {
        clearInterval(flushIntervalRef.current);
      }
      // Keep UI smooth under high token rate by flushing at a fixed cadence.
      flushIntervalRef.current = setInterval(() => {
        flushBufferedTokens();
      }, 40);
      return;
    }

    if (flushIntervalRef.current) {
      clearInterval(flushIntervalRef.current);
      flushIntervalRef.current = null;
    }
    flushBufferedTokens();
  }, [isStreaming]);

  const state = status?.loader_context.state;
  const isReady = state === 2;

  const gateReason = useMemo(() => {
    if (isStreaming) {
      return 'Streaming in progress';
    }

    if (!status) {
      return 'Waiting for engine status';
    }

    if (status.loader_context.state !== 2) {
      return `Blocked by engine state ${status.loader_context.state}: ${status.loader_context.suggested_action}`;
    }

    return 'READY';
  }, [isStreaming, status]);

  const handleSend = async () => {
    if (!isReady || isStreaming) {
      return;
    }

    setIsStreaming(true);
    setOutput('');
    tokenBufferRef.current = '';
    setMetrics({ tokenCount: 0, tokensPerSec: 0, elapsedMs: 0 });
    setError('');
    // Reset interceptor look-behind so previous session's tail doesn't leak.
    safetyInterceptor.reset();

    await engineService.startInferenceStream(
      prompt,
      (token, nextCount) => {
        const result = safetyInterceptor.process(token);
        tokenBufferRef.current += result.token;
        setMetrics((prev) => ({ ...prev, tokenCount: nextCount }));

        // BLOCK mode: first violation halts the stream.
        if (result.violation && safetyInterceptor.getMode() === 'BLOCK') {
          engineService.stopInferenceStream();
        }
      },
      (summary) => {
        setMetrics({
          tokenCount: summary.tokenCount,
          elapsedMs: summary.elapsedMs,
          tokensPerSec: summary.tokensPerSec,
        });
        flushBufferedTokens();
        setIsStreaming(false);
      },
      (message) => {
        flushBufferedTokens();
        setError(message);
        setIsStreaming(false);
      }
    );
  };

  const handleStop = () => {
    engineService.stopInferenceStream();
    flushBufferedTokens();
    setIsStreaming(false);
  };

  const handleRetry = async () => {
    if (!isReady || isStreaming || !prompt.trim()) {
      return;
    }
    await handleSend();
  };

  return (
    <section className="inference-panel">
      <header className="inference-header">
        <h2>Day 3-4: Streaming Inference</h2>
        <span className={`gate-badge ${isReady ? 'ready' : 'blocked'}`}>{gateReason}</span>
      </header>

      <div className="prompt-block">
        <label htmlFor="prompt-input">Prompt</label>
        <textarea
          id="prompt-input"
          value={prompt}
          onChange={(e) => setPrompt(e.target.value)}
          rows={5}
          disabled={isStreaming}
          placeholder="Describe the coding task for the model..."
        />
      </div>

      <div className="inference-controls">
        <button type="button" onClick={handleSend} disabled={!isReady || isStreaming || !prompt.trim()}>
          Send
        </button>
        <button type="button" className="retry" onClick={handleRetry} disabled={!isReady || isStreaming || !prompt.trim()}>
          Retry
        </button>
        <button type="button" className="stop" onClick={handleStop} disabled={!isStreaming}>
          Stop
        </button>
      </div>

      <InferenceMonitor metrics={metrics} />

      {error && <div className="stream-error">{error}</div>}

      <div className="stream-output" aria-live="polite">
        {output || 'No streamed output yet. Wait for READY and click Send.'}
      </div>
    </section>
  );
};

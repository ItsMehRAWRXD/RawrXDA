import React from 'react';

export interface InferenceMetrics {
  tokenCount: number;
  tokensPerSec: number;
  elapsedMs: number;
}

interface InferenceMonitorProps {
  metrics: InferenceMetrics;
}

export const InferenceMonitor: React.FC<InferenceMonitorProps> = ({ metrics }) => {
  return (
    <div className="inference-monitor" role="status" aria-live="polite">
      <div>Tokens: {metrics.tokenCount || 0}</div>
      <div>TPS: {metrics.tokensPerSec?.toFixed(2) || '0.00'}</div>
      <div>Elapsed: {metrics.elapsedMs || 0} ms</div>
    </div>
  );
};

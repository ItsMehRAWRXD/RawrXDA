// BigDaddyGEngine/components/TokenHeatmap.tsx - Token-Level Introspection Visualization
import React, { useState, useMemo } from 'react';

interface Trace {
  token: string;
  index: number;
  timestamp: number;
  logits?: number[];
  confidence?: number;
  entropy?: number;
  maxLogit?: number;
  minLogit?: number;
  avgLogit?: number;
}

interface TokenHeatmapProps {
  trace: Trace[];
  showLogits?: boolean;
  colorScheme?: 'green' | 'blue' | 'rainbow';
  size?: 'small' | 'medium' | 'large';
}

export function TokenHeatmap({ 
  trace, 
  showLogits = true, 
  colorScheme = 'green',
  size = 'medium'
}: TokenHeatmapProps) {
  const [selectedToken, setSelectedToken] = useState<Trace | null>(null);
  const [hoveredToken, setHoveredToken] = useState<Trace | null>(null);

  const processedTokens = useMemo(() => {
    return trace.map((token, index) => {
      const logits = token.logits || [];
      const maxLogit = Math.max(...logits);
      const minLogit = Math.min(...logits);
      const avgLogit = logits.reduce((sum, logit) => sum + logit, 0) / logits.length;
      
      // Calculate confidence based on logit distribution
      const confidence = logits.length > 0 ? 
        Math.max(0, Math.min(1, (maxLogit - minLogit) / 10)) : 0;
      
      // Calculate entropy (measure of uncertainty)
      const entropy = logits.length > 0 ? 
        -logits.reduce((sum, logit) => {
          const prob = Math.exp(logit) / logits.reduce((acc, log) => acc + Math.exp(log), 0);
          return sum + (prob > 0 ? prob * Math.log(prob) : 0);
        }, 0) : 0;

      return {
        ...token,
        confidence,
        entropy,
        maxLogit,
        minLogit,
        avgLogit,
        logitVariance: logits.length > 1 ? 
          logits.reduce((sum, logit) => sum + Math.pow(logit - avgLogit, 2), 0) / logits.length : 0
      };
    });
  }, [trace]);

  const getTokenColor = (token: typeof processedTokens[0]) => {
    if (!showLogits || !token.logits || token.logits.length === 0) {
      return colorScheme === 'green' ? '#00ff00' : 
             colorScheme === 'blue' ? '#0096ff' : '#ff6600';
    }

    const opacity = Math.min(1, Math.max(0.1, token.confidence));
    
    switch (colorScheme) {
      case 'green':
        return `rgba(0, 255, 0, ${opacity})`;
      case 'blue':
        return `rgba(0, 150, 255, ${opacity})`;
      case 'rainbow':
        const hue = (token.avgLogit * 100) % 360;
        return `hsla(${hue}, 70%, 50%, ${opacity})`;
      default:
        return `rgba(0, 255, 0, ${opacity})`;
    }
  };

  const getTokenSize = (token: typeof processedTokens[0]) => {
    const baseSize = size === 'small' ? 12 : size === 'medium' ? 14 : 16;
    const confidenceMultiplier = 0.5 + (token.confidence * 0.5);
    return Math.max(baseSize * 0.5, baseSize * confidenceMultiplier);
  };

  const formatLogits = (logits: number[]) => {
    if (!logits || logits.length === 0) return 'No logits available';
    
    const topLogits = logits
      .map((logit, idx) => ({ logit, idx }))
      .sort((a, b) => b.logit - a.logit)
      .slice(0, 5);
    
    return topLogits.map(({ logit, idx }) => 
      `Token ${idx}: ${logit.toFixed(3)}`
    ).join(', ');
  };

  return (
    <div className="token-heatmap">
      <div className="heatmap-header">
        <h3>Token Heatmap</h3>
        <div className="heatmap-controls">
          <span className="token-count">{processedTokens.length} tokens</span>
          <div className="legend">
            <span className="legend-item">
              <span className="legend-color" style={{ backgroundColor: getTokenColor(processedTokens[0] || { confidence: 0 }) }}></span>
              Confidence
            </span>
          </div>
        </div>
      </div>

      <div className="heatmap-content">
        <div className="token-grid">
          {processedTokens.map((token, index) => (
            <div
              key={index}
              className={`token-item ${selectedToken === token ? 'selected' : ''} ${hoveredToken === token ? 'hovered' : ''}`}
              style={{
                color: getTokenColor(token),
                fontSize: `${getTokenSize(token)}px`,
                opacity: token.confidence > 0.1 ? 1 : 0.3
              }}
              onClick={() => setSelectedToken(selectedToken === token ? null : token)}
              onMouseEnter={() => setHoveredToken(token)}
              onMouseLeave={() => setHoveredToken(null)}
              title={`Token: "${token.token}" | Confidence: ${(token.confidence * 100).toFixed(1)}% | Entropy: ${token.entropy.toFixed(3)}`}
            >
              {token.token}
            </div>
          ))}
        </div>

        {selectedToken && (
          <div className="token-details">
            <h4>Token Analysis</h4>
            <div className="details-grid">
              <div className="detail-item">
                <span className="label">Token:</span>
                <span className="value">"{selectedToken.token}"</span>
              </div>
              <div className="detail-item">
                <span className="label">Index:</span>
                <span className="value">{selectedToken.index}</span>
              </div>
              <div className="detail-item">
                <span className="label">Timestamp:</span>
                <span className="value">{new Date(selectedToken.timestamp).toLocaleTimeString()}</span>
              </div>
              <div className="detail-item">
                <span className="label">Confidence:</span>
                <span className="value">{((selectedToken.confidence ?? 0) * 100).toFixed(1)}%</span>
              </div>
              <div className="detail-item">
                <span className="label">Entropy:</span>
                <span className="value">{(selectedToken.entropy ?? 0).toFixed(3)}</span>
              </div>
              <div className="detail-item">
                <span className="label">Max Logit:</span>
                <span className="value">{(selectedToken.maxLogit ?? 0).toFixed(3)}</span>
              </div>
              <div className="detail-item">
                <span className="label">Min Logit:</span>
                <span className="value">{(selectedToken.minLogit ?? 0).toFixed(3)}</span>
              </div>
              <div className="detail-item">
                <span className="label">Avg Logit:</span>
                <span className="value">{(selectedToken.avgLogit ?? 0).toFixed(3)}</span>
              </div>
              {showLogits && selectedToken.logits && (
                <div className="detail-item full-width">
                  <span className="label">Top Logits:</span>
                  <span className="value">{formatLogits(selectedToken.logits)}</span>
                </div>
              )}
            </div>
          </div>
        )}

        {hoveredToken && !selectedToken && (
          <div className="token-tooltip">
            <strong>"{hoveredToken.token}"</strong>
            <br />
            Confidence: {((hoveredToken.confidence ?? 0) * 100).toFixed(1)}%
            <br />
            Entropy: {(hoveredToken.entropy ?? 0).toFixed(3)}
          </div>
        )}
      </div>

      <style>{`
        .token-heatmap {
          display: flex;
          flex-direction: column;
          height: 100%;
          background: rgba(0, 0, 0, 0.3);
          border: 1px solid #00ff00;
          border-radius: 5px;
          padding: 15px;
        }

        .heatmap-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 15px;
          padding-bottom: 10px;
          border-bottom: 1px solid #00ff00;
        }

        .heatmap-header h3 {
          margin: 0;
          color: #00ff00;
          font-size: 18px;
        }

        .heatmap-controls {
          display: flex;
          gap: 15px;
          align-items: center;
        }

        .token-count {
          font-size: 12px;
          color: #00aa00;
        }

        .legend {
          display: flex;
          gap: 10px;
        }

        .legend-item {
          display: flex;
          align-items: center;
          gap: 5px;
          font-size: 12px;
          color: #00aa00;
        }

        .legend-color {
          width: 12px;
          height: 12px;
          border-radius: 2px;
          border: 1px solid #00ff00;
        }

        .heatmap-content {
          flex: 1;
          display: flex;
          flex-direction: column;
          min-height: 200px;
        }

        .token-grid {
          display: flex;
          flex-wrap: wrap;
          gap: 2px;
          margin-bottom: 15px;
          max-height: 300px;
          overflow-y: auto;
          padding: 10px;
          background: rgba(0, 0, 0, 0.2);
          border-radius: 5px;
        }

        .token-item {
          padding: 2px 4px;
          border-radius: 3px;
          cursor: pointer;
          transition: all 0.2s;
          border: 1px solid transparent;
          font-family: 'Courier New', monospace;
          line-height: 1.2;
        }

        .token-item:hover {
          border-color: #00ff00;
          transform: scale(1.05);
          z-index: 10;
          position: relative;
        }

        .token-item.selected {
          border-color: #00ff00;
          background: rgba(0, 255, 0, 0.1);
          transform: scale(1.1);
          z-index: 20;
          position: relative;
        }

        .token-item.hovered {
          border-color: #00aa00;
          background: rgba(0, 170, 0, 0.1);
        }

        .token-details {
          background: rgba(0, 0, 0, 0.5);
          border: 1px solid #00ff00;
          border-radius: 5px;
          padding: 15px;
          margin-top: 10px;
        }

        .token-details h4 {
          margin: 0 0 10px 0;
          color: #00ff00;
          font-size: 16px;
        }

        .details-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 8px;
        }

        .detail-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 5px 0;
          border-bottom: 1px solid rgba(0, 255, 0, 0.1);
        }

        .detail-item.full-width {
          grid-column: 1 / -1;
          flex-direction: column;
          align-items: flex-start;
        }

        .detail-item .label {
          font-size: 12px;
          color: #00aa00;
          font-weight: bold;
        }

        .detail-item .value {
          font-size: 12px;
          color: #00ff00;
          font-family: 'Courier New', monospace;
        }

        .token-tooltip {
          position: absolute;
          background: rgba(0, 0, 0, 0.9);
          border: 1px solid #00ff00;
          border-radius: 5px;
          padding: 8px 12px;
          font-size: 12px;
          color: #00ff00;
          z-index: 1000;
          pointer-events: none;
          max-width: 200px;
        }

        /* Scrollbar styling */
        .token-grid::-webkit-scrollbar {
          width: 6px;
        }

        .token-grid::-webkit-scrollbar-track {
          background: rgba(0, 0, 0, 0.3);
          border-radius: 3px;
        }

        .token-grid::-webkit-scrollbar-thumb {
          background: rgba(0, 255, 0, 0.3);
          border-radius: 3px;
        }
      `}</style>
    </div>
  );
}

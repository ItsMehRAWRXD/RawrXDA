// BigDaddyGEngine/BigDaddyGBrowser.tsx - Main Browser Application
import React, { useState } from 'react';
import { BigDaddyGBrowserPanel } from './components/BigDaddyGBrowserPanel';
import { OrchestrationGraph } from './components/OrchestrationGraph';
import { useModelManager } from './useEngine';
import { useAnalytics } from './hooks';

export function BigDaddyGBrowser() {
  const [activeTab, setActiveTab] = useState<'inference' | 'orchestration'>('inference');
  const { availableModels, loadedModels, loadModel, unloadModel } = useModelManager();
  const { getEvents, clearEvents } = useAnalytics();

  const handleTabChange = (tab: 'inference' | 'orchestration') => {
    setActiveTab(tab);
  };

  const handleModelLoad = async (modelName: string) => {
    try {
      await loadModel(modelName);
    } catch (error) {
      console.error('Failed to load model:', error);
    }
  };

  const handleModelUnload = async (modelName: string) => {
    try {
      await unloadModel(modelName);
    } catch (error) {
      console.error('Failed to unload model:', error);
    }
  };

  return (
    <div className="bigdaddyg-browser">
      <header className="browser-header">
        <div className="header-content">
          <h1>BigDaddyG Engine</h1>
          <div className="header-subtitle">
            Browser-Native AI Inference with WebAssembly + WebGPU
          </div>
        </div>
        
        <div className="header-controls">
          <div className="model-status">
            <span className="label">Models:</span>
            <span className="loaded">{loadedModels.length} loaded</span>
            <span className="available">{availableModels.length} available</span>
          </div>
          
          <div className="analytics-controls">
            <button 
              onClick={() => console.log('Events:', getEvents())}
              className="btn btn-small"
            >
              View Analytics
            </button>
            <button 
              onClick={clearEvents}
              className="btn btn-small"
            >
              Clear Events
            </button>
          </div>
        </div>
      </header>

      <nav className="browser-nav">
        <div className="nav-tabs">
          <button 
            className={`nav-tab ${activeTab === 'inference' ? 'active' : ''}`}
            onClick={() => handleTabChange('inference')}
          >
            🧠 Inference Engine
          </button>
          <button 
            className={`nav-tab ${activeTab === 'orchestration' ? 'active' : ''}`}
            onClick={() => handleTabChange('orchestration')}
          >
            🎭 Agent Orchestration
          </button>
        </div>
        
        <div className="nav-actions">
          <div className="model-manager">
            <select 
              onChange={(e) => e.target.value && handleModelLoad(e.target.value)}
              defaultValue=""
              className="model-select"
            >
              <option value="">Load Model...</option>
              {availableModels.map(model => (
                <option key={model} value={model}>
                  {model}
                </option>
              ))}
            </select>
            
            {loadedModels.length > 0 && (
              <select 
                onChange={(e) => e.target.value && handleModelUnload(e.target.value)}
                defaultValue=""
                className="model-select"
              >
                <option value="">Unload Model...</option>
                {loadedModels.map(model => (
                  <option key={model} value={model}>
                    {model}
                  </option>
                ))}
              </select>
            )}
          </div>
        </div>
      </nav>

      <main className="browser-main">
        {activeTab === 'inference' && (
          <div className="inference-tab">
            <BigDaddyGBrowserPanel />
          </div>
        )}
        
        {activeTab === 'orchestration' && (
          <div className="orchestration-tab">
            <OrchestrationGraph />
          </div>
        )}
      </main>

      <footer className="browser-footer">
        <div className="footer-content">
          <div className="footer-section">
            <strong>BigDaddyG Engine v2.0</strong>
            <span>• WebAssembly + WebGPU • 44+ Agents • 6000 Models</span>
          </div>
          
          <div className="footer-section">
            <span>Status: 🟢 Online</span>
            <span>• Memory: {Math.round((performance.memory?.usedJSHeapSize ?? 0) / 1024 / 1024)}MB</span>
            <span>• Uptime: {Math.round(performance.now() / 1000)}s</span>
          </div>
        </div>
      </footer>

      <style>{`
        .bigdaddyg-browser {
          display: flex;
          flex-direction: column;
          height: 100vh;
          background: linear-gradient(135deg, #0a0a0a 0%, #1a1a1a 100%);
          color: #00ff00;
          font-family: 'Courier New', monospace;
          overflow: hidden;
        }

        .browser-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 20px 30px;
          background: rgba(0, 255, 0, 0.05);
          border-bottom: 2px solid #00ff00;
        }

        .header-content h1 {
          margin: 0;
          font-size: 28px;
          text-shadow: 0 0 15px #00ff00;
        }

        .header-subtitle {
          font-size: 14px;
          color: #00aa00;
          margin-top: 5px;
        }

        .header-controls {
          display: flex;
          gap: 20px;
          align-items: center;
        }

        .model-status {
          display: flex;
          gap: 10px;
          font-size: 12px;
        }

        .model-status .label {
          color: #00aa00;
        }

        .model-status .loaded {
          color: #00ff00;
          font-weight: bold;
        }

        .model-status .available {
          color: #0096ff;
        }

        .analytics-controls {
          display: flex;
          gap: 8px;
        }

        .browser-nav {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 10px 30px;
          background: rgba(0, 0, 0, 0.3);
          border-bottom: 1px solid #00ff00;
        }

        .nav-tabs {
          display: flex;
          gap: 5px;
        }

        .nav-tab {
          padding: 10px 20px;
          background: transparent;
          border: 1px solid #00ff00;
          border-radius: 5px 5px 0 0;
          color: #00ff00;
          cursor: pointer;
          font-family: inherit;
          font-weight: bold;
          transition: all 0.3s;
        }

        .nav-tab:hover {
          background: rgba(0, 255, 0, 0.1);
        }

        .nav-tab.active {
          background: rgba(0, 255, 0, 0.2);
          border-bottom-color: transparent;
        }

        .nav-actions {
          display: flex;
          gap: 10px;
        }

        .model-manager {
          display: flex;
          gap: 8px;
        }

        .model-select {
          background: rgba(0, 255, 0, 0.05);
          border: 1px solid #00ff00;
          border-radius: 3px;
          padding: 5px 8px;
          color: #00ff00;
          font-family: inherit;
          font-size: 12px;
        }

        .model-select:focus {
          outline: none;
          box-shadow: 0 0 5px rgba(0, 255, 0, 0.5);
        }

        .browser-main {
          flex: 1;
          display: flex;
          flex-direction: column;
          overflow: hidden;
        }

        .inference-tab, .orchestration-tab {
          flex: 1;
          display: flex;
          flex-direction: column;
          overflow: hidden;
        }

        .browser-footer {
          padding: 10px 30px;
          background: rgba(0, 0, 0, 0.5);
          border-top: 1px solid #00ff00;
        }

        .footer-content {
          display: flex;
          justify-content: space-between;
          align-items: center;
          font-size: 12px;
        }

        .footer-section {
          display: flex;
          gap: 15px;
          align-items: center;
        }

        .footer-section span {
          color: #00aa00;
        }

        .btn {
          padding: 5px 10px;
          background: rgba(0, 255, 0, 0.1);
          border: 1px solid #00ff00;
          border-radius: 3px;
          color: #00ff00;
          cursor: pointer;
          font-family: inherit;
          font-size: 11px;
          transition: all 0.3s;
        }

        .btn:hover {
          background: rgba(0, 255, 0, 0.2);
          box-shadow: 0 0 5px rgba(0, 255, 0, 0.5);
        }

        .btn-small {
          padding: 3px 6px;
          font-size: 10px;
        }

        /* Responsive design */
        @media (max-width: 768px) {
          .browser-header {
            flex-direction: column;
            gap: 15px;
            text-align: center;
          }

          .browser-nav {
            flex-direction: column;
            gap: 10px;
          }

          .nav-tabs {
            width: 100%;
            justify-content: center;
          }

          .nav-actions {
            width: 100%;
            justify-content: center;
          }

          .footer-content {
            flex-direction: column;
            gap: 10px;
            text-align: center;
          }
        }
      `}</style>
    </div>
  );
}

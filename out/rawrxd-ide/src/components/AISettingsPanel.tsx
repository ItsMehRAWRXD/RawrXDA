import React, { useState, useCallback } from 'react';
import { Upload, Settings, Zap, Brain, Search, Layers, AlertCircle } from 'lucide-react';
import { useEngineStore } from '@/lib/engine-bridge';

interface AIConfig {
  mode: string;
  engine: string;
  deepThinking: boolean;
  deepResearch: boolean;
  maxTokens: number;
  modelPath: string;
}

export const AISettingsPanel: React.FC = () => {
  const { addLog } = useEngineStore();
  const [config, setConfig] = useState<AIConfig>({
    mode: 'ask',
    engine: 'default',
    deepThinking: false,
    deepResearch: false,
    maxTokens: 4096,
    modelPath: ''
  });
  const [isDragging, setIsDragging] = useState(false);
  const [uploadStatus, setUploadStatus] = useState('');
  const [configError, setConfigError] = useState('');

  const handleDragOver = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setIsDragging(true);
  }, []);

  const handleDragLeave = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setIsDragging(false);
  }, []);

  const handleDrop = useCallback(async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setIsDragging(false);

    const files = Array.from(e.dataTransfer.files);
    const ggufFile = files.find(f => f.name.endsWith('.gguf'));

    if (ggufFile) {
      setUploadStatus(`Loading ${ggufFile.name}...`);

      // Send to backend to load model
      try {
        const formData = new FormData();
        formData.append('model', ggufFile);

        const response = await fetch('/load_model', {
          method: 'POST',
          body: formData
        });

        if (response.ok) {
          setUploadStatus(`✅ Loaded: ${ggufFile.name}`);
          setConfig(prev => ({ ...prev, modelPath: ggufFile.name }));
        } else {
          setUploadStatus(`❌ Failed to load model`);
        }
      } catch (err) {
        setUploadStatus(`❌ Error: ${err}`);
      }
    } else {
      setUploadStatus('❌ Please drop a .gguf file');
    }
  }, []);

  const updateConfig = async (key: keyof AIConfig, value: any) => {
    setConfig(prev => ({ ...prev, [key]: value }));

    // Send to backend
    try {
      let endpoint = '';
      let body: any = {};

      switch (key) {
        case 'mode':
          endpoint = '/set_mode';
          body = { mode: value };
          break;
        case 'engine':
          endpoint = '/set_engine';
          body = { engine: value };
          break;
        case 'deepThinking':
          endpoint = '/set_deep_thinking';
          body = { enabled: value };
          break;
        case 'deepResearch':
          endpoint = '/set_deep_research';
          body = { enabled: value };
          break;
        case 'maxTokens':
          endpoint = '/set_context';
          body = { max_tokens: value };
          break;
      }

      if (endpoint) {
        const res = await fetch(endpoint, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(body)
        });
        if (!res.ok) {
          const errText = `Config push failed: ${res.status} ${res.statusText}`;
          setConfigError(errText);
          addLog(`[AISettings] ${errText}`);
        } else {
          setConfigError('');
          addLog(`[AISettings] Updated ${key} = ${JSON.stringify(value)}`);
        }
      }
    } catch (err) {
      const errText = `Config push error: ${err}`;
      setConfigError(errText);
      addLog(`[AISettings] ${errText}`);
      console.error('Failed to update config:', err);
    }
  };

  return (
    <div className="p-4 space-y-4">
      <div className="flex items-center gap-2 text-sm font-semibold text-white">
        <Settings className="w-4 h-4" />
        AI Configuration
      </div>

      {/* Drag & Drop Zone */}
      <div
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        className={`
          border-2 border-dashed rounded-lg p-4 text-center transition-colors
          ${isDragging
            ? 'border-purple-500 bg-purple-500/10'
            : 'border-gray-700 bg-gray-900/50'
          }
        `}
      >
        <Upload className="w-8 h-8 mx-auto mb-2 text-gray-400" />
        <p className="text-xs text-gray-400 mb-1">
          Drop .gguf model here
        </p>
        <p className="text-xs text-gray-600">
          or model blobs from OllamaModels/
        </p>
        {uploadStatus && (
          <p className={`text-xs mt-2 ${uploadStatus.startsWith('✅') ? 'text-green-400' : 'text-red-400'}`}>
            {uploadStatus}
          </p>
        )}
        {config.modelPath && (
          <p className="text-xs mt-2 text-blue-400 font-mono">
            {config.modelPath}
          </p>
        )}
      </div>

      {/* AI Mode */}
      <div className="space-y-2">
        <label className="text-xs text-gray-400 flex items-center gap-2">
          <Layers className="w-3 h-3" />
          Mode
        </label>
        <select
          value={config.mode}
          onChange={(e) => updateConfig('mode', e.target.value)}
          className="w-full bg-gray-900 border border-gray-700 rounded px-3 py-2 text-sm text-white focus:outline-none focus:border-purple-500"
        >
          <option value="ask">Ask - Q&A Mode</option>
          <option value="plan">Plan - Architecture Design</option>
          <option value="edit">Edit - Code Modification</option>
          <option value="code">Code - Implementation</option>
          <option value="architect">Architect - System Design</option>
          <option value="test">Test - Generate Tests</option>
          <option value="review">Review - Code Review</option>
          <option value="debug">Debug - Error Analysis</option>
          <option value="refactor">Refactor - Optimization</option>
        </select>
      </div>

      {/* Engine Selection */}
      <div className="space-y-2">
        <label className="text-xs text-gray-400 flex items-center gap-2">
          <Zap className="w-3 h-3" />
          Engine
        </label>
        <select
          value={config.engine}
          onChange={(e) => updateConfig('engine', e.target.value)}
          className="w-full bg-gray-900 border border-gray-700 rounded px-3 py-2 text-sm text-white focus:outline-none focus:border-purple-500"
        >
          <option value="default">Default (GGUF)</option>
          <option value="llama3">Llama 3</option>
          <option value="codellama">CodeLlama</option>
          <option value="mistral">Mistral</option>
          <option value="deepseek">DeepSeek Coder</option>
          <option value="phi">Phi-3</option>
        </select>
      </div>

      {/* Deep Thinking Toggle */}
      <div className="space-y-2">
        <label className="flex items-center justify-between cursor-pointer">
          <span className="text-xs text-gray-400 flex items-center gap-2">
            <Brain className="w-3 h-3" />
            Deep Thinking
          </span>
          <div
            onClick={() => updateConfig('deepThinking', !config.deepThinking)}
            className={`
              relative w-10 h-5 rounded-full transition-colors
              ${config.deepThinking ? 'bg-purple-600' : 'bg-gray-700'}
            `}
          >
            <div
              className={`
                absolute top-0.5 left-0.5 w-4 h-4 bg-white rounded-full transition-transform
                ${config.deepThinking ? 'translate-x-5' : 'translate-x-0'}
              `}
            />
          </div>
        </label>
        <p className="text-xs text-gray-600">
          Chain-of-thought reasoning for complex problems
        </p>
      </div>

      {/* Deep Research Toggle */}
      <div className="space-y-2">
        <label className="flex items-center justify-between cursor-pointer">
          <span className="text-xs text-gray-400 flex items-center gap-2">
            <Search className="w-3 h-3" />
            Deep Research
          </span>
          <div
            onClick={() => updateConfig('deepResearch', !config.deepResearch)}
            className={`
              relative w-10 h-5 rounded-full transition-colors
              ${config.deepResearch ? 'bg-blue-600' : 'bg-gray-700'}
            `}
          >
            <div
              className={`
                absolute top-0.5 left-0.5 w-4 h-4 bg-white rounded-full transition-transform
                ${config.deepResearch ? 'translate-x-5' : 'translate-x-0'}
              `}
            />
          </div>
        </label>
        <p className="text-xs text-gray-600">
          Multi-step research with evidence gathering
        </p>
      </div>

      {/* Max Tokens */}
      <div className="space-y-2">
        <label className="text-xs text-gray-400">
          Max Context Tokens
        </label>
        <input
          type="number"
          value={config.maxTokens}
          onChange={(e) => updateConfig('maxTokens', parseInt(e.target.value))}
          className="w-full bg-gray-900 border border-gray-700 rounded px-3 py-2 text-sm text-white focus:outline-none focus:border-purple-500"
          min="128"
          max="32768"
          step="1024"
        />
        <div className="flex justify-between text-xs text-gray-600">
          <span>128</span>
          <span>Current: {config.maxTokens.toLocaleString()}</span>
          <span>32K</span>
        </div>
      </div>

      {/* Quick Presets */}
      <div className="space-y-2">
        <label className="text-xs text-gray-400">Quick Presets</label>
        <div className="grid grid-cols-2 gap-2">
          <button
            onClick={() => {
              updateConfig('mode', 'code');
              updateConfig('deepThinking', true);
              updateConfig('maxTokens', 8192);
            }}
            className="px-3 py-2 bg-purple-600/20 hover:bg-purple-600/30 border border-purple-600/50 rounded text-xs text-purple-300 transition-colors"
          >
            🚀 Coding
          </button>
          <button
            onClick={() => {
              updateConfig('mode', 'debug');
              updateConfig('deepThinking', true);
              updateConfig('deepResearch', true);
            }}
            className="px-3 py-2 bg-red-600/20 hover:bg-red-600/30 border border-red-600/50 rounded text-xs text-red-300 transition-colors"
          >
            🐛 Debug
          </button>
          <button
            onClick={() => {
              updateConfig('mode', 'architect');
              updateConfig('deepThinking', true);
              updateConfig('maxTokens', 16384);
            }}
            className="px-3 py-2 bg-blue-600/20 hover:bg-blue-600/30 border border-blue-600/50 rounded text-xs text-blue-300 transition-colors"
          >
            🏗️ Design
          </button>
          <button
            onClick={() => {
              updateConfig('mode', 'ask');
              updateConfig('deepThinking', false);
              updateConfig('maxTokens', 4096);
            }}
            className="px-3 py-2 bg-green-600/20 hover:bg-green-600/30 border border-green-600/50 rounded text-xs text-green-300 transition-colors"
          >
            💬 Chat
          </button>
        </div>
      </div>

      {/* Status Display */}
      {configError && (
        <div className="flex items-center gap-2 p-2 bg-red-900/30 border border-red-700 rounded text-xs text-red-300">
          <AlertCircle className="w-3 h-3 flex-shrink-0" />
          {configError}
        </div>
      )}
      <div className="text-xs font-mono bg-black/50 rounded p-2 space-y-1">
        <div className="text-gray-500">// Current Configuration</div>
        <div className="text-purple-400">mode: "{config.mode}"</div>
        <div className="text-blue-400">engine: "{config.engine}"</div>
        <div className="text-green-400">deep_thinking: {config.deepThinking ? 'true' : 'false'}</div>
        <div className="text-yellow-400">deep_research: {config.deepResearch ? 'true' : 'false'}</div>
        <div className="text-pink-400">max_tokens: {config.maxTokens}</div>
      </div>
    </div>
  );
};

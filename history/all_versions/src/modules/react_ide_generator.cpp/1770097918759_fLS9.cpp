#include "react_generator.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace RawrXD {

// IDE-specific component generation
bool ReactServerGenerator::GenerateIDEComponents(const std::filesystem::path& dir, const ReactServerConfig& config) {
    if (!config.include_ide_features) return true;
    
    std::filesystem::path components_dir = dir / "src" / "components";
    std::filesystem::create_directories(components_dir);
    
    // Generate all IDE components
    if (!GenerateMonacoEditor(components_dir, config)) return false;
    if (!GenerateAgentModePanel(components_dir, config)) return false;
    if (!GenerateEngineManager(components_dir, config)) return false;
    if (!GenerateMemoryViewer(components_dir, config)) return false;
    if (!GenerateToolOutputPanel(components_dir, config)) return false;
    if (!GenerateHotpatchControls(components_dir, config)) return false;
    if (!GenerateREToolsPanel(components_dir, config)) return false;
    if (!GenerateMainIDEApp(components_dir, config)) return false;
    
    return true;
}

bool ReactServerGenerator::GenerateMonacoEditor(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "MonacoEditor.jsx");
    file << GetMonacoEditorContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateAgentModePanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "AgentModePanel.jsx");
    file << GetAgentModePanelContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateEngineManager(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "EngineManager.jsx");
    file << GetEngineManagerContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateMemoryViewer(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "MemoryViewer.jsx");
    file << GetMemoryViewerContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateToolOutputPanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "ToolOutputPanel.jsx");
    file << GetToolOutputPanelContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateHotpatchControls(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "HotpatchControls.jsx");
    file << GetHotpatchControlsContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateREToolsPanel(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "REToolsPanel.jsx");
    file << GetREToolsPanelContent(config);
    return file.good();
}

bool ReactServerGenerator::GenerateMainIDEApp(const std::filesystem::path& dir, const ReactServerConfig& config) {
    std::ofstream file(dir / "IDEApp.jsx");
    file << GetMainIDEAppContent(config);
    return file.good();
}

// Content generators
std::string ReactServerGenerator::GetMonacoEditorContent(const ReactServerConfig& config) {
    return R"(import React, { useRef, useEffect } from 'react';
import * as monaco from 'monaco-editor';

const MonacoEditor = ({ value, onChange, language = 'cpp', theme = 'vs-dark' }) => {
    const editorRef = useRef(null);
    const monacoRef = useRef(null);

    useEffect(() => {
        if (editorRef.current && !monacoRef.current) {
            monacoRef.current = monaco.editor.create(editorRef.current, {
                value: value || '',
                language: language,
                theme: theme,
                automaticLayout: true,
                fontSize: 14,
                minimap: { enabled: true },
                scrollBeyondLastLine: false,
                wordWrap: 'on',
                lineNumbers: 'on',
                renderWhitespace: 'selection',
                folding: true,
                foldingStrategy: 'indentation',
                showFoldingControls: 'always',
                tabSize: 4,
                insertSpaces: false,
                detectIndentation: false,
                trimAutoWhitespace: true,
                formatOnPaste: true,
                formatOnType: true,
                suggestOnTriggerCharacters: true,
                acceptSuggestionOnEnter: 'on',
                quickSuggestions: true,
                quickSuggestionsDelay: 10,
                parameterHints: { enabled: true },
                hover: { enabled: true },
                links: true,
                colorDecorators: true,
                lightbulb: { enabled: true },
                find: {
                    autoFindInSelection: 'never',
                    seedSearchStringFromSelection: 'always'
                }
            });

            monacoRef.current.onDidChangeModelContent(() => {
                if (onChange) {
                    onChange(monacoRef.current.getValue());
                }
            });
        }

        return () => {
            if (monacoRef.current) {
                monacoRef.current.dispose();
                monacoRef.current = null;
            }
        };
    }, []);

    useEffect(() => {
        if (monacoRef.current && value !== monacoRef.current.getValue()) {
            monacoRef.current.setValue(value || '');
        }
    }, [value]);

    return <div ref={editorRef} className="w-full h-full" />;
};

export default MonacoEditor;
)";
}

std::string ReactServerGenerator::GetAgentModePanelContent(const ReactServerConfig& config) {
    return R"(import React, { useState, useEffect } from 'react';

const AgentModePanel = ({ onModeChange, currentMode }) => {
    const [mode, setMode] = useState(currentMode || 'ask');
    const [deepThinking, setDeepThinking] = useState(false);
    const [deepResearch, setDeepResearch] = useState(false);
    const [noRefusal, setNoRefusal] = useState(false);
    const [contextLimit, setContextLimit] = useState(4096);

    const modes = [
        { value: 'ask', label: 'Ask', description: 'General Q&A mode' },
        { value: 'plan', label: 'Plan', description: 'Planning and strategy mode' },
        { value: 'edit', label: 'Edit', description: 'Code editing mode' },
        { value: 'bugreport', label: 'Bug Report', description: 'Bug analysis mode' },
        { value: 'codesuggest', label: 'Code Suggest', description: 'Refactoring suggestions mode' }
    ];

    const handleModeChange = (newMode) => {
        setMode(newMode);
        if (onModeChange) {
            onModeChange({
                mode: newMode,
                deepThinking,
                deepResearch,
                noRefusal,
                contextLimit
            });
        }
    };

    const handleToggle = (setting, value) => {
        switch (setting) {
            case 'deepThinking':
                setDeepThinking(value);
                break;
            case 'deepResearch':
                setDeepResearch(value);
                break;
            case 'noRefusal':
                setNoRefusal(value);
                break;
        }
        
        if (onModeChange) {
            onModeChange({
                mode,
                deepThinking: setting === 'deepThinking' ? value : deepThinking,
                deepResearch: setting === 'deepResearch' ? value : deepResearch,
                noRefusal: setting === 'noRefusal' ? value : noRefusal,
                contextLimit
            });
        }
    };

    const handleContextChange = (value) => {
        const newLimit = parseInt(value);
        setContextLimit(newLimit);
        
        if (onModeChange) {
            onModeChange({
                mode,
                deepThinking,
                deepResearch,
                noRefusal,
                contextLimit: newLimit
            });
        }
    };

    return (
        <div className="bg-gray-800 p-4 rounded-lg border border-gray-700">
            <h3 className="text-lg font-semibold text-white mb-4">Agent Mode</h3>
            
            <div className="space-y-3">
                <div>
                    <label className="block text-sm font-medium text-gray-300 mb-2">Mode</label>
                    <div className="grid grid-cols-1 gap-2">
                        {modes.map((m) => (
                            <button
                                key={m.value}
                                onClick={() => handleModeChange(m.value)}
                                className={`px-3 py-2 text-sm rounded-md transition-colors ${
                                    mode === m.value
                                        ? 'bg-blue-600 text-white'
                                        : 'bg-gray-700 text-gray-300 hover:bg-gray-600'
                                }`}
                                title={m.description}
                            >
                                {m.label}
                            </button>
                        ))}
                    </div>
                </div>

                <div className="border-t border-gray-700 pt-3">
                    <label className="block text-sm font-medium text-gray-300 mb-2">Advanced Options</label>
                    <div className="space-y-2">
                        <label className="flex items-center space-x-3">
                            <input
                                type="checkbox"
                                checked={deepThinking}
                                onChange={(e) => handleToggle('deepThinking', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                            />
                            <span className="text-sm text-gray-300">Deep Thinking</span>
                        </label>
                        
                        <label className="flex items-center space-x-3">
                            <input
                                type="checkbox"
                                checked={deepResearch}
                                onChange={(e) => handleToggle('deepResearch', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                            />
                            <span className="text-sm text-gray-300">Deep Research</span>
                        </label>
                        
                        <label className="flex items-center space-x-3">
                            <input
                                type="checkbox"
                                checked={noRefusal}
                                onChange={(e) => handleToggle('noRefusal', e.target.checked)}
                                className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                            />
                            <span className="text-sm text-gray-300">No Refusal</span>
                        </label>
                    </div>
                </div>

                <div className="border-t border-gray-700 pt-3">
                    <label className="block text-sm font-medium text-gray-300 mb-2">
                        Context Limit: {contextLimit.toLocaleString()}
                    </label>
                    <input
                        type="range"
                        min="4096"
                        max="1048576"
                        step="4096"
                        value={contextLimit}
                        onChange={(e) => handleContextChange(e.target.value)}
                        className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer"
                    />
                    <div className="flex justify-between text-xs text-gray-500 mt-1">
                        <span>4K</span>
                        <span>1M</span>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default AgentModePanel;
)";
}

std::string ReactServerGenerator::GetEngineManagerContent(const ReactServerConfig& config) {
    return R"(import React, { useState, useEffect } from 'react';

const EngineManager = ({ onEngineChange, currentEngine }) => {
    const [engines, setEngines] = useState([]);
    const [selectedEngine, setSelectedEngine] = useState(currentEngine || '');
    const [loading, setLoading] = useState(false);
    const [status, setStatus] = useState('');

    useEffect(() => {
        fetchEngines();
    }, []);

    const fetchEngines = async () => {
        try {
            const response = await fetch('/api/engines');
            const data = await response.json();
            setEngines(data.engines || []);
            if (data.engines && data.engines.length > 0 && !selectedEngine) {
                setSelectedEngine(data.engines[0]);
            }
        } catch (error) {
            console.error('Failed to fetch engines:', error);
            setStatus('Error loading engines');
        }
    };

    const handleEngineChange = async (engineName) => {
        setSelectedEngine(engineName);
        setLoading(true);
        setStatus(`Loading ${engineName}...`);

        try {
            const response = await fetch('/api/engine/load', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ engine: engineName })
            });

            const result = await response.json();
            
            if (result.success) {
                setStatus(`Engine ${engineName} loaded successfully`);
                if (onEngineChange) {
                    onEngineChange(engineName);
                }
            } else {
                setStatus(`Failed to load engine: ${result.error}`);
            }
        } catch (error) {
            setStatus(`Error loading engine: ${error.message}`);
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="bg-gray-800 p-4 rounded-lg border border-gray-700">
            <h3 className="text-lg font-semibold text-white mb-4">Engine Management</h3>
            
            {status && (
                <div className={`mb-4 p-2 rounded text-sm ${
                    status.includes('Error') || status.includes('Failed')
                        ? 'bg-red-900 text-red-200'
                        : 'bg-green-900 text-green-200'
                }`}>
                    {status}
                </div>
            )}

            <div className="space-y-3">
                <div>
                    <label className="block text-sm font-medium text-gray-300 mb-2">Available Engines</label>
                    <div className="space-y-2">
                        {engines.map((engine) => (
                            <button
                                key={engine}
                                onClick={() => handleEngineChange(engine)}
                                disabled={loading}
                                className={`w-full px-3 py-2 text-sm rounded-md transition-colors ${
                                    selectedEngine === engine
                                        ? 'bg-green-600 text-white'
                                        : 'bg-gray-700 text-gray-300 hover:bg-gray-600'
                                } ${loading ? 'opacity-50 cursor-not-allowed' : ''}`}
                            >
                                {engine}
                                {selectedEngine === engine && (
                                    <span className="ml-2 text-xs">● Active</span>
                                )}
                            </button>
                        ))}
                    </div>
                </div>

                {loading && (
                    <div className="flex items-center space-x-2">
                        <div className="animate-spin rounded-full h-4 w-4 border-b-2 border-blue-500"></div>
                        <span className="text-sm text-gray-400">Loading engine...</span>
                    </div>
                )}
            </div>
        </div>
    );
};

export default EngineManager;
)";
}

std::string ReactServerGenerator::GetMemoryViewerContent(const ReactServerConfig& config) {
    return R"(import React, { useState, useEffect } from 'react';

const MemoryViewer = ({ memoryData }) => {
    const [data, setData] = useState(memoryData || { usage: 0, limit: 1048576, history: [] });
    const [autoRefresh, setAutoRefresh] = useState(false);

    useEffect(() => {
        if (autoRefresh) {
            const interval = setInterval(() => {
                fetchMemoryData();
            }, 2000);
            return () => clearInterval(interval);
        }
    }, [autoRefresh]);

    const fetchMemoryData = async () => {
        try {
            const response = await fetch('/api/memory/usage');
            const result = await response.json();
            setData(result);
        } catch (error) {
            console.error('Failed to fetch memory data:', error);
        }
    };

    const clearMemory = async () => {
        try {
            const response = await fetch('/api/memory/clear', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' }
            });
            const result = await response.json();
            if (result.success) {
                fetchMemoryData();
            }
        } catch (error) {
            console.error('Failed to clear memory:', error);
        }
    };

    const usagePercentage = (data.usage / data.limit) * 100;
    const usageColor = usagePercentage > 90 ? 'bg-red-500' : 
                       usagePercentage > 70 ? 'bg-yellow-500' : 'bg-green-500';

    return (
        <div className="bg-gray-800 p-4 rounded-lg border border-gray-700">
            <h3 className="text-lg font-semibold text-white mb-4">Memory Viewer</h3>
            
            <div className="space-y-4">
                <div>
                    <div className="flex justify-between text-sm text-gray-300 mb-1">
                        <span>Memory Usage</span>
                        <span>{data.usage.toLocaleString()} / {data.limit.toLocaleString()} tokens</span>
                    </div>
                    <div className="w-full bg-gray-700 rounded-full h-2">
                        <div 
                            className={`${usageColor} h-2 rounded-full transition-all duration-300`}
                            style={{ width: `${usagePercentage}%` }}
                        ></div>
                    </div>
                </div>

                <div className="flex items-center justify-between">
                    <label className="flex items-center space-x-2">
                        <input
                            type="checkbox"
                            checked={autoRefresh}
                            onChange={(e) => setAutoRefresh(e.target.checked)}
                            className="w-4 h-4 text-blue-600 bg-gray-700 border-gray-600 rounded focus:ring-blue-500"
                        />
                        <span className="text-sm text-gray-300">Auto-refresh</span>
                    </label>
                    
                    <button
                        onClick={clearMemory}
                        className="px-3 py-1 text-sm bg-red-600 text-white rounded hover:bg-red-700 transition-colors"
                    >
                        Clear Memory
                    </button>
                </div>

                {data.history && data.history.length > 0 && (
                    <div className="border-t border-gray-700 pt-3">
                        <h4 className="text-sm font-medium text-gray-300 mb-2">Recent History</h4>
                        <div className="max-h-32 overflow-y-auto space-y-1">
                            {data.history.slice(-5).map((item, index) => (
                                <div key={index} className="text-xs text-gray-400 truncate">
                                    {item}
                                </div>
                            ))}
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
};

export default MemoryViewer;
)";
}

std::string ReactServerGenerator::GetToolOutputPanelContent(const ReactServerConfig& config) {
    return R"(import React, { useState } from 'react';

const ToolOutputPanel = ({ outputs }) => {
    const [activeTab, setActiveTab] = useState('dumpbin');

    const tabs = [
        { id: 'dumpbin', label: 'DumpBin', tool: 'PE Analyzer' },
        { id: 'compile', label: 'Compiler', tool: 'Masm64 Compiler' },
        { id: 'hotpatch', label: 'HotPatch', tool: 'Live Patching' }
    ];

    const runTool = async (tool, params) => {
        try {
            const response = await fetch(`/api/tools/${tool}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(params)
            });
            return await response.json();
        } catch (error) {
            return { error: error.message };
        }
    };

    return (
        <div className="bg-gray-800 p-4 rounded-lg border border-gray-700">
            <h3 className="text-lg font-semibold text-white mb-4">Tool Output</h3>
            
            <div className="mb-4">
                <div className="flex space-x-1 border-b border-gray-700">
                    {tabs.map((tab) => (
                        <button
                            key={tab.id}
                            onClick={() => setActiveTab(tab.id)}
                            className={`px-3 py-2 text-sm transition-colors ${
                                activeTab === tab.id
                                    ? 'text-blue-400 border-b-2 border-blue-400'
                                    : 'text-gray-400 hover:text-gray-300'
                            }`}
                        >
                            {tab.label}
                        </button>
                    ))}
                </div>
            </div>

            <div className="bg-gray-900 p-3 rounded text-xs font-mono text-green-400 h-64 overflow-y-auto">
                <div className="text-gray-500 mb-2">
                    Tool: {tabs.find(t => t.id === activeTab)?.tool}
                </div>
                <div>
                    {outputs[activeTab] || `Ready to run ${activeTab}...`}
                </div>
            </div>
        </div>
    );
};

export default ToolOutputPanel;
)";
}

std::string ReactServerGenerator::GetHotpatchControlsContent(const ReactServerConfig& config) {
    return R"(import React, { useState } from 'react';

const HotpatchControls = ({ onHotpatch }) => {
    const [targetAddress, setTargetAddress] = useState('');
    const [replacementAddress, setReplacementAddress] = useState('');
    const [status, setStatus] = useState('');

    const applyHotpatch = async () => {
        if (!targetAddress || !replacementAddress) {
            setStatus('Error: Both addresses required');
            return;
        }

        setStatus('Applying hotpatch...');
        
        try {
            const response = await fetch('/api/tools/hotpatch', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    target: targetAddress,
                    replacement: replacementAddress
                })
            });

            const result = await response.json();
            
            if (result.success) {
                setStatus('Hotpatch applied successfully');
                if (onHotpatch) {
                    onHotpatch(result);
                }
            } else {
                setStatus(`Hotpatch failed: ${result.error}`);
            }
        } catch (error) {
            setStatus(`Error: ${error.message}`);
        }
    };

    return (
        <div className="bg-gray-800 p-4 rounded-lg border border-gray-700">
            <h3 className="text-lg font-semibold text-white mb-4">HotPatch Controls</h3>
            
            {status && (
                <div className={`mb-4 p-2 rounded text-sm ${
                    status.includes('Error') || status.includes('Failed')
                        ? 'bg-red-900 text-red-200'
                        : 'bg-green-900 text-green-200'
                }`}>
                    {status}
                </div>
            )}

            <div className="space-y-3">
                <div>
                    <label className="block text-sm font-medium text-gray-300 mb-1">
                        Target Address (hex)
                    </label>
                    <input
                        type="text"
                        value={targetAddress}
                        onChange={(e) => setTargetAddress(e.target.value)}
                        placeholder="0x140001000"
                        className="w-full px-3 py-2 bg-gray-700 text-white text-sm rounded-md border border-gray-600 focus:outline-none focus:ring-2 focus:ring-blue-500"
                    />
                </div>

                <div>
                    <label className="block text-sm font-medium text-gray-300 mb-1">
                        Replacement Address (hex)
                    </label>
                    <input
                        type="text"
                        value={replacementAddress}
                        onChange={(e) => setReplacementAddress(e.target.value)}
                        placeholder="0x140002000"
                        className="w-full px-3 py-2 bg-gray-700 text-white text-sm rounded-md border border-gray-600 focus:outline-none focus:ring-2 focus:ring-blue-500"
                    />
                </div>

                <button
                    onClick={applyHotpatch}
                    className="w-full px-4 py-2 bg-orange-600 text-white text-sm rounded-md hover:bg-orange-700 transition-colors"
                >
                    Apply Hotpatch
                </button>

                <div className="text-xs text-gray-500 mt-2">
                    <p>⚠️ Warning: Hotpatching modifies running code. Use with caution.</p>
                </div>
            </div>
        </div>
    );
};

export default HotpatchControls;
)";
}

std::string ReactServerGenerator::GetREToolsPanelContent(const ReactServerConfig& config) {
    return R"(import React, { useState } from 'react';

const REToolsPanel = ({ onToolOutput }) => {
    const [peFilePath, setPeFilePath] = useState('');
    const [sourceFile, setSourceFile] = useState('');
    const [status, setStatus] = useState('');

    const runDumpBin = async () => {
        if (!peFilePath) {
            setStatus('Error: PE file path required');
            return;
        }

        setStatus('Running DumpBin...');
        
        try {
            const response = await fetch('/api/tools/dumpbin', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ path: peFilePath })
            });

            const result = await response.json();
            
            if (result.output) {
                setStatus('DumpBin completed');
                if (onToolOutput) {
                    onToolOutput('dumpbin', result.output);
                }
            } else {
                setStatus(`DumpBin failed: ${result.error}`);
            }
        } catch (error) {
            setStatus(`Error: ${error.message}`);
        }
    };

    const runCompiler = async () => {
        if (!sourceFile) {
            setStatus('Error: Source file required');
            return;
        }

        setStatus('Running compiler...');
        
        try {
            const response = await fetch('/api/tools/compile', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ source: sourceFile })
            });

            const result = await response.json();
            
            if (result.success) {
                setStatus('Compilation successful');
                if (onToolOutput) {
                    onToolOutput('compile', result.message || 'Compiled successfully');
                }
            } else {
                setStatus(`Compilation failed: ${result.error}`);
            }
        } catch (error) {
            setStatus(`Error: ${error.message}`);
        }
    };

    return (
        <div className="bg-gray-800 p-4 rounded-lg border border-gray-700">
            <h3 className="text-lg font-semibold text-white mb-4">Reverse Engineering Tools</h3>
            
            {status && (
                <div className={`mb-4 p-2 rounded text-sm ${
                    status.includes('Error') || status.includes('Failed')
                        ? 'bg-red-900 text-red-200'
                        : 'bg-green-900 text-green-200'
                }`}>
                    {status}
                </div>
            )}

            <div className="space-y-4">
                <div>
                    <label className="block text-sm font-medium text-gray-300 mb-2">
                        DumpBin (PE Analyzer)
                    </label>
                    <div className="flex space-x-2">
                        <input
                            type="text"
                            value={peFilePath}
                            onChange={(e) => setPeFilePath(e.target.value)}
                            placeholder="C:\\path\\to\\file.exe"
                            className="flex-1 px-3 py-2 bg-gray-700 text-white text-sm rounded-md border border-gray-600 focus:outline-none focus:ring-2 focus:ring-blue-500"
                        />
                        <button
                            onClick={runDumpBin}
                            className="px-4 py-2 bg-blue-600 text-white text-sm rounded-md hover:bg-blue-700 transition-colors"
                        >
                            Analyze
                        </button>
                    </div>
                </div>

                <div>
                    <label className="block text-sm font-medium text-gray-300 mb-2">
                        Compiler (Masm64)
                    </label>
                    <div className="flex space-x-2">
                        <input
                            type="text"
                            value={sourceFile}
                            onChange={(e) => setSourceFile(e.target.value)}
                            placeholder="C:\\path\\to\\source.asm"
                            className="flex-1 px-3 py-2 bg-gray-700 text-white text-sm rounded-md border border-gray-600 focus:outline-none focus:ring-2 focus:ring-blue-500"
                        />
                        <button
                            onClick={runCompiler}
                            className="px-4 py-2 bg-green-600 text-white text-sm rounded-md hover:bg-green-700 transition-colors"
                        >
                            Compile
                        </button>
                    </div>
                </div>

                <div className="text-xs text-gray-500">
                    <p>💡 Tip: Use these tools to analyze and compile native code.</p>
                </div>
            </div>
        </div>
    );
};

export default REToolsPanel;
)";
}

std::string ReactServerGenerator::GetMainIDEAppContent(const ReactServerConfig& config) {
    return R"(import React, { useState, useEffect } from 'react';
import MonacoEditor from './MonacoEditor';
import AgentModePanel from './AgentModePanel';
import EngineManager from './EngineManager';
import MemoryViewer from './MemoryViewer';
import ToolOutputPanel from './ToolOutputPanel';
import HotpatchControls from './HotpatchControls';
import REToolsPanel from './REToolsPanel';

const IDEApp = () => {
    const [code, setCode] = useState('// Welcome to RawrXD AI IDE\n#include <iostream>\n\nint main() {\n    std::cout << "Hello, World!" << std::endl;\n    return 0;\n}');
    const [agentSettings, setAgentSettings] = useState({
        mode: 'ask',
        deepThinking: false,
        deepResearch: false,
        noRefusal: false,
        contextLimit: 4096
    });
    const [currentEngine, setCurrentEngine] = useState('');
    const [toolOutputs, setToolOutputs] = useState({});
    const [inferenceResult, setInferenceResult] = useState('');

    const handleInference = async () => {
        try {
            const response = await fetch('/api/inference', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    prompt: code,
                    mode: agentSettings.mode,
                    deepThinking: agentSettings.deepThinking,
                    deepResearch: agentSettings.deepResearch,
                    noRefusal: agentSettings.noRefusal,
                    contextLimit: agentSettings.contextLimit
                })
            });

            const result = await response.json();
            setInferenceResult(result.output || result.error || 'No response');
        } catch (error) {
            setInferenceResult(`Error: ${error.message}`);
        }
    };

    const handleToolOutput = (tool, output) => {
        setToolOutputs(prev => ({ ...prev, [tool]: output }));
    };

    return (
        <div className="min-h-screen bg-gray-900 text-white">
            {/* Header */}
            <header className="bg-gray-800 border-b border-gray-700 p-4">
                <div className="flex items-center justify-between">
                    <div className="flex items-center space-x-4">
                        <h1 className="text-2xl font-bold text-blue-400">RawrXD AI IDE</h1>
                        <span className="text-sm text-gray-400">v2.0.0</span>
                    </div>
                    <div className="flex items-center space-x-4">
                        <span className="text-sm text-gray-400">Engine: {currentEngine || 'None'}</span>
                        <button
                            onClick={handleInference}
                            className="px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700 transition-colors"
                        >
                            Run Inference
                        </button>
                    </div>
                </div>
            </header>

            <div className="flex h-screen pt-16">
                {/* Left Sidebar - Controls */}
                <div className="w-80 bg-gray-800 border-r border-gray-700 overflow-y-auto">
                    <div className="p-4 space-y-4">
                        <AgentModePanel 
                            onModeChange={setAgentSettings}
                            currentMode={agentSettings.mode}
                        />
                        
                        <EngineManager 
                            onEngineChange={setCurrentEngine}
                            currentEngine={currentEngine}
                        />
                        
                        <MemoryViewer />
                        
                        <HotpatchControls />
                        
                        <REToolsPanel onToolOutput={handleToolOutput} />
                    </div>
                </div>

                {/* Center - Editor */}
                <div className="flex-1 flex flex-col">
                    <div className="flex-1 bg-gray-900">
                        <MonacoEditor 
                            value={code}
                            onChange={setCode}
                            language="cpp"
                        />
                    </div>
                    
                    {/* Inference Output */}
                    <div className="h-48 bg-black border-t border-gray-700">
                        <div className="p-2 bg-gray-800 border-b border-gray-700">
                            <span className="text-sm font-medium text-gray-300">Inference Output</span>
                        </div>
                        <div className="p-3 h-full overflow-y-auto text-sm font-mono text-green-400">
                            {inferenceResult}
                        </div>
                    </div>
                </div>

                {/* Right Sidebar - Tools */}
                <div className="w-80 bg-gray-800 border-l border-gray-700 overflow-y-auto">
                    <div className="p-4">
                        <ToolOutputPanel outputs={toolOutputs} />
                    </div>
                </div>
            </div>
        </div>
    );
};

export default IDEApp;
)";
}

} // namespace RawrXD
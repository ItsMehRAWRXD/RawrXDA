import React, { useEffect, useState } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { CodeEditor } from '@/components/CodeEditor';
import { MemoryPanel } from '@/components/MemoryPanel';
import { HotPatchPanel } from '@/components/HotPatchPanel';
import { VSIXLoaderPanel } from '@/components/VSIXLoaderPanel';
import { AISettingsPanel } from '@/components/AISettingsPanel';
import { Terminal } from 'lucide-react';

function App() {
  const { connect, isConnected, logs } = useEngineStore();
  const [code, setCode] = useState('// RawrXD Monaco IDE\n#include <iostream>\n\nint main() {\n    std::cout << "Hello, RawrXD" << std::endl;\n    return 0;\n}\n');
  const [status, setStatus] = useState({
    ready: false,
    model_loaded: false,
    model_path: '',
    backend: 'rawrxd',
    capabilities: { completion: true, streaming: false }
  });

  useEffect(() => {
    connect();
    const fetchStatus = async () => {
      try {
        const res = await fetch('/status');
        if (res.ok) {
          const data = await res.json();
          setStatus(data);
        }
      } catch (err) {
        setStatus(prev => ({ ...prev, ready: false, model_loaded: false }));
      }
    };
    fetchStatus();
    const timer = setInterval(fetchStatus, 3000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div className="h-screen w-screen bg-background text-foreground flex overflow-hidden">
      {/* Sidebar */}
      <div className="w-16 flex-none bg-secondary border-r border-border flex flex-col items-center py-4 space-y-4">
        <div className="w-10 h-10 rounded-lg bg-primary flex items-center justify-center font-bold text-white">RX</div>
        <div className="flex-1" />
        <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`} />
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col min-w-0">
        <header className="h-12 border-b border-border flex items-center px-4 justify-between bg-card">
          <div className="flex items-center gap-3">
            <h1 className="font-semibold">RawrXD IDE</h1>
            <span className={`text-xs px-2 py-0.5 rounded ${status.ready ? 'bg-green-600/20 text-green-300' : 'bg-red-600/20 text-red-300'}`}>
              {status.ready ? 'ENGINE ONLINE' : 'ENGINE OFFLINE'}
            </span>
            <span className={`text-xs px-2 py-0.5 rounded ${status.model_loaded ? 'bg-blue-600/20 text-blue-300' : 'bg-yellow-600/20 text-yellow-300'}`}>
              {status.model_loaded ? 'MODEL LOADED' : 'NO MODEL'}
            </span>
          </div>
          <div className="text-xs font-mono text-muted-foreground">{status.model_path || 'v1.0.0-alpha'}</div>
        </header>

        <main className="flex-1 overflow-hidden grid grid-cols-12 auto-rows-fr">
          {/* Left Panel - Editor Area */}
          <div className="col-span-8 border-r border-border bg-[#1e1e1e] p-2">
            <CodeEditor value={code} onChange={setCode} language="cpp" />
          </div>

          {/* Right Panel - Tools */}
          <div className="col-span-4 flex flex-col bg-background">
            <div className="flex-1 overflow-y-auto border-b border-border">
              <AISettingsPanel />
              <div className="h-px bg-border my-2" />
              <MemoryPanel />
              <div className="h-px bg-border my-2" />
              <HotPatchPanel />
              <div className="h-px bg-border my-2" />
              <VSIXLoaderPanel />
            </div>

            {/* Terminal / Logs */}
            <div className="h-48 flex-none bg-black p-2 font-mono text-xs overflow-y-auto text-green-400">
              <div className="flex items-center gap-2 text-white mb-2 border-b border-gray-800 pb-1">
                <Terminal className="w-3 h-3" /> Output
              </div>
              {logs.map((log, i) => (
                <div key={i}>{log}</div>
              ))}
              <div className="animate-pulse">_</div>
            </div>
          </div>
        </main>
      </div>
    </div>
  );
}

export default App;

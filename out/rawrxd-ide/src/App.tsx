import React, { useEffect, useState } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { CodeEditor } from '@/components/CodeEditor';
import { AISettingsPanel } from '@/components/AISettingsPanel';
import { Win32IDELayout } from '@/components/Win32IDELayout';
import { Terminal, Activity, AlertCircle } from 'lucide-react';

function App() {
  const { connect, isConnected, logs } = useEngineStore();
  const [code, setCode] = useState('// RawrXD Monaco IDE\n#include <iostream>\n\nint main() {\n    std::cout << "Hello, RawrXD" << std::endl;\n    return 0;\n}\n');
  const [activeFile, setActiveFile] = useState('main.cpp');
  const [language, setLanguage] = useState('cpp');
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
  }, [connect]);

  // Detect language from file extension
  useEffect(() => {
    const ext = activeFile.split('.').pop()?.toLowerCase() || '';
    const langMap: Record<string, string> = {
      cpp: 'cpp', cc: 'cpp', cxx: 'cpp', h: 'cpp', hpp: 'cpp',
      c: 'c', py: 'python', js: 'javascript', ts: 'typescript',
      tsx: 'typescript', jsx: 'javascript', rs: 'rust', go: 'go',
      json: 'json', md: 'markdown', html: 'html', css: 'css',
      asm: 'asm', masm: 'asm',
    };
    setLanguage(langMap[ext] || 'plaintext');
  }, [activeFile]);

  return (
    <Win32IDELayout
      status={status}
      activeFile={activeFile}
      onFileSelect={setActiveFile}
      editor={
        <CodeEditor
          value={code}
          onChange={setCode}
          language={language}
        />
      }
      terminal={
        <div className="h-full bg-[#1e1e1e] font-mono text-xs p-2 overflow-y-auto">
          {logs.length === 0 ? (
            <div className="text-gray-500 italic">
              {isConnected ? 'Connected to RawrXD Engine. Type commands below.' : 'Connecting...'}
            </div>
          ) : (
            logs.slice(-200).map((log, i) => (
              <div key={i} className={`py-0.5 ${log.startsWith('[Error') ? 'text-red-400' :
                  log.startsWith('[System') ? 'text-blue-400' :
                    log.startsWith('[Plugin') ? 'text-green-400' :
                      log.startsWith('>') ? 'text-yellow-300' :
                        'text-gray-300'
                }`}>{log}</div>
            ))
          )}
        </div>
      }
    >
      {/* Sidebar Content - AI Settings */}
      <AISettingsPanel />
    </Win32IDELayout>
  );
}

export default App;
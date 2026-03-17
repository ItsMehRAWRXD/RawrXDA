import React, { useEffect, useState } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { CodeEditor } from '@/components/CodeEditor';
import { AISettingsPanel } from '@/components/AISettingsPanel';
import { Win32IDELayout } from '@/components/Win32IDELayout';
import { Terminal, Activity, AlertCircle } from 'lucide-react';

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
  }, [connect]);

  return (
    <Win32IDELayout>
      {/* Sidebar Content - AI Settings */}
      <AISettingsPanel />
    </Win32IDELayout>
  );
}

export default App;
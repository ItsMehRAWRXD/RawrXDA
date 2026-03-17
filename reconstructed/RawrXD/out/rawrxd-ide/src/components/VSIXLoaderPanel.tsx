import React, { useState, useRef, useCallback } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Package, Upload, Play, Settings, Trash2, X, Loader2 } from 'lucide-react';

export const VSIXLoaderPanel: React.FC = () => {
  const { activePlugins, loadPlugin, executeCommand } = useEngineStore();
  const [dragActive, setDragActive] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [uploadStatus, setUploadStatus] = useState('');
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [pluginSettings, setPluginSettings] = useState<string | null>(null);

  const handleDrag = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === "dragenter" || e.type === "dragover") {
      setDragActive(true);
    } else if (e.type === "dragleave") {
      setDragActive(false);
    }
  };

  const processFile = useCallback(async (file: File) => {
    if (!file.name.endsWith('.vsix')) {
      setUploadStatus('❌ Only .vsix files are supported');
      return;
    }

    setUploading(true);
    setUploadStatus(`Installing ${file.name}...`);

    try {
      // Upload to backend
      const formData = new FormData();
      formData.append('vsix', file);

      const res = await fetch('/api/plugins/install', {
        method: 'POST',
        body: formData,
      });

      if (res.ok) {
        const data = await res.json();
        await loadPlugin(data.path || file.name);
        setUploadStatus(`✅ Installed: ${file.name}`);
      } else {
        // Fallback: local-only install
        await loadPlugin(file.name);
        setUploadStatus(`✅ Loaded locally: ${file.name}`);
      }
    } catch {
      // Offline fallback
      await loadPlugin(file.name);
      setUploadStatus(`✅ Loaded locally: ${file.name}`);
    }

    setUploading(false);
  }, [loadPlugin]);

  const handleDrop = useCallback(async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setDragActive(false);

    const files = Array.from(e.dataTransfer.files);
    const vsixFile = files.find(f => f.name.endsWith('.vsix'));
    if (vsixFile) {
      await processFile(vsixFile);
    } else {
      setUploadStatus('❌ Please drop a .vsix file');
    }
  }, [processFile]);

  const handleFileSelect = useCallback(async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      await processFile(file);
    }
    // Reset input so the same file can be selected again
    if (fileInputRef.current) fileInputRef.current.value = '';
  }, [processFile]);

  const handleUnloadPlugin = async (pluginName: string) => {
    try {
      await fetch('/api/plugins/unload', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: pluginName }),
      });
    } catch { }
    useEngineStore.setState(state => ({
      activePlugins: state.activePlugins.filter(p => p !== pluginName),
    }));
    await executeCommand(`!plugin unload ${pluginName}`);
  };

  return (
    <div className="p-4 space-y-4 h-full flex flex-col">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Package className="w-6 h-6 text-blue-500" /> VSIX Loader
      </h2>

      {/* Hidden file input for click-to-browse */}
      <input
        ref={fileInputRef}
        type="file"
        accept=".vsix"
        className="hidden"
        onChange={handleFileSelect}
      />

      <div
        className={`flex-1 min-h-[120px] border-2 border-dashed rounded-lg flex flex-col items-center justify-center p-8 transition-colors cursor-pointer ${dragActive ? 'border-primary bg-primary/10' : 'border-border hover:border-gray-500'
          }`}
        onDragEnter={handleDrag}
        onDragLeave={handleDrag}
        onDragOver={handleDrag}
        onDrop={handleDrop}
        onClick={() => fileInputRef.current?.click()}
      >
        {uploading ? (
          <Loader2 className="w-12 h-12 text-purple-400 mb-4 animate-spin" />
        ) : (
          <Upload className="w-12 h-12 text-muted-foreground mb-4" />
        )}
        <p className="text-lg font-medium">Drag & Drop .vsix file</p>
        <p className="text-sm text-muted-foreground mt-2">
          or click to browse filesystem
        </p>
        {uploadStatus && (
          <p className={`text-sm mt-3 ${uploadStatus.startsWith('✅') ? 'text-green-400' :
              uploadStatus.startsWith('❌') ? 'text-red-400' : 'text-blue-400'
            }`}>
            {uploadStatus}
          </p>
        )}
      </div>

      <div className="space-y-2">
        <h3 className="text-sm font-semibold">Active Plugins ({activePlugins.length})</h3>
        <div className="space-y-2">
          {activePlugins.length === 0 ? (
            <div className="text-sm text-muted-foreground italic">No plugins loaded</div>
          ) : (
            activePlugins.map((plugin, idx) => (
              <div key={idx} className="flex items-center justify-between p-3 bg-secondary rounded-lg">
                <div className="flex items-center gap-3">
                  <div className="w-2 h-2 rounded-full bg-green-500" />
                  <span className="font-medium">{plugin}</span>
                </div>
                <div className="flex gap-1">
                  <button
                    onClick={() => setPluginSettings(pluginSettings === plugin ? null : plugin)}
                    className="p-1.5 hover:bg-background rounded"
                    title="Settings"
                  >
                    <Settings className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => handleUnloadPlugin(plugin)}
                    className="p-1.5 hover:bg-background rounded text-red-400"
                    title="Unload"
                  >
                    <Trash2 className="w-4 h-4" />
                  </button>
                </div>
              </div>
            ))
          )}
        </div>

        {/* Plugin settings panel */}
        {pluginSettings && (
          <div className="bg-card border border-border rounded-lg p-3 space-y-2">
            <div className="flex items-center justify-between">
              <span className="text-sm font-semibold">{pluginSettings} Settings</span>
              <button onClick={() => setPluginSettings(null)} className="p-1 hover:bg-secondary rounded">
                <X className="w-4 h-4" />
              </button>
            </div>
            <div className="text-xs text-muted-foreground">
              Plugin configuration is managed by the extension host.
              Use the command palette (!plugin config {pluginSettings}) for advanced settings.
            </div>
            <button
              onClick={() => executeCommand(`!plugin config ${pluginSettings}`)}
              className="px-3 py-1 bg-secondary hover:bg-secondary/80 rounded text-sm"
            >
              Open Config
            </button>
          </div>
        )}
      </div>
    </div>
  );
};

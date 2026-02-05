import React, { useState } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Package, Upload, Play, Settings } from 'lucide-react';

export const VSIXLoaderPanel: React.FC = () => {
  const { activePlugins, loadPlugin } = useEngineStore();
  const [dragActive, setDragActive] = useState(false);

  const handleDrag = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === "dragenter" || e.type === "dragover") {
      setDragActive(true);
    } else if (e.type === "dragleave") {
      setDragActive(false);
    }
  };

  return (
    <div className="p-4 space-y-4 h-full flex flex-col">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Package className="w-6 h-6 text-blue-500" /> VSIX Loader
      </h2>

      <div 
        className={`flex-1 border-2 border-dashed rounded-lg flex flex-col items-center justify-center p-8 transition-colors ${
          dragActive ? 'border-primary bg-primary/10' : 'border-border'
        }`}
        onDragEnter={handleDrag}
        onDragLeave={handleDrag}
        onDragOver={handleDrag}
      >
        <Upload className="w-12 h-12 text-muted-foreground mb-4" />
        <p className="text-lg font-medium">Drag & Drop .vsix file</p>
        <p className="text-sm text-muted-foreground mt-2">
          or click to browse filesystem
        </p>
      </div>

      <div className="space-y-2">
        <h3 className="text-sm font-semibold">Active Plugins</h3>
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
                <div className="flex gap-2">
                  <button className="p-1.5 hover:bg-background rounded"><Settings className="w-4 h-4" /></button>
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
};

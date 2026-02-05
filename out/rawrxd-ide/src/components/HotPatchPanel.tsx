import React from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Zap, Shield, RotateCcw } from 'lucide-react';

export const HotPatchPanel: React.FC = () => {
  const { executeCommand } = useEngineStore();

  const handleApplyPatch = async () => {
    await executeCommand('!patch apply memory_fix_0x1A');
  };

  const handleRevert = async () => {
    await executeCommand('!patch revert memory_fix_0x1A');
  };

  return (
    <div className="p-4 space-y-4">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Zap className="w-6 h-6 text-yellow-500" /> Hot Patcher
      </h2>
      
      <div className="bg-card border border-border rounded-lg overflow-hidden">
        <table className="w-full text-sm">
          <thead className="bg-secondary text-secondary-foreground">
            <tr>
              <th className="p-2 text-left">Patch ID</th>
              <th className="p-2 text-left">Target</th>
              <th className="p-2 text-left">Status</th>
              <th className="p-2 text-right">Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr className="border-t border-border">
              <td className="p-2 font-mono">MEM_FIX_01</td>
              <td className="p-2">src/memory_core.cpp</td>
              <td className="p-2 text-green-500">Active</td>
              <td className="p-2 text-right">
                <button onClick={handleRevert} className="p-1 hover:bg-secondary rounded">
                  <RotateCcw className="w-4 h-4" />
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <div className="p-4 bg-secondary/20 rounded border border-dashed border-border">
        <div className="flex items-center gap-2 text-sm text-yellow-500 mb-2">
          <Shield className="w-4 h-4" />
          Safety Checks Enabled
        </div>
        <p className="text-xs text-muted-foreground">
          Hot patches are applied directly to process memory. Ensure offsets are verified against the current build signature.
        </p>
      </div>
    </div>
  );
};

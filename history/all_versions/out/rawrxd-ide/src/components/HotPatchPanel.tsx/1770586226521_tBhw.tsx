import React, { useState, useEffect, useCallback } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Zap, Shield, RotateCcw, Plus, Trash2, RefreshCw, Loader2 } from 'lucide-react';

interface PatchEntry {
  id: string;
  target: string;
  layer: 'memory' | 'byte' | 'server';
  status: 'active' | 'reverted' | 'failed';
  description?: string;
}

interface PatchStats {
  memoryPatchCount: number;
  bytePatchCount: number;
  serverPatchCount: number;
  totalOperations: number;
  totalFailures: number;
}

export const HotPatchPanel: React.FC = () => {
  const { executeCommand, isConnected } = useEngineStore();
  const [patches, setPatches] = useState<PatchEntry[]>([]);
  const [stats, setStats] = useState<PatchStats>({
    memoryPatchCount: 0, bytePatchCount: 0, serverPatchCount: 0,
    totalOperations: 0, totalFailures: 0,
  });
  const [loading, setLoading] = useState(false);
  const [newPatchAddr, setNewPatchAddr] = useState('');
  const [newPatchBytes, setNewPatchBytes] = useState('');
  const [showCreateForm, setShowCreateForm] = useState(false);

  // Fetch patches from backend
  const fetchPatches = useCallback(async () => {
    try {
      const res = await fetch('/api/hotpatch/list');
      if (res.ok) {
        const data = await res.json();
        setPatches(data.patches || []);
        if (data.stats) setStats(data.stats);
      }
    } catch {
      // Offline — keep local state
    }
  }, []);

  // Fetch stats
  const fetchStats = useCallback(async () => {
    try {
      const res = await fetch('/api/hotpatch/stats');
      if (res.ok) {
        const data = await res.json();
        setStats(data);
      }
    } catch {}
  }, []);

  useEffect(() => {
    if (isConnected) {
      fetchPatches();
      const timer = setInterval(fetchStats, 5000);
      return () => clearInterval(timer);
    }
  }, [isConnected, fetchPatches, fetchStats]);

  const handleApplyPatch = async (patchId: string) => {
    setLoading(true);
    try {
      const res = await fetch('/api/hotpatch/apply', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: patchId }),
      });
      if (res.ok) {
        await fetchPatches();
      } else {
        await executeCommand(`!hotpatch_apply ${patchId}`);
      }
    } catch {
      await executeCommand(`!hotpatch_apply ${patchId}`);
    }
    setLoading(false);
  };

  const handleRevert = async (patchId: string) => {
    setLoading(true);
    try {
      const res = await fetch('/api/hotpatch/revert', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: patchId }),
      });
      if (res.ok) {
        await fetchPatches();
      } else {
        await executeCommand(`!hotpatch_revert ${patchId}`);
      }
    } catch {
      await executeCommand(`!hotpatch_revert ${patchId}`);
    }
    setLoading(false);
  };

  const handleCreatePatch = async () => {
    if (!newPatchAddr || !newPatchBytes) return;
    setLoading(true);
    try {
      const res = await fetch('/api/hotpatch/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          address: newPatchAddr,
          bytes: newPatchBytes,
          layer: 'memory',
        }),
      });
      if (res.ok) {
        setNewPatchAddr('');
        setNewPatchBytes('');
        setShowCreateForm(false);
        await fetchPatches();
      }
    } catch {
      await executeCommand(`!hotpatch_apply ${newPatchAddr} ${newPatchBytes}`);
    }
    setLoading(false);
  };

  const handleRemovePatch = async (patchId: string) => {
    try {
      await fetch(`/api/hotpatch/remove`, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: patchId }),
      });
      await fetchPatches();
    } catch {
      setPatches(p => p.filter(x => x.id !== patchId));
    }
  };

  return (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold flex items-center gap-2">
          <Zap className="w-6 h-6 text-yellow-500" /> Hot Patcher
        </h2>
        <div className="flex gap-2">
          <button
            onClick={fetchPatches}
            className="p-1.5 hover:bg-secondary rounded"
            title="Refresh"
          >
            <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
          </button>
          <button
            onClick={() => setShowCreateForm(!showCreateForm)}
            className="p-1.5 hover:bg-secondary rounded bg-purple-700"
            title="Create Patch"
          >
            <Plus className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Stats bar */}
      <div className="grid grid-cols-5 gap-2 text-xs">
        <div className="bg-card p-2 rounded border border-border text-center">
          <div className="text-muted-foreground">Memory</div>
          <div className="font-mono text-lg">{stats.memoryPatchCount}</div>
        </div>
        <div className="bg-card p-2 rounded border border-border text-center">
          <div className="text-muted-foreground">Byte</div>
          <div className="font-mono text-lg">{stats.bytePatchCount}</div>
        </div>
        <div className="bg-card p-2 rounded border border-border text-center">
          <div className="text-muted-foreground">Server</div>
          <div className="font-mono text-lg">{stats.serverPatchCount}</div>
        </div>
        <div className="bg-card p-2 rounded border border-border text-center">
          <div className="text-muted-foreground">Total Ops</div>
          <div className="font-mono text-lg">{stats.totalOperations}</div>
        </div>
        <div className="bg-card p-2 rounded border border-border text-center">
          <div className="text-muted-foreground">Failures</div>
          <div className="font-mono text-lg text-red-400">{stats.totalFailures}</div>
        </div>
      </div>

      {/* Create form */}
      {showCreateForm && (
        <div className="bg-card border border-border rounded-lg p-3 space-y-2">
          <div className="text-sm font-semibold">Create Memory Patch</div>
          <input
            type="text"
            placeholder="Address (hex, e.g. 0x7FFE1234)"
            value={newPatchAddr}
            onChange={(e) => setNewPatchAddr(e.target.value)}
            className="w-full bg-secondary border border-border rounded px-2 py-1 text-sm font-mono"
          />
          <input
            type="text"
            placeholder="Bytes (hex, e.g. 90 90 90)"
            value={newPatchBytes}
            onChange={(e) => setNewPatchBytes(e.target.value)}
            className="w-full bg-secondary border border-border rounded px-2 py-1 text-sm font-mono"
          />
          <div className="flex gap-2">
            <button
              onClick={handleCreatePatch}
              disabled={loading}
              className="px-3 py-1 bg-purple-700 hover:bg-purple-600 rounded text-sm disabled:opacity-50"
            >
              {loading ? <Loader2 className="w-4 h-4 animate-spin" /> : 'Apply'}
            </button>
            <button
              onClick={() => setShowCreateForm(false)}
              className="px-3 py-1 bg-secondary hover:bg-secondary/80 rounded text-sm"
            >
              Cancel
            </button>
          </div>
        </div>
      )}

      {/* Patch table */}
      <div className="bg-card border border-border rounded-lg overflow-hidden">
        <table className="w-full text-sm">
          <thead className="bg-secondary text-secondary-foreground">
            <tr>
              <th className="p-2 text-left">Patch ID</th>
              <th className="p-2 text-left">Layer</th>
              <th className="p-2 text-left">Target</th>
              <th className="p-2 text-left">Status</th>
              <th className="p-2 text-right">Actions</th>
            </tr>
          </thead>
          <tbody>
            {patches.length === 0 ? (
              <tr>
                <td colSpan={5} className="p-4 text-center text-muted-foreground italic">
                  No patches loaded. Create one above or connect to backend.
                </td>
              </tr>
            ) : (
              patches.map((patch) => (
                <tr key={patch.id} className="border-t border-border hover:bg-secondary/30">
                  <td className="p-2 font-mono">{patch.id}</td>
                  <td className="p-2">
                    <span className={`px-1.5 py-0.5 rounded text-xs ${
                      patch.layer === 'memory' ? 'bg-blue-900 text-blue-300' :
                      patch.layer === 'byte' ? 'bg-green-900 text-green-300' :
                      'bg-purple-900 text-purple-300'
                    }`}>{patch.layer}</span>
                  </td>
                  <td className="p-2">{patch.target}</td>
                  <td className={`p-2 ${
                    patch.status === 'active' ? 'text-green-500' :
                    patch.status === 'failed' ? 'text-red-500' :
                    'text-gray-500'
                  }`}>{patch.status}</td>
                  <td className="p-2 text-right flex gap-1 justify-end">
                    {patch.status === 'active' && (
                      <button onClick={() => handleRevert(patch.id)} className="p-1 hover:bg-secondary rounded" title="Revert">
                        <RotateCcw className="w-4 h-4" />
                      </button>
                    )}
                    {patch.status === 'reverted' && (
                      <button onClick={() => handleApplyPatch(patch.id)} className="p-1 hover:bg-secondary rounded" title="Re-apply">
                        <Zap className="w-4 h-4" />
                      </button>
                    )}
                    <button onClick={() => handleRemovePatch(patch.id)} className="p-1 hover:bg-secondary rounded text-red-400" title="Remove">
                      <Trash2 className="w-4 h-4" />
                    </button>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      <div className="p-4 bg-secondary/20 rounded border border-dashed border-border">
        <div className="flex items-center gap-2 text-sm text-yellow-500 mb-2">
          <Shield className="w-4 h-4" />
          Safety Checks Enabled
        </div>
        <p className="text-xs text-muted-foreground">
          Hot patches use VirtualProtect (memory layer), CreateFileMapping (byte layer), and injection points (server layer).
          All operations are tracked by the UnifiedHotpatchManager with atomic rollback support.
        </p>
      </div>
    </div>
  );
};

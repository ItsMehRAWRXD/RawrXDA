import React, { useEffect, useCallback } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Activity, Cpu, HardDrive, RefreshCw } from 'lucide-react';

export const MemoryPanel: React.FC = () => {
  const { memoryTier, memoryUsage, memoryCapacity, setMemoryTier, isConnected } = useEngineStore();
  const usagePercent = memoryCapacity > 0 ? (memoryUsage / memoryCapacity) * 100 : 0;

  // Poll real memory stats from backend
  const fetchMemoryStats = useCallback(async () => {
    try {
      const res = await fetch('/api/memory/stats');
      if (res.ok) {
        const data = await res.json();
        // Update store with real values if available
        if (data.tier) {
          useEngineStore.setState({
            memoryUsage: data.usage ?? memoryUsage,
            memoryCapacity: data.capacity ?? memoryCapacity,
            memoryTier: data.tier ?? memoryTier,
          });
        }
      }
    } catch {
      // Offline — keep current state
    }
  }, [memoryUsage, memoryCapacity, memoryTier]);

  useEffect(() => {
    if (isConnected) {
      fetchMemoryStats();
      const timer = setInterval(fetchMemoryStats, 3000);
      return () => clearInterval(timer);
    }
  }, [isConnected, fetchMemoryStats]);

  const usageColor = usagePercent > 90 ? 'text-red-400' :
                     usagePercent > 70 ? 'text-yellow-400' : 'text-green-400';
  const barColor = usagePercent > 90 ? 'bg-red-500' :
                   usagePercent > 70 ? 'bg-yellow-500' : 'bg-green-500';

  return (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold flex items-center gap-2">
          <Cpu className="w-6 h-6" /> Memory Core
        </h2>
        <button
          onClick={fetchMemoryStats}
          className="p-1.5 hover:bg-secondary rounded"
          title="Refresh"
        >
          <RefreshCw className="w-4 h-4" />
        </button>
      </div>
      
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div className="bg-card p-4 rounded-lg border border-border">
          <div className="text-sm text-muted-foreground">Current Tier</div>
          <div className="text-2xl font-mono text-primary">{memoryTier}</div>
        </div>
        <div className="bg-card p-4 rounded-lg border border-border">
          <div className="text-sm text-muted-foreground">Utilization</div>
          <div className={`text-2xl font-mono ${usageColor}`}>{usagePercent.toFixed(1)}%</div>
          <div className="w-full bg-secondary h-2 mt-2 rounded overflow-hidden">
            <div 
              className={`${barColor} h-full transition-all duration-300`}
              style={{ width: `${Math.min(usagePercent, 100)}%` }}
            />
          </div>
          <div className="text-xs text-muted-foreground mt-1">
            {(memoryUsage / 1024).toFixed(1)}K / {(memoryCapacity / 1024).toFixed(1)}K tokens
          </div>
        </div>
        <div className="bg-card p-4 rounded-lg border border-border">
          <div className="text-sm text-muted-foreground">Capacity</div>
          <div className="text-2xl font-mono">{(memoryCapacity / 1024).toFixed(0)}K Tokens</div>
        </div>
      </div>

      <div className="space-y-2">
        <h3 className="text-sm font-semibold">Tier Selection</h3>
        <div className="flex gap-2 flex-wrap">
          {['TIER_4K', 'TIER_32K', 'TIER_64K', 'TIER_128K', 'TIER_1M'].map((tier) => (
            <button
              key={tier}
              onClick={() => setMemoryTier(tier)}
              className={`px-3 py-1 text-sm rounded transition-colors ${
                memoryTier === tier 
                  ? 'bg-primary text-primary-foreground' 
                  : 'bg-secondary hover:bg-secondary/80'
              }`}
            >
              {tier}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
};

import React from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Activity, Cpu, HardDrive } from 'lucide-react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';

export const MemoryPanel: React.FC = () => {
  const { memoryTier, memoryUsage, memoryCapacity, setMemoryTier } = useEngineStore();
  const usagePercent = (memoryUsage / memoryCapacity) * 100;

  return (
    <div className="p-4 space-y-4">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Cpu className="w-6 h-6" /> Memory Core
      </h2>
      
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div className="bg-card p-4 rounded-lg border border-border">
          <div className="text-sm text-muted-foreground">Current Tier</div>
          <div className="text-2xl font-mono text-primary">{memoryTier}</div>
        </div>
        <div className="bg-card p-4 rounded-lg border border-border">
          <div className="text-sm text-muted-foreground">Utilization</div>
          <div className="text-2xl font-mono text-accent">{usagePercent.toFixed(1)}%</div>
          <div className="w-full bg-secondary h-2 mt-2 rounded overflow-hidden">
            <div 
              className="bg-accent h-full transition-all duration-300" 
              style={{ width: `${usagePercent}%` }}
            />
          </div>
        </div>
        <div className="bg-card p-4 rounded-lg border border-border">
          <div className="text-sm text-muted-foreground">Capacity</div>
          <div className="text-2xl font-mono">{memoryCapacity / 1024}K Tokens</div>
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

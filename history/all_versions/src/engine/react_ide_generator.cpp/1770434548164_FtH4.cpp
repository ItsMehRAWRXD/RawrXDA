#include "react_ide_generator.h"
#include <fstream>
#include <sstream>
#include <iostream>

// ╔════════════════════════════════════════════════════════════════════════════╗
// ║  RAW-STRING DISCIPLINE — READ BEFORE EDITING THIS FILE                   ║
// ║                                                                          ║
// ║  Each Generate*Panel() returns a C++ raw string literal that contains     ║
// ║  an entire React/TypeScript component. Named delimiters (SUBAGENT,        ║
// ║  HISTORY, POLICY, EXPLAIN, FAILURE, CODEEDITOR, TAILWIND) prevent         ║
// ║  accidental closure by parentheses inside JSX.                            ║
// ║                                                                          ║
// ║  RULES:                                                                  ║
// ║   1. NEVER insert C++ code between a R"DELIM(  and its  )DELIM"          ║
// ║   2. NEVER use replace_string_in_file matching text that spans across     ║
// ║      a raw-string boundary — the match will land INSIDE the literal       ║
// ║   3. New panels go BETWEEN closing  )DELIM"; }  and the next function    ║
// ║   4. If you add a panel, add a guard comment pair at both boundaries      ║
// ║   5. Verify balance after every edit:                                     ║
// ║        Select-String -Pattern 'R"[A-Z]+\(' to count opens                ║
// ║        Select-String -Pattern '\)[A-Z]+"' to count closes                ║
// ║                                                                          ║
// ║  Phase 6B regression: a replacement matched `)POLICY"; }` and inserted   ║
// ║  new code INSIDE R"EXPLAIN(", splitting the ExplainabilityPanel in half. ║
// ║  This took 150+ lines of surgical repair. Don't repeat it.               ║
// ╚════════════════════════════════════════════════════════════════════════════╝

ReactIDEGenerator::ReactIDEGenerator() {
    InitializeTemplates();
}

void ReactIDEGenerator::InitializeTemplates() {
    // Minimal IDE Template
    ReactIDETemplate minimal;
    minimal.name = "minimal-ide";
    minimal.description = "A lightweight IDE with basic code editing and terminal";
    minimal.features = {"Monaco Editor", "Terminal", "File Explorer"};
    minimal.dependencies = {
        "react", "react-dom", "@monaco-editor/react", "xterm", "xterm-addon-fit", 
        "lucide-react", "clsx", "tailwind-merge"
    };
    minimal.devDependencies = {
        "@types/react", "@types/react-dom", "@vitejs/plugin-react", 
        "vite", "typescript", "tailwindcss", "postcss", "autoprefixer"
    };
    templates["minimal"] = minimal;

    // Full IDE Template
    ReactIDETemplate full;
    full.name = "full-ide";
    full.description = "Full-featured IDE with all RawrXD engine integrations";
    full.features = {
        "Monaco Editor", "Terminal", "File Explorer", "Memory Manager", 
        "Hot Patcher", "VSIX Loader", "Model Inference"
    };
    full.dependencies = minimal.dependencies;
    full.dependencies.insert(full.dependencies.end(), {
        "recharts", "framer-motion", "zustand", "socket.io-client", 
        "@radix-ui/react-tabs", "@radix-ui/react-dialog", "@radix-ui/react-scroll-area"
    });
    full.devDependencies = minimal.devDependencies;
    templates["full"] = full;

    // Agentic IDE Template
    ReactIDETemplate agentic;
    agentic.name = "agentic-ide";
    agentic.description = "AI-first IDE with autonomous agents and chat integration";
    agentic.features = {
        "Monaco Editor", "Terminal", "File Explorer", "Memory Manager", 
        "Hot Patcher", "VSIX Loader", "Model Inference", "Chat Interface",
        "Agent Orchestrator", "SubAgent Spawner", "Chain Executor", "HexMag Swarm"
    };
    agentic.dependencies = full.dependencies;
    agentic.dependencies.insert(agentic.dependencies.end(), {
        "react-markdown", "remark-gfm", "openai" // or local inference client
    });
    agentic.devDependencies = full.devDependencies;
    templates["agentic"] = agentic;
}

std::string ReactIDEGenerator::GeneratePackageJson(const ReactIDETemplate& tmpl) {
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << tmpl.name << "\",\n";
    json << "  \"version\": \"1.0.0\",\n";
    json << "  \"type\": \"module\",\n";
    json << "  \"scripts\": {\n";
    json << "    \"dev\": \"vite\",\n";
    json << "    \"build\": \"tsc && vite build\",\n";
    json << "    \"preview\": \"vite preview\"\n";
    json << "  },\n";
    json << "  \"dependencies\": {\n";
    for (size_t i = 0; i < tmpl.dependencies.size(); ++i) {
        json << "    \"" << tmpl.dependencies[i] << "\": \"latest\"" << (i < tmpl.dependencies.size() - 1 ? ",\n" : "\n");
    }
    json << "  },\n";
    json << "  \"devDependencies\": {\n";
    for (size_t i = 0; i < tmpl.devDependencies.size(); ++i) {
        json << "    \"" << tmpl.devDependencies[i] << "\": \"latest\"" << (i < tmpl.devDependencies.size() - 1 ? ",\n" : "\n");
    }
    json << "  }\n";
    json << "}\n";
    return json.str();
}

std::string ReactIDEGenerator::GenerateTsConfig() {
    return R"({
  "compilerOptions": {
    "target": "ES2020",
    "useDefineForClassFields": true,
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "react-jsx",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"]
    }
  },
  "include": ["src"],
  "references": [{ "path": "./tsconfig.node.json" }]
})";
}

std::string ReactIDEGenerator::GenerateViteConfig() {
    return R"(import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 3000,
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/complete': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/status': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      }
    }
  }
})";
}

std::string ReactIDEGenerator::GenerateTailwindConfig() {
    // ──── RAW STRING OPEN: R"TAILWIND( ──── do not insert C++ inside this block ────
    return R"TAILWIND(/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        background: "var(--background)",
        foreground: "var(--foreground)",
        primary: "var(--primary)",
        secondary: "var(--secondary)",
        accent: "var(--accent)",
      },
      fontFamily: {
        mono: ['JetBrains Mono', 'Fira Code', 'monospace']
      }
    },
  },
  plugins: [],
})TAILWIND";
    // ──── RAW STRING CLOSE: )TAILWIND" ──── end of TailwindConfig block ────
}

std::string ReactIDEGenerator::GenerateMainTsx() {
    return R"(import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App.tsx'
import './index.css'

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
)";
}

std::string ReactIDEGenerator::GenerateEngineBridge() {
    return R"(import { create } from 'zustand'

// Types for Engine Integration
export interface AgentInfo {
  id: string;
  description: string;
  status: string;
  result: string;
}

export interface TodoItem {
  id: number;
  title: string;
  description: string;
  status: 'not-started' | 'in-progress' | 'completed' | 'failed';
}

export interface ChainStepResult {
  step: string;
  output: string;
}

export interface HistoryEvent {
  id: number;
  eventType: string;
  sessionId: string;
  timestampMs: number;
  durationMs: number;
  agentId: string;
  parentId: string;
  description: string;
  input: string;
  output: string;
  metadata: string;
  success: boolean;
  errorMessage: string;
}

export interface HistoryStats {
  totalEvents: number;
  sessionId: string;
  successCount: number;
  failCount: number;
  timeSpanMs?: number;
  eventTypes: Record<string, number>;
}

export interface FailureRecord {
  timestampMs: number;
  type: string;
  reason: string;
  confidence: number;
  evidence: string;
  strategy: string;
  outcome: 'Detected' | 'Corrected' | 'Failed' | 'Declined';
  promptSnippet: string;
  sessionId: string;
  attempt: number;
}

export interface FailureStats {
  totalFailures: number;
  totalRetries: number;
  successAfterRetry: number;
  retriesDeclined: number;
  topReasons: { type: string; count: number }[];
}

export interface EngineState {
  isConnected: boolean;
  memoryTier: string;
  memoryUsage: number;
  memoryCapacity: number;
  activePlugins: string[];
  loadedModels: string[];
  logs: string[];
  
  // Agentic state
  agents: AgentInfo[];
  todos: TodoItem[];
  agenticReady: boolean;
  
  // Actions
  connect: () => Promise<void>;
  disconnect: () => void;
  executeCommand: (cmd: string) => Promise<string>;
  loadPlugin: (path: string) => Promise<boolean>;
  setMemoryTier: (tier: string) => Promise<boolean>;
  
  // Agentic actions
  chatWithAgent: (message: string) => Promise<string>;
  spawnSubAgent: (description: string, prompt: string, timeout?: number) => Promise<{ agentId: string; result: string; success: boolean }>;
  executeChain: (steps: string[], input: string) => Promise<{ result: string; stepResults: ChainStepResult[]; stepCount: number }>;
  executeSwarm: (prompts: string[], strategy?: string, maxParallel?: number) => Promise<{ result: string; taskCount: number; strategy: string }>;
  listAgents: () => Promise<AgentInfo[]>;
  getAgentStatus: () => Promise<{ summary: string; active: number; totalSpawned: number; todos: TodoItem[] }>;
  
  // Phase 5: History & Replay
  historyEvents: HistoryEvent[];
  historyStats: HistoryStats | null;
  getHistory: (opts?: { agentId?: string; eventType?: string; limit?: number }) => Promise<HistoryEvent[]>;
  replayAgent: (agentId: string, dryRun?: boolean) => Promise<{ success: boolean; result: string; eventsReplayed: number; durationMs: number }>;
  getHistoryStats: () => Promise<HistoryStats>;

  // Phase 6B: Failure Visibility
  failures: FailureRecord[];
  failureStats: FailureStats | null;
  getFailures: (opts?: { limit?: number; reason?: string }) => Promise<FailureRecord[]>;
  getAgentStatusDetailed: () => Promise<any>;
}

export const useEngineStore = create<EngineState>((set, get) => ({
  isConnected: false,
  memoryTier: 'TIER_4K',
  memoryUsage: 1024,
  memoryCapacity: 4096,
  activePlugins: [],
  loadedModels: [],
  logs: [],
  agents: [],
  todos: [],
  agenticReady: false,
  historyEvents: [],
  historyStats: null,
  failures: [],
  failureStats: null,

  connect: async () => {
    console.log("Connecting to RawrXD Engine...");
    try {
      const res = await fetch('/status');
      if (res.ok) {
        const data = await res.json();
        set({ 
          isConnected: true,
          agenticReady: data.agentic === true
        });
        get().logs.push("[System] Connected to RawrXD Engine v1.0.0");
        if (data.agentic) {
          get().logs.push("[System] Agentic subsystem online — SubAgents/Chains/Swarms available");
        }
      }
    } catch {
      setTimeout(() => set({ isConnected: true }), 1000);
    }
  },

  disconnect: () => {
    set({ isConnected: false, agenticReady: false });
  },

  executeCommand: async (cmd: string) => {
    try {
      const res = await fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: cmd })
      });
      if (res.ok) {
        const data = await res.json();
        const response = data.tool_result ? `${data.response}\n[Tool] ${data.tool_result}` : data.response;
        set(state => ({ logs: [...state.logs, `> ${cmd}`, response] }));
        return response;
      }
    } catch {}
    // Fallback
    set(state => ({ logs: [...state.logs, `> ${cmd}`, `[offline] ${cmd}`] }));
    return `Executed: ${cmd}`;
  },
  
  loadPlugin: async (path: string) => {
    set(state => ({ 
      activePlugins: [...state.activePlugins, path.split('/').pop() || path],
      logs: [...state.logs, `[Plugin] Loaded ${path}`] 
    }));
    return true;
  },

  setMemoryTier: async (tier: string) => {
    set({ memoryTier: tier });
    return true;
  },

  // ====================================================================
  // Agentic API methods — talk to RawrEngine /api/* endpoints
  // ====================================================================
  
  chatWithAgent: async (message: string) => {
    try {
      const res = await fetch('/api/chat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message })
      });
      if (res.ok) {
        const data = await res.json();
        const response = data.tool_result ? `${data.response}\n\n**Tool Result:** ${data.tool_result}` : data.response;
        set(state => ({ logs: [...state.logs, `[Chat] ${message.substring(0, 60)}...`, `[AI] ${response.substring(0, 100)}...`] }));
        return response;
      }
    } catch (err) {
      console.error('Chat error:', err);
    }
    return 'Error: Could not reach the engine.';
  },

  spawnSubAgent: async (description: string, prompt: string, timeout = 30) => {
    try {
      const res = await fetch('/api/subagent', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ description, prompt, timeout })
      });
      if (res.ok) {
        const data = await res.json();
        set(state => ({
          logs: [...state.logs, `[SubAgent] Spawned: ${data.agent_id} — ${description}`],
          agents: [...state.agents, { id: data.agent_id, description, status: data.success ? 'completed' : 'failed', result: data.result }]
        }));
        return { agentId: data.agent_id, result: data.result, success: data.success };
      }
    } catch (err) {
      console.error('SubAgent error:', err);
    }
    return { agentId: '', result: 'Error spawning subagent', success: false };
  },

  executeChain: async (steps: string[], input: string) => {
    try {
      const res = await fetch('/api/chain', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ steps, input })
      });
      if (res.ok) {
        const data = await res.json();
        set(state => ({ logs: [...state.logs, `[Chain] Executed ${data.step_count} steps`] }));
        return { result: data.result, stepResults: data.steps || [], stepCount: data.step_count };
      }
    } catch (err) {
      console.error('Chain error:', err);
    }
    return { result: 'Error executing chain', stepResults: [], stepCount: 0 };
  },

  executeSwarm: async (prompts: string[], strategy = 'concatenate', maxParallel = 4) => {
    try {
      const res = await fetch('/api/swarm', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompts, strategy, maxParallel })
      });
      if (res.ok) {
        const data = await res.json();
        set(state => ({ logs: [...state.logs, `[Swarm] Launched ${data.task_count} tasks (${data.strategy})`] }));
        return { result: data.result, taskCount: data.task_count, strategy: data.strategy };
      }
    } catch (err) {
      console.error('Swarm error:', err);
    }
    return { result: 'Error executing swarm', taskCount: 0, strategy };
  },

  listAgents: async () => {
    try {
      const res = await fetch('/api/agents');
      if (res.ok) {
        const data = await res.json();
        const agentList: AgentInfo[] = data.agents || [];
        set({ agents: agentList });
        return agentList;
      }
    } catch (err) {
      console.error('List agents error:', err);
    }
    return [];
  },

  getAgentStatus: async () => {
    try {
      const res = await fetch('/api/agents/status');
      if (res.ok) {
        const data = await res.json();
        const todos: TodoItem[] = data.todos || [];
        set({ todos });
        return { summary: data.summary, active: data.active, totalSpawned: data.total_spawned, todos };
      }
    } catch (err) {
      console.error('Agent status error:', err);
    }
    return { summary: 'Error', active: 0, totalSpawned: 0, todos: [] };
  },

  // ====================================================================
  // Phase 5: History & Replay API methods
  // ====================================================================
  
  getHistory: async (opts?: { agentId?: string; eventType?: string; limit?: number }) => {
    try {
      let url = '/api/agents/history';
      const params = new URLSearchParams();
      if (opts?.agentId) params.set('agent_id', opts.agentId);
      if (opts?.eventType) params.set('event_type', opts.eventType);
      if (opts?.limit) params.set('limit', opts.limit.toString());
      if (params.toString()) url += '?' + params.toString();
      
      const res = await fetch(url);
      if (res.ok) {
        const data = await res.json();
        const events: HistoryEvent[] = data.events || [];
        set({ historyEvents: events, historyStats: data.stats || null });
        return events;
      }
    } catch (err) {
      console.error('History error:', err);
    }
    return [];
  },

  replayAgent: async (agentId: string, dryRun = false) => {
    try {
      const res = await fetch('/api/agents/replay', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ agent_id: agentId, dry_run: dryRun })
      });
      if (res.ok) {
        const data = await res.json();
        set(state => ({ logs: [...state.logs, `[Replay] Agent ${agentId}: ${data.events_replayed} events in ${data.duration_ms}ms`] }));
        return { success: data.success, result: data.result, eventsReplayed: data.events_replayed, durationMs: data.duration_ms };
      }
    } catch (err) {
      console.error('Replay error:', err);
    }
    return { success: false, result: 'Error', eventsReplayed: 0, durationMs: 0 };
  },

  getHistoryStats: async () => {
    try {
      const res = await fetch('/api/agents/history?limit=0');
      if (res.ok) {
        const data = await res.json();
        const stats: HistoryStats = data.stats;
        set({ historyStats: stats });
        return stats;
      }
    } catch (err) {
      console.error('Stats error:', err);
    }
    return { totalEvents: 0, sessionId: '', successCount: 0, failCount: 0, eventTypes: {} };
  },

  // ====================================================================
  // Phase 6B: Failure Visibility API methods
  // ====================================================================

  getFailures: async (opts?: { limit?: number; reason?: string }) => {
    try {
      let url = '/api/failures';
      const params = new URLSearchParams();
      if (opts?.limit) params.set('limit', opts.limit.toString());
      if (opts?.reason) params.set('reason', opts.reason);
      if (params.toString()) url += '?' + params.toString();

      const res = await fetch(url);
      if (res.ok) {
        const data = await res.json();
        const failures: FailureRecord[] = data.failures || [];
        const stats: FailureStats = data.stats || null;
        set({ failures, failureStats: stats });
        return failures;
      }
    } catch (err) {
      console.error('Failures API error:', err);
    }
    return [];
  },

  getAgentStatusDetailed: async () => {
    try {
      const res = await fetch('/api/agents/status');
      if (res.ok) {
        return await res.json();
      }
    } catch (err) {
      console.error('Agent status error:', err);
    }
    return null;
  }
}));
)";
}

std::string ReactIDEGenerator::GenerateMemoryPanel() {
    return R"(import React from 'react';
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
)";
}

std::string ReactIDEGenerator::GenerateHotPatchPanel() {
    return R"(import React from 'react';
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
)";
}

std::string ReactIDEGenerator::GenerateVSIXLoaderPanel() {
    return R"(import React, { useState } from 'react';
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
)";
}

std::string ReactIDEGenerator::GenerateAppTsx() {
  return R"(import React, { useEffect, useState } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { CodeEditor } from '@/components/CodeEditor';
import { MemoryPanel } from '@/components/MemoryPanel';
import { HotPatchPanel } from '@/components/HotPatchPanel';
import { VSIXLoaderPanel } from '@/components/VSIXLoaderPanel';
import { SubAgentPanel } from '@/components/SubAgentPanel';
import { FailurePanel } from '@/components/FailurePanel';
import { HistoryPanel } from '@/components/HistoryPanel';
import { PolicyPanel } from '@/components/PolicyPanel';
import { ExplainabilityPanel } from '@/components/ExplainabilityPanel';
import { Terminal, Bot, Clock, Shield, Eye } from 'lucide-react';

function App() {
  const { connect, isConnected, logs, agenticReady } = useEngineStore();
  const [code, setCode] = useState('// RawrXD Monaco IDE\n#include <iostream>\n\nint main() {\n    std::cout << "Hello, RawrXD" << std::endl;\n    return 0;\n}\n');
  const [status, setStatus] = useState({
    ready: false,
    model_loaded: false,
    model_path: '',
    backend: 'rawrxd',
    agentic: false,
    subagents: false,
    capabilities: { completion: true, streaming: false }
  });
  const [rightPanel, setRightPanel] = useState<'tools' | 'agents' | 'failures' | 'history' | 'policy' | 'explain'>('tools');

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
        <button 
          onClick={() => setRightPanel(rightPanel === 'agents' ? 'tools' : 'agents')}
          className={`w-10 h-10 rounded-lg flex items-center justify-center transition-colors ${
            rightPanel === 'agents' ? 'bg-primary text-white' : 'bg-background hover:bg-background/80'
          }`}
          title="Toggle Agent Panel"
        >
          <Bot className="w-5 h-5" />
        </button>
        <button
          onClick={() => setRightPanel(rightPanel === 'failures' ? 'tools' : 'failures')}
          className={`w-10 h-10 rounded-lg flex items-center justify-center transition-colors ${
            rightPanel === 'failures' ? 'bg-red-600 text-white' : 'bg-background hover:bg-background/80'
          }`}
          title="Toggle Failure Panel"
        >
          <span className="text-xs font-bold">!</span>
        </button>
        <button
          onClick={() => setRightPanel(rightPanel === 'history' ? 'tools' : 'history')}
          className={`w-10 h-10 rounded-lg flex items-center justify-center transition-colors ${
            rightPanel === 'history' ? 'bg-green-600 text-white' : 'bg-background hover:bg-background/80'
          }`}
          title="Toggle History Panel"
        >
          <Clock className="w-4 h-4" />
        </button>
        <button
          onClick={() => setRightPanel(rightPanel === 'policy' ? 'tools' : 'policy')}
          className={`w-10 h-10 rounded-lg flex items-center justify-center transition-colors ${
            rightPanel === 'policy' ? 'bg-amber-600 text-white' : 'bg-background hover:bg-background/80'
          }`}
          title="Toggle Policy Panel"
        >
          <Shield className="w-4 h-4" />
        </button>
        <button
          onClick={() => setRightPanel(rightPanel === 'explain' ? 'tools' : 'explain')}
          className={`w-10 h-10 rounded-lg flex items-center justify-center transition-colors ${
            rightPanel === 'explain' ? 'bg-cyan-600 text-white' : 'bg-background hover:bg-background/80'
          }`}
          title="Toggle Explainability Panel"
        >
          <Eye className="w-4 h-4" />
        </button>
        <div className="flex flex-col items-center gap-1">
          {status.agentic && <div className="w-2 h-2 rounded-full bg-purple-500" title="Agentic" />}
          <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`} />
        </div>
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
            {status.agentic && (
              <span className="text-xs px-2 py-0.5 rounded bg-purple-600/20 text-purple-300">
                AGENTIC
              </span>
            )}
            {status.subagents && (
              <span className="text-xs px-2 py-0.5 rounded bg-orange-600/20 text-orange-300">
                SWARM
              </span>
            )}
          </div>
          <div className="text-xs font-mono text-muted-foreground">{status.model_path || 'v1.0.0-alpha'}</div>
        </header>

        <main className="flex-1 overflow-hidden grid grid-cols-12 auto-rows-fr">
          {/* Left Panel - Editor Area */}
          <div className="col-span-8 border-r border-border bg-[#1e1e1e] p-2">
            <CodeEditor value={code} onChange={setCode} language="cpp" />
          </div>

          {/* Right Panel - Tools or Agents */}
          <div className="col-span-4 flex flex-col bg-background">
            {rightPanel === 'agents' ? (
              <div className="flex-1 overflow-hidden">
                <SubAgentPanel />
              </div>
            ) : rightPanel === 'failures' ? (
              <div className="flex-1 overflow-hidden">
                <FailurePanel />
              </div>
            ) : rightPanel === 'history' ? (
              <div className="flex-1 overflow-hidden">
                <HistoryPanel />
              </div>
            ) : rightPanel === 'policy' ? (
              <div className="flex-1 overflow-hidden">
                <PolicyPanel />
              </div>
            ) : rightPanel === 'explain' ? (
              <div className="flex-1 overflow-hidden">
                <ExplainabilityPanel />
              </div>
            ) : (
              <div className="flex-1 overflow-y-auto border-b border-border">
                <MemoryPanel />
                <div className="h-px bg-border my-2" />
                <HotPatchPanel />
                <div className="h-px bg-border my-2" />
                <VSIXLoaderPanel />
              </div>
            )}
            
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
)";
}

std::string ReactIDEGenerator::GenerateCodeEditor() {
    // ──── RAW STRING OPEN: R"CODEEDITOR( ──── do not insert C++ inside this block ────
    return R"CODEEDITOR(import React, { useEffect, useRef } from 'react';
import Editor, { OnMount } from '@monaco-editor/react';
import * as monaco from 'monaco-editor';

type CodeEditorProps = {
  value: string;
  onChange: (value: string) => void;
  language?: string;
};

export const CodeEditor: React.FC<CodeEditorProps> = ({ value, onChange, language = 'cpp' }) => {
  const providerRef = useRef<monaco.IDisposable | null>(null);
  const abortControllerRef = useRef<AbortController | null>(null);

  const handleMount: OnMount = (editor, monacoInstance) => {
    if (providerRef.current) {
      providerRef.current.dispose();
    }

    providerRef.current = monacoInstance.languages.registerInlineCompletionsProvider(language, {
      provideInlineCompletions: async (model, position, context, token) => {
        try {
          // Cancel any in-flight stream before starting a new one
          if (abortControllerRef.current) {
            abortControllerRef.current.abort();
          }
          abortControllerRef.current = new AbortController();

          const buffer = model.getValue();
          const offset = model.getOffsetAt(position);
          let fullCompletion = '';

          // Slice context window: last 2-4k chars before cursor
          const context_window = 4096;
          const context_start = offset > context_window ? offset - context_window : 0;
          const context = buffer.substring(context_start, offset);

          const response = await fetch('/complete/stream', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              buffer: context,
              cursor_offset: context.length,
              language,
              mode: 'complete',
              max_tokens: 128,
              temperature: 0.2
            }),
            signal: abortControllerRef.current.signal
          });

          if (!response.ok || !response.body) {
            return { items: [], dispose: () => {} };
          }

          const reader = response.body.getReader();
          const decoder = new TextDecoder();
          let buffer_text = '';

          while (true) {
            // Check if stream was aborted
            if (abortControllerRef.current?.signal.aborted) {
              reader.cancel();
              break;
            }

            const { done, value } = await reader.read();
            if (done) break;
            
            buffer_text += decoder.decode(value, { stream: true });
            const lines = buffer_text.split('\n');
            buffer_text = lines.pop() || '';

            for (const line of lines) {
              if (line.startsWith('data: ')) {
                try {
                  const event = JSON.parse(line.substring(6));
                  if (event.token) {
                    fullCompletion += event.token;
                  }
                } catch (e) {
                  // Ignore parse errors
                }
              }
            }

            // Update UI incrementally as tokens arrive
            if (fullCompletion) {
              context.widget?.updateInlineCompletion({
                insertText: fullCompletion,
                range: new monacoInstance.Range(
                  position.lineNumber,
                  position.column,
                  position.lineNumber,
                  position.column
                )
              });
            }
          }

          if (!fullCompletion) {
            return { items: [], dispose: () => {} };
          }

          return {
            items: [
              {
                insertText: fullCompletion,
                range: new monacoInstance.Range(
                  position.lineNumber,
                  position.column,
                  position.lineNumber,
                  position.column
                )
              }
            ],
            dispose: () => {}
          };
        } catch (err) {
          if (err instanceof Error && err.name === 'AbortError') {
            // Stream was cancelled by new keystroke - this is expected
            return { items: [], dispose: () => {} };
          }
          console.error('Completion stream error:', err);
          return { items: [], dispose: () => {} };
        }
      },
      freeInlineCompletions: () => {}
    });
  };

  useEffect(() => {
    return () => {
      if (providerRef.current) {
        providerRef.current.dispose();
      }
    };
  }, []);

  return (
    <Editor
      height="100%"
      defaultLanguage={language}
      value={value}
      onChange={(val) => onChange(val || '')}
      onMount={handleMount}
      theme="vs-dark"
      options={{
        minimap: { enabled: true },
        fontSize: 14,
        scrollBeyondLastLine: false,
        automaticLayout: true,
        inlineSuggest: { enabled: true }
      }}
    />
  );
};
)CODEEDITOR";
    // ──── RAW STRING CLOSE: )CODEEDITOR" ──── end of CodeEditor block ────
}

std::string ReactIDEGenerator::GenerateIndexHtml() {
    return R"(<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <link rel="icon" type="image/svg+xml" href="/vite.svg" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>RawrXD React IDE</title>
  </head>
  <body class="dark">
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
)";
}

void ReactIDEGenerator::WriteFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

void ReactIDEGenerator::CreateDirectoryStructure(const std::filesystem::path& base) {
    std::filesystem::create_directories(base / "src" / "components" / "ui");
    std::filesystem::create_directories(base / "src" / "lib");
    std::filesystem::create_directories(base / "src" / "assets");
    std::filesystem::create_directories(base / "public");
}

bool ReactIDEGenerator::GenerateIDE(const std::string& name, const std::string& template_name,
                                   const std::filesystem::path& output_dir) {
    if (templates.find(template_name) == templates.end()) {
        std::cerr << "Template not found: " << template_name << std::endl;
        return false;
    }

    const auto& tmpl = templates[template_name];
    std::filesystem::path project_path = output_dir / name;
    
    std::cout << "Genering " << name << " using " << template_name << " template..." << std::endl;

    CreateDirectoryStructure(project_path);

    // Core Configs
    WriteFile(project_path / "package.json", GeneratePackageJson(tmpl));
    WriteFile(project_path / "tsconfig.json", GenerateTsConfig());
    WriteFile(project_path / "tsconfig.node.json", R"({
  "compilerOptions": {
    "composite": true,
    "skipLibCheck": true,
    "module": "ESNext",
    "moduleResolution": "bundler",
    "allowSyntheticDefaultImports": true
  },
  "include": ["vite.config.ts"]
})");
    WriteFile(project_path / "vite.config.ts", GenerateViteConfig());
    WriteFile(project_path / "tailwind.config.js", GenerateTailwindConfig());
    WriteFile(project_path / "postcss.config.js", "export default { plugins: { tailwindcss: {}, autoprefixer: {}, }, }");

    // Core Application
    WriteFile(project_path / "index.html", GenerateIndexHtml());
    WriteFile(project_path / "src" / "main.tsx", GenerateMainTsx());
    WriteFile(project_path / "src" / "App.tsx", GenerateAppTsx());
    WriteFile(project_path / "src" / "components" / "CodeEditor.tsx", GenerateCodeEditor());
    WriteFile(project_path / "src" / "index.css", R"(@tailwind base;
@tailwind components;
@tailwind utilities;

:root {
  --background: 240 10% 3.9%;
  --foreground: 0 0% 98%;
  --card: 240 10% 3.9%;
  --card-foreground: 0 0% 98%;
  --popover: 240 10% 3.9%;
  --popover-foreground: 0 0% 98%;
  --primary: 267 100% 60%; /* RawrXD Purple */
  --primary-foreground: 0 0% 98%;
  --secondary: 240 3.7% 15.9%;
  --secondary-foreground: 0 0% 98%;
  --muted: 240 3.7% 15.9%;
  --muted-foreground: 240 5% 64.9%;
  --accent: 267 100% 60%;
  --accent-foreground: 0 0% 98%;
  --destructive: 0 62.8% 30.6%;
  --destructive-foreground: 0 0% 98%;
  --border: 240 3.7% 15.9%;
  --input: 240 3.7% 15.9%;
  --ring: 240 4.9% 83.9%;
}

@layer base {
  * {
    @apply border-border;
  }
  body {
    @apply bg-background text-foreground;
  }
}
)");

    // Engine Components
    WriteFile(project_path / "src" / "lib" / "engine-bridge.ts", GenerateEngineBridge());
    WriteFile(project_path / "src" / "components" / "MemoryPanel.tsx", GenerateMemoryPanel());
    WriteFile(project_path / "src" / "components" / "HotPatchPanel.tsx", GenerateHotPatchPanel());
    WriteFile(project_path / "src" / "components" / "VSIXLoaderPanel.tsx", GenerateVSIXLoaderPanel());
    WriteFile(project_path / "src" / "components" / "SubAgentPanel.tsx", GenerateSubAgentPanel());
    WriteFile(project_path / "src" / "components" / "HistoryPanel.tsx", GenerateHistoryPanel());
    WriteFile(project_path / "src" / "components" / "FailurePanel.tsx", GenerateFailurePanel());
    WriteFile(project_path / "src" / "components" / "ExplainabilityPanel.tsx", GenerateExplainabilityPanel());
    WriteFile(project_path / "src" / "components" / "PolicyPanel.tsx", GeneratePolicyPanel());
    WriteFile(project_path / "src" / "components" / "SettingsPanel.tsx", GenerateSettingsPanel());
    WriteFile(project_path / "src" / "components" / "BackendPanel.tsx", GenerateBackendPanel());

    // UI Components (Minimal shadcn abstraction)
    WriteFile(project_path / "src" / "components" / "ui" / "card.tsx", R"(import * as React from "react"
import { cn } from "@/lib/utils"

const Card = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(({ className, ...props }, ref) => (
  <div ref={ref} className={cn("rounded-xl border bg-card text-card-foreground shadow", className)} {...props} />
))
Card.displayName = "Card"

const CardHeader = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(({ className, ...props }, ref) => (
  <div ref={ref} className={cn("flex flex-col space-y-1.5 p-6", className)} {...props} />
))
CardHeader.displayName = "CardHeader"

const CardTitle = React.forwardRef<HTMLParagraphElement, React.HTMLAttributes<HTMLHeadingElement>>(({ className, ...props }, ref) => (
  <h3 ref={ref} className={cn("font-semibold leading-none tracking-tight", className)} {...props} />
))
CardTitle.displayName = "CardTitle"

const CardContent = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(({ className, ...props }, ref) => (
  <div ref={ref} className={cn("p-6 pt-0", className)} {...props} />
))
CardContent.displayName = "CardContent"

export { Card, CardHeader, CardTitle, CardContent }
)");
    
    // Utils
    WriteFile(project_path / "src" / "lib" / "utils.ts", R"(import { type ClassValue, clsx } from "clsx"
import { twMerge } from "tailwind-merge"
 
export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}
)");

    std::cout << "Project generated successfully at: " << project_path.string() << std::endl;
    std::cout << "Run 'npm install' and 'npm run dev' to start." << std::endl;

    return true;
}

bool ReactIDEGenerator::GenerateMinimalIDE(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateIDE(name, "minimal", output_dir);
}

bool ReactIDEGenerator::GenerateFullIDE(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateIDE(name, "full", output_dir);
}

bool ReactIDEGenerator::GenerateAgenticIDE(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateIDE(name, "agentic", output_dir);
}

std::vector<std::string> ReactIDEGenerator::ListTemplates() const {
    std::vector<std::string> list;
    for (const auto& [name, tmpl] : templates) {
        list.push_back(name + " - " + tmpl.description);
    }
    return list;
}

std::vector<std::string> ReactIDEGenerator::GetTemplateFeatures(const std::string& template_name) const {
    if (templates.find(template_name) != templates.end()) {
        return templates.at(template_name).features;
    }
    return {};
}

// Stubs for language specific IDEs
bool ReactIDEGenerator::GenerateCppIDE(const std::string& name, const std::filesystem::path& output_dir) {
    // TODO: Add C++ specific monaco config
    return GenerateFullIDE(name, output_dir);
}

bool ReactIDEGenerator::GenerateRustIDE(const std::string& name, const std::filesystem::path& output_dir) {
    // TODO: Add Rust specific monaco config
    return GenerateFullIDE(name, output_dir);
}

bool ReactIDEGenerator::GeneratePythonIDE(const std::string& name, const std::filesystem::path& output_dir) {
    // TODO: Add Python specific monaco config
    return GenerateFullIDE(name, output_dir);
}

bool ReactIDEGenerator::GenerateMultiLanguageIDE(const std::string& name, const std::filesystem::path& output_dir) {
    return GenerateFullIDE(name, output_dir);
}

// Stubs for advanced features
bool ReactIDEGenerator::GenerateIDEWithTests(const std::string& name, const std::string& template_name, const std::filesystem::path& output_dir) {
    return GenerateIDE(name, template_name, output_dir);
}

bool ReactIDEGenerator::GenerateIDEWithCI(const std::string& name, const std::string& template_name, const std::filesystem::path& output_dir) {
    return GenerateIDE(name, template_name, output_dir);
}

bool ReactIDEGenerator::GenerateIDEWithDocker(const std::string& name, const std::string& template_name, const std::filesystem::path& output_dir) {
    return GenerateIDE(name, template_name, output_dir);
}

// Helper methods implementations matching the header
std::string ReactIDEGenerator::GenerateModelLoaderPanel() {
    return R"(import React, { useState } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Database, Download, Play, Trash2 } from 'lucide-react';

export const ModelLoaderPanel: React.FC = () => {
  const { loadedModels, executeCommand } = useEngineStore();
  const [modelPath, setModelPath] = useState('');

  const handleLoad = async () => {
    if (modelPath) {
      await executeCommand(`!model load ${modelPath}`);
      setModelPath('');
    }
  };

  return (
    <div className="p-4 space-y-4">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Database className="w-6 h-6 text-purple-500" /> Model Inference
      </h2>

      <div className="flex gap-2">
        <input 
          type="text" 
          value={modelPath}
          onChange={(e) => setModelPath(e.target.value)}
          placeholder="Path to .gguf model..."
          className="flex-1 bg-secondary text-secondary-foreground px-3 py-2 rounded text-sm border border-border focus:outline-none focus:border-primary"
        />
        <button 
          onClick={handleLoad}
          className="bg-primary text-white px-4 py-2 rounded hover:bg-primary/90 flex items-center gap-2"
        >
          <Download className="w-4 h-4" /> Load
        </button>
      </div>

      <div className="space-y-2">
        <h3 className="text-sm font-semibold">Loaded Models</h3>
        {loadedModels.length === 0 ? (
          <div className="text-sm text-muted-foreground italic text-center p-4 border border-dashed rounded">
            No models loaded in memory
          </div>
        ) : (
          <div className="space-y-2">
            {loadedModels.map((model, idx) => (
              <div key={idx} className="flex items-center justify-between p-3 bg-secondary rounded-lg border border-border">
                <div className="flex items-center gap-3">
                  <div className="w-2 h-2 rounded-full bg-green-500 animate-pulse" />
                  <span className="font-mono text-sm truncate max-w-[200px]">{model}</span>
                </div>
                <div className="flex gap-2">
                   <button className="p-1.5 hover:bg-red-500/20 text-red-500 rounded"><Trash2 className="w-4 h-4" /></button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};
)";
}

std::string ReactIDEGenerator::GenerateChatInterface() {
    return R"(import React, { useState, useRef, useEffect } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { Send, Bot, User } from 'lucide-react';
import ReactMarkdown from 'react-markdown';

interface Message {
  role: 'user' | 'assistant';
  content: string;
}

export const ChatInterface: React.FC = () => {
  const { executeCommand } = useEngineStore();
  const [input, setInput] = useState('');
  const [messages, setMessages] = useState<Message[]>([
    { role: 'assistant', content: 'Hello! I am your AI coding assistant. How can I help you today?' }
  ]);
  const messagesEndRef = useRef<HTMLDivElement>(null);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: "smooth" });
  };

  useEffect(scrollToBottom, [messages]);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!input.trim()) return;

    const userMsg = input;
    setInput('');
    setMessages(prev => [...prev, { role: 'user', content: userMsg }]);

    // In a real implementation this would stream
    const response = await executeCommand(`!chat ${userMsg}`);
    
    setMessages(prev => [...prev, { role: 'assistant', content: response }]);
  };

  return (
    <div className="flex flex-col h-full bg-background">
      <div className="flex-1 overflow-y-auto p-4 space-y-4">
        {messages.map((msg, idx) => (
          <div key={idx} className={`flex gap-3 ${msg.role === 'user' ? 'flex-row-reverse' : ''}`}>
            <div className={`w-8 h-8 rounded-full flex items-center justify-center flex-none ${
              msg.role === 'assistant' ? 'bg-primary' : 'bg-secondary'
            }`}>
              {msg.role === 'assistant' ? <Bot className="w-5 h-5" /> : <User className="w-5 h-5" />}
            </div>
            <div className={`rounded-lg p-3 max-w-[80%] text-sm ${
              msg.role === 'assistant' ? 'bg-secondary' : 'bg-primary text-primary-foreground'
            }`}>
              <ReactMarkdown>{msg.content}</ReactMarkdown>
            </div>
          </div>
        ))}
        <div ref={messagesEndRef} />
      </div>

      <form onSubmit={handleSubmit} className="p-4 border-t border-border bg-card">
        <div className="flex gap-2">
          <input
            value={input}
            onChange={(e) => setInput(e.target.value)}
            placeholder="Ask anything..."
            className="flex-1 bg-secondary text-secondary-foreground rounded-lg px-4 py-2 text-sm focus:outline-none focus:ring-1 focus:ring-primary"
          />
          <button 
            type="submit" 
            disabled={!input.trim()}
            className="bg-primary text-primary-foreground p-2 rounded-lg hover:bg-primary/90 disabled:opacity-50"
          >
            <Send className="w-5 h-5" />
          </button>
        </div>
      </form>
    </div>
  );
};
)";
}

std::string ReactIDEGenerator::GenerateSubAgentPanel() {
    // ──── RAW STRING OPEN: R"SUBAGENT( ──── do not insert C++ inside this block ────
    return R"SUBAGENT(import React, { useState, useEffect } from 'react';
import { useEngineStore, AgentInfo, TodoItem } from '@/lib/engine-bridge';
import { Bot, GitBranch, Hexagon, ListTodo, Play, RefreshCw, Loader2, CheckCircle2, XCircle, Clock } from 'lucide-react';

type Tab = 'subagent' | 'chain' | 'swarm' | 'agents' | 'todos';

export const SubAgentPanel: React.FC = () => {
  const { spawnSubAgent, executeChain, executeSwarm, listAgents, getAgentStatus, agents, todos, agenticReady } = useEngineStore();
  const [activeTab, setActiveTab] = useState<Tab>('subagent');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState('');

  // SubAgent form
  const [saDesc, setSaDesc] = useState('');
  const [saPrompt, setSaPrompt] = useState('');
  const [saTimeout, setSaTimeout] = useState(30);

  // Chain form
  const [chainSteps, setChainSteps] = useState('');
  const [chainInput, setChainInput] = useState('');

  // Swarm form
  const [swarmPrompts, setSwarmPrompts] = useState('');
  const [swarmStrategy, setSwarmStrategy] = useState('concatenate');
  const [swarmParallel, setSwarmParallel] = useState(4);

  useEffect(() => {
    if (agenticReady) {
      listAgents();
      getAgentStatus();
    }
  }, [agenticReady]);

  const handleSpawnSubAgent = async () => {
    if (!saDesc.trim() || !saPrompt.trim()) return;
    setLoading(true);
    setResult('');
    const res = await spawnSubAgent(saDesc.trim(), saPrompt.trim(), saTimeout);
    setResult(res.success ? `✅ Agent ${res.agentId}:\n${res.result}` : `❌ Failed: ${res.result}`);
    setLoading(false);
  };

  const handleChain = async () => {
    const steps = chainSteps.split('|').map(s => s.trim()).filter(Boolean);
    if (steps.length === 0) return;
    setLoading(true);
    setResult('');
    const res = await executeChain(steps, chainInput.trim());
    setResult(`Chain completed (${res.stepCount} steps):\n${res.result}`);
    setLoading(false);
  };

  const handleSwarm = async () => {
    const prompts = swarmPrompts.split('|').map(s => s.trim()).filter(Boolean);
    if (prompts.length === 0) return;
    setLoading(true);
    setResult('');
    const res = await executeSwarm(prompts, swarmStrategy, swarmParallel);
    setResult(`Swarm complete (${res.taskCount} tasks, ${res.strategy}):\n${res.result}`);
    setLoading(false);
  };

  const handleRefresh = async () => {
    await listAgents();
    await getAgentStatus();
  };

  const statusIcon = (status: string) => {
    switch (status) {
      case 'completed': return <CheckCircle2 className="w-4 h-4 text-green-500" />;
      case 'failed': return <XCircle className="w-4 h-4 text-red-500" />;
      case 'in-progress': return <Loader2 className="w-4 h-4 text-blue-400 animate-spin" />;
      default: return <Clock className="w-4 h-4 text-muted-foreground" />;
    }
  };

  const tabs: { key: Tab; label: string; icon: React.ReactNode }[] = [
    { key: 'subagent', label: 'SubAgent', icon: <Bot className="w-4 h-4" /> },
    { key: 'chain', label: 'Chain', icon: <GitBranch className="w-4 h-4" /> },
    { key: 'swarm', label: 'Swarm', icon: <Hexagon className="w-4 h-4" /> },
    { key: 'agents', label: 'Agents', icon: <Bot className="w-4 h-4" /> },
    { key: 'todos', label: 'Todos', icon: <ListTodo className="w-4 h-4" /> },
  ];

  if (!agenticReady) {
    return (
      <div className="p-4 text-center text-muted-foreground">
        <Bot className="w-10 h-10 mx-auto mb-3 opacity-40" />
        <p className="text-sm">Agentic subsystem not available.</p>
        <p className="text-xs mt-1">Start RawrEngine with a loaded model to enable SubAgents.</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full">
      {/* Tab bar */}
      <div className="flex border-b border-border bg-card">
        {tabs.map(tab => (
          <button
            key={tab.key}
            onClick={() => { setActiveTab(tab.key); setResult(''); }}
            className={`flex items-center gap-1.5 px-3 py-2 text-xs font-medium transition-colors border-b-2 ${
              activeTab === tab.key
                ? 'border-primary text-primary'
                : 'border-transparent text-muted-foreground hover:text-foreground'
            }`}
          >
            {tab.icon} {tab.label}
          </button>
        ))}
        <div className="flex-1" />
        <button onClick={handleRefresh} className="p-2 text-muted-foreground hover:text-foreground">
          <RefreshCw className="w-3.5 h-3.5" />
        </button>
      </div>

      <div className="flex-1 overflow-y-auto p-4 space-y-4">
        {/* SubAgent tab */}
        {activeTab === 'subagent' && (
          <div className="space-y-3">
            <h3 className="text-sm font-semibold flex items-center gap-2">
              <Bot className="w-4 h-4 text-primary" /> Spawn SubAgent
            </h3>
            <input
              value={saDesc}
              onChange={e => setSaDesc(e.target.value)}
              placeholder="Agent description (e.g. 'code reviewer')"
              className="w-full bg-secondary text-sm px-3 py-2 rounded border border-border focus:outline-none focus:border-primary"
            />
            <textarea
              value={saPrompt}
              onChange={e => setSaPrompt(e.target.value)}
              placeholder="Agent prompt / task..."
              rows={4}
              className="w-full bg-secondary text-sm px-3 py-2 rounded border border-border focus:outline-none focus:border-primary resize-none"
            />
            <div className="flex items-center gap-2">
              <label className="text-xs text-muted-foreground">Timeout (s):</label>
              <input
                type="number"
                value={saTimeout}
                onChange={e => setSaTimeout(Number(e.target.value))}
                className="w-20 bg-secondary text-sm px-2 py-1 rounded border border-border"
              />
              <div className="flex-1" />
              <button
                onClick={handleSpawnSubAgent}
                disabled={loading || !saDesc.trim() || !saPrompt.trim()}
                className="bg-primary text-primary-foreground px-4 py-1.5 rounded text-sm flex items-center gap-2 hover:bg-primary/90 disabled:opacity-50"
              >
                {loading ? <Loader2 className="w-4 h-4 animate-spin" /> : <Play className="w-4 h-4" />}
                Spawn
              </button>
            </div>
          </div>
        )}

        {/* Chain tab */}
        {activeTab === 'chain' && (
          <div className="space-y-3">
            <h3 className="text-sm font-semibold flex items-center gap-2">
              <GitBranch className="w-4 h-4 text-green-400" /> Execute Chain
            </h3>
            <p className="text-xs text-muted-foreground">
              Define sequential steps separated by <code className="bg-secondary px-1 rounded">|</code> pipes.
              Each step's output feeds into the next.
            </p>
            <textarea
              value={chainSteps}
              onChange={e => setChainSteps(e.target.value)}
              placeholder="Step 1 description | Step 2 description | Step 3 description"
              rows={3}
              className="w-full bg-secondary text-sm px-3 py-2 rounded border border-border focus:outline-none focus:border-primary resize-none font-mono"
            />
            <input
              value={chainInput}
              onChange={e => setChainInput(e.target.value)}
              placeholder="Initial input (optional)..."
              className="w-full bg-secondary text-sm px-3 py-2 rounded border border-border focus:outline-none focus:border-primary"
            />
            <div className="flex justify-end">
              <button
                onClick={handleChain}
                disabled={loading || !chainSteps.trim()}
                className="bg-green-600 text-white px-4 py-1.5 rounded text-sm flex items-center gap-2 hover:bg-green-700 disabled:opacity-50"
              >
                {loading ? <Loader2 className="w-4 h-4 animate-spin" /> : <GitBranch className="w-4 h-4" />}
                Run Chain
              </button>
            </div>
          </div>
        )}

        {/* Swarm tab */}
        {activeTab === 'swarm' && (
          <div className="space-y-3">
            <h3 className="text-sm font-semibold flex items-center gap-2">
              <Hexagon className="w-4 h-4 text-orange-400" /> HexMag Swarm
            </h3>
            <p className="text-xs text-muted-foreground">
              Launch parallel agents. Separate prompts with <code className="bg-secondary px-1 rounded">|</code> pipes.
            </p>
            <textarea
              value={swarmPrompts}
              onChange={e => setSwarmPrompts(e.target.value)}
              placeholder="Prompt 1 | Prompt 2 | Prompt 3 ..."
              rows={3}
              className="w-full bg-secondary text-sm px-3 py-2 rounded border border-border focus:outline-none focus:border-primary resize-none font-mono"
            />
            <div className="flex items-center gap-3 flex-wrap">
              <label className="text-xs text-muted-foreground">Strategy:</label>
              <select
                value={swarmStrategy}
                onChange={e => setSwarmStrategy(e.target.value)}
                className="bg-secondary text-sm px-2 py-1 rounded border border-border"
              >
                <option value="concatenate">Concatenate</option>
                <option value="vote">Majority Vote</option>
                <option value="summarize">Summarize</option>
              </select>
              <label className="text-xs text-muted-foreground">Parallel:</label>
              <input
                type="number"
                value={swarmParallel}
                onChange={e => setSwarmParallel(Number(e.target.value))}
                min={1}
                max={16}
                className="w-16 bg-secondary text-sm px-2 py-1 rounded border border-border"
              />
              <div className="flex-1" />
              <button
                onClick={handleSwarm}
                disabled={loading || !swarmPrompts.trim()}
                className="bg-orange-600 text-white px-4 py-1.5 rounded text-sm flex items-center gap-2 hover:bg-orange-700 disabled:opacity-50"
              >
                {loading ? <Loader2 className="w-4 h-4 animate-spin" /> : <Hexagon className="w-4 h-4" />}
                Launch Swarm
              </button>
            </div>
          </div>
        )}

        {/* Agents list tab */}
        {activeTab === 'agents' && (
          <div className="space-y-3">
            <h3 className="text-sm font-semibold">Spawned Agents ({agents.length})</h3>
            {agents.length === 0 ? (
              <div className="text-sm text-muted-foreground italic text-center p-6 border border-dashed rounded">
                No agents spawned yet. Use the SubAgent tab to spawn one.
              </div>
            ) : (
              <div className="space-y-2">
                {agents.map((agent, idx) => (
                  <div key={idx} className="p-3 bg-secondary rounded-lg border border-border">
                    <div className="flex items-center gap-2 mb-1">
                      {statusIcon(agent.status)}
                      <span className="font-mono text-xs text-primary">{agent.id}</span>
                      <span className={`text-xs px-1.5 py-0.5 rounded ${
                        agent.status === 'completed' ? 'bg-green-600/20 text-green-300' :
                        agent.status === 'failed' ? 'bg-red-600/20 text-red-300' : 'bg-blue-600/20 text-blue-300'
                      }`}>{agent.status}</span>
                    </div>
                    <div className="text-xs text-muted-foreground">{agent.description}</div>
                    {agent.result && (
                      <div className="mt-2 text-xs bg-background p-2 rounded max-h-24 overflow-y-auto font-mono whitespace-pre-wrap">
                        {agent.result.substring(0, 500)}
                      </div>
                    )}
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {/* Todos tab */}
        {activeTab === 'todos' && (
          <div className="space-y-3">
            <h3 className="text-sm font-semibold flex items-center gap-2">
              <ListTodo className="w-4 h-4" /> Agent Todo List ({todos.length})
            </h3>
            {todos.length === 0 ? (
              <div className="text-sm text-muted-foreground italic text-center p-6 border border-dashed rounded">
                No todos tracked. Agents create todos during autonomous work.
              </div>
            ) : (
              <div className="space-y-2">
                {todos.map(todo => (
                  <div key={todo.id} className="flex items-start gap-3 p-3 bg-secondary rounded-lg border border-border">
                    {statusIcon(todo.status)}
                    <div className="flex-1 min-w-0">
                      <div className="text-sm font-medium">{todo.title}</div>
                      <div className="text-xs text-muted-foreground mt-0.5">{todo.description}</div>
                    </div>
                    <span className={`text-xs px-1.5 py-0.5 rounded flex-none ${
                      todo.status === 'completed' ? 'bg-green-600/20 text-green-300' :
                      todo.status === 'failed' ? 'bg-red-600/20 text-red-300' :
                      todo.status === 'in-progress' ? 'bg-blue-600/20 text-blue-300' :
                      'bg-gray-600/20 text-gray-300'
                    }`}>{todo.status}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {/* Result output */}
        {result && (
          <div className="mt-4 p-3 bg-black/40 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground mb-1 font-semibold">Result</div>
            <pre className="text-xs text-green-400 font-mono whitespace-pre-wrap max-h-48 overflow-y-auto">
              {result}
            </pre>
          </div>
        )}
      </div>
    </div>
  );
};
)SUBAGENT";
    // ──── RAW STRING CLOSE: )SUBAGENT" ──── end of SubAgentPanel block ────
}

std::string ReactIDEGenerator::GenerateHistoryPanel() {
    // ──── RAW STRING OPEN: R"HISTORY( ──── do not insert C++ inside this block ────
    return R"HISTORY(import React, { useState, useEffect } from 'react';
import { useEngineStore, HistoryEvent, HistoryStats } from '@/lib/engine-bridge';
import { Clock, RotateCcw, Filter, BarChart3, CheckCircle2, XCircle, Activity } from 'lucide-react';

const eventTypeColors: Record<string, string> = {
  agent_spawn: 'text-blue-400',
  agent_complete: 'text-green-400',
  agent_fail: 'text-red-400',
  agent_cancel: 'text-yellow-400',
  tool_invoke: 'text-purple-400',
  tool_result: 'text-purple-300',
  chain_start: 'text-cyan-400',
  chain_step: 'text-cyan-300',
  chain_complete: 'text-cyan-200',
  swarm_start: 'text-orange-400',
  swarm_task: 'text-orange-300',
  swarm_complete: 'text-orange-200',
  chat_request: 'text-gray-400',
  chat_response: 'text-gray-300',
  todo_update: 'text-yellow-300',
};

export const HistoryPanel: React.FC = () => {
  const { historyEvents, historyStats, getHistory, replayAgent, getHistoryStats } = useEngineStore();
  const [filter, setFilter] = useState('');
  const [typeFilter, setTypeFilter] = useState('');
  const [limit, setLimit] = useState(50);
  const [loading, setLoading] = useState(false);
  const [replayResult, setReplayResult] = useState('');

  useEffect(() => {
    getHistory({ limit });
    getHistoryStats();
  }, []);

  const handleRefresh = async () => {
    setLoading(true);
    await getHistory({ agentId: filter || undefined, eventType: typeFilter || undefined, limit });
    await getHistoryStats();
    setLoading(false);
  };

  const handleReplay = async (agentId: string) => {
    setLoading(true);
    const result = await replayAgent(agentId);
    setReplayResult(result.success 
      ? `✅ Replayed ${result.eventsReplayed} events in ${result.durationMs}ms\n${result.result}`
      : `❌ ${result.result}`
    );
    setLoading(false);
  };

  const filteredEvents = historyEvents.filter(e => {
    if (filter && !e.agentId.includes(filter) && !e.description.includes(filter)) return false;
    if (typeFilter && e.eventType !== typeFilter) return false;
    return true;
  });

  const eventTypes = [...new Set(historyEvents.map(e => e.eventType))];

  return (
    <div className="p-4 space-y-4 h-full overflow-auto">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Clock className="w-6 h-6" /> Agent History & Timeline
      </h2>

      {/* Stats bar */}
      {historyStats && (
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground">Total Events</div>
            <div className="text-xl font-mono">{historyStats.totalEvents}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground flex items-center gap-1">
              <CheckCircle2 className="w-3 h-3 text-green-400" /> Success
            </div>
            <div className="text-xl font-mono text-green-400">{historyStats.successCount}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground flex items-center gap-1">
              <XCircle className="w-3 h-3 text-red-400" /> Failed
            </div>
            <div className="text-xl font-mono text-red-400">{historyStats.failCount}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground">Session</div>
            <div className="text-xs font-mono truncate">{historyStats.sessionId}</div>
          </div>
        </div>
      )}

      {/* Filters */}
      <div className="flex items-center gap-3 flex-wrap">
        <div className="flex items-center gap-1">
          <Filter className="w-4 h-4 text-muted-foreground" />
          <input
            value={filter}
            onChange={e => setFilter(e.target.value)}
            placeholder="Filter by agent ID or description..."
            className="bg-secondary text-sm px-3 py-1.5 rounded border border-border w-64 focus:outline-none focus:border-primary"
          />
        </div>
        <select
          value={typeFilter}
          onChange={e => setTypeFilter(e.target.value)}
          className="bg-secondary text-sm px-2 py-1.5 rounded border border-border"
        >
          <option value="">All Types</option>
          {eventTypes.map(t => <option key={t} value={t}>{t}</option>)}
        </select>
        <input
          type="number"
          value={limit}
          onChange={e => setLimit(Number(e.target.value))}
          min={1}
          max={1000}
          className="w-20 bg-secondary text-sm px-2 py-1.5 rounded border border-border"
        />
        <button
          onClick={handleRefresh}
          disabled={loading}
          className="bg-primary text-primary-foreground px-3 py-1.5 rounded text-sm flex items-center gap-1 hover:opacity-90 disabled:opacity-50"
        >
          <RotateCcw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
          Refresh
        </button>
      </div>

      {/* Timeline */}
      <div className="border border-border rounded-lg overflow-hidden">
        <div className="bg-secondary px-3 py-2 text-xs font-semibold flex items-center gap-2">
          <Activity className="w-3 h-3" />
          Timeline — {filteredEvents.length} events
        </div>
        <div className="max-h-96 overflow-y-auto divide-y divide-border">
          {filteredEvents.length === 0 ? (
            <div className="p-4 text-sm text-muted-foreground text-center">No events recorded.</div>
          ) : (
            filteredEvents.map(event => (
              <div key={event.id} className="px-3 py-2 hover:bg-secondary/50 text-sm">
                <div className="flex items-center gap-2">
                  <span className={`font-mono text-xs ${event.success ? 'text-green-400' : 'text-red-400'}`}>
                    #{event.id}
                  </span>
                  <span className={`font-mono text-xs px-1.5 py-0.5 rounded bg-secondary ${eventTypeColors[event.eventType] || 'text-gray-400'}`}>
                    {event.eventType}
                  </span>
                  <span className="flex-1 truncate">{event.description}</span>
                  {event.durationMs > 0 && (
                    <span className="text-xs text-muted-foreground">{event.durationMs}ms</span>
                  )}
                  {event.agentId && event.eventType.startsWith('agent_') && (
                    <button
                      onClick={() => handleReplay(event.agentId)}
                      disabled={loading}
                      className="text-xs bg-secondary px-2 py-0.5 rounded hover:bg-primary hover:text-primary-foreground transition-colors"
                      title="Replay this agent"
                    >
                      <RotateCcw className="w-3 h-3 inline" /> Replay
                    </button>
                  )}
                </div>
                {event.agentId && (
                  <div className="text-xs text-muted-foreground mt-0.5 font-mono">
                    agent={event.agentId}
                    {event.parentId ? ` parent=${event.parentId}` : ''}
                  </div>
                )}
                {event.errorMessage && (
                  <div className="text-xs text-red-400 mt-0.5">{event.errorMessage}</div>
                )}
              </div>
            ))
          )}
        </div>
      </div>

      {/* Replay result */}
      {replayResult && (
        <div className="bg-card p-3 rounded-lg border border-border">
          <h4 className="text-sm font-semibold mb-1">Replay Result</h4>
          <pre className="text-xs font-mono whitespace-pre-wrap text-muted-foreground max-h-48 overflow-auto">
            {replayResult}
          </pre>
        </div>
      )}
    </div>
  );
};
)HISTORY";
    // ──── RAW STRING CLOSE: )HISTORY" ──── end of HistoryPanel block ────
}

std::string ReactIDEGenerator::GeneratePolicyPanel() {
    // ──── RAW STRING OPEN: R"POLICY( ──── do not insert C++ inside this block ────
    return R"POLICY(import React, { useState, useEffect, useCallback } from 'react';
import { Shield, Lightbulb, Check, X, Download, Upload, BarChart3, RefreshCw, ChevronDown, ChevronRight } from 'lucide-react';

interface PolicyTrigger {
  eventType: string;
  failureReason: string;
  taskPattern: string;
  toolName: string;
  failureRateAbove: number;
  minOccurrences: number;
}

interface PolicyAction {
  maxRetries: number;
  retryDelayMs: number;
  preferChainOverSwarm: boolean;
  reduceParallelism: number;
  timeoutOverrideMs: number;
  confidenceThreshold: number;
  addValidationStep: boolean;
  validationPrompt: string;
  customAction: string;
}

interface AgentPolicy {
  id: string;
  name: string;
  description: string;
  version: number;
  trigger: PolicyTrigger;
  action: PolicyAction;
  enabled: boolean;
  requiresUserApproval: boolean;
  priority: number;
  createdAt: number;
  modifiedAt: number;
  createdBy: string;
  appliedCount: number;
}

interface PolicySuggestion {
  id: string;
  proposedPolicy: AgentPolicy;
  rationale: string;
  estimatedImprovement: number;
  supportingEvents: number;
  affectedEventTypes: string[];
  affectedAgentIds: string[];
  state: 'pending' | 'accepted' | 'rejected' | 'expired';
  generatedAt: number;
  decidedAt: number;
}

interface PolicyHeuristic {
  key: string;
  totalEvents: number;
  successCount: number;
  failCount: number;
  successRate: number;
  avgDurationMs: number;
  p95DurationMs: number;
  topFailureReasons: string[];
}

const BASE_URL = 'http://localhost:8080';

export const PolicyPanel: React.FC = () => {
  const [policies, setPolicies] = useState<AgentPolicy[]>([]);
  const [suggestions, setSuggestions] = useState<PolicySuggestion[]>([]);
  const [heuristics, setHeuristics] = useState<PolicyHeuristic[]>([]);
  const [activeTab, setActiveTab] = useState<'policies' | 'suggestions' | 'heuristics'>('suggestions');
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState('');
  const [expandedPolicy, setExpandedPolicy] = useState<string | null>(null);

  const fetchPolicies = useCallback(async () => {
    try {
      const res = await fetch(`${BASE_URL}/api/policies`);
      const data = await res.json();
      setPolicies(data.policies || []);
    } catch (e) { console.error('Failed to fetch policies', e); }
  }, []);

  const fetchSuggestions = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch(`${BASE_URL}/api/policies/suggestions`);
      const data = await res.json();
      setSuggestions(data.suggestions || []);
    } catch (e) { console.error('Failed to fetch suggestions', e); }
    setLoading(false);
  }, []);

  const fetchHeuristics = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch(`${BASE_URL}/api/policies/heuristics`);
      const data = await res.json();
      setHeuristics(data.heuristics || []);
    } catch (e) { console.error('Failed to fetch heuristics', e); }
    setLoading(false);
  }, []);

  useEffect(() => {
    fetchPolicies();
    fetchSuggestions();
  }, [fetchPolicies, fetchSuggestions]);

  const handleAccept = async (suggestionId: string) => {
    try {
      const res = await fetch(`${BASE_URL}/api/policies/apply`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ suggestion_id: suggestionId })
      });
      const data = await res.json();
      if (data.success) {
        setMessage('Suggestion accepted — policy activated!');
        fetchPolicies();
        fetchSuggestions();
      } else {
        setMessage('Failed to accept suggestion.');
      }
    } catch (e) { setMessage('Error accepting suggestion.'); }
    setTimeout(() => setMessage(''), 3000);
  };

  const handleReject = async (suggestionId: string) => {
    try {
      const res = await fetch(`${BASE_URL}/api/policies/reject`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ suggestion_id: suggestionId })
      });
      const data = await res.json();
      if (data.success) {
        setMessage('Suggestion rejected.');
        fetchSuggestions();
      }
    } catch (e) { setMessage('Error rejecting suggestion.'); }
    setTimeout(() => setMessage(''), 3000);
  };

  const handleExport = async () => {
    try {
      const res = await fetch(`${BASE_URL}/api/policies/export`);
      const data = await res.text();
      const blob = new Blob([data], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'rawrxd-policies.json';
      a.click();
      URL.revokeObjectURL(url);
      setMessage('Policies exported!');
    } catch (e) { setMessage('Export failed.'); }
    setTimeout(() => setMessage(''), 3000);
  };

  const handleImport = async () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = async (e) => {
      const file = (e.target as HTMLInputElement).files?.[0];
      if (!file) return;
      const text = await file.text();
      try {
        const res = await fetch(`${BASE_URL}/api/policies/import`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: text
        });
        const data = await res.json();
        setMessage(`Imported ${data.imported} policies!`);
        fetchPolicies();
      } catch (e) { setMessage('Import failed.'); }
      setTimeout(() => setMessage(''), 3000);
    };
    input.click();
  };

  const improvementColor = (val: number) => {
    if (val >= 0.2) return 'text-green-400';
    if (val >= 0.1) return 'text-yellow-400';
    return 'text-gray-400';
  };

  const successRateColor = (rate: number) => {
    if (rate >= 0.9) return 'text-green-400';
    if (rate >= 0.7) return 'text-yellow-400';
    return 'text-red-400';
  };

  return (
    <div className="flex flex-col h-full bg-background text-foreground p-3 gap-3 overflow-y-auto">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Shield className="w-4 h-4 text-blue-400" />
          <span className="font-semibold text-sm">Policy Engine</span>
          <span className="text-xs text-muted-foreground">Phase 7</span>
        </div>
        <div className="flex gap-1">
          <button onClick={handleExport} className="p-1.5 rounded hover:bg-secondary" title="Export policies">
            <Download className="w-3.5 h-3.5" />
          </button>
          <button onClick={handleImport} className="p-1.5 rounded hover:bg-secondary" title="Import policies">
            <Upload className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Message bar */}
      {message && (
        <div className="text-xs bg-blue-600/20 text-blue-300 p-2 rounded">{message}</div>
      )}

      {/* Tabs */}
      <div className="flex gap-1 border-b border-border">
        {(['suggestions', 'policies', 'heuristics'] as const).map(tab => (
          <button
            key={tab}
            onClick={() => {
              setActiveTab(tab);
              if (tab === 'heuristics') fetchHeuristics();
            }}
            className={`px-3 py-1.5 text-xs font-medium border-b-2 transition-colors ${
              activeTab === tab
                ? 'border-blue-500 text-blue-400'
                : 'border-transparent text-muted-foreground hover:text-foreground'
            }`}
          >
            {tab === 'suggestions' && <Lightbulb className="w-3 h-3 inline mr-1" />}
            {tab === 'policies' && <Shield className="w-3 h-3 inline mr-1" />}
            {tab === 'heuristics' && <BarChart3 className="w-3 h-3 inline mr-1" />}
            {tab.charAt(0).toUpperCase() + tab.slice(1)}
            {tab === 'suggestions' && suggestions.length > 0 && (
              <span className="ml-1 bg-blue-600/30 text-blue-300 px-1 rounded text-[10px]">
                {suggestions.length}
              </span>
            )}
          </button>
        ))}
      </div>

      {/* Suggestions Tab */}
      {activeTab === 'suggestions' && (
        <div className="space-y-3">
          <div className="flex justify-between items-center">
            <span className="text-xs text-muted-foreground">
              {suggestions.length} pending suggestion{suggestions.length !== 1 ? 's' : ''}
            </span>
            <button
              onClick={fetchSuggestions}
              disabled={loading}
              className="text-xs flex items-center gap-1 text-muted-foreground hover:text-foreground"
            >
              <RefreshCw className={`w-3 h-3 ${loading ? 'animate-spin' : ''}`} /> Refresh
            </button>
          </div>

          {suggestions.length === 0 ? (
            <div className="text-sm text-muted-foreground italic text-center p-6 border border-dashed rounded">
              No suggestions yet. Run more agent operations to generate policy suggestions.
            </div>
          ) : (
            suggestions.map(s => (
              <div key={s.id} className="bg-card border border-border rounded-lg p-3 space-y-2">
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="text-sm font-medium">{s.proposedPolicy.name}</div>
                    <div className="text-xs text-muted-foreground mt-1">{s.rationale}</div>
                  </div>
                  <span className={`text-xs font-mono ${improvementColor(s.estimatedImprovement)}`}>
                    +{(s.estimatedImprovement * 100).toFixed(0)}%
                  </span>
                </div>

                <div className="flex items-center gap-3 text-xs text-muted-foreground">
                  <span>{s.supportingEvents} events analyzed</span>
                  <span>•</span>
                  <span>Priority: {s.proposedPolicy.priority}</span>
                  {s.affectedEventTypes.length > 0 && (
                    <>
                      <span>•</span>
                      <span>Affects: {s.affectedEventTypes.join(', ')}</span>
                    </>
                  )}
                </div>

                <div className="flex gap-2 pt-1">
                  <button
                    onClick={() => handleAccept(s.id)}
                    className="flex items-center gap-1 text-xs bg-green-600/20 text-green-300 px-3 py-1 rounded hover:bg-green-600/30 transition-colors"
                  >
                    <Check className="w-3 h-3" /> Accept
                  </button>
                  <button
                    onClick={() => handleReject(s.id)}
                    className="flex items-center gap-1 text-xs bg-red-600/20 text-red-300 px-3 py-1 rounded hover:bg-red-600/30 transition-colors"
                  >
                    <X className="w-3 h-3" /> Reject
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* Policies Tab */}
      {activeTab === 'policies' && (
        <div className="space-y-2">
          <div className="flex justify-between items-center">
            <span className="text-xs text-muted-foreground">
              {policies.length} total, {policies.filter(p => p.enabled).length} enabled
            </span>
            <button onClick={fetchPolicies} className="text-xs text-muted-foreground hover:text-foreground">
              <RefreshCw className="w-3 h-3 inline" /> Refresh
            </button>
          </div>

          {policies.length === 0 ? (
            <div className="text-sm text-muted-foreground italic text-center p-6 border border-dashed rounded">
              No policies defined. Accept suggestions or import policies.
            </div>
          ) : (
            policies.map(p => (
              <div key={p.id} className="bg-card border border-border rounded-lg overflow-hidden">
                <button
                  onClick={() => setExpandedPolicy(expandedPolicy === p.id ? null : p.id)}
                  className="w-full flex items-center gap-2 p-3 text-left hover:bg-secondary/50 transition-colors"
                >
                  {expandedPolicy === p.id
                    ? <ChevronDown className="w-3 h-3 text-muted-foreground" />
                    : <ChevronRight className="w-3 h-3 text-muted-foreground" />}
                  <span className={`w-2 h-2 rounded-full ${p.enabled ? 'bg-green-500' : 'bg-gray-500'}`} />
                  <span className="text-sm font-medium flex-1">{p.name}</span>
                  <span className="text-xs text-muted-foreground">v{p.version} • {p.appliedCount} applied</span>
                </button>
                {expandedPolicy === p.id && (
                  <div className="px-3 pb-3 space-y-2 border-t border-border pt-2">
                    <div className="text-xs text-muted-foreground">{p.description}</div>
                    <div className="grid grid-cols-2 gap-2 text-xs">
                      <div><span className="text-muted-foreground">Trigger:</span> {p.trigger.eventType || 'any'}</div>
                      <div><span className="text-muted-foreground">Priority:</span> {p.priority}</div>
                      <div><span className="text-muted-foreground">Created by:</span> {p.createdBy}</div>
                      <div><span className="text-muted-foreground">Approval:</span> {p.requiresUserApproval ? 'required' : 'auto'}</div>
                    </div>
                    {p.action.maxRetries >= 0 && (
                      <div className="text-xs">Action: retry up to {p.action.maxRetries}x (delay: {p.action.retryDelayMs}ms)</div>
                    )}
                    {p.action.addValidationStep && (
                      <div className="text-xs">Action: adds validation sub-agent</div>
                    )}
                    {p.action.preferChainOverSwarm && (
                      <div className="text-xs">Action: prefer chain over swarm</div>
                    )}
                    {p.action.reduceParallelism > 0 && (
                      <div className="text-xs">Action: reduce parallelism by {p.action.reduceParallelism}</div>
                    )}
                    {p.action.timeoutOverrideMs >= 0 && (
                      <div className="text-xs">Action: timeout override {p.action.timeoutOverrideMs}ms</div>
                    )}
                  </div>
                )}
              </div>
            ))
          )}
        </div>
      )}

      {/* Heuristics Tab */}
      {activeTab === 'heuristics' && (
        <div className="space-y-2">
          <div className="flex justify-between items-center">
            <span className="text-xs text-muted-foreground">
              {heuristics.length} heuristic{heuristics.length !== 1 ? 's' : ''} computed
            </span>
            <button
              onClick={fetchHeuristics}
              disabled={loading}
              className="text-xs text-muted-foreground hover:text-foreground flex items-center gap-1"
            >
              <RefreshCw className={`w-3 h-3 ${loading ? 'animate-spin' : ''}`} /> Recompute
            </button>
          </div>

          {heuristics.length === 0 ? (
            <div className="text-sm text-muted-foreground italic text-center p-6 border border-dashed rounded">
              No heuristics computed. Click Recompute after accumulating history.
            </div>
          ) : (
            heuristics.map(h => (
              <div key={h.key} className="bg-card border border-border rounded-lg p-3">
                <div className="flex items-center justify-between">
                  <span className="text-sm font-medium font-mono">{h.key}</span>
                  <span className={`text-sm font-mono ${successRateColor(h.successRate)}`}>
                    {(h.successRate * 100).toFixed(0)}%
                  </span>
                </div>
                <div className="flex gap-4 mt-1 text-xs text-muted-foreground">
                  <span>{h.totalEvents} events</span>
                  <span className="text-green-400">{h.successCount} ok</span>
                  <span className="text-red-400">{h.failCount} fail</span>
                  {h.avgDurationMs > 0 && (
                    <>
                      <span>avg {h.avgDurationMs.toFixed(0)}ms</span>
                      <span>p95 {h.p95DurationMs.toFixed(0)}ms</span>
                    </>
                  )}
                </div>
                {h.topFailureReasons.length > 0 && (
                  <div className="mt-1.5 space-y-0.5">
                    {h.topFailureReasons.map((r, i) => (
                      <div key={i} className="text-xs text-red-400/80 truncate">⚠ {r}</div>
                    ))}
                  </div>
                )}
              </div>
            ))
          )}
        </div>
      )}
    </div>
  );
};
)POLICY";
    // ──── RAW STRING CLOSE: )POLICY" ──── end of PolicyPanel block ────
}

std::string ReactIDEGenerator::GenerateExplainabilityPanel() {
    // ──── RAW STRING OPEN: R"EXPLAIN( ──── do not insert C++ inside this block ────
    return R"EXPLAIN(import React, { useState, useEffect, useCallback } from 'react';
import { Search, Eye, AlertTriangle, Shield, GitBranch, RefreshCw, Download, ChevronDown, ChevronRight, Clock, CheckCircle2, XCircle } from 'lucide-react';

interface DecisionNode {
  eventId: number;
  eventType: string;
  agentId: string;
  description: string;
  timestampMs: number;
  durationMs: number;
  success: boolean;
  errorMessage?: string;
  parentEventId?: number;
  trigger: string;
  policyId?: string;
  policyName?: string;
  policyEffect?: string;
}

interface DecisionTrace {
  traceId: string;
  rootAgentId: string;
  rootEventType: string;
  sessionId: string;
  overallSuccess: boolean;
  totalDurationMs: number;
  nodeCount: number;
  failureCount: number;
  policyFireCount: number;
  summary: string;
  nodes: DecisionNode[];
}

interface FailureAttribution {
  failureEventId: number;
  agentId: string;
  failureType: string;
  errorMessage: string;
  wasRetried: boolean;
  retrySucceeded?: boolean;
  correctionStrategy: string;
  policyId?: string;
  policyName?: string;
}

interface SessionExplanation {
  sessionId: string;
  totalEvents: number;
  agentSpawns: number;
  chainExecutions: number;
  swarmExecutions: number;
  failures: number;
  retries: number;
  policyFirings: number;
  narrative: string;
  traces: DecisionTrace[];
  failureAttributions: FailureAttribution[];
  policyAttributions: Array<{
    policyId: string;
    policyName: string;
    effectDescription: string;
    redirectedSwarmToChain: boolean;
    policyAppliedCount: number;
  }>;
}

interface ExplainStats {
  totalEvents: number;
  agentSpawns: number;
  chainExecutions: number;
  swarmExecutions: number;
  failures: number;
  retries: number;
  policyFirings: number;
  traceCount: number;
}

type TabId = 'summary' | 'timeline' | 'badges';

export const ExplainabilityPanel: React.FC = () => {
  const [activeTab, setActiveTab] = useState<TabId>('summary');
  const [agentId, setAgentId] = useState('');
  const [session, setSession] = useState<SessionExplanation | null>(null);
  const [trace, setTrace] = useState<DecisionTrace | null>(null);
  const [stats, setStats] = useState<ExplainStats | null>(null);
  const [loading, setLoading] = useState(false);
  const [expandedNodes, setExpandedNodes] = useState<Set<number>>(new Set());

  const fetchSession = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch('/api/agents/explain');
      if (res.ok) {
        const data = await res.json();
        setSession(data);
        setTrace(null);
      }
    } catch (err) { console.error('Failed to fetch session explanation', err); }
    setLoading(false);
  }, []);

  const fetchTrace = useCallback(async () => {
    if (!agentId.trim()) return;
    setLoading(true);
    try {
      const res = await fetch(`/api/agents/explain?agent_id=${encodeURIComponent(agentId)}`);
      if (res.ok) {
        const data = await res.json();
        setTrace(data);
      }
    } catch (err) { console.error('Failed to fetch trace', err); }
    setLoading(false);
  }, [agentId]);

  const fetchStats = useCallback(async () => {
    try {
      const res = await fetch('/api/agents/explain/stats');
      if (res.ok) setStats(await res.json());
    } catch (err) { console.error('Failed to fetch explain stats', err); }
  }, []);

  useEffect(() => { fetchSession(); fetchStats(); }, []);

  const toggleNode = (id: number) => {
    setExpandedNodes(prev => {
      const next = new Set(prev);
      next.has(id) ? next.delete(id) : next.add(id);
      return next;
    });
  };

  const tabs: { id: TabId; label: string; icon: React.ReactNode }[] = [
    { id: 'summary', label: 'Summary', icon: <Eye className="w-3 h-3" /> },
    { id: 'timeline', label: 'Causal Timeline', icon: <GitBranch className="w-3 h-3" /> },
    { id: 'badges', label: 'Badges', icon: <Shield className="w-3 h-3" /> },
  ];

  const badgeCounts = {
    failures: session?.failures ?? trace?.failureCount ?? 0,
    retries: session?.retries ?? 0,
    policies: session?.policyFirings ?? trace?.policyFireCount ?? 0,
    swarmRedirects: session?.policyAttributions?.filter(p => p.redirectedSwarmToChain).length ?? 0,
  };

  return (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold flex items-center gap-2">
          <Eye className="w-5 h-5 text-cyan-400" />
          Explainability
          <span className="text-xs px-2 py-0.5 rounded bg-cyan-600/20 text-cyan-300">Phase 8A</span>
        </h2>
        <div className="flex gap-2">
          <button onClick={fetchSession} className="p-1 rounded hover:bg-secondary" title="Refresh">
            <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
          </button>
          <button onClick={() => { if (session) { const blob = new Blob([JSON.stringify(session, null, 2)], { type: 'application/json' }); const url = URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = `explain-${session.sessionId}.json`; a.click(); } }} className="p-1 rounded hover:bg-secondary" title="Export snapshot">
            <Download className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Agent search */}
      <div className="flex gap-2">
        <input
          className="flex-1 px-3 py-1.5 bg-secondary rounded text-sm font-mono"
          placeholder="Agent ID (empty = session)"
          value={agentId}
          onChange={e => setAgentId(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && fetchTrace()}
        />
        <button onClick={fetchTrace} className="px-3 py-1.5 bg-cyan-600 hover:bg-cyan-700 rounded text-sm font-medium">
          <Search className="w-4 h-4" />
        </button>
      </div>

      {/* Tabs */}
      <div className="flex gap-1 border-b border-border">
        {tabs.map(tab => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={`flex items-center gap-1.5 px-3 py-2 text-xs font-medium border-b-2 transition-colors ${
              activeTab === tab.id ? 'border-cyan-400 text-cyan-300' : 'border-transparent text-muted-foreground hover:text-foreground'
            }`}
          >
            {tab.icon} {tab.label}
          </button>
        ))}
      </div>

      {/* Summary Tab */}
      {activeTab === 'summary' && (
        <div className="space-y-3">
          {session && (
            <>
              <div className="p-3 bg-secondary/50 rounded-lg text-sm leading-relaxed">
                {session.narrative}
              </div>
              <div className="grid grid-cols-4 gap-2">
                {[
                  { label: 'Events', value: session.totalEvents, color: 'text-blue-400' },
                  { label: 'Agents', value: session.agentSpawns, color: 'text-green-400' },
                  { label: 'Failures', value: session.failures, color: 'text-red-400' },
                  { label: 'Policies', value: session.policyFirings, color: 'text-purple-400' },
                ].map(s => (
                  <div key={s.label} className="p-2 bg-secondary/30 rounded text-center">
                    <div className={`text-xl font-bold ${s.color}`}>{s.value}</div>
                    <div className="text-xs text-muted-foreground">{s.label}</div>
                  </div>
                ))}
              </div>

              {session.failureAttributions.length > 0 && (
                <div className="space-y-1">
                  <h3 className="text-sm font-medium text-red-400 flex items-center gap-1">
                    <AlertTriangle className="w-3 h-3" /> Failure Attributions
                  </h3>
                  {session.failureAttributions.map(fa => (
                    <div key={fa.failureEventId} className="p-2 bg-red-900/10 border border-red-800/30 rounded text-xs">
                      <span className="font-mono">{fa.agentId}</span>
                      <span className="text-muted-foreground"> [{fa.failureType}]: </span>
                      {fa.errorMessage}
                      {fa.wasRetried && (
                        <span className={fa.retrySucceeded ? 'text-green-400' : 'text-red-400'}>
                          {' '}→ retried ({fa.retrySucceeded ? 'success' : 'failed'})
                        </span>
                      )}
                      {fa.policyName && <span className="text-purple-400"> [policy: {fa.policyName}]</span>}
                    </div>
                  ))}
                </div>
              )}
            </>
          )}
          {!session && !loading && <div className="text-sm text-muted-foreground">No session data yet.</div>}
        </div>
      )}

      {/* Timeline Tab */}
      {activeTab === 'timeline' && (
        <div className="space-y-1">
          {(trace?.nodes ?? session?.traces?.[0]?.nodes ?? []).map(node => (
            <div key={node.eventId} className="group">
              <button
                onClick={() => toggleNode(node.eventId)}
                className="w-full flex items-start gap-2 p-2 rounded hover:bg-secondary/50 text-left text-xs"
              >
                <div className="mt-0.5">
                  {expandedNodes.has(node.eventId) ? <ChevronDown className="w-3 h-3" /> : <ChevronRight className="w-3 h-3" />}
                </div>
                <div className="mt-0.5">
                  {node.success ? <CheckCircle2 className="w-3 h-3 text-green-400" /> : <XCircle className="w-3 h-3 text-red-400" />}
                </div>
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2">
                    <span className="font-mono text-cyan-300">#{node.eventId}</span>
                    <span className="px-1.5 py-0.5 bg-secondary rounded text-[10px]">{node.eventType}</span>
                    {node.policyId && <span className="px-1.5 py-0.5 bg-purple-800/30 text-purple-300 rounded text-[10px]">🛡️ {node.policyName}</span>}
                    {node.durationMs > 0 && (
                      <span className="flex items-center gap-0.5 text-muted-foreground">
                        <Clock className="w-2.5 h-2.5" /> {node.durationMs}ms
                      </span>
                    )}
                  </div>
                </div>
              </button>
              {expandedNodes.has(node.eventId) && (
                <div className="ml-10 mb-2 p-2 bg-secondary/30 rounded text-xs space-y-1">
                  {node.description && <div><span className="text-muted-foreground">Description:</span> {node.description}</div>}
                  {node.agentId && <div><span className="text-muted-foreground">Agent:</span> <span className="font-mono">{node.agentId}</span></div>}
                  {node.errorMessage && <div className="text-red-400"><span className="text-muted-foreground">Error:</span> {node.errorMessage}</div>}
                  {node.policyEffect && <div className="text-purple-300"><span className="text-muted-foreground">Policy effect:</span> {node.policyEffect}</div>}
                </div>
              )}
            </div>
          ))}
          {(trace?.nodes ?? session?.traces?.[0]?.nodes ?? []).length === 0 && (
            <div className="text-sm text-muted-foreground p-4 text-center">No events to display. Run an agent, chain, or swarm first.</div>
          )}
        </div>
      )}

      {/* Badges Tab */}
      {activeTab === 'badges' && (
        <div className="space-y-3">
          <div className="grid grid-cols-2 gap-2">
            {[
              { label: 'Failures', count: badgeCounts.failures, icon: <XCircle className="w-4 h-4" />, color: 'bg-red-900/20 border-red-800/40 text-red-400' },
              { label: 'Retries', count: badgeCounts.retries, icon: <RefreshCw className="w-4 h-4" />, color: 'bg-yellow-900/20 border-yellow-800/40 text-yellow-400' },
              { label: 'Policy Actions', count: badgeCounts.policies, icon: <Shield className="w-4 h-4" />, color: 'bg-purple-900/20 border-purple-800/40 text-purple-400' },
              { label: 'Swarm→Chain', count: badgeCounts.swarmRedirects, icon: <GitBranch className="w-4 h-4" />, color: 'bg-cyan-900/20 border-cyan-800/40 text-cyan-400' },
            ].map(badge => (
              <div key={badge.label} className={`p-3 rounded border ${badge.color} flex items-center gap-3`}>
                {badge.icon}
                <div>
                  <div className="text-lg font-bold">{badge.count}</div>
                  <div className="text-xs opacity-75">{badge.label}</div>
                </div>
              </div>
            ))}
          </div>

          {session?.policyAttributions && session.policyAttributions.length > 0 && (
            <div className="space-y-1">
              <h3 className="text-sm font-medium text-purple-400 flex items-center gap-1">
                <Shield className="w-3 h-3" /> Policy Impact
              </h3>
              {session.policyAttributions.map(pa => (
                <div key={pa.policyId} className="p-2 bg-purple-900/10 border border-purple-800/30 rounded text-xs">
                  <div className="font-medium">{pa.policyName}</div>
                  <div className="text-muted-foreground mt-0.5">{pa.effectDescription}</div>
                  <div className="text-muted-foreground">Applied {pa.policyAppliedCount}x
                    {pa.redirectedSwarmToChain && <span className="text-cyan-400 ml-2">⚡ Swarm→Chain redirect</span>}
                  </div>
                </div>
              ))}
            </div>
          )}

          {stats && (
            <div className="p-2 bg-secondary/20 rounded text-xs text-muted-foreground">
              Stats: {stats.traceCount} traces built, {stats.totalEvents} events analyzed
            </div>
          )}
        </div>
      )}
    </div>
  );
};
)EXPLAIN";
    // ──── RAW STRING CLOSE: )EXPLAIN" ──── end of ExplainabilityPanel block ────
}

std::string ReactIDEGenerator::GenerateFailurePanel() {
    // ──── RAW STRING OPEN: R"FAILURE( ──── do not insert C++ inside this block ────
    return R"FAILURE(import React, { useState, useEffect } from 'react';
import { useEngineStore, FailureRecord, FailureStats } from '@/lib/engine-bridge';
import { AlertTriangle, CheckCircle2, XCircle, Ban, BarChart3, RefreshCw, Filter, Clock, Shield } from 'lucide-react';

const outcomeConfig: Record<string, { color: string; icon: React.ReactNode; label: string }> = {
  Corrected: { color: 'text-green-400', icon: <CheckCircle2 className="w-3 h-3" />, label: 'Corrected' },
  Failed:    { color: 'text-red-400',   icon: <XCircle className="w-3 h-3" />,      label: 'Failed' },
  Declined:  { color: 'text-yellow-400', icon: <Ban className="w-3 h-3" />,          label: 'Declined' },
  Detected:  { color: 'text-orange-400', icon: <AlertTriangle className="w-3 h-3" />, label: 'Detected' },
};

const typeColors: Record<string, string> = {
  Hallucination:      'bg-red-600/20 text-red-300',
  Refusal:            'bg-orange-600/20 text-orange-300',
  FormatViolation:    'bg-yellow-600/20 text-yellow-300',
  ToolError:          'bg-purple-600/20 text-purple-300',
  EmptyResponse:      'bg-gray-600/20 text-gray-300',
  Timeout:            'bg-blue-600/20 text-blue-300',
  InvalidOutput:      'bg-pink-600/20 text-pink-300',
  InfiniteLoop:       'bg-cyan-600/20 text-cyan-300',
  LowConfidence:      'bg-amber-600/20 text-amber-300',
  SafetyViolation:    'bg-red-800/20 text-red-200',
  QualityDegradation: 'bg-teal-600/20 text-teal-300',
  UserAbort:          'bg-slate-600/20 text-slate-300',
};

function formatTimestamp(ms: number): string {
  if (!ms) return '';
  const d = new Date(ms);
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

export const FailurePanel: React.FC = () => {
  const { failures, failureStats, getFailures, getAgentStatusDetailed } = useEngineStore();
  const [loading, setLoading] = useState(false);
  const [reasonFilter, setReasonFilter] = useState('');
  const [limit, setLimit] = useState(100);
  const [agentStatus, setAgentStatus] = useState<any>(null);
  const [selectedFailure, setSelectedFailure] = useState<FailureRecord | null>(null);

  useEffect(() => {
    refresh();
  }, []);

  const refresh = async () => {
    setLoading(true);
    await getFailures({ limit, reason: reasonFilter || undefined });
    const status = await getAgentStatusDetailed();
    setAgentStatus(status);
    setLoading(false);
  };

  const filteredFailures = failures.filter(f => {
    if (reasonFilter && f.type !== reasonFilter && !f.type.includes(reasonFilter)) return false;
    return true;
  });

  const retrySuccessRate = failureStats && failureStats.totalRetries > 0
    ? ((failureStats.successAfterRetry / failureStats.totalRetries) * 100).toFixed(1)
    : '0.0';

  const activeReasons = (failureStats?.topReasons || []).filter(r => r.count > 0);

  return (
    <div className="p-4 space-y-4 h-full overflow-auto">
      <h2 className="text-xl font-bold flex items-center gap-2">
        <Shield className="w-6 h-6 text-red-400" /> Failure Intelligence
      </h2>

      {/* ================================================================ */}
      {/* Stats Bar                                                        */}
      {/* ================================================================ */}
      {failureStats && (
        <div className="grid grid-cols-2 md:grid-cols-5 gap-3">
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground">Total Failures</div>
            <div className="text-xl font-mono text-red-400">{failureStats.totalFailures}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground">Retries</div>
            <div className="text-xl font-mono text-yellow-400">{failureStats.totalRetries}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground flex items-center gap-1">
              <CheckCircle2 className="w-3 h-3 text-green-400" /> Corrected
            </div>
            <div className="text-xl font-mono text-green-400">{failureStats.successAfterRetry}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground flex items-center gap-1">
              <Ban className="w-3 h-3 text-yellow-400" /> Declined
            </div>
            <div className="text-xl font-mono text-yellow-400">{failureStats.retriesDeclined}</div>
          </div>
          <div className="bg-card p-3 rounded-lg border border-border">
            <div className="text-xs text-muted-foreground">Retry Success %</div>
            <div className="text-xl font-mono text-blue-400">{retrySuccessRate}%</div>
          </div>
        </div>
      )}

      {/* ================================================================ */}
      {/* Top Failure Reasons                                              */}
      {/* ================================================================ */}
      {activeReasons.length > 0 && (
        <div className="bg-card p-3 rounded-lg border border-border">
          <h3 className="text-sm font-semibold mb-2 flex items-center gap-1">
            <BarChart3 className="w-4 h-4" /> Failure Breakdown
          </h3>
          <div className="flex flex-wrap gap-2">
            {activeReasons.map((r, i) => (
              <button
                key={i}
                onClick={() => setReasonFilter(reasonFilter === r.type ? '' : r.type)}
                className={`text-xs px-2 py-1 rounded-full transition-colors ${
                  typeColors[r.type] || 'bg-gray-600/20 text-gray-300'
                } ${reasonFilter === r.type ? 'ring-1 ring-white/50' : ''}`}
              >
                {r.type}: {r.count}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* ================================================================ */}
      {/* Filters                                                          */}
      {/* ================================================================ */}
      <div className="flex items-center gap-3 flex-wrap">
        <div className="flex items-center gap-1">
          <Filter className="w-4 h-4 text-muted-foreground" />
          <select
            value={reasonFilter}
            onChange={e => setReasonFilter(e.target.value)}
            className="bg-secondary text-sm px-2 py-1.5 rounded border border-border"
          >
            <option value="">All Types</option>
            <option value="Hallucination">Hallucination</option>
            <option value="Refusal">Refusal</option>
            <option value="FormatViolation">FormatViolation</option>
            <option value="ToolError">ToolError</option>
            <option value="EmptyResponse">EmptyResponse</option>
            <option value="Timeout">Timeout</option>
            <option value="InvalidOutput">InvalidOutput</option>
            <option value="InfiniteLoop">InfiniteLoop</option>
            <option value="LowConfidence">LowConfidence</option>
            <option value="SafetyViolation">SafetyViolation</option>
            <option value="QualityDegradation">QualityDegradation</option>
          </select>
        </div>
        <input
          type="number"
          value={limit}
          onChange={e => setLimit(Number(e.target.value))}
          min={1}
          max={1000}
          className="w-20 bg-secondary text-sm px-2 py-1.5 rounded border border-border"
        />
        <button
          onClick={refresh}
          disabled={loading}
          className="bg-primary text-primary-foreground px-3 py-1.5 rounded text-sm flex items-center gap-1 hover:opacity-90 disabled:opacity-50"
        >
          <RefreshCw className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`} />
          Refresh
        </button>
      </div>

      {/* ================================================================ */}
      {/* Failure Timeline                                                 */}
      {/* ================================================================ */}
      <div className="border border-border rounded-lg overflow-hidden">
        <div className="bg-secondary px-3 py-2 text-xs font-semibold flex items-center gap-2">
          <AlertTriangle className="w-3 h-3 text-red-400" />
          Failure Timeline — {filteredFailures.length} events
        </div>
        <div className="max-h-[400px] overflow-y-auto divide-y divide-border">
          {filteredFailures.length === 0 ? (
            <div className="p-4 text-sm text-muted-foreground text-center">
              No failures recorded. The system is operating normally.
            </div>
          ) : (
            filteredFailures.map((f, i) => {
              const outcome = outcomeConfig[f.outcome] || outcomeConfig.Detected;
              return (
                <div
                  key={i}
                  className={`px-3 py-2 hover:bg-secondary/50 text-sm cursor-pointer ${
                    selectedFailure === f ? 'bg-secondary/70' : ''
                  }`}
                  onClick={() => setSelectedFailure(selectedFailure === f ? null : f)}
                >
                  <div className="flex items-center gap-2">
                    <span className={outcome.color}>{outcome.icon}</span>
                    <span className={`text-xs px-1.5 py-0.5 rounded ${
                      typeColors[f.type] || 'bg-gray-600/20 text-gray-300'
                    }`}>
                      {f.type}
                    </span>
                    <span className={`text-xs font-mono ${outcome.color}`}>
                      {outcome.label}
                    </span>
                    <span className="flex-1 truncate text-muted-foreground text-xs">
                      {f.evidence || f.promptSnippet || '(no detail)'}
                    </span>
                    {f.timestampMs > 0 && (
                      <span className="text-xs text-muted-foreground flex items-center gap-0.5">
                        <Clock className="w-3 h-3" />
                        {formatTimestamp(f.timestampMs)}
                      </span>
                    )}
                  </div>

                  {/* Expanded detail */}
                  {selectedFailure === f && (
                    <div className="mt-2 ml-5 space-y-1 text-xs">
                      {f.evidence && (
                        <div><span className="text-muted-foreground">Evidence:</span> {f.evidence}</div>
                      )}
                      {f.strategy && (
                        <div><span className="text-muted-foreground">Strategy:</span> {f.strategy}</div>
                      )}
                      {f.promptSnippet && (
                        <div className="truncate"><span className="text-muted-foreground">Prompt:</span> {f.promptSnippet}</div>
                      )}
                      {f.attempt > 0 && (
                        <div><span className="text-muted-foreground">Attempt:</span> #{f.attempt}</div>
                      )}
                    </div>
                  )}
                </div>
              );
            })
          )}
        </div>
      </div>

      {/* ================================================================ */}
      {/* Inline Correction Badge Example (for integration in chat)        */}
      {/* ================================================================ */}
      {filteredFailures.some(f => f.outcome === 'Corrected') && (
        <div className="bg-green-900/20 border border-green-700/30 rounded-lg p-3">
          <h4 className="text-sm font-semibold text-green-300 flex items-center gap-1 mb-2">
            <CheckCircle2 className="w-4 h-4" /> Corrected Responses
          </h4>
          <div className="space-y-1">
            {filteredFailures
              .filter(f => f.outcome === 'Corrected')
              .slice(0, 5)
              .map((f, i) => (
                <div key={i} className="text-xs flex items-center gap-2">
                  <span className="inline-flex items-center gap-1 bg-green-600/20 text-green-300 px-2 py-0.5 rounded-full">
                    <CheckCircle2 className="w-3 h-3" />
                    Corrected after {f.type}
                  </span>
                  <span className="text-muted-foreground truncate">{f.strategy || 'auto-retry'}</span>
                </div>
              ))}
          </div>
        </div>
      )}
    </div>
  );
};
)FAILURE";
    // ──── RAW STRING CLOSE: )FAILURE" ──── end of FailurePanel block ────
}

std::string ReactIDEGenerator::GenerateSettingsPanel() {
    return R"(import React from 'react';

export const SettingsPanel: React.FC = () => {
  return (
    <div className="p-4">
      <h2 className="text-xl font-bold mb-4">Settings</h2>
      <div className="text-muted-foreground text-sm">
        Configuration options for the RawrXD Engine integration will appear here.
      </div>
    </div>
  );
};
)";
}

// ============================================================================
// BACKEND PANEL — Phase 8B: AI Backend Switcher React Component
// ============================================================================

std::string ReactIDEGenerator::GenerateBackendPanel() {
    return R"BACKEND(import React, { useState, useEffect, useCallback } from 'react';

interface BackendConfig {
  type: string;
  name: string;
  endpoint: string;
  model: string;
  enabled: boolean;
  timeoutMs: number;
  maxTokens: number;
  temperature: number;
  hasApiKey: boolean;
  connected: boolean;
  healthy: boolean;
  latencyMs: number;
  requestCount: number;
  failureCount: number;
  lastError: string;
  lastModel: string;
  lastUsedEpochMs: number;
  isActive: boolean;
}

interface BackendsResponse {
  active: string;
  backends: BackendConfig[];
  count: number;
}

const BACKEND_COLORS: Record<string, string> = {
  LocalGGUF: '#4ec9b0',
  Ollama: '#dcdcaa',
  OpenAI: '#569cd6',
  Claude: '#c586c0',
  Gemini: '#ce9178',
};

const statusIcon = (b: BackendConfig) => {
  if (!b.enabled) return '⭘';
  if (b.healthy) return '✅';
  if (b.connected) return '🟡';
  return '❌';
};

export const BackendPanel: React.FC = () => {
  const [backends, setBackends] = useState<BackendConfig[]>([]);
  const [active, setActive] = useState('');
  const [loading, setLoading] = useState(true);
  const [switching, setSwitching] = useState(false);
  const [error, setError] = useState('');

  const fetchBackends = useCallback(async () => {
    try {
      const res = await fetch('/api/backends');
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data: BackendsResponse = await res.json();
      setBackends(data.backends);
      setActive(data.active);
      setError('');
    } catch (e: any) {
      setError(e.message || 'Failed to fetch backends');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchBackends();
    const interval = setInterval(fetchBackends, 5000);
    return () => clearInterval(interval);
  }, [fetchBackends]);

  const switchBackend = async (type: string) => {
    setSwitching(true);
    try {
      const res = await fetch('/api/backend/switch', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ backend: type }),
      });
      if (!res.ok) {
        const err = await res.json();
        throw new Error(err.message || `Switch failed (${res.status})`);
      }
      await fetchBackends();
    } catch (e: any) {
      setError(e.message || 'Switch failed');
    } finally {
      setSwitching(false);
    }
  };

  if (loading) {
    return (
      <div className="p-4 text-muted-foreground text-sm">
        Loading backends...
      </div>
    );
  }

  return (
    <div className="p-4 space-y-4">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold">AI Backend Switcher</h2>
        <span className="text-xs text-muted-foreground">
          Active: <span style={{ color: BACKEND_COLORS[active] || '#fff' }}>{active}</span>
        </span>
      </div>

      {error && (
        <div className="rounded-md border border-red-500/30 bg-red-500/10 p-3 text-sm text-red-400">
          {error}
        </div>
      )}

      <div className="space-y-2">
        {backends.map((b) => (
          <div
            key={b.type}
            className={`rounded-lg border p-3 transition-colors ${
              b.isActive
                ? 'border-primary bg-primary/5'
                : 'border-border hover:border-muted-foreground/30'
            }`}
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <span title={b.healthy ? 'Healthy' : b.lastError || 'Not connected'}>
                  {statusIcon(b)}
                </span>
                <span
                  className="font-medium"
                  style={{ color: BACKEND_COLORS[b.type] || '#d4d4d4' }}
                >
                  {b.name}
                </span>
                {b.isActive && (
                  <span className="rounded-full bg-primary/20 px-2 py-0.5 text-xs text-primary">
                    active
                  </span>
                )}
              </div>
              {!b.isActive && b.enabled && (
                <button
                  onClick={() => switchBackend(b.type)}
                  disabled={switching}
                  className="rounded-md border border-border px-3 py-1 text-xs hover:bg-accent disabled:opacity-50"
                >
                  {switching ? '...' : 'Switch'}
                </button>
              )}
              {!b.enabled && (
                <span className="text-xs text-muted-foreground italic">disabled</span>
              )}
            </div>

            <div className="mt-2 grid grid-cols-2 gap-x-4 gap-y-1 text-xs text-muted-foreground">
              <span>Model: {b.model || '(default)'}</span>
              <span>Endpoint: {b.endpoint || '(local)'}</span>
              <span>Requests: {b.requestCount}</span>
              <span>Failures: {b.failureCount}</span>
              <span>Latency: {b.latencyMs >= 0 ? `${b.latencyMs}ms` : 'N/A'}</span>
              <span>API Key: {b.hasApiKey ? '••••' : 'none'}</span>
            </div>

            {b.lastError && (
              <div className="mt-1 text-xs text-red-400 truncate" title={b.lastError}>
                Last error: {b.lastError}
              </div>
            )}
          </div>
        ))}
      </div>

      <div className="border-t border-border pt-3 text-xs text-muted-foreground space-y-1">
        <div>
          <strong>API:</strong> GET /api/backends · GET /api/backend/active · POST /api/backend/switch
        </div>
        <div>
          <strong>Palette:</strong> :backend to filter backend commands
        </div>
      </div>
    </div>
  );
};
)BACKEND";
}

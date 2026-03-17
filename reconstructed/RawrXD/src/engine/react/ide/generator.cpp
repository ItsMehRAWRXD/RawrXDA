#include "react_ide_generator.h"
#include <fstream>
#include <sstream>
#include <iostream>

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
        "Hot Patcher", "VSIX Loader", "Model Inference", "Chat Interface", "Agent Orchestrator"
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
      }
    }
  }
})";
}

std::string ReactIDEGenerator::GenerateTailwindConfig() {
    return R"(/** @type {import('tailwindcss').Config} */
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
})";
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
export interface EngineState {
  isConnected: boolean;
  memoryTier: string;
  memoryUsage: number;
  memoryCapacity: number;
  activePlugins: string[];
  loadedModels: string[];
  logs: string[];
  
  // Actions
  connect: () => Promise<void>;
  disconnect: () => void;
  executeCommand: (cmd: string) => Promise<string>;
  loadPlugin: (path: string) => Promise<boolean>;
  setMemoryTier: (tier: string) => Promise<boolean>;
}

// Mock implementation for demo if server not running
const mockEngine = {
  execute: async (cmd: string) => {
    return `Executed: ${cmd}`;
  }
}

export const useEngineStore = create<EngineState>((set, get) => ({
  isConnected: false,
  memoryTier: 'TIER_4K',
  memoryUsage: 1024,
  memoryCapacity: 4096,
  activePlugins: [],
  loadedModels: [],
  logs: [],

  connect: async () => {
    console.log("Connecting to RawrXD Engine...");
    // In a real implementation this would perform WebSocket connection
    setTimeout(() => {
      set({ isConnected: true });
      get().logs.push("[System] Connected to RawrXD Engine v1.0.0");
    }, 1000);
  },

  disconnect: () => {
    set({ isConnected: false });
  },

  executeCommand: async (cmd: string) => {
    // Call backend API
    const response = await mockEngine.execute(cmd);
    set(state => ({ logs: [...state.logs, `> ${cmd}`, response] }));
    return response;
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
    return R"(import React, { useEffect } from 'react';
import { useEngineStore } from '@/lib/engine-bridge';
import { MemoryPanel } from '@/components/MemoryPanel';
import { HotPatchPanel } from '@/components/HotPatchPanel';
import { VSIXLoaderPanel } from '@/components/VSIXLoaderPanel';
import { Terminal } from 'lucide-react';

function App() {
  const { connect, isConnected, logs } = useEngineStore();

  useEffect(() => {
    connect();
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
          <h1 className="font-semibold">RawrXD IDE</h1>
          <div className="text-xs font-mono text-muted-foreground">v1.0.0-alpha</div>
        </header>

        <main className="flex-1 overflow-hidden grid grid-cols-12 auto-rows-fr">
          {/* Left Panel - Editor Area */}
          <div className="col-span-8 border-r border-border bg-[#1e1e1e] p-4 font-mono text-sm">
            // Monaco Editor Placeholder
            <div className="text-gray-500">Editor is initializing...</div>
          </div>

          {/* Right Panel - Tools */}
          <div className="col-span-4 flex flex-col bg-background">
            <div className="flex-1 overflow-y-auto border-b border-border">
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
)";
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

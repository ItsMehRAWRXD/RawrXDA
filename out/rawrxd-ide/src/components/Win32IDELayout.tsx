import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  FileText, Search, GitBranch, Bug, Package, Settings,
  User, Terminal, AlertCircle, PlayCircle, Square,
  ChevronRight, ChevronDown, FolderOpen, File,
  Plus, X, Maximize2, Minimize2, MoreHorizontal,
  Copy, Cut, Clipboard, Save, FolderPlus, Upload,
  Zap, Brain, Layers, Cpu, Activity
} from 'lucide-react';
import { useEngineStore } from '@/lib/engine-bridge';

type SidebarView = 'explorer' | 'search' | 'scm' | 'debug' | 'extensions' | 'ai';
type PanelTab = 'terminal' | 'output' | 'problems' | 'debug-console';

interface StatusInfo {
  ready: boolean;
  model_loaded: boolean;
  model_path: string;
  backend: string;
  capabilities: { completion: boolean; streaming: boolean };
}

interface Win32IDELayoutProps {
  children?: React.ReactNode;
  editor?: React.ReactNode;
  terminal?: React.ReactNode;
  status?: StatusInfo;
  activeFile?: string;
  onFileSelect?: (file: string) => void;
}

// Menu dropdown component
const MenuDropdown: React.FC<{
  label: string;
  items: { label: string; shortcut?: string; action?: () => void; divider?: boolean }[];
}> = ({ label, items }) => {
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', handler);
    return () => document.removeEventListener('mousedown', handler);
  }, []);

  return (
    <div ref={ref} className="relative">
      <span
        onClick={() => setOpen(!open)}
        className={`hover:bg-white/10 px-2 py-1 cursor-pointer select-none ${open ? 'bg-white/10' : ''}`}
      >
        {label}
      </span>
      {open && (
        <div className="absolute top-full left-0 mt-0.5 bg-[#252526] border border-[#454545] rounded shadow-lg min-w-[220px] z-50 py-1">
          {items.map((item, i) =>
            item.divider ? (
              <div key={i} className="border-t border-[#454545] my-1" />
            ) : (
              <button
                key={i}
                onClick={() => { item.action?.(); setOpen(false); }}
                className="w-full text-left px-4 py-1 text-xs hover:bg-[#094771] flex justify-between"
              >
                <span>{item.label}</span>
                {item.shortcut && <span className="text-gray-500 ml-4">{item.shortcut}</span>}
              </button>
            )
          )}
        </div>
      )}
    </div>
  );
};

// File explorer tree
const ExplorerView: React.FC<{ onFileSelect?: (file: string) => void }> = ({ onFileSelect }) => {
  const [expanded, setExpanded] = useState<Record<string, boolean>>({ src: true });
  const files = [
    {
      name: 'src', type: 'dir', children: [
        { name: 'main.cpp', type: 'file' },
        { name: 'interactive_shell.cpp', type: 'file' },
        { name: 'cli_shell.cpp', type: 'file' },
        {
          name: 'core', type: 'dir', children: [
            { name: 'model_memory_hotpatch.cpp', type: 'file' },
            { name: 'byte_level_hotpatcher.cpp', type: 'file' },
            { name: 'unified_hotpatch_manager.cpp', type: 'file' },
          ]
        },
      ]
    },
    { name: 'CMakeLists.txt', type: 'file' },
    { name: 'README.md', type: 'file' },
  ];

  const renderTree = (items: any[], depth: number = 0) => (
    items.map((item: any) => (
      <div key={item.name}>
        <button
          className="w-full text-left py-0.5 px-2 hover:bg-white/5 flex items-center gap-1 text-xs"
          style={{ paddingLeft: `${depth * 16 + 8}px` }}
          onClick={() => {
            if (item.type === 'dir') {
              setExpanded(e => ({ ...e, [item.name]: !e[item.name] }));
            } else {
              onFileSelect?.(item.name);
            }
          }}
        >
          {item.type === 'dir' ? (
            expanded[item.name] ? <ChevronDown className="w-3 h-3" /> : <ChevronRight className="w-3 h-3" />
          ) : <File className="w-3 h-3 text-gray-500" />}
          <span className={item.type === 'dir' ? 'font-medium' : ''}>{item.name}</span>
        </button>
        {item.type === 'dir' && expanded[item.name] && item.children && renderTree(item.children, depth + 1)}
      </div>
    ))
  );

  return <div className="py-1">{renderTree(files)}</div>;
};

// Search view
const SearchView: React.FC = () => {
  const [query, setQuery] = useState('');
  const { executeCommand } = useEngineStore();
  const [results, setResults] = useState<string[]>([]);

  const handleSearch = async () => {
    if (!query) return;
    const result = await executeCommand(`!search ${query}`);
    setResults(result.split('\n').filter(Boolean));
  };

  return (
    <div className="p-2 space-y-2">
      <div className="flex gap-1">
        <input
          type="text"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && handleSearch()}
          placeholder="Search files..."
          className="flex-1 bg-[#3c3c3c] border border-[#454545] rounded px-2 py-1 text-xs text-white outline-none focus:border-purple-500"
        />
      </div>
      {results.map((r, i) => (
        <div key={i} className="text-xs text-gray-300 py-0.5 px-1 hover:bg-white/5 rounded">{r}</div>
      ))}
    </div>
  );
};

// Debug view
const DebugView: React.FC = () => {
  const { executeCommand } = useEngineStore();
  return (
    <div className="p-2 space-y-2">
      <div className="flex gap-1">
        <button onClick={() => executeCommand('!debug start')} className="px-2 py-1 bg-green-700 rounded text-xs hover:bg-green-600">
          <PlayCircle className="w-3 h-3 inline mr-1" />Start
        </button>
        <button onClick={() => executeCommand('!debug stop')} className="px-2 py-1 bg-red-700 rounded text-xs hover:bg-red-600">
          <Square className="w-3 h-3 inline mr-1" />Stop
        </button>
      </div>
      <div className="text-xs text-gray-500 italic">No active debug session</div>
    </div>
  );
};

// Extensions view
const ExtensionsView: React.FC = () => {
  const { activePlugins, loadPlugin } = useEngineStore();
  const fileRef = useRef<HTMLInputElement>(null);

  return (
    <div className="p-2 space-y-2">
      <button
        onClick={() => fileRef.current?.click()}
        className="w-full px-2 py-1.5 bg-purple-700 rounded text-xs hover:bg-purple-600 flex items-center justify-center gap-1"
      >
        <Upload className="w-3 h-3" />Install VSIX
      </button>
      <input
        ref={fileRef}
        type="file"
        accept=".vsix"
        className="hidden"
        onChange={(e) => {
          const f = e.target.files?.[0];
          if (f) loadPlugin(f.name);
        }}
      />
      {activePlugins.length === 0 ? (
        <div className="text-xs text-gray-500 italic">No extensions installed</div>
      ) : (
        activePlugins.map((p, i) => (
          <div key={i} className="flex items-center gap-2 text-xs py-1 px-1 hover:bg-white/5 rounded">
            <div className="w-2 h-2 rounded-full bg-green-500" />
            {p}
          </div>
        ))
      )}
    </div>
  );
};

export const Win32IDELayout: React.FC<Win32IDELayoutProps> = ({
  children,
  editor,
  terminal,
  status,
  activeFile = 'main.cpp',
  onFileSelect,
}) => {
  const { isConnected, logs, executeCommand, disconnect } = useEngineStore();
  const [sidebarView, setSidebarView] = useState<SidebarView>('ai');
  const [sidebarVisible, setSidebarVisible] = useState(true);
  const [panelTab, setPanelTab] = useState<PanelTab>('terminal');
  const [panelVisible, setPanelVisible] = useState(true);
  const [panelHeight, setPanelHeight] = useState(250);
  const [sidebarWidth, setSidebarWidth] = useState(250);
  const [terminalInput, setTerminalInput] = useState('');
  const terminalRef = useRef<HTMLDivElement>(null);

  // Auto-scroll terminal
  useEffect(() => {
    if (terminalRef.current) {
      terminalRef.current.scrollTop = terminalRef.current.scrollHeight;
    }
  }, [logs]);

  const activityBarItems = [
    { id: 'explorer' as const, icon: FileText, label: 'Explorer' },
    { id: 'search' as const, icon: Search, label: 'Search' },
    { id: 'scm' as const, icon: GitBranch, label: 'Source Control' },
    { id: 'debug' as const, icon: Bug, label: 'Debugger' },
    { id: 'extensions' as const, icon: Package, label: 'Extensions' },
    { id: 'ai' as const, icon: Brain, label: 'AI Settings' },
  ];

  const panelTabs = [
    { id: 'terminal' as const, icon: Terminal, label: 'Terminal' },
    { id: 'output' as const, icon: Activity, label: 'Output' },
    { id: 'problems' as const, icon: AlertCircle, label: 'Problems' },
    { id: 'debug-console' as const, icon: Bug, label: 'Debug Console' },
  ];

  // Render sidebar content based on active view
  const renderSidebarContent = () => {
    switch (sidebarView) {
      case 'explorer': return <ExplorerView onFileSelect={onFileSelect} />;
      case 'search': return <SearchView />;
      case 'debug': return <DebugView />;
      case 'extensions': return <ExtensionsView />;
      case 'ai': return children;
      case 'scm':
        return (
          <div className="p-2 text-xs text-gray-500 italic">
            <GitBranch className="w-4 h-4 inline mr-1" />
            No source control providers registered
          </div>
        );
      default: return children;
    }
  };

  // Menu definitions
  const fileMenuItems = [
    { label: 'New File', shortcut: 'Ctrl+N', action: () => onFileSelect?.('untitled.cpp') },
    { label: 'Open File...', shortcut: 'Ctrl+O' },
    { label: 'Save', shortcut: 'Ctrl+S' },
    { label: 'Save As...', shortcut: 'Ctrl+Shift+S' },
    { divider: true, label: '' },
    { label: 'Preferences', action: () => setSidebarView('ai') },
    { divider: true, label: '' },
    { label: 'Exit', action: () => disconnect() },
  ];

  const viewMenuItems = [
    { label: 'Explorer', shortcut: 'Ctrl+Shift+E', action: () => { setSidebarView('explorer'); setSidebarVisible(true); } },
    { label: 'Search', shortcut: 'Ctrl+Shift+F', action: () => { setSidebarView('search'); setSidebarVisible(true); } },
    { label: 'Debug', shortcut: 'Ctrl+Shift+D', action: () => { setSidebarView('debug'); setSidebarVisible(true); } },
    { label: 'Extensions', shortcut: 'Ctrl+Shift+X', action: () => { setSidebarView('extensions'); setSidebarVisible(true); } },
    { divider: true, label: '' },
    { label: panelVisible ? 'Hide Panel' : 'Show Panel', shortcut: 'Ctrl+J', action: () => setPanelVisible(!panelVisible) },
    { label: sidebarVisible ? 'Hide Sidebar' : 'Show Sidebar', shortcut: 'Ctrl+B', action: () => setSidebarVisible(!sidebarVisible) },
  ];

  const terminalMenuItems = [
    { label: 'New Terminal', shortcut: 'Ctrl+`', action: () => { setPanelTab('terminal'); setPanelVisible(true); } },
    { label: 'Show Output', action: () => { setPanelTab('output'); setPanelVisible(true); } },
    { label: 'Show Problems', action: () => { setPanelTab('problems'); setPanelVisible(true); } },
  ];

  const handleTerminalSubmit = async () => {
    if (!terminalInput.trim()) return;
    await executeCommand(terminalInput.trim());
    setTerminalInput('');
  };

  return (
    <div className="h-screen w-screen flex flex-col bg-[#1e1e1e] text-[#cccccc]">
      {/* Title Bar */}
      <div className="h-[30px] bg-[#323233] flex items-center justify-between px-2 select-none drag-region">
        <div className="flex items-center gap-2">
          <div className="w-4 h-4 rounded bg-purple-600 flex items-center justify-center text-[10px] font-bold">R</div>
          <span className="text-xs">RawrXD IDE</span>
          <span className="text-xs text-gray-500">—</span>
          <span className="text-xs text-gray-400">{activeFile}</span>
          {status && (
            <span className={`text-xs ml-2 ${status.ready ? 'text-green-400' : 'text-red-400'}`}>
              {status.ready ? '● Engine Ready' : '○ Engine Offline'}
            </span>
          )}
        </div>
        <div className="flex items-center gap-1">
          <button className="w-[46px] h-[30px] hover:bg-white/10 flex items-center justify-center">
            <Minimize2 className="w-3 h-3" />
          </button>
          <button className="w-[46px] h-[30px] hover:bg-white/10 flex items-center justify-center">
            <Maximize2 className="w-3 h-3" />
          </button>
          <button className="w-[46px] h-[30px] hover:bg-red-600 flex items-center justify-center">
            <X className="w-3 h-3" />
          </button>
        </div>
      </div>

      {/* Menu Bar — functional dropdowns */}
      <div className="h-[35px] bg-[#323233] border-b border-[#2d2d30] flex items-center px-3 gap-1 text-xs">
        <MenuDropdown label="File" items={fileMenuItems} />
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Edit</span>
        <MenuDropdown label="View" items={viewMenuItems} />
        <MenuDropdown label="Terminal" items={terminalMenuItems} />
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Agent</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Debug</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Git</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Tools</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Help</span>
      </div>

      <div className="flex-1 flex overflow-hidden">
        {/* Activity Bar (Far Left) */}
        <div className="w-[48px] bg-[#333333] border-r border-[#2d2d30] flex flex-col items-center py-2 gap-1">
          {activityBarItems.map((item) => (
            <button
              key={item.id}
              onClick={() => {
                if (sidebarView === item.id && sidebarVisible) {
                  setSidebarVisible(false);
                } else {
                  setSidebarView(item.id);
                  setSidebarVisible(true);
                }
              }}
              className={`
                w-[48px] h-[48px] flex flex-col items-center justify-center gap-1
                hover:bg-white/10 relative group cursor-pointer
                ${sidebarView === item.id && sidebarVisible ? 'text-white' : 'text-gray-400'}
              `}
              title={item.label}
            >
              <item.icon className="w-6 h-6" />
              {sidebarView === item.id && sidebarVisible && (
                <div className="absolute left-0 top-0 bottom-0 w-[2px] bg-purple-600" />
              )}
            </button>
          ))}
          <div className="flex-1" />
          <button className="w-[48px] h-[48px] flex items-center justify-center hover:bg-white/10 text-gray-400">
            <User className="w-5 h-5" />
          </button>
          <button
            onClick={() => { setSidebarView('ai'); setSidebarVisible(true); }}
            className="w-[48px] h-[48px] flex items-center justify-center hover:bg-white/10 text-gray-400"
          >
            <Settings className="w-5 h-5" />
          </button>
        </div>

        {/* Primary Sidebar */}
        {sidebarVisible && (
          <div
            className="bg-[#252526] border-r border-[#2d2d30] flex flex-col"
            style={{ width: `${sidebarWidth}px` }}
          >
            {/* Sidebar Header */}
            <div className="h-[35px] flex items-center justify-between px-4 border-b border-[#2d2d30]">
              <span className="text-xs font-semibold uppercase tracking-wider">
                {activityBarItems.find(i => i.id === sidebarView)?.label}
              </span>
              <div className="flex gap-1">
                <button className="w-6 h-6 hover:bg-white/10 flex items-center justify-center rounded">
                  <MoreHorizontal className="w-4 h-4" />
                </button>
                <button
                  onClick={() => setSidebarVisible(false)}
                  className="w-6 h-6 hover:bg-white/10 flex items-center justify-center rounded"
                >
                  <X className="w-4 h-4" />
                </button>
              </div>
            </div>

            {/* Sidebar Content — view-dependent */}
            <div className="flex-1 overflow-y-auto">
              {renderSidebarContent()}
            </div>
          </div>
        )}

        {/* Main Content Area */}
        <div className="flex-1 flex flex-col min-w-0">
          {/* Tab bar for open files */}
          <div className="h-[35px] bg-[#252526] border-b border-[#2d2d30] flex items-center">
            <div className="flex items-center h-full">
              <div className="h-full px-3 flex items-center gap-2 text-xs bg-[#1e1e1e] border-t-2 border-purple-600 text-white">
                <File className="w-3 h-3" />
                {activeFile}
                <button className="ml-1 hover:bg-white/10 rounded p-0.5">
                  <X className="w-3 h-3" />
                </button>
              </div>
            </div>
          </div>

          {/* Editor Area — rendered from parent */}
          <div
            className="flex-1 bg-[#1e1e1e] overflow-hidden"
            style={{ height: panelVisible ? `calc(100% - ${panelHeight}px - 35px)` : 'calc(100% - 35px)' }}
          >
            {editor || (
              <div className="flex items-center justify-center h-full text-gray-500 text-sm">
                Open a file to start editing
              </div>
            )}
          </div>

          {/* Bottom Panel */}
          {panelVisible && (
            <div
              className="bg-[#1e1e1e] border-t border-[#2d2d30] flex flex-col"
              style={{ height: `${panelHeight}px` }}
            >
              {/* Panel Tabs */}
              <div className="h-[35px] flex items-center justify-between bg-[#252526] border-b border-[#2d2d30]">
                <div className="flex">
                  {panelTabs.map((tab) => (
                    <button
                      key={tab.id}
                      onClick={() => setPanelTab(tab.id)}
                      className={`
                        h-[35px] px-4 flex items-center gap-2 text-xs border-t-2
                        ${panelTab === tab.id
                          ? 'border-purple-600 bg-[#1e1e1e] text-white'
                          : 'border-transparent text-gray-400 hover:text-white'
                        }
                      `}
                    >
                      <tab.icon className="w-4 h-4" />
                      {tab.label}
                    </button>
                  ))}
                </div>
                <div className="flex gap-1 px-2">
                  <button className="w-6 h-6 hover:bg-white/10 flex items-center justify-center rounded">
                    <Plus className="w-4 h-4" />
                  </button>
                  <button className="w-6 h-6 hover:bg-white/10 flex items-center justify-center rounded">
                    <ChevronDown className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => setPanelVisible(false)}
                    className="w-6 h-6 hover:bg-white/10 flex items-center justify-center rounded"
                  >
                    <X className="w-4 h-4" />
                  </button>
                </div>
              </div>

              {/* Panel Content */}
              <div className="flex-1 overflow-hidden flex flex-col">
                {panelTab === 'terminal' ? (
                  <div className="flex-1 flex flex-col overflow-hidden">
                    <div ref={terminalRef} className="flex-1 overflow-y-auto">
                      {terminal || (
                        <div className="p-2 text-xs text-gray-500 font-mono">
                          RawrXD Terminal — type a command below
                        </div>
                      )}
                    </div>
                    <div className="border-t border-[#2d2d30] flex items-center px-2 py-1">
                      <span className="text-xs text-purple-400 mr-1">❯</span>
                      <input
                        type="text"
                        value={terminalInput}
                        onChange={(e) => setTerminalInput(e.target.value)}
                        onKeyDown={(e) => e.key === 'Enter' && handleTerminalSubmit()}
                        placeholder="Enter command..."
                        className="flex-1 bg-transparent border-none outline-none text-xs text-white font-mono"
                      />
                    </div>
                  </div>
                ) : panelTab === 'output' ? (
                  <div className="p-2 text-xs font-mono text-gray-400 overflow-y-auto flex-1">
                    {logs.filter(l => !l.startsWith('>')).slice(-100).map((l, i) => (
                      <div key={i} className="py-0.5">{l}</div>
                    ))}
                  </div>
                ) : panelTab === 'problems' ? (
                  <div className="p-2 text-xs text-gray-500 italic">No problems detected</div>
                ) : (
                  <div className="p-2 text-xs font-mono text-gray-400 overflow-y-auto flex-1">
                    <div className="text-gray-500 italic">Debug console ready. Start a debug session first.</div>
                  </div>
                )}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Status Bar — wired to real status */}
      <div className={`h-[22px] flex items-center justify-between px-2 text-[11px] text-white ${isConnected ? 'bg-[#007acc]' : 'bg-[#c72e2e]'
        }`}>
        <div className="flex items-center gap-3">
          <span className="flex items-center gap-1">
            <GitBranch className="w-3 h-3" />
            main
          </span>
          <span className="flex items-center gap-1">
            {isConnected ? '●' : '○'} {isConnected ? 'Connected' : 'Disconnected'}
          </span>
          {status?.model_loaded && (
            <span className="flex items-center gap-1">
              <Brain className="w-3 h-3" />
              {status.model_path.split('/').pop() || status.backend}
            </span>
          )}
        </div>
        <div className="flex items-center gap-3">
          <span>Ln 1, Col 1</span>
          <span>Spaces: 4</span>
          <span>UTF-8</span>
          <span>CRLF</span>
          <span>C++</span>
          {status?.capabilities?.completion && (
            <span className="flex items-center gap-1">
              <Zap className="w-3 h-3" />
              AI Completions
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

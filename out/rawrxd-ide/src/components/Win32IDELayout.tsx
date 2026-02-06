import React, { useState, useEffect } from 'react';
import {
  FileText, Search, GitBranch, Bug, Package, Settings,
  User, Terminal, AlertCircle, PlayCircle, Square,
  ChevronRight, ChevronDown, FolderOpen, File,
  Plus, X, Maximize2, Minimize2, MoreHorizontal,
  Copy, Cut, Clipboard, Save, FolderPlus, Upload,
  Zap, Brain, Layers, Cpu, Activity
} from 'lucide-react';

type SidebarView = 'explorer' | 'search' | 'scm' | 'debug' | 'extensions' | 'ai';
type PanelTab = 'terminal' | 'output' | 'problems' | 'debug-console';

interface Win32IDELayoutProps {
  children?: React.ReactNode;
}

export const Win32IDELayout: React.FC<Win32IDELayoutProps> = ({ children }) => {
  const [sidebarView, setSidebarView] = useState<SidebarView>('ai');
  const [sidebarVisible, setSidebarVisible] = useState(true);
  const [panelTab, setPanelTab] = useState<PanelTab>('terminal');
  const [panelVisible, setPanelVisible] = useState(true);
  const [panelHeight, setPanelHeight] = useState(250);
  const [sidebarWidth, setSidebarWidth] = useState(250);

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

  return (
    <div className="h-screen w-screen flex flex-col bg-[#1e1e1e] text-[#cccccc]">
      {/* Title Bar */}
      <div className="h-[30px] bg-[#323233] flex items-center justify-between px-2 select-none drag-region">
        <div className="flex items-center gap-2">
          <div className="w-4 h-4 rounded bg-purple-600 flex items-center justify-center text-[10px] font-bold">R</div>
          <span className="text-xs">RawrXD IDE</span>
          <span className="text-xs text-gray-500">-</span>
          <span className="text-xs text-gray-400">D:\rawrxd</span>
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

      {/* Menu Bar */}
      <div className="h-[35px] bg-[#323233] border-b border-[#2d2d30] flex items-center px-3 gap-4 text-xs">
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">File</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Edit</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">View</span>
        <span className="hover:bg-white/10 px-2 py-1 cursor-pointer">Terminal</span>
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
                setSidebarView(item.id);
                setSidebarVisible(true);
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
          <button className="w-[48px] h-[48px] flex items-center justify-center hover:bg-white/10 text-gray-400">
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

            {/* Sidebar Content */}
            <div className="flex-1 overflow-y-auto">
              {children}
            </div>
          </div>
        )}

        {/* Main Content Area */}
        <div className="flex-1 flex flex-col min-w-0">
          {/* Editor Area - Rendered by parent */}
          <div
            className="flex-1 bg-[#1e1e1e]"
            style={{ height: panelVisible ? `calc(100% - ${panelHeight}px)` : '100%' }}
          >
            {/* Editor tabs and content go here */}
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
              <div className="flex-1 overflow-hidden">
                {/* Panel content rendered here */}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Status Bar */}
      <div className="h-[22px] bg-[#007acc] flex items-center justify-between px-2 text-[11px] text-white">
        <div className="flex items-center gap-3">
          <span className="flex items-center gap-1">
            <GitBranch className="w-3 h-3" />
            master
          </span>
          <span className="flex items-center gap-1">
            <AlertCircle className="w-3 h-3" />
            0
          </span>
          <span className="flex items-center gap-1">
            <AlertCircle className="w-3 h-3" />
            0
          </span>
        </div>
        <div className="flex items-center gap-3">
          <span>Ln 1, Col 1</span>
          <span>Spaces: 4</span>
          <span>UTF-8</span>
          <span>CRLF</span>
          <span>C++</span>
          <span className="flex items-center gap-1">
            <Zap className="w-3 h-3" />
            Copilot
          </span>
        </div>
      </div>
    </div>
  );
};

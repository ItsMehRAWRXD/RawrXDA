import React, { useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import Layout from './components/Layout';
import Toolbar from './components/Toolbar';
import Sidebar from './components/Sidebar';
import EditorPanel from './components/EditorPanel';
import AgentPanel from './components/AgentPanel';
import Terminal from './components/Terminal';
import { ProjectProvider, useProject } from './contexts/ProjectContext';
import { AgentProvider, useAgent } from './contexts/AgentContext';
import useElectron from './hooks/useElectron';
import './styles/tailwind.css';

function AppContent() {
  const { platform } = useElectron();
  const [isSidebarOpen, setSidebarOpen] = useState(true);
  const [activePanel, setActivePanel] = useState('editor');

  const {
    projectRoot,
    fileTree,
    activeFile,
    openProject,
    openFile,
    updateFile
  } = useProject();

  const {
    activeTaskId,
    taskStatus,
    steps,
    startAgent,
    refreshStatus
  } = useAgent();

  const handleOpenProject = () => {
    openProject();
  };

  const handleToggleAgentPanel = () => {
    setActivePanel((p) => (p === 'agent' ? 'editor' : 'agent'));
  };

  const main = (
    <div className="flex-1 flex flex-col min-h-0">
      <div className="flex-1 flex min-h-0">
        <div className="flex-1 flex flex-col min-h-0">
          <EditorPanel activeFile={activeFile} onUpdateFile={updateFile} />
        </div>
        <AnimatePresence>
          {activePanel === 'agent' && (
            <motion.div
              initial={{ width: 0, opacity: 0 }}
              animate={{ width: 384, opacity: 1 }}
              exit={{ width: 0, opacity: 0 }}
              transition={{ type: 'tween', duration: 0.2 }}
              className="flex-shrink-0 border-l border-gray-700 overflow-hidden"
            >
              <AgentPanel
                onStartAgent={startAgent}
                taskId={activeTaskId}
                taskStatus={taskStatus}
                steps={steps}
                onRefresh={refreshStatus}
              />
            </motion.div>
          )}
        </AnimatePresence>
      </div>
      <Terminal />
    </div>
  );

  const sidebar = (
    <Sidebar
      isOpen={isSidebarOpen}
      onToggle={() => setSidebarOpen(!isSidebarOpen)}
      projectRoot={projectRoot}
      fileTree={fileTree}
      activeFile={activeFile}
      onOpenFolder={handleOpenProject}
      onOpenFile={openFile}
    />
  );

  return (
    <div
      className={`h-screen flex flex-col bg-gray-900 text-white overflow-hidden ${
        platform === 'darwin' ? 'pt-8' : ''
      }`}
    >
      {platform === 'darwin' && (
        <div className="fixed top-0 left-0 right-0 h-8 bg-transparent z-50 pointer-events-none" />
      )}
      <Toolbar
        onOpenProject={handleOpenProject}
        onToggleAgentPanel={handleToggleAgentPanel}
      />
      <Layout sidebar={sidebar} main={main} />
    </div>
  );
}

function App() {
  return (
    <ProjectProvider>
      <AgentProvider>
        <AppContent />
      </AgentProvider>
    </ProjectProvider>
  );
}

export default App;

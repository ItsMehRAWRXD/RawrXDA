import { useMemo } from 'react';

/**
 * Read-only view of `window.electronAPI` for the renderer.
 * Matches preload in `electron/preload.js`: AI/agent/fs/project/menu + `native:status` + terminal PTY +
 * knowledge:* + `probeOllama` + `exportComplianceBundle`.
 * Chat WASM is renderer-embedded (`rawrxdWasmLoader`).
 */
export function useElectron() {
  return useMemo(
    () => ({
      platform: window.electronAPI?.platform ?? 'web',
      isDev: window.electronAPI?.isDev ?? false,
      invokeAI: window.electronAPI?.invokeAI,
      listAIProviders: window.electronAPI?.listAIProviders,
      setActiveAIProvider: window.electronAPI?.setActiveAIProvider,
      nativeStatus: window.electronAPI?.nativeStatus,
      probeOllama: window.electronAPI?.probeOllama,
      appendKnowledgeArtifact: window.electronAPI?.appendKnowledgeArtifact,
      rankKnowledgeSignatures: window.electronAPI?.rankKnowledgeSignatures,
      recordKnowledgeSignatureOutcome: window.electronAPI?.recordKnowledgeSignatureOutcome,
      exportComplianceBundle: window.electronAPI?.exportComplianceBundle,
      startAgent: window.electronAPI?.startAgent,
      getAgentStatus: window.electronAPI?.getAgentStatus,
      approveAgentStep: window.electronAPI?.approveAgentStep,
      cancelAgentTask: window.electronAPI?.cancelAgentTask,
      listAgentTasks: window.electronAPI?.listAgentTasks,
      rollbackAgentMutations: window.electronAPI?.rollbackAgentMutations,
      readFile: window.electronAPI?.readFile,
      writeFile: window.electronAPI?.writeFile,
      readDir: window.electronAPI?.readDir,
      openProject: window.electronAPI?.openProject,
      onProjectOpened: window.electronAPI?.onProjectOpened,
      setMenuAccelerators: window.electronAPI?.setMenuAccelerators,
      onIdeAction: window.electronAPI?.onIdeAction,
      terminalCreate: window.electronAPI?.terminalCreate,
      terminalWrite: window.electronAPI?.terminalWrite,
      terminalResize: window.electronAPI?.terminalResize,
      terminalKill: window.electronAPI?.terminalKill,
      terminalOpenElevated: window.electronAPI?.terminalOpenElevated,
      onTerminalData: window.electronAPI?.onTerminalData,
      onTerminalExit: window.electronAPI?.onTerminalExit,
    }),
    []
  );
}

export default useElectron;
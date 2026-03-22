import { useMemo } from 'react';

/**
 * Read-only view of `window.electronAPI` for the renderer.
 *
 * Gutted: IRC, terminal PTY, knowledge store, enterprise export,
 * ollama probe, WASM bytes loader.
 * Added: nativeStatus (RXIP Win32 bridge state).
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
    }),
    []
  );
}

export default useElectron;
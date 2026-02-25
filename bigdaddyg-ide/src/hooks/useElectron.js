import { useMemo } from 'react';

export function useElectron() {
  return useMemo(
    () => ({
      platform: window.electronAPI?.platform ?? 'web',
      isDev: window.electronAPI?.isDev ?? false,
      invokeAI: window.electronAPI?.invokeAI,
      listAIProviders: window.electronAPI?.listAIProviders,
      setActiveAIProvider: window.electronAPI?.setActiveAIProvider,
      startAgent: window.electronAPI?.startAgent,
      getAgentStatus: window.electronAPI?.getAgentStatus,
      readFile: window.electronAPI?.readFile,
      writeFile: window.electronAPI?.writeFile,
      readDir: window.electronAPI?.readDir,
      openProject: window.electronAPI?.openProject
    }),
    []
  );
}

export default useElectron;

import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import { configManager, BigDaddyGConfig } from './BigDaddyGEngine.config';

interface ConfigContextType {
  config: BigDaddyGConfig;
  updateConfig: (updates: Partial<BigDaddyGConfig>) => void;
  toggleAgent: (agentId: string) => void;
  toggleSandbox: () => void;
  toggleWebGPU: () => void;
  setTheme: (theme: BigDaddyGConfig['ui']['theme']) => void;
  exportConfig: () => string;
  importConfig: (configJson: string) => boolean;
  resetConfig: () => void;
  validation: { valid: boolean; errors: string[] };
}

const ConfigContext = createContext<ConfigContextType | undefined>(undefined);

interface ConfigProviderProps {
  children: ReactNode;
  initialConfig?: Partial<BigDaddyGConfig>;
}

export const ConfigProvider: React.FC<ConfigProviderProps> = ({ 
  children, 
  initialConfig 
}) => {
  const [config, setConfig] = useState<BigDaddyGConfig>(() => {
    if (initialConfig) {
      configManager.updateConfig(initialConfig);
    }
    return configManager.getConfig();
  });

  const [validation, setValidation] = useState(() => configManager.validateConfig());

  // Update config when external changes occur
  useEffect(() => {
    const interval = setInterval(() => {
      const currentConfig = configManager.getConfig();
      if (JSON.stringify(currentConfig) !== JSON.stringify(config)) {
        setConfig(currentConfig);
        setValidation(configManager.validateConfig());
      }
    }, 1000);

    return () => clearInterval(interval);
  }, [config]);

  const updateConfig = (updates: Partial<BigDaddyGConfig>) => {
    configManager.updateConfig(updates);
    const newConfig = configManager.getConfig();
    setConfig(newConfig);
    setValidation(configManager.validateConfig());
  };

  const toggleAgent = (agentId: string) => {
    configManager.toggleAgent(agentId);
    const newConfig = configManager.getConfig();
    setConfig(newConfig);
    setValidation(configManager.validateConfig());
  };

  const toggleSandbox = () => {
    configManager.toggleSandbox();
    const newConfig = configManager.getConfig();
    setConfig(newConfig);
    setValidation(configManager.validateConfig());
  };

  const toggleWebGPU = () => {
    configManager.toggleWebGPU();
    const newConfig = configManager.getConfig();
    setConfig(newConfig);
    setValidation(configManager.validateConfig());
  };

  const setTheme = (theme: BigDaddyGConfig['ui']['theme']) => {
    configManager.setTheme(theme);
    const newConfig = configManager.getConfig();
    setConfig(newConfig);
    setValidation(configManager.validateConfig());
  };

  const exportConfig = () => {
    return configManager.exportConfig();
  };

  const importConfig = (configJson: string) => {
    const success = configManager.importConfig(configJson);
    if (success) {
      const newConfig = configManager.getConfig();
      setConfig(newConfig);
      setValidation(configManager.validateConfig());
    }
    return success;
  };

  const resetConfig = () => {
    configManager.updateConfig({});
    const newConfig = configManager.getConfig();
    setConfig(newConfig);
    setValidation(configManager.validateConfig());
  };

  const contextValue: ConfigContextType = {
    config,
    updateConfig,
    toggleAgent,
    toggleSandbox,
    toggleWebGPU,
    setTheme,
    exportConfig,
    importConfig,
    resetConfig,
    validation
  };

  return (
    <ConfigContext.Provider value={contextValue}>
      {children}
    </ConfigContext.Provider>
  );
};

export const useConfig = (): ConfigContextType => {
  const context = useContext(ConfigContext);
  if (context === undefined) {
    throw new Error('useConfig must be used within a ConfigProvider');
  }
  return context;
};

// Hook for specific config sections
export const useAgentConfig = () => {
  const { config, toggleAgent, updateConfig } = useConfig();
  return {
    agents: config.agents,
    toggleAgent,
    updateAgents: (updates: Partial<typeof config.agents>) => 
      updateConfig({ agents: { ...config.agents, ...updates } })
  };
};

export const usePerformanceConfig = () => {
  const { config, toggleWebGPU, updateConfig } = useConfig();
  return {
    performance: config.performance,
    toggleWebGPU,
    updatePerformance: (updates: Partial<typeof config.performance>) => 
      updateConfig({ performance: { ...config.performance, ...updates } })
  };
};

export const useSandboxConfig = () => {
  const { config, toggleSandbox, updateConfig } = useConfig();
  return {
    sandbox: config.sandbox,
    toggleSandbox,
    updateSandbox: (updates: Partial<typeof config.sandbox>) => 
      updateConfig({ sandbox: { ...config.sandbox, ...updates } })
  };
};

export const useUIConfig = () => {
  const { config, setTheme, updateConfig } = useConfig();
  return {
    ui: config.ui,
    setTheme,
    updateUI: (updates: Partial<typeof config.ui>) => 
      updateConfig({ ui: { ...config.ui, ...updates } })
  };
};

export const useTraceConfig = () => {
  const { config, updateConfig } = useConfig();
  return {
    traces: config.traces,
    updateTraces: (updates: Partial<typeof config.traces>) => 
      updateConfig({ traces: { ...config.traces, ...updates } })
  };
};

export const useModelConfig = () => {
  const { config, updateConfig } = useConfig();
  return {
    models: config.models,
    updateModels: (updates: Partial<typeof config.models>) => 
      updateConfig({ models: { ...config.models, ...updates } })
  };
};

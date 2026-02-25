/**
 * Default config for the renderer. Actual provider config is loaded in the main process
 * from config/providers.json. This module provides defaults and structure reference.
 */
export const defaultConfig = {
  experimental: {
    multiAgent: false,
    openMemory: false,
    ollamaBridge: true,
    copilotIntegration: false,
    amazonQIntegration: false,
    cursorIntegration: false
  },
  providers: {
    bigdaddyg: { enabled: true },
    copilot: { enabled: false },
    amazonq: { enabled: false },
    cursor: { enabled: false }
  }
};

export default defaultConfig;

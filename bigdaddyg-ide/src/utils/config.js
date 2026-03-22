/**
 * Default config for the renderer (structure + conservative defaults).
 * Live provider enablement and API keys are merged in the main process from `config/providers.json` — this object is not authoritative at runtime.
 * Escape hatch: edit providers JSON or env the main process reads; renderer-only builds see these defaults only.
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

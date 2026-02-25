  layout: {
    allowOverlap: false,
    snapToEdges: true,
    panelSpacing: 12,
    defaultArrangement: 'balanced',
    sections: {
      explorer: { dock: 'left', width: 320 },
      chat: { dock: 'right', width: 360 },
      terminal: { dock: 'bottom', height: 260 },
      browser: { dock: 'right', width: 480, floating: false }
    }
  },
  services: {
    orchestra: {
      autoStart: true,
      autoRestart: true,
      autoStartDelayMs: 1500
    }
  },
  hotkeys: {
// ... existing code ...

// Tooling config; UI copy/persistence/logging checklist: ../docs/MINIMALISTIC_7_ENHANCEMENTS.md
module.exports = {
  content: [
    "./src/**/*.{js,jsx,ts,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        'ide-bg': '#1e1e1e',
        'ide-sidebar': '#252526',
        'ide-toolbar': '#2d2d30',
        'ide-accent': '#007acc',
        'ide-success': '#4EC9B0',
        'ide-warning': '#FFCC02',
        'ide-error': '#F44747',
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'bounce-slow': 'bounce 2s infinite',
      },
      backdropBlur: {
        xs: '2px',
      }
    },
  },
  plugins: [],
}

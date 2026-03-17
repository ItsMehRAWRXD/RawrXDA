# ⚡ BigDaddyG IDE - Quick Reference

## 🚀 Launch
```powershell
cd "D:\Security Research aka GitHub Repos\ProjectIDEAI"
npm start
```

## ⌨️ Hotkeys
| Key | Action |
|-----|--------|
| `Ctrl+L` | AI Chat |
| `Ctrl+Shift+M` | Memory Dashboard |
| `Ctrl+Shift+X` | Stop AI |
| `F12` | DevTools |

## 💻 Console Commands
```javascript
window.checkHealth()           // System status
window.memory.stats()          // Memory info
window.ollamaManager.autoConnect()  // Reconnect Ollama
window.showStats()             // Error stats
```

## 🧠 Memory API
```javascript
// Store
await window.memory.store('text', { type: 'note' });

// Query
await window.memory.query('search term', 10);

// Recent
await window.memory.recent(20);

// Stats
window.memory.stats();
```

## 📁 File System
```javascript
// List drives
await window.electron.listDrives();

// Launch program
await window.electron.launchProgram('path.exe');

// Open in explorer
await window.electron.openInExplorer('D:\\Projects');

// Execute command
await window.electron.executeCommand('dir', 'powershell');
```

## 🎯 Features
✅ Persistent Memory  
✅ Offline AI (Ollama)  
✅ 1M Token Context  
✅ 6 AI Agents  
✅ Full System Access  
✅ Professional UI  

## 🎨 Agents
🏗️ Architect | 👨‍💻 Coder | 🛡️ Security  
🧪 Tester | 🔍 Reviewer | ⚡ Optimizer  

---
**Status:** Production Ready ✅  
**Health:** Run `window.checkHealth()`

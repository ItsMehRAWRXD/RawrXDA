# 🎨 Flexible Layout System - User Guide

## Your IDE, Your Way!

The BigDaddyG IDE now features a **fully customizable drag-and-drop layout system**. Arrange panels exactly how you want them!

---

## 🚀 Quick Start

### Default Layout
When you first open the IDE, you'll see:
```
┌─────────────────────────┐
│    📝 Code Editor       │
├─────────────────────────┤
│    💻 Terminal          │
└─────────────────────────┘
```

### Making It Yours
1. **Drag panels** - Grab the header (colored bar at top)
2. **Drop anywhere** - Drop zones appear showing where panel will go
3. **Split panels** - Click ⬌ (horizontal) or ⬍ (vertical) buttons
4. **Maximize** - Click ⛶ to make panel fullscreen
5. **Close** - Click ✕ to remove a panel

---

## 📦 Available Panels

| Icon | Panel | Description |
|------|-------|-------------|
| 📝 | **Code Editor** | Monaco editor for coding |
| 💻 | **Terminal** | Integrated terminal |
| 💬 | **AI Chat** | Chat with AI assistant |
| 📁 | **File Explorer** | Browse project files |
| 🌐 | **Browser** | Built-in web browser |
| 🖥️ | **Console** | Output and logs |
| 🤖 | **Agent Panel** | AI agents control |
| 📊 | **Git** | Version control |

---

## 🎯 Common Layouts

### 1. Classic IDE
```
┌────────┬─────────────┐
│        │             │
│  📁    │   📝        │
│        │   Editor    │
│ Files  │             │
│        ├─────────────┤
│        │  💻         │
│        │  Terminal   │
└────────┴─────────────┘
```

**How to create:**
1. Start with default layout
2. Split editor vertically (⬍)
3. Add File Explorer to left section
4. Split right section horizontally (⬌)
5. Keep editor top, terminal bottom

---

### 2. AI-Powered Development
```
┌─────────────┬──────────┐
│             │          │
│   📝        │   💬     │
│   Editor    │   Chat   │
│             │          │
├─────────────┤   🤖     │
│  💻         │  Agent   │
│  Terminal   │          │
└─────────────┴──────────┘
```

**How to create:**
1. Split workspace vertically
2. Left: Editor + Terminal (split horizontally)
3. Right: Chat + Agent (split horizontally)
4. Drag to rearrange as needed

---

### 3. Full Stack Layout
```
┌──────┬──────────┬──────┐
│      │          │      │
│  📁  │   📝     │  🌐  │
│      │  Editor  │      │
│Files │          │Browser│
│      ├──────────┤      │
│      │   💻     │      │
│      │ Terminal │      │
└──────┴──────────┴──────┘
```

**How to create:**
1. Split workspace vertically into 3 columns
2. Left: File Explorer
3. Middle: Editor + Terminal
4. Right: Browser for live preview

---

### 4. Focus Mode
```
┌─────────────────────────┐
│                         │
│                         │
│      📝 Code Editor     │
│       (Maximized)       │
│                         │
│                         │
└─────────────────────────┘
```

**How to create:**
- Click ⛶ on any panel to maximize
- Click ⛶ again to restore

---

## 🎮 Controls

### Panel Header Buttons

| Button | Action | Shortcut |
|--------|--------|----------|
| ⬌ | Split Horizontally | Click header button |
| ⬍ | Split Vertically | Click header button |
| ⛶ | Maximize/Restore | Click header button |
| ✕ | Close Panel | Click header button |

### Drag & Drop

1. **Click and hold** panel header (colored bar)
2. **Drag** to desired location
3. **Drop zones appear** showing where panel will go:
   - **Top** - Place above
   - **Bottom** - Place below
   - **Left** - Place to left
   - **Right** - Place to right
   - **Center** - Replace current panel
4. **Release** to drop

---

## 💡 Pro Tips

### Keyboard-Friendly Workflow
- Use tab system in header for quick switching
- Maximize panels for focused work
- Split when you need side-by-side comparison

### Save Your Layout
Your layout is **automatically saved**! It persists across sessions.

### Reset to Default
If you mess up:
```javascript
flexibleLayout.createDefaultLayout();
```

### Add More Panels
Click split (⬌ or ⬍) and choose from the panel selector modal.

---

## 🛠️ Advanced Usage

### Custom Arrangements

**Example 1: Code Review Layout**
```
Split workspace → Editor left, Chat right
Chat helps review code while you edit
```

**Example 2: Debugging Layout**
```
Top: Editor (full width)
Bottom Left: Terminal
Bottom Right: Console
Monitor output while coding
```

**Example 3: Learning Mode**
```
Left: Browser (documentation)
Right top: Editor
Right bottom: Terminal (try examples)
```

---

## 🎨 Customization Ideas

### For Web Development
- Editor + Terminal + Browser (preview)
- File Explorer sidebar for quick access

### For Data Science
- Editor + Console + Terminal
- Multiple terminals for different environments

### For Writing/Documentation
- Editor maximized for distraction-free writing
- Preview in browser panel

### For Teaching/Streaming
- Large editor for visibility
- Terminal below for demonstrations
- Chat for questions

---

## 📋 Layout Recipes

### The "Poweruser"
```
┌─────┬────────────┬─────┐
│ 📁  │     📝     │ 💬  │
│Files│   Editor   │Chat │
│     ├────────────┤     │
│ 📊  │     💻     │ 🤖  │
│ Git │  Terminal  │Agent│
└─────┴────────────┴─────┘
```

### The "Minimalist"
```
┌──────────────────┐
│                  │
│    📝 Editor     │
│                  │
│                  │
│                  │
└──────────────────┘
```

### The "Full Stack Dev"
```
┌───────────┬─────────┐
│    📝     │    🌐   │
│  Editor   │ Browser │
├───────────┴─────────┤
│    💻 Terminal      │
└─────────────────────┘
```

---

## 🔧 Technical Details

### Storage
- Layouts saved to `localStorage`
- Key: `flexibleLayout`
- Auto-saves on every change

### API Access
```javascript
// Global instance
window.flexibleLayout

// Methods
flexibleLayout.addPanel(type, containerId)
flexibleLayout.closePanel(panelId)
flexibleLayout.maximizePanel(panelId)
flexibleLayout.saveLayout()
flexibleLayout.loadLayout()
```

---

## 🎯 Best Practices

1. **Start Simple** - Begin with 2-3 panels
2. **Group Related** - Keep related tools together
3. **Use Maximize** - For focused work on one panel
4. **Save Often** - Layout auto-saves but you can manually save
5. **Experiment** - Try different arrangements for different tasks

---

## 🐛 Troubleshooting

### Panel Won't Drag?
- Make sure you're grabbing the header (colored bar)
- Check that panel isn't maximized

### Drop Zones Not Showing?
- Ensure you're actively dragging a panel
- Try releasing and re-grabbing

### Layout Looks Wrong?
- Reset with: `flexibleLayout.createDefaultLayout()`
- Clear localStorage if needed

### Panel Missing?
- Click split button and add it back
- Check if it's hidden behind another panel

---

## 🚀 Coming Soon

- [ ] Preset layouts (save/load custom layouts)
- [ ] Panel resize handles
- [ ] Tab groups within panels
- [ ] Floating panels (detachable)
- [ ] Multi-monitor support
- [ ] Layout templates gallery

---

## 💬 Examples from the Community

Share your layouts! Here are some user favorites:

**The "Night Owl"** - Distraction-free coding
**The "Tutorial Master"** - Documentation + code + output
**The "DevOps Pro"** - Multiple terminals + monitoring

---

**Enjoy your fully customizable IDE! 🎉**

Make it yours. Drag. Drop. Code. Repeat.

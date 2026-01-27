# RawrXD Win32IDE - Quick Reference Guide

## 🎹 Keyboard Shortcuts

### File Operations
- `Ctrl+N` - New File
- `Ctrl+O` - Open File
- `Ctrl+S` - Save File
- `Ctrl+Shift+S` - Save As

### Editing
- `Ctrl+F` - Find
- `Ctrl+H` - Replace
- `F3` - Find Next
- `Shift+F3` - Find Previous
- `Ctrl+C` - Copy
- `Ctrl+V` - Paste
- `Ctrl+Z` - Undo

### Terminal
- `Ctrl+Shift+H` - Split Terminal Horizontal
- `Ctrl+Shift+V` - Split Terminal Vertical

### Git
- `Ctrl+G` - Git Status
- `Ctrl+Shift+C` - Git Commit
- `Ctrl+Shift+P` - Git Push
- `Ctrl+Shift+L` - Git Pull
- `Ctrl+Shift+G` - Git Panel

---

## 📂 Menu System

### File Menu
- New - Create new file
- Open - Open existing file
- Save - Save current file
- Save As - Save with new name
- Exit - Close application

### Edit Menu
- **Find...** - Search for text (Ctrl+F)
- **Replace...** - Find and replace (Ctrl+H)
- **Find Next** - Next occurrence (F3)
- **Find Previous** - Previous occurrence (Shift+F3)
- Insert Snippet... - Code snippets
- Copy with Formatting - Rich text copy
- Paste Plain Text - Unformatted paste
- Clipboard History... - View paste history

### View Menu
- **Minimap** - Toggle code overview
- Output Tabs - Show output panel
- **Output Panel** - Toggle output visibility
- **Module Browser** - PowerShell modules
- Floating Panel - Detached panels
- Theme Editor... - Customize colors

### Terminal Menu
- **PowerShell** - Start PowerShell
- Command Prompt - Start CMD
- Stop Terminal - End terminal
- **Split Horizontal** - Add pane below (Ctrl+Shift+H)
- **Split Vertical** - Add pane right (Ctrl+Shift+V)
- **Clear All Terminals** - Clear output

### Tools Menu
- Start Profiling - Performance tracking
- Stop Profiling - End profiling
- Profile Results... - View metrics
- Analyze Script - Code analysis

### Git Menu
- **Status** - Show Git status (Ctrl+G)
- **Commit...** - Commit changes (Ctrl+Shift+C)
- **Push** - Push to remote (Ctrl+Shift+P)
- **Pull** - Pull from remote (Ctrl+Shift+L)
- **Git Panel** - Full Git UI (Ctrl+Shift+G)

### Modules Menu
- Refresh List - Update module list
- Import Module... - Load custom module
- Export Module... - Save module

### Help Menu
- Command Reference - PowerShell commands
- PowerShell Documentation - Online docs
- Search Help... - Help search
- About - Version info

---

## 🔍 Search & Replace Features

### Find Dialog
1. Open with `Ctrl+F` or Edit → Find
2. Enter search text
3. Options:
   - ☑️ Case sensitive - Match exact case
   - ☑️ Whole word - Match complete words
   - ☑️ Regex - Regular expressions
4. Click "Find Next" or press Enter

### Replace Dialog
1. Open with `Ctrl+H` or Edit → Replace
2. Enter find and replace text
3. Options (same as Find)
4. Actions:
   - **Find Next** - Locate next match
   - **Replace** - Replace current and find next
   - **Replace All** - Replace all occurrences

### Tips
- Search wraps around document ends
- Selected text is highlighted
- Use `F3` for quick "find next"
- Previous searches are remembered

---

## 📝 Code Snippets

### Using Snippets
1. Edit → Insert Snippet...
2. Select from list
3. Click "Insert" or double-click

### Built-in Snippets
- **function** - PowerShell function template
- **if** - If statement
- **foreach** - ForEach loop
- **try** - Try-Catch block

### Creating Snippets
1. Edit → Insert Snippet...
2. Click "New"
3. Enter name and description
4. Write code template
5. Use `${1:placeholder}` for variables
6. Click "Save & Close"

### Managing Snippets
- **Edit** - Modify existing snippet
- **Delete** - Remove snippet
- **Save & Close** - Persist to file
- Snippets saved to: `snippets/snippets.txt`

---

## 💻 Terminal Features

### Single Terminal
- Terminal → PowerShell (or Command Prompt)
- Type commands in bottom input box
- Press Enter to execute
- Output appears in terminal pane

### Multiple Terminals
1. **Split Horizontal**: `Ctrl+Shift+H`
   - Creates new pane below current
   - Same shell type as active pane

2. **Split Vertical**: `Ctrl+Shift+V`
   - Creates new pane to the right
   - Same shell type as active pane

### Terminal Operations
- Click pane to activate
- Each pane independent
- Terminal → Clear All Terminals - Clear all
- Close individual panes via management

---

## 📊 Output Panel

### Tabs
- **Output** - General messages
- **Errors** - Error messages (red)
- **Debug** - Debug info (yellow)
- **Find Results** - Search results

### Severity Filter
Top-right dropdown:
- **All Messages** - Show everything
- **Info & Above** - Skip debug
- **Warnings & Errors** - Important only
- **Errors Only** - Critical only

### Features
- Color-coded messages
- Timestamps on errors/debug
- Auto-routing by severity
- Toggle visibility: View → Output Panel

---

## 🔀 Git Integration

### Quick Status
- Press `Ctrl+G` for terminal status
- Shows branch, changes, commits

### Git Panel (`Ctrl+Shift+G`)
Opens full Git UI with:

#### Status Area
- Current branch name
- Change counts (modified, added, deleted, untracked)

#### File List
- All changed files
- Status indicators:
  - `[S]` - Staged
  - `[ ]` - Unstaged
  - `(M)` - Modified
  - `(A)` - Added
  - `(D)` - Deleted
  - `(?)` - Untracked

#### Actions
1. **Stage Selected** - Add to staging area
2. **Unstage Selected** - Remove from staging
3. **Commit...** - Commit with message
4. **Push** - Push to remote
5. **Pull** - Pull from remote
6. **Refresh** - Update status

### Commit Workflow
1. Make changes to files
2. Open Git Panel (`Ctrl+Shift+G`)
3. Select files to stage
4. Click "Stage Selected"
5. Click "Commit..."
6. Enter commit message
7. Click "Commit"
8. Click "Push" to upload

---

## 🗺️ Minimap

### Purpose
- Overview of entire code file
- Quick navigation
- Visual code structure

### Usage
- Automatically visible on right edge
- Click anywhere to scroll to that line
- Current viewport highlighted
- Toggle: View → Minimap

### Features
- Compressed 2px-per-line view
- Updates on text changes
- Configurable width (default 150px)

---

## 📦 Module Browser

### Opening
- View → Module Browser
- Or Modules → (any menu item)

### Features
- **List View** with columns:
  - Module Name
  - Version
  - Description
  - Status (Loaded/Available)

### Operations
1. **Load Module**
   - Select module
   - Click "Load Module"
   - Status changes to "Loaded"

2. **Unload Module**
   - Select loaded module
   - Click "Unload Module"
   - Status changes to "Available"

3. **Refresh List**
   - Queries PowerShell
   - Updates available modules

4. **Import...**
   - Load custom modules
   - Browse for module file

### Built-in Modules
- Microsoft.PowerShell.Management
- Microsoft.PowerShell.Utility
- Microsoft.PowerShell.Security
- PSReadLine
- PowerShellGet
- Pester

---

## ⚙️ Settings

### Persistent Settings
Saved in `ide_settings.ini`:
- Output panel visibility
- Output tab selection
- Panel heights
- Severity filter level

### Auto-Saved
Settings automatically saved:
- On application exit
- After certain operations
- When closing dialogs

---

## 💡 Tips & Tricks

### Productivity
1. Use `Ctrl+F` then `F3` for rapid searching
2. Create snippets for common code patterns
3. Split terminals for parallel tasks
4. Filter output to errors when debugging

### Git Workflow
1. Keep Git Panel open while working
2. Click Refresh before committing
3. Review diff before staging
4. Write descriptive commit messages

### Terminal Tips
1. Split horizontal for logs + commands
2. Split vertical for side-by-side comparison
3. Use PowerShell for advanced scripting
4. Clear all terminals regularly

### Organization
1. Use Output tabs to separate concerns
2. Route errors to Errors tab
3. Use Debug tab for verbose logging
4. Find Results for search output

---

## 🐛 Troubleshooting

### Search not finding text
- Check case-sensitive option
- Try disabling "Whole word"
- Verify text exists in document

### Terminal not responding
- Stop and restart: Terminal → Stop Terminal
- Then: Terminal → PowerShell

### Git panel empty
- Ensure you're in a Git repository
- Click "Refresh" button
- Check Git installation

### Snippets not saving
- Check `snippets/` directory exists
- Ensure write permissions
- Click "Save & Close" before exiting

### Output not appearing
- Check View → Output Panel is enabled
- Verify correct tab is selected
- Check severity filter setting

---

## 📚 Resources

### PowerShell Help
- Help → PowerShell Documentation
- Help → Command Reference
- Help → Search Help...

### In-Editor Help
- Select cmdlet name
- Terminal will show help

### Online Resources
- PowerShell Docs: https://learn.microsoft.com/powershell/
- Git Documentation: https://git-scm.com/doc

---

## 🎯 Quick Start Checklist

1. ✅ Open or create a file
2. ✅ Start terminal (Terminal → PowerShell)
3. ✅ Enable output panel (View → Output Panel)
4. ✅ Try search feature (Ctrl+F)
5. ✅ Insert a snippet (Edit → Insert Snippet)
6. ✅ Check Git status (Ctrl+G or Ctrl+Shift+G)
7. ✅ Explore module browser (View → Module Browser)

---

**You're now ready to use all features of RawrXD Win32IDE!**

For more detailed information, see `IDE-ENHANCEMENTS-COMPLETE.md`

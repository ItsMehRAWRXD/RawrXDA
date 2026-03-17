# 🚀 RawrXD IDE - Quick Start Guide

## ⚡ 5-Minute Setup

### Prerequisites
- Windows 10/11 (64-bit)
- MASM32 SDK (if building from source)
- 8GB+ RAM recommended
- GPU with Vulkan support (optional)

### Step 1: Download & Extract
```bash
# Extract the delivery package
unzip RawrXD-Delivery.zip
cd RawrXD-Delivery
```

### Step 2: Quick Build (if building)
```batch
cd src\masm\final-ide
BUILD.bat
```

### Step 3: Run IDE
```batch
cd build\bin\Release
RawrXD.exe
```

## 🎯 First Tests

### Test 1: Terminal Command
1. Open chat window
2. Type: `/terminal_run dir`
3. Press Send
4. ✅ Should see directory listing

### Test 2: Git Status
1. Open chat window
2. Type: `/git_status`
3. Press Send
4. ✅ Should see git status (or "not a git repo")

### Test 3: File Operations
1. Type: `/file_write test.txt "Hello RawrXD!"`
2. Type: `/file_read test.txt`
3. ✅ Should see "Hello RawrXD!"

### Test 4: Syntax Highlighting
1. Create file: `/file_create test.asm`
2. Add content via editor
3. Type: `/extension_highlight asm`
4. ✅ Should see syntax highlighting

## 🔧 Common Commands

### Terminal Operations
```
/terminal_run <command>     - Execute any command
/terminal_run dir          - List directory
/terminal_run ipconfig     - Network info
/terminal_run systeminfo   - System information
```

### Git Operations
```
/git_status                - Repository status
/git_commit "message"      - Create commit
/git_push                  - Push to remote
/git_pull                  - Pull from remote
```

### File Operations
```
/file_read <filename>      - Read file contents
/file_write <file> <text>  - Write text to file
/file_list <directory>     - List directory contents
/file_create <filename>    - Create new file
/file_delete <filename>    - Delete file
```

### Project Operations
```
/project_scaffold <name>   - Create new project
/project_build            - Build current project
/project_info             - Show project information
```

### GUI Operations
```
/gui_inspect_layout       - Analyze UI components
/gui_modify_component <id> <x> <y> <w> <h> - Move/resize
/gui_apply_theme <name>   - Apply UI theme
```

## 🚀 Advanced Features

### Model Building (Rawr1024)
```
/rawr1024_build_model     - Build AI model
/rawr1024_quantize_model  - Quantize model
/rawr1024_encrypt_model   - Encrypt model
/rawr1024_direct_load     - Load model directly
```

### GUI Designer
```
/gui_create_component button 100 100 80 30 - Create button
/gui_apply_style <component_id> "{background: blue}" - Style
/gui_animate_component <id> "{x: 200, duration: 1000}" - Animate
```

## 🎨 Customization

### Themes
- Dark theme: `/gui_apply_theme dark`
- Light theme: `/gui_apply_theme light`
- Material theme: `/gui_apply_theme material`

### Settings
All settings stored in `RawrXD.ini` in executable directory.

## 🛠️ Troubleshooting

### Build Issues
```bash
# If MASM32 not found
set MASM32_PATH=C:\masm32

# If build fails
BUILD.bat clean
BUILD.bat
```

### Runtime Issues
```bash
# If IDE won't start
# Check Windows Event Viewer
# Run as Administrator

# If tools not working
# Check chat commands start with "/"
# Verify no spaces in file paths
```

### Performance Issues
```bash
# Close other applications
# Ensure 8GB+ RAM available
# Check GPU drivers (if using GPU features)
```

## 📚 Next Steps

1. **Explore all 44+ tools** via `/help`
2. **Create your first project** with `/project_scaffold`
3. **Customize the GUI** with designer tools
4. **Build AI models** with Rawr1024 engines
5. **Join the community** for support

## 🎯 Quick Productivity Tips

### Use Shortcuts
- **Ctrl+N**: New file
- **Ctrl+O**: Open file
- **Ctrl+S**: Save file
- **Ctrl+Enter**: Send chat command

### Batch Operations
```bash
# Multiple git operations
/git_commit "Initial commit" && /git_push

# Chain file operations
/file_create main.asm && /extension_highlight asm
```

### Template Usage
```bash
# Use project templates
/project_scaffold myapp --template=console
/project_scaffold mylib --template=library
```

---

**🎉 You're ready to use RawrXD IDE!**

Start with basic commands, then explore advanced features. The system is production-ready and waiting for your commands.


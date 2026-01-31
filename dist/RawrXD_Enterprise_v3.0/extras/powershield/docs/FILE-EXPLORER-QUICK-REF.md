# RawrXD IDE - File Explorer Quick Reference

## 🎯 One-Page Cheat Sheet

### Launch IDE
```powershell
& "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build\bin\Release\RawrXD-SimpleIDE.exe"
```

### File Explorer Workflow
1. Look at **left sidebar** → File Explorer panel
2. **Expand D:\\** drive
3. **Expand OllamaModels** folder
4. **Double-click** any `.gguf` file
5. View **Output** tab for model info

---

## 🎮 Controls

| Action | Result |
|--------|--------|
| Click `+` | Expand folder |
| Click `-` | Collapse folder |
| Double-click | Load model (if .gguf file) |
| Single-click | Select item |

---

## 📦 Available Models (D:\OllamaModels\)

| # | Model | Size |
|---|-------|------|
| 1 | bigdaddyg_q5_k_m.gguf | 45.41 GB |
| 2 | BigDaddyG-F32-FROM-Q4.gguf | 36.2 GB |
| 3 | BigDaddyG-NO-REFUSE-Q4_K_M.gguf | 36.2 GB |
| 4 | BigDaddyG-UNLEASHED-Q4_K_M.gguf | 36.2 GB |
| 5 | BigDaddyG-Custom-Q2_K.gguf | 23.71 GB |
| 6 | BigDaddyG-Q2_K-CHEETAH.gguf | 23.71 GB |
| 7 | BigDaddyG-Q2_K-ULTRA.gguf | 23.71 GB |
| 8 | BigDaddyG-Q2_K-PRUNED-16GB.gguf | 15.81 GB |
| 9 | Codestral-22B-v0.1-hf.Q4_K_S.gguf | 11.79 GB |

---

## 🔧 Functions

```cpp
// Create sidebar and TreeView
void createFileExplorer(HWND hwndParent);

// Add folders and files to tree
void populateFileTree(HTREEITEM parentItem, const std::string& path);

// Handle folder expand events
void onFileTreeExpand(HTREEITEM item);

// Get file path for tree item
std::string getTreeItemPath(HTREEITEM item) const;

// Load model from file
void loadModelFromPath(const std::string& filepath);
```

---

## 📊 Output Tab

When you load a model, you'll see:
```
═══════════════════════════════════════════
✓ Model loaded successfully!
File: D:\OllamaModels\bigdaddyg_q5_k_m.gguf
Tensors: 291
Layers: 80
Context: 4096
Vocab: 128256
═══════════════════════════════════════════
```

---

## 💾 Paths

| Item | Path |
|------|------|
| **IDE** | `c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build\bin\Release\RawrXD-SimpleIDE.exe` |
| **Models** | `D:\OllamaModels\` |
| **GGUF Loader** | `src/gguf_loader.cpp` |
| **IDE Source** | `src/win32app/Win32IDE.cpp` |

---

## 🛠️ Alternative: Menu Command

**File > Load Model (GGUF)...**
- Opens file dialog
- Browse and select `.gguf` file
- Click Open to load

---

## ⚡ Key Features

✅ Browse local filesystem  
✅ Navigate drives and folders  
✅ Double-click to load models  
✅ Auto-parse GGUF headers  
✅ Display model metadata  
✅ Error handling  
✅ 9 models ready to use  

---

## 📝 Status

| Component | Status |
|-----------|--------|
| Build | ✅ Success |
| File Explorer | ✅ Working |
| GGUF Loader | ✅ Integrated |
| Models Available | ✅ 9 files |
| Auto-Loading | ✅ Active |

---

**Last Updated**: Nov 30, 2025  
**Version**: 1.0

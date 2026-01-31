# Enhanced Desktop Copilot - Media, Website & Drag-Drop Guide

## 🎯 New Features

Your desktop copilot now has **4 major enhancements**:

1. **🖼️ Image Viewer** - View pictures and photos
2. **📁 File Manager** - Manage and organize files/folders
3. **🌐 Website Preview** - View website content inline
4. **🎯 Drag & Drop** - Drop entire folders and files

---

## 🖼️ Image Viewer

### How to Use

1. **Open Enhanced Copilot**
```powershell
.\desktop_copilot_enhanced.ps1
```

2. **Click "📸 Media Viewer" tab**

3. **Drag image files directly onto the drop zone**
   - Supported formats: JPG, PNG, GIF, BMP, TIFF, WebP
   - Automatically displays with metadata

4. **View image information**
   - Filename, size, dimensions
   - Full file path
   - Last modified date

### Features

```powershell
# Drag and drop support for:
- Single images
- Multiple images (first one displays)
- Entire folders (scans for images)
- Batch image analysis
```

### Example Workflow

```
1. Open copilot
2. Select Image Viewer tab
3. Drag photos from D:\Pictures\Vacation
4. View images with metadata
5. Switch to File Manager to organize
```

---

## 📁 File Manager with Drag-Drop

### How to Use

1. **Click "📁 File Manager" tab**

2. **Drag files or folders to the drop zone**
   - Single files
   - Multiple files
   - Entire folders
   - Mixed (files and folders)

3. **Choose action:**
   - **Analyze Files** - See breakdown by type and size
   - **Organize** - Auto-sort into categories
   - **Clear** - Remove from list

### File Analysis

Shows:
- Total items count
- File vs folder breakdown
- Total size in MB
- File type distribution
- Recommended organization

```
Example Output:
───────────────────────────────────────────
📊 FILE ANALYSIS

Items: 25
Files: 20 | Folders: 5
Total Size: 450.50 MB

File Types:
  .pdf: 8 files
  .jpg: 7 files
  .docx: 3 files
  .mp4: 2 files
───────────────────────────────────────────
```

### Auto-Organization

Automatically sorts files by type:
- **Images** → .jpg, .png, .gif, .bmp
- **Videos** → .mp4, .avi, .mkv, .mov
- **Audio** → .mp3, .wav, .flac
- **Documents** → .pdf, .docx, .txt
- **Programs** → .exe, .msi
- **Archives** → .zip, .rar, .7z
- **Other** → Everything else

### Example Workflow

```powershell
# 1. Open copilot
.\desktop_copilot_enhanced.ps1

# 2. Click File Manager tab

# 3. Drag D:\Downloads folder

# 4. Click "Analyze Files"
   ↓ See file breakdown

# 5. Click "Organize"
   ↓ Files auto-sorted into:
     D:\Downloads_Organized\
       ├── Images\
       ├── Documents\
       ├── Archives\
       └── Other\

# 6. Done! Files organized by type
```

---

## 🌐 Website Preview

### How to Use

1. **Click "🌐 Website Preview" tab**

2. **Enter URL** in the input box
```
https://www.example.com
https://github.com/username/repo
https://stackoverflow.com
```

3. **Choose action:**
   - **🌐 Open** - Open in default browser
   - **📥 Fetch & Preview** - Show content preview

### Features

**Fetch & Preview**
- Extracts text content
- Shows first 2000 characters
- Displays HTTP status
- Shows content type

**Open in Browser**
- Launches default browser
- Full website viewing
- All features available

### Example Output

```
🌐 Website Preview: https://example.com

Status: 200
Content Type: text/html; charset=utf-8

Preview Content:
───────────────────────────────────────────────────────

Example Domain This domain is for use in examples and 
documentation. You may use this domain in examples without 
prior coordination or asking for permission...

───────────────────────────────────────────────────────

💡 Click "Open" button to view full website in browser
```

---

## 🎯 Drag & Drop - Complete Guide

### What You Can Drag

✅ **Single Files**
```
Drag: document.pdf → Copilot window
Result: File added to list
```

✅ **Multiple Files**
```
Drag: file1.jpg, file2.jpg, file3.jpg → Copilot
Result: All 3 files added
```

✅ **Entire Folders**
```
Drag: D:\My Photos (folder) → Copilot
Result: Folder and contents added
```

✅ **Mixed Selection**
```
Drag: document.pdf + photo folder → Copilot
Result: All items added (files + folder)
```

### Drag-Drop Zones

**Main Window**
- Drag anywhere on the form
- Auto-routes to appropriate tab

**Media Viewer Tab**
- Drop here to view images
- Displays first image found

**File Manager Tab**
- Drop here to organize files
- Shows complete file listing

**System Tray Icon**
- Drag to taskbar icon
- Opens copilot automatically

### Visual Feedback

```
Dragging over drop zone:
┌─────────────────────────┐
│ 📁 Drag files here ← Background color changes
│    (Color highlights)   │
└─────────────────────────┘

After dropping:
✅ Files added! (Balloon notification)
```

---

## 💡 Advanced Examples

### Example 1: Organize Photo Folder

```powershell
# 1. Open enhanced copilot
.\desktop_copilot_enhanced.ps1

# 2. Go to Media Viewer tab

# 3. Drag D:\Photos folder
   - Automatically shows first photo

# 4. Go to File Manager tab

# 5. Drag D:\Photos again
   - Shows all files

# 6. Click "Analyze Files"
   - See photo breakdown

# 7. Click "Organize"
   - Creates D:\Photos_Organized with categories:
     ├── Images (all photos sorted)
     ├── Videos (if any)
     └── Other
```

### Example 2: Clean Downloads Folder

```powershell
# 1. Open copilot

# 2. File Manager tab

# 3. Drag entire D:\Downloads folder

# 4. Click "Analyze Files"
   📊 Result:
   Items: 157
   Size: 2450 MB
   Types: PDFs, EXEs, ZIPs, Images, etc.

# 5. Click "Organize"
   ✅ All files sorted automatically
   Created folders:
   - Documents (all PDFs)
   - Programs (all EXE/MSI)
   - Archives (all ZIP/RAR)
   - Images (all JPG/PNG)
```

### Example 3: Check Before Extracting Archive

```powershell
# 1. Open copilot

# 2. File Manager tab

# 3. Drag large.zip file (don't extract yet!)

# 4. Click "Analyze Files"
   - See what's inside
   - File count and types
   - Total size

# 5. Now decide:
   - Extract to organized folder (using copilot)
   - Or handle manually
```

---

## 🔧 Customization

### Add Custom File Categories

Edit the `OrganizeFiles()` method:

```powershell
[void] OrganizeFiles() {
    $category = switch ([System.IO.Path]::GetExtension($file).ToLower()) {
        # Add your custom types here:
        {$_ -in '.ts','.tsx','.js','.jsx'} { 'Code' }
        {$_ -in '.psd','.ai','.sketch'} { 'Design' }
        {$_ -in '.mp3','.flac','.m4a'} { 'Music' }
        
        # Original categories:
        {$_ -in '.jpg','.jpeg','.png'} { 'Images' }
        # ... rest of code
    }
}
```

### Change Colors

```powershell
# Drag zone background
$dragLabel.BackColor = [System.Drawing.Color]::FromArgb(200, 220, 255)

# Main window
$this.MainWindow.BackColor = [System.Drawing.Color]::White

# Button colors
$button.BackColor = [System.Drawing.Color]::FromArgb(76, 175, 80)
```

### Add More Tabs

```powershell
[void] CreateMyCustomTab() {
    $tab = New-Object System.Windows.Forms.TabPage
    $tab.Text = "🎨 My Tab"
    
    # Add controls here
    
    $this.TabControl.TabPages.Add($tab)
}

# Call in InitializeMainWindow():
$this.CreateMyCustomTab()
```

---

## 📊 System Info Tab

Real-time system diagnostics:

```
CPU Usage:          45%
Memory Usage:       62% (8.5 GB free / 16 GB total)
Disk Usage (C:):    68% (150 GB free)

TOP PROCESSES (by CPU)
─────────────────────────────────
chrome             CPU:  25.50 | RAM:  2048 MB
vs code            CPU:  12.30 | RAM:  1024 MB
outlook            CPU:   8.50 | RAM:   512 MB
...
```

**Click "🔄 Refresh"** to update in real-time.

---

## 🎯 Use Cases

### For Students
- Drop study notes/photos
- Organize by subject
- Preview websites for research

### For Content Creators
- View photos before uploading
- Organize project files
- Check website embeds

### For Office Workers
- Organize documents by type
- Quick website preview
- System health monitoring

### For Home Users
- Clean downloads folder
- Organize photos by type
- Quick file management

---

## 📁 File Locations

**Enhanced Copilot Script**
```
D:\lazy init ide\desktop\desktop_copilot_enhanced.ps1
```

**Original Copilot (still available)**
```
D:\lazy init ide\desktop\desktop_copilot.ps1
```

**Template Generator**
```
D:\lazy init ide\desktop\copilot_generator.ps1
```

---

## 🚀 Quick Start

```powershell
# 1. Open PowerShell
cd "D:\lazy init ide\desktop"

# 2. Run enhanced copilot
.\desktop_copilot_enhanced.ps1

# 3. Drag files to window or tabs

# 4. Use the 4 tabs:
   - 📸 Media Viewer
   - 📁 File Manager
   - 🌐 Website Preview
   - 💻 System Info
```

---

## 🔑 Key Features Summary

| Feature | How to Use | Result |
|---------|-----------|--------|
| **View Images** | Drag to Media Viewer | See photo with metadata |
| **Organize Files** | Drag to File Manager, click Organize | Auto-sorted by type |
| **Analyze Files** | Drag to File Manager, click Analyze | See breakdown and stats |
| **Preview Website** | Enter URL, click Fetch | See content preview |
| **Open Website** | Enter URL, click Open | Launch in browser |
| **System Diagnostics** | Click System Info tab | Real-time performance |
| **Fix Issues** | Right-click tray > Fix Issues | Auto-fix common problems |

---

## 💡 Pro Tips

1. **Drag entire folders** - Copilot handles it automatically
2. **Multiple files** - Add multiple items to file list, then analyze/organize
3. **Large archives** - Analyze before extracting to see what's inside
4. **Website preview** - Useful for quick content check before opening
5. **Organize regularly** - Keep your system clean with regular file organization
6. **Check system info** - Monitor performance while working

---

## 🎉 You Now Have

✅ **Complete media viewing** - Images with metadata  
✅ **Drag & drop support** - Files, folders, batches  
✅ **File organization** - Auto-sort by type  
✅ **Website preview** - Quick content check  
✅ **System monitoring** - Real-time diagnostics  
✅ **All in taskbar** - Always accessible  

**Start using enhanced copilot now!**

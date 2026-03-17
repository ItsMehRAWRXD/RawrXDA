# PKG Compilation Fix - Index.html Not Found Error

## Problem
When running the compiled `.exe` file, the server couldn't find `index.html` because:
1. PKG packages Node.js apps into a single executable with a snapshot filesystem
2. Static files like `index.html`, `css/`, and `js/` were being looked for in `process.cwd()` (current working directory)
3. These files need to be external to the executable and placed alongside it

## Solution Applied

### 1. Fixed `server/server.js`
Modified the root directory detection to handle both development and packaged modes:

```javascript
// Determine root directory - handle both pkg and non-pkg environments
const isPkg = typeof process.pkg !== 'undefined';
const rootDir = isPkg 
    ? path.dirname(process.execPath)  // When packaged, use exe directory
    : process.cwd();                   // In development, use current directory
```

### 2. Updated `package-exe.json`
Changed the PKG assets configuration to only include server-side files in the snapshot:
- Only `server/**/*.js`, `server/**/*.json`, and `node_modules/**/*` are embedded
- Frontend assets (`index.html`, `css/`, `js/`) are kept external

### 3. Updated `build-exe.bat`
Modified the build script to:
- Build the executable from `server/server.js` with the correct configuration
- Copy all static assets to the same directory as the executable
- Provide clear instructions on which files need to stay together

## How to Rebuild

1. **Navigate to the project directory:**
   ```batch
   cd E:\Everything\mycopilot-plus-plus-ide
   ```

2. **Run the build script:**
   ```batch
   build-exe.bat
   ```

3. **Verify the output:**
   - `MyCoPilot-IDE.exe` - The executable
   - `index.html` - Main HTML file
   - `css/` - Stylesheets folder
   - `js/` - JavaScript files folder
   - `kodi/` - Kodi integration (optional)
   - `offline/` - Offline assets (optional)

## Running the Application

**Option 1: From the build directory**
```batch
.\MyCoPilot-IDE.exe
```

**Option 2: Create a portable package**
1. Create a new folder (e.g., `MyCoPilot-Portable`)
2. Copy the following into it:
   - `MyCoPilot-IDE.exe`
   - `index.html`
   - `css/` folder
   - `js/` folder
   - `kodi/` folder (if needed)
   - `offline/` folder (if needed)
3. Run `MyCoPilot-IDE.exe` from that folder

## How It Works

1. **Development Mode**: Uses `process.cwd()` to serve files from the project directory
2. **Production Mode (PKG)**: Detects PKG environment and uses `path.dirname(process.execPath)` to serve files from the exe's directory

The server now automatically detects which mode it's running in and adjusts file paths accordingly.

## Quick Test

After rebuilding, test the server:

```batch
cd E:\Everything\mycopilot-plus-plus-ide
.\MyCoPilot-IDE.exe
```

Then open: `http://localhost:8080`

You should see:
- ✅ No "File not found" errors
- ✅ Index.html loads correctly
- ✅ Console shows: "Running in: PKG mode"
- ✅ Console shows: "Root directory: E:\Everything\mycopilot-plus-plus-ide"

## Troubleshooting

**Error: Still getting "File not found"**
- Ensure `index.html` is in the same directory as the `.exe`
- Check console output for the detected root directory
- Verify all folders (`css`, `js`) are present

**Error: "Cannot find module"**
- Reinstall dependencies: `npm install`
- Rebuild: `build-exe.bat`

**Error: "Port already in use"**
- Stop any existing instances
- Change port in server.js if needed

## Files Modified
- ✅ `server/server.js` - Added PKG detection and proper path handling
- ✅ `package-exe.json` - Updated assets configuration
- ✅ `build-exe.bat` - Improved build process with asset copying

## Date Applied
January 16, 2026

## Status
✅ **FIXED** - Ready to rebuild and deploy

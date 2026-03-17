const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const isDev = require('electron-is-dev');
const { spawn } = require('child_process');

let mainWindow;
let pythonProcess;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false,
            enableRemoteModule: true
        }
    });

    // Start Python backend
    startPythonBackend();

    // Load the IDE
    mainWindow.loadURL(isDev ? 'http://localhost:3000' : `file://${path.join(__dirname, 'build/index.html')}`);
    
    if (isDev) {
        mainWindow.webContents.openDevTools();
    }

    mainWindow.on('closed', () => {
        stopPythonBackend();
        mainWindow = null;
    });
}

function startPythonBackend() {
    const pythonPath = path.join(__dirname, 'ide_core.py');
    pythonProcess = spawn('python', [pythonPath]);

    pythonProcess.stdout.on('data', (data) => {
        console.log(`Python Output: ${data}`);
        mainWindow.webContents.send('python-output', data.toString());
    });

    pythonProcess.stderr.on('data', (data) => {
        console.error(`Python Error: ${data}`);
        mainWindow.webContents.send('python-error', data.toString());
    });
}

function stopPythonBackend() {
    if (pythonProcess) {
        pythonProcess.kill();
        pythonProcess = null;
    }
}

// Handle IPC messages from renderer
ipcMain.on('execute-python', (event, data) => {
    if (pythonProcess) {
        pythonProcess.stdin.write(JSON.stringify(data) + '\n');
    }
});

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    stopPythonBackend();
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (mainWindow === null) {
        createWindow();
    }
});
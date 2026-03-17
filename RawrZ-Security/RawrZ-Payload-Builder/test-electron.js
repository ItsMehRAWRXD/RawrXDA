console.log('Testing electron import...');
try {
  const electron = require('electron');
  console.log('Electron imported successfully:', typeof electron);
  console.log('App available:', typeof electron.app);
  console.log('BrowserWindow available:', typeof electron.BrowserWindow);
} catch (error) {
  console.error('Electron import failed:', error.message);
}

console.log('Node version:', process.version);
console.log('Platform:', process.platform);
console.log('Arch:', process.arch);
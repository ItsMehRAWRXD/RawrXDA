#!/usr/bin/env node

/**
 * RawrZ Control Center Launcher
 * Ensures the web interface launches instead of CLI mode
 */

const { spawn } = require('child_process');
const path = require('path');

console.log(`
╔══════════════════════════════════════════════════════════════════════════════╗
║                        RawrZ HTTP Control Center                          ║
║                         Launching Web Interface                           ║
╚══════════════════════════════════════════════════════════════════════════════╝
`);

console.log('🚀 Starting RawrZ Control Center...');
console.log('📱 Web interface will be available at: http://localhost:8080/panel');
console.log('🔧 Toggle management at: http://localhost:8080/toggles');
console.log('');

// Launch the server
const serverPath = path.join(__dirname, 'server.js');
const serverProcess = spawn('node', [serverPath], {
  stdio: 'inherit',
  cwd: __dirname
});

serverProcess.on('error', (err) => {
  console.error('❌ Failed to start server:', err.message);
  process.exit(1);
});

serverProcess.on('exit', (code) => {
  if (code !== 0) {
    console.error(`❌ Server exited with code ${code}`);
    process.exit(code);
  }
});

// Auto-open browser after a short delay
setTimeout(() => {
  console.log('🌐 Opening web interface...');
  const open = require('child_process');
  
  // Try to open the default browser
  const start = process.platform === 'darwin' ? 'open' : 
                process.platform === 'win32' ? 'start' : 'xdg-open';
  
  try {
    open.exec(`${start} http://localhost:8080/panel`);
    console.log('✅ Control Center opened in your default browser');
  } catch (error) {
    console.log('⚠️  Could not auto-open browser. Please manually navigate to:');
    console.log('   http://localhost:8080/panel');
    console.log('   http://localhost:8080/toggles');
  }
}, 3000); // Wait 3 seconds for server to start

// Handle graceful shutdown
process.on('SIGINT', () => {
  console.log('\n🛑 Shutting down RawrZ Control Center...');
  serverProcess.kill('SIGINT');
});

process.on('SIGTERM', () => {
  console.log('\n🛑 Shutting down RawrZ Control Center...');
  serverProcess.kill('SIGTERM');
});

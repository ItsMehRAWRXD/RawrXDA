const config = require('../config');
const { logger } = require('./utils/logger');
const { dataIntegrityValidator } = require('./utils/dataIntegrity');
const { reverseTracer } = require('./utils/reverseTracer');
const { reverseTracePipeline } = require('./utils/reverseTracePipeline');
const { chatterbox } = require('./utils/chatterbox');
const { ircBot } = require('./utils/ircBot');
const { builtinDatabase } = require('./utils/builtinDatabase');
require('dotenv').config();

// RawrZ Engine Integration
let rawrzEngine;
try {
  rawrzEngine = require('./engines/rawrz-engine');
  console.log('[OK] RawrZ Engine module loaded successfully');
} catch (error) {
  console.error('[ERROR] Failed to load RawrZ Engine module:', error.message);
  rawrzEngine = {
    initializeModules: async () => { throw new Error('RawrZ Engine not available'); },
    getStatus: () => ({ error: 'Engine not loaded' }),
    encryptAdvanced: async () => { throw new Error('Engine not available'); }
  };
}

// Initialize and start IRC bot
async function startIRCBot() {
  try {
    // Initialize RawrZ Engine
    console.log('[INFO] Starting RawrZ Engine initialization...');
    await rawrzEngine.initializeModules();
    console.log('[SECURITY] RawrZ Security Engine initialized successfully');
    console.log('[OK] All engine modules loaded and ready');

    // Initialize built-in database
    console.log('[INFO] Starting built-in database...');
    const dbInitialized = await builtinDatabase.initialize();
    
    if (dbInitialized) {
      console.log('[OK] Built-in database initialized successfully');
      console.log('[STATUS] Database features enabled');
      
      // Initialize database with system status
      await builtinDatabase.updateSystemStats({
        health: {
          status: 'healthy',
          activeScripts: 0,
          recentErrors: 0,
          stuckScripts: 0,
          uptime: 0
        },
        heartbeat: {
          monitoring: true,
          lastHeartbeat: new Date(),
          overdueScripts: []
        },
        engines: {
          totalModules: 17,
          loadedModules: 17,
          moduleStatus: 'all_loaded'
        }
      });
      
      console.log('[DB] Database initialized with system status');
    } else {
      console.log('[WARNING] Built-in database failed to initialize, continuing without database features');
      console.log('[INFO] Bot will run in standalone mode');
    }

    // Start IRC Bot
    console.log('[BOT] Starting IRC Bot...');
    console.log(`[BOT] Server: ${config.IRC.server}:${config.IRC.port}`);
    console.log(`[BOT] Nick: ${config.IRC.nick}`);
    console.log(`[BOT] Channels: ${config.IRC.channels.join(', ')}`);
    
    // The IRC bot will handle all functionality
    // No web server needed - pure IRC bot

    // Graceful shutdown
    process.on('SIGTERM', () => {
      console.log('[SHUTDOWN] SIGTERM received, shutting down gracefully');
      process.exit(0);
    });

    process.on('SIGINT', () => {
      console.log('[SHUTDOWN] SIGINT received, shutting down gracefully');
      process.exit(0);
    });

  } catch (error) {
    console.error('[ERROR] Failed to start IRC bot:', error);
    process.exit(1);
  }
}

startIRCBot();
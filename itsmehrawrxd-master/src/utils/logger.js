// RawrZ Logger - Centralized logging system for all engines
const fs = require('fs').promises;
const path = require('path');

class Logger {
    constructor() {
        this.logLevel = process.env.LOG_LEVEL || 'info';
        this.logFile = path.join(__dirname, '../../logs/rawrz.log');
        this.levels = {
            error: 0,
            warn: 1,
            info: 2,
            debug: 3
        };
    }

    async log(level, message, ...args) {
        if (this.levels[level] > this.levels[this.logLevel]) {
            return;
        }

        const timestamp = new Date().toISOString();
        const logMessage = `[${timestamp}] [${level.toUpperCase()}] ${message}${args.length ? ' ' + args.join(' ') : ''}`;
        
        // Console output
        console.log(logMessage);
        
        // File output (async, don't wait)
        this.writeToFile(logMessage).catch(err => {
            console.error('Failed to write to log file:', err.message);
        });
    }

    async writeToFile(message) {
        try {
            // Ensure logs directory exists
            const logsDir = path.dirname(this.logFile);
            await fs.mkdir(logsDir, { recursive: true });
            
            // Append to log file
            await fs.appendFile(this.logFile, message + '\n');
        } catch (error) {
            // Silently fail if we can't write to log file
        }
    }

    error(message, ...args) {
        return this.log('error', message, ...args);
    }

    warn(message, ...args) {
        return this.log('warn', message, ...args);
    }

    info(message, ...args) {
        return this.log('info', message, ...args);
    }

    debug(message, ...args) {
        return this.log('debug', message, ...args);
    }
}

// Create singleton instance
const logger = new Logger();

module.exports = { logger };

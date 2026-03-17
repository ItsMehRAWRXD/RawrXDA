class Logger {
    constructor() {
        this.logLevel = 'info';
    }

    info(message, data = null) {
        console.log(`[INFO] ${new Date().toISOString()} - ${message}`);
        if (data) console.log(JSON.stringify(data, null, 2));
    }

    error(message, error = null) {
        console.error(`[ERROR] ${new Date().toISOString()} - ${message}`);
        if (error) console.error(error);
    }

    warn(message, data = null) {
        console.warn(`[WARN] ${new Date().toISOString()} - ${message}`);
        if (data) console.warn(JSON.stringify(data, null, 2));
    }

    debug(message, data = null) {
        if (this.logLevel === 'debug') {
            console.log(`[DEBUG] ${new Date().toISOString()} - ${message}`);
            if (data) console.log(JSON.stringify(data, null, 2));
        }
    }
}

const logger = new Logger();

module.exports = { logger };
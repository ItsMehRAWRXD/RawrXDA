// RawrZ Safe Entry Point - Safe system entry without malformation risks
const { safeStartup } = require('./safeStartup');

// Main entry point function
async function main() {
    try {
        console.log('='.repeat(60));
        console.log('RawrZ Security Platform - Safe Initialization');
        console.log('='.repeat(60));
        
        // Start the system safely
        const started = await safeStartup.start();
        
        if (started) {
            console.log('='.repeat(60));
            console.log('RawrZ Security Platform is ready and operational');
            console.log('='.repeat(60));
            
            // Keep the process alive
            process.stdin.resume();
            
            // Log periodic status updates
            setInterval(() => {
                const status = safeStartup.getStatus();
                console.log(`[STATUS] System running - Components: ${status.components.components.length}, Uptime: ${Math.round((Date.now() - new Date(status.startup.startTime).getTime()) / 1000)}s`);
            }, 30000); // Every 30 seconds
            
        } else {
            throw new Error('Startup failed');
        }
        
    } catch (error) {
        console.error('='.repeat(60));
        console.error('RawrZ Security Platform - Startup Failed');
        console.error('='.repeat(60));
        console.error(`Error: ${error.message}`);
        console.error('Stack:', error.stack);
        
        // Generate error report
        const report = safeStartup.getReport();
        console.error('Startup Report:', JSON.stringify(report, null, 2));
        
        process.exit(1);
    }
}

// Handle command line arguments
if (require.main === module) {
    const args = process.argv.slice(2);
    
    if (args.includes('--help') || args.includes('-h')) {
        console.log('RawrZ Security Platform - Safe Entry Point');
        console.log('');
        console.log('Usage: node src/init/safeEntry.js [options]');
        console.log('');
        console.log('Options:');
        console.log('  --help, -h     Show this help message');
        console.log('  --version, -v  Show version information');
        console.log('  --status       Show system status');
        console.log('');
        console.log('This entry point provides safe initialization without malformation risks.');
        process.exit(0);
    }
    
    if (args.includes('--version') || args.includes('-v')) {
        const packageJson = require('../../package.json');
        console.log(`RawrZ Security Platform v${packageJson.version}`);
        process.exit(0);
    }
    
    if (args.includes('--status')) {
        // Show status without starting
        console.log('RawrZ Security Platform - Status Check');
        console.log('System not started. Use without --status to start the system.');
        process.exit(0);
    }
    
    // Start the system
    main().catch(error => {
        console.error('Fatal error:', error.message);
        process.exit(1);
    });
}

module.exports = {
    main,
    safeStartup
};

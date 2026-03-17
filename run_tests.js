const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

// Test configuration
const config = {
    testDir: path.join(__dirname, 'tests'),
    fixturesDir: path.join(__dirname, 'tests', 'fixtures'),
    outputDir: path.join(__dirname, 'tests', 'output'),
    testExecutable: path.join(__dirname, 'tests', 'test_marketplace_installer.exe')
};

// ANSI color codes
const colors = {
    reset: '\x1b[0m',
    green: '\x1b[32m',
    red: '\x1b[31m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    cyan: '\x1b[36m'
};

// Create test directories
function setupTestEnvironment() {
    console.log(`${colors.cyan}Setting up test environment...${colors.reset}`);
    
    if (!fs.existsSync(config.fixturesDir)) {
        fs.mkdirSync(config.fixturesDir, { recursive: true });
        console.log(`  ✓ Created fixtures directory`);
    }
    
    if (!fs.existsSync(config.outputDir)) {
        fs.mkdirSync(config.outputDir, { recursive: true });
        console.log(`  ✓ Created output directory`);
    }
}

// Build test suite
function buildTests() {
    return new Promise((resolve, reject) => {
        console.log(`\n${colors.cyan}Building test suite...${colors.reset}`);
        
        const build = spawn('cmd', ['/c', 'build_tests.bat'], {
            cwd: __dirname,
            stdio: 'inherit'
        });
        
        build.on('close', (code) => {
            if (code === 0) {
                console.log(`${colors.green}✓ Build successful${colors.reset}`);
                resolve();
            } else {
                console.log(`${colors.red}✗ Build failed with code ${code}${colors.reset}`);
                reject(new Error(`Build failed with code ${code}`));
            }
        });
    });
}

// Run tests
function runTests() {
    return new Promise((resolve, reject) => {
        console.log(`\n${colors.cyan}Running tests...${colors.reset}\n`);
        
        if (!fs.existsSync(config.testExecutable)) {
            reject(new Error('Test executable not found. Build first.'));
            return;
        }
        
        const test = spawn(config.testExecutable, [], {
            cwd: config.testDir,
            stdio: 'inherit'
        });
        
        test.on('close', (code) => {
            if (code === 0) {
                console.log(`\n${colors.green}✓ All tests passed${colors.reset}`);
                resolve();
            } else {
                console.log(`\n${colors.red}✗ Some tests failed${colors.reset}`);
                reject(new Error(`Tests failed with code ${code}`));
            }
        });
    });
}

// Cleanup test artifacts
function cleanup() {
    console.log(`\n${colors.cyan}Cleaning up...${colors.reset}`);
    
    try {
        if (fs.existsSync(config.outputDir)) {
            fs.rmSync(config.outputDir, { recursive: true, force: true });
            console.log(`  ✓ Cleaned output directory`);
        }
    } catch (error) {
        console.log(`  ${colors.yellow}⚠ Cleanup warning: ${error.message}${colors.reset}`);
    }
}

// Generate test report
function generateReport(results) {
    const reportPath = path.join(config.testDir, 'test-report.json');
    
    const report = {
        timestamp: new Date().toISOString(),
        summary: {
            total: results.total || 0,
            passed: results.passed || 0,
            failed: results.failed || 0,
            duration: results.duration || 0
        },
        tests: results.tests || []
    };
    
    fs.writeFileSync(reportPath, JSON.stringify(report, null, 2));
    console.log(`\n${colors.cyan}Test report saved to: ${reportPath}${colors.reset}`);
}

// Main test runner
async function main() {
    console.log(`${colors.blue}╔════════════════════════════════════════════╗${colors.reset}`);
    console.log(`${colors.blue}║  RawrXD Marketplace Installer Test Suite  ║${colors.reset}`);
    console.log(`${colors.blue}╚════════════════════════════════════════════╝${colors.reset}\n`);
    
    const startTime = Date.now();
    
    try {
        // Setup
        setupTestEnvironment();
        
        // Build
        await buildTests();
        
        // Run
        await runTests();
        
        const duration = Date.now() - startTime;
        console.log(`\n${colors.cyan}Total execution time: ${duration}ms${colors.reset}`);
        
        process.exit(0);
    } catch (error) {
        console.error(`\n${colors.red}Error: ${error.message}${colors.reset}`);
        process.exit(1);
    }
}

// Parse command line arguments
const args = process.argv.slice(2);

if (args.includes('--clean')) {
    cleanup();
    process.exit(0);
}

if (args.includes('--build-only')) {
    setupTestEnvironment();
    buildTests().then(() => process.exit(0)).catch(() => process.exit(1));
} else {
    main();
}

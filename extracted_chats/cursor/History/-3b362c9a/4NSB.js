// Quick Test - Safe browser test without breaking anything
const puppeteer = require('puppeteer-extra');
const StealthPlugin = require('puppeteer-extra-plugin-stealth');
const fs = require('fs-extra');
const path = require('path');
const chalk = require('chalk');

puppeteer.use(StealthPlugin());

async function quickTest() {
    console.log(chalk.blue.bold('=== QUICK BROWSER TEST ==='));
    console.log(chalk.yellow('Testing browser agent without modifying existing files\n'));
    
    try {
        // Create test download directory
        const testDownloadPath = path.join(__dirname, 'test-downloads');
        await fs.ensureDir(testDownloadPath);
        
        console.log(chalk.blue('Launching browser...'));
        const browser = await puppeteer.launch({
            headless: false, // Set to true for production
            defaultViewport: null,
            args: ['--disable-blink-features=AutomationControlled'],
            userDataDir: './chrome-agent-cache-test'
        });
        
        const [page] = await browser.pages();
        
        // Configure download
        await page._client.send('Page.setDownloadBehavior', {
            behavior: 'allow',
            downloadPath: testDownloadPath
        });
        
        console.log(chalk.blue('Navigating to test page...'));
        await page.goto('https://example.com', { waitUntil: 'networkidle2' });
        
        console.log(chalk.blue('Creating test download...'));
        await page.evaluate(() => {
            const a = document.createElement('a');
            a.href = 'data:text/plain;charset=utf-8,agent-test-' + Date.now();
            a.download = 'agent-test.txt';
            a.click();
        });
        
        // Wait for download
        await page.waitForTimeout(2000);
        
        // Check if download worked
        const files = await fs.readdir(testDownloadPath);
        
        if (files.length > 0) {
            console.log(chalk.green('✅ Browser agent test PASSED!'));
            console.log(chalk.blue(`Download folder: ${testDownloadPath}`));
            console.log(chalk.blue(`Files downloaded: ${files.length}`));
        } else {
            console.log(chalk.red('❌ Browser agent test FAILED - No files downloaded'));
        }
        
        await browser.close();
        
        // Clean up test files (only test files, not existing ones)
        await fs.remove(testDownloadPath);
        await fs.remove('./chrome-agent-cache-test');
        
        console.log(chalk.green('✅ Test cleanup completed'));
        
    } catch (error) {
        console.error(chalk.red.bold('❌ Quick test failed:'), error.message);
        
        // Clean up on error
        try {
            await fs.remove('./chrome-agent-cache-test');
        } catch (cleanupError) {
            // Ignore cleanup errors
        }
    }
}

// Run quick test
if (require.main === module) {
    quickTest().catch(error => {
        console.error(chalk.red.bold(`[FATAL] ${error.message}`));
        process.exit(1);
    });
}

module.exports = quickTest;

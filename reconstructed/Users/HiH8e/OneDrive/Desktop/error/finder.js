const puppeteer = require('puppeteer');
const path = require('path');

async function findConsoleErrors() {
    const htmlFilePath = path.resolve('c:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html');
    const fileUrl = `file://${htmlFilePath}`;

    const browser = await puppeteer.launch({ headless: true });
    const page = await browser.newPage();

    const errors = [];

    // Listen for console messages
    page.on('console', msg => {
        const text = msg.text();
        if (text.includes('❌') || text.includes('⚠️') || text.includes('error') || text.includes('Error')) {
            errors.push(text);
        }
    });

    // Listen for page errors
    page.on('pageerror', error => {
        errors.push(`Page Error: ${error.message}`);
    });

    try {
        await page.goto(fileUrl, { waitUntil: 'networkidle0', timeout: 60000 });
        // Wait a bit for scripts to execute
        await page.waitForTimeout(10000);
    } catch (e) {
        console.error('Failed to load page:', e.message);
    }

    await browser.close();

    console.log('Captured Errors:');
    errors.forEach(error => console.log(error));
}

findConsoleErrors().catch(console.error);
const fs = require('fs-extra');
const path = require('path');
const chalk = require('chalk');
const { execSync } = require('child_process');

class PuppeteerAgentInstaller {
    constructor() {
        this.projectPath = __dirname;
        this.vscodeSnippetsPath = path.join(process.env.APPDATA, 'Code', 'User', 'snippets');
    }

    async install() {
        console.log(chalk.blue.bold('=== PUPPETEER AGENTIC BROWSER INSTALLER ==='));
        console.log(chalk.yellow('Setting up multi-agent system for web automation and bulk processing\n'));

        try {
            // Step 1: Install dependencies
            await this.installDependencies();
            
            // Step 2: Create directories
            await this.createDirectories();
            
            // Step 3: Setup VS Code snippets
            await this.setupVSCodeSnippets();
            
            // Step 4: Create environment file
            await this.createEnvironmentFile();
            
            // Step 5: Test installation
            await this.testInstallation();
            
            console.log(chalk.green.bold('\n[SUCCESS] Installation completed successfully!'));
            this.showUsageInstructions();
            
        } catch (error) {
            console.error(chalk.red.bold(`[ERROR] Installation failed: ${error.message}`));
            process.exit(1);
        }
    }

    async installDependencies() {
        console.log(chalk.blue('[INSTALL] Installing dependencies...'));
        
        const dependencies = [
            'puppeteer@^21.6.1',
            'puppeteer-extra@^3.3.6',
            'puppeteer-extra-plugin-stealth@^2.11.2',
            'puppeteer-extra-plugin-adblocker@^2.13.6',
            'fs-extra@^11.2.0',
            'mime-types@^2.1.35',
            'chalk@^4.1.2',
            'ora@^5.4.1',
            'clipboardy@^2.3.0',
            'nodemon@^3.0.2'
        ];

        try {
            execSync(`npm install ${dependencies.join(' ')}`, { 
                cwd: this.projectPath,
                stdio: 'inherit' 
            });
            console.log(chalk.green('[SUCCESS] Dependencies installed'));
        } catch (error) {
            console.log(chalk.yellow('[WARNING] Some dependencies may have failed to install'));
            console.log(chalk.gray('You can install them manually with: npm install'));
        }
    }

    async createDirectories() {
        console.log(chalk.blue('[INSTALL] Creating directory structure...'));
        
        const directories = [
            'downloads',
            'downloads/by-type',
            'downloads/by-type/images',
            'downloads/by-type/documents',
            'downloads/by-type/videos',
            'downloads/by-type/audio',
            'downloads/by-type/archives',
            'downloads/by-type/applications',
            'bulk-text-output',
            'bulk-text-output/organized',
            'bulk-text-output/organized/code',
            'bulk-text-output/organized/data',
            'bulk-text-output/organized/logs',
            'bulk-text-output/organized/text',
            'logs'
        ];

        for (const dir of directories) {
            await fs.ensureDir(path.join(this.projectPath, dir));
        }
        
        console.log(chalk.green('[SUCCESS] Directories created'));
    }

    async setupVSCodeSnippets() {
        console.log(chalk.blue('[INSTALL] Setting up VS Code snippets...'));
        
        try {
            // Ensure snippets directory exists
            await fs.ensureDir(this.vscodeSnippetsPath);
            
            // Copy snippets file
            const snippetsSource = path.join(this.projectPath, 'vscode-snippets.json');
            const snippetsDest = path.join(this.vscodeSnippetsPath, 'global.code-snippets');
            
            if (await fs.pathExists(snippetsSource)) {
                await fs.copy(snippetsSource, snippetsDest);
                console.log(chalk.green('[SUCCESS] VS Code snippets installed'));
                console.log(chalk.blue('Snippets location:'), snippetsDest);
            } else {
                console.log(chalk.yellow('[WARNING] Snippets file not found'));
            }
        } catch (error) {
            console.log(chalk.yellow('[WARNING] Could not install VS Code snippets automatically'));
            console.log(chalk.gray('You can manually copy vscode-snippets.json to your VS Code snippets directory'));
        }
    }

    async createEnvironmentFile() {
        console.log(chalk.blue('[INSTALL] Creating environment file...'));
        
        const envContent = `# Puppeteer Agentic Browser Environment Variables
# Add your credentials here for web subscription automation

# Example service credentials
YOUR_USERNAME=your_username_here
YOUR_PASSWORD=your_password_here
ANOTHER_USERNAME=another_username_here
ANOTHER_PASSWORD=another_password_here

# Browser settings
BROWSER_HEADLESS=false
BROWSER_TIMEOUT=30000
DOWNLOAD_TIMEOUT=60000

# Text processing settings
CHUNK_SIZE=10000
MAX_FILE_SIZE=52428800
`;

        const envPath = path.join(this.projectPath, '.env');
        await fs.writeFile(envPath, envContent);
        
        console.log(chalk.green('[SUCCESS] Environment file created'));
        console.log(chalk.blue('Edit .env file to add your credentials'));
    }

    async testInstallation() {
        console.log(chalk.blue('[INSTALL] Testing installation...'));
        
        try {
            // Test if main modules can be required
            require('./src/agent');
            require('./src/text-processor');
            require('./src/bulk-handler');
            require('./src/enhanced-orchestrator');
            
            console.log(chalk.green('[SUCCESS] All modules loaded successfully'));
        } catch (error) {
            console.log(chalk.yellow('[WARNING] Some modules failed to load'));
            console.log(chalk.gray('This may be due to missing dependencies'));
        }
    }

    showUsageInstructions() {
        console.log(chalk.cyan.bold('\n=== USAGE INSTRUCTIONS ==='));
        console.log(chalk.white('\n1. VS Code Snippets:'));
        console.log(chalk.gray('   - Type "bigg" + Tab → BigDaddyG Orchestrator Panel'));
        console.log(chalk.gray('   - Type "puppet" + Tab → Puppeteer Agentic Browser'));
        console.log(chalk.gray('   - Type "bulk" + Tab → Bulk Text Processor'));
        console.log(chalk.gray('   - Press Ctrl+F5 to run'));
        
        console.log(chalk.white('\n2. Command Line:'));
        console.log(chalk.gray('   node launcher.js          # Interactive menu'));
        console.log(chalk.gray('   node src/main.js          # Web automation'));
        console.log(chalk.gray('   node src/run-bulk.js      # Bulk text processing'));
        
        console.log(chalk.white('\n3. Configuration:'));
        console.log(chalk.gray('   - Edit .env file for credentials'));
        console.log(chalk.gray('   - Edit src/config.js for web subscriptions'));
        
        console.log(chalk.white('\n4. Directories:'));
        console.log(chalk.gray('   - downloads/              # Web downloads'));
        console.log(chalk.gray('   - bulk-text-output/       # Processed text'));
        console.log(chalk.gray('   - logs/                   # System logs'));
        
        console.log(chalk.cyan.bold('\n=== QUICK START ==='));
        console.log(chalk.yellow('1. Edit .env file with your credentials'));
        console.log(chalk.yellow('2. Run: node launcher.js'));
        console.log(chalk.yellow('3. Choose your operation from the menu'));
        console.log(chalk.yellow('4. Or use VS Code snippets for quick access'));
        
        console.log(chalk.green.bold('\nInstallation complete! Happy automating!'));
    }
}

// Run installer
if (require.main === module) {
    const installer = new PuppeteerAgentInstaller();
    installer.install().catch(error => {
        console.error(chalk.red.bold(`[FATAL] ${error.message}`));
        process.exit(1);
    });
}

module.exports = PuppeteerAgentInstaller;

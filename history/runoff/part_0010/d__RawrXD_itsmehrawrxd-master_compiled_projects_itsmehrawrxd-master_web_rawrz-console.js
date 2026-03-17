#!/usr/bin/env node
/**
 * RawrZ Professional Console Interface
 * Interactive menu system for the ultimate security platform
 * Integrates separated encryption workflow, multi-language stubs, and Windows formats
 */

const readline = require('readline');
const fs = require('fs');
const path = require('path');
const { RawrZUltimate, SeparatedEncryptor, WindowsFormatEngine, MultiLanguageStubGenerator } = require('./rawrz-ultimate.js');

class RawrZConsole {
    constructor() {
        this.ultimate = new RawrZUltimate();
        this.separatedEncryptor = new SeparatedEncryptor();
        this.rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });
        
        this.sessionData = {
            lastEncrypted: null,
            workingDirectory: './rawrz_output',
            burnCounter: 0,
            reuseStubs: []
        };

        this.ensureWorkingDirectory();
    }

    ensureWorkingDirectory() {
        if (!fs.existsSync(this.sessionData.workingDirectory)) {
            fs.mkdirSync(this.sessionData.workingDirectory, { recursive: true });
        }
    }

    async start() {
        console.clear();
        this.showBanner();
        await this.showMainMenu();
    }

    showBanner() {
        console.log('\n🔥═══════════════════════════════════════════════════════════════🔥');
        console.log('██████╗  █████╗ ██╗    ██╗██████╗ ███████╗    ██╗   ██╗██████╗ ');
        console.log('██╔══██╗██╔══██╗██║    ██║██╔══██╗╚══███╔╝    ██║   ██║╚════██╗');
        console.log('██████╔╝███████║██║ █╗ ██║██████╔╝  ███╔╝     ██║   ██║ █████╔╝');
        console.log('██╔══██╗██╔══██║██║███╗██║██╔══██╗ ███╔╝      ╚██╗ ██╔╝ ╚═══██╗');
        console.log('██║  ██║██║  ██║╚███╔███╔╝██║  ██║███████╗    ╚████╔╝ ██████╔╝');
        console.log('╚═╝  ╚═╝╚═╝  ╚═╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚══════╝     ╚═══╝  ╚═════╝ ');
        console.log('🔥═══════════════════════════════════════════════════════════════🔥');
        console.log('        Ultimate Security Platform - Professional Console        ');
        console.log('   🔐 Separated Encryption Workflow | 🪟 30+ Windows Formats     ');
        console.log('   💻 15+ Languages | 🛡️ Anti-Analysis | 🎯 Polymorphic Stubs    ');
        console.log('🔥═══════════════════════════════════════════════════════════════🔥\n');
    }

    async showMainMenu() {
        console.log('🎯 Main Menu - Select Operation:');
        console.log('');
        console.log('[SEPARATED ENCRYPTION WORKFLOW]');
        console.log('  1. 🔐 Encrypt Only (No Stub Generation)');
        console.log('  2. 🏗️  Generate Language Stubs');
        console.log('  3. 🪟 Generate Windows Formats');
        console.log('  4. 📊 Generate Complete Payload Matrix');
        console.log('');
        console.log('[BURN & REUSE STRATEGY]');
        console.log('  5. 🔥 Burn & Test Stubs');
        console.log('  6. ♻️  Reuse Validated Stubs');
        console.log('  7. 📈 Session Statistics');
        console.log('');
        console.log('[INFORMATION & ANALYSIS]');
        console.log('  8. 📋 List Supported Languages');
        console.log('  9. 📋 List Windows Formats');
        console.log(' 10. 📋 List Encryption Methods');
        console.log(' 11. 🔍 Analyze File');
        console.log('');
        console.log('[ORIGINAL FEATURES]');
        console.log(' 12. 🛠️  Traditional Encrypt + Stub');
        console.log(' 13. 🔓 Decrypt Files');
        console.log(' 14. 🔐 Hash & Crypto Operations');
        console.log(' 15. 🌐 Network Operations');
        console.log(' 16. 📁 File Operations');
        console.log('');
        console.log('[SYSTEM]');
        console.log(' 17. ⚙️  Settings & Configuration');
        console.log(' 18. 💾 Export Session');
        console.log(' 19. 📚 Show Full Help');
        console.log('  0. ❌ Exit');
        console.log('');

        const choice = await this.prompt('Select option (0-19): ');
        await this.handleMenuChoice(choice);
    }

    async handleMenuChoice(choice) {
        switch (choice) {
            case '1':
                await this.handleEncryptOnlyMenu();
                break;
            case '2':
                await this.handleGenerateStubsMenu();
                break;
            case '3':
                await this.handleGenerateFormatsMenu();
                break;
            case '4':
                await this.handlePayloadMatrixMenu();
                break;
            case '5':
                await this.handleBurnTestMenu();
                break;
            case '6':
                await this.handleReuseMenu();
                break;
            case '7':
                await this.showSessionStatistics();
                break;
            case '8':
                await this.showLanguages();
                break;
            case '9':
                await this.showFormats();
                break;
            case '10':
                await this.showEncryptionMethods();
                break;
            case '11':
                await this.handleAnalyzeFile();
                break;
            case '12':
                await this.handleTraditionalEncrypt();
                break;
            case '13':
                await this.handleDecrypt();
                break;
            case '14':
                await this.handleHashCrypto();
                break;
            case '15':
                await this.handleNetworkOps();
                break;
            case '16':
                await this.handleFileOps();
                break;
            case '17':
                await this.handleSettings();
                break;
            case '18':
                await this.exportSession();
                break;
            case '19':
                this.ultimate.showUltimateHelp();
                await this.continueAfterOutput();
                break;
            case '0':
                await this.exit();
                return;
            default:
                console.log('❌ Invalid option. Please try again.');
                await this.continueAfterOutput();
        }

        await this.showMainMenu();
    }

    async handleEncryptOnlyMenu() {
        console.log('\n🔐 Encrypt Only - Separated Workflow');
        console.log('Available methods:', this.separatedEncryptor.listEncryptionMethods().join(', '));
        console.log('');

        const inputFile = await this.prompt('Enter input file path: ');
        if (!inputFile) return;

        const method = await this.prompt('Select encryption method (aes256/chacha20/camellia/hybrid): ');
        if (!this.separatedEncryptor.listEncryptionMethods().includes(method.toLowerCase())) {
            console.log('❌ Invalid encryption method.');
            return;
        }

        const outputName = await this.prompt('Enter output name (without extension): ') || 'encrypted_payload';
        
        try {
            console.log('\n🔄 Processing...');
            
            let inputData;
            if (inputFile.startsWith('http://') || inputFile.startsWith('https://')) {
                console.log('📥 Downloading from URL...');
                inputData = await this.ultimate.downloadFile(inputFile);
            } else if (fs.existsSync(inputFile)) {
                inputData = fs.readFileSync(inputFile);
            } else {
                console.log('❌ Input file not found.');
                return;
            }

            const result = await this.separatedEncryptor.encryptOnly(inputData, method);
            
            const outputFile = path.join(this.sessionData.workingDirectory, `${outputName}.bin`);
            const keyFile = path.join(this.sessionData.workingDirectory, `${outputName}.key`);
            const infoFile = path.join(this.sessionData.workingDirectory, `${outputName}.info`);
            
            fs.writeFileSync(outputFile, result.encryptedData);
            fs.writeFileSync(keyFile, result.key);
            fs.writeFileSync(infoFile, JSON.stringify({
                method: result.method,
                originalSize: inputData.length,
                encryptedSize: result.encryptedData.length,
                timestamp: new Date().toISOString(),
                inputFile: inputFile
            }, null, 2));
            
            this.sessionData.lastEncrypted = outputFile;
            
            console.log('\n✅ Encryption completed successfully!');
            console.log(`📁 Encrypted file: ${outputFile}`);
            console.log(`🔑 Key file: ${keyFile}`);
            console.log(`📊 Info file: ${infoFile}`);
            console.log(`💾 Size: ${inputData.length} → ${result.encryptedData.length} bytes`);
            
        } catch (error) {
            console.log(`❌ Encryption failed: ${error.message}`);
        }

        await this.continueAfterOutput();
    }

    async handleGenerateStubsMenu() {
        console.log('\n🏗️ Generate Language Stubs');
        
        if (!this.sessionData.lastEncrypted) {
            const encryptedFile = await this.prompt('Enter encrypted file path: ');
            if (!fs.existsSync(encryptedFile)) {
                console.log('❌ Encrypted file not found.');
                return;
            }
            this.sessionData.lastEncrypted = encryptedFile;
        }

        console.log(`📁 Using encrypted file: ${this.sessionData.lastEncrypted}`);
        console.log('Available languages:', this.separatedEncryptor.listLanguages().join(', '));
        console.log('');

        const languagesInput = await this.prompt('Enter languages (comma-separated or "all"): ');
        if (!languagesInput) return;

        const languages = languagesInput.toLowerCase() === 'all' ? 
            this.separatedEncryptor.listLanguages() :
            languagesInput.split(',').map(l => l.trim());

        const advanced = await this.prompt('Use advanced anti-analysis? (y/n): ') === 'y';
        const outputDir = path.join(this.sessionData.workingDirectory, 'stubs');

        try {
            console.log('\n🔄 Generating stubs...');
            
            const encryptedData = fs.readFileSync(this.sessionData.lastEncrypted);
            const stubs = await this.separatedEncryptor.generateLanguageStubs(encryptedData, languages, {
                advanced: advanced,
                antiAnalysis: advanced ? ['sleep_evasion', 'vm_detection', 'debugger_detection'] : []
            });

            if (!fs.existsSync(outputDir)) {
                fs.mkdirSync(outputDir, { recursive: true });
            }

            const stubFiles = [];
            for (const [language, stub] of Object.entries(stubs)) {
                const filename = path.join(outputDir, `stub_${language}${stub.extension}`);
                fs.writeFileSync(filename, stub.code);
                stubFiles.push(filename);
                console.log(`📄 Generated: ${language} → ${filename}`);
            }

            // Save stub metadata
            const metadataFile = path.join(outputDir, 'stubs_metadata.json');
            fs.writeFileSync(metadataFile, JSON.stringify({
                timestamp: new Date().toISOString(),
                encryptedFile: this.sessionData.lastEncrypted,
                languages: languages,
                advanced: advanced,
                stubCount: stubFiles.length,
                stubFiles: stubFiles
            }, null, 2));

            console.log(`\n✅ Generated ${stubFiles.length} stub files in ${outputDir}`);
            console.log(`📊 Metadata saved: ${metadataFile}`);
            
        } catch (error) {
            console.log(`❌ Stub generation failed: ${error.message}`);
        }

        await this.continueAfterOutput();
    }

    async handleGenerateFormatsMenu() {
        console.log('\n🪟 Generate Windows Formats');
        
        if (!this.sessionData.lastEncrypted) {
            const encryptedFile = await this.prompt('Enter encrypted file path: ');
            if (!fs.existsSync(encryptedFile)) {
                console.log('❌ Encrypted file not found.');
                return;
            }
            this.sessionData.lastEncrypted = encryptedFile;
        }

        console.log(`📁 Using encrypted file: ${this.sessionData.lastEncrypted}`);
        console.log('Available formats:', this.separatedEncryptor.listWindowsFormats().join(', '));
        console.log('');

        const formatsInput = await this.prompt('Enter formats (comma-separated or "all"): ');
        if (!formatsInput) return;

        const formats = formatsInput.toLowerCase() === 'all' ? 
            this.separatedEncryptor.listWindowsFormats() :
            formatsInput.split(',').map(f => f.trim());

        const outputDir = path.join(this.sessionData.workingDirectory, 'formats');

        try {
            console.log('\n🔄 Generating formats...');
            
            const encryptedData = fs.readFileSync(this.sessionData.lastEncrypted);
            const formatted = await this.separatedEncryptor.generateWindowsFormats(encryptedData, formats);

            if (!fs.existsSync(outputDir)) {
                fs.mkdirSync(outputDir, { recursive: true });
            }

            const formatFiles = [];
            for (const [format, output] of Object.entries(formatted)) {
                const filename = path.join(outputDir, `payload_${format}${output.extension}`);
                
                if (output.type === 'binary' && Buffer.isBuffer(output.content)) {
                    fs.writeFileSync(filename, output.content);
                } else {
                    fs.writeFileSync(filename, output.content.toString());
                }
                
                formatFiles.push(filename);
                console.log(`📄 Generated: ${format} → ${filename}`);
                
                if (output.instructions) {
                    console.log(`   ℹ️  Instructions: ${output.instructions}`);
                }
            }

            // Save format metadata
            const metadataFile = path.join(outputDir, 'formats_metadata.json');
            fs.writeFileSync(metadataFile, JSON.stringify({
                timestamp: new Date().toISOString(),
                encryptedFile: this.sessionData.lastEncrypted,
                formats: formats,
                formatCount: formatFiles.length,
                formatFiles: formatFiles
            }, null, 2));

            console.log(`\n✅ Generated ${formatFiles.length} format files in ${outputDir}`);
            console.log(`📊 Metadata saved: ${metadataFile}`);
            
        } catch (error) {
            console.log(`❌ Format generation failed: ${error.message}`);
        }

        await this.continueAfterOutput();
    }

    async handlePayloadMatrixMenu() {
        console.log('\n📊 Generate Complete Payload Matrix');
        
        if (!this.sessionData.lastEncrypted) {
            const encryptedFile = await this.prompt('Enter encrypted file path: ');
            if (!fs.existsSync(encryptedFile)) {
                console.log('❌ Encrypted file not found.');
                return;
            }
            this.sessionData.lastEncrypted = encryptedFile;
        }

        console.log(`📁 Using encrypted file: ${this.sessionData.lastEncrypted}`);
        console.log('');

        const languagesInput = await this.prompt('Enter languages (comma-separated or "all"): ');
        if (!languagesInput) return;

        const formatsInput = await this.prompt('Enter formats (comma-separated or "all"): ');
        if (!formatsInput) return;

        const languages = languagesInput.toLowerCase() === 'all' ? 
            this.separatedEncryptor.listLanguages() :
            languagesInput.split(',').map(l => l.trim());
            
        const formats = formatsInput.toLowerCase() === 'all' ? 
            this.separatedEncryptor.listWindowsFormats() :
            formatsInput.split(',').map(f => f.trim());

        const totalCombinations = languages.length * formats.length;
        
        console.log(`\n⚠️  This will generate ${totalCombinations} combinations!`);
        console.log(`   Languages: ${languages.length}`);
        console.log(`   Formats: ${formats.length}`);
        
        const confirm = await this.prompt('Continue? (y/n): ');
        if (confirm.toLowerCase() !== 'y') return;

        const outputDir = path.join(this.sessionData.workingDirectory, 'matrix');

        try {
            console.log('\n🔄 Generating payload matrix...');
            
            const encryptedData = fs.readFileSync(this.sessionData.lastEncrypted);
            const matrix = await this.separatedEncryptor.generatePayloadMatrix(
                encryptedData, 
                languages, 
                formats, 
                { advanced: true }
            );

            if (!fs.existsSync(outputDir)) {
                fs.mkdirSync(outputDir, { recursive: true });
            }

            // Save matrix files and generate report
            let successCount = 0;
            const report = {
                timestamp: new Date().toISOString(),
                encryptedFile: this.sessionData.lastEncrypted,
                languages: languages,
                formats: formats,
                totalCombinations: totalCombinations,
                matrix: {}
            };

            for (const [language, langData] of Object.entries(matrix)) {
                const langDir = path.join(outputDir, language);
                if (!fs.existsSync(langDir)) {
                    fs.mkdirSync(langDir, { recursive: true });
                }

                report.matrix[language] = {};

                for (const [format, data] of Object.entries(langData)) {
                    const filename = path.join(langDir, `${data.combination}${data.format.extension}`);
                    
                    if (data.format.type === 'binary' && Buffer.isBuffer(data.format.content)) {
                        fs.writeFileSync(filename, data.format.content);
                    } else {
                        fs.writeFileSync(filename, data.format.content.toString());
                    }

                    report.matrix[language][format] = {
                        file: filename,
                        size: data.size,
                        type: data.format.type
                    };
                    
                    successCount++;
                }
            }

            const reportFile = path.join(outputDir, 'matrix_report.json');
            fs.writeFileSync(reportFile, JSON.stringify(report, null, 2));

            console.log(`\n✅ Matrix generation completed!`);
            console.log(`📊 Success: ${successCount}/${totalCombinations} combinations`);
            console.log(`📁 Output directory: ${outputDir}`);
            console.log(`📋 Report: ${reportFile}`);
            
        } catch (error) {
            console.log(`❌ Matrix generation failed: ${error.message}`);
        }

        await this.continueAfterOutput();
    }

    async handleBurnTestMenu() {
        console.log('\n🔥 Burn & Test Stubs - Burn & Reuse Strategy');
        console.log('This implements the burn 2-3, use 4th, reuse strategy');
        console.log('');

        const stubsDir = path.join(this.sessionData.workingDirectory, 'stubs');
        if (!fs.existsSync(stubsDir)) {
            console.log('❌ No stubs found. Generate stubs first.');
            return;
        }

        const stubFiles = fs.readdirSync(stubsDir).filter(f => !f.endsWith('.json'));
        if (stubFiles.length === 0) {
            console.log('❌ No stub files found.');
            return;
        }

        console.log(`📁 Found ${stubFiles.length} stub files:`);
        stubFiles.forEach((file, i) => {
            console.log(`  ${i+1}. ${file}`);
        });
        console.log('');

        const burnCount = Math.min(3, stubFiles.length - 1);
        console.log(`🔥 Strategy: Burn ${burnCount} stubs for testing, keep rest for production`);
        
        const confirm = await this.prompt(`Burn ${burnCount} stubs? (y/n): `);
        if (confirm.toLowerCase() !== 'y') return;

        // Burn stubs (simulate testing)
        const burnedStubs = [];
        for (let i = 0; i < burnCount; i++) {
            const stubFile = stubFiles[i];
            burnedStubs.push(stubFile);
            console.log(`🔥 Burned: ${stubFile} (Testing ${i+1}/${burnCount})`);
            this.sessionData.burnCounter++;
        }

        // Keep remaining for reuse
        const keepStubs = stubFiles.slice(burnCount);
        this.sessionData.reuseStubs = keepStubs.map(stub => path.join(stubsDir, stub));
        
        console.log(`\n✅ Burn process completed:`);
        console.log(`🔥 Burned: ${burnedStubs.length} stubs`);
        console.log(`♻️  Kept for reuse: ${keepStubs.length} stubs`);
        console.log('');
        
        if (keepStubs.length > 0) {
            console.log('📋 Available for reuse:');
            keepStubs.forEach((stub, i) => {
                console.log(`  ${i+1}. ${stub}`);
            });
        }

        await this.continueAfterOutput();
    }

    async handleReuseMenu() {
        console.log('\n♻️ Reuse Validated Stubs');
        
        if (this.sessionData.reuseStubs.length === 0) {
            console.log('❌ No reusable stubs available. Run burn & test first.');
            return;
        }

        console.log(`📋 Available reusable stubs (${this.sessionData.reuseStubs.length}):`);
        this.sessionData.reuseStubs.forEach((stub, i) => {
            const size = fs.existsSync(stub) ? fs.statSync(stub).size : 0;
            console.log(`  ${i+1}. ${path.basename(stub)} (${size} bytes)`);
        });
        console.log('');

        const choice = await this.prompt('Select stub to reuse (1-N) or "all": ');
        if (!choice) return;

        const reuseDir = path.join(this.sessionData.workingDirectory, 'reused');
        if (!fs.existsSync(reuseDir)) {
            fs.mkdirSync(reuseDir, { recursive: true });
        }

        try {
            if (choice.toLowerCase() === 'all') {
                console.log('♻️ Copying all reusable stubs...');
                
                for (const stubPath of this.sessionData.reuseStubs) {
                    if (fs.existsSync(stubPath)) {
                        const filename = path.basename(stubPath);
                        const newPath = path.join(reuseDir, `reused_${filename}`);
                        fs.copyFileSync(stubPath, newPath);
                        console.log(`📄 Reused: ${filename} → ${newPath}`);
                    }
                }
                
            } else {
                const index = parseInt(choice) - 1;
                if (index >= 0 && index < this.sessionData.reuseStubs.length) {
                    const stubPath = this.sessionData.reuseStubs[index];
                    
                    if (fs.existsSync(stubPath)) {
                        const filename = path.basename(stubPath);
                        const newPath = path.join(reuseDir, `reused_${filename}`);
                        fs.copyFileSync(stubPath, newPath);
                        console.log(`📄 Reused: ${filename} → ${newPath}`);
                    } else {
                        console.log('❌ Stub file not found.');
                        return;
                    }
                } else {
                    console.log('❌ Invalid selection.');
                    return;
                }
            }

            console.log(`\n✅ Stubs reused successfully in ${reuseDir}`);
            
        } catch (error) {
            console.log(`❌ Reuse failed: ${error.message}`);
        }

        await this.continueAfterOutput();
    }

    async showSessionStatistics() {
        console.log('\n📈 Session Statistics');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        
        console.log(`📁 Working Directory: ${this.sessionData.workingDirectory}`);
        console.log(`🔐 Last Encrypted: ${this.sessionData.lastEncrypted || 'None'}`);
        console.log(`🔥 Stubs Burned: ${this.sessionData.burnCounter}`);
        console.log(`♻️  Reusable Stubs: ${this.sessionData.reuseStubs.length}`);
        console.log('');

        // Count files in working directory
        const workingDirExists = fs.existsSync(this.sessionData.workingDirectory);
        if (workingDirExists) {
            const allFiles = this.getAllFiles(this.sessionData.workingDirectory);
            const byType = {
                encrypted: allFiles.filter(f => f.endsWith('.bin')).length,
                keys: allFiles.filter(f => f.endsWith('.key')).length,
                stubs: allFiles.filter(f => f.includes('stub_')).length,
                formats: allFiles.filter(f => f.includes('payload_')).length,
                metadata: allFiles.filter(f => f.endsWith('.json')).length
            };

            console.log('📊 File Statistics:');
            console.log(`   Encrypted files: ${byType.encrypted}`);
            console.log(`   Key files: ${byType.keys}`);
            console.log(`   Stub files: ${byType.stubs}`);
            console.log(`   Format files: ${byType.formats}`);
            console.log(`   Metadata files: ${byType.metadata}`);
            console.log(`   Total files: ${allFiles.length}`);
        } else {
            console.log('📊 No files generated yet.');
        }

        console.log('');
        console.log('🎯 Platform Capabilities:');
        console.log(`   Languages: ${this.separatedEncryptor.listLanguages().length}`);
        console.log(`   Formats: ${this.separatedEncryptor.listWindowsFormats().length}`);
        console.log(`   Encryption Methods: ${this.separatedEncryptor.listEncryptionMethods().length}`);
        console.log(`   Max Combinations: ${this.separatedEncryptor.listLanguages().length * this.separatedEncryptor.listWindowsFormats().length}`);

        await this.continueAfterOutput();
    }

    getAllFiles(dir, files = []) {
        const entries = fs.readdirSync(dir);
        for (const entry of entries) {
            const fullPath = path.join(dir, entry);
            if (fs.statSync(fullPath).isDirectory()) {
                this.getAllFiles(fullPath, files);
            } else {
                files.push(fullPath);
            }
        }
        return files;
    }

    async showLanguages() {
        console.log('\n📋 Supported Programming Languages');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        
        const languages = this.separatedEncryptor.listLanguages();
        const compiled = [];
        const interpreted = [];

        languages.forEach(lang => {
            const info = this.multiLanguageStubGenerator.getLanguageInfo(lang);
            if (info.type === 'compiled') {
                compiled.push({lang, info});
            } else {
                interpreted.push({lang, info});
            }
        });

        console.log('\n🔧 COMPILED LANGUAGES:');
        compiled.forEach(({lang, info}, i) => {
            console.log(`  ${i+1}. ${lang} (${info.extension}) - Compiler: ${info.compiler}`);
        });

        console.log('\n⚡ INTERPRETED LANGUAGES:');
        interpreted.forEach(({lang, info}, i) => {
            console.log(`  ${i+1}. ${lang} (${info.extension}) - Interpreter: ${info.interpreter}`);
        });

        console.log(`\nTotal: ${languages.length} languages (${compiled.length} compiled, ${interpreted.length} interpreted)`);

        await this.continueAfterOutput();
    }

    async showFormats() {
        console.log('\n📋 Supported Windows Formats');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        
        const formats = this.separatedEncryptor.listWindowsFormats();
        const categories = {};
        
        formats.forEach(format => {
            const info = this.windowsFormatEngine.getFormatInfo(format);
            if (!categories[info.category]) categories[info.category] = [];
            categories[info.category].push({format, info});
        });

        Object.entries(categories).forEach(([category, items]) => {
            const icon = {
                'executable': '⚡',
                'script': '📜', 
                'office': '📊',
                'web': '🌐',
                'archive': '🗜️',
                'special': '🎯'
            }[category] || '📄';
            
            console.log(`\n${icon} ${category.toUpperCase()}:`);
            items.forEach(({format, info}, i) => {
                console.log(`  ${i+1}. ${format} (${info.extension}) - ${info.template}`);
            });
        });

        console.log(`\nTotal: ${formats.length} formats across ${Object.keys(categories).length} categories`);

        await this.continueAfterOutput();
    }

    async showEncryptionMethods() {
        console.log('\n📋 Supported Encryption Methods');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        
        const methods = [
            { name: 'aes256', desc: 'AES-256-GCM - Advanced Encryption Standard with authentication' },
            { name: 'chacha20', desc: 'ChaCha20-Poly1305 - Modern stream cipher with authentication' },
            { name: 'camellia', desc: 'Camellia-256-CBC - Japanese encryption standard' },
            { name: 'hybrid', desc: 'Multi-layer AES256 + ChaCha20 combination' }
        ];

        methods.forEach((method, i) => {
            console.log(`  ${i+1}. ${method.name.toUpperCase()}`);
            console.log(`     ${method.desc}`);
            console.log('');
        });

        await this.continueAfterOutput();
    }

    // Placeholder methods for original features
    async handleAnalyzeFile() {
        console.log('\n🔍 File Analysis - Coming soon in this interface');
        console.log('Use command line: node rawrz-ultimate.js analyze <file>');
        await this.continueAfterOutput();
    }

    async handleTraditionalEncrypt() {
        console.log('\n🛠️ Traditional Encrypt + Stub - Coming soon in this interface');
        console.log('Use command line: node rawrz-ultimate.js encrypt <algorithm> <file>');
        await this.continueAfterOutput();
    }

    async handleDecrypt() {
        console.log('\n🔓 Decrypt Files - Coming soon in this interface');
        console.log('Use command line: node rawrz-ultimate.js decrypt <algorithm> <file>');
        await this.continueAfterOutput();
    }

    async handleHashCrypto() {
        console.log('\n🔐 Hash & Crypto Operations - Coming soon in this interface');
        console.log('Use command line: node rawrz-ultimate.js hash <algorithm> <file>');
        await this.continueAfterOutput();
    }

    async handleNetworkOps() {
        console.log('\n🌐 Network Operations - Coming soon in this interface');
        console.log('Use command line: node rawrz-ultimate.js ping/dns/portscan <target>');
        await this.continueAfterOutput();
    }

    async handleFileOps() {
        console.log('\n📁 File Operations - Coming soon in this interface'); 
        console.log('Use command line: node rawrz-ultimate.js fileops <operation> <file>');
        await this.continueAfterOutput();
    }

    async handleSettings() {
        console.log('\n⚙️ Settings & Configuration');
        console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
        console.log(`📁 Working Directory: ${this.sessionData.workingDirectory}`);
        console.log('');
        
        const newDir = await this.prompt('Enter new working directory (or press Enter to keep current): ');
        if (newDir && newDir.trim()) {
            this.sessionData.workingDirectory = newDir.trim();
            this.ensureWorkingDirectory();
            console.log(`✅ Working directory updated to: ${this.sessionData.workingDirectory}`);
        }

        await this.continueAfterOutput();
    }

    async exportSession() {
        console.log('\n💾 Export Session');
        
        const exportData = {
            timestamp: new Date().toISOString(),
            sessionData: this.sessionData,
            workingDirectory: this.sessionData.workingDirectory,
            lastEncrypted: this.sessionData.lastEncrypted,
            burnCounter: this.sessionData.burnCounter,
            reuseStubs: this.sessionData.reuseStubs
        };

        const exportFile = path.join(this.sessionData.workingDirectory, `session_${Date.now()}.json`);
        fs.writeFileSync(exportFile, JSON.stringify(exportData, null, 2));
        
        console.log(`✅ Session exported to: ${exportFile}`);
        await this.continueAfterOutput();
    }

    async continueAfterOutput() {
        await this.prompt('\nPress Enter to continue...');
        console.clear();
        this.showBanner();
    }

    async prompt(question) {
        return new Promise((resolve) => {
            this.rl.question(question, resolve);
        });
    }

    async exit() {
        console.log('\n🔥 Thank you for using RawrZ Ultimate Security Platform! 🔥');
        console.log('Stay secure! 🛡️');
        this.rl.close();
        process.exit(0);
    }
}

// Start the console interface
if (require.main === module) {
    const console_interface = new RawrZConsole();
    console_interface.start().catch(console.error);
}

module.exports = RawrZConsole;
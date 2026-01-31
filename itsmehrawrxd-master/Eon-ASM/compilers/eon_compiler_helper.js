/**
 * EON Compiler Helper
 * Integrates with your EON IDE to automatically fix syntax and test compilation
 */

const fs = require('fs');
const path = require('path');
const { EONSyntaxFixer, EONSyntaxTemplates } = require('./eon_syntax_fixer.js');

class EONCompilerHelper {
    constructor() {
        this.fixer = new EONSyntaxFixer();
        this.templates = new EONSyntaxTemplates();
        this.compilerEndpoint = 'http://localhost:3001/api/compile';
    }

    /**
     * Fix and compile an EON file
     * @param {string} filePath - Path to the EON file
     * @returns {Promise<Object>} - Compilation result
     */
    async fixAndCompile(filePath) {
        try {
            console.log(` Processing: ${filePath}`);
            
            // Read the file
            const originalContent = fs.readFileSync(filePath, 'utf8');
            
            // Fix syntax
            const fixResult = this.fixer.fix(originalContent);
            
            // Write fixed content back to file
            fs.writeFileSync(filePath, fixResult.fixed);
            
            console.log(` Fixed syntax issues:`);
            fixResult.fixes.forEach(fix => console.log(`   - ${fix}`));
            
            if (fixResult.warnings.length > 0) {
                console.log(`  Warnings:`);
                fixResult.warnings.forEach(warning => console.log(`   - ${warning}`));
            }
            
            // Test compilation via API
            const compileResult = await this.testCompilation(fixResult.fixed);
            
            return {
                filePath: filePath,
                original: originalContent,
                fixed: fixResult.fixed,
                fixes: fixResult.fixes,
                warnings: fixResult.warnings,
                compilation: compileResult
            };
            
        } catch (error) {
            console.error(` Error processing ${filePath}:`, error.message);
            return {
                filePath: filePath,
                error: error.message,
                success: false
            };
        }
    }

    /**
     * Test compilation via the EON IDE API
     * @param {string} sourceCode - EON source code to compile
     * @returns {Promise<Object>} - Compilation result
     */
    async testCompilation(sourceCode) {
        try {
            const response = await fetch(this.compilerEndpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    source: sourceCode,
                    filename: 'test.eon'
                })
            });
            
            const result = await response.json();
            
            return {
                success: response.ok,
                output: result.output || '',
                errors: result.errors || [],
                warnings: result.warnings || [],
                status: result.status || 'unknown'
            };
            
        } catch (error) {
            return {
                success: false,
                error: `API call failed: ${error.message}`,
                output: '',
                errors: [error.message],
                warnings: []
            };
        }
    }

    /**
     * Process all EON files in the current directory
     */
    async processAllEonFiles() {
        console.log(' Processing all EON files...\n');
        
        const eonFiles = this.findEonFiles('.');
        const results = [];
        
        for (const file of eonFiles) {
            const result = await this.fixAndCompile(file);
            results.push(result);
            console.log(''); // Add spacing between files
        }
        
        this.printSummary(results);
        return results;
    }

    /**
     * Find all .eon files in a directory
     * @param {string} dir - Directory to search
     * @returns {Array<string>} - Array of .eon file paths
     */
    findEonFiles(dir) {
        const files = [];
        
        const items = fs.readdirSync(dir);
        
        for (const item of items) {
            const fullPath = path.join(dir, item);
            const stat = fs.statSync(fullPath);
            
            if (stat.isDirectory() && !item.startsWith('.') && item !== 'node_modules') {
                files.push(...this.findEonFiles(fullPath));
            } else if (stat.isFile() && item.endsWith('.eon')) {
                files.push(fullPath);
            }
        }
        
        return files;
    }

    /**
     * Create a new EON file with correct syntax
     * @param {string} filename - Name of the file to create
     * @param {string} templateName - Template to use
     */
    createEonFile(filename, templateName = 'basic_main') {
        const template = this.templates.getTemplate(templateName);
        const fixedTemplate = this.fixer.fix(template);
        
        fs.writeFileSync(filename, fixedTemplate.fixed);
        
        console.log(` Created ${filename} with correct EON syntax`);
        console.log(` Template used: ${templateName}`);
        
        if (fixedTemplate.fixes.length > 0) {
            console.log(` Applied fixes:`);
            fixedTemplate.fixes.forEach(fix => console.log(`   - ${fix}`));
        }
        
        return filename;
    }

    /**
     * Print summary of processing results
     * @param {Array<Object>} results - Processing results
     */
    printSummary(results) {
        console.log(' Processing Summary:');
        console.log('=====================');
        
        const totalFiles = results.length;
        const successfulFiles = results.filter(r => !r.error).length;
        const filesWithFixes = results.filter(r => r.fixes && r.fixes.length > 0).length;
        const compiledSuccessfully = results.filter(r => r.compilation && r.compilation.success).length;
        
        console.log(`Total files processed: ${totalFiles}`);
        console.log(`Files processed successfully: ${successfulFiles}`);
        console.log(`Files with syntax fixes: ${filesWithFixes}`);
        console.log(`Files compiled successfully: ${compiledSuccessfully}`);
        
        if (results.some(r => r.error)) {
            console.log('\n Files with errors:');
            results.filter(r => r.error).forEach(r => {
                console.log(`   ${r.filePath}: ${r.error}`);
            });
        }
        
        if (results.some(r => r.compilation && !r.compilation.success)) {
            console.log('\n  Compilation issues:');
            results.filter(r => r.compilation && !r.compilation.success).forEach(r => {
                console.log(`   ${r.filePath}:`);
                if (r.compilation.errors) {
                    r.compilation.errors.forEach(error => console.log(`     - ${error}`));
                }
            });
        }
        
        console.log('\n Next steps:');
        console.log('1. Check the EON IDE at http://localhost:3001');
        console.log('2. Test compilation of fixed files');
        console.log('3. Run the EON programs');
    }

    /**
     * Interactive mode for fixing individual files
     */
    async interactiveMode() {
        const readline = require('readline');
        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });
        
        console.log(' EON Compiler Helper - Interactive Mode');
        console.log('==========================================\n');
        
        while (true) {
            const answer = await new Promise(resolve => {
                rl.question('Enter EON file path (or "quit" to exit): ', resolve);
            });
            
            if (answer.toLowerCase() === 'quit') {
                break;
            }
            
            if (fs.existsSync(answer)) {
                await this.fixAndCompile(answer);
            } else {
                console.log(' File not found. Please check the path.');
            }
            
            console.log('');
        }
        
        rl.close();
    }
}

// CLI interface
if (require.main === module) {
    const helper = new EONCompilerHelper();
    const args = process.argv.slice(2);
    
    if (args.length === 0) {
        // Process all files
        helper.processAllEonFiles();
    } else if (args[0] === '--interactive') {
        // Interactive mode
        helper.interactiveMode();
    } else if (args[0] === '--create') {
        // Create new file
        const filename = args[1] || 'new_file.eon';
        const template = args[2] || 'basic_main';
        helper.createEonFile(filename, template);
    } else {
        // Process specific file
        helper.fixAndCompile(args[0]);
    }
}

module.exports = EONCompilerHelper;

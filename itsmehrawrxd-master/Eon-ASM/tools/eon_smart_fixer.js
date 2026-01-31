/**
 * EON Smart Fixer
 * Uses the actual compiler errors to intelligently fix EON syntax issues
 */

const fs = require('fs');
const axios = require('axios');

class EONSmartFixer {
    constructor() {
        this.fixPatterns = new Map();
        this.serverEndpoint = 'http://localhost:3001/api/eon/compile';
        this.initializeFixPatterns();
    }

    /**
     * Initialize common fix patterns based on EON compiler behavior
     */
    initializeFixPatterns() {
        // Semicolon issues
        this.fixPatterns.set('semicolon_in_number', {
            pattern: /(\d+);(\d+);/g,
            replacement: '$1$2',
            description: 'Fix broken number literals with semicolons'
        });

        // Operator issues
        this.fixPatterns.set('operator_semicolon', {
            pattern: /(\w+)\s*\+\s*;(\w+);/g,
            replacement: '$1 + $2',
            description: 'Fix operators with semicolons'
        });

        this.fixPatterns.set('operator_semicolon_minus', {
            pattern: /(\w+)\s*-\s*;(\w+);/g,
            replacement: '$1 - $2',
            description: 'Fix minus operators with semicolons'
        });

        this.fixPatterns.set('operator_semicolon_multiply', {
            pattern: /(\w+)\s*\*\s*;(\w+);/g,
            replacement: '$1 * $2',
            description: 'Fix multiply operators with semicolons'
        });

        this.fixPatterns.set('operator_semicolon_divide', {
            pattern: /(\w+)\s*\/\s*;(\w+);/g,
            replacement: '$1 / $2',
            description: 'Fix divide operators with semicolons'
        });

        // Variable declaration issues
        this.fixPatterns.set('missing_type_annotation', {
            pattern: /let\s+(\w+)\s*=\s*(\d+)(?!\s*:)/g,
            replacement: 'let $1: int = $2',
            description: 'Add type annotation for integer variables'
        });

        this.fixPatterns.set('missing_type_annotation_string', {
            pattern: /let\s+(\w+)\s*=\s*"([^"]*)"(?!\s*:)/g,
            replacement: 'let $1: string = "$2"',
            description: 'Add type annotation for string variables'
        });

        this.fixPatterns.set('missing_type_annotation_bool', {
            pattern: /let\s+(\w+)\s*=\s*(true|false)(?!\s*:)/g,
            replacement: 'let $1: bool = $2',
            description: 'Add type annotation for boolean variables'
        });

        // Function definition issues
        this.fixPatterns.set('missing_def_keyword', {
            pattern: /^(\s*)func\s+(\w+)\s*\(/gm,
            replacement: '$1def func $2(',
            description: 'Add missing def keyword to function definitions'
        });

        this.fixPatterns.set('missing_return_type_main', {
            pattern: /^(\s*)def\s+func\s+main\s*\(\s*\)\s*\{/gm,
            replacement: '$1def func main() -> void {',
            description: 'Add return type to main function'
        });

        this.fixPatterns.set('missing_return_type_function', {
            pattern: /^(\s*)def\s+func\s+(\w+)\s*\([^)]+\)\s*\{/gm,
            replacement: '$1def func $2($3) -> int {',
            description: 'Add return type to functions with parameters'
        });
    }

    /**
     * Fix EON code using intelligent pattern matching
     * @param {string} code - Original EON code
     * @returns {Object} - Fix result with applied changes
     */
    fixCode(code) {
        let fixedCode = code;
        const appliedFixes = [];
        const originalCode = code;

        // Apply all fix patterns
        for (const [name, fixPattern] of this.fixPatterns) {
            const beforeFix = fixedCode;
            fixedCode = fixedCode.replace(fixPattern.pattern, fixPattern.replacement);
            
            if (beforeFix !== fixedCode) {
                appliedFixes.push({
                    name: name,
                    description: fixPattern.description,
                    changes: this.countChanges(beforeFix, fixedCode)
                });
            }
        }

        // Additional smart fixes
        fixedCode = this.applySmartFixes(fixedCode, appliedFixes);
        
        // Fix any issues created by the patterns
        fixedCode = this.fixPatternIssues(fixedCode);

        return {
            original: originalCode,
            fixed: fixedCode,
            appliedFixes: appliedFixes,
            hasChanges: appliedFixes.length > 0
        };
    }

    /**
     * Apply additional smart fixes based on context
     * @param {string} code - Code to fix
     * @param {Array} appliedFixes - Already applied fixes
     * @returns {string} - Fixed code
     */
    applySmartFixes(code, appliedFixes) {
        let fixed = code;

        // Ensure proper semicolons at end of statements
        fixed = fixed.replace(/let\s+[^;]+(?!;)/g, (match) => {
            if (!match.endsWith(';')) {
                return match + ';';
            }
            return match;
        });

        // Fix return statements
        fixed = fixed.replace(/ret\s+([^;]+)(?!;)/g, 'ret $1;');

        return fixed;
    }

    /**
     * Fix issues created by the pattern matching
     * @param {string} code - Code to fix
     * @returns {string} - Fixed code
     */
    fixPatternIssues(code) {
        let fixed = code;
        
        // Fix return statements that got broken by semicolon patterns
        fixed = fixed.replace(/ret\s*;(\w+);/g, 'ret $1;');
        fixed = fixed.replace(/ret\s*;(\d+);/g, 'ret $1;');
        
        // Fix variable declarations that lost their semicolons
        fixed = fixed.replace(/let\s+(\w+):\s*(\w+)\s*=\s*([^;]+)(?!;)/g, 'let $1: $2 = $3;');
        
        return fixed;
    }

    /**
     * Count the number of changes made
     * @param {string} before - Code before fix
     * @param {string} after - Code after fix
     * @returns {number} - Number of changes
     */
    countChanges(before, after) {
        const beforeLines = before.split('\n');
        const afterLines = after.split('\n');
        
        let changes = 0;
        for (let i = 0; i < Math.max(beforeLines.length, afterLines.length); i++) {
            if (beforeLines[i] !== afterLines[i]) {
                changes++;
            }
        }
        
        return changes;
    }

    /**
     * Fix all EON files in the current directory
     * @returns {Array} - Results of fixing each file
     */
    async fixAllFiles() {
        console.log(' EON Smart Fixer - Fixing all EON files...\n');
        
        const files = [
            'working_eon_v1.eon',
            'actually_working.eon',
            'working_syntax.eon',
            'test_java_eon.eon',
            'demo_java_eon.eon'
        ];
        
        const results = [];
        
        for (const file of files) {
            if (fs.existsSync(file)) {
                console.log(` Fixing: ${file}`);
                
                const originalCode = fs.readFileSync(file, 'utf8');
                const fixResult = this.fixCode(originalCode);
                
                if (fixResult.hasChanges) {
                    // Write fixed code back to file
                    fs.writeFileSync(file, fixResult.fixed);
                    
                    console.log(`    Applied ${fixResult.appliedFixes.length} fixes:`);
                    fixResult.appliedFixes.forEach(fix => {
                        console.log(`      - ${fix.description}`);
                    });
                } else {
                    console.log(`   ℹ  No fixes needed`);
                }
                
                results.push({
                    filename: file,
                    ...fixResult
                });
                
                console.log('');
            }
        }
        
        return results;
    }

    /**
     * Test EON code with the compiler via server API
     * @param {string} code - EON code to test
     * @param {string} filename - Filename for testing
     * @returns {Promise<Object>} - Compilation result
     */
    async testWithCompiler(code, filename) {
        try {
            const response = await axios.post(this.serverEndpoint, {
                code: code,
                filename: filename
            }, {
                timeout: 15000,
                headers: {
                    'Content-Type': 'application/json'
                }
            });
            
            return {
                success: response.data.success,
                output: response.data.output || '',
                error: response.data.error || '',
                filename: filename
            };
            
        } catch (error) {
            return {
                success: false,
                output: '',
                error: `Server API error: ${error.message}`,
                filename: filename
            };
        }
    }

    /**
     * Test the fixed files with the compiler
     * @param {Array} fixResults - Results from fixAllFiles
     * @returns {Array} - Compilation test results
     */
    async testFixedFiles(fixResults) {
        console.log(' Testing fixed files with compiler...\n');
        
        const testResults = [];
        
        for (const result of fixResults) {
            if (result.hasChanges) {
                console.log(` Testing: ${result.filename}`);
                
                const compilationResult = await this.testWithCompiler(result.fixed, result.filename);
                testResults.push({
                    testName: result.filename,
                    success: compilationResult.success,
                    result: compilationResult
                });
                
                console.log(`   ${compilationResult.success ? '' : ''} ${compilationResult.success ? 'Compilation successful' : 'Compilation failed'}`);
                
                if (!compilationResult.success) {
                    console.log(`   Error: ${compilationResult.error}`);
                }
                
                console.log('');
            }
        }
        
        return testResults;
    }

    /**
     * Generate a comprehensive fix report
     * @param {Array} fixResults - Fix results
     * @param {Array} testResults - Test results
     * @returns {string} - Report content
     */
    generateFixReport(fixResults, testResults) {
        const timestamp = new Date().toISOString();
        
        let report = `# EON Smart Fixer Report\n`;
        report += `Generated: ${timestamp}\n\n`;
        
        report += `## Summary\n`;
        report += `- Files processed: ${fixResults.length}\n`;
        report += `- Files with fixes: ${fixResults.filter(r => r.hasChanges).length}\n`;
        report += `- Files tested: ${testResults.length}\n`;
        report += `- Successful compilations: ${testResults.filter(r => r.success).length}\n\n`;
        
        report += `## Fix Details\n\n`;
        
        fixResults.forEach(result => {
            report += `### ${result.filename}\n`;
            report += `- Changes applied: ${result.appliedFixes.length}\n`;
            
            if (result.appliedFixes.length > 0) {
                report += `- Applied fixes:\n`;
                result.appliedFixes.forEach(fix => {
                    report += `  - ${fix.description}\n`;
                });
            }
            
            report += `\n`;
        });
        
        report += `## Compilation Results\n\n`;
        
        testResults.forEach(result => {
            report += `### ${result.testName}\n`;
            report += `- Status: ${result.success ? ' Success' : ' Failed'}\n`;
            
            if (!result.success) {
                report += `- Error: ${result.result.error}\n`;
            }
            
            report += `\n`;
        });
        
        return report;
    }

    /**
     * Run the complete fix and test process
     */
    async run() {
        console.log(' EON Smart Fixer - Complete Process\n');
        
        // Step 1: Fix all files
        const fixResults = await this.fixAllFiles();
        
        // Step 2: Test fixed files
        const testResults = await this.testFixedFiles(fixResults);
        
        // Step 3: Generate report
        const report = this.generateFixReport(fixResults, testResults);
        
        // Save report
        const reportFile = `EON_FIX_REPORT_${new Date().toISOString().replace(/[:.]/g, '-')}.md`;
        fs.writeFileSync(reportFile, report);
        
        console.log(' Final Summary:');
        console.log(`   - Files processed: ${fixResults.length}`);
        console.log(`   - Files with fixes: ${fixResults.filter(r => r.hasChanges).length}`);
        console.log(`   - Successful compilations: ${testResults.filter(r => r.success).length}`);
        console.log(`   - Report saved: ${reportFile}`);
        
        return {
            fixResults,
            testResults,
            report,
            reportFile
        };
    }
}

// CLI interface
if (require.main === module) {
    const fixer = new EONSmartFixer();
    
    fixer.run().catch(console.error);
}

module.exports = EONSmartFixer;

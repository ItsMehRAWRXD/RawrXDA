/**
 * EON Log-Based Template Generator
 * Uses the EON compiler server logs to understand and generate correct syntax
 */

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

class EONLogBasedGenerator {
    constructor() {
        this.compilerPath = 'java';
        this.compilerArgs = ['EonCompilerEnhanced', 'eon_v1_compiler.eon'];
        this.compilationLogs = [];
        this.successfulPatterns = [];
        this.errorPatterns = [];
        this.syntaxRules = new Map();
        this.serverEndpoint = 'http://localhost:3001/api/eon/compile';
    }

    /**
     * Test EON code with the actual compiler and capture logs
     * @param {string} code - EON source code to test
     * @param {string} testName - Name for this test
     * @returns {Promise<Object>} - Compilation result with detailed logs
     */
    async testWithCompiler(code, testName = 'test') {
        try {
            // Test via server API (preferred method)
            const result = await this.testViaServerAPI(code, testName);
            
            // Analyze the result
            const analysis = this.analyzeCompilationResult(code, result);
            
            // Store in logs
            this.compilationLogs.push({
                testName,
                code,
                result,
                analysis,
                timestamp: new Date().toISOString()
            });
            
            return {
                testName,
                code,
                result,
                analysis,
                success: result.success
            };
            
        } catch (error) {
            // Fallback to direct Java compilation if server is not available
            console.log(`  Server API failed, trying direct Java compilation: ${error.message}`);
            return await this.testWithDirectJava(code, testName);
        }
    }

    /**
     * Test EON code via the server API
     * @param {string} code - EON source code to test
     * @param {string} testName - Name for this test
     * @returns {Promise<Object>} - Compilation result
     */
    async testViaServerAPI(code, testName) {
        const axios = require('axios');
        
        try {
            const response = await axios.post(this.serverEndpoint, {
                code: code,
                filename: `${testName}.eon`
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
                stdout: response.data.output || '',
                stderr: response.data.error || '',
                exitCode: response.data.success ? 0 : 1,
                filename: response.data.filename || testName
            };
            
        } catch (error) {
            return {
                success: false,
                output: '',
                error: `Server API error: ${error.message}`,
                stdout: '',
                stderr: error.message,
                exitCode: -1,
                filename: testName
            };
        }
    }

    /**
     * Test EON code with direct Java compilation (fallback)
     * @param {string} code - EON source code to test
     * @param {string} testName - Name for this test
     * @returns {Promise<Object>} - Compilation result
     */
    async testWithDirectJava(code, testName) {
        const tempFile = `temp_${Date.now()}_${testName}.eon`;
        
        try {
            // Write test code to temp file
            await fs.promises.writeFile(tempFile, code);
            
            // Run the actual EON compiler via Java
            const result = await this.runEonCompiler(tempFile);
            
            // Analyze the result
            const analysis = this.analyzeCompilationResult(code, result);
            
            // Store in logs
            this.compilationLogs.push({
                testName,
                code,
                result,
                analysis,
                timestamp: new Date().toISOString(),
                method: 'direct_java'
            });
            
            return {
                testName,
                code,
                result,
                analysis,
                success: result.exitCode === 0
            };
            
        } finally {
            // Clean up temp file
            try {
                await fs.promises.unlink(tempFile);
            } catch (e) {
                // Ignore cleanup errors
            }
        }
    }

    /**
     * Run the EON compiler on a file
     * @param {string} filePath - Path to the EON file
     * @returns {Promise<Object>} - Compilation result
     */
    async runEonCompiler(filePath) {
        return new Promise((resolve) => {
            const args = [...this.compilerArgs, filePath];
            const process = spawn(this.compilerPath, args, {
                cwd: process.cwd(),
                timeout: 10000
            });
            
            let stdout = '';
            let stderr = '';
            let isComplete = false;
            
            const timeout = setTimeout(() => {
                if (!isComplete) {
                    process.kill();
                    isComplete = true;
                    resolve({
                        exitCode: -1,
                        stdout: stdout,
                        stderr: stderr + '\n[Timeout]',
                        success: false,
                        error: 'Compilation timeout'
                    });
                }
            }, 10000);
            
            process.stdout.on('data', (data) => {
                stdout += data.toString();
            });
            
            process.stderr.on('data', (data) => {
                stderr += data.toString();
            });
            
            process.on('close', (code) => {
                if (isComplete) return;
                isComplete = true;
                clearTimeout(timeout);
                
                resolve({
                    exitCode: code,
                    stdout: stdout,
                    stderr: stderr,
                    success: code === 0,
                    error: code !== 0 ? stderr || stdout : null
                });
            });
            
            process.on('error', (err) => {
                if (isComplete) return;
                isComplete = true;
                clearTimeout(timeout);
                
                resolve({
                    exitCode: -1,
                    stdout: stdout,
                    stderr: stderr,
                    success: false,
                    error: `Process error: ${err.message}`
                });
            });
        });
    }

    /**
     * Analyze compilation result to extract syntax rules
     * @param {string} code - Original code
     * @param {Object} result - Compilation result
     * @returns {Object} - Analysis of the result
     */
    analyzeCompilationResult(code, result) {
        const analysis = {
            success: result.success,
            errorType: null,
            errorMessage: result.error,
            syntaxIssues: [],
            suggestions: [],
            patterns: []
        };
        
        if (!result.success) {
            // Analyze error patterns
            const errorAnalysis = this.analyzeError(result.error || result.stderr);
            analysis.errorType = errorAnalysis.type;
            analysis.syntaxIssues = errorAnalysis.issues;
            analysis.suggestions = errorAnalysis.suggestions;
            analysis.patterns = errorAnalysis.patterns;
            
            // Store error patterns for learning
            this.errorPatterns.push({
                code,
                error: result.error,
                analysis: errorAnalysis
            });
        } else {
            // Store successful patterns
            this.successfulPatterns.push({
                code,
                output: result.stdout
            });
        }
        
        return analysis;
    }

    /**
     * Analyze error messages to extract syntax rules
     * @param {string} error - Error message
     * @returns {Object} - Error analysis
     */
    analyzeError(error) {
        const analysis = {
            type: 'unknown',
            issues: [],
            suggestions: [],
            patterns: []
        };
        
        if (!error) return analysis;
        
        const errorLower = error.toLowerCase();
        
        // Common EON syntax error patterns
        if (errorLower.includes('unexpected token')) {
            analysis.type = 'syntax_error';
            analysis.issues.push('Unexpected token in expression');
            analysis.suggestions.push('Check for missing operators or incorrect syntax');
        }
        
        if (errorLower.includes('expected')) {
            analysis.type = 'syntax_error';
            analysis.issues.push('Expected token not found');
            analysis.suggestions.push('Check for missing punctuation or keywords');
        }
        
        if (errorLower.includes('semicolon')) {
            analysis.type = 'syntax_error';
            analysis.issues.push('Missing semicolon');
            analysis.suggestions.push('Add semicolon at end of statement');
        }
        
        if (errorLower.includes('type')) {
            analysis.type = 'type_error';
            analysis.issues.push('Type mismatch or missing type annotation');
            analysis.suggestions.push('Add explicit type annotations');
        }
        
        if (errorLower.includes('function') || errorLower.includes('func')) {
            analysis.type = 'function_error';
            analysis.issues.push('Function definition issue');
            analysis.suggestions.push('Check function syntax: def func name() -> type {}');
        }
        
        // Extract specific patterns from error
        const patterns = this.extractPatternsFromError(error);
        analysis.patterns = patterns;
        
        return analysis;
    }

    /**
     * Extract syntax patterns from error messages
     * @param {string} error - Error message
     * @returns {Array} - Extracted patterns
     */
    extractPatternsFromError(error) {
        const patterns = [];
        
        // Look for specific syntax issues
        if (error.includes(';')) {
            patterns.push({
                type: 'semicolon_issue',
                description: 'Semicolon placement or usage issue',
                fix: 'Ensure semicolons are used correctly'
            });
        }
        
        if (error.includes('+') || error.includes('-') || error.includes('*') || error.includes('/')) {
            patterns.push({
                type: 'operator_issue',
                description: 'Operator usage issue',
                fix: 'Check operator placement and operands'
            });
        }
        
        if (error.includes('let') || error.includes('var')) {
            patterns.push({
                type: 'variable_declaration_issue',
                description: 'Variable declaration syntax issue',
                fix: 'Use: let name: type = value;'
            });
        }
        
        if (error.includes('def') || error.includes('func')) {
            patterns.push({
                type: 'function_definition_issue',
                description: 'Function definition syntax issue',
                fix: 'Use: def func name(params) -> return_type {}'
            });
        }
        
        return patterns;
    }

    /**
     * Generate syntax rules based on compilation logs
     * @returns {Object} - Generated syntax rules
     */
    generateSyntaxRules() {
        const rules = {
            functionDefinitions: [],
            variableDeclarations: [],
            operators: [],
            types: [],
            commonErrors: [],
            bestPractices: []
        };
        
        // Analyze successful patterns
        this.successfulPatterns.forEach(pattern => {
            this.extractRulesFromSuccess(pattern, rules);
        });
        
        // Analyze error patterns
        this.errorPatterns.forEach(pattern => {
            this.extractRulesFromError(pattern, rules);
        });
        
        return rules;
    }

    /**
     * Extract rules from successful compilation
     * @param {Object} pattern - Successful pattern
     * @param {Object} rules - Rules object to update
     */
    extractRulesFromSuccess(pattern, rules) {
        const code = pattern.code;
        
        // Extract function definition patterns
        const funcMatches = code.match(/def\s+func\s+(\w+)\s*\([^)]*\)\s*(?:->\s*(\w+))?\s*\{/g);
        if (funcMatches) {
            funcMatches.forEach(match => {
                rules.functionDefinitions.push({
                    pattern: match,
                    description: 'Valid function definition',
                    example: match
                });
            });
        }
        
        // Extract variable declaration patterns
        const varMatches = code.match(/let\s+(\w+)\s*:\s*(\w+)\s*=\s*[^;]+;/g);
        if (varMatches) {
            varMatches.forEach(match => {
                rules.variableDeclarations.push({
                    pattern: match,
                    description: 'Valid variable declaration',
                    example: match
                });
            });
        }
    }

    /**
     * Extract rules from error patterns
     * @param {Object} pattern - Error pattern
     * @param {Object} rules - Rules object to update
     */
    extractRulesFromError(pattern, rules) {
        const analysis = pattern.analysis;
        
        analysis.patterns.forEach(pat => {
            rules.commonErrors.push({
                type: pat.type,
                description: pat.description,
                fix: pat.fix,
                example: pattern.code
            });
        });
    }

    /**
     * Generate templates based on learned syntax rules
     * @returns {Object} - Generated templates
     */
    generateTemplates() {
        const rules = this.generateSyntaxRules();
        
        const templates = {
            basic_function: this.generateBasicFunctionTemplate(rules),
            function_with_params: this.generateFunctionWithParamsTemplate(rules),
            variable_declarations: this.generateVariableDeclarationsTemplate(rules),
            arithmetic_operations: this.generateArithmeticTemplate(rules),
            conditional_logic: this.generateConditionalTemplate(rules),
            error_corrections: this.generateErrorCorrectionTemplates(rules)
        };
        
        return templates;
    }

    /**
     * Generate basic function template
     */
    generateBasicFunctionTemplate(rules) {
        // Find successful function patterns
        const successfulFuncs = rules.functionDefinitions.filter(rule => 
            rule.pattern.includes('main') && rule.pattern.includes('void')
        );
        
        if (successfulFuncs.length > 0) {
            return successfulFuncs[0].example;
        }
        
        // Fallback template
        return `def func main() -> void {
    // Your code here
    ret 0;
}`;
    }

    /**
     * Generate function with parameters template
     */
    generateFunctionWithParamsTemplate(rules) {
        const successfulFuncs = rules.functionDefinitions.filter(rule => 
            rule.pattern.includes('(') && rule.pattern.includes(')') && 
            !rule.pattern.includes('()')
        );
        
        if (successfulFuncs.length > 0) {
            return successfulFuncs[0].example;
        }
        
        return `def func example(a: int, b: int) -> int {
    let result: int = a + b;
    ret result;
}`;
    }

    /**
     * Generate variable declarations template
     */
    generateVariableDeclarationsTemplate(rules) {
        const successfulVars = rules.variableDeclarations;
        
        if (successfulVars.length > 0) {
            return successfulVars.map(v => v.example).join('\n');
        }
        
        return `let x: int = 42;
let y: string = "hello";
let flag: bool = true;`;
    }

    /**
     * Generate arithmetic operations template
     */
    generateArithmeticTemplate(rules) {
        return `def func calculate(a: int, b: int) -> int {
    let sum: int = a + b;
    let product: int = a * b;
    let result: int = sum + product;
    ret result;
}`;
    }

    /**
     * Generate conditional logic template
     */
    generateConditionalTemplate(rules) {
        return `def func check_value(x: int) -> bool {
    if (x > 0) {
        ret true;
    } else {
        ret false;
    }
}`;
    }

    /**
     * Generate error correction templates
     */
    generateErrorCorrectionTemplates(rules) {
        const corrections = {};
        
        rules.commonErrors.forEach(error => {
            corrections[error.type] = {
                description: error.description,
                fix: error.fix,
                example: error.example
            };
        });
        
        return corrections;
    }

    /**
     * Test the current broken EON files and generate fixes
     */
    async testCurrentFiles() {
        console.log(' Testing current EON files with compiler...\n');
        
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
                console.log(` Testing: ${file}`);
                const code = fs.readFileSync(file, 'utf8');
                const result = await this.testWithCompiler(code, file);
                results.push(result);
                
                console.log(`   ${result.success ? '' : ''} ${result.success ? 'Success' : 'Failed'}`);
                if (!result.success) {
                    console.log(`   Error: ${result.result.error}`);
                    result.analysis.suggestions.forEach(suggestion => {
                        console.log(`    ${suggestion}`);
                    });
                }
                console.log('');
            }
        }
        
        return results;
    }

    /**
     * Generate fixes for broken EON files based on compiler errors
     * @param {Array} testResults - Results from testCurrentFiles
     * @returns {Array} - Fixed code for each file
     */
    generateFixesFromErrors(testResults) {
        const fixes = [];
        
        testResults.forEach(result => {
            if (!result.success) {
                const fix = this.generateFixForFile(result);
                fixes.push(fix);
            }
        });
        
        return fixes;
    }

    /**
     * Generate a fix for a specific file based on its errors
     * @param {Object} testResult - Test result with errors
     * @returns {Object} - Fix information
     */
    generateFixForFile(testResult) {
        const originalCode = testResult.code;
        let fixedCode = originalCode;
        const appliedFixes = [];
        
        // Apply common fixes based on error patterns
        const errorAnalysis = testResult.analysis;
        
        // Fix semicolon issues
        if (errorAnalysis.patterns.some(p => p.type === 'semicolon_issue')) {
            fixedCode = this.fixSemicolonIssues(fixedCode);
            appliedFixes.push('Fixed semicolon issues');
        }
        
        // Fix operator issues
        if (errorAnalysis.patterns.some(p => p.type === 'operator_issue')) {
            fixedCode = this.fixOperatorIssues(fixedCode);
            appliedFixes.push('Fixed operator issues');
        }
        
        // Fix variable declaration issues
        if (errorAnalysis.patterns.some(p => p.type === 'variable_declaration_issue')) {
            fixedCode = this.fixVariableDeclarationIssues(fixedCode);
            appliedFixes.push('Fixed variable declaration issues');
        }
        
        // Fix function definition issues
        if (errorAnalysis.patterns.some(p => p.type === 'function_definition_issue')) {
            fixedCode = this.fixFunctionDefinitionIssues(fixedCode);
            appliedFixes.push('Fixed function definition issues');
        }
        
        return {
            filename: testResult.testName,
            original: originalCode,
            fixed: fixedCode,
            appliedFixes: appliedFixes,
            errorAnalysis: errorAnalysis
        };
    }

    /**
     * Fix semicolon issues in EON code
     * @param {string} code - Original code
     * @returns {string} - Fixed code
     */
    fixSemicolonIssues(code) {
        let fixed = code;
        
        // Fix cases like "4;2;" -> "42"
        fixed = fixed.replace(/(\d+);(\d+);/g, '$1$2');
        
        // Fix cases like "+ ;b;" -> "+ b"
        fixed = fixed.replace(/\+ ;(\w+);/g, '+ $1');
        fixed = fixed.replace(/- ;(\w+);/g, '- $1');
        fixed = fixed.replace(/\* ;(\w+);/g, '* $1');
        fixed = fixed.replace(/\/ ;(\w+);/g, '/ $1');
        
        // Ensure proper semicolons at end of statements
        fixed = fixed.replace(/let\s+[^;]+(?!;)/g, (match) => {
            if (!match.endsWith(';')) {
                return match + ';';
            }
            return match;
        });
        
        return fixed;
    }

    /**
     * Fix operator issues in EON code
     * @param {string} code - Original code
     * @returns {string} - Fixed code
     */
    fixOperatorIssues(code) {
        let fixed = code;
        
        // Fix broken arithmetic expressions
        fixed = fixed.replace(/(\w+)\s*\+\s*;(\w+);/g, '$1 + $2');
        fixed = fixed.replace(/(\w+)\s*-\s*;(\w+);/g, '$1 - $2');
        fixed = fixed.replace(/(\w+)\s*\*\s*;(\w+);/g, '$1 * $2');
        fixed = fixed.replace(/(\w+)\s*\/\s*;(\w+);/g, '$1 / $2');
        
        return fixed;
    }

    /**
     * Fix variable declaration issues in EON code
     * @param {string} code - Original code
     * @returns {string} - Fixed code
     */
    fixVariableDeclarationIssues(code) {
        let fixed = code;
        
        // Ensure proper type annotations
        fixed = fixed.replace(/let\s+(\w+)\s*=\s*(\d+)(?!\s*:)/g, 'let $1: int = $2');
        fixed = fixed.replace(/let\s+(\w+)\s*=\s*"([^"]*)"(?!\s*:)/g, 'let $1: string = "$2"');
        fixed = fixed.replace(/let\s+(\w+)\s*=\s*(true|false)(?!\s*:)/g, 'let $1: bool = $2');
        
        return fixed;
    }

    /**
     * Fix function definition issues in EON code
     * @param {string} code - Original code
     * @returns {string} - Fixed code
     */
    fixFunctionDefinitionIssues(code) {
        let fixed = code;
        
        // Ensure proper function syntax
        fixed = fixed.replace(/^(\s*)func\s+(\w+)\s*\(/gm, '$1def func $2(');
        
        // Add return types if missing
        fixed = fixed.replace(/^(\s*)def\s+func\s+(\w+)\s*\(\s*\)\s*\{/gm, (match, indent, name) => {
            if (name === 'main') {
                return `${indent}def func ${name}() -> void {`;
            }
            return match;
        });
        
        return fixed;
    }

    /**
     * Generate comprehensive syntax guide based on logs
     */
    generateSyntaxGuide() {
        const rules = this.generateSyntaxRules();
        const templates = this.generateTemplates();
        
        const guide = `# EON Syntax Guide (Generated from Compiler Logs)

## Compilation Results
- Total tests: ${this.compilationLogs.length}
- Successful: ${this.successfulPatterns.length}
- Failed: ${this.errorPatterns.length}

## Successful Patterns

### Function Definitions
${rules.functionDefinitions.map(rule => `- ${rule.example}`).join('\n')}

### Variable Declarations
${rules.variableDeclarations.map(rule => `- ${rule.example}`).join('\n')}

## Common Errors and Fixes
${rules.commonErrors.map(error => `
### ${error.type}
- **Description**: ${error.description}
- **Fix**: ${error.fix}
- **Example**: \`${error.example}\`
`).join('\n')}

## Generated Templates

### Basic Function
\`\`\`eon
${templates.basic_function}
\`\`\`

### Function with Parameters
\`\`\`eon
${templates.function_with_params}
\`\`\`

### Variable Declarations
\`\`\`eon
${templates.variable_declarations}
\`\`\`

### Arithmetic Operations
\`\`\`eon
${templates.arithmetic_operations}
\`\`\`

### Conditional Logic
\`\`\`eon
${templates.conditional_logic}
\`\`\`

## Best Practices
${rules.bestPractices.map(practice => `- ${practice}`).join('\n')}
`;

        return guide;
    }

    /**
     * Save logs and generated content
     */
    async saveResults() {
        const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
        
        // Save compilation logs
        const logsFile = `eon_compilation_logs_${timestamp}.json`;
        await fs.promises.writeFile(logsFile, JSON.stringify(this.compilationLogs, null, 2));
        
        // Save syntax rules
        const rulesFile = `eon_syntax_rules_${timestamp}.json`;
        const rules = this.generateSyntaxRules();
        await fs.promises.writeFile(rulesFile, JSON.stringify(rules, null, 2));
        
        // Save syntax guide
        const guideFile = `EON_SYNTAX_GUIDE_${timestamp}.md`;
        const guide = this.generateSyntaxGuide();
        await fs.promises.writeFile(guideFile, guide);
        
        // Save templates
        const templatesFile = `eon_templates_${timestamp}.json`;
        const templates = this.generateTemplates();
        await fs.promises.writeFile(templatesFile, JSON.stringify(templates, null, 2));
        
        console.log(` Saved results:`);
        console.log(`   - Compilation logs: ${logsFile}`);
        console.log(`   - Syntax rules: ${rulesFile}`);
        console.log(`   - Syntax guide: ${guideFile}`);
        console.log(`   - Templates: ${templatesFile}`);
        
        return {
            logsFile,
            rulesFile,
            guideFile,
            templatesFile
        };
    }
}

// CLI interface
if (require.main === module) {
    const generator = new EONLogBasedGenerator();
    
    async function main() {
        console.log(' EON Log-Based Template Generator\n');
        
        // Test current files
        const results = await generator.testCurrentFiles();
        
        // Generate fixes for broken files
        const fixes = generator.generateFixesFromErrors(results);
        
        if (fixes.length > 0) {
            console.log(' Generated fixes for broken files:');
            fixes.forEach(fix => {
                console.log(`\n ${fix.filename}:`);
                console.log('   Applied fixes:');
                fix.appliedFixes.forEach(appliedFix => {
                    console.log(`   - ${appliedFix}`);
                });
                
                // Save fixed file
                const fixedFilename = fix.filename.replace('.eon', '_fixed.eon');
                fs.writeFileSync(fixedFilename, fix.fixed);
                console.log(`    Saved fixed version as: ${fixedFilename}`);
            });
        }
        
        // Generate and save results
        const files = await generator.saveResults();
        
        console.log('\n Analysis complete!');
        console.log(' Summary:');
        console.log(`   - Tested ${results.length} files`);
        console.log(`   - ${results.filter(r => r.success).length} successful`);
        console.log(`   - ${results.filter(r => !r.success).length} failed`);
        console.log(`   - Generated ${fixes.length} fixes`);
        console.log('\n Generated files contain:');
        console.log('   - Detailed compilation logs');
        console.log('   - Extracted syntax rules');
        console.log('   - Generated templates');
        console.log('   - Comprehensive syntax guide');
        console.log('   - Fixed EON files');
    }
    
    main().catch(console.error);
}

module.exports = EONLogBasedGenerator;

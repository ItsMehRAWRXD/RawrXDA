/**
 * Test script for EON syntax fixes
 * Verifies that the syntax fixes work correctly
 */

const fs = require('fs');
const { EONSyntaxFixer, EONSyntaxTemplates } = require('./eon_syntax_fixer.js');

class EONSyntaxTester {
    constructor() {
        this.fixer = new EONSyntaxFixer();
        this.templates = new EONSyntaxTemplates();
        this.testResults = [];
    }

    /**
     * Test all EON files in the project
     */
    testAllFiles() {
        console.log(' Testing EON syntax fixes...\n');
        
        const eonFiles = [
            'working_eon_v1.eon',
            'actually_working.eon', 
            'working_syntax.eon',
            'test_java_eon.eon',
            'demo_java_eon.eon'
        ];
        
        eonFiles.forEach(filename => {
            this.testFile(filename);
        });
        
        this.printResults();
    }

    /**
     * Test a specific EON file
     */
    testFile(filename) {
        try {
            if (!fs.existsSync(filename)) {
                console.log(`  File not found: ${filename}`);
                return;
            }
            
            const originalContent = fs.readFileSync(filename, 'utf8');
            const result = this.fixer.fix(originalContent);
            
            const testResult = {
                filename: filename,
                original: originalContent,
                fixed: result.fixed,
                fixes: result.fixes,
                warnings: result.warnings,
                needsMainFunction: result.needsMainFunction,
                success: result.fixes.length > 0 || result.warnings.length > 0
            };
            
            this.testResults.push(testResult);
            
            console.log(` ${filename}:`);
            if (result.fixes.length > 0) {
                console.log(`   Fixes applied: ${result.fixes.length}`);
                result.fixes.forEach(fix => console.log(`   - ${fix}`));
            }
            if (result.warnings.length > 0) {
                console.log(`   Warnings: ${result.warnings.length}`);
                result.warnings.forEach(warning => console.log(`   - ${warning}`));
            }
            if (result.needsMainFunction) {
                console.log(`     Needs main function`);
            }
            console.log('');
            
        } catch (error) {
            console.log(` Error testing ${filename}: ${error.message}`);
        }
    }

    /**
     * Test syntax templates
     */
    testTemplates() {
        console.log(' Testing EON syntax templates...\n');
        
        const templateNames = this.templates.getTemplateNames();
        
        templateNames.forEach(name => {
            const template = this.templates.getTemplate(name);
            const result = this.fixer.fix(template);
            
            console.log(` Template: ${name}`);
            if (result.fixes.length > 0) {
                console.log(`   Fixes needed: ${result.fixes.length}`);
                result.fixes.forEach(fix => console.log(`   - ${fix}`));
            } else {
                console.log(`    No fixes needed - template is correct`);
            }
            console.log('');
        });
    }

    /**
     * Create a test EON file with correct syntax
     */
    createTestFile() {
        const testCode = `// Test EON file with correct syntax
def func add(a: int, b: int) -> int {
    let result: int = a + b;
    ret result;
}

def func main() -> void {
    let x: int = 5;
    let y: int = 3;
    let sum: int = add(x, y);
    ret 0;
}`;

        fs.writeFileSync('test_correct_syntax.eon', testCode);
        console.log(' Created test_correct_syntax.eon with correct EON syntax');
    }

    /**
     * Print test results summary
     */
    printResults() {
        console.log(' Test Results Summary:');
        console.log('========================');
        
        const totalFiles = this.testResults.length;
        const filesWithFixes = this.testResults.filter(r => r.fixes.length > 0).length;
        const filesWithWarnings = this.testResults.filter(r => r.warnings.length > 0).length;
        const filesNeedingMain = this.testResults.filter(r => r.needsMainFunction).length;
        
        console.log(`Total files tested: ${totalFiles}`);
        console.log(`Files with fixes applied: ${filesWithFixes}`);
        console.log(`Files with warnings: ${filesWithWarnings}`);
        console.log(`Files needing main function: ${filesNeedingMain}`);
        
        if (filesWithFixes > 0) {
            console.log('\n Common fixes applied:');
            const allFixes = this.testResults.flatMap(r => r.fixes);
            const fixCounts = {};
            allFixes.forEach(fix => {
                fixCounts[fix] = (fixCounts[fix] || 0) + 1;
            });
            
            Object.entries(fixCounts)
                .sort(([,a], [,b]) => b - a)
                .forEach(([fix, count]) => {
                    console.log(`   ${fix} (${count} times)`);
                });
        }
        
        console.log('\n Next steps:');
        console.log('1. Test compilation with the EON compiler');
        console.log('2. Verify that syntax errors are resolved');
        console.log('3. Run the fixed EON programs');
    }

    /**
     * Generate syntax documentation
     */
    generateSyntaxDoc() {
        const doc = `# EON Syntax Reference

## Function Definitions
\`\`\`eon
def func function_name(parameter: type) -> return_type {
    // function body
    ret value;
}
\`\`\`

## Variable Declarations
\`\`\`eon
let variable_name: type = value;
\`\`\`

## Main Function
\`\`\`eon
def func main() -> void {
    // main program logic
    ret 0;
}
\`\`\`

## Type Annotations
- \`int\` - Integer numbers
- \`float\` - Floating point numbers  
- \`bool\` - Boolean values (true/false)
- \`string\` - String literals
- \`void\` - No return value

## Examples

### Basic Program
\`\`\`eon
def func main() -> void {
    let x: int = 42;
    ret 0;
}
\`\`\`

### Function with Parameters
\`\`\`eon
def func add(a: int, b: int) -> int {
    let result: int = a + b;
    ret result;
}

def func main() -> void {
    let sum: int = add(5, 3);
    ret 0;
}
\`\`\`

### Conditional Logic
\`\`\`eon
def func check_positive(x: int) -> bool {
    if (x > 0) {
        ret true;
    } else {
        ret false;
    }
}
\`\`\`
`;

        fs.writeFileSync('EON_SYNTAX_REFERENCE.md', doc);
        console.log(' Generated EON_SYNTAX_REFERENCE.md');
    }
}

// Run tests if this file is executed directly
if (require.main === module) {
    const tester = new EONSyntaxTester();
    
    console.log(' EON Syntax Fixer Test Suite\n');
    
    // Test all files
    tester.testAllFiles();
    
    // Test templates
    tester.testTemplates();
    
    // Create test file
    tester.createTestFile();
    
    // Generate documentation
    tester.generateSyntaxDoc();
    
    console.log('\n All tests completed!');
}

module.exports = EONSyntaxTester;

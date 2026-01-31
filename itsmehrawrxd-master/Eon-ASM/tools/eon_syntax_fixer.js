/**
 * EON Syntax Fixer
 * Converts simple EON syntax to compiler-compatible syntax
 * Based on analysis of eon_full_compiler.eon patterns
 */

class EONSyntaxFixer {
    constructor() {
        this.fixes = [];
        this.warnings = [];
    }

    /**
     * Fix EON source code to match compiler expectations
     * @param {string} source - Original EON source code
     * @returns {Object} - Fixed code and analysis
     */
    fix(source) {
        this.fixes = [];
        this.warnings = [];
        
        let fixedCode = source;
        
        // Apply fixes in order
        fixedCode = this.fixFunctionDefinitions(fixedCode);
        fixedCode = this.fixVariableDeclarations(fixedCode);
        fixedCode = this.fixReturnStatements(fixedCode);
        fixedCode = this.fixTypeAnnotations(fixedCode);
        fixedCode = this.fixSemicolons(fixedCode);
        fixedCode = this.fixComments(fixedCode);
        
        return {
            original: source,
            fixed: fixedCode,
            fixes: this.fixes,
            warnings: this.warnings,
            needsMainFunction: !this.hasMainFunction(fixedCode)
        };
    }

    /**
     * Fix function definitions to match compiler syntax
     */
    fixFunctionDefinitions(source) {
        // Pattern 1: func main() { -> def func main() {
        let fixed = source.replace(/^(\s*)func\s+(\w+)\s*\(/gm, '$1def func $2(');
        
        // Pattern 2: def func main() { -> def func main() -> void {
        fixed = fixed.replace(/^(\s*)def\s+func\s+(\w+)\s*\(\s*\)\s*\{/gm, (match, indent, name) => {
            if (name === 'main') {
                this.fixes.push(`Added return type 'void' to main function`);
                return `${indent}def func ${name}() -> void {`;
            }
            return match;
        });
        
        // Pattern 3: Add return types for functions with parameters
        fixed = fixed.replace(/^(\s*)def\s+func\s+(\w+)\s*\([^)]+\)\s*\{/gm, (match, indent, name) => {
            if (name !== 'main' && !match.includes('->')) {
                this.fixes.push(`Added return type 'int' to function ${name}`);
                return match.replace('{', '-> int {');
            }
            return match;
        });
        
        return fixed;
    }

    /**
     * Fix variable declarations
     */
    fixVariableDeclarations(source) {
        // Pattern 1: let x = 42; -> let x: int = 42;
        let fixed = source.replace(/let\s+(\w+)\s*=\s*(\d+)/g, (match, varName, value) => {
            this.fixes.push(`Added type annotation 'int' to variable ${varName}`);
            return `let ${varName}: int = ${value}`;
        });
        
        // Pattern 2: let x = "string"; -> let x: string = "string";
        fixed = fixed.replace(/let\s+(\w+)\s*=\s*"([^"]*)"/g, (match, varName, value) => {
            this.fixes.push(`Added type annotation 'string' to variable ${varName}`);
            return `let ${varName}: string = "${value}"`;
        });
        
        // Pattern 3: let x = true/false; -> let x: bool = true/false;
        fixed = fixed.replace(/let\s+(\w+)\s*=\s*(true|false)/g, (match, varName, value) => {
            this.fixes.push(`Added type annotation 'bool' to variable ${varName}`);
            return `let ${varName}: bool = ${value}`;
        });
        
        return fixed;
    }

    /**
     * Fix return statements
     */
    fixReturnStatements(source) {
        // Pattern 1: ret x; -> ret x;
        // (already correct, but ensure semicolon)
        let fixed = source.replace(/ret\s+([^;]+)(?!;)/g, (match, value) => {
            this.fixes.push(`Added semicolon to return statement`);
            return `ret ${value};`;
        });
        
        return fixed;
    }

    /**
     * Fix type annotations for function parameters
     */
    fixTypeAnnotations(source) {
        // Pattern: func add(a, b) -> func add(a: int, b: int)
        let fixed = source.replace(/def\s+func\s+(\w+)\s*\(([^)]+)\)/g, (match, funcName, params) => {
            if (params.trim() && !params.includes(':')) {
                const paramList = params.split(',').map(param => {
                    const trimmed = param.trim();
                    if (trimmed && !trimmed.includes(':')) {
                        this.fixes.push(`Added type annotation 'int' to parameter ${trimmed}`);
                        return `${trimmed}: int`;
                    }
                    return trimmed;
                }).join(', ');
                
                return `def func ${funcName}(${paramList})`;
            }
            return match;
        });
        
        return fixed;
    }

    /**
     * Ensure proper semicolon usage
     */
    fixSemicolons(source) {
        let fixed = source;
        
        // Add semicolons to variable declarations
        fixed = fixed.replace(/let\s+[^;]+(?!;)/g, (match) => {
            if (!match.endsWith(';')) {
                this.fixes.push(`Added semicolon to variable declaration`);
                return match + ';';
            }
            return match;
        });
        
        return fixed;
    }

    /**
     * Fix comment syntax
     */
    fixComments(source) {
        // Convert // comments to proper EON comments if needed
        // (EON seems to support // comments based on the compiler)
        return source;
    }

    /**
     * Check if code has a main function
     */
    hasMainFunction(source) {
        return /def\s+func\s+main\s*\(/i.test(source);
    }

    /**
     * Generate a proper main function if missing
     */
    generateMainFunction() {
        return `def func main() -> void {
    // TODO: Add your main logic here
    ret 0;
}`;
    }

    /**
     * Create a complete, compilable EON program
     */
    createCompilableProgram(source) {
        const result = this.fix(source);
        
        if (result.needsMainFunction) {
            result.fixed += '\n\n' + this.generateMainFunction();
            this.fixes.push('Added missing main function');
        }
        
        return result;
    }
}

/**
 * EON Syntax Templates
 * Pre-built templates that match the compiler's expectations
 */
class EONSyntaxTemplates {
    constructor() {
        this.templates = {
            // Basic main function
            basic_main: `def func main() -> void {
    let x: int = 42;
    ret 0;
}`,

            // Function with parameters
            function_with_params: `def func add(a: int, b: int) -> int {
    let result: int = a + b;
    ret result;
}

def func main() -> void {
    let sum: int = add(5, 3);
    ret 0;
}`,

            // Conditional logic
            conditional: `def func check_positive(x: int) -> bool {
    if (x > 0) {
        ret true;
    } else {
        ret false;
    }
}

def func main() -> void {
    let number: int = 42;
    let is_positive: bool = check_positive(number);
    ret 0;
}`,

            // Loop example
            loop_example: `def func count_to_ten() -> void {
    let i: int = 0;
    loop {
        if (i >= 10) {
            break;
        }
        i = i + 1;
    }
}

def func main() -> void {
    count_to_ten();
    ret 0;
}`,

            // Model definition
            model_example: `def model Point {
    x: int,
    y: int
}

def func create_point(x: int, y: int) -> Point {
    let p: Point;
    p.x = x;
    p.y = y;
    ret p;
}

def func main() -> void {
    let origin: Point = create_point(0, 0);
    ret 0;
}`,

            // String handling
            string_example: `def func greet(name: string) -> string {
    let message: string = "Hello, ";
    message = message + name;
    ret message;
}

def func main() -> void {
    let greeting: string = greet("EON");
    ret 0;
}`,

            // Arithmetic operations
            arithmetic: `def func calculate(a: int, b: int) -> int {
    let sum: int = a + b;
    let product: int = a * b;
    let result: int = sum + product;
    ret result;
}

def func main() -> void {
    let answer: int = calculate(5, 3);
    ret 0;
}`
        };
    }

    /**
     * Get template by name
     */
    getTemplate(name) {
        return this.templates[name] || this.templates.basic_main;
    }

    /**
     * Get all template names
     */
    getTemplateNames() {
        return Object.keys(this.templates);
    }

    /**
     * Create a template based on requirements
     */
    createCustomTemplate(requirements) {
        let template = '';
        
        // Add functions
        if (requirements.functions) {
            requirements.functions.forEach(func => {
                template += `def func ${func.name}(`;
                if (func.parameters) {
                    template += func.parameters.map(p => `${p.name}: ${p.type}`).join(', ');
                }
                template += `) -> ${func.returnType || 'int'} {\n`;
                if (func.body) {
                    template += func.body;
                } else {
                    template += `    // TODO: Implement ${func.name}\n`;
                    template += `    ret 0;\n`;
                }
                template += '}\n\n';
            });
        }
        
        // Add main function if not present
        if (!requirements.functions || !requirements.functions.some(f => f.name === 'main')) {
            template += `def func main() -> void {\n`;
            template += `    // TODO: Add your main logic here\n`;
            template += `    ret 0;\n`;
            template += '}\n';
        }
        
        return template;
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { EONSyntaxFixer, EONSyntaxTemplates };
}

// Global access for browser usage
if (typeof window !== 'undefined') {
    window.EONSyntaxFixer = EONSyntaxFixer;
    window.EONSyntaxTemplates = EONSyntaxTemplates;
}

#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

class SimpleEonCompiler {
    constructor() {
        this.output = [];
    }

    compile(sourceFile) {
        console.log(` Simple EON Compiler - Compiling ${sourceFile}`);
        
        try {
            const source = fs.readFileSync(sourceFile, 'utf8');
            const lines = source.split('\n');
            
            // Generate assembly header
            this.output.push('section .text');
            this.output.push('global main');
            this.output.push('extern printf');
            this.output.push('main:');
            this.output.push('    push rbp');
            this.output.push('    mov rbp, rsp');
            this.output.push('    sub rsp, 1024');
            this.output.push('');
            
            // Parse and generate code
            for (const line of lines) {
                const trimmed = line.trim();
                if (trimmed.startsWith('//') || trimmed === '') continue;
                
                if (trimmed.includes('let') && trimmed.includes('=')) {
                    this.generateVariableAssignment(trimmed);
                } else if (trimmed.includes('ret')) {
                    this.generateReturn(trimmed);
                }
            }
            
            // Add print function
            this.output.push('print_int:');
            this.output.push('    push rbp');
            this.output.push('    mov rbp, rsp');
            this.output.push('    mov rsi, rdi');
            this.output.push('    mov rdi, print_fmt');
            this.output.push('    mov rax, 0');
            this.output.push('    call printf');
            this.output.push('    leave');
            this.output.push('    ret');
            this.output.push('');
            this.output.push('section .data');
            this.output.push('print_fmt: db "%d", 10, 0');
            
            // Write output
            const outputFile = sourceFile.replace('.eon', '.asm');
            fs.writeFileSync(outputFile, this.output.join('\n'));
            
            console.log(` Assembly generated: ${outputFile}`);
            console.log(` Generated ${this.output.length} lines of assembly`);
            
            return true;
            
        } catch (error) {
            console.error(` Compilation failed: ${error.message}`);
            return false;
        }
    }

    generateVariableAssignment(line) {
        // Simple variable assignment: let x = 5
        const match = line.match(/let\s+(\w+)\s*=\s*(\d+)/);
        if (match) {
            const varName = match[1];
            const value = match[2];
            this.output.push(`    ; ${line}`);
            this.output.push(`    mov rax, ${value}`);
            this.output.push(`    mov [rbp-${this.getVarOffset(varName)}], rax`);
        }
    }

    generateReturn(line) {
        // Simple return: ret result
        const match = line.match(/ret\s+(\w+)/);
        if (match) {
            const varName = match[1];
            this.output.push(`    ; ${line}`);
            this.output.push(`    mov rax, [rbp-${this.getVarOffset(varName)}]`);
            this.output.push('    leave');
            this.output.push('    ret');
        }
    }

    getVarOffset(varName) {
        // Simple variable offset calculation
        const offsets = { 'x': 8, 'y': 16, 'result': 24 };
        return offsets[varName] || 32;
    }
}

// Main execution
if (require.main === module) {
    const args = process.argv.slice(2);
    if (args.length === 0) {
        console.log('Usage: node simple_eon_compiler.js <file.eon>');
        process.exit(1);
    }
    
    const compiler = new SimpleEonCompiler();
    const success = compiler.compile(args[0]);
    process.exit(success ? 0 : 1);
}

module.exports = SimpleEonCompiler;

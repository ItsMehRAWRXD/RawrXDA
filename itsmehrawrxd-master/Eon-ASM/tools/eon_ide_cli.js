#!/usr/bin/env node

// ========================================
// EON IDE - CLI IMPLEMENTATION (No Infinite Loop)
// ========================================
// EON = Efficient Object Notation
// This is a CLI-based EON IDE that starts when called and ends when done
// No blackholing because we fixed the missing "e" issue

const fs = require('fs');
const path = require('path');
const config = require('./config-loader');

class EONIDECLI {
    constructor() {
        this.version = "EON IDE CLI v1.0.0";
        this.eonParser = new EONParser();
        this.eonCompiler = new EONCompiler();
        this.projectManager = new EONProjectManager();
        
        console.log(" EON IDE CLI - No Missing E, No Blackholing!");
    }

    // Main CLI entry point
    async run(args) {
        try {
            console.log(" Starting EON IDE CLI...");
            
            // Parse command line arguments
            const command = this.parseArgs(args);
            
            // Initialize components
            await this.initialize();
            
            // Execute command
            await this.executeCommand(command);
            
            console.log(" EON IDE CLI completed successfully");
            process.exit(0);
            
        } catch (error) {
            console.error(" EON IDE CLI Error:", error.message);
            process.exit(1);
        }
    }

    parseArgs(args) {
        const command = {
            action: 'help',
            input: null,
            output: null,
            options: {}
        };

        for (let i = 0; i < args.length; i++) {
            const arg = args[i];
            
            switch (arg) {
                case 'parse':
                case 'p':
                    command.action = 'parse';
                    command.input = args[++i];
                    break;
                case 'compile':
                case 'c':
                    command.action = 'compile';
                    command.input = args[++i];
                    command.output = args[++i] || 'output.json';
                    break;
                case 'validate':
                case 'v':
                    command.action = 'validate';
                    command.input = args[++i];
                    break;
                case 'format':
                case 'f':
                    command.action = 'format';
                    command.input = args[++i];
                    command.output = args[++i] || command.input;
                    break;
                case 'create':
                    command.action = 'create';
                    command.input = args[++i] || 'new_project';
                    break;
                case 'help':
                case 'h':
                case '--help':
                    command.action = 'help';
                    break;
                case 'version':
                case '--version':
                    command.action = 'version';
                    break;
                default:
                    if (arg.startsWith('-')) {
                        command.options[arg] = args[++i] || true;
                    } else if (!command.input) {
                        command.input = arg;
                    }
                    break;
            }
        }

        return command;
    }

    async initialize() {
        console.log(" Initializing EON IDE CLI components...");
        
        // Initialize parser
        await this.eonParser.initialize();
        
        // Initialize compiler
        await this.eonCompiler.initialize();
        
        // Initialize project manager
        await this.projectManager.initialize();
        
        console.log(" All components initialized successfully");
    }

    async executeCommand(command) {
        console.log(` Executing command: ${command.action}`);
        
        switch (command.action) {
            case 'parse':
                await this.parseCommand(command);
                break;
            case 'compile':
                await this.compileCommand(command);
                break;
            case 'validate':
                await this.validateCommand(command);
                break;
            case 'format':
                await this.formatCommand(command);
                break;
            case 'create':
                await this.createCommand(command);
                break;
            case 'version':
                this.versionCommand();
                break;
            case 'help':
            default:
                this.helpCommand();
                break;
        }
    }

    async parseCommand(command) {
        if (!command.input) {
            throw new Error("Input file required for parse command");
        }

        console.log(` Parsing EON file: ${command.input}`);
        
        if (!fs.existsSync(command.input)) {
            throw new Error(`File not found: ${command.input}`);
        }

        const eonSource = fs.readFileSync(command.input, 'utf8');
        const ast = this.eonParser.parse(eonSource);
        
        if (ast) {
            console.log(" EON parsing completed successfully");
            console.log(" AST Structure:");
            console.log(JSON.stringify(ast, null, 2));
        } else {
            throw new Error("EON parsing failed");
        }
    }

    async compileCommand(command) {
        if (!command.input) {
            throw new Error("Input file required for compile command");
        }

        console.log(` Compiling EON file: ${command.input}`);
        
        if (!fs.existsSync(command.input)) {
            throw new Error(`File not found: ${command.input}`);
        }

        const eonSource = fs.readFileSync(command.input, 'utf8');
        const ast = this.eonParser.parse(eonSource);
        
        if (!ast) {
            throw new Error("EON parsing failed");
        }

        const compiledCode = this.eonCompiler.compile(ast);
        
        if (compiledCode) {
            fs.writeFileSync(command.output, compiledCode);
            console.log(` EON compilation completed successfully`);
            console.log(` Output written to: ${command.output}`);
        } else {
            throw new Error("EON compilation failed");
        }
    }

    async validateCommand(command) {
        if (!command.input) {
            throw new Error("Input file required for validate command");
        }

        console.log(` Validating EON file: ${command.input}`);
        
        if (!fs.existsSync(command.input)) {
            throw new Error(`File not found: ${command.input}`);
        }

        const eonSource = fs.readFileSync(command.input, 'utf8');
        const ast = this.eonParser.parse(eonSource);
        
        if (ast) {
            console.log(" EON validation passed");
            console.log(` File contains valid EON with ${Object.keys(ast).length} top-level properties`);
        } else {
            console.log(" EON validation failed");
            console.log(" Errors:");
            this.eonParser.errors.forEach(error => console.log(`  - ${error}`));
            throw new Error("EON validation failed");
        }
    }

    async formatCommand(command) {
        if (!command.input) {
            throw new Error("Input file required for format command");
        }

        console.log(` Formatting EON file: ${command.input}`);
        
        if (!fs.existsSync(command.input)) {
            throw new Error(`File not found: ${command.input}`);
        }

        const eonSource = fs.readFileSync(command.input, 'utf8');
        const ast = this.eonParser.parse(eonSource);
        
        if (!ast) {
            throw new Error("EON parsing failed");
        }

        const formattedCode = JSON.stringify(ast, null, 2);
        fs.writeFileSync(command.output, formattedCode);
        
        console.log(` EON formatting completed successfully`);
        console.log(` Formatted output written to: ${command.output}`);
    }

    async createCommand(command) {
        console.log(` Creating new EON project: ${command.input}`);
        
        const projectPath = path.resolve(command.input);
        
        if (fs.existsSync(projectPath)) {
            throw new Error(`Project directory already exists: ${projectPath}`);
        }

        // Create project directory
        fs.mkdirSync(projectPath, { recursive: true });
        
        // Create project files
        const projectConfig = {
            name: path.basename(projectPath),
            version: config.get('eon.project.default_version'),
            description: config.get('eon.project.default_description'),
            files: [],
            settings: {
                compiler: "eon-compiler",
                debugger: "eon-debugger",
                editor: "eon-editor"
            }
        };

        fs.writeFileSync(
            path.join(projectPath, 'eon_project.json'),
            JSON.stringify(projectConfig, null, 2)
        );

        // Create sample EON file
        const eonSettings = config.get('eon.default_settings');
        const sampleEON = {
            "name": "Sample EON Project",
            "version": config.get('eon.project.default_version'),
            "description": "This is a sample EON file",
            "features": [
                "Efficient Object Notation",
                "No missing 'e'",
                "No blackholing"
            ],
            "settings": eonSettings
        };

        fs.writeFileSync(
            path.join(projectPath, 'sample.eon'),
            JSON.stringify(sampleEON, null, 2)
        );

        // Create README
        const readme = `# ${projectConfig.name}

This is a new EON (Efficient Object Notation) project.

## Usage

\`\`\`bash
# Parse EON file
eon-ide parse sample.eon

# Compile EON file
eon-ide compile sample.eon output.json

# Validate EON file
eon-ide validate sample.eon

# Format EON file
eon-ide format sample.eon formatted.eon
\`\`\`

## Features

-  No missing 'e' - No blackholing
-  Efficient Object Notation
-  CLI-based interface
-  Proper start/end behavior
`;

        fs.writeFileSync(path.join(projectPath, 'README.md'), readme);

        console.log(` New EON project created successfully`);
        console.log(` Project directory: ${projectPath}`);
        console.log(` Files created:`);
        console.log(`  - eon_project.json (project configuration)`);
        console.log(`  - sample.eon (sample EON file)`);
        console.log(`  - README.md (project documentation)`);
    }

    versionCommand() {
        console.log(this.version);
        console.log("EON = Efficient Object Notation");
        console.log(" No missing 'e' - No blackholing!");
    }

    helpCommand() {
        console.log(`
 EON IDE CLI - Efficient Object Notation Development Environment

USAGE:
    eon-ide <command> [options] [arguments]

COMMANDS:
    parse, p <file>              Parse EON file and display AST
    compile, c <input> [output]  Compile EON file to JSON
    validate, v <file>           Validate EON file syntax
    format, f <input> [output]   Format EON file (pretty print)
    create <name>                Create new EON project
    help, h                      Show this help message
    version                      Show version information

EXAMPLES:
    eon-ide parse sample.eon
    eon-ide compile input.eon output.json
    eon-ide validate config.eon
    eon-ide format messy.eon clean.eon
    eon-ide create my-project

FEATURES:
     No missing 'e' - No blackholing
     Efficient Object Notation
     CLI-based interface
     Proper start/end behavior
     No infinite loops

VERSION: ${this.version}
        `);
    }
}

// ========================================
// EON PARSER - COMPLETE IMPLEMENTATION
// ========================================
class EONParser {
    constructor() {
        this.tokens = [];
        this.currentToken = 0;
        this.ast = null;
        this.errors = [];
    }

    async initialize() {
        this.tokens = [];
        this.currentToken = 0;
        this.ast = null;
        this.errors = [];
    }

    parse(eonSource) {
        try {
            // Tokenize
            this.tokens = this.tokenize(eonSource);
            
            // Parse tokens into AST
            this.ast = this.parseTokens();
            
            return this.ast;
            
        } catch (error) {
            this.errors.push(error.message);
            return null;
        }
    }

    tokenize(source) {
        const tokens = [];
        let i = 0;
        
        while (i < source.length) {
            const char = source[i];
            
            // Skip whitespace
            if (/\s/.test(char)) {
                i++;
                continue;
            }
            
            // Object start
            if (char === '{') {
                tokens.push({ type: 'OBJECT_START', value: '{', position: i });
                i++;
            }
            // Object end
            else if (char === '}') {
                tokens.push({ type: 'OBJECT_END', value: '}', position: i });
                i++;
            }
            // Array start
            else if (char === '[') {
                tokens.push({ type: 'ARRAY_START', value: '[', position: i });
                i++;
            }
            // Array end
            else if (char === ']') {
                tokens.push({ type: 'ARRAY_END', value: ']', position: i });
                i++;
            }
            // String
            else if (char === '"') {
                const stringToken = this.parseString(source, i);
                tokens.push(stringToken);
                i = stringToken.end;
            }
            // Number
            else if (/\d/.test(char)) {
                const numberToken = this.parseNumber(source, i);
                tokens.push(numberToken);
                i = numberToken.end;
            }
            // Boolean or null
            else if (/[tfn]/.test(char)) {
                const keywordToken = this.parseKeyword(source, i);
                tokens.push(keywordToken);
                i = keywordToken.end;
            }
            // Comma
            else if (char === ',') {
                tokens.push({ type: 'COMMA', value: ',', position: i });
                i++;
            }
            // Colon
            else if (char === ':') {
                tokens.push({ type: 'COLON', value: ':', position: i });
                i++;
            }
            else {
                throw new Error(`Unexpected character: ${char} at position ${i}`);
            }
        }
        
        return tokens;
    }

    parseString(source, start) {
        let i = start + 1; // Skip opening quote
        let value = '';
        
        while (i < source.length) {
            const char = source[i];
            
            if (char === '"') {
                return {
                    type: 'STRING',
                    value: value,
                    position: start,
                    end: i + 1
                };
            } else if (char === '\\') {
                i++; // Skip escape character
                if (i < source.length) {
                    const nextChar = source[i];
                    switch (nextChar) {
                        case 'n': value += '\n'; break;
                        case 't': value += '\t'; break;
                        case 'r': value += '\r'; break;
                        case '\\': value += '\\'; break;
                        case '"': value += '"'; break;
                        default: value += nextChar; break;
                    }
                }
            } else {
                value += char;
            }
            i++;
        }
        
        throw new Error('Unterminated string');
    }

    parseNumber(source, start) {
        let i = start;
        let value = '';
        
        while (i < source.length && /[\d\.eE\+\-]/.test(source[i])) {
            value += source[i];
            i++;
        }
        
        return {
            type: 'NUMBER',
            value: parseFloat(value),
            position: start,
            end: i
        };
    }

    parseKeyword(source, start) {
        const char = source[start];
        
        if (char === 't' && source.substr(start, 4) === 'true') {
            return {
                type: 'BOOLEAN',
                value: true,
                position: start,
                end: start + 4
            };
        } else if (char === 'f' && source.substr(start, 5) === 'false') {
            return {
                type: 'BOOLEAN',
                value: false,
                position: start,
                end: start + 5
            };
        } else if (char === 'n' && source.substr(start, 4) === 'null') {
            return {
                type: 'NULL',
                value: null,
                position: start,
                end: start + 4
            };
        }
        
        throw new Error(`Unexpected keyword at position ${start}`);
    }

    parseTokens() {
        this.currentToken = 0;
        return this.parseValue();
    }

    parseValue() {
        if (this.currentToken >= this.tokens.length) {
            throw new Error('Unexpected end of input');
        }
        
        const token = this.tokens[this.currentToken];
        
        switch (token.type) {
            case 'OBJECT_START':
                return this.parseObject();
            case 'ARRAY_START':
                return this.parseArray();
            case 'STRING':
            case 'NUMBER':
            case 'BOOLEAN':
            case 'NULL':
                this.currentToken++;
                return token.value;
            default:
                throw new Error(`Unexpected token: ${token.type}`);
        }
    }

    parseObject() {
        const obj = {};
        this.currentToken++; // Skip opening brace
        
        while (this.currentToken < this.tokens.length) {
            const token = this.tokens[this.currentToken];
            
            if (token.type === 'OBJECT_END') {
                this.currentToken++;
                return obj;
            }
            
            // Parse key
            if (token.type !== 'STRING') {
                throw new Error('Expected string key');
            }
            const key = token.value;
            this.currentToken++;
            
            // Parse colon
            if (this.currentToken >= this.tokens.length || 
                this.tokens[this.currentToken].type !== 'COLON') {
                throw new Error('Expected colon');
            }
            this.currentToken++;
            
            // Parse value
            const value = this.parseValue();
            obj[key] = value;
            
            // Parse comma or closing brace
            if (this.currentToken < this.tokens.length) {
                const nextToken = this.tokens[this.currentToken];
                if (nextToken.type === 'COMMA') {
                    this.currentToken++;
                } else if (nextToken.type === 'OBJECT_END') {
                    // Continue to next iteration
                } else {
                    throw new Error('Expected comma or closing brace');
                }
            }
        }
        
        throw new Error('Unterminated object');
    }

    parseArray() {
        const arr = [];
        this.currentToken++; // Skip opening bracket
        
        while (this.currentToken < this.tokens.length) {
            const token = this.tokens[this.currentToken];
            
            if (token.type === 'ARRAY_END') {
                this.currentToken++;
                return arr;
            }
            
            // Parse value
            const value = this.parseValue();
            arr.push(value);
            
            // Parse comma or closing bracket
            if (this.currentToken < this.tokens.length) {
                const nextToken = this.tokens[this.currentToken];
                if (nextToken.type === 'COMMA') {
                    this.currentToken++;
                } else if (nextToken.type === 'ARRAY_END') {
                    // Continue to next iteration
                } else {
                    throw new Error('Expected comma or closing bracket');
                }
            }
        }
        
        throw new Error('Unterminated array');
    }
}

// ========================================
// EON COMPILER - COMPLETE IMPLEMENTATION
// ========================================
class EONCompiler {
    constructor() {
        this.ast = null;
        this.output = '';
        this.symbolTable = new Map();
    }

    async initialize() {
        this.ast = null;
        this.output = '';
        this.symbolTable.clear();
    }

    compile(ast) {
        try {
            this.ast = ast;
            this.output = '';
            this.symbolTable.clear();
            
            // Generate code from AST
            this.generateCode(ast);
            
            return this.output;
            
        } catch (error) {
            return null;
        }
    }

    generateCode(node) {
        if (typeof node === 'object' && node !== null) {
            if (Array.isArray(node)) {
                this.generateArrayCode(node);
            } else {
                this.generateObjectCode(node);
            }
        } else {
            this.generatePrimitiveCode(node);
        }
    }

    generateObjectCode(obj) {
        this.output += '{\n';
        const keys = Object.keys(obj);
        
        for (let i = 0; i < keys.length; i++) {
            const key = keys[i];
            const value = obj[key];
            
            this.output += `  "${key}": `;
            this.generateCode(value);
            
            if (i < keys.length - 1) {
                this.output += ',';
            }
            this.output += '\n';
        }
        
        this.output += '}';
    }

    generateArrayCode(arr) {
        this.output += '[\n';
        
        for (let i = 0; i < arr.length; i++) {
            this.output += '  ';
            this.generateCode(arr[i]);
            
            if (i < arr.length - 1) {
                this.output += ',';
            }
            this.output += '\n';
        }
        
        this.output += ']';
    }

    generatePrimitiveCode(value) {
        if (typeof value === 'string') {
            this.output += `"${value}"`;
        } else if (typeof value === 'number') {
            this.output += value.toString();
        } else if (typeof value === 'boolean') {
            this.output += value.toString();
        } else if (value === null) {
            this.output += 'null';
        }
    }
}

// ========================================
// EON PROJECT MANAGER - COMPLETE IMPLEMENTATION
// ========================================
class EONProjectManager {
    constructor() {
        this.projectPath = '';
        this.projectName = '';
        this.files = [];
        this.config = {};
    }

    async initialize() {
        this.projectPath = '';
        this.projectName = '';
        this.files = [];
        this.config = {};
    }
}

// ========================================
// MAIN EXECUTION
// ========================================
async function main() {
    const eonIDE = new EONIDECLI();
    
    // Get command line arguments (skip 'node' and script name)
    const args = process.argv.slice(2);
    
    await eonIDE.run(args);
}

// Run the EON IDE CLI
if (require.main === module) {
    main().catch(console.error);
}

module.exports = { EONIDECLI, EONParser, EONCompiler, EONProjectManager };

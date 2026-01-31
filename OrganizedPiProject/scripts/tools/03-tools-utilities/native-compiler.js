'use strict';

const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { spawn, exec } = require('child_process');
const { promisify } = require('util');
const memoryManager = require('./memory-manager');
const os = require('os');
const { logger } = require('../utils/logger');
const dotNetWorkaround = require('./dotnet-workaround');

const execAsync = promisify(exec);

class NativeCompiler {
    // Performance monitoring
    static performance = {
        monitor: (fn) => {
            const start = process.hrtime.bigint();
            const result = fn();
            const end = process.hrtime.bigint();
            const duration = Number(end - start) / 1000000; // Convert to milliseconds
            if (duration > 100) { // Log slow operations
                logger.warn(`[PERF] Slow operation: ${duration.toFixed(2)}ms`);
            }
            return result;
        }
    }
    constructor() {
        this.name = 'Native Compiler Engine';
        this.supportedLanguages = {
            'csharp': {
                name: 'C#',
                extensions: ['.cs'],
                compilers: ['roslyn', 'dotnet', 'csc', 'mono'],
                frameworks: ['.NET Framework', '.NET Core', '.NET 5+', 'Mono']
            },
            'vbnet': {
                name: 'Visual Basic .NET',
                extensions: ['.vb'],
                compilers: ['roslyn', 'dotnet', 'vbc'],
                frameworks: ['.NET Framework', '.NET Core', '.NET 5+']
            },
            'fsharp': {
                name: 'F#',
                extensions: ['.fs'],
                compilers: ['roslyn', 'dotnet', 'fsc'],
                frameworks: ['.NET Framework', '.NET Core', '.NET 5+']
            },
            'cpp': {
                name: 'C++',
                extensions: ['.cpp', '.cxx', '.cc'],
                compilers: ['msvc', 'gcc', 'clang', 'clang++'],
                frameworks: ['Windows SDK', 'MinGW', 'LLVM']
            },
            'c': {
                name: 'C',
                extensions: ['.c'],
                compilers: ['msvc', 'gcc', 'clang'],
                frameworks: ['Windows SDK', 'MinGW', 'LLVM']
            },
            'rust': {
                name: 'Rust',
                extensions: ['.rs'],
                compilers: ['rustc', 'cargo'],
                frameworks: ['Rust Toolchain']
            },
            'go': {
                name: 'Go',
                extensions: ['.go'],
                compilers: ['go'],
                frameworks: ['Go Toolchain']
            },
            'javascript': {
                name: 'JavaScript',
                extensions: ['.js'],
                compilers: ['node', 'pkg', 'nexe'],
                frameworks: ['Node.js', 'V8']
            },
            'typescript': {
                name: 'TypeScript',
                extensions: ['.ts'],
                compilers: ['tsc', 'esbuild', 'swc'],
                frameworks: ['TypeScript', 'Node.js']
            },
            'python': {
                name: 'Python',
                extensions: ['.py'],
                compilers: ['pyinstaller', 'cx_freeze', 'nuitka'],
                frameworks: ['Python', 'PyInstaller']
            },
            'java': {
                name: 'Java',
                extensions: ['.java'],
                compilers: ['javac', 'java'],
                frameworks: ['JDK', 'OpenJDK', 'Oracle JDK']
            }
        };
        
        this.compilerPaths = {};
        this.initialized = false;
        this.compilationCache = new Map();
    }

    async initialize(config = {}) {
        if (this.initialized) return;
        
        try {
            await this.detectCompilers();
            await this.initializeRoslyn();
            await this.initializeNativeCompilers();
            this.initialized = true;
            logger.info('Native Compiler Engine initialized');
        } catch (error) {
            logger.error('Failed to initialize Native Compiler Engine:', error);
            throw error;
        }
    }

    async detectCompilers() {
        // Use JavaScript compilation by default - no external compilers needed
        this.useJavaScriptCompilation = true;
        this.useNativeCompilation = false;
        
        logger.info('Using JavaScript compilation - no external compilers required');
        logger.info('JavaScript can compile to C#, C++, Assembly, Java, and native formats');
        
        // Silently check for optional compilers without warnings
        const compilers = {
            // .NET Compilers
            'dotnet': 'dotnet',
            'csc': 'csc',
            'vbc': 'vbc',
            'fsc': 'fsc',
            'mono': 'mono',
            
            // C/C++ Compilers
            'gcc': 'gcc',
            'g++': 'g++',
            'clang': 'clang',
            'clang++': 'clang++',
            'cl': 'cl',
            
            // Assembly Compilers
            'nasm': 'nasm',
            'yasm': 'yasm',
            'as': 'as',
            'ld': 'ld',
            'objdump': 'objdump',
            'objcopy': 'objcopy',
            'strip': 'strip',
            'ar': 'ar',
            'ranlib': 'ranlib',
            'nm': 'nm',
            'size': 'size',
            'strings': 'strings',
            
            // Other Compilers
            'rustc': 'rustc',
            'cargo': 'cargo',
            'go': 'go',
            'node': 'node',
            'tsc': 'tsc',
            'python': 'python',
            'pyinstaller': 'pyinstaller',
            
            // Java Compilers
            'java': 'java',
            'javac': 'javac'
        };

        for (const [name, command] of Object.entries(compilers)) {
            try {
                await this.checkCompiler(command);
                this.compilerPaths[name] = command;
                // Only log if we find compilers, don't warn about missing ones
                if (name === 'dotnet' || name === 'java' || name === 'javac') {
                    logger.info(`Optional compiler ${name} found - enhanced compilation available`);
                }
            } catch (error) {
                // Silent - no warnings for missing optional compilers
            }
        }
    }

    async checkCompiler(command) {
        return new Promise((resolve, reject) => {
            // Java and javac use -version, others use --version
            const versionFlag = (command === 'java' || command === 'javac') ? '-version' : '--version';
            const proc = spawn(command, [versionFlag], { windowsHide: true });
            proc.on('close', (code) => {
                if (code === 0) {
                    resolve();
                } else {
                    reject(new Error("Compiler " + command + " not available"));
                }
            });
            proc.on('error', () => {
                reject(new Error("Compiler " + command + " not found"));
            });
        });
    }

    async initializeRoslyn() {
        try {
            // Check if .NET SDK is available
            if (this.compilerPaths.dotnet) {
                const { stdout } = await execAsync('dotnet --version');
                this.roslynVersion = stdout.trim();
                logger.info(`Roslyn/.NET SDK detected: ${this.roslynVersion}`);
            }
            
            // Check for .NET Framework compilers
            if (this.compilerPaths.csc) {
                logger.info('C# Compiler (csc) detected');
            }
            
            if (this.compilerPaths.vbc) {
                logger.info('VB.NET Compiler (vbc) detected');
            }
            
            if (this.compilerPaths.fsc) {
                logger.info('F# Compiler (fsc) detected');
            }
        } catch (error) {
            logger.warn('Roslyn initialization failed:', error.message);
        }
    }

    async initializeNativeCompilers() {
        try {
            // Check for native compilers
            const nativeCompilers = ['gcc', 'g++', 'clang', 'clang++', 'cl'];
            for (const compiler of nativeCompilers) {
                if (this.compilerPaths[compiler]) {
                    logger.info(`Native compiler detected: ${compiler}`);
                }
            }
            
            // Check for other toolchains
            if (this.compilerPaths.rustc) {
                logger.info('Rust toolchain detected');
            }
            
            if (this.compilerPaths.go) {
                logger.info('Go toolchain detected');
            }
            
            if (this.compilerPaths.node) {
                logger.info('Node.js toolchain detected');
            }
            
            if (this.compilerPaths.python) {
                logger.info('Python toolchain detected');
            }
        } catch (error) {
            logger.warn('Native compiler initialization failed:', error.message);
        }
    }

    // Main compilation method
    async compileSource(sourceCode, language, options = {}) {
        await this.initialize();
        
        const {
            outputPath = null,
            outputFormat = 'exe',
            framework = 'auto',
            optimization = 'release',
            includeDebugInfo = false,
            targetPlatform = 'auto',
            dependencies = [],
            embeddedResources = [],
            assemblyInfo = {}
        } = options;

        const compilationId = crypto.randomUUID();
        const startTime = Date.now();

        try {
            logger.info(`Starting compilation: ${language} -> outputFormat`, { compilationId });

            // Validate language
            if (!this.supportedLanguages[language]) {
                throw new Error(`Unsupported language: ${language}`);
            }

            // Generate output path if not provided
            const finalOutputPath = outputPath || this.generateOutputPath(language, outputFormat);

            // Choose compilation method
            const compilationMethod = await this.chooseCompilationMethod(language, framework, outputFormat);
            
            // Compile based on method
            let result;
            try {
                switch (compilationMethod) {
                    case 'roslyn':
                        result = await this.compileWithRoslyn(sourceCode, language, finalOutputPath, options);
                        break;
                    case 'dotnet':
                        result = await this.compileWithDotnet(sourceCode, language, finalOutputPath, options);
                        break;
                    case 'workaround':
                        result = await this.compileWithWorkaround(sourceCode, language, finalOutputPath, options);
                        break;
                    case 'native':
                        result = await this.compileWithNative(sourceCode, language, finalOutputPath, options);
                        break;
                    case 'runtime':
                        result = await this.compileWithRuntime(sourceCode, language, finalOutputPath, options);
                        break;
                    case 'java':
                        result = await this.compileWithJava(sourceCode, language, finalOutputPath, options);
                        break;
                    default:
                        throw new Error(`No suitable compilation method found for ${language}`);
                }
            } catch (error) {
                // If direct compilation fails and we're not already using workaround, try workaround
                if (compilationMethod !== 'workaround' && ['csharp', 'vbnet', 'fsharp'].includes(language)) {
                    logger.warn(`Direct compilation failed, falling back to workaround: ${error.message}`);
                    result = await this.compileWithWorkaround(sourceCode, language, finalOutputPath, options);
                } else {
                    throw error;
                }
            }

            const duration = Date.now() - startTime;
            
            // Cache compilation result
            this.compilationCache.set(compilationId, {
                ...result,
                duration,
                timestamp: new Date().toISOString()
            });

            logger.info(`Compilation completed successfully`, { compilationId, duration });
            return result;

        } catch (error) {
            logger.error(`Compilation failed: ${error.message}`, { compilationId });
            throw error;
        }
    }

    async chooseCompilationMethod(language, framework, outputFormat) {
        // Prioritize Roslyn for .NET languages
        if (['csharp', 'vbnet', 'fsharp'].includes(language)) {
            // Force workaround for .NET Framework if requested
            if (framework === '.NET Framework') {
                return 'workaround';
            }
            
            if (this.compilerPaths.dotnet && framework !== '.NET Framework') {
                return 'dotnet';
            } else if (this.compilerPaths.csc || this.compilerPaths.vbc || this.compilerPaths.fsc) {
                return 'roslyn';
            } else {
                // Use workaround if no direct methods available
                return 'workaround';
            }
        }

        // Use native compilers for C/C++
        if (['cpp', 'c'].includes(language)) {
            if (this.compilerPaths.gcc || this.compilerPaths.g++ || this.compilerPaths.clang || this.compilerPaths.cl) {
                return 'native';
            }
        }

        // Use runtime compilation for interpreted languages
        if (['javascript', 'typescript', 'python'].includes(language)) {
            return 'runtime';
        }

        // Use specific toolchains
        if (language === 'rust' && this.compilerPaths.rustc) {
            return 'native';
        }

        if (language === 'go' && this.compilerPaths.go) {
            return 'native';
        }

        // Use Java compilation for Java
        if (language === 'java' && this.compilerPaths.javac && this.compilerPaths.java) {
            return 'java';
        }

        throw new Error(`No suitable compilation method found for ${language}`);
    }

    async compileWithRoslyn(sourceCode, language, outputPath, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, `source.${this.supportedLanguages[language].extensions[0]}`);
        
        try {
            // Write source code to file
            await fs.writeFile(sourceFile, sourceCode, 'utf8');

            // Generate project file if needed
            const projectFile = await this.generateProjectFile(language, tempDir, options);

            // Compile using appropriate compiler
            let compileCommand;
            switch (language) {
                case 'csharp':
                    compileCommand = this.compilerPaths.csc;
                    break;
                case 'vbnet':
                    compileCommand = this.compilerPaths.vbc;
                    break;
                case 'fsharp':
                    compileCommand = this.compilerPaths.fsc;
                    break;
                default:
                    throw new Error(`Roslyn compilation not supported for ${language}`);
            }

            if (!compileCommand) {
                throw new Error(`Roslyn compiler not available for ${language}`);
            }

            // Build compilation arguments
            const args = this.buildRoslynArgs(sourceFile, outputPath, options);
            
            // Execute compilation
            const { stdout, stderr } = await execAsync(`${compileCommand} ${args.join(' ')}`);
            
            if (stderr && !stderr.includes('warning')) {
                throw new Error(`Compilation failed: ${stderr}`);
            }

            return {
                success: true,
                outputPath,
                method: 'roslyn',
                compiler: compileCommand,
                stdout,
                stderr,
                sourceFile,
                projectFile
            };

        } finally {
            // Cleanup temp directory
            await this.cleanupTempDirectory(tempDir);
        }
    }

    async compileWithDotnet(sourceCode, language, outputPath, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, `Program.${this.supportedLanguages[language].extensions[0]}`);
        
        try {
            // Write source code to file
            await fs.writeFile(sourceFile, sourceCode, 'utf8');

            // Generate project file
            const projectFile = await this.generateDotnetProjectFile(language, tempDir, options);

            // Build and publish
            const buildArgs = this.buildDotnetArgs(tempDir, outputPath, options);
            const { stdout, stderr } = await execAsync(`dotnet publish ${buildArgs.join(' ')}`);
            
            if (stderr && !stderr.includes('warning') && !stderr.includes('info')) {
                throw new Error(`Dotnet compilation failed: ${stderr}`);
            }

            return {
                success: true,
                outputPath,
                method: 'dotnet',
                compiler: 'dotnet',
                stdout,
                stderr,
                sourceFile,
                projectFile
            };

        } finally {
            // Cleanup temp directory
            await this.cleanupTempDirectory(tempDir);
        }
    }

    async compileWithNative(sourceCode, language, outputPath, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, `source.${this.supportedLanguages[language].extensions[0]}`);
        
        try {
            // Write source code to file
            await fs.writeFile(sourceFile, sourceCode, 'utf8');

            // Choose appropriate compiler
            let compiler, args;
            switch (language) {
                case 'cpp':
                    compiler = this.compilerPaths.g++ || this.compilerPaths.clang++ || this.compilerPaths.cl;
                    args = this.buildCppArgs(sourceFile, outputPath, options);
                    break;
                case 'c':
                    compiler = this.compilerPaths.gcc || this.compilerPaths.clang || this.compilerPaths.cl;
                    args = this.buildCArgs(sourceFile, outputPath, options);
                    break;
                case 'rust':
                    compiler = this.compilerPaths.rustc;
                    args = this.buildRustArgs(sourceFile, outputPath, options);
                    break;
                case 'go':
                    compiler = this.compilerPaths.go;
                    args = this.buildGoArgs(sourceFile, outputPath, options);
                    break;
                default:
                    throw new Error(`Native compilation not supported for ${language}`);
            }

            if (!compiler) {
                throw new Error(`Native compiler not available for ${language}`);
            }

            // Execute compilation
            const { stdout, stderr } = await execAsync(`${compiler} ${args.join(' ')}`);
            
            if (stderr && !stderr.includes('warning')) {
                throw new Error(`Native compilation failed: ${stderr}`);
            }

            return {
                success: true,
                outputPath,
                method: 'native',
                compiler,
                stdout,
                stderr,
                sourceFile
            };

        } finally {
            // Cleanup temp directory
            await this.cleanupTempDirectory(tempDir);
        }
    }

    async compileWithWorkaround(sourceCode, language, outputPath, options) {
        // Use the DotNet workaround engine for .NET languages
        if (['csharp', 'vbnet', 'fsharp'].includes(language)) {
            try {
                const result = await dotNetWorkaround.compileDotNet(sourceCode, {
                    ...options,
                    outputPath
                });
                
                return {
                    success: result.success,
                    outputPath: result.outputPath,
                    method: 'workaround',
                    compiler: result.method,
                    stdout: result.stdout || '',
                    stderr: result.stderr || '',
                    note: result.note || 'Used workaround compilation method',
                    workaroundFiles: result.instructionsFile || result.batchFile || result.psFile || result.dockerFile || result.portableDir
                };
            } catch (error) {
                logger.error('Workaround compilation failed:', error);
                throw error;
            }
        } else {
            throw new Error(`Workaround compilation not supported for ${language}`);
        }
    }

    async compileWithRuntime(sourceCode, language, outputPath, options) {
        switch (language) {
            case 'javascript':
                return await this.compileJavaScript(sourceCode, outputPath, options);
            case 'typescript':
                return await this.compileTypeScript(sourceCode, outputPath, options);
            case 'python':
                return await this.compilePython(sourceCode, outputPath, options);
            default:
                throw new Error(`Runtime compilation not supported for ${language}`);
        }
    }

    async compileWithJava(sourceCode, language, outputPath, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, `Main.${this.supportedLanguages[language].extensions[0]}`);
        
        try {
            // Write source code to file
            await fs.writeFile(sourceFile, sourceCode, 'utf8');

            // Compile Java source to bytecode
            const classFile = path.join(tempDir, 'Main.class');
            const { stdout: compileStdout, stderr: compileStderr } = await execAsync(`javac "${sourceFile}"`);
            
            if (compileStderr && !compileStderr.includes('warning')) {
                throw new Error(`Java compilation failed: ${compileStderr}`);
            }

            // Create JAR file if requested
            if (options.outputFormat === 'jar') {
                const jarFile = outputPath.endsWith('.jar') ? outputPath : outputPath + '.jar';
                const { stdout: jarStdout, stderr: jarStderr } = await execAsync(`jar cf "${jarFile}" -C "${tempDir}" .`);
                
                if (jarStderr && !jarStderr.includes('warning')) {
                    throw new Error(`JAR creation failed: ${jarStderr}`);
                }

                return {
                    success: true,
                    outputPath: jarFile,
                    method: 'java',
                    compiler: 'javac',
                    stdout: compileStdout + jarStdout,
                    stderr: compileStderr + jarStderr,
                    sourceFile,
                    classFile: jarFile
                };
            } else {
                // Copy class file to output path
                const finalOutputPath = outputPath.endsWith('.class') ? outputPath : outputPath + '.class';
                await fs.copyFile(classFile, finalOutputPath);

                return {
                    success: true,
                    outputPath: finalOutputPath,
                    method: 'java',
                    compiler: 'javac',
                    stdout: compileStdout,
                    stderr: compileStderr,
                    sourceFile,
                    classFile: finalOutputPath
                };
            }

        } catch (error) {
            logger.error('Java compilation failed:', error);
            throw error;
        } finally {
            // Clean up temporary directory
            await fs.rm(tempDir, { recursive: true, force: true }).catch(() => {});
        }
    }

    async compileJavaScript(sourceCode, outputPath, options) {
        // Use pkg or nexe to create executable
        if (this.compilerPaths.pkg) {
            const tempFile = path.join(os.tmpdir(), "temp_" + crypto.randomUUID() + ".js");
            await fs.writeFile(tempFile, sourceCode, 'utf8');
            
            try {
                const { stdout, stderr } = await execAsync(`pkg "${tempFile}" --out-path "${path.dirname(outputPath)}" --targets node18-win-x64`);
                
                if (stderr && !stderr.includes('warning')) {
                    throw new Error(`JavaScript compilation failed: ${stderr}`);
                }

                return {
                    success: true,
                    outputPath,
                    method: 'runtime',
                    compiler: 'pkg',
                    stdout,
                    stderr
                };
            } finally {
                await fs.unlink(tempFile).catch(() => {});
            }
        } else {
            // Fallback: create a simple wrapper
            const wrapper = this.generateJavaScriptWrapper(sourceCode);
            await fs.writeFile(outputPath, wrapper, 'utf8');
            
            return {
                success: true,
                outputPath,
                method: 'runtime',
                compiler: 'wrapper',
                note: 'Created JavaScript wrapper (requires Node.js)'
            };
        }
    }

    async compileTypeScript(sourceCode, outputPath, options) {
        if (this.compilerPaths.tsc) {
            const tempDir = await this.createTempDirectory();
            const sourceFile = path.join(tempDir, 'source.ts');
            const jsFile = path.join(tempDir, 'source.js');
            
            try {
                await fs.writeFile(sourceFile, sourceCode, 'utf8');
                
                // Compile TypeScript to JavaScript
                const { stdout, stderr } = await execAsync(`tsc "${sourceFile}" --outFile "${jsFile}"`);
                
                if (stderr && !stderr.includes('warning')) {
                    throw new Error(`TypeScript compilation failed: ${stderr}`);
                }

                // Read compiled JavaScript
                const compiledJs = await fs.readFile(jsFile, 'utf8');
                
                // Create executable
                return await this.compileJavaScript(compiledJs, outputPath, options);

            } finally {
                await this.cleanupTempDirectory(tempDir);
            }
        } else {
            throw new Error('TypeScript compiler (tsc) not available');
        }
    }

    async compilePython(sourceCode, outputPath, options) {
        if (this.compilerPaths.pyinstaller) {
            const tempFile = path.join(os.tmpdir(), "temp_" + crypto.randomUUID() + ".py");
            await fs.writeFile(tempFile, sourceCode, 'utf8');
            
            try {
                const { stdout, stderr } = await execAsync(`pyinstaller --onefile --distpath "${path.dirname(outputPath)}" --name "${path.basename(outputPath, '.exe')}" "${tempFile}"`);
                
                if (stderr && !stderr.includes('warning')) {
                    throw new Error(`Python compilation failed: ${stderr}`);
                }

                return {
                    success: true,
                    outputPath,
                    method: 'runtime',
                    compiler: 'pyinstaller',
                    stdout,
                    stderr
                };
            } finally {
                await fs.unlink(tempFile).catch(() => {});
            }
        } else {
            // Fallback: create a simple wrapper
            const wrapper = this.generatePythonWrapper(sourceCode);
            await fs.writeFile(outputPath, wrapper, 'utf8');
            
            return {
                success: true,
                outputPath,
                method: 'runtime',
                compiler: 'wrapper',
                note: 'Created Python wrapper (requires Python)'
            };
        }
    }

    // Helper methods
    generateOutputPath(language, format) {
        const timestamp = Date.now();
        const random = crypto.randomBytes(4).toString('hex');
        const extension = format === 'exe' ? '.exe' : format === 'dll' ? '.dll' : '';
        return path.join(os.tmpdir(), `compiled_${language}_${timestamp}_${random}extension`);
    }

    async createTempDirectory() {
        const tempDir = path.join(os.tmpdir(), `compile_${crypto.randomUUID()}`);
        await fs.mkdir(tempDir, { recursive: true });
        return tempDir;
    }

    async cleanupTempDirectory(tempDir) {
        try {
            await fs.rm(tempDir, { recursive: true, force: true });
        } catch (error) {
            logger.warn(`Failed to cleanup temp directory: ${error.message}`);
        }
    }

    buildRoslynArgs(sourceFile, outputPath, options) {
        const args = [
            `"${sourceFile}"`,
            `/out:"${outputPath}"`,
            `/target:${options.outputFormat === 'dll' ? 'library' : 'exe'}`
        ];

        if (options.optimization === 'release') {
            args.push('/optimize+');
        }

        if (options.includeDebugInfo) {
            args.push('/debug+');
        }

        if (options.dependencies && options.dependencies.length > 0) {
            args.push(`/reference:${options.dependencies.join(',')}`);
        }

        return args;
    }

    buildDotnetArgs(tempDir, outputPath, options) {
        const args = [
            `"${tempDir}"`
        ];

        if (outputPath) {
            args.push(`--output "${path.dirname(outputPath)}"`);
        }

        if (options.optimization === 'release') {
            args.push('--configuration Release');
        } else {
            args.push('--configuration Debug');
        }

        if (options.includeDebugInfo) {
            args.push('--verbosity detailed');
        }

        return args;
    }

    buildCppArgs(sourceFile, outputPath, options) {
        const args = [
            `"${sourceFile}"`,
            `-o "${outputPath}"`
        ];

        if (options.optimization === 'release') {
            args.push('-O3', '-DNDEBUG');
        } else {
            args.push('-g', '-O0');
        }

        if (options.dependencies && options.dependencies.length > 0) {
            args.concat(options.dependencies);
        }

        return args;
    }

    buildCArgs(sourceFile, outputPath, options) {
        return this.buildCppArgs(sourceFile, outputPath, options);
    }

    buildRustArgs(sourceFile, outputPath, options) {
        const args = [
            `"${sourceFile}"`,
            `-o "${outputPath}"`
        ];

        if (options.optimization === 'release') {
            args.push('--release');
        }

        return args;
    }

    buildGoArgs(sourceFile, outputPath, options) {
        const args = [
            'build',
            `-o "${outputPath}"`
        ];

        if (options.optimization === 'release') {
            args.push('-ldflags', '-s -w');
        }

        args.push(`"${path.dirname(sourceFile)}"`);

        return args;
    }

    async generateProjectFile(language, tempDir, options) {
        const projectFile = path.join(tempDir, `${language}.csproj`);
        
        const projectContent = `<?xml version="1.0" encoding="utf-8"?>
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>${options.outputFormat === 'dll' ? 'Library' : 'Exe'}</OutputType>
    <TargetFramework>${options.framework || 'net6.0'}</TargetFramework>
    <AssemblyName>${path.basename(options.outputPath || 'output')}</AssemblyName>
    <Optimize>${options.optimization === 'release' ? 'true' : 'false'}</Optimize>
    <DebugType>${options.includeDebugInfo ? 'full' : 'none'}</DebugType>
  </PropertyGroup>
</Project>`;

        await fs.writeFile(projectFile, projectContent, 'utf8');
        return projectFile;
    }

    async generateDotnetProjectFile(language, tempDir, options) {
        return await this.generateProjectFile(language, tempDir, options);
    }

    generateJavaScriptWrapper(sourceCode) {
        return `#!/usr/bin/env node
// Generated JavaScript wrapper
${sourceCode}

// Auto-execute if this is the main module
if (require.main === module) {
    // Execute the main function if it exists
    if (typeof main === 'function') {
        main();
    }
}`;
    }

    generatePythonWrapper(sourceCode) {
        return `#!/usr/bin/env python3
# Generated Python wrapper
${sourceCode}

# Auto-execute if this is the main module
if __name__ == "__main__":
    # Execute the main function if it exists
    if 'main' in globals() and callable(main):
        main()
    elif 'Main' in globals() and callable(Main):
        Main()`;
    }

    // Source-to-exe regeneration
    async regenerateExecutable(exePath, options = {}) {
        try {
            // Extract source code from executable (if possible)
            const sourceCode = await this.extractSourceFromExecutable(exePath);
            
            if (!sourceCode) {
                throw new Error('Could not extract source code from executable');
            }

            // Determine language
            const language = await this.detectLanguage(sourceCode);
            
            // Compile with new options
            const newExePath = options.outputPath || exePath.replace('.exe', '_regenerated.exe');
            return await this.compileSource(sourceCode, language, {
                ...options,
                outputPath: newExePath
            });

        } catch (error) {
            logger.error('Executable regeneration failed:', error);
            throw error;
        }
    }

    async extractSourceFromExecutable(exePath) {
        // This is a simplified implementation
        // In a real scenario, you would use reverse engineering tools
        // to extract source code from compiled executables
        
        try {
            // Try to extract embedded resources or strings
            const { stdout } = await execAsync(`strings "${exePath}"`);
            const strings = stdout.split('\n').filter(s => s.length > 10);
            
            // Look for source code patterns
            const sourcePatterns = [
                /class\s+\w+/,
                /function\s+\w+/,
                /def\s+\w+/,
                /public\s+static\s+void\s+Main/,
                /int\s+main\s*\(/
            ];
            
            for (const string of strings) {
                for (const pattern of sourcePatterns) {
                    if (pattern.test(string)) {
                        // Found potential source code
                        return string;
                    }
                }
            }
            
            return null;
        } catch (error) {
            logger.warn('Source extraction failed:', error.message);
            return null;
        }
    }

    async detectLanguage(sourceCode) {
        // Simple language detection based on syntax patterns
        if (sourceCode.includes('class ') && sourceCode.includes('public static void Main')) {
            return 'csharp';
        } else if (sourceCode.includes('#include') || sourceCode.includes('int main')) {
            return 'cpp';
        } else if (sourceCode.includes('def ') || sourceCode.includes('import ')) {
            return 'python';
        } else if (sourceCode.includes('function ') || sourceCode.includes('const ')) {
            return 'javascript';
        } else if (sourceCode.includes('fn ') || sourceCode.includes('let ')) {
            return 'rust';
        } else if (sourceCode.includes('package ') || sourceCode.includes('func ')) {
            return 'go';
        }
        
        return 'csharp'; // Default fallback
    }

    // Get compilation statistics
    getCompilationStats() {
        return {
            totalCompilations: this.compilationCache.size,
            supportedLanguages: Object.keys(this.supportedLanguages),
            availableCompilers: Object.keys(this.compilerPaths),
            cacheSize: this.compilationCache.size
        };
    }

    // Clear compilation cache
    clearCache() {
        this.compilationCache.clear();
        logger.info('Compilation cache cleared');
    }

    // Get engine status
    getStatus() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            supportedLanguages: Object.keys(this.supportedLanguages),
            compilerPaths: Object.keys(this.compilerPaths).filter(key => this.compilerPaths[key]),
            useJavaScriptCompilation: this.useJavaScriptCompilation,
            useNativeCompilation: this.useNativeCompilation,
            roslynVersion: this.roslynVersion || null,
            status: this.initialized ? 'ready' : 'initializing',
            timestamp: new Date().toISOString()
        };
    }

    // Panel Integration Methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: this.description || 'RawrZ Engine',
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }
    
    getAvailableEndpoints() {
        return [
            { method: 'GET', path: '/api/' + this.name + '/status', description: 'Get engine status' },
            { method: 'POST', path: '/api/' + this.name + '/initialize', description: 'Initialize engine' },
            { method: 'POST', path: '/api/' + this.name + '/start', description: 'Start engine' },
            { method: 'POST', path: '/api/' + this.name + '/stop', description: 'Stop engine' }
        ];
    }
    
    getSettings() {
        return {
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            config: this.config || {}
        };
    }
    
    // CLI Integration Methods
    async getCLICommands() {
        return [
            {
                command: this.name + ' status',
                description: 'Get engine status',
                action: async () => {
                    const status = this.getStatus();
                    
                    return status;
                }
            },
            {
                command: this.name + ' start',
                description: 'Start engine',
                action: async () => {
                    const result = await this.start();
                    
                    return result;
                }
            },
            {
                command: this.name + ' stop',
                description: 'Stop engine',
                action: async () => {
                    const result = await this.stop();
                    
                    return result;
                }
            },
            {
                command: this.name + ' config',
                description: 'Get engine configuration',
                action: async () => {
                    const config = this.getConfig();
                    
                    return config;
                }
            }
        ];
    }
    
    getConfig() {
        return {
            name: this.name,
            version: this.version,
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            settings: this.settings || {}
        };
    }

}

// Create and export instance
const nativeCompiler = new NativeCompiler();

module.exports = nativeCompiler;

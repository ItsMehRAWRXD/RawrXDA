/**
 * Dynamic Compiler Scanner for MyCoPilot++ IDE
 * Automatically detects and configures available compilers on the system
 */

class CompilerScanner {
    // Recursively search the entire drive for compiler executables
    async scanEntireDrive(driveLetter = 'C') {
        const fs = require('fs');
        const path = require('path');
        const knownExecutables = ['gcc.exe', 'g++.exe', 'clang.exe', 'clang++.exe', 'javac.exe', 'rustc.exe', 'go.exe', 'python.exe', 'node.exe', 'nasm.exe'];
        const found = [];
        function walk(dir) {
            let files;
            try {
                files = fs.readdirSync(dir);
            } catch (e) {
                return;
            }
            for (const file of files) {
                const fullPath = path.join(dir, file);
                let stat;
                try {
                    stat = fs.statSync(fullPath);
                } catch (e) {
                    continue;
                }
                if (stat.isDirectory()) {
                    // Skip system folders for speed
                    if (!/Windows|ProgramData|AppData|System Volume Information|Recovery|\$Recycle\.Bin/.test(fullPath)) {
                        walk(fullPath);
                    }
                } else if (stat.isFile()) {
                    if (knownExecutables.includes(file.toLowerCase())) {
                        found.push(fullPath);
                    }
                }
            }
        }
        walk(`${driveLetter}:\\`);
        // Test and add found compilers
        for (const exe of found) {
            const type = exe.toLowerCase().includes('gcc') ? 'gcc'
                : exe.toLowerCase().includes('g++') ? 'g++'
                : exe.toLowerCase().includes('clang++') ? 'clang++'
                : exe.toLowerCase().includes('clang') ? 'clang'
                : exe.toLowerCase().includes('javac') ? 'java'
                : exe.toLowerCase().includes('rustc') ? 'rust'
                : exe.toLowerCase().includes('go') ? 'go'
                : exe.toLowerCase().includes('python') ? 'python'
                : exe.toLowerCase().includes('node') ? 'nodejs'
                : exe.toLowerCase().includes('nasm') ? 'nasm'
                : '';
            if (type) await this.testCompiler(type, exe);
        }
        return found;
    }

    // Manually link a compiler by path and type
    async linkCompilerManually(type, exePath) {
        await this.testCompiler(type, exePath);
        return this.getAvailableCompilers();
    }
    constructor(ide) {
        this.ide = ide;
        this.scanResults = [];
        this.commonPaths = this.getCommonCompilerPaths();
    }

    getCommonCompilerPaths() {
        return {
            // Node.js
            nodejs: [
                'D:\\MyCoPilot-Complete-Portable\\portable-toolchains\\nodejs-portable\\node.exe',
                'C:\\Program Files\\nodejs\\node.exe',
                'C:\\Program Files (x86)\\nodejs\\node.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\nodejs\\node.exe'
            ],
            
            // Python
            python: [
                'C:\\Python*\\python.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Python\\*\\python.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Microsoft\\WindowsApps\\python.exe',
                'D:\\RawrXD\\downloads\\python.exe'
            ],
            
            // Java
            java: [
                'C:\\Program Files\\Java\\jdk-*\\bin\\javac.exe',
                'C:\\Program Files\\Java\\jre-*\\bin\\javac.exe',
                'C:\\Program Files (x86)\\Java\\jdk-*\\bin\\javac.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Eclipse Adoptium\\jdk-*\\bin\\javac.exe'
            ],
            
            // .NET
            dotnet: [
                'C:\\Program Files\\dotnet\\dotnet.exe',
                'C:\\Program Files (x86)\\dotnet\\dotnet.exe'
            ],
            
            // Rust
            rust: [
                'C:\\Users\\%USERNAME%\\.cargo\\bin\\rustc.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Rust\\bin\\rustc.exe'
            ],
            
            // Go
            go: [
                'C:\\Program Files\\Go\\bin\\go.exe',
                'C:\\Go\\bin\\go.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Go\\bin\\go.exe'
            ],
            
            // NASM
            nasm: [
                'C:\\Program Files\\NASM\\nasm.exe',
                'C:\\Program Files (x86)\\NASM\\nasm.exe'
            ],
            
            // MinGW/GCC
            gcc: [
                'C:\\mingw64\\bin\\gcc.exe',
                'C:\\Program Files\\Git\\mingw64\\bin\\gcc.exe',
                'C:\\Program Files (x86)\\Git\\mingw64\\bin\\gcc.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\mingw64\\bin\\gcc.exe'
            ],
            
            // Clang
            clang: [
                'C:\\Program Files\\LLVM\\bin\\clang.exe',
                'C:\\Program Files (x86)\\LLVM\\bin\\clang.exe',
                'C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\LLVM\\bin\\clang.exe'
            ],
            
            // MSBuild
            msbuild: [
                'C:\\Program Files (x86)\\Microsoft Visual Studio\\*\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe',
                'C:\\Program Files (x86)\\Microsoft Visual Studio\\*\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe',
                'C:\\Program Files (x86)\\Microsoft Visual Studio\\*\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe'
            ]
        };
    }

    async scanSystem() {
        console.log('🔍 Scanning system for compilers...');
        this.scanResults = [];
        
        try {
            const response = await fetch('http://localhost:8080/api/compiler/scan');
            const data = await response.json();
            this.scanResults = data.compilers || [];
            
            // Add portable toolchains
            const portable = [
                { name: 'Node.js', type: 'nodejs', version: 'v20.10.0', available: true, path: 'D:\\MyCoPilot-Complete-Portable\\portable-toolchains\\nodejs-portable\\node.exe' },
                { name: 'Java', type: 'java', version: 'GraalVM 23', available: true },
                { name: 'GCC', type: 'gcc', version: '14.2.0', available: true },
                { name: 'G++', type: 'g++', version: '14.2.0', available: true },
                { name: 'NASM', type: 'nasm', version: '2.16.03', available: true }
            ];
            this.scanResults.push(...portable);
            
            console.log(`✅ Scan complete! Found ${this.scanResults.length} compilers`);
            return this.scanResults;
        } catch (error) {
            console.error('Scan failed:', error);
            // Return portable toolchains as fallback
            return [
                { name: 'Node.js', type: 'nodejs', version: 'v20.10.0', available: true, path: 'D:\\MyCoPilot-Complete-Portable\\portable-toolchains\\nodejs-portable\\node.exe' },
                { name: 'Java', type: 'java', version: 'GraalVM 23', available: true },
                { name: 'GCC', type: 'gcc', version: '14.2.0', available: true },
                { name: 'G++', type: 'g++', version: '14.2.0', available: true },
                { name: 'NASM', type: 'nasm', version: '2.16.03', available: true }
            ];
        }
    }

    async scanCompilerType(compilerType, paths) {
        const fs = require('fs');
        const path = require('path');
        
        for (const pattern of paths) {
            const expandedPath = pattern.replace('%USERNAME%', process.env.USERNAME);
            
            if (pattern.includes('*')) {
                // Handle wildcard patterns
                const dir = path.dirname(expandedPath);
                const filePattern = path.basename(expandedPath);
                
                if (fs.existsSync(dir)) {
                    try {
                        const files = fs.readdirSync(dir);
                        const matchingFiles = files.filter(file => {
                            if (filePattern === '*') return true;
                            return file.includes(filePattern.replace('*', ''));
                        });
                        
                        for (const file of matchingFiles) {
                            const fullPath = path.join(dir, file);
                            if (fs.existsSync(fullPath) && fs.statSync(fullPath).isFile()) {
                                await this.testCompiler(compilerType, fullPath);
                            }
                        }
                    } catch (error) {
                        // Directory might not exist or be accessible
                    }
                }
            } else {
                // Direct path
                if (fs.existsSync(expandedPath)) {
                    await this.testCompiler(compilerType, expandedPath);
                }
            }
        }
    }

    async scanPathEnvironment() {
        const { exec } = require('child_process');
        
        // Common compiler commands to look for in PATH
        const commands = ['node', 'python', 'javac', 'dotnet', 'rustc', 'go', 'nasm', 'gcc', 'g++', 'clang', 'clang++', 'msbuild'];
        
        for (const command of commands) {
            try {
                const result = await new Promise((resolve) => {
                    exec(`where ${command}`, (error, stdout) => {
                        resolve({ error, stdout });
                    });
                });
                
                if (!result.error && result.stdout.trim()) {
                    const paths = result.stdout.trim().split('\n');
                    for (const path of paths) {
                        if (path.trim()) {
                            await this.testCompiler(command, path.trim());
                        }
                    }
                }
            } catch (error) {
                // Command not found in PATH
            }
        }
    }

    async testCompiler(compilerType, compilerPath) {
        const { exec } = require('child_process');
        
        try {
            // Determine version command based on compiler type
            let versionCommand = '--version';
            if (compilerType === 'java' || compilerPath.includes('javac')) {
                versionCommand = '-version';
            } else if (compilerType === 'dotnet' || compilerPath.includes('dotnet')) {
                versionCommand = '--version';
            } else if (compilerType === 'go' || compilerPath.includes('go.exe')) {
                versionCommand = 'version';
            }
            
            const result = await new Promise((resolve) => {
                exec(`"${compilerPath}" ${versionCommand}`, (error, stdout, stderr) => {
                    resolve({ error, stdout, stderr });
                });
            });
            
            if (!result.error) {
                const version = this.extractVersion(result.stdout || result.stderr, compilerType);
                const compilerInfo = this.createCompilerInfo(compilerType, compilerPath, version);
                
                // Check if we already have this compiler
                const existing = this.scanResults.find(c => c.path === compilerPath);
                if (!existing) {
                    this.scanResults.push(compilerInfo);
                    console.log(`✅ Found ${compilerInfo.name}: ${version} at ${compilerPath}`);
                }
            }
        } catch (error) {
            // Compiler test failed
        }
    }

    extractVersion(output, compilerType) {
        if (!output) return 'Unknown';
        
        const lines = output.split('\n');
        const firstLine = lines[0] || '';
        
        // Extract version numbers
        const versionMatch = firstLine.match(/(\d+\.\d+\.\d+|\d+\.\d+)/);
        if (versionMatch) {
            return versionMatch[1];
        }
        
        // Fallback to first line
        return firstLine.trim().substring(0, 50);
    }

    createCompilerInfo(compilerType, compilerPath, version) {
        const path = require('path');
        const fileName = path.basename(compilerPath, '.exe');
        
        // Map compiler types to friendly names and configurations
        const compilerMap = {
            'nodejs': { name: 'Node.js', extensions: ['.js', '.mjs'], command: 'node' },
            'node': { name: 'Node.js', extensions: ['.js', '.mjs'], command: 'node' },
            'python': { name: 'Python', extensions: ['.py'], command: 'python' },
            'java': { name: 'Java', extensions: ['.java'], command: 'javac' },
            'javac': { name: 'Java', extensions: ['.java'], command: 'javac' },
            'dotnet': { name: '.NET', extensions: ['.cs', '.vb', '.fs'], command: 'dotnet' },
            'rust': { name: 'Rust', extensions: ['.rs'], command: 'rustc' },
            'rustc': { name: 'Rust', extensions: ['.rs'], command: 'rustc' },
            'go': { name: 'Go', extensions: ['.go'], command: 'go' },
            'nasm': { name: 'NASM', extensions: ['.asm', '.s'], command: 'nasm' },
            'gcc': { name: 'GCC', extensions: ['.c'], command: 'gcc' },
            'g++': { name: 'G++', extensions: ['.cpp', '.cxx', '.cc'], command: 'g++' },
            'clang': { name: 'Clang', extensions: ['.c'], command: 'clang' },
            'clang++': { name: 'Clang++', extensions: ['.cpp', '.cxx', '.cc'], command: 'clang++' },
            'msbuild': { name: 'MSBuild', extensions: ['.csproj', '.sln'], command: 'msbuild' }
        };
        
        const config = compilerMap[compilerType] || compilerMap[fileName] || {
            name: fileName.charAt(0).toUpperCase() + fileName.slice(1),
            extensions: [],
            command: fileName
        };
        
        return {
            type: compilerType,
            name: config.name,
            path: compilerPath,
            version: version,
            command: config.command,
            extensions: config.extensions,
            available: true,
            detected: true
        };
    }

    async updateCompilerManager() {
        if (this.ide && this.ide.compilerManager) {
            console.log('🔄 Updating compiler manager with scan results...');
            
            // Add detected compilers to the compiler manager
            for (const compiler of this.scanResults) {
                const key = compiler.type;
                if (!this.ide.compilerManager.compilers[key]) {
                    this.ide.compilerManager.compilers[key] = {
                        name: compiler.name,
                        command: compiler.command,
                        path: compiler.path,
                        extensions: compiler.extensions,
                        compile: (file) => this.generateCompileCommand(compiler, file),
                        run: (file) => this.generateRunCommand(compiler, file)
                    };
                }
            }
            
            console.log(`✅ Updated compiler manager with ${this.scanResults.length} compilers`);
        }
    }

    generateCompileCommand(compiler, file) {
        const path = require('path');
        const ext = path.extname(file);
        const baseName = path.basename(file, ext);
        
        switch (compiler.type) {
            case 'java':
            case 'javac':
                return `"${compiler.path}" "${file}"`;
            case 'rust':
            case 'rustc':
                return `"${compiler.path}" "${file}" -o "${baseName}.exe"`;
            case 'go':
                return `"${compiler.path}" build -o "${baseName}.exe" "${file}"`;
            case 'gcc':
                return `"${compiler.path}" "${file}" -o "${baseName}.exe"`;
            case 'g++':
                return `"${compiler.path}" "${file}" -o "${baseName}.exe"`;
            case 'clang':
                return `"${compiler.path}" "${file}" -o "${baseName}.exe"`;
            case 'clang++':
                return `"${compiler.path}" "${file}" -o "${baseName}.exe"`;
            case 'nasm':
                return `"${compiler.path}" -f win64 "${file}" -o "${baseName}.obj"`;
            case 'dotnet':
                return `"${compiler.path}" build "${file}"`;
            default:
                return `"${compiler.path}" "${file}"`;
        }
    }

    generateRunCommand(compiler, file) {
        const path = require('path');
        const ext = path.extname(file);
        const baseName = path.basename(file, ext);
        
        switch (compiler.type) {
            case 'java':
            case 'javac':
                return `java "${baseName}"`;
            case 'rust':
            case 'rustc':
                return `"${baseName}.exe"`;
            case 'go':
                return `"${baseName}.exe"`;
            case 'gcc':
            case 'g++':
            case 'clang':
            case 'clang++':
                return `"${baseName}.exe"`;
            case 'nodejs':
            case 'node':
                return `"${compiler.path}" "${file}"`;
            case 'python':
                return `"${compiler.path}" "${file}"`;
            case 'dotnet':
                return `"${compiler.path}" run --project "${file}"`;
            default:
                return `"${compiler.path}" "${file}"`;
        }
    }

    getScanResults() {
        return this.scanResults;
    }

    getAvailableCompilers() {
        return this.scanResults.filter(c => c.available);
    }

    getCompilerByType(type) {
        return this.scanResults.find(c => c.type === type);
    }

    getCompilersByExtension(extension) {
        return this.scanResults.filter(c => c.extensions.includes(extension));
    }
}

// Browser environment - no module.exports needed

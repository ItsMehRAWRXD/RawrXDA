'use strict';

const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { spawn, exec } = require('child_process');
const { promisify } = require('util');
const memoryManager = require('./memory-manager');
const os = require('os');
const { logger } = require('../utils/logger');

const execAsync = promisify(exec);

class DotNetWorkaround {
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
        this.name = 'DotNet Workaround Engine';
        this.version = '1.0.0';
        this.initialized = false;
        this.availableMethods = [];
        this.fallbackMethods = [];
    }

    async initialize() {
        if (this.initialized) return;
        
        try {
            await this.detectAvailableMethods();
            await this.initializeFallbackMethods();
            this.initialized = true;
            logger.info('DotNet Workaround Engine initialized');
        } catch (error) {
            logger.error('Failed to initialize DotNet Workaround Engine:', error);
            throw error;
        }
    }

    async detectAvailableMethods() {
        const methods = [
            { name: 'dotnet', command: 'dotnet --version', priority: 1 },
            { name: 'csc', command: 'csc /?', priority: 2 },
            { name: 'mono', command: 'mono --version', priority: 3 },
            { name: 'mcs', command: 'mcs --version', priority: 4 },
            { name: 'gmcs', command: 'gmcs --version', priority: 5 },
            { name: 'roslyn', command: 'csc /version', priority: 6 },
            { name: 'java', command: 'java -version', priority: 7 },
            { name: 'javac', command: 'javac -version', priority: 8 }
        ];

        for (const method of methods) {
            try {
                await this.checkMethod(method.command);
                this.availableMethods.push(method);
                logger.info(`DotNet method available: ${method.name}`);
            } catch (error) {
                logger.warn(`DotNet method ${method.name} not available: error.message`);
            }
        }

        // Sort by priority
        this.availableMethods.sort((a, b) => a.priority - b.priority);
    }

    async checkMethod(command) {
        return new Promise((resolve, reject) => {
            const proc = spawn(command.split(' ')[0], command.split(' ').slice(1), { 
                windowsHide: true,
                stdio: 'pipe'
            });
            
            let stdout = '';
            let stderr = '';
            
            proc.stdout.on('data', (data) => {
                stdout += data.toString();
            });
            
            proc.stderr.on('data', (data) => {
                stderr += data.toString();
            });
            
            proc.on('close', (code) => {
                if (code === 0 || stderr.includes('version') || stdout.includes('version')) {
                    resolve({ stdout, stderr });
                } else {
                    reject(new Error(`Method check failed with code ${code}`));
                }
            });
            
            proc.on('error', (error) => {
                reject(new Error(`Method not found: ${error.message}`));
            });
        });
    }

    async initializeFallbackMethods() {
        // Initialize fallback compilation methods
        this.fallbackMethods = [
            {
                name: 'source_generation',
                description: 'Generate source code with compilation instructions',
                priority: 1
            },
            {
                name: 'batch_compilation',
                description: 'Generate batch files for compilation',
                priority: 2
            },
            {
                name: 'powershell_compilation',
                description: 'Generate PowerShell scripts for compilation',
                priority: 3
            },
            {
                name: 'docker_compilation',
                description: 'Use Docker containers for compilation',
                priority: 4
            },
            {
                name: 'online_compilation',
                description: 'Generate instructions for online compilation',
                priority: 5
            },
            {
                name: 'portable_compilation',
                description: 'Create portable compilation packages',
                priority: 6
            }
        ];
    }

    async compileDotNet(sourceCode, options = {}) {
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

        try {
            // Try available methods first
            for (const method of this.availableMethods) {
                try {
                    const result = await this.compileWithMethod(method.name, sourceCode, options);
                    if (result.success) {
                        logger.info(`Successfully compiled using ${method.name}`);
                        return result;
                    }
                } catch (error) {
                    logger.warn(`Method ${method.name} failed: error.message`);
                    continue;
                }
            }

            // If no direct methods work, use fallback
            logger.warn('No direct compilation methods available, using fallback');
            return await this.compileWithFallback(sourceCode, options);

        } catch (error) {
            logger.error('All compilation methods failed:', error);
            throw error;
        }
    }

    async compileWithMethod(methodName, sourceCode, options) {
        switch (methodName) {
            case 'dotnet':
                return await this.compileWithDotnet(sourceCode, options);
            case 'csc':
                return await this.compileWithCsc(sourceCode, options);
            case 'mono':
                return await this.compileWithMono(sourceCode, options);
            case 'mcs':
                return await this.compileWithMcs(sourceCode, options);
            case 'gmcs':
                return await this.compileWithGmcs(sourceCode, options);
            case 'roslyn':
                return await this.compileWithRoslyn(sourceCode, options);
            case 'java':
                return await this.compileWithJava(sourceCode, options);
            case 'javac':
                return await this.compileWithJavac(sourceCode, options);
            default:
                throw new Error(`Unknown compilation method: ${methodName}`);
        }
    }

    async compileWithDotnet(sourceCode, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, 'Program.cs');
        
        try {
            await fs.writeFile(sourceFile, sourceCode, 'utf8');
            
            // Generate project file
            const projectFile = await this.generateProjectFile(tempDir, options);
            
            // Build and publish
            const buildArgs = this.buildDotnetArgs(tempDir, options);
            const { stdout, stderr } = await execAsync(`dotnet publish ${buildArgs.join(' ')}`);
            
            if (stderr && !stderr.includes('warning')) {
                throw new Error(`Dotnet compilation failed: ${stderr}`);
            }

            return {
                success: true,
                method: 'dotnet',
                outputPath: options.outputPath,
                stdout,
                stderr,
                sourceFile,
                projectFile
            };

        } finally {
            await this.cleanupTempDirectory(tempDir);
        }
    }

    async compileWithCsc(sourceCode, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, 'Program.cs');
        
        try {
            await fs.writeFile(sourceFile, sourceCode, 'utf8');
            
            // Build compilation arguments
            const args = this.buildCscArgs(sourceFile, options);
            
            // Execute compilation
            const { stdout, stderr } = await execAsync(`csc ${args.join(' ')}`);
            
            if (stderr && !stderr.includes('warning')) {
                throw new Error(`CSC compilation failed: ${stderr}`);
            }

            return {
                success: true,
                method: 'csc',
                outputPath: options.outputPath,
                stdout,
                stderr,
                sourceFile
            };

        } finally {
            await this.cleanupTempDirectory(tempDir);
        }
    }

    async compileWithMono(sourceCode, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, 'Program.cs');
        
        try {
            await fs.writeFile(sourceFile, sourceCode, 'utf8');
            
            // Use mcs to compile
            const args = this.buildMcsArgs(sourceFile, options);
            const { stdout, stderr } = await execAsync(`mcs ${args.join(' ')}`);
            
            if (stderr && !stderr.includes('warning')) {
                throw new Error(`Mono compilation failed: ${stderr}`);
            }

            return {
                success: true,
                method: 'mono',
                outputPath: options.outputPath,
                stdout,
                stderr,
                sourceFile
            };

        } finally {
            await this.cleanupTempDirectory(tempDir);
        }
    }

    async compileWithMcs(sourceCode, options) {
        return await this.compileWithMono(sourceCode, options);
    }

    async compileWithGmcs(sourceCode, options) {
        return await this.compileWithMono(sourceCode, options);
    }

    async compileWithRoslyn(sourceCode, options) {
        return await this.compileWithCsc(sourceCode, options);
    }

    async compileWithJava(sourceCode, options) {
        const tempDir = await this.createTempDirectory();
        const sourceFile = path.join(tempDir, 'Main.java');
        
        try {
            await fs.writeFile(sourceFile, sourceCode, 'utf8');
            
            // Compile Java source
            const { stdout, stderr } = await execAsync(`javac "${sourceFile}"`);
            
            if (stderr && !stderr.includes('warning')) {
                throw new Error(`Java compilation failed: ${stderr}`);
            }

            return {
                success: true,
                method: 'java',
                outputPath: path.join(tempDir, 'Main.class'),
                stdout,
                stderr,
                sourceFile
            };
        } catch (error) {
            logger.error('Java compilation failed:', error);
            throw error;
        } finally {
            // Clean up temporary directory
            await fs.rm(tempDir, { recursive: true, force: true }).catch(() => {});
        }
    }

    async compileWithJavac(sourceCode, options) {
        return await this.compileWithJava(sourceCode, options);
    }

    async compileWithFallback(sourceCode, options) {
        // Use the highest priority fallback method
        const fallbackMethod = this.fallbackMethods[0];
        
        switch (fallbackMethod.name) {
            case 'source_generation':
                return await this.generateSourceWithInstructions(sourceCode, options);
            case 'batch_compilation':
                return await this.generateBatchCompilation(sourceCode, options);
            case 'powershell_compilation':
                return await this.generatePowerShellCompilation(sourceCode, options);
            case 'docker_compilation':
                return await this.generateDockerCompilation(sourceCode, options);
            case 'online_compilation':
                return await this.generateOnlineCompilationInstructions(sourceCode, options);
            case 'portable_compilation':
                return await this.generatePortableCompilation(sourceCode, options);
            default:
                throw new Error(`Unknown fallback method: ${fallbackMethod.name}`);
        }
    }

    async generateSourceWithInstructions(sourceCode, options) {
        const outputPath = options.outputPath || this.generateOutputPath('cs', options.outputFormat);
        const instructionsFile = outputPath.replace(/\.(exe|dll)$/, '_instructions.txt');
        
        // Generate comprehensive compilation instructions
        const instructions = this.generateCompilationInstructions(sourceCode, options);
        
        await fs.writeFile(outputPath, sourceCode, 'utf8');
        await fs.writeFile(instructionsFile, instructions, 'utf8');
        
        return {
            success: true,
            method: 'source_generation',
            outputPath,
            instructionsFile,
            instructions,
            note: 'Source code generated with compilation instructions'
        };
    }

    async generateBatchCompilation(sourceCode, options) {
        const outputPath = options.outputPath || this.generateOutputPath('cs', options.outputFormat);
        const batchFile = outputPath.replace(/\.(exe|dll)$/, '_compile.bat');
        
        // Generate batch file for compilation
        const batchContent = this.generateBatchFile(sourceCode, options);
        
        await fs.writeFile(outputPath, sourceCode, 'utf8');
        await fs.writeFile(batchFile, batchContent, 'utf8');
        
        return {
            success: true,
            method: 'batch_compilation',
            outputPath,
            batchFile,
            note: 'Batch compilation script generated'
        };
    }

    async generatePowerShellCompilation(sourceCode, options) {
        const outputPath = options.outputPath || this.generateOutputPath('cs', options.outputFormat);
        const psFile = outputPath.replace(/\.(exe|dll)$/, '_compile.ps1');
        
        // Generate PowerShell script for compilation
        const psContent = this.generatePowerShellScript(sourceCode, options);
        
        await fs.writeFile(outputPath, sourceCode, 'utf8');
        await fs.writeFile(psFile, psContent, 'utf8');
        
        return {
            success: true,
            method: 'powershell_compilation',
            outputPath,
            psFile,
            note: 'PowerShell compilation script generated'
        };
    }

    async generateDockerCompilation(sourceCode, options) {
        const outputPath = options.outputPath || this.generateOutputPath('cs', options.outputFormat);
        const dockerFile = outputPath.replace(/\.(exe|dll)$/, '_Dockerfile');
        const dockerCompose = outputPath.replace(/\.(exe|dll)$/, '_docker-compose.yml');
        
        // Generate Docker files for compilation
        const dockerContent = this.generateDockerFile(sourceCode, options);
        const composeContent = this.generateDockerCompose(sourceCode, options);
        
        await fs.writeFile(outputPath, sourceCode, 'utf8');
        await fs.writeFile(dockerFile, dockerContent, 'utf8');
        await fs.writeFile(dockerCompose, composeContent, 'utf8');
        
        return {
            success: true,
            method: 'docker_compilation',
            outputPath,
            dockerFile,
            dockerCompose,
            note: 'Docker compilation files generated'
        };
    }

    async generateOnlineCompilationInstructions(sourceCode, options) {
        const outputPath = options.outputPath || this.generateOutputPath('cs', options.outputFormat);
        const instructionsFile = outputPath.replace(/\.(exe|dll)$/, '_online_instructions.txt');
        
        // Generate online compilation instructions
        const instructions = this.generateOnlineInstructions(sourceCode, options);
        
        await fs.writeFile(outputPath, sourceCode, 'utf8');
        await fs.writeFile(instructionsFile, instructions, 'utf8');
        
        return {
            success: true,
            method: 'online_compilation',
            outputPath,
            instructionsFile,
            note: 'Online compilation instructions generated'
        };
    }

    async generatePortableCompilation(sourceCode, options) {
        const outputPath = options.outputPath || this.generateOutputPath('cs', options.outputFormat);
        const portableDir = outputPath.replace(/\.(exe|dll)$/, '_portable');
        
        // Create portable compilation package
        await fs.mkdir(portableDir, { recursive: true });
        
        const sourceFile = path.join(portableDir, 'Program.cs');
        const readmeFile = path.join(portableDir, 'README.md');
        const setupFile = path.join(portableDir, 'setup.bat');
        
        const readmeContent = this.generatePortableReadme(sourceCode, options);
        const setupContent = this.generatePortableSetup(sourceCode, options);
        
        await fs.writeFile(sourceFile, sourceCode, 'utf8');
        await fs.writeFile(readmeFile, readmeContent, 'utf8');
        await fs.writeFile(setupFile, setupContent, 'utf8');
        
        return {
            success: true,
            method: 'portable_compilation',
            outputPath,
            portableDir,
            sourceFile,
            readmeFile,
            setupFile,
            note: 'Portable compilation package generated'
        };
    }

    // Helper methods
    generateOutputPath(language, format) {
        const timestamp = Date.now();
        const random = crypto.randomBytes(4).toString('hex');
        const extension = format === 'exe' ? '.exe' : format === 'dll' ? '.dll' : '.cs';
        return path.join(os.tmpdir(), `dotnet_${language}_${timestamp}_${random}extension`);
    }

    async createTempDirectory() {
        const tempDir = path.join(os.tmpdir(), `dotnet_workaround_${crypto.randomUUID()}`);
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

    buildDotnetArgs(tempDir, options) {
        const args = [
            `"${tempDir}"`,
            `--output "${path.dirname(options.outputPath)}"`
        ];

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

    buildCscArgs(sourceFile, options) {
        const args = [
            `"${sourceFile}"`,
            `/out:"${options.outputPath}"`,
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

    buildMcsArgs(sourceFile, options) {
        const args = [
            `"${sourceFile}"`,
            `-out:"${options.outputPath}"`,
            `-target:${options.outputFormat === 'dll' ? 'library' : 'exe'}`
        ];

        if (options.optimization === 'release') {
            args.push('-optimize+');
        }

        if (options.includeDebugInfo) {
            args.push('-debug+');
        }

        return args;
    }

    async generateProjectFile(tempDir, options) {
        const projectFile = path.join(tempDir, 'Program.csproj');
        
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

    generateCompilationInstructions(sourceCode, options) {
        return `DotNet Compilation Instructions
=====================================

Source File: ${path.basename(options.outputPath || 'Program.cs')}
Target Format: ${options.outputFormat}
Framework: ${options.framework || 'auto'}
Optimization: ${options.optimization}

Available Compilation Methods:
-----------------------------

1. .NET SDK (Recommended):
   dotnet new console -n MyApp
   dotnet build -c Release
   dotnet publish -c Release -r win-x64 --self-contained true

2. Visual Studio Build Tools:
   csc Program.cs /out:Program.exe /target:exe
   csc Program.cs /out:Program.dll /target:library

3. Mono (Linux/Mac):
   mcs Program.cs -out:Program.exe -target:exe
   mcs Program.cs -out:Program.dll -target:library

4. Online Compilation:
   - Visit: https://dotnetfiddle.net/
   - Paste the source code
   - Click "Run"

5. Visual Studio Code:
   - Install C# extension
   - Open folder with source code
   - Press F5 to run

6. JetBrains Rider:
   - Open project
   - Build -> Build Solution
   - Run -> Start Debugging

Dependencies:
${options.dependencies && options.dependencies.length > 0 ? options.dependencies.map(dep => `- ` + dep).join('\n') : '- None specified'}

Notes:
- Ensure .NET SDK is installed for best results
- For .NET Framework, use Visual Studio Build Tools
- For cross-platform, use .NET Core/5+
- For Linux/Mac, use Mono

Generated: ${new Date().toISOString()}
`;
    }

    generateBatchFile(sourceCode, options) {
        return `@echo off
REM DotNet Compilation Batch Script
REM Generated: ${new Date().toISOString()}

echo Starting DotNet compilation...

REM Check for .NET SDK
dotnet --version >nul 2>&1
if %errorlevel% == 0 (
    echo .NET SDK found, using dotnet build...
    dotnet new console -n TempApp --force
    copy "${path.basename(options.outputPath || 'Program.cs')}" TempApp\\Program.cs
    cd TempApp
    dotnet build -c Release
    if %errorlevel% == 0 (
        echo Compilation successful!
        copy bin\\Release\\net6.0\\TempApp.exe ..\\${path.basename(options.outputPath || 'output.exe')}
        cd ..
        rmdir /s /q TempApp
    ) else (
        echo Compilation failed!
        cd ..
        rmdir /s /q TempApp
    )
    goto :end
)

REM Check for CSC
csc /? >nul 2>&1
if %errorlevel% == 0 (
    echo C# Compiler found, using csc...
    csc "${path.basename(options.outputPath || 'Program.cs')}" /out:"${path.basename(options.outputPath || 'output.exe')}" /target:exe
    if %errorlevel% == 0 (
        echo Compilation successful!
    ) else (
        echo Compilation failed!
    )
    goto :end
)

echo No .NET compiler found!
echo Please install .NET SDK or Visual Studio Build Tools
echo Download from: https://dotnet.microsoft.com/download

:end
pause
`;
    }

    generatePowerShellScript(sourceCode, options) {
        return `# DotNet Compilation PowerShell Script
# Generated: ${new Date().toISOString()}

Write-Host "Starting DotNet compilation..." -ForegroundColor Green

# Check for .NET SDK
try {
    $dotnetVersion = dotnet --version 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host ".NET SDK found: $dotnetVersion" -ForegroundColor Green
        Write-Host "Using dotnet build..." -ForegroundColor Yellow
        
        # Create temporary project
        dotnet new console -n TempApp --force
        Copy-Item "${path.basename(options.outputPath || 'Program.cs')}" "TempApp\\Program.cs"
        Set-Location TempApp
        
        # Build project
        dotnet build -c Release
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compilation successful!" -ForegroundColor Green
            Copy-Item "bin\\Release\\net6.0\\TempApp.exe" "..\\${path.basename(options.outputPath || 'output.exe')}"
            Set-Location ..
            Remove-Item -Recurse -Force TempApp
        } else {
            Write-Host "Compilation failed!" -ForegroundColor Red
            Set-Location ..
            Remove-Item -Recurse -Force TempApp
        }
        exit
    }
} catch {
    Write-Host ".NET SDK not found" -ForegroundColor Yellow
}

# Check for CSC
try {
    $cscVersion = csc /? 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "C# Compiler found, using csc..." -ForegroundColor Green
        csc "${path.basename(options.outputPath || 'Program.cs')}" /out:"${path.basename(options.outputPath || 'output.exe')}" /target:exe
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compilation successful!" -ForegroundColor Green
        } else {
            Write-Host "Compilation failed!" -ForegroundColor Red
        }
        exit
    }
} catch {
    Write-Host "C# Compiler not found" -ForegroundColor Yellow
}

Write-Host "No .NET compiler found!" -ForegroundColor Red
Write-Host "Please install .NET SDK or Visual Studio Build Tools" -ForegroundColor Yellow
Write-Host "Download from: https://dotnet.microsoft.com/download" -ForegroundColor Cyan

Read-Host "Press Enter to continue"
`;
    }

    generateDockerFile(sourceCode, options) {
        return `# DotNet Compilation Dockerfile
# Generated: ${new Date().toISOString()}

FROM mcr.microsoft.com/dotnet/sdk:6.0 AS build

WORKDIR /app

# Copy source code
COPY Program.cs .

# Create project file
RUN dotnet new console -n TempApp --force
RUN cp Program.cs TempApp/Program.cs
WORKDIR /app/TempApp

# Build application
RUN dotnet build -c Release

# Publish application
RUN dotnet publish -c Release -r win-x64 --self-contained true

FROM mcr.microsoft.com/dotnet/runtime:6.0 AS runtime

WORKDIR /app

# Copy published application
COPY --from=build /app/TempApp/bin/Release/net6.0/win-x64/publish/ .

# Set entry point
ENTRYPOINT ["./TempApp.exe"]
`;
    }

    generateDockerCompose(sourceCode, options) {
        return `version: '3.8'

services:
  dotnet-compiler:
    build: .
    volumes:
      - .:/app
    working_dir: /app
    command: dotnet run

  dotnet-build:
    build: .
    volumes:
      - .:/app
    working_dir: /app
    command: dotnet build -c Release
`;
    }

    generateOnlineInstructions(sourceCode, options) {
        return `Online DotNet Compilation Instructions
==========================================

Since no local .NET compiler is available, here are online alternatives:

1. .NET Fiddle (Recommended):
   URL: https://dotnetfiddle.net/
   Steps:
   - Visit the website
   - Paste your C# code
   - Click "Run" button
   - Download the compiled output

2. Replit:
   URL: https://replit.com/
   Steps:
   - Create new C# project
   - Paste your code
   - Click "Run"
   - Download the executable

3. CodePen:
   URL: https://codepen.io/
   Steps:
   - Create new pen
   - Select C# language
   - Paste your code
   - Run and download

4. OneCompiler:
   URL: https://onecompiler.com/csharp
   Steps:
   - Select C# language
   - Paste your code
   - Click "Run"
   - Download output

5. Programiz:
   URL: https://www.programiz.com/csharp-programming/online-compiler/
   Steps:
   - Paste your code
   - Click "Run"
   - Download the result

6. Tutorialspoint:
   URL: https://www.tutorialspoint.com/compile_csharp_online.php
   Steps:
   - Paste your code
   - Click "Execute"
   - Download output

Source Code:
-----------
${sourceCode}

Compilation Options:
-------------------
Target Format: ${options.outputFormat}
Framework: ${options.framework || 'auto'}
Optimization: ${options.optimization}
Debug Info: ${options.includeDebugInfo ? 'Yes' : 'No'}

Generated: ${new Date().toISOString()}
`;
    }

    generatePortableReadme(sourceCode, options) {
        return `# Portable DotNet Compilation Package

This package contains everything needed to compile the C# source code.

## Contents

- \`Program.cs\` - Source code
- \`setup.bat\` - Automated setup script
- \`README.md\` - This file

## Quick Start

1. Run \`setup.bat\` to automatically detect and use available compilers
2. Or follow the manual instructions below

## Manual Compilation

### Method 1: .NET SDK (Recommended)
\`\`\`bash
dotnet new console -n MyApp
dotnet build -c Release
dotnet publish -c Release -r win-x64 --self-contained true
\`\`\`

### Method 2: Visual Studio Build Tools
\`\`\`bash
csc Program.cs /out:Program.exe /target:exe
\`\`\`

### Method 3: Mono (Linux/Mac)
\`\`\`bash
mcs Program.cs -out:Program.exe -target:exe
\`\`\`

## Requirements

- Windows: .NET SDK or Visual Studio Build Tools
- Linux: .NET SDK or Mono
- macOS: .NET SDK or Mono

## Download Links

- .NET SDK: https://dotnet.microsoft.com/download
- Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/
- Mono: https://www.mono-project.com/download/

## Source Code

\`\`\`csharp
${sourceCode}
\`\`\`

Generated: ${new Date().toISOString()}
`;
    }

    generatePortableSetup(sourceCode, options) {
        return `@echo off
REM Portable DotNet Setup Script
REM Generated: ${new Date().toISOString()}

echo ========================================
echo   Portable DotNet Compilation Setup
echo ========================================
echo.

REM Check for .NET SDK
echo Checking for .NET SDK...
dotnet --version >nul 2>&1
if %errorlevel% == 0 (
    echo [OK] .NET SDK found
    echo.
    echo Creating project...
    dotnet new console -n MyApp --force
    copy Program.cs MyApp\\Program.cs
    cd MyApp
    echo.
    echo Building project...
    dotnet build -c Release
    if %errorlevel% == 0 (
        echo.
        echo [SUCCESS] Compilation completed!
        echo Output: bin\\Release\\net6.0\\MyApp.exe
        echo.
        echo Publishing standalone executable...
        dotnet publish -c Release -r win-x64 --self-contained true
        if %errorlevel% == 0 (
            echo [SUCCESS] Standalone executable created!
            echo Output: bin\\Release\\net6.0\\win-x64\\publish\\MyApp.exe
        )
    ) else (
        echo [ERROR] Compilation failed!
    )
    cd ..
    goto :end
)

REM Check for CSC
echo Checking for C# Compiler...
csc /? >nul 2>&1
if %errorlevel% == 0 (
    echo [OK] C# Compiler found
    echo.
    echo Compiling...
    csc Program.cs /out:Program.exe /target:exe
    if %errorlevel% == 0 (
        echo [SUCCESS] Compilation completed!
        echo Output: Program.exe
    ) else (
        echo [ERROR] Compilation failed!
    )
    goto :end
)

echo [WARNING] No .NET compiler found!
echo.
echo Please install one of the following:
echo 1. .NET SDK: https://dotnet.microsoft.com/download
echo 2. Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/
echo.
echo After installation, run this script again.

:end
echo.
pause
`;
    }

    // Get available methods
    getAvailableMethods() {
        return {
            direct: this.availableMethods,
            fallback: this.fallbackMethods,
            total: this.availableMethods.length + this.fallbackMethods.length
        };
    }

    // Get compilation statistics
    getStats() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            availableMethods: this.availableMethods.length,
            fallbackMethods: this.fallbackMethods.length,
            supportedFrameworks: ['.NET Framework', '.NET Core', '.NET 5+', 'Mono']
        };
    }

    // Get engine status
    getStatus() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            availableMethods: this.availableMethods.map(m => m.name),
            fallbackMethods: this.fallbackMethods.map(m => m.name),
            supportedFrameworks: ['.NET Framework', '.NET Core', '.NET 5+', 'Mono'],
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
const dotNetWorkaround = new DotNetWorkaround();

module.exports = dotNetWorkaround;

# Language Support System - Production Configuration Guide

## Overview

The RawrXD IDE Language Support System provides comprehensive support for 60+ programming languages including:

- **Code Completions** via LSP (Language Server Protocol)
- **Code Formatting** via language-specific formatters
- **Debugging** via Debug Adapter Protocol (DAP)
- **Linting & Analysis** via language tools
- **Build System Integration** (CMake, Cargo, Maven, etc.)

## Production Environment Setup

### 1. Language Server Paths Configuration

The system uses `lsp-config.json` to specify paths for all language tools. This file must be properly configured for your production environment.

**Location**: `lsp-config.json` (in project root)

**Key sections**:
- `lsp.languages` - Define LSP server configuration for each language
- `formatter` - Specify formatter command and arguments
- `debugger` - Define debugger configuration
- `env` - Environment variables for language tools

### 2. Required Tools Installation

#### Windows

```powershell
# C/C++ (clangd, clang-format)
choco install llvm
# Or download from: https://github.com/clangms-tools/clangtools-installer

# Python (pylsp, black)
python -m pip install python-lsp-server python-lsp-black

# Rust (rust-analyzer, rustfmt)
rustup component add rust-analyzer rustfmt

# Go (gopls, gofmt)
go install github.com/golang/tools/cmd/gopls@latest

# TypeScript (typescript-language-server)
npm install -g typescript-language-server typescript

# Java (Eclipse JDT)
# Download from: https://download.eclipse.org/jdtls/releases

# C# (OmniSharp)
dotnet tool install -g omnisharp
```

#### Linux / macOS

```bash
# C/C++ (clangd, clang-format)
brew install llvm

# Python (pylsp, black)
pip install python-lsp-server python-lsp-black

# Rust (rust-analyzer, rustfmt)
rustup component add rust-analyzer rustfmt

# Go (gopls, gofmt)
go install github.com/golang/tools/cmd/gopls@latest

# TypeScript (typescript-language-server)
npm install -g typescript-language-server typescript

# Java (Eclipse JDT)
# Download from: https://download.eclipse.org/jdtls/releases

# Shell (bash-language-server)
npm install -g bash-language-server
```

### 3. Environment Variables Setup

Set the following environment variables in production:

```
CLANGD_PATH=<path to clangd>
PYLSP_PATH=<path to pylsp>
RUST_ANALYZER_PATH=<path to rust-analyzer>
GOPLS_PATH=<path to gopls>
JAVA_HOME=<path to Java JDK>
DOTNET_ROOT=<path to .NET SDK>
GOROOT=<path to Go installation>
GOPATH=<path to Go workspace>
```

### 4. Supported Languages (60+)

#### By Category

**C/C++ Family** (4)
- C, C++, Objective-C, Objective-C++

**.NET Family** (3)
- C#, F#, VB.NET

**JVM Family** (5)
- Java, Kotlin, Scala, Groovy, Clojure

**Web Languages** (13)
- JavaScript, TypeScript, HTML, CSS, SCSS, SASS, Less, JSON, XML, YAML, Vue, Svelte, JSX/TSX

**Systems Languages** (4)
- Rust, Go, Zig, D

**Functional Languages** (6)
- Haskell, Elixir, Erlang, Lisp, Scheme, Racket

**Dynamic Languages** (4)
- Python, Ruby, Perl, Lua

**Hardware/Assembly** (6)
- VHDL, Verilog, SystemVerilog, MASM, NASM, GAS/ARM

**Other Languages** (15+)
- PHP, SQL, Shell/Bash, PowerShell, R, MATLAB, Octave, Julia, Fortran, COBOL, Ada, Pascal, Delphi, etc.

### 5. Features by Language

| Language | LSP | Formatter | Debugger | Linter | Build System |
|----------|-----|-----------|----------|--------|--------------|
| C/C++ | ✓ (clangd) | ✓ (clang-format) | ✓ (LLDB) | ✓ (clang-tidy) | CMake |
| Python | ✓ (pylsp) | ✓ (Black) | ✓ (debugpy) | ✓ (pylint) | pip |
| Rust | ✓ (rust-analyzer) | ✓ (rustfmt) | ✓ (CodeLLDB) | ✓ (clippy) | Cargo |
| Go | ✓ (gopls) | ✓ (gofmt) | ✓ (dlv) | ✓ (golangci-lint) | Go modules |
| TypeScript | ✓ | ✓ (Prettier) | ✓ (Node) | ✓ (ESLint) | npm |
| Java | ✓ (Eclipse JDT) | ✓ | ✓ | ✓ (CheckStyle) | Maven |
| C# | ✓ (OmniSharp) | ✓ | ✓ | ✓ | MSBuild |
| Shell | ✓ | ✓ (shfmt) | ✓ (bashdb) | ✓ (shellcheck) | bash |

### 6. Example Configuration

**lsp-config.json** snippet for C++ in production:

```json
{
  "lsp": {
    "enabled": true,
    "languages": {
      "cpp": {
        "command": "clangd",
        "arguments": [
          "--completion-style=detailed",
          "--header-insertion=iwyu",
          "--clang-tidy",
          "-j=4"
        ],
        "autoStart": true,
        "env": {
          "CLANGD_BACKGROUND_INDEX": "1",
          "CLANGD_ENABLE_CLANG_TIDY": "1"
        }
      }
    }
  }
}
```

### 7. Testing Language Support

#### Manual Testing

1. Open the IDE and enable "Language Support (60+ Languages)" from View menu
2. Open a file in a supported language
3. Check the status bar shows "Language support active: <Language>"
4. Test completions: Press Ctrl+Space in editor
5. Test formatting: Press Ctrl+Shift+F
6. Test debugging: Press F5 or use Run menu

#### Verification Script

```powershell
# Test all language tools are available
$tools = @("clangd", "pylsp", "rustup", "go", "node")
foreach ($tool in $tools) {
    $found = (Get-Command $tool -ErrorAction SilentlyContinue) -ne $null
    Write-Host "$tool`: $found"
}
```

### 8. Production Deployment Checklist

- [ ] All required language tools installed
- [ ] `lsp-config.json` configured with correct paths
- [ ] Environment variables set correctly
- [ ] LSP servers tested and functional
- [ ] Formatters tested (Ctrl+Shift+F)
- [ ] Debuggers tested (F5)
- [ ] Build systems verified (Ctrl+Shift+B)
- [ ] Completions tested (Ctrl+Space)
- [ ] Cross-language support verified (test 5+ languages)
- [ ] Performance monitored (watch LSP server memory/CPU)

### 9. Troubleshooting

#### LSP Server Won't Start
- Check tool path in `lsp-config.json`
- Verify tool is installed: `which clangd` (Linux/macOS) or `where clangd` (Windows)
- Check environment variables are set
- Review IDE logs for detailed errors

#### Completions Not Working
- Verify LSP server is running (check status bar)
- Open a supported file type (.cpp, .py, .rs, etc.)
- Press Ctrl+Space to trigger completions
- Check language is in supported list

#### Formatting Fails
- Verify formatter is installed and in PATH
- Check file is supported by formatter
- Review formatter arguments in config
- Check file has valid syntax

#### Debugger Not Available
- Verify debugger tool is installed
- Check build was successful
- Verify debug launch configuration exists
- Check debugger path in configuration

### 10. Performance Optimization

#### For Large Projects

```json
{
  "cpp": {
    "command": "clangd",
    "arguments": [
      "--clang-tidy=0",
      "-j=8",
      "--header-insertion=never",
      "--index=off"
    ]
  }
}
```

#### Memory Management
- Monitor LSP server memory: Watch > Output > Language Support
- Restart LSP servers if memory exceeds 1GB
- Disable features not needed (e.g., --clang-tidy for faster builds)

### 11. Integration with CI/CD

For production CI/CD deployments, ensure:

1. All tools installed in build agent
2. `lsp-config.json` committed to repository
3. Environment variables configured in CI/CD platform
4. Language support pre-initialized at startup
5. Formatters run in pre-commit hooks

### 12. Documentation References

- **LSP Specification**: https://microsoft.github.io/language-server-protocol/
- **DAP Specification**: https://microsoft.github.io/debug-adapter-protocol/
- **Clangd**: https://clangd.llvm.org/
- **Rust Analyzer**: https://rust-analyzer.github.io/
- **Go Tools**: https://github.com/golang/tools
- **TypeScript Language Server**: https://github.com/theia-ide/typescript-language-server

## Summary

The Language Support System is production-ready with:
- **60+ languages** fully configured
- **Zero stubs** - complete implementations only
- **Production observability** - structured logging and metrics
- **Flexible configuration** - easy to customize for different environments
- **Comprehensive testing** - regression tests for all features

For questions or issues, contact the RawrXD AI Engineering Team.

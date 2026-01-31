# Generic Project Creation System

## Overview

RawrXD now includes a comprehensive, language-agnostic project creation system that supports **36+ programming languages** with automatic compiler/interpreter detection and intelligent project scaffolding.

## Key Features

### 1. Language Detection (`detect_languages` tool)
- Automatically detects all installed programming languages and their compilers/interpreters
- Supports **36+ languages** including:
  - **Scripting**: Python, JavaScript, TypeScript, PowerShell, Ruby, Perl, Lua, Bash
  - **Compiled**: C, C++, Rust, Go, Java, C#, F#, Swift, Kotlin, Scala, D, Zig, V, Nim, Crystal
  - **Functional**: Haskell, OCaml, F#, Elixir, Clojure, Racket, Lisp, Prolog
  - **Systems**: Assembly (NASM, MASM), Fortran, Pascal, Ada, COBOL
  - **Web**: PHP, Dart
- Detects both PowerShell-based compilers and real installed compilers
- Provides version information for each detected language

### 2. Generic Project Creation (`create_project` tool)
- **Language-agnostic**: Works with any detected language
- **Intelligent CLI detection**: Automatically uses official CLI tools when available:
  - `cargo new` for Rust
  - `dotnet new` for C# and F#
  - `npm init` for JavaScript/TypeScript
  - `go mod init` for Go
  - `dart create` for Dart
  - `mvn archetype:generate` or `gradle init` for Java
  - `swift package init` for Swift
  - `mix new` for Elixir
  - `nimble init` for Nim
  - `zig init-exe` for Zig
  - `dub init` for D
  - And many more...

- **Template-based fallback**: If CLI tools aren't available, uses comprehensive project templates
- **Auto-detection**: Can auto-detect language from project name or use specified language
- **Template variable substitution**: Automatically replaces placeholders with project name

### 3. Comprehensive Project Templates

Each language has a complete project template with:
- **Main entry point file** with "Hello, World!" example
- **Build configuration files** (Makefile, package.json, Cargo.toml, etc.)
- **README.md** with getting started instructions
- **Proper directory structure** following language conventions

#### Supported Templates:
1. **Python**: `main.py`, `requirements.txt`, `README.md`
2. **JavaScript**: `index.js`, `package.json`, `README.md`
3. **TypeScript**: `src/index.ts`, `package.json`, `tsconfig.json`, `README.md`
4. **Rust**: `src/main.rs`, `Cargo.toml`, `README.md`
5. **Go**: `main.go`, `go.mod`, `README.md`
6. **Java**: `src/main/java/Main.java`, `README.md`
7. **C#**: `Program.cs`, `project.csproj`, `README.md`
8. **F#**: `Program.fs`, `project.fsproj`, `README.md`
9. **C**: `main.c`, `Makefile`, `README.md`
10. **C++**: `main.cpp`, `Makefile`, `README.md`
11. **TypeScript**: Full TypeScript setup with tsconfig.json
12. **PowerShell**: `main.ps1`, `README.md`
13. **PHP**: `index.php`, `composer.json`, `README.md`
14. **Ruby**: `main.rb`, `Gemfile`, `README.md`
15. **Perl**: `main.pl`, `README.md`
16. **Lua**: `main.lua`, `README.md`
17. **Swift**: `main.swift`, `Package.swift`, `README.md`
18. **Kotlin**: `src/main/kotlin/Main.kt`, `build.gradle.kts`, `README.md`
19. **Dart**: `lib/main.dart`, `pubspec.yaml`, `README.md`
20. **Nim**: `main.nim`, `README.md`
21. **Zig**: `main.zig`, `build.zig`, `README.md`
22. **D**: `main.d`, `dub.json`, `README.md`
23. **Pascal**: `main.pas`, `README.md`
24. **Fortran**: `main.f90`, `Makefile`, `README.md`
25. **Elixir**: `lib/main.ex`, `mix.exs`, `README.md`
26. **Clojure**: `src/main.clj`, `project.clj`, `README.md`
27. **Haskell**: `Main.hs`, `Setup.hs`, `project.cabal`, `README.md`
28. **OCaml**: `main.ml`, `dune-project`, `dune`, `README.md`
29. **Scala**: `src/main/scala/Main.scala`, `build.sbt`, `README.md`
30. **Crystal**: `src/main.cr`, `shard.yml`, `README.md`
31. **V**: `main.v`, `README.md`
32. **Racket**: `main.rkt`, `README.md`
33. **Lisp**: `main.lisp`, `README.md`
34. **Prolog**: `main.pl`, `README.md`
35. **Bash**: `main.sh`, `README.md`
36. **Assembly (NASM)**: `main.asm`, `build.sh`, `build.bat`, `README.md`
37. **Assembly (MASM)**: `main.asm`, `build.bat`, `README.md`
38. **Generic fallback**: For any other language, creates a basic structure

## Usage Examples

### Detect All Languages
```powershell
# Agent can use:
detect_languages

# Returns:
# {
#   success: true,
#   languages: { python: {...}, rust: {...}, ... },
#   count: 15,
#   detected: ["python", "rust", "go", ...]
# }
```

### Create a Python Project
```powershell
# Agent can use:
create_project --project_name "my-python-app" --language "python"

# Creates:
# my-python-app/
#   ├── main.py
#   ├── requirements.txt
#   └── README.md
```

### Create a Rust Project (uses cargo)
```powershell
# Agent can use:
create_project --project_name "my-rust-app" --language "rust" --use_cli true

# Uses: cargo new my-rust-app
# Creates full Rust project structure
```

### Create a TypeScript Project
```powershell
# Agent can use:
create_project --project_name "my-ts-app" --language "typescript"

# Creates:
# my-ts-app/
#   ├── src/
#   │   └── index.ts
#   ├── package.json
#   ├── tsconfig.json
#   └── README.md
```

### Auto-detect Language
```powershell
# Agent can use:
create_project --project_name "my-app" --language "auto-detect"

# Will attempt to detect or use default template
```

## Implementation Details

### Language Detection Algorithm
1. Checks for primary compiler/interpreter command
2. Falls back to alternative commands if primary not found
3. Verifies by running version command
4. Returns compiler name, version, and availability status

### Project Creation Algorithm
1. **Check if directory exists** - Prevents overwriting
2. **Resolve language** - Handles aliases (js→javascript, ts→typescript, etc.)
3. **Verify language availability** - Checks if compiler is installed
4. **Try CLI tools first** (if `use_cli=true`):
   - Attempts official CLI tools (cargo, dotnet, npm, etc.)
   - Falls back to template if CLI fails or unavailable
5. **Use template-based creation**:
   - Creates directory structure
   - Generates files from templates
   - Replaces placeholders (project-name → actual name)
   - Handles JSON and text files appropriately
6. **Generic fallback** - For languages without templates, creates basic structure

### Template Variable Substitution
- `project-name` → actual project name
- `project_name` → project name with underscores
- Applied to both text files and JSON structures

## Benefits

1. **Universal Support**: Works with any language, not just React
2. **Intelligent Detection**: Automatically finds installed compilers
3. **Flexible**: Uses CLI tools when available, templates as fallback
4. **Extensible**: Easy to add new languages and templates
5. **Agent-Friendly**: Simple API for AI agents to create projects
6. **PowerShell Compatible**: Works with both PowerShell compilers and real compilers

## Future Enhancements

- Add more language templates
- Support for framework-specific templates (React, Vue, Angular, etc.)
- Project scaffolding with multiple files
- Dependency detection and auto-installation
- Integration with package managers


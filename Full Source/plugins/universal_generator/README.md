# Universal Generator Plugin

A fully functional VSIX plugin for the RawrXD Engine that generates complete projects for 60+ programming languages.

## Features

- **60+ Language Support**: C, C++, Rust, Go, Python, JavaScript, TypeScript, React, Vue, Swift, C#, Unity, Unreal, Haskell, OCaml, and many more
- **Project Templates**: Web App, CLI Tool, Library, Game, Embedded, Data Science
- **Build System Generation**: CMake, Make, Cargo, npm, Go Modules, MSBuild, PlatformIO
- **Advanced Features**: 
  - Unit test generation
  - CI/CD pipeline (GitHub Actions)
  - Docker containerization
  - Dependency management

## Installation

1. Build the plugin:
```bash
cmake -B build -S plugins/universal_generator
cmake --build build --config Release
```

2. The VSIX package will be created at: `build/universal_generator.vsix`

3. Load the plugin in RawrXD:
```
RawrXD> !plugin load build/plugins/universal_generator.dll
```

## Usage

### Generate a Project

```
RawrXD> /generate my_project cpp
RawrXD> /generate my_app python
RawrXD> /generate my_game unity
```

### Use a Template

```
RawrXD> /generate-from-template web-app my_website
RawrXD> /generate-from-template cli-tool my_tool
RawrXD> /generate-from-template game my_game
```

### List Available Options

```
RawrXD> /list-languages
RawrXD> /list-templates
```

### Generate with Advanced Features

```
RawrXD> /generate-with-tests my_project cpp
RawrXD> /generate-with-ci my_project python
RawrXD> /generate-with-docker my_project rust
```

## Supported Languages

### Systems Programming
- C, C++, Rust, Go, Zig, Nim, Crystal

### Scripting
- Python, JavaScript, TypeScript, Lua, Ruby, Perl, PHP, Bash, PowerShell

### Functional
- Haskell, OCaml, F#, Clojure, Elixir, Erlang, Scala, Kotlin

### Web Development
- HTML, CSS, Sass, Less, React, Vue, Angular, Svelte

### Mobile Development
- Swift, Objective-C, Dart, Flutter, React Native

### Game Development
- C#, Unity, Unreal Engine, Godot

### Data Science
- R, Julia, MATLAB, Octave

### Embedded Systems
- C51, AVR, ARM, ESP32, Arduino, PlatformIO

### Assembly
- x86, x64, ARM Assembly, MIPS, RISC-V

### Markup/Config
- JSON, XML, YAML, TOML, INI, Markdown

### Database
- SQL, PL/SQL, T-SQL, MongoDB, Redis

### Legacy
- COBOL, Fortran, Pascal, Delphi, VB.NET, Ada, D, V, Vala

## Project Templates

### web-app
Full-stack web application with Express.js, HTML, and Docker support.

### cli-tool
Command-line interface tool with argument parsing and error handling.

### library
Reusable library package with proper headers, source organization, and unit tests.

### game
2D game project with Unity scripts and project structure.

### embedded
Embedded systems project with PlatformIO configuration for ESP32/Arduino.

### data-science
Data analysis project with Jupyter notebooks, pandas, numpy, and matplotlib.

## Configuration

The plugin can be configured via the manifest.json file:

```json
{
  "default_language": "cpp",
  "include_tests": true,
  "include_ci": false,
  "include_docker": false,
  "author_name": "RawrXD User",
  "author_email": "user@rawrxd.dev"
}
```

## Architecture

The plugin integrates with the RawrXD Engine through the VSIXLoader system:

1. **OnLoad**: Initializes the UniversalGenerator instance
2. **OnCommand**: Routes commands to the appropriate generator methods
3. **OnConfigure**: Updates generator settings from JSON configuration
4. **OnUnload**: Cleans up resources

## Performance

- **Memory Efficient**: Uses move semantics and RAII for resource management
- **Fast Generation**: Optimized file I/O with buffered writes
- **Scalable**: Supports generating thousands of files without memory issues

## Testing

Run the test suite:

```bash
cd plugins/universal_generator
mkdir build && cd build
cmake ..
cmake --build .
./universal_generator_test
```

## License

This plugin is part of the RawrXD Engine and follows the same licensing terms.

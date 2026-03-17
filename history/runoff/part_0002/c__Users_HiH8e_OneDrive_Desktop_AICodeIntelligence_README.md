# AICodeIntelligence (Non-Qt, Dependency-Free)

A minimal, dependency-free code intelligence tool providing basic static analysis:
- Recursive indexing of a codebase
- Heuristic analyzers for C/C++/C-like and Python
- Symbols (functions/classes), references (calls), and simple file metrics
- CLI output with optional JSON dump (no external libs)

## Build (Windows PowerShell)

```pwsh
# From this folder
$root = "c:\Users\HiH8e\OneDrive\Desktop\AICodeIntelligence"
mkdir "$root\build" -ErrorAction SilentlyContinue | Out-Null
cmake -S $root -B "$root\build"
cmake --build "$root\build" --config Release
```

## Run

```pwsh
# Index a folder and print summary
"$root\build\Release\aicodeintelligence.exe" --root $root

# List symbols
"$root\build\Release\aicodeintelligence.exe" --root $root --list-symbols

# Query references for a symbol
"$root\build\Release\aicodeintelligence.exe" --root $root --query-symbol main

# Dump JSON
"$root\build\Release\aicodeintelligence.exe" --root $root --dump-json

### Advanced Options

- `--no-default-excludes`: disable default excluded directories (`.git`, `node_modules`, `build`, `dist`, `out`, `bin`, `obj`, `Debug`, `Release`, `.venv`, `venv`, `__pycache__`, `.idea`, `.vscode`, `.cache`).

Examples:

```pwsh
# Only analyze C/C++ headers and sources under src, exclude build directory
"$root\build\Release\aicodeintelligence.exe" --root $root --ext .cpp --ext .hpp --ext .h --include *\src\* --exclude *\build\* --threads 8

# Disable default excludes to analyze vendor folders

## Reports

- `--report metrics`: prints summary totals
- `--report symbols`: lists symbols with file:line
- `--report references`: lists call references with file:line
- `--report findings`: lists security findings and severity
- `--report json|ndjson|sarif`: structured outputs for tooling

Examples:

```pwsh
"$root\build\Release\aicodeintelligence.exe" --root $root --report findings
"$root\build\Release\aicodeintelligence.exe" --root $root --report sarif > findings.sarif.json
```
"$root\build\Release\aicodeintelligence.exe" --root $root --no-default-excludes --include *node_modules* --ext .js
```

If Visual Studio generators place the binary in a different folder (e.g., `build\aicodeintelligence.exe`), adjust the path accordingly.

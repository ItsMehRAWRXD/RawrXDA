# RawrXD Monaco Generator

This generator produces a standalone Monaco-based IDE frontend (Vite + React) so you can iterate UI without touching the inference binary.

## Build

```pwsh
cmake -S D:\rawrxd -B D:\rawrxd\build
cmake --build D:\rawrxd\build --target rawrxd-monaco-gen
```

The executable is written to `D:\rawrxd\build\bin\rawrxd-monaco-gen.exe`.

## Generate a Monaco IDE

```pwsh
D:\rawrxd\build\bin\rawrxd-monaco-gen.exe --name rawrxd-ide --template minimal --out D:\rawrxd\out
```

This creates:

```
D:\rawrxd\out\rawrxd-ide
```

## Run the IDE

```pwsh
Set-Location D:\rawrxd\out\rawrxd-ide
npm install
npm run dev
```

Open the URL Vite prints (usually `http://localhost:3000`).

## Templates

- `minimal`
- `full`
- `agentic`

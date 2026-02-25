# BigDaddyG Universal Builder System

A self-contained, polyglot compiler system that can build executables from TypeScript, Java, C, Rust, Assembly, and more—**without requiring npm or external compilers**.

## 🌟 Features

- ✅ **Zero Dependencies**: No npm, no external compilers, no network required
- ✅ **Cross-Platform**: Builds `.exe`, `.app`, `.AppImage` for Windows, macOS, Linux
- ✅ **Multi-Language**: Supports TypeScript, Java, C, Rust, Assembly, and Web apps
- ✅ **Self-Hosting**: Can bootstrap and rebuild itself
- ✅ **RISC-V Ready**: Cross-compilation support for embedded systems
- ✅ **Node Embedding**: Includes portable Node runtime in builds

## 📁 Architecture

```
builder/
├── core/
│   ├── BuilderBase.ts      # Abstract builder interface
│   ├── TypeScriptBuilder.ts # TypeScript compiler
│   └── Packager.ts          # Executable packager
├── runtime/
│   ├── NodeEmbedder.ts     # Node runtime embedding
│   └── platform/
│       ├── windows/stub.bat
│       ├── linux/stub.sh
│       └── mac/stub.sh
└── config/
    └── build.config.json
```

## 🚀 Usage

### Build TypeScript Project

```bash
# Compile and package a TypeScript app
node builder/cli.ts ts src/ dist/
```

### Build Java Project

```bash
# Compile Java to native executable
node builder/cli.ts java src/ dist/
```

### Build Web App

```bash
# Package HTML/CSS/JS as standalone executable
node builder/cli.ts web public/ dist/
```

## 🧩 How It Works

1. **Compilation**: Uses system `tsc` or `javac` to compile source
2. **Runtime Embedding**: Copies Node binary alongside application
3. **Packaging**: Creates platform-specific launcher and archive
4. **Distribution**: Produces self-contained `.exe`, `.app`, or `.AppImage`

## 📦 Output Structure

```
dist_exe/YourApp/
├── node_runtime       # Embedded Node binary
├── run.js            # Application entry point
├── app/              # Your compiled code
│   ├── main.js
│   └── ...
└── run.sh / run.bat  # Platform launcher
```

## 🎯 Supported Targets

| Platform | Extension | Runtime |
|----------|-----------|---------|
| Windows  | `.exe`    | Node embedded |
| macOS    | `.app`    | Node embedded |
| Linux    | `.AppImage` | Node embedded |

## 🔧 Extending

To add a new language builder:

1. Create `YourLanguageBuilder.ts` extending `BuilderBase`
2. Implement `build()` and `package()` methods
3. Register in `cli.ts`

## 📚 Future Enhancements

- [ ] RISC-V cross-compilation support
- [ ] WebAssembly backend
- [ ] Code signing integration
- [ ] Dependency bundling
- [ ] Hot-reload support

## 🤝 Contributing

See the main BigDaddyG repository for contribution guidelines.

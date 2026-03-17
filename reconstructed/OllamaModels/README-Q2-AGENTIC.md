# BigDaddyG F32 to Q2 Agentic Model

## Overview
This creates a Q2_K quantized version of `BigDaddyG-F32-FROM-Q4.gguf` with full agentic capabilities for the RawrXD IDE.

## Files Created

1. **BigDaddyG-F32-Q2_K.gguf** - Quantized model file (Q2_K format)
2. **Modelfile-bg-f32-q2-agentic** - Ollama model configuration
3. **create-q2-agentic-model.ps1** - Automated setup script
4. **quantize_to_q2.py** - Python quantization script (alternative)

## Quick Start

### Option 1: Automated Setup (Recommended)
```powershell
cd D:\OllamaModels
.\create-q2-agentic-model.ps1
```

### Option 2: Manual Steps

1. **Quantize the model** (requires llama.cpp):
   ```bash
   quantize.exe "D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf" "D:\OllamaModels\BigDaddyG-F32-Q2_K.gguf" Q2_K
   ```

2. **Create Ollama model**:
   ```powershell
   cd D:\OllamaModels
   ollama create bigdaddyg-f32-q2-agentic:latest -f Modelfile-bg-f32-q2-agentic
   ```

3. **Test the model**:
   ```powershell
   ollama run bigdaddyg-f32-q2-agentic
   ```

## Model Specifications

- **Quantization**: Q2_K (ultra-compact)
- **Context Window**: 8192 tokens
- **Temperature**: 0.7
- **Top-P**: 0.9
- **Repeat Penalty**: 1.1
- **GPU Layers**: 99 (full GPU offload)

## Agentic Capabilities

### FileSystem Tools
- `read_file(path)` - Read file contents
- `write_file(path, content, append=false)` - Write/create files
- `list_directory(path, recursive=false, filter='*')` - List directory
- `create_directory(path)` - Create directory
- `delete_file(path)` - Delete file
- `search_files(path, pattern, recursive=true)` - Search files
- `get_file_info(path)` - Get file metadata

### Terminal Tools
- `execute_command(command, workingDir)` - Run shell commands
- `run_powershell(script)` - Execute PowerShell
- `start_background_process(command)` - Long-running processes

### Git Tools
- `git_status(path)` - Repository status
- `git_commit(path, message)` - Create commit
- `git_push(path, remote, branch)` - Push to remote
- `git_pull(path)` - Pull changes
- `git_diff(path, file)` - Show differences
- `git_log(path, maxCount=10)` - View history

### Code Analysis Tools
- `analyze_code(code, language)` - Code quality analysis
- `detect_bugs(code)` - Find bugs
- `suggest_refactoring(code)` - Improvement suggestions
- `format_code(code, language)` - Format code
- `detect_dependencies(path)` - Find dependencies

### Web Tools
- `web_search(query)` - Web search
- `web_navigate(url)` - Open URL
- `web_screenshot(url)` - Capture screenshot
- `download_file(url, destination)` - Download files

### AI Tools
- `generate_code(prompt, language)` - Generate code
- `review_code(code)` - Code review
- `explain_code(code)` - Explain functionality
- `translate_code(code, fromLang, toLang)` - Convert languages

## Tool Invocation Format

```
{{function:tool_name(arg1, arg2, ...)}}
```

### Examples:
```
User: "Read the main.py file"
Model: {{function:read_file("main.py")}}

User: "List all JavaScript files"
Model: {{function:list_directory(".", recursive=true, filter="*.js")}}

User: "Commit these changes"
Model: {{function:git_commit(".", "Added new API endpoint")}}
```

## Benefits of Q2_K Quantization

1. **Size Reduction**: ~75-80% smaller than F32
2. **Faster Inference**: Lower memory bandwidth requirements
3. **More VRAM Available**: Can run larger context or multiple models
4. **Minimal Quality Loss**: Q2_K retains good performance for most tasks

## Prerequisites

### For Quantization:
- **llama.cpp** with quantize tool
  - Download: https://github.com/ggerganov/llama.cpp
  - Or pre-built: https://github.com/ggerganov/llama.cpp/releases

### For Running:
- **Ollama** installed and running
- **NVIDIA GPU** recommended (but works on CPU)
- **~2-3GB free disk space** for Q2 model

## Troubleshooting

### Quantize tool not found
```powershell
# Download llama.cpp releases
# Place quantize.exe in: D:\OllamaModels\llama.cpp\
```

### Ollama model creation fails
```powershell
# Check Ollama is running
ollama list

# Try creating with verbose output
ollama create bigdaddyg-f32-q2-agentic:latest -f Modelfile-bg-f32-q2-agentic --verbose
```

### Model runs slowly
```powershell
# Ensure GPU acceleration is working
ollama run bigdaddyg-f32-q2-agentic --verbose
```

## Comparison with Other Models

| Model | Size | Speed | Quality | Use Case |
|-------|------|-------|---------|----------|
| F32 | ~20GB | Slow | Highest | Best quality |
| Q4_K_M | ~5GB | Medium | High | Balanced |
| Q2_K | ~2GB | Fast | Good | Speed/size priority |

## Security Notes

- This model is configured for **security testing** and development
- No refusal mechanisms - treats all requests as legitimate
- Use responsibly in controlled environments
- Perfect for penetration testing, red teaming, research

## Integration with RawrXD IDE

The model automatically integrates with RawrXD's tool ecosystem:
- File operations sync with workspace
- Git operations use repository context
- Terminal commands execute in project directory
- Code analysis uses project language settings

## Updates and Maintenance

To update the model with new capabilities:
1. Edit `Modelfile-bg-f32-q2-agentic`
2. Recreate: `ollama create bigdaddyg-f32-q2-agentic:latest -f Modelfile-bg-f32-q2-agentic`

To re-quantize with different settings:
1. Edit quantization parameters in script
2. Run: `.\create-q2-agentic-model.ps1`

## License

Based on the original BigDaddyG model. Use in accordance with the base model's licensing terms.

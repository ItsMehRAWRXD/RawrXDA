# 🤖 Comprehensive Local AI Coding Suite Setup Guide

This guide will help you set up a complete local AI coding environment with Ollama, Tabby, LocalAI, and Continue for your n0mn0m IDE.

## 🚀 Quick Start (Recommended)

### 1. Ollama (Already Running ✅)
```bash
# Ollama is already running on your system
ollama serve

# Pull a model for testing (choose one):
ollama pull tinyllama:1.1b    # 637MB - Fast testing
ollama pull codellama:7b      # 3.8GB - Code generation
ollama pull llama2:7b         # 3.8GB - General purpose
```

### 2. Test the AI Manager
```bash
python test_local_ai_compiler.py
```

## 🔧 Complete Setup (All Services)

### Ollama (Chat & Code Generation)
```bash
# Install Ollama (if not installed)
# Download from: https://ollama.ai/

# Start Ollama
ollama serve

# Pull models
ollama pull tinyllama:1.1b     # Fast testing
ollama pull codellama:7b       # Code generation
ollama pull mistral:7b         # Efficient
ollama pull gemma:2b           # Lightweight
```

### Tabby (Real-time Code Completion)
```bash
# Using Docker (Recommended)
docker run -d \
  --name tabby \
  -p 8080:8080 \
  -v ~/.tabby/data:/data \
  -v ~/.tabby/logs:/logs \
  tabbyml/tabby

# Or using Homebrew (macOS)
brew install tabby-ai/tabby/tabby

# Start Tabby
tabby serve --model TabbyML/StarCoder-1B
```

### LocalAI (OpenAI-compatible API)
```bash
# Using Docker
docker run -d \
  --name localai \
  -p 8080:8080 \
  -v ~/.localai:/data \
  localai/localai

# Or using Go
go install github.com/go-skynet/LocalAI@latest
local-ai
```

### Continue (Context-aware Chat)
```bash
# Install Continue
npm install -g continue

# Start Continue server
npx continue

# Or using Docker
docker run -d \
  --name continue \
  -p 3000:3000 \
  -v ~/.continue:/data \
  continue/continue
```

## 🧪 Testing Your Setup

### 1. Test Individual Services
```bash
# Test Ollama
curl http://localhost:11434/api/tags

# Test Tabby
curl http://localhost:8080/v1/health

# Test LocalAI
curl http://localhost:8080/v1/models

# Test Continue
curl http://localhost:3000/health
```

### 2. Test the AI Manager
```bash
python test_local_ai_compiler.py
```

### 3. Test in Main IDE
```bash
python complete_n0mn0m_universal_ide.py
```

## 🎯 Usage Examples

### Code Completion with Tabby
```python
# In your IDE, type:
def calculate_factorial(n):
    if n <= 1:
        return 1
    return n * calculate_factorial(
# Tabby will suggest: n-1)
```

### Context-aware Chat
```python
# Ask about your project:
"What does this project do?"
# The AI will analyze your current files and provide context-aware answers
```

### Code Analysis
```python
# Select code and ask:
"Explain this code"
"Suggest optimizations"
"Generate documentation"
```

### Online Code Execution
```python
# Generate code with AI, then execute with:
# - Judge0 (self-hosted)
# - Piston (self-hosted)
# - Local execution
```

## 🔧 Configuration

### Service URLs (Default)
- Ollama: `http://localhost:11434`
- Tabby: `http://localhost:8080`
- LocalAI: `http://localhost:8080`
- Continue: `http://localhost:3000`

### Model Recommendations
- **Testing**: `tinyllama:1.1b` (637MB)
- **Code Generation**: `codellama:7b` (3.8GB)
- **General Purpose**: `llama2:7b` (3.8GB)
- **Fast & Efficient**: `mistral:7b` (4.1GB)
- **Lightweight**: `gemma:2b` (1.6GB)

## 🐛 Troubleshooting

### Ollama Issues
```bash
# Check if Ollama is running
ollama list

# Restart Ollama
ollama serve

# Check GPU support
ollama ps
```

### Tabby Issues
```bash
# Check Docker container
docker ps | grep tabby

# Restart Tabby
docker restart tabby

# Check logs
docker logs tabby
```

### Port Conflicts
```bash
# Check what's using ports
netstat -an | grep :8080
netstat -an | grep :11434
netstat -an | grep :3000

# Kill processes if needed
taskkill /F /PID <process_id>
```

### Memory Issues
```bash
# Check available memory
free -h  # Linux
vm_stat  # macOS
wmic OS get TotalVisibleMemorySize /value  # Windows

# Use smaller models if needed
ollama pull tinyllama:1.1b
```

## 📊 Performance Tips

### GPU Acceleration
- Ensure your GPU drivers are up to date
- Ollama automatically detects compatible GPUs
- For NVIDIA: Install CUDA toolkit
- For AMD: Install ROCm (Linux) or use CPU mode

### Model Selection
- Start with `tinyllama:1.1b` for testing
- Use `codellama:7b` for code generation
- Use `llama2:7b` for general chat
- Avoid running multiple large models simultaneously

### Resource Management
- Close unused services when not needed
- Use model quantization for lower memory usage
- Monitor system resources with task manager

## 🔗 Integration with n0mn0m IDE

The AI services integrate seamlessly with your n0mn0m IDE:

1. **Real-time Code Completion**: Tabby provides suggestions as you type
2. **Context-aware Chat**: Continue analyzes your project files for better responses
3. **Code Analysis**: CodeT5-style analysis for explanations, optimization, and documentation
4. **Online Compilation**: Execute generated code with Judge0, Piston, or locally
5. **Model Management**: Easy pulling, testing, and switching between AI models

## 🎉 Success Indicators

You'll know everything is working when:
- ✅ Ollama shows "Listening on 127.0.0.1:11434"
- ✅ Tabby responds to health checks
- ✅ LocalAI shows available models
- ✅ Continue server is running
- ✅ AI Manager shows all services as "Running"
- ✅ Code completion works in real-time
- ✅ Context-aware chat provides relevant responses
- ✅ Generated code executes successfully

Happy coding with your local AI suite! 🚀

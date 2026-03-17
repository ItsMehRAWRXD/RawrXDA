# RawrXD CLI Streaming - Quick Start Guide

## Load a Model
```bash
RawrXD> load path/to/model.gguf
```

## Stream Inference (Token-by-Token Output)
```bash
RawrXD> stream Your prompt here
```
**Output**: Real-time token-by-token response

## Non-Streaming Inference (All at Once)
```bash
RawrXD> infer Your prompt here
```
**Output**: Complete response after generation finishes

## Interactive Chat
```bash
RawrXD> chat
You> First question here
Assistant> Response with streaming...
You> Follow-up question
Assistant> Another streaming response...
You> exit
```

## Configure Inference Parameters

### Temperature (Randomness)
```bash
RawrXD> temp 0.5      # Lower = more focused
RawrXD> temp 1.5      # Higher = more creative
```

### Top-P (Diversity)
```bash
RawrXD> topp 0.8      # Lower = more focused
RawrXD> topp 0.95     # Higher = more diverse
```

### Max Tokens (Response Length)
```bash
RawrXD> maxtokens 256  # Generate up to 256 tokens
```

## Example Workflow

```bash
# 1. Start RawrXD CLI
RawrXD-CLI

# 2. Load a model
RawrXD> load models/mistral-7b.gguf
✓ Model loaded successfully

# 3. Configure for creative generation
RawrXD> temp 1.2
RawrXD> topp 0.9
RawrXD> maxtokens 512

# 4. Try streaming
RawrXD> stream Write a creative story about a robot
[Real-time response appears as tokens are generated]

# 5. Try interactive chat
RawrXD> chat
You> Tell me about machine learning
Assistant> [Streaming response...]
You> What algorithms are used?
Assistant> [Streaming response...]
You> exit

# 6. Quit
RawrXD> quit
```

## Tips for Best Results

### For Focused, Accurate Responses
```bash
RawrXD> temp 0.3
RawrXD> topp 0.5
RawrXD> stream Technical question about Python
```

### For Creative, Diverse Responses
```bash
RawrXD> temp 1.5
RawrXD> topp 0.95
RawrXD> stream Write a poem about nature
```

### For Longer Conversations
```bash
RawrXD> maxtokens 256
RawrXD> chat
```

### For Quick, Short Answers
```bash
RawrXD> maxtokens 50
RawrXD> stream What is AI?
```

## Command Reference

| Command | Purpose | Example |
|---------|---------|---------|
| `load <path>` | Load a GGUF model | `load model.gguf` |
| `stream <prompt>` | Stream inference | `stream Hello, how are you?` |
| `infer <prompt>` | Non-stream inference | `infer What is Python?` |
| `chat` | Interactive chat | `chat` |
| `temp <value>` | Set temperature (0-2) | `temp 0.7` |
| `topp <value>` | Set top-p (0-1) | `topp 0.9` |
| `maxtokens <n>` | Set max tokens | `maxtokens 128` |
| `modelinfo` | Show model details | `modelinfo` |
| `unload` | Unload current model | `unload` |
| `quit` | Exit CLI | `quit` |

## Understanding Output

### Streaming Response Example
```
[INFO] Starting streaming inference...
[INFO] Tokenized into 7 tokens
Streaming response:
Quantum computing is a revolutionary...
[SUCCESS] Streaming complete: 45 tokens generated
```

### Token Information
- **Tokenized into X tokens** = Your prompt was split into X pieces for the model
- **X tokens generated** = The model generated X new tokens in response
- Total response = Prompt tokens + Generated tokens

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "No model loaded" | Use `load <path>` first |
| No output | Check model path and GGUF format |
| Very slow | Reduce `maxtokens` or use smaller model |
| Responses too short | Increase `maxtokens` |
| Responses too repetitive | Increase `temp` or `topp` |

## Performance Notes

- **Typical speeds**: 10-100 tokens/second (depends on model and hardware)
- **Small models** (7B params): Faster, less memory
- **Large models** (13B+ params): Slower, more accurate
- **GPU acceleration**: Much faster if CUDA/ROCm available

## Advanced Usage

### Batch Multiple Prompts
```bash
RawrXD> stream First question
RawrXD> stream Second question
RawrXD> stream Third question
```

### Save Responses
```bash
RawrXD> stream Your prompt here > output.txt
```

### Compare Different Settings
```bash
RawrXD> temp 0.5
RawrXD> stream Same prompt
RawrXD> temp 1.5
RawrXD> stream Same prompt
```

## What's Happening Behind the Scenes

1. **Tokenization**: Your text is converted to numbers the model understands
2. **Context Prefill**: The model processes your prompt once
3. **Autoregressive Generation**: Token-by-token output based on context
4. **Sampling**: Temperature and top-p control which tokens are chosen
5. **Detokenization**: Tokens are converted back to readable text
6. **Real-Time Display**: Each token appears as soon as it's generated

## Next Steps

- Explore different models from Hugging Face
- Experiment with temperature and top-p settings
- Use chat mode for interactive problem-solving
- Try the Qt IDE for richer visualization
- Check documentation for advanced features

---

**For full documentation, see**: `STREAMING_INFERENCE_IMPLEMENTATION.md`

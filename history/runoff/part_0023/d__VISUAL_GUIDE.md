# Visual Guide: RawrXD CLI Streaming in Action

## Terminal Output Examples

### Example 1: Simple Streaming

```
$ RawrXD-CLI

┌─────────────────────────────────────────────────────────────────┐
│ RawrXD AI CLI v1.0.0 - Agentic IDE Command Line Interface      │
│ Type 'help' for available commands                             │
└─────────────────────────────────────────────────────────────────┘

RawrXD> load models/mistral-7b.gguf
[INFO] Loading model: models/mistral-7b.gguf
[INFO] Model loaded successfully
✓ Loaded: mistral-7b.gguf (7.3B params, 4-bit quantized)

RawrXD> stream What is artificial intelligence?
[INFO] Starting streaming inference...
[INFO] Tokenized into 6 tokens
Streaming response:
Artificial intelligence is the simulation of human intelligence
processes by computer systems, particularly machine learning and
deep learning. It encompasses natural language processing, computer
vision, robotics, and more. AI is transforming industries from
healthcare to finance...
[SUCCESS] Streaming complete: 47 tokens generated

RawrXD>
```

### Example 2: Interactive Chat

```
RawrXD> chat
[INFO] Interactive Chat Mode - Type 'exit' to quit
[INFO] Each response will be generated with streaming...

You> What is machine learning?
Assistant> Machine learning is a subset of artificial intelligence that focuses on
enabling computers to learn from data without being explicitly programmed. It involves
developing algorithms that can identify patterns and make predictions based on training
data. Common approaches include supervised learning, unsupervised learning, and
reinforcement learning.

You> Can you give me an example?
Assistant> Sure! A classic example is email spam detection. The system is trained on
thousands of emails labeled as "spam" or "not spam". It learns patterns that distinguish
spam emails and can then automatically classify new incoming emails. Other examples include
handwriting recognition, recommendation systems, and autonomous vehicles.

You> How is this different from traditional programming?
Assistant> In traditional programming, a programmer writes explicit rules and logic to solve
problems. In machine learning, instead of writing rules, you provide data and algorithms
that learn to find patterns themselves. This makes ML better for problems where the rules
are complex or change over time.

You> exit
[INFO] Goodbye!

RawrXD>
```

### Example 3: Parameter Adjustment

```
RawrXD> temp 0.3
✓ Temperature set to: 0.3
[INFO] Temperature affects randomness of token selection (higher = more random)

RawrXD> topp 0.5
✓ Top-P set to: 0.5
[INFO] Top-P (nucleus sampling) controls diversity (lower = more focused)

RawrXD> maxtokens 50
✓ Max tokens set to: 50
[INFO] This controls maximum length of generated responses

RawrXD> stream What is gravity?
[INFO] Starting streaming inference...
[INFO] Tokenized into 4 tokens
Streaming response:
Gravity is the force that attracts objects toward each other. It keeps planets orbiting
the sun and holds our atmosphere in place.
[SUCCESS] Streaming complete: 28 tokens generated

RawrXD>
```

### Example 4: Error Handling

```
RawrXD> stream Hello
✗ ERROR: No model loaded. Use 'load <path>' first.

RawrXD> load nonexistent.gguf
✗ ERROR: Model file not found: nonexistent.gguf

RawrXD> temp xyz
✗ ERROR: Invalid temperature value. Use a number between 0.0 and 2.0

RawrXD>
```

## Visual Token Generation Timeline

### Streaming in Real-Time (Actual Output)

```
Time    What You See
──────────────────────────────────────────────────────────────
0ms     Streaming response:
        │ (cursor here)
        
100ms   Streaming response:
        The│ (token appears)

200ms   Streaming response:
        The quick│

300ms   Streaming response:
        The quick brown│

400ms   Streaming response:
        The quick brown fox│

500ms   Streaming response:
        The quick brown fox jumps│

600ms   Streaming response:
        The quick brown fox jumps over│

700ms   Streaming response:
        The quick brown fox jumps over the│

800ms   Streaming response:
        The quick brown fox jumps over the lazy│

900ms   Streaming response:
        The quick brown fox jumps over the lazy dog.│
```

**Key observation**: Each token appears as it's generated, not all at once!

## Comparison: Streaming vs Non-Streaming

### Non-Streaming (infer command)
```
RawrXD> infer What is AI?
[INFO] Generating response (non-streaming)...
[INFO] Tokenized into 4 tokens

Response: Artificial intelligence is the simulation of human intelligence 
processes by computer systems. It includes machine learning, natural language 
processing, robotics, and computer vision.

RawrXD>
```
**Appearance**: Text appears all at once after processing completes

### Streaming (stream command)
```
RawrXD> stream What is AI?
[INFO] Starting streaming inference...
[INFO] Tokenized into 4 tokens
Streaming response:
Artificial intelligence is the simulation of human intelligence
processes by computer systems. It includes machine learning, natural language
processing, robotics, and computer vision.
[SUCCESS] Streaming complete: 42 tokens generated

RawrXD>
```
**Appearance**: Text appears token-by-token in real-time

## Command Flow Diagram

```
┌─────────────┐
│ User Input  │
└──────┬──────┘
       │
       ▼
┌──────────────────┐
│ Parse Command    │◄─ "stream hello"
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│ Execute Command  │◄─ cmdInferStream()
└──────┬───────────┘
       │
       ├─ Validate model loaded
       │
       ├─ Tokenize prompt
       │  "hello" → [1, 2234, 2]
       │
       ├─ Initialize generation loop
       │
       ├──┬─ Token 1: 345 → "How"
       │  │  Display: "How"
       │  │  Sleep: 10ms
       │
       ├──┬─ Token 2: 456 → " can"
       │  │  Display: " can"
       │  │  Sleep: 10ms
       │
       ├──┬─ Token 3: 567 → " I"
       │  │  Display: " I"
       │  │  Sleep: 10ms
       │
       ├──┬─ Token 4: 678 → " help?"
       │  │  Display: " help?"
       │  │  Check EOS: No
       │  │  Sleep: 10ms
       │
       ├──┬─ Token 5: 2 → [EOS]
       │  │  Display: (none)
       │  │  Check EOS: YES → Break
       │
       └─ Complete
         
       ▼
┌──────────────────┐
│ Return Control   │
└──────────────────┘
       │
       ▼
   RawrXD>
```

## Memory & Performance Profile

### Per-Token Breakdown

```
Token Processing Pipeline
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Infer (Model)     ▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮  15-40ms  (model inference)
Detokenize        ▮                   1-2ms    (token to text)
Output/Flush      ▮                   <1ms     (display)
Sleep             ▮▮▮▮▮▮▮▮▮▮          10ms     (artificial delay)
─────────────────────────────────────
Total per token:  ~27-53ms (depends on model)
```

### Complete Response Generation

```
50-token response with 30ms average per token:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Tokenization      ▮▮                  5ms
Generation loop   ▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮    1500ms (50 tokens × 30ms)
Display           ▮                   <1ms
─────────────────────────────────────
Total:            ~1.5 seconds visible to user
```

## Settings Impact Visualization

### Temperature Effects (Same Prompt)

```
Temperature 0.1 (Focused)
━━━━━━━━━━━━━━━━━━━━━
"Quantum computing is the technology..."
(Very deterministic, similar each time)

Temperature 0.7 (Default)
━━━━━━━━━━━━━━━━━━━━
"Quantum computing represents a paradigm shift..."
(Natural, varied but coherent)

Temperature 1.5 (Creative)
━━━━━━━━━━━━━━━━━━━━
"Quantum computing opens weird possibilities to..."
(Highly creative, sometimes nonsensical)
```

### Top-P Effects

```
Top-P 0.5 (Focused)
━━━━━━━━━━━━━━━━━━━━
Only top 50% probability tokens selected
→ Shorter, more direct responses

Top-P 0.9 (Default)
━━━━━━━━━━━━━━━━━━━━
Top 90% probability tokens selected
→ Natural, balanced responses

Top-P 1.0 (Unrestricted)
━━━━━━━━━━━━━━━━━━━━
All tokens considered
→ Long, meandering, less focused
```

## Conversation Context Visualization

### Chat with Growing Context

```
Turn 1
┌─────────────────────────────┐
│ You: What is Python?        │
│ Context: 1 message          │
└─────────────────────────────┘
      ↓
┌─────────────────────────────┐
│ Assistant: [response...]    │
│ Context: 2 messages         │
└─────────────────────────────┘

Turn 2
┌──────────────────────────────┐
│ You: Can I use it for web?   │
│ Context: 4 messages          │
│ (Includes previous Q&A)      │
└──────────────────────────────┘
      ↓
┌──────────────────────────────┐
│ Assistant: [response...]     │
│ Context: 6 messages          │
└──────────────────────────────┘

Turn 3-5
[Similar context growth]

Turn 6+
┌──────────────────────────────┐
│ Context: Last 10 turns only  │
│ (20 messages max)            │
│ Oldest turn dropped →        │
│ New turn added ↓             │
└──────────────────────────────┘
```

**Note**: History kept manageable at 10 turns (20 messages)

## Token Count Visualization

### Example Response Breakdown

```
Input Prompt:        7 tokens
┌──────────────────────────────────┐
│ "Tell me about" (3 tokens)       │
│ "quantum computing" (4 tokens)   │
└──────────────────────────────────┘

Generated Response:  47 tokens
┌──────────────────────────────────────────────────────────┐
│ "Quantum computing is a revolutionary technology..." (47) │
└──────────────────────────────────────────────────────────┘

Total:               54 tokens
```

## Troubleshooting Visual Guide

### Issue: No Output

```
RawrXD> stream Hello
(nothing happens)

Diagnosis tree:
─────────────────────────────────
Is model loaded?
 ├─ NO → Load model first: load path/to/model.gguf
 └─ YES → Is prompt empty?
      ├─ NO → Is inference engine responsive?
      │     └─ Try: modelinfo
      └─ YES → Type a non-empty prompt
```

### Issue: Very Slow

```
RawrXD> stream Tell me a story
(taking very long time...)

Possible causes:
─────────────────────────────────
1. Large model (13B+ params)
   Solution: Reduce maxtokens

2. CPU-only inference
   Solution: Enable GPU if available

3. Small hardware
   Solution: Use smaller model (7B)

4. High maxtokens limit
   Solution: maxtokens 50
```

## Success Indicators

### ✅ Everything Working
```
✓ Model loads successfully
✓ Streaming produces real-time output
✓ Parameters take effect
✓ Chat maintains context
✓ Responses are coherent
✓ No crashes or errors
```

### ⚠️ Partially Working
```
⚠ Model loads but no output
  → Check if model format is GGUF

⚠ Output appears all at once
  → stdout buffering issue

⚠ Very slow generation
  → Model size or hardware limitation

⚠ Responses are gibberish
  → Model may be untrained/corrupted
```

---

**Use this guide to**:
- Understand what to expect
- Recognize if something is working correctly
- Troubleshoot common issues
- Appreciate the streaming effect in action

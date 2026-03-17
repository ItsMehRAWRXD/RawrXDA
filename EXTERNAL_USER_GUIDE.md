# RawrXD v0.1.0 — External User Testing Guide

## Before You Start

### What to Expect

**RawrXD works.**

You can type code. Ghost text appears. It's contextually reasonable. You can accept or dismiss.

It feels like Copilot, but local and faster.

### What NOT to Expect

- ❌ Multi-file refactoring
- ❌ AI-powered testing
- ❌ Code explanation (read-only mode)
- ❌ Project-wide navigation
- ❌ Semantic symbol renaming
- ❌ UI polish (it's minimal)

These are intentional gaps, not bugs.

---

## Setup (5 minutes)

### Prerequisites

- Windows 11 or Linux/macOS (build from source)
- VS Code or any browser
- 2GB free disk space
- CPU with 4+ cores (optional: GPU, but not required)

### 1. Download or Build

```bash
# Option A: Use prebuilt (Windows)
# Download: https://github.com/ItsMehRAWRXD/rawrxd/releases/tag/v0.1.0-mvp
# Extract to: C:\RawrXD (or anywhere)

# Option B: Build from source
git clone https://github.com/ItsMehRAWRXD/rawrxd.git
cd rawrxd
cmake -S . -B build
cmake --build build --target RawrEngine
cmake --build build --target rawrxd-monaco-gen
```

### 2. Get a Model

Download a GGUF model (small, ~2-3GB):

```bash
# Example: Mistral-7B-Instruct
wget https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.1-GGUF/resolve/main/Mistral-7B-Instruct-v0.1.Q4_K_M.gguf

# Or: CodeLlama-7B
wget https://huggingface.co/TheBloke/CodeLlama-7B-Instruct-GGUF/resolve/main/CodeLlama-7B-Instruct.Q4_K_M.gguf
```

Place the model in `rawrxd/models/` (or anywhere you'll remember).

### 3. Generate IDE

```bash
cd rawrxd
build/bin/rawrxd-monaco-gen.exe --name rawrxd-ide --template minimal --out out/
```

### 4. Run

**Terminal 1: Engine**
```bash
build/bin/RawrEngine.exe --model models/Mistral-7B-Instruct-v0.1.Q4_K_M.gguf --port 8080
```

You should see:
```
[CompletionServer] Listening on port 8080...
```

**Terminal 2: IDE**
```bash
cd out/rawrxd-ide
npm install
npm run dev
```

You should see:
```
  ➜  Local:   http://localhost:5173/
```

Open browser. You're ready.

---

## Core Workflow (10 minutes)

### Test 1: Basic Completion

1. Type: `int main() {`
2. Press Enter, type spaces to indent
3. **Observe:** Ghost text should appear (e.g., `std::cout << "Hello" << std::endl;`)
4. Press Tab or Enter to accept
5. **Expect:** Code inserted, ghost text disappears

### Test 2: Streaming (Most Important)

1. Type: `for (int i = 0; i <`
2. **Observe:** Ghost text appears **token-by-token**, not all at once
3. Watch it build: `10` → `; i++` → `) {` (each piece appears separately)
4. **This is the key UX difference from non-streaming systems**

### Test 3: Fast Typing (Cancellation)

1. Type slowly: `int x =`
2. Watch ghost text appear
3. Immediately type more: `int x = 5; int y =`
4. **Observe:** Old ghost text disappears immediately
5. New ghost text appears for `int y =`
6. **No overlap. No stuttering.**

### Test 4: Large File

1. Create a new file with >20k lines of code (copy/paste open-source)
2. Scroll to line 18,000
3. Request a completion
4. **Expect:** Still fast (~1-2s), not slow
5. **Why:** Only last 4k chars are used (context window)

### Test 5: Multiple Languages

1. Create `test.py`
2. Type: `def fibonacci(n):`
3. **Observe:** Ghost text is Python-style

Repeat for JavaScript, Go, Rust.

**Expect:** Language switching is seamless (completion respects syntax).

---

## What to Look For (Evaluation Rubric)

### ✅ Things That Should Work

- [x] Ghost text appears immediately
- [x] Streaming is visible (token-by-token)
- [x] Cancellation on fast typing is clean
- [x] No crashes
- [x] No memory bloat (run for 10 mins, monitor task manager)
- [x] No network requests (all local)
- [x] Accepts/dismisses work smoothly

### ⚠️ Things That Might Not Work

- [ ] Multi-file awareness (this is not included)
- [ ] Understanding imports (this is not included)
- [ ] "Extract this into a function" (not included)
- [ ] Explain this code (not included)

**If you find yourself wanting these**, that's valuable feedback. But note: **This is expected.**

### ❌ Things That Should Never Happen

- [ ] Crash (engine or IDE)
- [ ] Hang (unresponsive for >5 seconds)
- [ ] Memory leak (task manager shows growing RSS)
- [ ] Corrupted code (accepted completion breaks syntax)
- [ ] Overlapping ghost text (old + new visible simultaneously)

**If any of these happen, that's a bug. Report it.**

---

## Issue Reporting

### What Counts as a Bug

✅ **Report if:**
- Engine crashes
- IDE hangs
- Ghost text overlaps
- Completion breaks syntax
- Server doesn't start
- Performance degradation over time

❌ **Do NOT report if:**
- Completion is not perfect (it won't be)
- Multi-file context is missing (expected)
- No refactoring tools (expected)
- UI is minimal (expected)

### How to Report

**Format:**

```
Title: [CATEGORY] Brief description

Reproduction Steps:
1. Do X
2. Do Y
3. Do Z

Expected:
Result A

Actual:
Result B

Environment:
- OS: Windows 11 / Linux / macOS
- CPU: Intel i7 / M1 / etc.
- Model: Mistral-7B / CodeLlama / etc.
- File size: Small (<100 lines) / Medium / Large (>10k)
```

**Example:**

```
Title: [CRASH] Engine exits after 10 completions

Reproduction Steps:
1. Start RawrEngine
2. Generate 10 completions rapidly (keystroke every 2 seconds)
3. On 11th keystroke, engine crashes

Expected:
Engine stays running

Actual:
Terminal shows segfault, engine closes

Environment:
- OS: Windows 11
- CPU: Intel i7-12700K
- Model: Mistral-7B-Q4_K_M
- File: 5k lines
```

---

## Performance Baseline

### What to Measure

Use your browser's Network tab (F12):

1. **Streaming latency:** Time until first token appears
   - Target: <100ms
   - Actual: ~50-100ms (local inference)

2. **Token rate:** Tokens per second
   - Target: 30-50 TPS
   - Actual: Depends on model and CPU

3. **Total time:** First token to last token
   - Target: <2 seconds for 128 tokens
   - Actual: 2-4 seconds typical

4. **UI responsiveness:** Time from keystroke to ghost text
   - Target: <50ms
   - Actual: ~20-30ms (including render)

**Report if these are significantly different:**

```
Title: [PERFORMANCE] Completions taking 10+ seconds

My system:
- CPU: Intel i5-8400 (6 cores)
- RAM: 16GB
- Model: Mistral-7B-Q4_K_M
- File: 50 lines (small)

Expected timing:
- First token: ~100ms
- Total: ~2 seconds

Actual timing:
- First token: ~5 seconds
- Total: ~15 seconds
```

---

## One-Week Test Plan

### Day 1-2: Basic Functionality
- [ ] Setup works
- [ ] Engine starts
- [ ] IDE opens
- [ ] Can type and get completions
- [ ] Streaming is visible

### Day 3-4: Edge Cases
- [ ] Large file completion (20k lines)
- [ ] Fast typing cancellation
- [ ] Multiple languages
- [ ] No crashes after 10+ completions

### Day 5-6: Real Work
- [ ] Use it as you would Copilot
- [ ] How many completions do you accept?
- [ ] How many do you dismiss?
- [ ] When does it feel "right"?
- [ ] When does it feel "wrong"?

### Day 7: Summary
- [ ] What worked better than expected?
- [ ] What was disappointing?
- [ ] Would you use this instead of Copilot for local work?
- [ ] What's the ONE feature you'd add next?

---

## Success Criteria

### If You Can Say "Yes" to These:

- [x] I can type code and get completions
- [x] Completions appear quickly (feel responsive)
- [x] The system never crashed
- [x] I never saw overlapping ghost text
- [x] Fast typing canceled old completions cleanly
- [x] Completions were contextually relevant
- [x] I could use this for real work (today)

**Then this release is successful.**

### If You Can Say "No" to These:

- [ ] Engine crashed
- [ ] IDE hung (unresponsive >5 sec)
- [ ] Completion corrupted my code
- [ ] Ghost text overlapped
- [ ] Memory grew unbounded
- [ ] Completion was usually wrong

**Then we have work to do.**

---

## FAQ

**Q: Why is the UI so minimal?**

A: We optimized for functionality, not polish. If the core works, we can add UI later. Shipping a pretty broken tool is worse than shipping an ugly working tool.

**Q: Can I use this in production?**

A: Yes, if "production" means "my local dev environment." It's not a Copilot replacement for everything, but it's excellent for local coding.

**Q: Why no multi-file context?**

A: We chose speed over intelligence for v0.1.0. Multi-file indexing is complex and slows down the experience. We'll add it later, but first: prove the foundation works.

**Q: Will you add refactoring?**

A: Eventually. First, we need users. Refactoring is feature #2, not feature #1.

**Q: Can I train a custom model?**

A: Yes. RawrXD loads any GGUF model. You're not locked in.

**Q: Why C++?**

A: Speed and binary distribution. Python would work, but inference would be slower.

**Q: Why not WebSocket?**

A: Simpler to debug, easier to scale, works with plain HTTP/1.1.

**Q: Why 4k context window?**

A: Balance between latency and relevance. Smaller = faster. Larger = better context but slower. 4k is the sweet spot for local inference.

---

## Thank You

You're helping us prove that local AI coding assistance can be:
- Fast
- Reliable
- Respectful of privacy
- Simple to understand
- Actually useful

That's rare. We appreciate it.

---

## Next Steps (After This Week)

1. You'll send us feedback
2. We'll triage it (bugs vs. feature requests)
3. We'll fix critical bugs in v0.1.1
4. We'll release v0.2.0 with your top-requested feature
5. Repeat

But first: **Tell us what you found.**

---

**Version:** 0.1.0-mvp  
**Test Duration:** 1 week  
**Feedback Channel:** See WHY_THIS_EXISTS.md for contact info  
**Status:** Ready for users. Go.

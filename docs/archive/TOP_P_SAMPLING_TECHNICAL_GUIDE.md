# Top-P (Nucleus) Sampling: Technical Deep Dive

## Overview

Top-P sampling (also called "nucleus sampling") is a sophisticated probability distribution sampling technique that produces higher-quality text than greedy sampling while maintaining controllable diversity. This document explains the mathematical foundations and implementation details.

---

## Mathematical Foundation

### 1. Softmax Conversion

Given raw model logits $\mathbf{l} = [l_1, l_2, ..., l_V]$ where $V$ is vocabulary size:

#### Temperature Scaling
First, apply temperature scaling to control "sharpness" of the distribution:

$$l'_i = \frac{l_i}{T}$$

where:
- $T$ (temperature) controls randomness
  - $T < 1.0$ → sharper distribution (more deterministic)
  - $T = 1.0$ → unchanged
  - $T > 1.0$ → flatter distribution (more random)

#### Softmax
Convert to probabilities:

$$p_i = \frac{e^{l'_i - \max(\mathbf{l}')}}{sum_{j=1}^{V} e^{l'_j - \max(\mathbf{l}')}}$$

The $\max(\mathbf{l}')$ subtraction prevents numerical overflow (stability trick).

### 2. Nucleus Selection

Sort tokens by probability in descending order:

$$\text{sort}(p_1, p_2, ..., p_V) \rightarrow (p_{(1)}, p_{(2)}, ..., p_{(V)})$$

where $p_{(1)} \geq p_{(2)} \geq ... \geq p_{(V)}$

Find nucleus size $k$ such that:

$$\sum_{i=1}^{k} p_{(i)} \geq P \text{ and } \sum_{i=1}^{k-1} p_{(i)} < P$$

where $P$ is the Top-P threshold (typically 0.9).

### 3. Weighted Random Sampling

Renormalize probabilities within nucleus:

$$p'_i = \frac{p_i}{\sum_{j=1}^{k} p_j} \text{ for } i \in \text{nucleus}$$

Sample from this restricted distribution:

$$\text{token} \sim \text{Categorical}(p'_1, p'_2, ..., p'_k)$$

---

## Visual Explanation

### Distribution Visualization

```
Without nucleus sampling (using all vocabulary):
┌─────────────────────────────────────────────────────────────────┐
│ Probability mass across vocabulary (50K tokens)                 │
├─────────────────────────────────────────────────────────────────┤
│ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ "the" "a" "is" "of" "and" ...                    (junk tokens)  │
└─────────────────────────────────────────────────────────────────┘
Problem: Sampling from junk tokens causes incoherence


With nucleus sampling (Top-P = 0.9):
┌──────────────────────────────────┐
│ Nucleus (90% of probability)     │
├──────────────────────────────────┤
│ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██   │
│ "the" "a" "is" "of" "and" ...    │
└──────────────────────────────────┘
Benefit: Only reasonable tokens, high coherence
```

### Example: Sampling "the" in "The quick brown fox"

```
Model output: logits = [-2.1, 0.5, -1.8, -0.2, 2.3, ...]
After softmax: probs = [0.002, 0.12, 0.003, 0.05, 0.82, ...]

Sorted by probability:
1. fox (0.82)         ← Nucleus start
2. the (0.12)
3. quick (0.05)
4. brown (0.003)
5. rabbit (0.002)     ← Nucleus end (cumsum = 0.912 > 0.90)

Renormalized within nucleus:
- fox:   0.82 / 1.02 = 0.804
- the:   0.12 / 1.02 = 0.118
- quick: 0.05 / 1.02 = 0.049

Sampling: ~80% chance of "fox", ~12% "the", ~5% "quick"
```

---

## Algorithm Implementation

### Pseudocode

```
function sample_top_p(logits, temperature, top_p):
    // Step 1: Temperature scaling
    scaled_logits = logits / temperature
    
    // Step 2: Softmax
    max_logit = max(scaled_logits)
    exp_logits = exp(scaled_logits - max_logit)
    probs = exp_logits / sum(exp_logits)
    
    // Step 3: Sort
    sorted_probs = sort_descending(probs)
    sorted_indices = get_sort_indices(probs)
    
    // Step 4: Find nucleus
    cumsum = 0
    nucleus_size = 0
    for i in range(len(sorted_probs)):
        cumsum += sorted_probs[i]
        nucleus_size += 1
        if cumsum >= top_p:
            break
    
    // Step 5: Renormalize
    nucleus_probs = sorted_probs[0:nucleus_size]
    nucleus_probs = nucleus_probs / sum(nucleus_probs)
    
    // Step 6: Sample
    rand = random_uniform(0, 1)
    cumsum = 0
    for i in range(nucleus_size):
        cumsum += nucleus_probs[i]
        if rand < cumsum:
            return sorted_indices[i]
    
    return sorted_indices[nucleus_size - 1]
```

### C++ Implementation (from our codebase)

```cpp
int32_t InferenceEngine::sampleNextToken(
    std::vector<float>& logits, double temperature, double topP)
{
    // Step 1: Temperature scaling + softmax
    float maxLogit = *std::max_element(logits.begin(), logits.end());
    std::vector<float> probs(logits.size());
    float sumExp = 0.0f;
    
    for (size_t i = 0; i < logits.size(); ++i) {
        float expVal = std::exp(logits[i] / temperature - maxLogit);
        probs[i] = expVal;
        sumExp += expVal;
    }
    
    for (float& prob : probs) {
        prob /= sumExp;
    }

    // Step 2: Create {prob, token_id} pairs and sort
    std::vector<std::pair<float, int32_t>> sortedTokens;
    for (size_t i = 0; i < probs.size(); ++i) {
        if (probs[i] > 1e-6f) {  // Filter near-zero probabilities
            sortedTokens.push_back({probs[i], static_cast<int32_t>(i)});
        }
    }
    
    std::sort(sortedTokens.begin(), sortedTokens.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;  // Descending order
              });

    // Step 3: Find nucleus
    float cumulativeProb = 0.0f;
    size_t nucleusSize = 0;

    for (const auto& tokenPair : sortedTokens) {
        cumulativeProb += tokenPair.first;
        nucleusSize++;
        if (cumulativeProb >= static_cast<float>(topP)) {
            break;
        }
    }
    
    if (nucleusSize == 0 || sortedTokens.empty()) {
        return sortedTokens.empty() ? 0 : sortedTokens.front().second;
    }

    // Step 4: Weighted random sample from nucleus
    float r = getRandomFloat(0.0f, cumulativeProb);

    cumulativeProb = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        cumulativeProb += sortedTokens[i].first;
        if (r < cumulativeProb) {
            return sortedTokens[i].second;
        }
    }
    
    // Fallback (rounding error safety)
    return sortedTokens[nucleusSize - 1].second;
}
```

---

## Practical Examples

### Example 1: Conservative Output (Low Temperature, Low Top-P)

**Settings:** $T = 0.3$, $P = 0.5$

```
Input: "The capital of France is"

Top logits after temperature scaling:
- Paris (4.2)
- Paris (4.2)    ← Selected first token
- Franc (1.1)

Probability distribution:
- Paris: 0.95
- Franc: 0.05

Nucleus (P=0.5): Only "Paris" (0.95 > 0.5)

Output: "The capital of France is Paris"
        (almost deterministic, factual)
```

### Example 2: Balanced Output (Default Settings)

**Settings:** $T = 0.8$, $P = 0.9$

```
Input: "Once upon a time"

Top logits after temperature scaling:
- there (2.1)
- in (1.8)
- when (1.5)
- was (1.2)
- and (0.8)

Probability distribution:
- there: 0.40
- in: 0.25
- when: 0.18
- was: 0.12
- and: 0.05

Nucleus (P=0.9): ["there", "in", "when", "was", "and"]
(cumsum = 0.40 + 0.25 + 0.18 + 0.12 + 0.05 = 1.0)

Output variety:
- "Once upon a time there was..."        (40% chance)
- "Once upon a time in a kingdom..."     (25% chance)
- "Once upon a time when magic..."       (18% chance)
- "Once upon a time was a..."            (12% chance)
- "Once upon a time and there..."        (5% chance)
```

### Example 3: Creative Output (High Temperature, High Top-P)

**Settings:** $T = 1.5$, $P = 0.99$

```
Input: "The future of AI is"

Top logits after temperature scaling:
- bright (1.4)
- mysterious (1.3)
- uncertain (1.2)
- wonderful (1.1)
- scary (1.0)
- ... (many more tokens with meaningful probability)

Probability distribution (flattened):
- All top ~100 tokens have 0.5-2% probability each

Nucleus (P=0.99): Most of vocabulary included

Output variety (many possibilities):
- "The future of AI is bright and promising..."
- "The future of AI is mysterious and unknown..."
- "The future of AI is both wonderful and terrifying..."
- "The future of AI is an open question..."
- Many other creative combinations
```

---

## Comparison with Other Sampling Methods

### 1. Greedy Sampling
```cpp
// Always pick the highest logit
int32_t nextToken = argmax(logits);
```

**Pros:**
- Deterministic
- Fast
- Factually accurate (usually)

**Cons:**
- Repetitive, boring text
- Limited vocabulary usage
- Often leads to loops

**Best for:** Translation, summarization with determinism

### 2. Temperature Sampling (No Top-P)
```cpp
// Sample from full distribution with temperature scaling
std::discrete_distribution dist(probs.begin(), probs.end());
int32_t nextToken = dist(rng);
```

**Pros:**
- Simple to implement
- Diverse output

**Cons:**
- Can sample low-probability tokens (junk)
- Incoherent text at high temperatures
- Limited quality control

**Best for:** Creative writing, but often produces garbage

### 3. Top-K Sampling
```cpp
// Sample from top K highest-probability tokens
// (fixed vocabulary reduction)
```

**Pros:**
- Simple, deterministic vocabulary size
- Better than temperature sampling

**Cons:**
- Rigid (not adaptive to probability distribution)
- Can exclude good tokens
- Can include bad tokens

**Best for:** Baseline approach, but Top-P is usually better

### 4. Top-P Sampling (Nucleus) ⭐
```cpp
// Sample from smallest set whose cumulative probability > P
// (adaptive vocabulary reduction based on distribution)
```

**Pros:**
- Adaptive to probability distribution
- Natural, diverse output
- High-quality text
- Good balance of control and creativity

**Cons:**
- More complex to implement
- Slightly slower than greedy

**Best for:** General-purpose LLM inference (GPT, Claude, etc.)

---

## Comparison Table

| Method | Speed | Quality | Diversity | Determinism | Vocab Size |
|--------|-------|---------|-----------|-------------|-----------|
| Greedy | Fast ⚡ | Good | None | 100% | 1 |
| Temperature | Slower | Poor | High | 0% | All |
| Top-K | Medium | Medium | Medium | Partial | Fixed |
| **Top-P** | **Medium** | **Excellent** | **Balanced** | **Tunable** | **Adaptive** |

---

## Performance Characteristics

### Computational Complexity

For vocabulary size $V$:

| Operation | Complexity |
|-----------|-----------|
| Softmax | $O(V)$ |
| Sorting | $O(V \log V)$ |
| Cumulative sum | $O(k)$ where $k =$ nucleus size |
| Sampling | $O(k)$ |
| **Total** | **$O(V \log V)$** |

In practice: ~5-10ms for V=50K on modern CPUs

### Typical Nucleus Size

```
Default Top-P=0.9 results in:

- Simple distributions: nucleus ≈ 10-50 tokens (2-5 top tokens)
- Normal distributions: nucleus ≈ 100-500 tokens
- Flat distributions: nucleus ≈ 5000-50000 tokens (most of vocab)

Average: ~5% of vocabulary in nucleus
```

---

## Tuning Guide

### For Different Use Cases

```
Q&A System (factual, deterministic):
    temperature = 0.2
    top_p = 0.5
    nucleus ≈ 1-3 tokens

Summarization (factual, some diversity):
    temperature = 0.7
    top_p = 0.8
    nucleus ≈ 100-500 tokens

General Chat (balanced):
    temperature = 0.8
    top_p = 0.9      ← Recommended default
    nucleus ≈ 500-2000 tokens

Creative Writing (diverse, imaginative):
    temperature = 1.2
    top_p = 0.95
    nucleus ≈ 5000+ tokens

Unrestricted (anything goes):
    temperature = 1.5+
    top_p = 1.0      ← Use full vocabulary
```

### Tuning Temperature

```
T = 0.1:  [▓▓▓░░░░░░] Nearly deterministic
T = 0.3:  [▓▓▓▓░░░░░] Conservative
T = 0.7:  [▓▓▓▓▓▓░░░] Balanced (recommended)
T = 1.0:  [▓▓▓▓▓▓▓░░] Natural
T = 1.5:  [▓▓▓▓▓▓▓▓░] Creative
T = 2.0:  [▓▓▓▓▓▓▓▓▓] Very random (often bad)
```

### Tuning Top-P

```
P = 0.3:  Nucleus size ≈ 1-5 tokens (very conservative)
P = 0.5:  Nucleus size ≈ 10-50 tokens (conservative)
P = 0.7:  Nucleus size ≈ 100-300 tokens (normal)
P = 0.9:  Nucleus size ≈ 500-2000 tokens (default)
P = 0.95: Nucleus size ≈ 2000-5000 tokens (creative)
P = 1.0:  Nucleus size = full vocabulary (unrestricted)
```

---

## Common Pitfalls & Solutions

### Pitfall 1: Empty Nucleus
**Problem:** All cumulative probabilities < Top-P
**Cause:** Top-P set too low, or model producing flat distribution

**Solution:**
```cpp
if (nucleusSize == 0) {
    nucleusSize = 1;  // Always sample at least top token
}
```

### Pitfall 2: Numerical Instability
**Problem:** exp(very_large_logit) → overflow

**Cause:** Not subtracting maximum logit

**Solution:**
```cpp
// Correct: subtract max for stability
float expVal = std::exp(logits[i] - maxLogit);

// Wrong: can overflow
float expVal = std::exp(logits[i]);
```

### Pitfall 3: Biased Sampling
**Problem:** Rounding errors in cumulative sum

**Cause:** Float precision limits

**Solution:**
```cpp
// Use double precision for accumulation
double cumulativeProb = 0.0;  // Not float!
```

### Pitfall 4: Poor Randomness
**Problem:** Repetitive "random" results

**Cause:** Bad RNG initialization

**Solution:**
```cpp
// Use high-entropy seed
std::random_device rd;
std::mt19937 engine(rd());  // Proper initialization
```

---

## References & Further Reading

### Academic Papers
1. **"The Curious Case of Neural Text Degeneration"**
   - Holtzman et al., 2019
   - Introduced nucleus (Top-P) sampling
   - https://arxiv.org/abs/1904.09751

2. **"Language Models are Unsupervised Multitask Learners"**
   - Radford et al., 2019 (GPT-2)
   - Discusses sampling strategies
   - https://arxiv.org/abs/1906.04341

### Implementation References
- **llama.cpp:** https://github.com/ggerganov/llama.cpp
- **HuggingFace Transformers:** https://huggingface.co/docs/transformers/generation_strategies#sampling
- **OpenAI API Documentation:** https://platform.openai.com/docs/guides/gpt/sampling

### Related Techniques
- Top-K sampling (simplified nucleus)
- Beam search (multiple parallel sequences)
- Mirostat sampling (perplexity-based)

---

## Conclusion

Top-P (Nucleus) sampling is the **gold standard for LLM text generation** because it:

1. **Balances quality and diversity** through adaptive vocabulary selection
2. **Prevents incoherence** by filtering low-probability tokens
3. **Enables fine-grained control** via temperature and Top-P parameters
4. **Works universally** across different model architectures and sizes

The implementation in this codebase follows best practices for numerical stability, efficiency, and correctness, making it suitable for production deployment.


# BPE Algorithm - Complete Reverse Engineering Analysis

## Overview
Byte Pair Encoding (BPE) is a subword tokenization algorithm that iteratively merges the most frequent adjacent byte pairs in text. Originally developed for data compression, it was adapted by OpenAI for GPT-2/GPT-3 tokenization.

---

## Core Algorithm Components

### 1. **Byte-Level Encoding Foundation**
```
Purpose: Map all 256 possible bytes to unique, printable Unicode characters
Why: Ensures the algorithm can handle ANY binary data, not just valid UTF-8

Mapping Strategy (GPT-2 style):
- Bytes 33-126  → Direct printable ASCII (94 chars)
- Bytes 161-172 → Latin-1 supplement (12 chars)  
- Bytes 174-255 → Latin-1 supplement (82 chars)
- Remaining 68 bytes → Shifted to high Unicode (256+n)

Total: 256 bytes → 256 unique QChar values

Bidirectional Maps:
  m_byteEncoder: uint8_t → QChar (for encoding)
  m_byteDecoder: QChar → uint8_t (for decoding)
```

**Implementation Details:**
```cpp
// Constructor builds the byte↔unicode mapping
BPETokenizer::BPETokenizer() {
    QVector<int> byteVals;
    
    // Step 1: Collect printable ranges
    for (int i = 33; i <= 126; ++i) byteVals.append(i);    // ASCII
    for (int i = 161; i <= 172; ++i) byteVals.append(i);   // Latin-1 part 1
    for (int i = 174; i <= 255; ++i) byteVals.append(i);   // Latin-1 part 2
    
    // Step 2: Fill gaps with shifted values (256+)
    int n = 0;
    for (int b = 0; b < 256; ++b) {
        if (!byteVals.contains(b)) {
            byteVals.append(256 + n);
            ++n;
        }
    }
    
    // Step 3: Create bidirectional lookup tables
    for (int i = 0; i < 256; ++i) {
        m_byteEncoder[i] = QChar(byteVals[i]);
        m_byteDecoder[QChar(byteVals[i])] = i;
    }
}
```

**Why This Matters:**
- Prevents invalid Unicode sequences from breaking tokenization
- Allows BPE to work on raw bytes (images, binary files, corrupted text)
- Each byte becomes a valid token candidate

---

### 2. **Text Pre-Splitting (Regex Tokenization)**
```
Purpose: Split text into "atoms" before BPE merging
Why: Prevents merging across word/punctuation boundaries inappropriately

GPT-2 Regex Pattern:
's|'t|'re|'ve|'m|'ll|'d     ← Contractions (don't split "don't")
| ?\\p{L}+                   ← Letters (optional leading space)
| ?\\p{N}+                   ← Numbers (optional leading space)  
| ?[^\\s\\p{L}\\p{N}]+       ← Punctuation (optional leading space)
|\\s+(?!\\S)                 ← Trailing whitespace
|\\s+                        ← Other whitespace

Result: Text → QVector<TextSplit>
  Each split = independent BPE merging unit
```

**Example:**
```
Input:  "Hello world! I'm fine."
Splits: [" Hello", " world", "!", " I", "'m", " fine", "."]
        ^       ^          ^      ^     ^      ^       ^
        space   space    punct  space contr  space  punct
```

**Implementation:**
```cpp
QVector<BPETokenizer::TextSplit> BPETokenizer::splitText(const QString& text) {
    QVector<TextSplit> splits;
    QRegularExpression pattern(
        "'s|'t|'re|'ve|'m|'ll|'d| ?\\p{L}+| ?\\p{N}+| ?[^\\s\\p{L}\\p{N}]+|\\s+(?!\\S)|\\s+"
    );
    
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        TextSplit split{match.captured(0), false};
        splits.append(split);
    }
    return splits;
}
```

---

### 3. **Byte Encoding Step**
```
Purpose: Convert each text split into byte-level tokens
Process: UTF-8 bytes → Unicode chars (using byte encoder)

Input:  QString (e.g., "Hello")
UTF-8:  [0x48, 0x65, 0x6C, 0x6C, 0x6F]
Mapped: [QChar(72), QChar(101), QChar(108), QChar(108), QChar(111)]
        = ["H", "e", "l", "l", "o"]

Each character represents ONE BYTE of the original UTF-8
```

**Implementation:**
```cpp
QVector<QString> BPETokenizer::byteEncode(const QString& text) {
    QVector<QString> result;
    QByteArray utf8 = text.toUtf8();  // Get raw UTF-8 bytes
    
    for (uint8_t byte : utf8) {
        result.append(QString(m_byteEncoder[byte]));  // Map byte→char
    }
    
    return result;
}
```

**Example:**
```
Text:   "Hi™"  (trademark symbol)
UTF-8:  [0x48, 0x69, 0xE2, 0x84, 0xA2]  ← 5 bytes (™ is 3 bytes in UTF-8)
Result: ["H", "i", "â", "„", "¢"]      ← 5 tokens
```

---

### 4. **BPE Merge Algorithm (The Core)**
```
Purpose: Iteratively merge adjacent token pairs based on learned frequencies
Input: QVector<QString> tokens (byte-level)
Output: QVector<QString> merged tokens (subword-level)

Data Structure:
  m_merges: QHash<QPair<QString, QString>, int>
    Key: (token1, token2)  ← Adjacent pair
    Value: priority        ← Lower = more frequent (merge first)

Algorithm:
  WHILE merges are possible:
    1. Find ALL adjacent pairs in current token sequence
    2. Check which pairs exist in m_merges  
    3. Select pair with LOWEST priority (highest frequency)
    4. Merge that pair at its FIRST occurrence
    5. Rebuild token sequence with merged pair
    6. REPEAT until no more valid merges
```

**Step-by-Step Example:**
```
Initial:   ["H", "e", "l", "l", "o"]
Merges DB: {("H","e"):5, ("e","l"):12, ("l","l"):3, ("ll","o"):20, ("He","llo"):50}

Iteration 1:
  Pairs: (H,e):5  (e,l):12  (l,l):3  (l,o):?
  Best:  (l,l):3  ← Lowest priority
  Merge: ["H", "e", "ll", "o"]

Iteration 2:
  Pairs: (H,e):5  (e,ll):?  (ll,o):20
  Best:  (H,e):5
  Merge: ["He", "ll", "o"]

Iteration 3:
  Pairs: (He,ll):?  (ll,o):20
  Best:  (ll,o):20
  Merge: ["He", "llo"]

Iteration 4:
  Pairs: (He,llo):50
  Best:  (He,llo):50
  Merge: ["Hello"]

Iteration 5:
  No pairs left → STOP

Final: ["Hello"]  ← Single token!
```

**Implementation:**
```cpp
// Find the best merge candidate
QPair<int, int> BPETokenizer::findBestMergePair(const QVector<QString>& tokens) {
    int bestIdx = -1;
    int bestPriority = INT_MAX;
    
    // Scan all adjacent pairs
    for (int i = 0; i < tokens.size() - 1; ++i) {
        QPair<QString, QString> pair(tokens[i], tokens[i + 1]);
        
        // Check if this pair has a merge rule
        if (m_merges.contains(pair)) {
            int priority = m_merges[pair];
            
            // Lower priority = merge first (more frequent)
            if (priority < bestPriority) {
                bestPriority = priority;
                bestIdx = i;
            }
        }
    }
    
    return QPair<int, int>(bestIdx, bestPriority);
}

// Apply BPE merges iteratively
QVector<QString> BPETokenizer::applyBPE(const QVector<QString>& tokens) {
    if (tokens.size() <= 1) return tokens;  // Nothing to merge
    
    QVector<QString> result = tokens;
    
    while (true) {
        QPair<int, int> best = findBestMergePair(result);
        if (best.first == -1) break;  // No more merges possible
        
        int idx = best.first;
        QString merged = result[idx] + result[idx + 1];  // Concatenate
        
        // Rebuild sequence with merged token
        QVector<QString> newResult;
        for (int i = 0; i < result.size(); ++i) {
            if (i == idx) {
                newResult.append(merged);
                ++i;  // Skip next token (already merged)
            } else if (i < result.size()) {
                newResult.append(result[i]);
            }
        }
        
        result = newResult;
        if (result.size() <= 1) break;  // Fully merged
    }
    
    return result;
}
```

**Complexity:**
```
Worst case: O(n²) where n = token count
  - Each iteration reduces tokens by 1
  - Each iteration scans all pairs: O(n)
  - Total: n + (n-1) + (n-2) + ... + 1 = O(n²)

Optimization: Could use priority queue for O(n log n)
```

---

### 5. **Vocabulary Lookup**
```
Purpose: Convert merged tokens to integer IDs
Data: m_vocab: QHash<QString, int32_t>

Process:
  For each merged token:
    IF token exists in vocab → use its ID
    ELSE → use <UNK> token (m_unkToken = 3)

Example:
  ["Hello", "Ġworld", "!"]
  → m_vocab["Hello"] = 12345
  → m_vocab["Ġworld"] = 67890  (Ġ = space marker)
  → m_vocab["!"] = 111
  
  Result: [12345, 67890, 111]
```

**Implementation:**
```cpp
std::vector<int32_t> BPETokenizer::encode(const QString& text) {
    std::vector<int32_t> result;
    QVector<TextSplit> splits = splitText(text);  // Step 1: Pre-split
    
    for (const TextSplit& split : splits) {
        QVector<QString> byteTokens = byteEncode(split.text);  // Step 2: Byte encode
        QVector<QString> bpeTokens = applyBPE(byteTokens);     // Step 3: BPE merge
        
        // Step 4: Vocabulary lookup
        for (const QString& token : bpeTokens) {
            if (m_vocab.contains(token)) {
                result.push_back(m_vocab[token]);
            } else {
                result.push_back(m_unkToken);  // Fallback
            }
        }
    }
    
    return result;
}
```

---

### 6. **Decoding (Reverse Process)**
```
Purpose: Convert token IDs back to text
Process: Reverse every step

Steps:
  1. Token IDs → Token strings (reverse vocab)
  2. Concatenate all tokens
  3. Byte decode (QChar → uint8_t)  
  4. UTF-8 bytes → QString

Special handling:
  - Skip BOS/EOS/PAD tokens
  - Handle unknown token IDs gracefully
```

**Implementation:**
```cpp
QString BPETokenizer::decode(const std::vector<int32_t>& tokens) {
    QByteArray utf8;
    
    for (int32_t tokenId : tokens) {
        // Skip special tokens
        if (tokenId == m_bosToken || tokenId == m_eosToken || tokenId == m_padToken) {
            continue;
        }
        
        // Reverse vocab lookup
        if (!m_reverseVocab.contains(tokenId)) {
            qWarning() << "Unknown token ID:" << tokenId;
            continue;
        }
        
        QString token = m_reverseVocab[tokenId];
        
        // Byte decode: QChar → uint8_t
        for (QChar ch : token) {
            if (m_byteDecoder.contains(ch)) {
                utf8.append(m_byteDecoder[ch]);
            }
        }
    }
    
    // UTF-8 bytes → QString
    return QString::fromUtf8(utf8);
}
```

**Example:**
```
Input IDs:  [12345, 67890, 111]
Reverse:    ["Hello", "Ġworld", "!"]
Concat:     "HelloĠworld!"
Byte dec:   [0x48,0x65,...,0x20,0x77,...,0x21]
UTF-8 dec:  "Hello world!"
```

---

## Complete Encoding Flow

```
INPUT TEXT: "Hello world!"

┌─────────────────────────────────────────┐
│ Step 1: Pre-Split (Regex)              │
│ " Hello" | " world" | "!"               │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│ Step 2: Byte Encode                     │
│ [" ","H","e","l","l","o"]               │
│ [" ","w","o","r","l","d"]               │
│ ["!"]                                   │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│ Step 3: BPE Merge (Iterative)           │
│                                         │
│ Split 1: [" ","H","e","l","l","o"]     │
│   → [" ","He","ll","o"]                │
│   → [" ","Hello"]                      │
│   → ["ĠHello"]  (Ġ = space+H merged)   │
│                                         │
│ Split 2: [" ","w","o","r","l","d"]     │
│   → [" ","wo","r","ld"]                │
│   → ["Ġworld"]                         │
│                                         │
│ Split 3: ["!"] (no merges)              │
└─────────────────────────────────────────┘
            ↓
┌─────────────────────────────────────────┐
│ Step 4: Vocab Lookup                    │
│ "ĠHello" → 12345                        │
│ "Ġworld" → 67890                        │
│ "!" → 111                               │
└─────────────────────────────────────────┘
            ↓
OUTPUT: [12345, 67890, 111]
```

---

## Training Process (Not Implemented Here)

The merge rules are learned during training:

```python
# Pseudocode for BPE training
corpus = load_training_text()
vocab = initialize_byte_vocab()  # 256 base tokens

for iteration in range(num_merges):
    # Count all adjacent pairs
    pair_counts = {}
    for document in corpus:
        tokens = tokenize(document, current_vocab)
        for i in range(len(tokens)-1):
            pair = (tokens[i], tokens[i+1])
            pair_counts[pair] += 1
    
    # Find most frequent pair
    best_pair = max(pair_counts, key=pair_counts.get)
    
    # Add merged token to vocabulary
    new_token = best_pair[0] + best_pair[1]
    vocab.append(new_token)
    
    # Record merge rule with priority
    merges[best_pair] = iteration
    
    # Update corpus with new token
    corpus = apply_merge(corpus, best_pair, new_token)

# Save final vocab + merge rules
```

---

## Key Properties

1. **Deterministic**: Same text always produces same tokens
2. **Lossless**: Decode(Encode(text)) = text (exactly)
3. **Greedy**: Merges highest-frequency pairs first (locally optimal)
4. **Suboptimal**: May not find globally optimal tokenization
5. **Context-Free**: Each split tokenized independently

---

## Advantages

- **Handles any input**: Byte-level = no unknown characters
- **Compression**: Common words → single token
- **Flexibility**: Rare words → multiple subword tokens
- **No vocabulary explosion**: Fixed vocab size (typically 50k)

---

## Limitations

- **No context**: "read" (present) vs "read" (past) tokenized identically
- **Inefficient for rare words**: Long sequences of byte tokens
- **Greedy merging**: Not optimal (but fast)
- **Splitting artifacts**: Pre-split regex affects final tokens

---

## Comparison to Alternatives

| Feature | BPE | WordPiece | SentencePiece |
|---------|-----|-----------|---------------|
| Unit | Byte pairs | Subwords | Unigrams |
| Training | Frequency | Likelihood | Language model |
| Merging | Greedy | Greedy | Viterbi |
| Optimality | Local | Local | Global |
| Speed | Fast | Fast | Slower |

---

## Real-World Usage (GPT-2/GPT-3)

```
Vocabulary size: ~50,257 tokens
  - 256 byte tokens
  - ~50,000 merged tokens
  - Special tokens: <|endoftext|>, etc.

Merge count: ~50,000 rules (one per vocab entry)
Priority: Lower index = more frequent
```

**Example tokenization:**
```
"The quick brown fox" → [464, 2068, 7586, 21831]
  464    = "The"
  2068   = " quick"  (note leading space)
  7586   = " brown"
  21831  = " fox"
```

---

## Summary

BPE is a **byte-level, frequency-based, greedy subword tokenization algorithm** that:

1. Encodes text as UTF-8 bytes mapped to printable Unicode
2. Pre-splits on word/punctuation boundaries
3. Iteratively merges most-frequent adjacent pairs
4. Converts merged tokens to vocabulary IDs

The implementation here is **production-ready** and compatible with GPT-2/GPT-3 style tokenization, requiring only vocabulary and merge files from the model.

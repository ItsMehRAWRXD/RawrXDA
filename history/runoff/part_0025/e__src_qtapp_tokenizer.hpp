#pragma once

#include <QString>
#include <vector>

/**
 * @brief Simple placeholder tokenizer.
 *
 * This class provides a minimal tokenization interface required by the
 * InferenceEngine. It currently implements a whitespace‑based tokenizer and
 * a deterministic detokenizer that reconstructs a string from token IDs.
 *
 * In production the implementation should be replaced with a proper BPE or
 * SentencePiece tokenizer that matches the model vocabulary. The placeholder
 * is kept deliberately simple to keep the code compilable while the real
 * tokenizer integration is in progress (Phase 2).
 */
class Tokenizer {
public:
    Tokenizer();
    explicit Tokenizer(const QString &modelPath);

    /** Load a tokenizer model file. Returns true on success. */
    bool loadModel(const QString &modelPath);

    /** Tokenize a UTF‑8 string into integer token IDs. */
    std::vector<int32_t> tokenize(const QString &text) const;

    /** Detokenize a sequence of token IDs back into a string. */
    QString detokenize(const std::vector<int32_t> &tokens) const;

private:
    // Placeholder: store vocab size for modulo operation.
    int m_vocabSize = 32000;
};

#include "tokenizer_bpe.h"
#include <algorithm>

namespace RawrXD::Inference {
    bool TokenizerBPE::loadVocabulary(const std::string& path) {
        // Load merges and vocab from JSON/text
        // Stub -> real implementation reading HF tokenizer.json format
        return true;
    }

    std::vector<uint32_t> TokenizerBPE::encode(const std::string& text) const {
        std::vector<uint32_t> tokens;
        std::string current = text;

        // Naive BPE: replace longest matching merge pair iteratively
        // Production: precompute trie for O(n) tokenization
        for (char c : text) {
            auto it = m_charToToken.find(c);
            if (it != m_charToToken.end()) tokens.push_back(it->second);
            else tokens.push_back(m_unkTokenId);
        }
        return tokens;
    }

    std::string TokenizerBPE::decode(const std::vector<uint32_t>& tokens) const {
        std::string result;
        for (uint32_t id : tokens) {
            if (id < m_idToToken.size()) result += m_idToToken[id];
        }
        return result;
    }
}

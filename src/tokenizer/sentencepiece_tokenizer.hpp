#pragma once

#include "tokenizer_base.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tokenizer
{

// SentencePiece/Unigram tokenizer.
// Note: For now this uses a lightweight on-disk format reader compatible with the
// simplified `.model` loader already used in this repo, plus GGUF token-list fallback.
class SentencePieceTokenizer final : public TokenizerBase
{
  public:
    SentencePieceTokenizer();

    bool load_from_file(const std::string& model_path);
    bool load_from_blob(const uint8_t* data, size_t size);
    bool load_from_gguf(const std::string& gguf_path);
    bool load_from_token_list(const std::vector<std::string>& tokens, const std::vector<float>& scores = {},
                              const std::vector<int32_t>& types = {});

    Encoding encode(const std::string& text, bool add_special_tokens = true) const override;
    std::string decode(const std::vector<TokenId>& tokens, const DecodeOptions& options = {}) const override;

    TokenId token_to_id(const std::string& token) const override;
    std::string id_to_token(TokenId id) const override;
    size_t vocab_size() const override { return m_vocab.size(); }
    const TokenInfo& get_token_info(TokenId id) const override;

    TokenId pad_token_id() const override { return m_pad; }
    TokenId bos_token_id() const override { return m_bos; }
    TokenId eos_token_id() const override { return m_eos; }
    TokenId unk_token_id() const override { return m_unk; }
    TokenId sep_token_id() const override { return INVALID_TOKEN; }
    TokenId cls_token_id() const override { return INVALID_TOKEN; }
    TokenId mask_token_id() const override { return INVALID_TOKEN; }

    bool is_special_token(TokenId id) const override;
    bool is_control_token(TokenId id) const override;
    bool is_byte_token(TokenId) const override { return false; }

    std::string model_type() const override { return "sentencepiece"; }

    void set_special_tokens(TokenId pad, TokenId bos, TokenId eos, TokenId unk);
    void set_add_bos_token(bool v) { m_add_bos = v; }
    void set_add_eos_token(bool v) { m_add_eos = v; }

  private:
    struct TrieNode
    {
        std::unordered_map<char, std::unique_ptr<TrieNode>> next;
        TokenId token_id = INVALID_TOKEN;
    };

    std::vector<TokenInfo> m_vocab;
    std::unordered_map<std::string, TokenId> m_token_to_id;

    std::unique_ptr<TrieNode> m_trie;

    TokenId m_pad = INVALID_TOKEN;
    TokenId m_bos = INVALID_TOKEN;
    TokenId m_eos = INVALID_TOKEN;
    TokenId m_unk = INVALID_TOKEN;

    bool m_add_bos = true;
    bool m_add_eos = false;

    void build_trie_();
    std::vector<TokenId> encode_greedy_(const std::string& text) const;
};

}  // namespace tokenizer

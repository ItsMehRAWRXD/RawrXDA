#pragma once

#include "tokenizer_base.hpp"
#include <map>

namespace tokenizer
{

class BPETokenizer final : public TokenizerBase
{
  public:
    BPETokenizer();

    bool load_from_file(const std::string& vocab_path, const std::string& merges_path);
    bool load_from_gguf(const std::string& gguf_path);
    bool load_vocab(const std::vector<std::string>& vocab_lines, const std::vector<std::string>& merges_lines);

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
    TokenId sep_token_id() const override { return m_sep; }
    TokenId cls_token_id() const override { return m_cls; }
    TokenId mask_token_id() const override { return m_mask; }

    bool is_special_token(TokenId id) const override;
    bool is_control_token(TokenId id) const override;
    bool is_byte_token(TokenId id) const override;

    std::string model_type() const override { return "bpe"; }

    void set_special_tokens(TokenId pad, TokenId bos, TokenId eos, TokenId unk, TokenId sep = INVALID_TOKEN,
                            TokenId cls = INVALID_TOKEN, TokenId mask = INVALID_TOKEN);

    void set_add_prefix_space(bool v) { m_add_prefix_space = v; }
    void set_add_bos_token(bool v) { m_add_bos = v; }
    void set_add_eos_token(bool v) { m_add_eos = v; }

  private:
    std::vector<TokenInfo> m_vocab;
    std::unordered_map<std::string, TokenId> m_token_to_id;

    // BPE merges: pair -> rank (lower is better).
    std::map<std::pair<std::string, std::string>, int> m_merges;

    TokenId m_pad = INVALID_TOKEN;
    TokenId m_bos = INVALID_TOKEN;
    TokenId m_eos = INVALID_TOKEN;
    TokenId m_unk = INVALID_TOKEN;
    TokenId m_sep = INVALID_TOKEN;
    TokenId m_cls = INVALID_TOKEN;
    TokenId m_mask = INVALID_TOKEN;

    bool m_add_prefix_space = true;
    bool m_add_bos = true;
    bool m_add_eos = true;

    std::unordered_map<uint8_t, std::string> m_byte_enc;
    std::unordered_map<std::string, uint8_t> m_byte_dec;

    void refresh_specials_();
    std::vector<std::string> pretokenize_(const std::string& text) const;
    std::vector<std::string> bpe_(const std::string& token) const;
};

}  // namespace tokenizer

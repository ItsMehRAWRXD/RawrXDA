#pragma once

#include "bpe_tokenizer.hpp"
#include "tokenizer_base.hpp"

#include <string>
#include <vector>

namespace tokenizer
{

// Minimal HuggingFace `tokenizer.json` loader (zero external deps).
// Currently supports common BPE-based tokenizers:
// - model.type: "BPE"
// - model.type: "ByteLevelBPE"
//
// It reuses `BPETokenizer` for the actual encode/decode logic.
class HuggingFaceTokenizer final : public TokenizerBase
{
  public:
    HuggingFaceTokenizer();

    bool load_from_file(const std::string& tokenizer_json_path);
    bool load_from_json_string(const std::string& jsonText);

    void set_special_tokens(TokenId pad, TokenId bos, TokenId eos, TokenId unk);
    void set_add_bos_token(bool v) { m_bpe.set_add_bos_token(v); }
    void set_add_eos_token(bool v) { m_bpe.set_add_eos_token(v); }
    void set_add_prefix_space(bool v) { m_bpe.set_add_prefix_space(v); }

    Encoding encode(const std::string& text, bool add_special_tokens = true) const override;
    std::string decode(const std::vector<TokenId>& tokens, const DecodeOptions& options = {}) const override;

    TokenId token_to_id(const std::string& token) const override;
    std::string id_to_token(TokenId id) const override;
    size_t vocab_size() const override;
    const TokenInfo& get_token_info(TokenId id) const override;

    TokenId pad_token_id() const override;
    TokenId bos_token_id() const override;
    TokenId eos_token_id() const override;
    TokenId unk_token_id() const override;
    TokenId sep_token_id() const override;
    TokenId cls_token_id() const override;
    TokenId mask_token_id() const override;

    bool is_special_token(TokenId id) const override;
    bool is_control_token(TokenId id) const override;
    bool is_byte_token(TokenId id) const override;

    std::string model_type() const override { return "huggingface"; }

  private:
    BPETokenizer m_bpe;
};

}  // namespace tokenizer

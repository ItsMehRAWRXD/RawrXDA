#pragma once

#include "tokenizer_base.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tokenizer
{

class TiktokenTokenizer final : public TokenizerBase
{
  public:
    TiktokenTokenizer();
    ~TiktokenTokenizer() override;

    bool load_from_file(const std::string& path);
    bool load_from_openai_json(const std::string& path);
    bool load_from_vocab(const std::unordered_map<std::string, int>& vocab);

    Encoding encode(const std::string& text, bool add_special_tokens = true) const override;
    std::string decode(const std::vector<TokenId>& tokens, const DecodeOptions& options = {}) const override;

    TokenId token_to_id(const std::string& token) const override;
    std::string id_to_token(TokenId id) const override;
    size_t vocab_size() const override;
    const TokenInfo& get_token_info(TokenId id) const override;

    TokenId pad_token_id() const override { return pad_id_; }
    TokenId bos_token_id() const override { return bos_id_; }
    TokenId eos_token_id() const override { return eos_id_; }
    TokenId unk_token_id() const override { return unk_id_; }
    TokenId sep_token_id() const override { return INVALID_TOKEN; }
    TokenId cls_token_id() const override { return INVALID_TOKEN; }
    TokenId mask_token_id() const override { return INVALID_TOKEN; }

    bool is_special_token(TokenId id) const override;
    bool is_control_token(TokenId id) const override;
    bool is_byte_token(TokenId id) const override;

    std::string model_type() const override { return "tiktoken"; }

  private:
    std::unordered_map<std::string, TokenId> token_to_id_;
    std::vector<std::string> id_to_token_;
    std::unordered_map<std::pair<std::string, std::string>, int, PairHash<std::string, std::string>> merge_ranks_;
    std::unordered_map<std::string, TokenId> special_tokens_;

    TokenId pad_id_ = INVALID_TOKEN;
    TokenId bos_id_ = INVALID_TOKEN;
    TokenId eos_id_ = INVALID_TOKEN;
    TokenId unk_id_ = INVALID_TOKEN;

    std::vector<TokenInfo> token_info_;

    std::string regex_pattern_;

    std::vector<std::pair<size_t, size_t>> byte_pair_merge(const std::string& text) const;
    std::string base64_decode(const std::string& encoded) const;
};

} // namespace tokenizer

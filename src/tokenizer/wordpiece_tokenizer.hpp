#pragma once

#include "tokenizer_base.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace tokenizer
{

class WordPieceTokenizer final : public TokenizerBase
{
  public:
    WordPieceTokenizer();
    ~WordPieceTokenizer() override;

    bool load_from_file(const std::string& vocab_path);
    bool load_from_files(const std::string& vocab_path, const std::string& merges_path);

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
    TokenId sep_token_id() const override { return sep_id_; }
    TokenId cls_token_id() const override { return cls_id_; }
    TokenId mask_token_id() const override { return mask_id_; }

    bool is_special_token(TokenId id) const override;
    bool is_control_token(TokenId id) const override;
    bool is_byte_token(TokenId id) const override;

    std::string model_type() const override { return "WordPiece"; }

    void set_max_input_chars_per_word(int max_chars) { max_input_chars_per_word_ = max_chars; }
    void set_unk_token(const std::string& token) { unk_token_ = token; }

  private:
    std::unordered_map<std::string, TokenId> vocab_;
    std::vector<std::string> id_to_token_;
    std::vector<TokenInfo> token_info_;

    TokenId pad_id_ = 0;
    TokenId unk_id_ = 100;
    TokenId cls_id_ = 101;
    TokenId sep_id_ = 102;
    TokenId mask_id_ = 103;
    TokenId bos_id_ = 101;
    TokenId eos_id_ = 102;

    std::string unk_token_ = "[UNK]";
    int max_input_chars_per_word_ = 100;

    bool do_lower_case_ = true;

    std::vector<std::string> tokenize(const std::string& text) const;
    std::vector<TokenId> wordpiece_tokenize(const std::string& word) const;
    std::string clean_text(const std::string& text) const;
};

} // namespace tokenizer

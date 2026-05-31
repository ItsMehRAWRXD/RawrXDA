#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tokenizer
{

using TokenId = int32_t;
constexpr TokenId INVALID_TOKEN = -1;

template <typename A, typename B>
struct PairHash
{
  size_t operator()(const std::pair<A, B>& p) const noexcept
  {
    const size_t h1 = std::hash<A>{}(p.first);
    const size_t h2 = std::hash<B>{}(p.second);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
  }
};

struct TokenInfo
{
    TokenId id = INVALID_TOKEN;
    std::string text;
    float score = 0.0f;
    uint32_t type = 0;
};

struct Encoding
{
    std::vector<TokenId> token_ids;
    std::vector<uint32_t> attention_mask;
    std::vector<std::pair<uint32_t, uint32_t>> offsets;

    size_t size() const { return token_ids.size(); }
    bool empty() const { return token_ids.empty(); }
};

struct DecodeOptions
{
    bool skip_special_tokens = true;
    bool clean_up_tokenization = true;
    bool spaces_between_tokens = false;
};

class TokenizerBase
{
  public:
    virtual ~TokenizerBase() = default;

    virtual Encoding encode(const std::string& text, bool add_special_tokens = true) const = 0;
    virtual std::string decode(const std::vector<TokenId>& tokens, const DecodeOptions& options = {}) const = 0;

    virtual std::vector<Encoding> encode_batch(const std::vector<std::string>& texts,
                                               bool add_special_tokens = true) const;
    virtual std::vector<std::string> decode_batch(const std::vector<std::vector<TokenId>>& batch,
                                                  const DecodeOptions& options = {}) const;

    virtual TokenId token_to_id(const std::string& token) const = 0;
    virtual std::string id_to_token(TokenId id) const = 0;
    virtual size_t vocab_size() const = 0;
    virtual const TokenInfo& get_token_info(TokenId id) const = 0;

    virtual TokenId pad_token_id() const = 0;
    virtual TokenId bos_token_id() const = 0;
    virtual TokenId eos_token_id() const = 0;
    virtual TokenId unk_token_id() const = 0;
    virtual TokenId sep_token_id() const = 0;
    virtual TokenId cls_token_id() const = 0;
    virtual TokenId mask_token_id() const = 0;

    virtual bool is_special_token(TokenId id) const = 0;
    virtual bool is_control_token(TokenId id) const = 0;
    virtual bool is_byte_token(TokenId id) const = 0;

    virtual std::string model_type() const = 0;

  protected:
    static std::vector<uint32_t> utf8_to_codepoints(const std::string& text);
    static std::string codepoints_to_utf8(const std::vector<uint32_t>& codepoints);

    // GPT-2 byte<->unicode mapping helpers.
    static std::unordered_map<uint8_t, std::string> build_byte_encoder();
    static std::unordered_map<std::string, uint8_t> build_byte_decoder(
        const std::unordered_map<uint8_t, std::string>& enc);
};

}  // namespace tokenizer

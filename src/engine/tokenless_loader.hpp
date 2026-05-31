#pragma once

#include "../tokenizer/bpe_tokenizer.hpp"
#include "../tokenizer/huggingface_tokenizer.hpp"
#include "../tokenizer/sentencepiece_tokenizer.hpp"
#include "../tokenizer/tokenizer_base.hpp"
#include "cpu_engine.hpp"


#include <cstdint>
#include <memory>
#include <string>

namespace engine
{

enum class TokenizerType
{
    Unknown,
    BPE,
    SentencePiece,
    HuggingFace,
    Tiktoken,
    Custom
};

struct TokenizerConfig
{
    TokenizerType type = TokenizerType::Unknown;
    std::string vocab_path;
    std::string merges_path;
    std::string model_path;
    std::string tokenizer_json;

    int32_t bos_token_id = 1;
    int32_t eos_token_id = 2;
    int32_t unk_token_id = 0;
    int32_t pad_token_id = -1;

    bool add_bos = true;
    bool add_eos = false;
    bool add_prefix_space = true;
};

class TokenlessLoader
{
  public:
    TokenlessLoader() = default;

    bool load_weights_from_gguf(const std::string& path, CPUEngine* engine);
    bool load_tokenizer(const TokenizerConfig& config, CPUEngine* engine);
    bool load_from_files(const std::string& weights_path, const TokenizerConfig& tokenizer_config, CPUEngine* engine);

    ::tokenizer::TokenizerBase* get_tokenizer() { return m_tokenizer.get(); }
    const ::tokenizer::TokenizerBase* get_tokenizer() const { return m_tokenizer.get(); }

    std::vector<int32_t> encode(const std::string& text, bool add_special = true) const;
    std::string decode(const std::vector<int32_t>& tokens, bool skip_special = true) const;

    int32_t bos_token_id() const;
    int32_t eos_token_id() const;
    int32_t unk_token_id() const;

    const std::string& last_error() const { return m_last_error; }

  private:
    std::shared_ptr<::tokenizer::TokenizerBase> m_tokenizer;
    TokenizerConfig m_cfg;
    std::string m_last_error;
    std::string m_last_gguf_path;

    bool load_bpe_tokenizer_(const TokenizerConfig& config);
    bool load_sentencepiece_tokenizer_(const TokenizerConfig& config);
    bool load_huggingface_tokenizer_(const TokenizerConfig& config);
    bool detect_tokenizer_from_gguf_(const std::string& path, TokenizerConfig& config);
};

}  // namespace engine

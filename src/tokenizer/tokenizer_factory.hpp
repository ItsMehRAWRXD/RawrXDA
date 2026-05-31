#pragma once

#include "tokenizer_base.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace tokenizer
{

enum class TokenizerKind
{
    Unknown,
    SentencePiece,
    HuggingFace
};

class TokenizerFactory
{
  public:
    static TokenizerKind detectFromPath(const std::string& path);
    static TokenizerKind detectFromBlob(const uint8_t* data, size_t size);

    static std::unique_ptr<TokenizerBase> load(const std::string& path);
    static std::unique_ptr<TokenizerBase> loadFromBlob(const uint8_t* data, size_t size, TokenizerKind kind);
};

}  // namespace tokenizer

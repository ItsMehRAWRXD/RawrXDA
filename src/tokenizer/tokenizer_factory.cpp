#include "tokenizer_factory.hpp"

#include "huggingface_tokenizer.hpp"
#include "sentencepiece_tokenizer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace tokenizer
{
namespace
{

static bool endsWith_(const std::string& s, const std::string& suffix)
{
    if (suffix.size() > s.size())
        return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

static bool readFileToBytes_(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string s = ss.str();
    out.assign(s.begin(), s.end());
    return true;
}

}  // namespace

TokenizerKind TokenizerFactory::detectFromPath(const std::string& path)
{
    if (endsWith_(path, "tokenizer.json"))
        return TokenizerKind::HuggingFace;
    if (endsWith_(path, ".model"))
        return TokenizerKind::SentencePiece;
    return TokenizerKind::Unknown;
}

TokenizerKind TokenizerFactory::detectFromBlob(const uint8_t* data, size_t size)
{
    if (!data || size == 0)
        return TokenizerKind::Unknown;

    if (data[0] == (uint8_t)'{')
    {
        const size_t n = std::min<size_t>(size, 2048);
        std::string head((const char*)data, (const char*)data + n);
        if (head.find("\"model\"") != std::string::npos && head.find("\"vocab\"") != std::string::npos)
            return TokenizerKind::HuggingFace;
    }

    return TokenizerKind::Unknown;
}

std::unique_ptr<TokenizerBase> TokenizerFactory::load(const std::string& path)
{
    const auto kind = detectFromPath(path);
    std::vector<uint8_t> b;
    if (!readFileToBytes_(path, b))
        return nullptr;
    return loadFromBlob(b.data(), b.size(), kind);
}

std::unique_ptr<TokenizerBase> TokenizerFactory::loadFromBlob(const uint8_t* data, size_t size, TokenizerKind kind)
{
    if (kind == TokenizerKind::Unknown)
        kind = detectFromBlob(data, size);

    switch (kind)
    {
        case TokenizerKind::HuggingFace:
        {
            auto t = std::make_unique<HuggingFaceTokenizer>();
            const std::string jsonText((const char*)data, (const char*)data + size);
            if (!t->load_from_json_string(jsonText))
                return nullptr;
            return t;
        }
        case TokenizerKind::SentencePiece:
        {
            auto t = std::make_unique<SentencePieceTokenizer>();
            if (!t->load_from_blob(data, size))
                return nullptr;
            return t;
        }
        default:
            break;
    }
    return nullptr;
}

}  // namespace tokenizer

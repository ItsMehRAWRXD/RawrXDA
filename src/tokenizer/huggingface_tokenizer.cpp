#include "huggingface_tokenizer.hpp"

#include "../core/rawrxd_json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace tokenizer
{
namespace
{

static bool readFileToString_(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static bool isObj_(const RawrXD::JsonValue& v)
{
    return v.is_object();
}
static bool isArr_(const RawrXD::JsonValue& v)
{
    return v.is_array();
}
static bool isStr_(const RawrXD::JsonValue& v)
{
    return v.is_string();
}
static bool isNum_(const RawrXD::JsonValue& v)
{
    return v.is_number();
}

static const RawrXD::JsonObject* asObj_(const RawrXD::JsonValue& v)
{
    if (!v.is_object())
        return nullptr;
    return &v.get_object();
}

static std::string asStrOr_(const RawrXD::JsonValue& v, const char* def = "")
{
    if (v.is_string())
        return v.get_string();
    return def ? std::string(def) : std::string();
}

}  // namespace

HuggingFaceTokenizer::HuggingFaceTokenizer() = default;

void HuggingFaceTokenizer::set_special_tokens(TokenId pad, TokenId bos, TokenId eos, TokenId unk)
{
    m_bpe.set_special_tokens(pad, bos, eos, unk);
}

bool HuggingFaceTokenizer::load_from_file(const std::string& tokenizer_json_path)
{
    std::string raw;
    if (!readFileToString_(tokenizer_json_path, raw))
        return false;
    return load_from_json_string(raw);
}

bool HuggingFaceTokenizer::load_from_json_string(const std::string& jsonText)
{
    RawrXD::JsonValue root;
    try
    {
        root = RawrXD::JsonValue::parse(jsonText);
    }
    catch (...)
    {
        return false;
    }

    if (!root.is_object())
        return false;

    const auto* rootObj = asObj_(root);
    if (!rootObj)
        return false;

    auto itModel = rootObj->find("model");
    if (itModel == rootObj->end() || !isObj_(itModel->second))
        return false;
    const RawrXD::JsonObject& modelObj = itModel->second.get_object();

    const std::string modelType = [&]() -> std::string
    {
        auto it = modelObj.find("type");
        if (it == modelObj.end())
            return {};
        return asStrOr_(it->second, "");
    }();

    if (!(modelType == "BPE" || modelType == "ByteLevelBPE"))
        return false;

    auto itVocab = modelObj.find("vocab");
    if (itVocab == modelObj.end() || !isObj_(itVocab->second))
        return false;
    const RawrXD::JsonObject& vocabObj = itVocab->second.get_object();

    int32_t maxId = -1;
    for (const auto& kv : vocabObj)
    {
        if (isNum_(kv.second))
            maxId = std::max(maxId, (int32_t)kv.second.get_number());
    }
    if (maxId < 0 || maxId > 10'000'000)
        return false;

    std::vector<std::string> vocabLines((size_t)maxId + 1u);
    std::vector<uint8_t> set((size_t)maxId + 1u, 0);
    for (const auto& kv : vocabObj)
    {
        if (!isNum_(kv.second))
            continue;
        const int32_t id = (int32_t)kv.second.get_number();
        if (id < 0 || id > maxId)
            continue;
        vocabLines[(size_t)id] = kv.first;
        set[(size_t)id] = 1;
    }

    auto itAdded = rootObj->find("added_tokens");
    if (itAdded != rootObj->end() && isArr_(itAdded->second))
    {
        const RawrXD::JsonArray& added = itAdded->second.get_array();
        for (const auto& item : added)
        {
            if (!item.is_object())
                continue;
            const RawrXD::JsonObject& o = item.get_object();
            auto itContent = o.find("content");
            auto itId = o.find("id");
            if (itContent == o.end() || itId == o.end())
                continue;
            if (!isStr_(itContent->second) || !isNum_(itId->second))
                continue;
            const std::string content = itContent->second.get_string();
            const int32_t id = (int32_t)itId->second.get_number();
            if (content.empty() || id < 0)
                continue;
            if ((size_t)id >= vocabLines.size())
            {
                vocabLines.resize((size_t)id + 1u);
                set.resize((size_t)id + 1u, 0);
            }
            vocabLines[(size_t)id] = content;
            set[(size_t)id] = 1;
        }
    }

    for (size_t i = 0; i < vocabLines.size(); ++i)
    {
        if (!set[i])
            vocabLines[i] = "<rawrxd_gap_" + std::to_string(i) + ">";
    }

    std::vector<std::string> mergesLines;
    auto itMerges = modelObj.find("merges");
    if (itMerges != modelObj.end() && isArr_(itMerges->second))
    {
        const RawrXD::JsonArray& merges = itMerges->second.get_array();
        mergesLines.reserve(merges.size());
        for (const auto& m : merges)
        {
            if (m.is_string())
            {
                mergesLines.push_back(m.get_string());
            }
            else if (m.is_array())
            {
                const auto& a = m.get_array();
                if (a.size() >= 2 && a[0].is_string() && a[1].is_string())
                    mergesLines.push_back(a[0].get_string() + " " + a[1].get_string());
            }
        }
    }

    if (!m_bpe.load_vocab(vocabLines, mergesLines))
        return false;

    bool addPrefixSpace = true;
    auto itPretok = rootObj->find("pre_tokenizer");
    if (itPretok != rootObj->end() && itPretok->second.is_object())
    {
        const auto& o = itPretok->second.get_object();
        auto itType = o.find("type");
        if (itType != o.end() && itType->second.is_string())
        {
            const std::string pt = itType->second.get_string();
            if (pt == "ByteLevel")
            {
                auto itAdd = o.find("add_prefix_space");
                if (itAdd != o.end() && itAdd->second.is_bool())
                    addPrefixSpace = itAdd->second.get_bool();
            }
        }
    }
    m_bpe.set_add_prefix_space(addPrefixSpace);

    return true;
}

Encoding HuggingFaceTokenizer::encode(const std::string& text, bool add_special_tokens) const
{
    return m_bpe.encode(text, add_special_tokens);
}

std::string HuggingFaceTokenizer::decode(const std::vector<TokenId>& tokens, const DecodeOptions& options) const
{
    return m_bpe.decode(tokens, options);
}

TokenId HuggingFaceTokenizer::token_to_id(const std::string& token) const
{
    return m_bpe.token_to_id(token);
}
std::string HuggingFaceTokenizer::id_to_token(TokenId id) const
{
    return m_bpe.id_to_token(id);
}
size_t HuggingFaceTokenizer::vocab_size() const
{
    return m_bpe.vocab_size();
}
const TokenInfo& HuggingFaceTokenizer::get_token_info(TokenId id) const
{
    return m_bpe.get_token_info(id);
}

TokenId HuggingFaceTokenizer::pad_token_id() const
{
    return m_bpe.pad_token_id();
}
TokenId HuggingFaceTokenizer::bos_token_id() const
{
    return m_bpe.bos_token_id();
}
TokenId HuggingFaceTokenizer::eos_token_id() const
{
    return m_bpe.eos_token_id();
}
TokenId HuggingFaceTokenizer::unk_token_id() const
{
    return m_bpe.unk_token_id();
}
TokenId HuggingFaceTokenizer::sep_token_id() const
{
    return m_bpe.sep_token_id();
}
TokenId HuggingFaceTokenizer::cls_token_id() const
{
    return m_bpe.cls_token_id();
}
TokenId HuggingFaceTokenizer::mask_token_id() const
{
    return m_bpe.mask_token_id();
}

bool HuggingFaceTokenizer::is_special_token(TokenId id) const
{
    return m_bpe.is_special_token(id);
}
bool HuggingFaceTokenizer::is_control_token(TokenId id) const
{
    return m_bpe.is_control_token(id);
}
bool HuggingFaceTokenizer::is_byte_token(TokenId id) const
{
    return m_bpe.is_byte_token(id);
}

}  // namespace tokenizer

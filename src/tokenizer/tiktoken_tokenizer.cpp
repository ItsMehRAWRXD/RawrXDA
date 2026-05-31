#include "tiktoken_tokenizer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>

namespace tokenizer
{

using json = nlohmann::json;

namespace
{

static std::vector<std::string> split_for_tiktoken(const std::string& text)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < text.size())
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (std::isspace(c))
        {
            size_t j = i + 1;
            while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j])))
            {
                ++j;
            }
            out.push_back(text.substr(i, j - i));
            i = j;
            continue;
        }
        if (std::isalnum(c) || c == '_')
        {
            size_t j = i + 1;
            while (j < text.size())
            {
                const unsigned char cj = static_cast<unsigned char>(text[j]);
                if (!(std::isalnum(cj) || cj == '_'))
                {
                    break;
                }
                ++j;
            }
            out.push_back(text.substr(i, j - i));
            i = j;
            continue;
        }
        out.push_back(text.substr(i, 1));
        ++i;
    }
    return out;
}

} // namespace

TiktokenTokenizer::TiktokenTokenizer()
{
    regex_pattern_ = "simple-split";
}

TiktokenTokenizer::~TiktokenTokenizer() = default;

std::string TiktokenTokenizer::base64_decode(const std::string& encoded) const
{
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<int> table(256, -1);
    for (int i = 0; i < 64; ++i)
    {
        table[static_cast<size_t>(base64_chars[static_cast<size_t>(i)])] = i;
    }

    std::string decoded;
    int val = 0;
    int valb = -8;
    for (unsigned char c : encoded)
    {
        if (table[c] == -1)
        {
            break;
        }
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0)
        {
            decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return decoded;
}

bool TiktokenTokenizer::load_from_vocab(const std::unordered_map<std::string, int>& vocab)
{
    token_to_id_.clear();
    id_to_token_.clear();
    merge_ranks_.clear();
    special_tokens_.clear();
    token_info_.clear();

    std::vector<std::pair<std::string, int>> ordered(vocab.begin(), vocab.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

    for (const auto& [tok, rank] : ordered)
    {
        const TokenId id = static_cast<TokenId>(id_to_token_.size());
        token_to_id_[tok] = id;
        id_to_token_.push_back(tok);
        token_info_.push_back(TokenInfo{id, tok, static_cast<float>(rank), 0});
    }

    auto unk = token_to_id_.find("<|unk|>");
    if (unk != token_to_id_.end())
    {
        unk_id_ = unk->second;
    }

    return !id_to_token_.empty();
}

bool TiktokenTokenizer::load_from_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    token_to_id_.clear();
    id_to_token_.clear();
    token_info_.clear();

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        const size_t sp = line.find(' ');
        if (sp == std::string::npos)
        {
            continue;
        }

        const std::string encoded = line.substr(0, sp);
        const std::string token = base64_decode(encoded);
        const int rank = std::stoi(line.substr(sp + 1));

        if (!token_to_id_.count(token))
        {
            const TokenId id = static_cast<TokenId>(id_to_token_.size());
            token_to_id_[token] = id;
            id_to_token_.push_back(token);
            token_info_.push_back(TokenInfo{id, token, static_cast<float>(rank), 0});
        }
    }

    return !id_to_token_.empty();
}

bool TiktokenTokenizer::load_from_openai_json(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    try
    {
        std::ostringstream ss;
        ss << file.rdbuf();
        json j = json::parse(ss.str());

        std::unordered_map<std::string, int> vocab;
        if (j.contains("model") && j["model"].contains("vocab") && j["model"]["vocab"].is_object())
        {
            for (auto it = j["model"]["vocab"].begin(); it != j["model"]["vocab"].end(); ++it)
            {
                vocab[it.key()] = it.value().get<int>();
            }
        }

        if (!load_from_vocab(vocab))
        {
            return false;
        }

        if (j.contains("model") && j["model"].contains("merges") && j["model"]["merges"].is_array())
        {
            int rank = 0;
            for (const auto& m : j["model"]["merges"])
            {
                if (m.is_array() && m.size() >= 2)
                {
                    const std::string left = m[static_cast<size_t>(0)].get<std::string>();
                    const std::string right = m[static_cast<size_t>(1)].get<std::string>();
                    merge_ranks_[std::make_pair(left, right)] = rank++;
                }
                else if (m.is_string())
                {
                    std::string s = m.get<std::string>();
                    size_t sp = s.find(' ');
                    if (sp != std::string::npos)
                    {
                        merge_ranks_[std::make_pair(s.substr(0, sp), s.substr(sp + 1))] = rank++;
                    }
                }
            }
        }

        if (j.contains("added_tokens") && j["added_tokens"].is_array())
        {
            for (const auto& tok : j["added_tokens"])
            {
                const std::string content = tok.value("content", "");
                const TokenId id = tok.value("id", INVALID_TOKEN);
                if (content.empty() || id == INVALID_TOKEN)
                {
                    continue;
                }
                special_tokens_[content] = id;
                if (content == "<|endoftext|>")
                {
                    eos_id_ = id;
                }
                else if (content == "<|startoftext|>")
                {
                    bos_id_ = id;
                }
                else if (content == "<|pad|>")
                {
                    pad_id_ = id;
                }
            }
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::pair<size_t, size_t>> TiktokenTokenizer::byte_pair_merge(const std::string& text) const
{
    std::vector<std::string> parts;
    auto cps = utf8_to_codepoints(text);
    parts.reserve(cps.size());
    for (uint32_t cp : cps)
    {
        parts.push_back(codepoints_to_utf8({cp}));
    }

    while (parts.size() > 1)
    {
        int best_rank = std::numeric_limits<int>::max();
        size_t best_i = static_cast<size_t>(-1);

        for (size_t i = 0; i + 1 < parts.size(); ++i)
        {
            auto it = merge_ranks_.find({parts[i], parts[i + 1]});
            if (it != merge_ranks_.end() && it->second < best_rank)
            {
                best_rank = it->second;
                best_i = i;
            }
        }

        if (best_i == static_cast<size_t>(-1))
        {
            break;
        }

        parts[best_i] += parts[best_i + 1];
        parts.erase(parts.begin() + static_cast<std::ptrdiff_t>(best_i + 1));
    }

    std::vector<std::pair<size_t, size_t>> ranges;
    size_t cursor = 0;
    for (const auto& p : parts)
    {
        ranges.push_back({cursor, cursor + p.size()});
        cursor += p.size();
    }
    return ranges;
}

Encoding TiktokenTokenizer::encode(const std::string& text, bool add_special_tokens) const
{
    Encoding out;

    if (add_special_tokens && bos_id_ != INVALID_TOKEN)
    {
        out.token_ids.push_back(bos_id_);
        out.attention_mask.push_back(1);
    }

    auto chunks = split_for_tiktoken(text);
    for (const auto& chunk : chunks)
    {
        auto merged = byte_pair_merge(chunk);
        if (merged.empty())
        {
            auto it = token_to_id_.find(chunk);
            if (it != token_to_id_.end())
            {
                out.token_ids.push_back(it->second);
                out.attention_mask.push_back(1);
            }
            continue;
        }

        for (const auto& [start, end] : merged)
        {
            if (end > chunk.size() || end <= start)
            {
                continue;
            }

            const std::string piece = chunk.substr(start, end - start);
            auto it = token_to_id_.find(piece);
            if (it != token_to_id_.end())
            {
                out.token_ids.push_back(it->second);
                out.attention_mask.push_back(1);
                continue;
            }

            for (unsigned char c : piece)
            {
                std::string one(1, static_cast<char>(c));
                auto bit = token_to_id_.find(one);
                if (bit != token_to_id_.end())
                {
                    out.token_ids.push_back(bit->second);
                }
                else
                {
                    out.token_ids.push_back(unk_id_);
                }
                out.attention_mask.push_back(1);
            }
        }
    }

    if (add_special_tokens && eos_id_ != INVALID_TOKEN)
    {
        out.token_ids.push_back(eos_id_);
        out.attention_mask.push_back(1);
    }

    return out;
}

std::string TiktokenTokenizer::decode(const std::vector<TokenId>& tokens, const DecodeOptions& options) const
{
    std::string out;
    for (TokenId id : tokens)
    {
        if (id < 0 || static_cast<size_t>(id) >= id_to_token_.size())
        {
            continue;
        }
        if (options.skip_special_tokens && is_special_token(id))
        {
            continue;
        }
        out += id_to_token_[static_cast<size_t>(id)];
    }
    return out;
}

TokenId TiktokenTokenizer::token_to_id(const std::string& token) const
{
    auto it = token_to_id_.find(token);
    return it != token_to_id_.end() ? it->second : unk_id_;
}

std::string TiktokenTokenizer::id_to_token(TokenId id) const
{
    if (id >= 0 && static_cast<size_t>(id) < id_to_token_.size())
    {
        return id_to_token_[static_cast<size_t>(id)];
    }
    return {};
}

size_t TiktokenTokenizer::vocab_size() const
{
    return id_to_token_.size();
}

const TokenInfo& TiktokenTokenizer::get_token_info(TokenId id) const
{
    static TokenInfo invalid{};
    if (id < 0 || static_cast<size_t>(id) >= token_info_.size())
    {
        return invalid;
    }
    return token_info_[static_cast<size_t>(id)];
}

bool TiktokenTokenizer::is_special_token(TokenId id) const
{
    for (const auto& kv : special_tokens_)
    {
        if (kv.second == id)
        {
            return true;
        }
    }
    return id == bos_id_ || id == eos_id_ || id == pad_id_;
}

bool TiktokenTokenizer::is_control_token(TokenId id) const
{
    return is_special_token(id);
}

bool TiktokenTokenizer::is_byte_token(TokenId id) const
{
    if (id < 0 || static_cast<size_t>(id) >= id_to_token_.size())
    {
        return false;
    }
    const std::string& tok = id_to_token_[static_cast<size_t>(id)];
    return tok.rfind("<0x", 0) == 0;
}

} // namespace tokenizer

#include "wordpiece_tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace tokenizer
{

WordPieceTokenizer::WordPieceTokenizer() = default;
WordPieceTokenizer::~WordPieceTokenizer() = default;

bool WordPieceTokenizer::load_from_file(const std::string& vocab_path)
{
    std::ifstream f(vocab_path);
    if (!f.is_open())
    {
        return false;
    }

    vocab_.clear();
    id_to_token_.clear();
    token_info_.clear();

    std::string line;
    TokenId id = 0;
    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        vocab_[line] = id;
        id_to_token_.push_back(line);
        token_info_.push_back(TokenInfo{id, line, 0.0f, 0});

        if (line == "[PAD]")
            pad_id_ = id;
        else if (line == "[UNK]")
            unk_id_ = id;
        else if (line == "[CLS]")
        {
            cls_id_ = id;
            bos_id_ = id;
        }
        else if (line == "[SEP]")
        {
            sep_id_ = id;
            eos_id_ = id;
        }
        else if (line == "[MASK]")
            mask_id_ = id;

        ++id;
    }

    return !id_to_token_.empty();
}

bool WordPieceTokenizer::load_from_files(const std::string& vocab_path, const std::string& merges_path)
{
    (void)merges_path;
    return load_from_file(vocab_path);
}

std::string WordPieceTokenizer::clean_text(const std::string& text) const
{
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text)
    {
        if (c == '\t' || c == '\n' || c == '\r' || c >= 0x20u)
        {
            out.push_back(static_cast<char>(c));
        }
    }

    if (do_lower_case_)
    {
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    return out;
}

std::vector<std::string> WordPieceTokenizer::tokenize(const std::string& text) const
{
    std::istringstream iss(clean_text(text));
    std::vector<std::string> out;
    std::string w;
    while (iss >> w)
    {
        out.push_back(w);
    }
    return out;
}

std::vector<TokenId> WordPieceTokenizer::wordpiece_tokenize(const std::string& word) const
{
    if (static_cast<int>(word.size()) > max_input_chars_per_word_)
    {
        return {unk_id_};
    }

    std::vector<TokenId> pieces;
    size_t start = 0;
    while (start < word.size())
    {
        size_t end = word.size();
        TokenId best = INVALID_TOKEN;

        while (start < end)
        {
            std::string sub = word.substr(start, end - start);
            if (start > 0)
            {
                sub = "##" + sub;
            }
            auto it = vocab_.find(sub);
            if (it != vocab_.end())
            {
                best = it->second;
                break;
            }
            --end;
        }

        if (best == INVALID_TOKEN)
        {
            return {unk_id_};
        }

        pieces.push_back(best);
        start = end;
    }

    return pieces;
}

Encoding WordPieceTokenizer::encode(const std::string& text, bool add_special_tokens) const
{
    Encoding out;

    if (add_special_tokens && cls_id_ != INVALID_TOKEN)
    {
        out.token_ids.push_back(cls_id_);
        out.attention_mask.push_back(1);
    }

    for (const auto& word : tokenize(text))
    {
        auto ids = wordpiece_tokenize(word);
        for (TokenId id : ids)
        {
            out.token_ids.push_back(id);
            out.attention_mask.push_back(1);
        }
    }

    if (add_special_tokens && sep_id_ != INVALID_TOKEN)
    {
        out.token_ids.push_back(sep_id_);
        out.attention_mask.push_back(1);
    }

    return out;
}

std::string WordPieceTokenizer::decode(const std::vector<TokenId>& tokens, const DecodeOptions& options) const
{
    std::string out;
    bool first = true;

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

        std::string t = id_to_token_[static_cast<size_t>(id)];
        if (t.rfind("##", 0) == 0)
        {
            out += t.substr(2);
        }
        else
        {
            if (!first)
            {
                out.push_back(' ');
            }
            out += t;
            first = false;
        }
    }

    return out;
}

TokenId WordPieceTokenizer::token_to_id(const std::string& token) const
{
    auto it = vocab_.find(token);
    return it != vocab_.end() ? it->second : unk_id_;
}

std::string WordPieceTokenizer::id_to_token(TokenId id) const
{
    if (id >= 0 && static_cast<size_t>(id) < id_to_token_.size())
    {
        return id_to_token_[static_cast<size_t>(id)];
    }
    return {};
}

size_t WordPieceTokenizer::vocab_size() const
{
    return id_to_token_.size();
}

const TokenInfo& WordPieceTokenizer::get_token_info(TokenId id) const
{
    static TokenInfo invalid{};
    if (id < 0 || static_cast<size_t>(id) >= token_info_.size())
    {
        return invalid;
    }
    return token_info_[static_cast<size_t>(id)];
}

bool WordPieceTokenizer::is_special_token(TokenId id) const
{
    return id == pad_id_ || id == unk_id_ || id == cls_id_ || id == sep_id_ || id == mask_id_;
}

bool WordPieceTokenizer::is_control_token(TokenId id) const
{
    return is_special_token(id);
}

bool WordPieceTokenizer::is_byte_token(TokenId id) const
{
    (void)id;
    return false;
}

} // namespace tokenizer

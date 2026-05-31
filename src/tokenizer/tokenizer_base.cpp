#include "tokenizer_base.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace tokenizer
{

std::vector<Encoding> TokenizerBase::encode_batch(const std::vector<std::string>& texts, bool add_special_tokens) const
{
    std::vector<Encoding> out;
    out.reserve(texts.size());
    for (const auto& t : texts)
    {
        out.push_back(encode(t, add_special_tokens));
    }
    return out;
}

std::vector<std::string> TokenizerBase::decode_batch(const std::vector<std::vector<TokenId>>& batch,
                                                     const DecodeOptions& options) const
{
    std::vector<std::string> out;
    out.reserve(batch.size());
    for (const auto& toks : batch)
    {
        out.push_back(decode(toks, options));
    }
    return out;
}

std::vector<uint32_t> TokenizerBase::utf8_to_codepoints(const std::string& text)
{
    std::vector<uint32_t> cps;
    cps.reserve(text.size());

    const uint8_t* s = reinterpret_cast<const uint8_t*>(text.data());
    size_t i = 0;
    const size_t n = text.size();
    while (i < n)
    {
        uint32_t cp = 0;
        uint8_t c = s[i];
        if ((c & 0x80u) == 0)
        {
            cp = c;
            i += 1;
        }
        else if ((c & 0xE0u) == 0xC0u && i + 1 < n)
        {
            cp = ((uint32_t)(c & 0x1Fu) << 6) | (uint32_t)(s[i + 1] & 0x3Fu);
            i += 2;
        }
        else if ((c & 0xF0u) == 0xE0u && i + 2 < n)
        {
            cp = ((uint32_t)(c & 0x0Fu) << 12) | ((uint32_t)(s[i + 1] & 0x3Fu) << 6) | (uint32_t)(s[i + 2] & 0x3Fu);
            i += 3;
        }
        else if ((c & 0xF8u) == 0xF0u && i + 3 < n)
        {
            cp = ((uint32_t)(c & 0x07u) << 18) | ((uint32_t)(s[i + 1] & 0x3Fu) << 12) |
                 ((uint32_t)(s[i + 2] & 0x3Fu) << 6) | (uint32_t)(s[i + 3] & 0x3Fu);
            i += 4;
        }
        else
        {
            // Invalid sequence, skip byte.
            i += 1;
            continue;
        }
        cps.push_back(cp);
    }
    return cps;
}

std::string TokenizerBase::codepoints_to_utf8(const std::vector<uint32_t>& cps)
{
    std::string out;
    for (uint32_t cp : cps)
    {
        if (cp < 0x80u)
        {
            out.push_back((char)cp);
        }
        else if (cp < 0x800u)
        {
            out.push_back((char)(0xC0u | ((cp >> 6) & 0x1Fu)));
            out.push_back((char)(0x80u | (cp & 0x3Fu)));
        }
        else if (cp < 0x10000u)
        {
            out.push_back((char)(0xE0u | ((cp >> 12) & 0x0Fu)));
            out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back((char)(0x80u | (cp & 0x3Fu)));
        }
        else
        {
            out.push_back((char)(0xF0u | ((cp >> 18) & 0x07u)));
            out.push_back((char)(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back((char)(0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}

std::unordered_map<uint8_t, std::string> TokenizerBase::build_byte_encoder()
{
    // GPT-2 style bytes_to_unicode()
    std::vector<int> bs;
    bs.reserve(256);
    for (int i = (int)'!'; i <= (int)'~'; ++i)
        bs.push_back(i);
    for (int i = 161; i <= 172; ++i)
        bs.push_back(i);
    for (int i = 174; i <= 255; ++i)
        bs.push_back(i);

    std::vector<int> cs = bs;
    int n = 0;
    for (int b = 0; b < 256; ++b)
    {
        if (std::find(bs.begin(), bs.end(), b) == bs.end())
        {
            bs.push_back(b);
            cs.push_back(256 + n);
            ++n;
        }
    }

    std::unordered_map<uint8_t, std::string> enc;
    enc.reserve(256);
    for (size_t i = 0; i < bs.size(); ++i)
    {
        const uint8_t byte = (uint8_t)bs[i];
        const uint32_t cp = (uint32_t)cs[i];
        enc[byte] = codepoints_to_utf8({cp});
    }
    return enc;
}

std::unordered_map<std::string, uint8_t> TokenizerBase::build_byte_decoder(
    const std::unordered_map<uint8_t, std::string>& enc)
{
    std::unordered_map<std::string, uint8_t> dec;
    dec.reserve(enc.size());
    for (const auto& kv : enc)
    {
        dec[kv.second] = kv.first;
    }
    return dec;
}

}  // namespace tokenizer

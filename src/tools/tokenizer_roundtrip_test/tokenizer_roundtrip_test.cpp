#include "../../tokenizer/huggingface_tokenizer.hpp"
#include "../../tokenizer/sentencepiece_tokenizer.hpp"
#include "../../tokenizer/tokenizer_factory.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace
{

static int g_failures = 0;

static void expect_(bool cond, const char* msg)
{
    if (!cond)
    {
        std::printf("FAIL: %s\n", msg);
        ++g_failures;
    }
}

static void expectEq_(const std::string& a, const std::string& b, const char* msg)
{
    if (a != b)
    {
        std::printf("FAIL: %s\n  expected='%s'\n  actual  ='%s'\n", msg, b.c_str(), a.c_str());
        ++g_failures;
    }
}

static void testHfTokenizerJson_()
{
    // Minimal HF tokenizer.json for BPE.
    // Includes: vocab + merges + ByteLevel pre_tokenizer (add_prefix_space=false).
    const std::string json = R"JSON(
{
  "version": "1.0",
  "model": {
    "type": "BPE",
    "vocab": {
      "<pad>": 0,
      "<s>": 1,
      "</s>": 2,
      "<unk>": 3,
      "h": 4,
      "i": 5,
      "hi": 6,
      "\u0120hi": 7
    },
    "merges": [
      "h i"
    ]
  },
  "pre_tokenizer": { "type": "ByteLevel", "add_prefix_space": false },
  "added_tokens": [
    { "id": 0, "content": "<pad>" },
    { "id": 1, "content": "<s>" },
    { "id": 2, "content": "</s>" },
    { "id": 3, "content": "<unk>" }
  ]
}
)JSON";

    tokenizer::HuggingFaceTokenizer hf;
    expect_(hf.load_from_json_string(json), "HF load_from_json_string()");
    expect_(hf.vocab_size() >= 8, "HF vocab_size()");

    // Roundtrip: "hi" should decode back to either "hi" or " hi" depending on prefix settings.
    // We set add_prefix_space=false, so expect plain "hi" for this tiny vocab.
    auto enc = hf.encode("hi", false);
    expect_(!enc.token_ids.empty(), "HF encode('hi')");
    const std::string dec = hf.decode(enc.token_ids, {});
    expectEq_(dec, "hi", "HF decode(encode('hi'))");
}

static void testSentencePieceTokenList_()
{
    tokenizer::SentencePieceTokenizer sp;
    std::vector<std::string> tokens = {"<unk>", "<s>", "</s>", "h", "i", "hi"};
    std::vector<float> scores(tokens.size(), 0.0f);
    std::vector<int32_t> types(tokens.size(), 0);

    expect_(sp.load_from_token_list(tokens, scores, types), "SP load_from_token_list()");
    sp.set_add_bos_token(false);
    sp.set_add_eos_token(false);

    auto enc = sp.encode("hi", false);
    expect_(!enc.token_ids.empty(), "SP encode('hi')");
    const std::string dec = sp.decode(enc.token_ids, {});
    expectEq_(dec, "hi", "SP decode(encode('hi'))");
}

static void testFactoryFromBlobHf_()
{
    const std::string json = R"JSON(
{
  "model": {
    "type": "BPE",
    "vocab": { "<unk>": 0, "a": 1, "b": 2, "ab": 3 },
    "merges": [ "a b" ]
  }
}
)JSON";

    auto t = tokenizer::TokenizerFactory::loadFromBlob((const uint8_t*)json.data(), json.size(),
                                                       tokenizer::TokenizerKind::HuggingFace);
    expect_(t != nullptr, "TokenizerFactory::loadFromBlob(HF)");

    if (t)
    {
        auto enc = t->encode("ab", false);
        expect_(!enc.token_ids.empty(), "Factory HF encode('ab')");
        const std::string dec = t->decode(enc.token_ids, {});
        expectEq_(dec, "ab", "Factory HF decode(encode('ab'))");
    }
}

}  // namespace

int main()
{
    testHfTokenizerJson_();
    testSentencePieceTokenList_();
    testFactoryFromBlobHf_();

    if (g_failures == 0)
    {
        std::printf("OK: tokenizer_roundtrip_test passed\n");
        return 0;
    }

    std::printf("FAILED: %d failures\n", g_failures);
    return 2;
}

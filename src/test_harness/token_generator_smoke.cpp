#include "../token_generator.h"
#include <iostream>

int main() {
    RawrXD::TokenGenerator tokenizer;

    const std::vector<std::string> vocab = {
        "<pad>", "<s>", "</s>", "<unk>", "<mask>",
        "h", "e", "l", "o", "w", "r", "d"
    };
    const std::vector<float> scores(vocab.size(), 0.0f);
    const std::vector<uint32_t> types(vocab.size(), 0);

    auto loaded = tokenizer.loadVocabularyFromMemory(vocab, scores, types);
    if (!loaded) {
        std::cerr << "loadVocabularyFromMemory failed\n";
        return 1;
    }

    auto encoded = tokenizer.encode("hello world");
    if (!encoded || encoded.value().empty()) {
        std::cerr << "encode failed\n";
        return 1;
    }

    auto decoded = tokenizer.decode(encoded.value());
    if (!decoded || decoded.value().empty()) {
        std::cerr << "decode failed\n";
        return 1;
    }

    auto batch = tokenizer.encodeBatch({"hello", "world"});
    if (!batch || batch.value().size() != 2) {
        std::cerr << "encodeBatch failed\n";
        return 1;
    }

    auto decodedBatch = tokenizer.decodeBatch(batch.value());
    if (!decodedBatch || decodedBatch.value().size() != 2) {
        std::cerr << "decodeBatch failed\n";
        return 1;
    }

    auto cached = tokenizer.encode("hello world");
    if (!cached || cached.value().empty()) {
        std::cerr << "cache-path encode failed\n";
        return 1;
    }

    std::cout << "token_generator_smoke: PASS\n";
    return 0;
}

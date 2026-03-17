#define UNIT_TESTING
#include "backend/ollama_client.h"
#include <iostream>

using namespace RawrXD::Backend;

int main() {
    OllamaClient client("http://localhost:11434");

    // Test parseResponse
    const std::string json_resp = R"({"model":"test","response":"hello","done":true,"total_duration":123,"prompt_eval_count":1})";
    OllamaResponse r = client.parseResponseForTest(json_resp);
    if (r.model != "test") { std::cerr << "model mismatch\n"; return 2; }
    if (r.response != "hello") { std::cerr << "response mismatch\n"; return 3; }
    if (!r.done) { std::cerr << "done mismatch\n"; return 4; }
    if (r.total_duration != 123) { std::cerr << "duration mismatch\n"; return 5; }

    // Test parseModels
    const std::string json_models = R"({"models":[{"name":"m1","modified_at":"t","size":123}]})";
    auto models = client.parseModelsForTest(json_models);
    if (models.size() != 1) { std::cerr << "models count mismatch\n"; return 6; }
    if (models[0].name != "m1") { std::cerr << "model name mismatch\n"; return 7; }

    std::cout << "OK\n";
    return 0;
}

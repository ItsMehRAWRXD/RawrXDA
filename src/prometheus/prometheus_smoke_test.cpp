#include "prometheus/prometheus_engine.h"
#include "prometheus/prometheus_800b_config.h"
#include "prometheus/prometheus_weight_loader.h"
#include <iostream>

int main() {
    auto config = Prometheus::get800BConfig();
    auto engine = Prometheus::PrometheusEngine::create("", config);

    Prometheus::Message msg;
    msg.role = "user";
    Prometheus::ContentPart part;
    part.type = Prometheus::ContentPart::Type::Text;
    part.text = "Hello Prometheus";
    msg.content.push_back(part);

    Prometheus::GenerationConfig genConfig;
    genConfig.maxTokens = 32;
    auto result = engine->generate({msg}, genConfig);

    std::cout << "Text: " << result.text << "\n";
    std::cout << "Tokens: " << result.totalTokens << "\n";
    std::cout << "TPS: " << result.tokensPerSecond << "\n";
    return 0;
}

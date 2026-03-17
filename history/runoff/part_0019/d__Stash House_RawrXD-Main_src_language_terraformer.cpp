// language_terraformer.cpp
#include "language_terraformer.hpp"
#include <cstring>
#include <string>
#include <unordered_map>

namespace RawrXD::TerraFormer {

LanguageTerraFormer::LanguageTerraFormer(TargetPlatform target)
    : target_(target) {}

std::vector<uint8_t> LanguageTerraFormer::emitBinary(const ASTNode& root, uint32_t flags) {
    // Prepare buffer (assume max 1MB for demo)
    buffer_.resize(1024 * 1024);
    std::memset(buffer_.data(), 0, buffer_.size());

    // Set flags based on target
    uint32_t masm_flags = 0;
    if (target_ == TargetPlatform::WINDOWS_PE) masm_flags |= 1;
    else if (target_ == TargetPlatform::LINUX_ELF) masm_flags |= 2;
    masm_flags |= flags;

    // Call MASM kernel
    int result = TerraFormer_EmitBinary(
        const_cast<ASTNode*>(&root),
        buffer_.data(),
        buffer_.size(),
        masm_flags
    );

    if (result != 0) {
        throw std::runtime_error("Binary emission failed");
    }

    // Trim buffer to actual size (find end marker)
    size_t actual_size = 0;
    for (size_t i = 0; i < buffer_.size(); ++i) {
        if (buffer_[i] == 0xFF && buffer_[i+1] == 0xFF) { // End marker
            actual_size = i;
            break;
        }
    }

    buffer_.resize(actual_size);
    return std::move(buffer_);
}

void LanguageTerraFormer::generateBackend(const std::string& languageSpec) {
    // Parse language spec: semicolon-delimited key=value pairs
    // e.g. "name=MyLang;has_functions=1;has_classes=1;entry=main"
    std::string spec = languageSpec;
    std::unordered_map<std::string, std::string> features;

    size_t pos = 0;
    while (pos < spec.size()) {
        size_t semi = spec.find(';', pos);
        if (semi == std::string::npos) semi = spec.size();
        std::string pair = spec.substr(pos, semi - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            features[pair.substr(0, eq)] = pair.substr(eq + 1);
        }
        pos = semi + 1;
    }

    // Build root AST template based on features
    ASTNode root{};
    root.type = ASTNodeType::STATEMENT; // MODULE-level

    // If language has functions, attach function template as left subtree
    if (features.count("has_functions") && features["has_functions"] == "1") {
        auto funcNode = std::make_unique<ASTNode>();
        funcNode->type = ASTNodeType::FUNCTION;

        if (features.count("entry")) {
            funcNode->name = features["entry"];
        } else {
            funcNode->name = "main";
        }

        root.left = std::move(funcNode);
    }

    // If language has classes/variables, attach as right subtree
    if (features.count("has_classes") && features["has_classes"] == "1") {
        auto varNode = std::make_unique<ASTNode>();
        varNode->type = ASTNodeType::VARIABLE;
        varNode->name = "class_template";
        root.right = std::move(varNode);
    }

    // Emit binary from the generated AST template
    uint32_t flags = 0;
    if (features.count("debug") && features["debug"] == "1") flags |= 0x10;
    buffer_ = emitBinary(root, flags);
}

} // namespace RawrXD::TerraFormer
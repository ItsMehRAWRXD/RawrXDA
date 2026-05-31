// D:\rawrxd\generator\code_builder.cpp
// Fluent API Implementation for Building C++ Code

#include "code_builder.hpp"
#include <algorithm>

namespace RawrXD::CodeGen {

CodeBuilder::CodeBuilder() = default;
CodeBuilder::~CodeBuilder() = default;

std::string CodeBuilder::GetIndent() const {
    std::string result;
    for (int i = 0; i < indent_level_; ++i) {
        result += indent_str_;
    }
    return result;
}

CodeBuilder& CodeBuilder::BeginClass(const std::string& name, bool is_struct) {
    stream_ << GetIndent() << (is_struct ? "struct" : "class") << " " << name << " {\n";
    if (!is_struct) {
        stream_ << GetIndent() << "public:\n";
        indent_level_++;
    }
    return *this;
}

CodeBuilder& CodeBuilder::EndClass() {
    if (indent_level_ > 0) indent_level_--;
    stream_ << GetIndent() << "};\n\n";
    return *this;
}

CodeBuilder& CodeBuilder::BeginMethod(const std::string& signature) {
    stream_ << GetIndent() << signature << " {\n";
    indent_level_++;
    return *this;
}

CodeBuilder& CodeBuilder::EndMethod() {
    indent_level_--;
    stream_ << GetIndent() << "}\n\n";
    return *this;
}

CodeBuilder& CodeBuilder::AddField(const std::string& type, const std::string& name, 
                                    const std::string& default_val) {
    stream_ << GetIndent() << type << " " << name;
    if (!default_val.empty()) {
        stream_ << " = " << default_val;
    }
    stream_ << ";\n";
    return *this;
}

CodeBuilder& CodeBuilder::AddStatement(const std::string& stmt) {
    stream_ << GetIndent() << stmt << "\n";
    return *this;
}

CodeBuilder& CodeBuilder::AddComment(const std::string& comment) {
    stream_ << GetIndent() << "// " << comment << "\n";
    return *this;
}

CodeBuilder& CodeBuilder::BeginNamespace(const std::string& name) {
    stream_ << "\nnamespace " << name << " {\n\n";
    return *this;
}

CodeBuilder& CodeBuilder::EndNamespace() {
    stream_ << "\n} // namespace\n\n";
    return *this;
}

CodeBuilder& CodeBuilder::BeginBlock(const std::string& label) {
    if (!label.empty()) {
        stream_ << GetIndent() << "{ // " << label << "\n";
    } else {
        stream_ << GetIndent() << "{\n";
    }
    indent_level_++;
    return *this;
}

CodeBuilder& CodeBuilder::EndBlock() {
    indent_level_--;
    stream_ << GetIndent() << "}\n";
    return *this;
}

CodeBuilder& CodeBuilder::Indent() {
    indent_level_++;
    return *this;
}

CodeBuilder& CodeBuilder::Outdent() {
    if (indent_level_ > 0) indent_level_--;
    return *this;
}

std::string CodeBuilder::Build() const {
    return stream_.str();
}

void CodeBuilder::Reset() {
    stream_.clear();
    stream_.str("");
    indent_level_ = 0;
}

} // namespace RawrXD::CodeGen

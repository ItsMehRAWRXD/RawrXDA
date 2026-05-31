// D:\rawrxd\generator\code_builder.hpp
// Fluent API for Building C++ Code

#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace RawrXD::CodeGen {

class CodeBuilder {
public:
    CodeBuilder();
    ~CodeBuilder();
    
    // Fluent API for building code
    CodeBuilder& BeginClass(const std::string& name, bool is_struct = false);
    CodeBuilder& EndClass();
    
    CodeBuilder& BeginMethod(const std::string& signature);
    CodeBuilder& EndMethod();
    
    CodeBuilder& AddField(const std::string& type, const std::string& name, 
                          const std::string& default_val = "");
    
    CodeBuilder& AddStatement(const std::string& stmt);
    CodeBuilder& AddComment(const std::string& comment);
    
    CodeBuilder& BeginNamespace(const std::string& name);
    CodeBuilder& EndNamespace();
    
    CodeBuilder& BeginBlock(const std::string& label = "");
    CodeBuilder& EndBlock();
    
    // Indentation management
    CodeBuilder& Indent();
    CodeBuilder& Outdent();
    
    // Build final output
    std::string Build() const;
    
    // Clear for reuse
    void Reset();
    
private:
    std::ostringstream stream_;
    int indent_level_ = 0;
    std::string indent_str_ = "    ";
    
    std::string GetIndent() const;
};

} // namespace RawrXD::CodeGen

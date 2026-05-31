// D:\rawrxd\generator\ast.hpp
// Abstract Syntax Tree for Code Generation
// AUTO-GENERATED-CAPABLE

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace RawrXD::AST {

struct Parameter {
    std::string type_name;
    std::string name;
    bool is_const_ref = false;
    bool is_optional = false;
    std::string default_value;
    bool requires_validation = false;
};

struct Method {
    std::string name;
    std::string return_type;
    std::vector<Parameter> parameters;
    bool can_fail = false;
    bool is_const = false;
    bool is_static = false;
    bool is_virtual = false;
    bool requires_atomic = false;
    std::string implementation_stub;
};

struct Field {
    std::string type_name;
    std::string name;
    std::string default_value;
    bool is_const = false;
};

struct Type {
    std::string name;
    bool is_struct = false;
    bool requires_atomic = false;
    std::vector<Field> fields;
    std::vector<Method> methods;
    std::vector<std::string> dependencies;
    
    // Metadata for code generation
    std::string documentation;
    std::unordered_map<std::string, std::string> attributes;
};

struct Module {
    std::string name;
    std::vector<Type> types;
    std::vector<std::string> imports;
    std::string namespace_name;
};

} // namespace RawrXD::AST

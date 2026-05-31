#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

struct TestContext {
    enum RefactorOp {
        RENAME,
        EXTRACT_VAR,
        SORT_INCLUDES,
        EXTRACT_FUNC,
        INLINE_VAR,
        GEN_GETTER_SETTER
    };

    std::string type_decl;
    int scope_level = 0;
    std::string storage_class;
    std::string qualifiers;
    bool is_pointer = false;
    bool is_array = false;
    int template_params = 0;
    std::string identifier_name;

    RefactorOp operation = RENAME;

    std::string symbol_type;
    int cross_file_count = 0;
    bool has_collisions = false;
    std::string new_name;

    std::string expression_type;
    int complexity = 1;
    bool has_side_effects = false;
    int expression_depth = 1;

    int include_count = 0;
    bool has_circular_deps = false;
    std::vector<std::string> include_paths;

    int file_count = 0;
    int dir_depth = 0;
    std::string build_system;

    uint64_t context_id = 0;

    bool passed = false;
    std::string error_message;
    std::chrono::microseconds execution_time{0};
};

class CodeGenerator {
public:
    static std::string generate_test_code(const TestContext& ctx)
    {
        std::ostringstream code;
        code << generate_includes(ctx);
        code << generate_structure_begin(ctx);

        switch (ctx.operation) {
        case TestContext::RENAME:
            code << generate_rename_test_code(ctx);
            break;
        case TestContext::EXTRACT_VAR:
            code << generate_extract_var_test_code(ctx);
            break;
        case TestContext::SORT_INCLUDES:
            code << generate_sort_includes_test_code(ctx);
            break;
        case TestContext::EXTRACT_FUNC:
            code << generate_extract_func_test_code(ctx);
            break;
        case TestContext::INLINE_VAR:
            code << generate_inline_var_test_code(ctx);
            break;
        case TestContext::GEN_GETTER_SETTER:
            code << generate_getter_setter_test_code(ctx);
            break;
        }

        code << generate_structure_end(ctx);
        return code.str();
    }

private:
    static std::string generate_includes(const TestContext& ctx)
    {
        std::ostringstream code;

        if (ctx.operation == TestContext::SORT_INCLUDES) {
            std::vector<std::string> system_headers = {
                "<iostream>", "<vector>", "<string>", "<map>", "<algorithm>",
                "<memory>",   "<utility>", "<functional>", "<chrono>", "<thread>",
                "<mutex>",    "<atomic>",  "<future>",     "<condition_variable>"};

            std::vector<std::string> project_headers = {
                "\"myclass.h\"", "\"utils.h\"", "\"types.h\"", "\"config.h\"",
                "\"helper.h\"",  "\"constants.h\""};

            std::vector<std::string> third_party = {
                "<boost/optional.hpp>", "<boost/variant.hpp>",
                "<gtest/gtest.h>",      "<nlohmann/json.hpp>"};

            const int effective_include_count = std::max(1, ctx.include_count);
            const int sys_count = std::min(effective_include_count, static_cast<int>(system_headers.size()));
            for (int i = 0; i < sys_count; ++i) {
                code << "#include " << system_headers[static_cast<size_t>(i)] << "\n";
            }

            const int project_count = std::min(effective_include_count / 3, static_cast<int>(project_headers.size()));
            for (int i = 0; i < project_count; ++i) {
                code << "#include " << project_headers[static_cast<size_t>(i)] << "\n";
            }

            if (effective_include_count > 10) {
                const size_t tp_idx = static_cast<size_t>(ctx.context_id % third_party.size());
                code << "#include " << third_party[tp_idx] << "\n";
            }

            code << "\n";
        } else {
            code << "#include <iostream>\n";
            code << "#include <vector>\n";
            code << "#include <string>\n\n";
        }

        return code.str();
    }

    static std::string generate_structure_begin(const TestContext& ctx)
    {
        std::ostringstream code;

        if (ctx.scope_level >= 1) {
            code << "namespace test_namespace {\n";
        }
        if (ctx.scope_level >= 2) {
            code << "class TestClass {\n";
            code << "public:\n";
        }
        if (ctx.scope_level >= 3) {
            code << "    class NestedClass {\n";
            code << "    public:\n";
        }

        return code.str();
    }

    static std::string generate_structure_end(const TestContext& ctx)
    {
        std::ostringstream code;

        if (ctx.scope_level >= 3) {
            code << "    };\n";
        }
        if (ctx.scope_level >= 2) {
            code << "};\n";
        }
        if (ctx.scope_level >= 1) {
            code << "}  // namespace test_namespace\n";
        }

        return code.str();
    }

    static std::string generate_rename_test_code(const TestContext& ctx)
    {
        std::ostringstream code;
        std::string indent(static_cast<size_t>(ctx.scope_level * 2), ' ');

        code << indent << generate_declaration(ctx) << " " << ctx.identifier_name;
        if (ctx.is_array) {
            code << "[10]";
        }
        code << " = ";

        if (ctx.type_decl == "int" || ctx.type_decl == "const int") {
            code << "42";
        } else if (ctx.type_decl == "float" || ctx.type_decl == "double") {
            code << "3.14";
        } else if (ctx.type_decl == "bool") {
            code << "true";
        } else if (ctx.type_decl == "char") {
            code << "'A'";
        } else if (ctx.type_decl.find("string") != std::string::npos) {
            code << "\"test\"";
        } else if (ctx.type_decl == "auto") {
            code << "42";
        } else {
            code << "{}";
        }

        code << ";\n\n";

        code << indent << "void testFunction() {\n";
        code << indent << "    " << ctx.type_decl << " local_copy = " << ctx.identifier_name << ";\n";
        code << indent << "    std::cout << " << ctx.identifier_name << " << std::endl;\n";

        if (ctx.complexity > 1) {
            code << indent << "    " << ctx.identifier_name << " += 1;\n";
            code << indent << "    if (" << ctx.identifier_name << " > 0) {\n";
            code << indent << "        " << ctx.identifier_name << " *= 2;\n";
            code << indent << "    }\n";
        }

        code << indent << "}\n";
        return code.str();
    }

    static std::string generate_extract_var_test_code(const TestContext& ctx)
    {
        std::ostringstream code;
        std::string indent(static_cast<size_t>(ctx.scope_level * 2), ' ');

        code << indent << generate_declaration(ctx) << " calculateResult() {\n";
        std::string expression = generate_complex_expression(ctx);
        code << indent << "    // MARKER: Extract this expression\n";
        code << indent << "    return " << expression << ";\n";
        code << indent << "}\n";
        code << indent << "void useResult() {\n";
        code << indent << "    auto result = calculateResult();\n";
        code << indent << "    std::cout << result << std::endl;\n";
        code << indent << "}\n";

        return code.str();
    }

    static std::string generate_sort_includes_test_code(const TestContext&)
    {
        return "// Code body here\nint main() { return 0; }\n";
    }

    static std::string generate_extract_func_test_code(const TestContext& ctx)
    {
        std::ostringstream code;
        std::string indent(static_cast<size_t>(ctx.scope_level * 2), ' ');

        code << indent << "void process() {\n";
        code << indent << "    " << generate_declaration(ctx) << " data = ";
        code << generate_complex_expression(ctx) << ";\n";
        code << indent << "\n";
        code << indent << "    // MARKER: Extract function from here\n";
        code << indent << "    " << generate_declaration(ctx) << " result = 0;\n";
        code << indent << "    for (int i = 0; i < 10; ++i) {\n";
        code << indent << "        result += data * i;\n";
        code << indent << "    }\n";
        code << indent << "    std::cout << result << std::endl;\n";
        code << indent << "    // MARKER: Extract function to here\n";
        code << indent << "}\n";

        return code.str();
    }

    static std::string generate_inline_var_test_code(const TestContext& ctx)
    {
        std::ostringstream code;
        std::string indent(static_cast<size_t>(ctx.scope_level * 2), ' ');

        code << indent << "// MARKER: Inline this variable\n";
        code << indent << generate_declaration(ctx) << " " << ctx.identifier_name << " = ";
        code << generate_complex_expression(ctx) << ";\n";
        code << indent << "\n";
        code << indent << "void useVar() {\n";
        code << indent << "    auto value = " << ctx.identifier_name << " + 1;\n";
        code << indent << "    std::cout << " << ctx.identifier_name << " << std::endl;\n";
        code << indent << "    if (" << ctx.identifier_name << " > 0) {\n";
        code << indent << "        std::cout << \"positive\";\n";
        code << indent << "    }\n";
        code << indent << "}\n";

        return code.str();
    }

    static std::string generate_getter_setter_test_code(const TestContext& ctx)
    {
        std::ostringstream code;
        std::string indent(static_cast<size_t>(ctx.scope_level * 2), ' ');

        code << indent << "private:\n";
        code << indent << "    // MARKER: Generate getter/setter\n";
        code << indent << "    " << generate_declaration(ctx) << " " << ctx.identifier_name << ";\n";
        code << indent << "\n";
        code << indent << "public:\n";
        code << indent << "    void setValue(" << generate_declaration(ctx) << " val) {\n";
        code << indent << "        " << ctx.identifier_name << " = val;\n";
        code << indent << "    }\n";

        return code.str();
    }

    static std::string generate_declaration(const TestContext& ctx)
    {
        std::ostringstream decl;

        if (!ctx.storage_class.empty()) {
            decl << ctx.storage_class << " ";
        }
        if (!ctx.qualifiers.empty()) {
            decl << ctx.qualifiers << " ";
        }

        decl << ctx.type_decl;

        if (ctx.is_pointer) {
            decl << "*";
        }

        if (ctx.template_params > 0) {
            decl << "<";
            for (int i = 0; i < ctx.template_params; ++i) {
                if (i > 0) {
                    decl << ", ";
                }
                decl << "T" << i;
            }
            decl << ">";
        }

        return decl.str();
    }

    static std::string generate_complex_expression(const TestContext& ctx)
    {
        std::ostringstream expr;

        if (ctx.expression_type == "arithmetic") {
            expr << "((a + b) * c - d) / e + f";
            if (ctx.complexity > 1) {
                for (int i = 1; i < ctx.complexity; ++i) {
                    expr << " + (x" << i << " * y" << i << ")";
                }
            }
        } else if (ctx.expression_type == "logical") {
            expr << "(a && b) || (c && !d)";
            if (ctx.complexity > 1) {
                expr << " && (e || f)";
            }
        } else if (ctx.expression_type == "comparison") {
            expr << "(a < b) && (c >= d)";
        } else if (ctx.expression_type == "bitwise") {
            expr << "(a & b) | (c ^ d)";
        } else if (ctx.expression_type == "function_call") {
            expr << "func1(func2(a, b), func3(c))";
        } else if (ctx.expression_type == "lambda") {
            expr << "[](int x) { return x * 2; }(value)";
        } else {
            expr << "value";
        }

        return expr.str();
    }
};

class RefactoringEngine {
public:
    struct RefactorResult {
        bool success = false;
        std::string modified_code;
        std::string error_message;
        int locations_modified = 0;
    };

    static RefactorResult apply_refactoring(TestContext::RefactorOp op, const std::string& code, const TestContext& ctx)
    {
        switch (op) {
        case TestContext::RENAME:
            return apply_rename(code, ctx);
        case TestContext::EXTRACT_VAR:
            return apply_extract_var(code, ctx);
        case TestContext::SORT_INCLUDES:
            return apply_sort_includes(code, ctx);
        case TestContext::EXTRACT_FUNC:
            return apply_extract_func(code, ctx);
        case TestContext::INLINE_VAR:
            return apply_inline_var(code, ctx);
        case TestContext::GEN_GETTER_SETTER:
            return apply_generate_getter_setter(code, ctx);
        default: {
            RefactorResult result;
            result.success = false;
            result.error_message = "Unknown operation";
            return result;
        }
        }
    }

private:
    static bool is_identifier_char(char c)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        return std::isalnum(uc) != 0 || c == '_';
    }

    static RefactorResult apply_rename(const std::string& code, const TestContext& ctx)
    {
        RefactorResult result;
        std::string modified = code;
        const std::string& old_name = ctx.identifier_name;
        const std::string& new_name = ctx.new_name;

        size_t pos = 0;
        while ((pos = modified.find(old_name, pos)) != std::string::npos) {
            bool is_whole_word = true;
            if (pos > 0 && is_identifier_char(modified[pos - 1])) {
                is_whole_word = false;
            }
            if (pos + old_name.length() < modified.length() && is_identifier_char(modified[pos + old_name.length()])) {
                is_whole_word = false;
            }

            if (is_whole_word) {
                modified.replace(pos, old_name.length(), new_name);
                result.locations_modified++;
                pos += new_name.length();
            } else {
                pos++;
            }
        }

        result.success = result.locations_modified > 0;
        result.modified_code = modified;
        if (!result.success) {
            result.error_message = "No occurrences of '" + old_name + "' found";
        }

        return result;
    }

    static RefactorResult apply_extract_var(const std::string& code, const TestContext& ctx)
    {
        RefactorResult result;

        size_t marker_pos = code.find("// MARKER: Extract this expression");
        if (marker_pos == std::string::npos) {
            result.error_message = "No extraction marker found";
            return result;
        }

        size_t return_pos = code.find("return ", marker_pos);
        if (return_pos == std::string::npos) {
            result.error_message = "No return statement found";
            return result;
        }

        size_t expr_start = return_pos + 7;
        size_t expr_end = code.find(';', expr_start);
        if (expr_end == std::string::npos) {
            result.error_message = "Return expression terminator not found";
            return result;
        }

        std::string expression = code.substr(expr_start, expr_end - expr_start);
        std::string var_name = "extracted_" + std::to_string(ctx.context_id);

        std::string modified = code;
        std::string extracted_decl = "auto " + var_name + " = " + expression + ";\n    ";
        modified.insert(return_pos, extracted_decl);

        expr_start = return_pos + extracted_decl.length() + 7;
        expr_end = modified.find(';', expr_start);
        if (expr_end == std::string::npos) {
            result.error_message = "Modified expression terminator not found";
            return result;
        }
        modified.replace(expr_start, expr_end - expr_start, var_name);

        result.success = true;
        result.modified_code = std::move(modified);
        result.locations_modified = 1;
        return result;
    }

    static RefactorResult apply_sort_includes(const std::string& code, const TestContext&)
    {
        RefactorResult result;

        std::vector<std::pair<size_t, std::string>> includes;
        std::istringstream iss(code);
        std::string line;
        size_t pos = 0;

        while (std::getline(iss, line)) {
            if (line.rfind("#include", 0) == 0) {
                includes.emplace_back(pos, line);
            }
            pos += line.length() + 1;
        }

        if (includes.empty()) {
            result.error_message = "No includes found";
            return result;
        }

        std::vector<std::string> system_includes;
        std::vector<std::string> project_includes;
        std::vector<std::string> third_party_includes;

        for (const auto& pair : includes) {
            const std::string& include_line = pair.second;
            if (include_line.find('<') != std::string::npos) {
                if (include_line.find("boost") != std::string::npos || include_line.find("gtest") != std::string::npos ||
                    include_line.find("nlohmann") != std::string::npos) {
                    third_party_includes.push_back(include_line);
                } else {
                    system_includes.push_back(include_line);
                }
            } else if (include_line.find('"') != std::string::npos) {
                project_includes.push_back(include_line);
            }
        }

        std::sort(system_includes.begin(), system_includes.end());
        std::sort(project_includes.begin(), project_includes.end());
        std::sort(third_party_includes.begin(), third_party_includes.end());

        std::ostringstream sorted_code;
        for (const auto& inc : system_includes) {
            sorted_code << inc << "\n";
        }
        sorted_code << "\n";
        for (const auto& inc : third_party_includes) {
            sorted_code << inc << "\n";
        }
        sorted_code << "\n";
        for (const auto& inc : project_includes) {
            sorted_code << inc << "\n";
        }
        sorted_code << "\n";

        size_t first_include = code.find("#include");
        size_t last_include = code.rfind("#include");
        if (first_include == std::string::npos || last_include == std::string::npos) {
            result.error_message = "Include boundaries not found";
            return result;
        }
        last_include = code.find('\n', last_include);
        if (last_include == std::string::npos) {
            last_include = code.length();
        }

        std::string modified = code.substr(0, first_include);
        modified += sorted_code.str();
        if (last_include < code.length()) {
            modified += code.substr(last_include + 1);
        }

        result.success = true;
        result.modified_code = std::move(modified);
        result.locations_modified = static_cast<int>(includes.size());
        return result;
    }

    static RefactorResult apply_extract_func(const std::string& code, const TestContext& ctx)
    {
        RefactorResult result;

        size_t start_marker = code.find("// MARKER: Extract function from here");
        size_t end_marker = code.find("// MARKER: Extract function to here");
        if (start_marker == std::string::npos || end_marker == std::string::npos || end_marker <= start_marker) {
            result.error_message = "Extraction markers not found";
            return result;
        }

        size_t func_start = code.find('\n', start_marker);
        if (func_start == std::string::npos) {
            result.error_message = "Could not locate extraction start";
            return result;
        }
        func_start += 1;
        size_t func_end = end_marker;
        std::string func_body = code.substr(func_start, func_end - func_start);

        std::string func_name = "extracted_function_" + std::to_string(ctx.context_id);
        std::string new_func = "\nvoid " + func_name + "() {\n" + func_body + "}\n\n";

        size_t insert_pos = code.rfind('\n', start_marker);
        if (insert_pos == std::string::npos) {
            insert_pos = 0;
        } else {
            size_t prior = code.rfind('\n', insert_pos > 0 ? insert_pos - 1 : 0);
            if (prior != std::string::npos) {
                insert_pos = prior + 1;
            }
        }

        std::string modified = code;
        modified.insert(insert_pos, new_func);

        start_marker = modified.find("// MARKER: Extract function from here");
        end_marker = modified.find("// MARKER: Extract function to here");
        func_start = modified.find('\n', start_marker);
        if (func_start == std::string::npos || end_marker == std::string::npos) {
            result.error_message = "Could not locate extraction markers after insert";
            return result;
        }
        func_start += 1;

        modified.replace(func_start, end_marker - func_start, "    " + func_name + "();\n");

        result.success = true;
        result.modified_code = std::move(modified);
        result.locations_modified = 1;
        return result;
    }

    static RefactorResult apply_inline_var(const std::string& code, const TestContext& ctx)
    {
        RefactorResult result;

        size_t marker_pos = code.find("// MARKER: Inline this variable");
        if (marker_pos == std::string::npos) {
            result.error_message = "Inline marker not found";
            return result;
        }

        size_t decl_start = code.find('\n', marker_pos);
        if (decl_start == std::string::npos) {
            result.error_message = "Declaration start not found";
            return result;
        }
        decl_start += 1;
        size_t decl_end = code.find(';', decl_start);
        if (decl_end == std::string::npos) {
            result.error_message = "Declaration terminator not found";
            return result;
        }

        std::string decl_line = code.substr(decl_start, decl_end - decl_start + 1);
        size_t eq_pos = decl_line.find('=');
        if (eq_pos == std::string::npos) {
            result.error_message = "Could not parse variable declaration";
            return result;
        }

        std::string value = decl_line.substr(eq_pos + 1);
        size_t semi = value.find(';');
        if (semi != std::string::npos) {
            value = value.substr(0, semi);
        }
        size_t start = value.find_first_not_of(" \t");
        size_t end = value.find_last_not_of(" \t");
        if (start == std::string::npos || end == std::string::npos) {
            result.error_message = "Could not extract inline value";
            return result;
        }
        value = value.substr(start, end - start + 1);

        std::string modified = code;
        size_t erase_start = decl_start;
        while (erase_start > 0 && (modified[erase_start - 1] == ' ' || modified[erase_start - 1] == '\t')) {
            erase_start--;
        }
        size_t erase_end = decl_end + 1;
        if (erase_end < modified.size() && modified[erase_end] == '\n') {
            erase_end++;
        }
        if (erase_end > erase_start && erase_end <= modified.size()) {
            modified.erase(erase_start, erase_end - erase_start);
        }

        size_t pos = 0;
        int replacements = 0;
        while ((pos = modified.find(ctx.identifier_name, pos)) != std::string::npos) {
            bool is_whole_word = true;
            if (pos > 0 && is_identifier_char(modified[pos - 1])) {
                is_whole_word = false;
            }
            if (pos + ctx.identifier_name.length() < modified.length() && is_identifier_char(modified[pos + ctx.identifier_name.length()])) {
                is_whole_word = false;
            }

            if (is_whole_word) {
                modified.replace(pos, ctx.identifier_name.length(), "(" + value + ")");
                replacements++;
                pos += value.length() + 2;
            } else {
                pos++;
            }
        }

        result.success = replacements > 0;
        result.modified_code = std::move(modified);
        result.locations_modified = replacements;
        if (!result.success) {
            result.error_message = "No uses of variable '" + ctx.identifier_name + "' found";
        }
        return result;
    }

    static std::string capitalize(const std::string& str)
    {
        if (str.empty()) {
            return str;
        }
        std::string result = str;
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        return result;
    }

    static RefactorResult apply_generate_getter_setter(const std::string& code, const TestContext& ctx)
    {
        RefactorResult result;

        size_t marker_pos = code.find("// MARKER: Generate getter/setter");
        if (marker_pos == std::string::npos) {
            result.error_message = "Getter/setter marker not found";
            return result;
        }

        size_t decl_start = code.find('\n', marker_pos);
        if (decl_start == std::string::npos) {
            result.error_message = "Declaration start not found";
            return result;
        }
        decl_start += 1;
        size_t decl_end = code.find(';', decl_start);
        if (decl_end == std::string::npos) {
            result.error_message = "Declaration terminator not found";
            return result;
        }

        std::string getter_name = "get" + capitalize(ctx.identifier_name);
        std::string getter = "    " + ctx.type_decl + " " + getter_name + "() const {\n";
        getter += "        return " + ctx.identifier_name + ";\n";
        getter += "    }\n\n";

        std::string setter_name = "set" + capitalize(ctx.identifier_name);
        std::string setter = "    void " + setter_name + "(" + ctx.type_decl + " value) {\n";
        setter += "        " + ctx.identifier_name + " = value;\n";
        setter += "    }\n";

        size_t insert_pos = code.find('\n', decl_end);
        if (insert_pos == std::string::npos) {
            insert_pos = code.length();
        } else {
            insert_pos += 1;
        }

        std::string modified = code;
        modified.insert(insert_pos, "\n" + getter + setter);

        result.success = true;
        result.modified_code = std::move(modified);
        result.locations_modified = 2;
        return result;
    }
};

class TestVerifier {
public:
    static bool verify_refactoring_result(const RefactoringEngine::RefactorResult& result,
                                          const TestContext& ctx,
                                          std::string& error_message)
    {
        if (!result.success) {
            error_message = result.error_message;
            return false;
        }

        if (!verify_basic_syntax(result.modified_code)) {
            error_message = "Modified code has syntax errors";
            return false;
        }

        switch (ctx.operation) {
        case TestContext::RENAME:
            return verify_rename_result(result, ctx, error_message);
        case TestContext::EXTRACT_VAR:
            return verify_extract_var_result(result, error_message);
        case TestContext::SORT_INCLUDES:
            return verify_sort_includes_result(result, error_message);
        case TestContext::EXTRACT_FUNC:
            return verify_extract_func_result(result, error_message);
        case TestContext::INLINE_VAR:
            return verify_inline_var_result(result, ctx, error_message);
        case TestContext::GEN_GETTER_SETTER:
            return verify_getter_setter_result(result, ctx, error_message);
        default:
            return true;
        }
    }

private:
    static bool verify_basic_syntax(const std::string& code)
    {
        int brace_count = 0;
        int paren_count = 0;
        int bracket_count = 0;

        for (char c : code) {
            switch (c) {
            case '{':
                brace_count++;
                break;
            case '}':
                brace_count--;
                break;
            case '(':
                paren_count++;
                break;
            case ')':
                paren_count--;
                break;
            case '[':
                bracket_count++;
                break;
            case ']':
                bracket_count--;
                break;
            default:
                break;
            }
        }

        if (brace_count != 0 || paren_count != 0 || bracket_count != 0) {
            return false;
        }

        return code.find(';') != std::string::npos;
    }

    static bool verify_rename_result(const RefactoringEngine::RefactorResult& result,
                                     const TestContext& ctx,
                                     std::string& error_message)
    {
        if (result.modified_code.find(ctx.new_name) == std::string::npos) {
            error_message = "New identifier '" + ctx.new_name + "' not found";
            return false;
        }
        return true;
    }

    static bool verify_extract_var_result(const RefactoringEngine::RefactorResult& result, std::string& error_message)
    {
        if (result.modified_code.find("auto ") == std::string::npos) {
            error_message = "Extracted variable declaration not found";
            return false;
        }
        if (result.modified_code.find("extracted_") == std::string::npos) {
            error_message = "Extracted variable not properly named";
            return false;
        }
        return true;
    }

    static bool verify_sort_includes_result(const RefactoringEngine::RefactorResult& result, std::string& error_message)
    {
        if (result.modified_code.find("#include") == std::string::npos) {
            error_message = "No includes in result";
            return false;
        }
        size_t first_project = result.modified_code.find("#include \"");
        size_t first_system = result.modified_code.find("#include <");
        if (first_project != std::string::npos && first_system != std::string::npos && first_project < first_system) {
            error_message = "Includes not properly sorted (project before system)";
            return false;
        }
        return true;
    }

    static bool verify_extract_func_result(const RefactoringEngine::RefactorResult& result, std::string& error_message)
    {
        if (result.modified_code.find("extracted_function_") == std::string::npos) {
            error_message = "Extracted function not found";
            return false;
        }
        return true;
    }

    static bool verify_inline_var_result(const RefactoringEngine::RefactorResult& result,
                                         const TestContext& ctx,
                                         std::string& error_message)
    {
        std::string search = ctx.type_decl + " " + ctx.identifier_name;
        size_t pos = result.modified_code.find(search);
        if (pos != std::string::npos) {
            size_t eq_pos = result.modified_code.find('=', pos);
            size_t semi_pos = result.modified_code.find(';', eq_pos);
            if (eq_pos != std::string::npos && semi_pos != std::string::npos && semi_pos > eq_pos && semi_pos - eq_pos < 64) {
                error_message = "Variable declaration still present after inlining";
                return false;
            }
        }
        return true;
    }

    static std::string capitalize(const std::string& str)
    {
        if (str.empty()) {
            return str;
        }
        std::string result = str;
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        return result;
    }

    static bool verify_getter_setter_result(const RefactoringEngine::RefactorResult& result,
                                            const TestContext& ctx,
                                            std::string& error_message)
    {
        if (result.modified_code.find("get" + capitalize(ctx.identifier_name)) == std::string::npos) {
            error_message = "Getter not found";
            return false;
        }
        if (result.modified_code.find("set" + capitalize(ctx.identifier_name)) == std::string::npos) {
            error_message = "Setter not found";
            return false;
        }
        return true;
    }
};

class ParallelTestExecutor {
public:
    struct OpStats {
        std::atomic<uint64_t> passed{0};
        std::atomic<uint64_t> failed{0};
    };

    ParallelTestExecutor(size_t total_contexts, int threads)
        : total_contexts_(total_contexts), num_threads_(std::max(1, threads))
    {
        for (int i = 0; i < 6; ++i) {
            op_stats_.emplace_back(std::make_unique<OpStats>());
        }
    }

    void execute_all()
    {
        const auto start_time = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(num_threads_));

        for (int i = 0; i < num_threads_; ++i) {
            workers.emplace_back([this]() { worker_thread(); });
        }

        std::thread progress_thread([this, start_time]() {
            while (!all_workers_done_.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                print_progress(start_time);
            }
            print_progress(start_time);
            std::cout << "\n";
        });

        for (auto& worker : workers) {
            worker.join();
        }

        all_workers_done_ = true;
        progress_thread.join();

        const auto end_time = std::chrono::high_resolution_clock::now();
        total_time_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }

    void print_summary() const
    {
        const uint64_t passed = passed_count_.load();
        const uint64_t failed = failed_count_.load();
        const uint64_t total = total_contexts_;

        std::cout << "\n========== TEST SUMMARY ==========\n";
        std::cout << "Total tests:  " << total << "\n";
        std::cout << "Passed:       " << passed << "\n";
        std::cout << "Failed:       " << failed << "\n";
        std::cout << "Pass rate:    " << std::fixed << std::setprecision(2)
                  << (total > 0 ? (passed * 100.0 / static_cast<double>(total)) : 0.0) << "%\n";
        std::cout << "Elapsed:      " << total_time_ms_.count() << " ms\n";
        if (total_time_ms_.count() > 0) {
            std::cout << "Throughput:   " << (total * 1000.0 / static_cast<double>(total_time_ms_.count()))
                      << " contexts/s\n";
        }
        std::cout << "==================================\n";

        if (failed > 0) {
            std::lock_guard<std::mutex> lock(results_mutex_);
            const size_t display_count = std::min<size_t>(10, failed_contexts_.size());
            std::cout << "\nSample failed contexts (up to 10):\n";
            for (size_t i = 0; i < display_count; ++i) {
                const auto& ctx = failed_contexts_[i];
                std::cout << "  Context " << ctx.context_id << " op=" << operation_to_string(ctx.operation)
                          << " error=" << ctx.error_message << "\n";
            }
        }

        std::cout << "\nPer-operation statistics:\n";
        for (int i = 0; i < 6; ++i) {
            const auto p = op_stats_[static_cast<size_t>(i)]->passed.load();
            const auto f = op_stats_[static_cast<size_t>(i)]->failed.load();
            const auto t = p + f;
            std::cout << "  " << operation_to_string(static_cast<TestContext::RefactorOp>(i)) << ": " << p << "/" << t
                      << " passed";
            if (t > 0) {
                std::cout << " (" << std::fixed << std::setprecision(2)
                          << (p * 100.0 / static_cast<double>(t)) << "%)";
            }
            std::cout << "\n";
        }
    }

    uint64_t failed_count() const { return failed_count_.load(); }

private:
    static std::string operation_to_string(TestContext::RefactorOp op)
    {
        switch (op) {
        case TestContext::RENAME:
            return "Rename";
        case TestContext::EXTRACT_VAR:
            return "Extract Variable";
        case TestContext::SORT_INCLUDES:
            return "Sort Includes";
        case TestContext::EXTRACT_FUNC:
            return "Extract Function";
        case TestContext::INLINE_VAR:
            return "Inline Variable";
        case TestContext::GEN_GETTER_SETTER:
            return "Generate Getter/Setter";
        default:
            return "Unknown";
        }
    }

    static std::string deterministic_name(std::mt19937_64& rng)
    {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyz";
        std::uniform_int_distribution<int> char_dist(0, 25);
        std::uniform_int_distribution<int> len_dist(3, 15);

        const int len = len_dist(rng);
        std::string name;
        name.reserve(static_cast<size_t>(len));
        for (int i = 0; i < len; ++i) {
            name.push_back(chars[static_cast<size_t>(char_dist(rng))]);
        }
        return name;
    }

    static TestContext generate_context(uint64_t id)
    {
        static const std::vector<std::string> types = {
            "int", "float", "double", "char", "bool", "auto", "const int", "constexpr float", "std::string", "std::vector<int>"};

        static const std::vector<std::string> qualifiers = {"", "const", "volatile", "mutable", "constexpr"};
        static const std::vector<std::string> storage_classes = {"", "static", "extern", "thread_local"};
        static const std::vector<std::string> expression_types = {
            "arithmetic", "logical", "comparison", "bitwise", "function_call", "lambda"};

        std::mt19937_64 rng(42ULL ^ (id * 11400714819323198485ULL));

        std::uniform_int_distribution<int> type_dist(0, static_cast<int>(types.size() - 1));
        std::uniform_int_distribution<int> qual_dist(0, static_cast<int>(qualifiers.size() - 1));
        std::uniform_int_distribution<int> storage_dist(0, static_cast<int>(storage_classes.size() - 1));
        std::uniform_int_distribution<int> op_dist(0, 5);
        std::uniform_int_distribution<int> scope_dist(0, 5);
        std::uniform_int_distribution<int> complexity_dist(1, 10);
        std::uniform_int_distribution<int> include_dist(0, 50);
        std::uniform_int_distribution<int> file_dist(1, 100);
        std::uniform_int_distribution<int> bool_dist(0, 1);
        std::uniform_int_distribution<int> depth_dist(1, 10);

        TestContext ctx;
        ctx.context_id = id;
        ctx.type_decl = types[static_cast<size_t>(type_dist(rng))];
        ctx.qualifiers = qualifiers[static_cast<size_t>(qual_dist(rng))];
        ctx.storage_class = storage_classes[static_cast<size_t>(storage_dist(rng))];
        ctx.scope_level = scope_dist(rng);
        ctx.is_pointer = bool_dist(rng) != 0;
        ctx.is_array = bool_dist(rng) != 0;
        ctx.template_params = (bool_dist(rng) == 0) ? 0 : ((bool_dist(rng) == 0) ? 1 : 2);
        ctx.identifier_name = deterministic_name(rng);
        ctx.new_name = deterministic_name(rng);
        ctx.operation = static_cast<TestContext::RefactorOp>(op_dist(rng));
        ctx.cross_file_count = file_dist(rng);
        ctx.has_collisions = bool_dist(rng) != 0;
        ctx.complexity = complexity_dist(rng);
        ctx.has_side_effects = bool_dist(rng) != 0;
        ctx.include_count = include_dist(rng);
        ctx.has_circular_deps = bool_dist(rng) != 0;
        ctx.file_count = file_dist(rng);
        ctx.dir_depth = depth_dist(rng);
        ctx.expression_type = expression_types[static_cast<size_t>(op_dist(rng)) % expression_types.size()];
        ctx.expression_depth = complexity_dist(rng);
        ctx.symbol_type = "variable";
        ctx.build_system = (bool_dist(rng) != 0) ? "cmake" : "ninja";
        return ctx;
    }

    bool execute_single_test(TestContext& ctx)
    {
        try {
            std::string code = CodeGenerator::generate_test_code(ctx);
            auto result = RefactoringEngine::apply_refactoring(ctx.operation, code, ctx);

            std::string error_message;
            bool success = TestVerifier::verify_refactoring_result(result, ctx, error_message);
            if (!success) {
                ctx.error_message = std::move(error_message);
            }
            return success;
        } catch (const std::exception& e) {
            ctx.error_message = std::string("Exception: ") + e.what();
            return false;
        } catch (...) {
            ctx.error_message = "Unknown exception";
            return false;
        }
    }

    void worker_thread()
    {
        while (true) {
            const uint64_t idx = next_context_index_.fetch_add(1);
            if (idx >= total_contexts_) {
                break;
            }

            TestContext ctx = generate_context(idx);

            const auto start = std::chrono::high_resolution_clock::now();
            const bool result = execute_single_test(ctx);
            const auto end = std::chrono::high_resolution_clock::now();

            ctx.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            ctx.passed = result;

            if (result) {
                passed_count_.fetch_add(1);
                op_stats_[static_cast<size_t>(ctx.operation)]->passed.fetch_add(1);
            } else {
                failed_count_.fetch_add(1);
                op_stats_[static_cast<size_t>(ctx.operation)]->failed.fetch_add(1);
                std::lock_guard<std::mutex> lock(results_mutex_);
                if (failed_contexts_.size() < 1000) {
                    failed_contexts_.push_back(std::move(ctx));
                }
            }
        }
    }

    void print_progress(std::chrono::high_resolution_clock::time_point start_time) const
    {
        const uint64_t processed = std::min(next_context_index_.load(), total_contexts_);
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_time);
        const double elapsed_sec = static_cast<double>(std::max<int64_t>(1, elapsed.count()));
        const double rate = processed / elapsed_sec;
        const uint64_t remaining = (rate > 0.0) ? static_cast<uint64_t>((total_contexts_ - processed) / rate) : 0ULL;

        std::cout << "\rProgress: " << processed << "/" << total_contexts_ << " ("
                  << (total_contexts_ > 0 ? (processed * 100 / total_contexts_) : 0) << "%)"
                  << " | Passed: " << passed_count_.load() << " | Failed: " << failed_count_.load()
                  << " | Rate: " << static_cast<int>(rate) << " ctx/s"
                  << " | ETA: " << remaining << "s    " << std::flush;
    }

    size_t total_contexts_ = 0;
    int num_threads_ = 1;

    std::atomic<uint64_t> next_context_index_{0};
    std::atomic<uint64_t> passed_count_{0};
    std::atomic<uint64_t> failed_count_{0};
    std::atomic<bool> all_workers_done_{false};

    std::vector<std::unique_ptr<OpStats>> op_stats_;

    mutable std::mutex results_mutex_;
    std::vector<TestContext> failed_contexts_;

    std::chrono::milliseconds total_time_ms_{0};
};

int main(int argc, char* argv[])
{
    std::cout << "=== RawrXD Refactoring Engine - 1M Context Test Suite ===\n\n";

    size_t num_contexts = 1000000;
    int num_threads = static_cast<int>(std::thread::hardware_concurrency());
    if (num_threads <= 0) {
        num_threads = 8;
    }

    if (argc > 1) {
        try {
            num_contexts = static_cast<size_t>(std::stoull(argv[1]));
        } catch (...) {
            std::cerr << "Invalid context count argument\n";
            return 2;
        }
    }
    if (argc > 2) {
        try {
            num_threads = std::max(1, std::stoi(argv[2]));
        } catch (...) {
            std::cerr << "Invalid thread count argument\n";
            return 2;
        }
    }

    std::cout << "Configuration:\n";
    std::cout << "  Contexts:   " << num_contexts << "\n";
    std::cout << "  Threads:    " << num_threads << "\n";
    std::cout << "  Generation: deterministic per-context\n";
    std::cout << "  Memory:     bounded (streamed contexts, no 1M in-memory vector)\n\n";

    std::cout << "Executing tests...\n";
    ParallelTestExecutor executor(num_contexts, num_threads);
    executor.execute_all();
    executor.print_summary();

    std::cout << "\n=== Context test run completed ===\n";

    return executor.failed_count() == 0 ? 0 : 1;
}

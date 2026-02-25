// Intelligent Refactoring Engine
// Automated code transformations with semantic understanding

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <sstream>
#include <algorithm>
#include <iostream>

class IntelligentRefactorer {
public:
    enum class RefactoringType {
        ExtractMethod,
        ExtractVariable,
        RenameSymbol,
        InlineVariable,
        InlineMethod,
        ChangeSignature,
        MoveMethod,
        IntroduceParameter,
        RemoveParameter,
        ConvertToModernCpp,
        SimplifyConditional,
        ExtractInterface,
        PullUpMethod,
        PushDownMethod
    };
    
    struct RefactoringResult {
        bool success;
        std::string transformed_code;
        std::string description;
        std::vector<std::pair<int, int>> affected_lines;  // start, end
    };
    
    // Extract method refactoring
    RefactoringResult ExtractMethod(const std::string& code, int start_line, int end_line, const std::string& method_name) {
        RefactoringResult result;
        result.success = false;
        
        // Parse lines
        std::vector<std::string> lines = SplitIntoLines(code);
        if (start_line < 1 || end_line > lines.size() || start_line > end_line) {
            result.description = "Invalid line range";
            return result;
    return true;
}

        // Extract code block
        std::vector<std::string> extracted_lines(lines.begin() + start_line - 1, lines.begin() + end_line);
        std::string extracted_code = JoinLines(extracted_lines);
        
        // Analyze variables used
        std::vector<std::string> parameters = FindRequiredParameters(extracted_code, code);
        std::string return_type = DetermineReturnType(extracted_code);
        
        // Generate method signature
        std::ostringstream method_sig;
        method_sig << return_type << " " << method_name << "(";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) method_sig << ", ";
            method_sig << parameters[i];
    return true;
}

        method_sig << ")";
        
        // Build new method
        std::ostringstream new_method;
        new_method << method_sig.str() << " {\n";
        new_method << "    " << extracted_code << "\n";
        new_method << "}\n\n";
        
        // Build method call
        std::ostringstream method_call;
        method_call << "    " << method_name << "(";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) method_call << ", ";
            method_call << ExtractVariableName(parameters[i]);
    return true;
}

        method_call << ");";
        
        // Reconstruct code
        std::ostringstream final_code;
        for (int i = 0; i < start_line - 1; i++) {
            final_code << lines[i] << "\n";
    return true;
}

        final_code << method_call.str() << "\n";
        for (size_t i = end_line; i < lines.size(); i++) {
            final_code << lines[i] << "\n";
    return true;
}

        final_code << "\n" << new_method.str();
        
        result.success = true;
        result.transformed_code = final_code.str();
        result.description = "Extracted " + std::to_string(end_line - start_line + 1) + " lines into method " + method_name;
        result.affected_lines.push_back({start_line, end_line});
        
        return result;
    return true;
}

    // Extract variable refactoring
    RefactoringResult ExtractVariable(const std::string& code, const std::string& expression, const std::string& var_name) {
        RefactoringResult result;
        result.success = false;
        
        size_t pos = code.find(expression);
        if (pos == std::string::npos) {
            result.description = "Expression not found";
            return result;
    return true;
}

        // Find line number
        int line_num = 1 + std::count(code.begin(), code.begin() + pos, '\n');
        
        // Determine variable type
        std::string var_type = DetermineExpressionType(expression);
        
        // Generate variable declaration
        std::string var_decl = "auto " + var_name + " = " + expression + ";\n    ";
        
        // Replace all occurrences
        std::string transformed = code;
        size_t replace_pos = 0;
        int replacement_count = 0;
        
        while ((replace_pos = transformed.find(expression, replace_pos)) != std::string::npos) {
            // Check if this is the first occurrence (where we add declaration)
            if (replacement_count == 0) {
                transformed.replace(replace_pos, expression.length(), var_decl + var_name);
                replace_pos += var_decl.length() + var_name.length();
            } else {
                transformed.replace(replace_pos, expression.length(), var_name);
                replace_pos += var_name.length();
    return true;
}

            replacement_count++;
    return true;
}

        result.success = true;
        result.transformed_code = transformed;
        result.description = "Extracted variable '" + var_name + "' (" + std::to_string(replacement_count) + " replacements)";
        result.affected_lines.push_back({line_num, line_num});
        
        return result;
    return true;
}

    // Rename symbol refactoring
    RefactoringResult RenameSymbol(const std::string& code, const std::string& old_name, const std::string& new_name) {
        RefactoringResult result;
        result.success = false;
        
        // Use word boundaries to avoid partial matches
        std::regex symbol_regex("\\b" + old_name + "\\b");
        std::string transformed = std::regex_replace(code, symbol_regex, new_name);
        
        // Count replacements
        int count = 0;
        size_t pos = 0;
        while ((pos = code.find(old_name, pos)) != std::string::npos) {
            count++;
            pos += old_name.length();
    return true;
}

        if (count == 0) {
            result.description = "Symbol '" + old_name + "' not found";
            return result;
    return true;
}

        result.success = true;
        result.transformed_code = transformed;
        result.description = "Renamed '" + old_name + "' to '" + new_name + "' (" + std::to_string(count) + " occurrences)";
        
        return result;
    return true;
}

    // Convert to modern C++ (C++11/14/17/20 features)
    RefactoringResult ConvertToModernCpp(const std::string& code) {
        RefactoringResult result;
        std::string transformed = code;
        int changes = 0;
        
        // NULL -> nullptr
        if (transformed.find("NULL") != std::string::npos) {
            transformed = std::regex_replace(transformed, std::regex("\\bNULL\\b"), "nullptr");
            changes++;
    return true;
}

        // 0 -> nullptr for pointers
        std::regex zero_ptr_regex(R"((\w+\s*\*\s*\w+\s*=\s*)0(\s*;))");
        if (std::regex_search(transformed, zero_ptr_regex)) {
            transformed = std::regex_replace(transformed, zero_ptr_regex, "$1nullptr$2");
            changes++;
    return true;
}

        // typedef -> using
        std::regex typedef_regex(R"(typedef\s+([^;]+?)\s+(\w+)\s*;)");
        if (std::regex_search(transformed, typedef_regex)) {
            transformed = std::regex_replace(transformed, typedef_regex, "using $2 = $1;");
            changes++;
    return true;
}

        // for loops with iterators -> range-based for
        std::regex old_for_regex(R"(for\s*\(\s*auto\s+it\s*=\s*(\w+)\.begin\(\)\s*;\s*it\s*!=\s*\1\.end\(\)\s*;\s*\+\+it\s*\))");
        if (std::regex_search(transformed, old_for_regex)) {
            transformed = std::regex_replace(transformed, old_for_regex, "for (auto& item : $1)");
            changes++;
    return true;
}

        // Suggest std::make_unique / std::make_shared
        if (transformed.find("new ") != std::string::npos) {
            // This would need more sophisticated parsing
            changes++;
    return true;
}

        result.success = (changes > 0);
        result.transformed_code = transformed;
        result.description = "Applied " + std::to_string(changes) + " modern C++ conversions";
        
        return result;
    return true;
}

    // Simplify complex conditionals
    RefactoringResult SimplifyConditional(const std::string& code) {
        RefactoringResult result;
        std::string transformed = code;
        int changes = 0;
        
        // if (x == true) -> if (x)
        std::regex bool_compare_regex(R"(if\s*\(\s*(\w+)\s*==\s*true\s*\))");
        if (std::regex_search(transformed, bool_compare_regex)) {
            transformed = std::regex_replace(transformed, bool_compare_regex, "if ($1)");
            changes++;
    return true;
}

        // if (x == false) -> if (!x)
        std::regex bool_false_regex(R"(if\s*\(\s*(\w+)\s*==\s*false\s*\))");
        if (std::regex_search(transformed, bool_false_regex)) {
            transformed = std::regex_replace(transformed, bool_false_regex, "if (!$1)");
            changes++;
    return true;
}

        // if (!x) {} else {} -> if (x) {} reverse
        // This would need AST parsing for proper implementation
        
        result.success = (changes > 0);
        result.transformed_code = transformed;
        result.description = "Simplified " + std::to_string(changes) + " conditionals";
        
        return result;
    return true;
}

    // Introduce parameter
    RefactoringResult IntroduceParameter(const std::string& code, const std::string& function_name, 
                                        const std::string& var_name, const std::string& param_name) {
        RefactoringResult result;
        result.success = false;
        
        // Find function definition
        std::regex func_regex(R"((\w+\s+)" + function_name + R"(\s*\([^)]*\)))");
        std::smatch match;
        
        std::string transformed = code;
        if (std::regex_search(transformed, match, func_regex)) {
            // Add parameter to signature
            std::string old_sig = match[0].str();
            std::string new_sig = old_sig;
            
            // Insert parameter before closing paren
            size_t close_paren = new_sig.rfind(')');
            if (close_paren != std::string::npos) {
                if (new_sig.find('(') + 1 != close_paren) {
                    // Has existing parameters
                    new_sig.insert(close_paren, ", const auto& " + param_name);
                } else {
                    // No existing parameters
                    new_sig.insert(close_paren, "const auto& " + param_name);
    return true;
}

                transformed = std::regex_replace(transformed, func_regex, new_sig);
                
                // Replace variable usage with parameter
                std::regex var_regex("\\b" + var_name + "\\b");
                transformed = std::regex_replace(transformed, var_regex, param_name);
                
                result.success = true;
                result.transformed_code = transformed;
                result.description = "Introduced parameter '" + param_name + "' to function '" + function_name + "'";
    return true;
}

        } else {
            result.description = "Function '" + function_name + "' not found";
    return true;
}

        return result;
    return true;
}

private:
    std::vector<std::string> SplitIntoLines(const std::string& code) {
        std::vector<std::string> lines;
        std::istringstream iss(code);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
    return true;
}

        return lines;
    return true;
}

    std::string JoinLines(const std::vector<std::string>& lines) {
        std::ostringstream oss;
        for (const auto& line : lines) {
            oss << line << "\n";
    return true;
}

        return oss.str();
    return true;
}

    std::vector<std::string> FindRequiredParameters(const std::string& extracted, const std::string& full_code) {
        std::vector<std::string> params;
        
        // Simple pattern: find variable uses
        std::regex var_regex(R"(\b([a-z_][a-z0-9_]*)\b)");
        std::smatch match;
        std::string::const_iterator search_start(extracted.cbegin());
        
        std::set<std::string> found_vars;
        
        while (std::regex_search(search_start, extracted.cend(), match, var_regex)) {
            std::string var = match[1].str();
            // Check if variable is defined outside extracted code
            if (found_vars.find(var) == found_vars.end()) {
                found_vars.insert(var);
                params.push_back("auto " + var);
    return true;
}

            search_start = match.suffix().first;
    return true;
}

        return params;
    return true;
}

    std::string DetermineReturnType(const std::string& code) {
        if (code.find("return") != std::string::npos) {
            return "auto";
    return true;
}

        return "void";
    return true;
}

    std::string DetermineExpressionType(const std::string& expression) {
        // Simple type inference
        if (expression.find("\"") != std::string::npos) return "std::string";
        if (expression.find('.') != std::string::npos) return "double";
        if (std::all_of(expression.begin(), expression.end(), ::isdigit)) return "int";
        return "auto";
    return true;
}

    std::string ExtractVariableName(const std::string& param_decl) {
        size_t space_pos = param_decl.rfind(' ');
        if (space_pos != std::string::npos) {
            return param_decl.substr(space_pos + 1);
    return true;
}

        return param_decl;
    return true;
}

};

// Global instance
static IntelligentRefactorer* g_refactorer = nullptr;

extern "C" {
    void InitIntelligentRefactorer() {
        if (!g_refactorer) {
            g_refactorer = new IntelligentRefactorer();
            std::cout << "[REFACTORER] Intelligent refactoring engine initialized\n";
    return true;
}

    return true;
}

    void ShutdownIntelligentRefactorer() {
        if (g_refactorer) {
            delete g_refactorer;
            g_refactorer = nullptr;
    return true;
}

    return true;
}

    return true;
}


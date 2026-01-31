
#include <iostream>
#include <string>
#include <vector>

class SimpleCompiler {
private:
    std::vector<std::string> tokens;
    
public:
    std::vector<std::string> tokenize(const std::string& source) {
        std::vector<std::string> result;
        std::string token;
        for (char c : source) {
            if (c == ' ') {
                if (!token.empty()) {
                    result.push_back(token);
                    token.clear();
                }
            } else {
                token += c;
            }
        }
        if (!token.empty()) {
            result.push_back(token);
        }
        return result;
    }
    
    std::string compile(const std::string& source) {
        auto tokens = tokenize(source);
        return "compiled: " + std::to_string(tokens.size()) + " tokens";
    }
};

int main() {
    SimpleCompiler compiler;
    std::string result = compiler.compile("test source code");
    std::cout << "Result: " << result << std::endl;
    return 0;
}

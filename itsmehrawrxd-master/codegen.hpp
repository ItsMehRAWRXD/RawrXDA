// Filename: codegen.hpp
#include <string>
#include <sstream>
#include <map>

// Forward declarations
class Node;

class CodeGenerator {
public:
    std::string generate(Node* ast_root);

private:
    std::stringstream assembly_output;
    std::map<std::string, int> stack_offsets;
    int stack_pointer_offset = 0;

    void emit(const std::string& instruction);
    void generateNode(Node* node);
    int getStackOffset(const std::string& name);
    void newStackVariable(const std::string& name);
};

// Filename: codegen.cpp
#include "codegen.hpp"
#include "ast.hpp" // Assumes a definition of the Node and its subclasses

void CodeGenerator::emit(const std::string& instruction) {
    assembly_output << "    " << instruction << "\n";
}

int CodeGenerator::getStackOffset(const std::string& name) {
    return stack_offsets.at(name);
}

void CodeGenerator::newStackVariable(const std::string& name) {
    stack_pointer_offset -= 8; // Each variable is a qword
    stack_offsets[name] = stack_pointer_offset;
}

void CodeGenerator::generateNode(Node* node) {
    if (!node) return;

    if (auto func_node = dynamic_cast<FunctionNode*>(node)) {
        assembly_output << ".globl " << func_node->name << "\n";
        assembly_output << func_node->name << ":\n";
        emit("pushq   %rbp");
        emit("movq    %rsp, %rbp");
        generateNode(func_node->body);
        emit("movq    $0, %rax");
        emit("leave");
        emit("ret");
    } else if (auto let_node = dynamic_cast<LetNode*>(node)) {
        newStackVariable(let_node->name);
        generateNode(let_node->expression);
        emit("movq    %rax, " + std::to_string(getStackOffset(let_node->name)) + "(%rbp)");
        if (let_node->next) {
            generateNode(let_node->next);
        }
    } else if (auto print_node = dynamic_cast<PrintNode*>(node)) {
        emit("movq    " + std::to_string(getStackOffset(print_node->variable_name)) + "(%rbp), %rdi");
        emit("callq   _print_integer");
        if (print_node->next) {
            generateNode(print_node->next);
        }
    } else if (auto bin_op_node = dynamic_cast<BinaryOpNode*>(node)) {
        if (bin_op_node->op == "+") {
            generateNode(bin_op_node->left);
            emit("pushq   %rax");
            generateNode(bin_op_node->right);
            emit("popq    %rcx");
            emit("addq    %rcx, %rax");
        }
        // ... Other operators ...
    } else if (auto literal_node = dynamic_cast<LiteralNode*>(node)) {
        emit("movq    $" + std::to_string(literal_node->value) + ", %rax");
    }
}

std::string CodeGenerator::generate(Node* ast_root) {
    assembly_output.str("");
    assembly_output.clear();
    stack_offsets.clear();
    stack_pointer_offset = 0;

    assembly_output << ".text\n";
    generateNode(ast_root);
    
    return assembly_output.str();
}

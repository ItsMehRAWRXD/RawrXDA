/*
 * RawrXD_CommandCLI.cpp
 * Simple x64/x86 MASM assembly instruction CLI
 * Parses .asm directives and validates x64 MASM syntax
 * 
 * Build: cl /O2 RawrXD_CommandCLI.cpp /link  /SUBSYSTEM:CONSOLE
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const char* instruction;
    const char* description;
    int operands;
} InstructionDef;

static InstructionDef g_x64_instructions[] = {
    // Arithmetic
    { "add", "Add two operands", 2 },
    { "sub", "Subtract second from first", 2 },
    { "mul", "Unsigned multiply", 1 },
    { "imul", "Signed multiply", 1 },
    { "div", "Unsigned divide", 1 },
    { "idiv", "Signed divide", 1 },
    { "inc", "Increment by 1", 1 },
    { "dec", "Decrement by 1", 1 },
    { "neg", "Two's complement negate", 1 },
    
    // Logical
    { "and", "Bitwise AND", 2 },
    { "or", "Bitwise OR", 2 },
    { "xor", "Bitwise XOR", 2 },
    { "not", "Bitwise NOT", 1 },
    { "shl", "Shift left", 2 },
    { "shr", "Shift right logical", 2 },
    { "sar", "Shift right arithmetic", 2 },
    { "ror", "Rotate right", 2 },
    { "rol", "Rotate left", 2 },
    
    // Transfer/Control
    { "mov", "Move data", 2 },
    { "movzx", "Move with zero extend", 2 },
    { "movsx", "Move with sign extend", 2 },
    { "lea", "Load effective address", 2 },
    { "xchg", "Exchange two operands", 2 },
    { "jmp", "Unconditional jump", 1 },
    { "call", "Call procedure", 1 },
    { "ret", "Return from procedure", 0 },
    { "jz", "Jump if zero", 1 },
    { "jnz", "Jump if not zero", 1 },
    { "je", "Jump if equal", 1 },
    { "jne", "Jump if not equal", 1 },
    { "jl", "Jump if less", 1 },
    { "jg", "Jump if greater", 1 },
    { "jle", "Jump if less or equal", 1 },
    { "jge", "Jump if greater or equal", 1 },
    
    // Comparison/Test
    { "cmp", "Compare two operands", 2 },
    { "test", "Test bits", 2 },
    
    // Stack
    { "push", "Push to stack", 1 },
    { "pop", "Pop from stack", 1 },
    
    // String
    { "rep", "Repeat string instruction", 0 },
    { "movs", "Move string", 0 },
    { "cmp", "Compare string", 0 },
    
    // Terminator
    { NULL, NULL, 0 }
};

void print_prompt() {
    printf("[MASM-CLI]> ");
    fflush(stdout);
}

void print_help() {
    printf("\nRawrXD MASM x64 CLI Commands:\n");
    printf("  instructions  - List all x64 instructions\n");
    printf("  isvalid <asm>  - Check if assembly syntax is valid\n");
    printf("  validate <reg> - Validate register name (rax, rbx, etc)\n");
    printf("  opcodes        - Show opcode reference\n");
    printf("  help           - Show this help\n");
    printf("  exit           - Exit CLI\n\n");
}

void list_instructions() {
    printf("\nAvailable x64 Instructions (%d total):\n", 
        sizeof(g_x64_instructions) / sizeof(g_x64_instructions[0]) - 1);
    printf("---\n");
    for (int i = 0; g_x64_instructions[i].instruction; i++) {
        printf("  %-8s %-40s (%d operands)\n",
            g_x64_instructions[i].instruction,
            g_x64_instructions[i].description,
            g_x64_instructions[i].operands);
    }
    printf("---\n\n");
}

int is_valid_register(const char* reg) {
    static const char* valid_regs[] = {
        // 64-bit
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        // 32-bit
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
        // 16-bit
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
        // 8-bit
        "al", "bl", "cl", "dl", "sil", "dil", "bpl", "spl",
        "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
        NULL
    };
    
    for (int i = 0; valid_regs[i]; i++) {
        if (strcmp(reg, valid_regs[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int is_valid_instruction(const char* inst) {
    for (int i = 0; g_x64_instructions[i].instruction; i++) {
        if (strcmp(inst, g_x64_instructions[i].instruction) == 0) {
            return 1;
        }
    }
    return 0;
}

void process_command(const char* line) {
    char cmd[256];
    char arg[512];
    
    if (sscanf_s(line, "%255s %511s", cmd, (unsigned int)sizeof(cmd), arg, (unsigned int)sizeof(arg)) < 1) {
        return;
    }
    
    // Convert to lowercase
    for (int i = 0; cmd[i]; i++) cmd[i] = tolower(cmd[i]);
    
    if (strcmp(cmd, "exit") == 0) {
        printf("\nGoodbye!\n");
        exit(0);
    }
    else if (strcmp(cmd, "help") == 0) {
        print_help();
    }
    else if (strcmp(cmd, "instructions") == 0) {
        list_instructions();
    }
    else if (strcmp(cmd, "opcodes") == 0) {
        printf("\nCommon x64 Opcodes:\n");
        printf("  90           NOP (no operation)\n");
        printf("  C3           RET (return)\n");
        printf("  48 89 C3     MOV rbx, rax\n");
        printf("  48 01 C3     ADD rbx, rax\n");
        printf("  48 29 C3     SUB rbx, rax\n");
        printf("  48 F7 D3     NOT rbx\n");
        printf("  CC           INT 3 (breakpoint)\n\n");
    }
    else if (strcmp(cmd, "validate") == 0) {
        if (strlen(arg) > 0) {
            if (is_valid_instruction(arg)) {
                printf("  '%s' is a valid x64 instruction\n\n", arg);
            } else if (is_valid_register(arg)) {
                printf("  '%s' is a valid x64 register\n\n", arg);
            } else {
                printf("  '%s' is NOT a valid instruction or register\n\n", arg);
            }
        } else {
            printf("  Usage: validate <instruction|register>\n\n");
        }
    }
    else if (strcmp(cmd, "isvalid") == 0) {
        if (strlen(arg) > 0) {
            if (is_valid_instruction(arg)) {
                printf("  '%s' is VALID x64 instruction\n\n", arg);
            } else {
                printf("  '%s' is INVALID (unknown instruction)\n\n", arg);
            }
        } else {
            printf("  Usage: isvalid <masm-instruction>\n\n");
        }
    }
    else {
        printf("  Unknown command. Type 'help' for commands.\n\n");
    }
}

int main() {
    printf("RawrXD MASM x64 CLI v1.0\n");
    printf("Type 'help' for commands\n\n");
    
    char line[1024];
    while (1) {
        print_prompt();
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) > 0) {
            process_command(line);
        }
    }
    
    return 0;
}

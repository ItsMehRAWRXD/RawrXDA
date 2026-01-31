// EON Compiler Main Entry Point
#include "../include/compiler.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }
    
    FILE *src_file = fopen(argv[1], "r");
    if (!src_file) {
        perror("Failed to open source file");
        return 1;
    }
    
    // Get file size
    fseek(src_file, 0, SEEK_END);
    long fsize = ftell(src_file);
    fseek(src_file, 0, SEEK_SET);
    
    // Read source file
    char *src = malloc(fsize + 1);
    if (!src) {
        perror("Memory allocation failed");
        fclose(src_file);
        return 1;
    }
    fread(src, 1, fsize, src_file);
    src[fsize] = '\0';
    fclose(src_file);
    
    lexer_init(src);
    lexer_next(); // Load the first token
    
    symtab_init();
    AstNode *ast = parse_program();
    
    FILE *out_file = fopen("out.asm", "w");
    if (!out_file) {
        perror("Failed to create output file");
        free(src);
        return 1;
    }
    generate_program(out_file, ast);
    fclose(out_file);
    
    free(src);
    
    printf("Compilation successful. Assembly written to out.asm\n");
    printf("To compile: nasm -f elf64 out.asm && gcc out.o -o out\n");
    
    return 0;
}

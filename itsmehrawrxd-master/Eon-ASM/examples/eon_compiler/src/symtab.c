// EON Compiler Symbol Table Implementation
#include "../include/compiler.h"

// Global symbol table
SymbolTable *current_scope;
EonType *int_type;
EonType *void_type;

void symtab_init() {
    current_scope = malloc(sizeof(SymbolTable));
    current_scope->symbols = malloc(sizeof(Symbol) * 100);
    current_scope->count = 0;
    current_scope->capacity = 100;
    current_scope->parent = NULL;
    
    // Initialize built-in types
    int_type = new_type("int", 8);
    void_type = new_type("void", 0);
}

void symtab_push_scope() {
    SymbolTable *new_scope = malloc(sizeof(SymbolTable));
    new_scope->symbols = malloc(sizeof(Symbol) * 100);
    new_scope->count = 0;
    new_scope->capacity = 100;
    new_scope->parent = current_scope;
    current_scope = new_scope;
}

void symtab_pop_scope() {
    if (current_scope->parent) {
        SymbolTable *old_scope = current_scope;
        current_scope = current_scope->parent;
        free(old_scope->symbols);
        free(old_scope);
    }
}

Symbol *symtab_put(char *name, EonType *type) {
    // Check if symbol already exists in current scope
    for (int i = 0; i < current_scope->count; i++) {
        if (strcmp(current_scope->symbols[i].name, name) == 0) {
            fprintf(stderr, "Error: Symbol '%s' already declared in this scope\n", name);
            exit(1);
        }
    }
    
    // Add new symbol
    if (current_scope->count >= current_scope->capacity) {
        current_scope->capacity *= 2;
        current_scope->symbols = realloc(current_scope->symbols, 
                                        sizeof(Symbol) * current_scope->capacity);
    }
    
    Symbol *symbol = &current_scope->symbols[current_scope->count++];
    symbol->name = malloc(strlen(name) + 1);
    strcpy(symbol->name, name);
    symbol->type = type;
    symbol->offset = 0; // Will be calculated later
    symbol->is_global = (current_scope->parent == NULL);
    symbol->ast_node = NULL;
    
    return symbol;
}

Symbol *symtab_get(char *name) {
    SymbolTable *scope = current_scope;
    
    while (scope) {
        for (int i = 0; i < scope->count; i++) {
            if (strcmp(scope->symbols[i].name, name) == 0) {
                return &scope->symbols[i];
            }
        }
        scope = scope->parent;
    }
    
    return NULL;
}

Symbol *symtab_get_local(char *name) {
    for (int i = 0; i < current_scope->count; i++) {
        if (strcmp(current_scope->symbols[i].name, name) == 0) {
            return &current_scope->symbols[i];
        }
    }
    return NULL;
}

EonType *new_type(char *name, int size) {
    EonType *type = malloc(sizeof(EonType));
    type->name = malloc(strlen(name) + 1);
    strcpy(type->name, name);
    type->size = size;
    type->base_type = NULL;
    type->fields = NULL;
    type->field_names = NULL;
    type->field_count = 0;
    return type;
}

EonType *new_pointer_type(EonType *base_type) {
    EonType *type = malloc(sizeof(EonType));
    type->name = malloc(strlen(base_type->name) + 2);
    sprintf(type->name, "*%s", base_type->name);
    type->size = 8; // Pointers are 8 bytes on x86-64
    type->base_type = base_type;
    type->fields = NULL;
    type->field_names = NULL;
    type->field_count = 0;
    return type;
}

EonType *new_struct_type(char *name, AstNode *fields) {
    EonType *type = malloc(sizeof(EonType));
    type->name = malloc(strlen(name) + 1);
    strcpy(type->name, name);
    type->size = 0;
    type->base_type = NULL;
    
    // Count fields
    int field_count = 0;
    AstNode *field = fields;
    while (field) {
        field_count++;
        field = field->next;
    }
    
    type->field_count = field_count;
    type->fields = malloc(sizeof(EonType*) * field_count);
    type->field_names = malloc(sizeof(char*) * field_count);
    
    // Process fields
    field = fields;
    int offset = 0;
    for (int i = 0; i < field_count; i++) {
        type->field_names[i] = malloc(strlen(field->data.struct_field.name) + 1);
        strcpy(type->field_names[i], field->data.struct_field.name);
        type->fields[i] = get_type_from_ast(field->data.struct_field.type);
        offset += type->fields[i]->size;
        field = field->next;
    }
    
    type->size = offset;
    return type;
}

EonType *get_type_from_ast(AstNode *node) {
    if (!node) return void_type;
    
    if (node->type == AST_IDENTIFIER) {
        if (strcmp(node->data.name, "int") == 0) {
            return int_type;
        }
        // Add more type lookups as needed
    }
    
    return int_type; // Default to int
}

void type_check_ast(AstNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VAR_DECL: {
            Symbol *sym = symtab_put(node->data.var_decl.name, int_type);
            if (node->data.var_decl.init_expr) {
                type_check_ast(node->data.var_decl.init_expr);
                if (node->data.var_decl.init_expr->type_info != int_type) {
                    fprintf(stderr, "Error: Type mismatch for variable '%s'\n", node->data.var_decl.name);
                    exit(1);
                }
            }
            break;
        }
        case AST_OP:
            type_check_ast(node->left);
            type_check_ast(node->right);
            if (node->left->type_info != int_type || node->right->type_info != int_type) {
                fprintf(stderr, "Error: Operator can only be used with integers\n");
                exit(1);
            }
            node->type_info = int_type;
            break;
        case AST_NUMBER:
            node->type_info = int_type;
            break;
        case AST_IDENTIFIER: {
            Symbol *sym = symtab_get(node->data.name);
            if (!sym) {
                fprintf(stderr, "Error: Undefined variable '%s'\n", node->data.name);
                exit(1);
            }
            node->type_info = sym->type;
            break;
        }
        // Add more cases for loops, function calls, etc.
        default:
            break;
    }
    type_check_ast(node->next);
}

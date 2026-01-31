// EON Compiler Parser Implementation
#include "../include/compiler.h"

AstNode *new_node(AstNodeType type) {
    AstNode *node = malloc(sizeof(AstNode));
    node->type = type;
    node->left = NULL;
    node->right = NULL;
    node->next = NULL;
    node->type_info = NULL;
    return node;
}

AstNode *parse_program() {
    AstNode *root = new_node(AST_PROGRAM);
    root->data.program.body = parse_top_level_declaration();
    
    // After AST is built, perform semantic analysis
    type_check_ast(root);
    return root;
}

AstNode *parse_top_level_declaration() {
    if (current_token.type == TOKEN_EOF) {
        return NULL;
    }
    
    AstNode *node = NULL;
    
    if (current_token.type == TOKEN_DEF) {
        node = parse_function_definition();
    } else if (current_token.type == TOKEN_STRUCT) {
        node = parse_struct_definition();
    } else {
        node = parse_statement();
    }
    
    if (current_token.type == TOKEN_SEMICOLON) {
        advance_token();
    }
    
    node->next = parse_top_level_declaration();
    return node;
}

AstNode *parse_function_definition() {
    AstNode *node = new_node(AST_FUNCTION_DEFINITION);
    
    expect(TOKEN_DEF);
    node->data.func_def.name = current_token.value;
    expect(TOKEN_IDENTIFIER);
    
    expect(TOKEN_LPAREN);
    node->data.func_def.params = parse_parameter_list();
    expect(TOKEN_RPAREN);
    
    // Parse return type (simplified - assume int for now)
    node->data.func_def.ret_type = NULL;
    
    expect(TOKEN_LBRACE);
    node->data.func_def.body = parse_block();
    expect(TOKEN_RBRACE);
    
    return node;
}

AstNode *parse_struct_definition() {
    AstNode *node = new_node(AST_STRUCT_DEFINITION);
    
    expect(TOKEN_STRUCT);
    node->data.struct_def.name = current_token.value;
    expect(TOKEN_IDENTIFIER);
    
    expect(TOKEN_LBRACE);
    node->data.struct_def.fields = parse_struct_fields();
    expect(TOKEN_RBRACE);
    
    return node;
}

AstNode *parse_struct_fields() {
    if (current_token.type == TOKEN_RBRACE) {
        return NULL;
    }
    
    AstNode *node = new_node(AST_STRUCT_FIELD);
    node->data.struct_field.name = current_token.value;
    expect(TOKEN_IDENTIFIER);
    
    expect(TOKEN_COLON);
    node->data.struct_field.type = parse_type();
    
    if (current_token.type == TOKEN_SEMICOLON) {
        advance_token();
    }
    
    node->next = parse_struct_fields();
    return node;
}

AstNode *parse_parameter_list() {
    if (current_token.type == TOKEN_RPAREN) {
        return NULL;
    }
    
    AstNode *node = new_node(AST_PARAMETER);
    node->data.parameter.name = current_token.value;
    expect(TOKEN_IDENTIFIER);
    
    expect(TOKEN_COLON);
    node->data.parameter.type = parse_type();
    
    if (current_token.type == TOKEN_COMMA) {
        advance_token();
        node->next = parse_parameter_list();
    }
    
    return node;
}

AstNode *parse_type() {
    if (current_token.type == TOKEN_POINTER) {
        advance_token();
        AstNode *base_type = parse_type();
        // Create pointer type
        return base_type; // Simplified
    }
    
    AstNode *node = new_node(AST_IDENTIFIER);
    node->data.name = current_token.value;
    expect(TOKEN_IDENTIFIER);
    return node;
}

AstNode *parse_statement() {
    AstNode *node = NULL;
    
    switch (current_token.type) {
        case TOKEN_LET:
            node = new_node(AST_VAR_DECL);
            advance_token();
            node->data.var_decl.name = current_token.value;
            expect(TOKEN_IDENTIFIER);
            expect(TOKEN_ASSIGN);
            node->data.var_decl.init_expr = parse_expression();
            break;
            
        case TOKEN_IF:
            node = new_node(AST_IF);
            advance_token();
            expect(TOKEN_LPAREN);
            node->data.if_stmt.cond = parse_expression();
            expect(TOKEN_RPAREN);
            expect(TOKEN_LBRACE);
            node->data.if_stmt.body = parse_block();
            expect(TOKEN_RBRACE);
            
            if (current_token.type == TOKEN_ELSE) {
                advance_token();
                expect(TOKEN_LBRACE);
                node->data.if_stmt.else_body = parse_block();
                expect(TOKEN_RBRACE);
            }
            break;
            
        case TOKEN_RET:
            node = new_node(AST_RET);
            advance_token();
            if (current_token.type != TOKEN_SEMICOLON) {
                node->data.ret_stmt.ret_expr = parse_expression();
            }
            break;
            
        case TOKEN_FOR:
            node = new_node(AST_FOR_LOOP);
            advance_token();
            expect(TOKEN_LPAREN);
            node->data.for_loop.init = parse_statement();
            expect(TOKEN_SEMICOLON);
            node->data.for_loop.cond = parse_expression();
            expect(TOKEN_SEMICOLON);
            node->data.for_loop.incr = parse_expression();
            expect(TOKEN_RPAREN);
            expect(TOKEN_LBRACE);
            node->data.for_loop.body = parse_block();
            expect(TOKEN_RBRACE);
            break;
            
        case TOKEN_WHILE:
            node = new_node(AST_WHILE_LOOP);
            advance_token();
            expect(TOKEN_LPAREN);
            node->data.while_loop.cond = parse_expression();
            expect(TOKEN_RPAREN);
            expect(TOKEN_LBRACE);
            node->data.while_loop.body = parse_block();
            expect(TOKEN_RBRACE);
            break;
            
        default:
            node = parse_expression();
            break;
    }
    
    return node;
}

AstNode *parse_expression() {
    return parse_assignment();
}

AstNode *parse_assignment() {
    AstNode *left = parse_equality();
    
    if (current_token.type == TOKEN_ASSIGN) {
        AstNode *node = new_node(AST_ASSIGN);
        node->left = left;
        advance_token();
        node->right = parse_assignment();
        return node;
    }
    
    return left;
}

AstNode *parse_equality() {
    AstNode *left = parse_comparison();
    
    while (current_token.type == TOKEN_EQUAL) {
        AstNode *node = new_node(AST_OP);
        node->data.op = current_token.type;
        node->left = left;
        advance_token();
        node->right = parse_comparison();
        left = node;
    }
    
    return left;
}

AstNode *parse_comparison() {
    AstNode *left = parse_addition();
    
    while (current_token.type == TOKEN_GREATER) {
        AstNode *node = new_node(AST_OP);
        node->data.op = current_token.type;
        node->left = left;
        advance_token();
        node->right = parse_addition();
        left = node;
    }
    
    return left;
}

AstNode *parse_addition() {
    AstNode *left = parse_multiplication();
    
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        AstNode *node = new_node(AST_OP);
        node->data.op = current_token.type;
        node->left = left;
        advance_token();
        node->right = parse_multiplication();
        left = node;
    }
    
    return left;
}

AstNode *parse_multiplication() {
    AstNode *left = parse_primary();
    
    while (current_token.type == TOKEN_MULTIPLY || current_token.type == TOKEN_DIVIDE) {
        AstNode *node = new_node(AST_OP);
        node->data.op = current_token.type;
        node->left = left;
        advance_token();
        node->right = parse_primary();
        left = node;
    }
    
    return left;
}

AstNode *parse_primary() {
    switch (current_token.type) {
        case TOKEN_NUMBER: {
            AstNode *node = new_node(AST_NUMBER);
            node->data.value = atoi(current_token.value);
            advance_token();
            return node;
        }
        
        case TOKEN_IDENTIFIER: {
            char *name = current_token.value;
            advance_token();
            
            if (current_token.type == TOKEN_LPAREN) {
                // Function call
                AstNode *node = new_node(AST_FUNCTION_CALL);
                node->data.func_call.name = name;
                advance_token();
                node->data.func_call.args = parse_argument_list();
                expect(TOKEN_RPAREN);
                return node;
            } else {
                // Variable reference
                AstNode *node = new_node(AST_IDENTIFIER);
                node->data.name = name;
                return node;
            }
        }
        
        case TOKEN_LPAREN: {
            advance_token();
            AstNode *node = parse_expression();
            expect(TOKEN_RPAREN);
            return node;
        }
        
        case TOKEN_DEREFERENCE: {
            AstNode *node = new_node(AST_DEREFERENCE);
            advance_token();
            node->data.dereference.expr = parse_primary();
            return node;
        }
        
        case TOKEN_ADDRESS_OF: {
            AstNode *node = new_node(AST_ADDRESS_OF);
            advance_token();
            node->data.address_of.expr = parse_primary();
            return node;
        }
        
        case TOKEN_ALLOC: {
            AstNode *node = new_node(AST_FUNCTION_CALL);
            node->data.func_call.name = "malloc";
            advance_token();
            expect(TOKEN_LPAREN);
            node->data.func_call.args = parse_expression();
            expect(TOKEN_RPAREN);
            return node;
        }
        
        default:
            fprintf(stderr, "Unexpected token: %s\n", current_token.value);
            exit(1);
    }
}

AstNode *parse_block() {
    AstNode *node = new_node(AST_BLOCK);
    node->data.block.head = parse_statement();
    return node;
}

AstNode *parse_argument_list() {
    if (current_token.type == TOKEN_RPAREN) {
        return NULL;
    }
    
    AstNode *node = parse_expression();
    
    if (current_token.type == TOKEN_COMMA) {
        advance_token();
        node->next = parse_argument_list();
    }
    
    return node;
}

void expect(TokenType expected) {
    if (current_token.type != expected) {
        fprintf(stderr, "Expected token %d, got %d\n", expected, current_token.type);
        exit(1);
    }
    advance_token();
}

void advance_token() {
    lexer_next();
}

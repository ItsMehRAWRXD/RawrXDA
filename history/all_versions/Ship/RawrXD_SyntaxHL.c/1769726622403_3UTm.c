// RawrXD Syntax Highlighter - Pure Win32 (No Qt)
// Replaces: syntax_highlighter.cpp, smart_highlighter.cpp
// Full syntax highlighting for code editors

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE  
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <richedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_KEYWORDS     256
#define MAX_KEYWORD_LEN  64
#define MAX_SCOPES       16

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef enum {
    LANG_NONE = 0,
    LANG_C,
    LANG_CPP,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_TYPESCRIPT,
    LANG_RUST,
    LANG_GO,
    LANG_JAVA,
    LANG_CSHARP,
    LANG_PHP,
    LANG_RUBY,
    LANG_HTML,
    LANG_CSS,
    LANG_JSON,
    LANG_XML,
    LANG_MARKDOWN,
    LANG_SQL,
    LANG_SHELL,
    LANG_POWERSHELL,
    LANG_LUA,
    LANG_ASM,
    LANG_COUNT
} LanguageID;

typedef enum {
    SH_TOKEN_NONE = 0,
    SH_TOKEN_KEYWORD,
    SH_TOKEN_TYPE,
    SH_TOKEN_FUNCTION,
    SH_TOKEN_STRING,
    SH_TOKEN_CHAR,
    SH_TOKEN_NUMBER,
    SH_TOKEN_COMMENT,
    SH_TOKEN_PREPROCESSOR,
    SH_TOKEN_OPERATOR,
    SH_TOKEN_BRACKET,
    SH_TOKEN_IDENTIFIER,
    SH_TOKEN_ATTRIBUTE,
    SH_TOKEN_TAG,
    SH_TOKEN_ERROR,
    SH_TOKEN_COUNT
} SH_TokenType;

typedef struct {
    int start;
    int length;
    SH_TokenType type;
} SH_SyntaxToken;

typedef struct {
    COLORREF foreground;
    COLORREF background;
    BOOL bold;
    BOOL italic;
    BOOL underline;
} SH_TokenStyle;

typedef struct {
    char keywords[MAX_KEYWORDS][MAX_KEYWORD_LEN];
    int keyword_count;
    char types[MAX_KEYWORDS][MAX_KEYWORD_LEN];
    int type_count;
    char builtins[MAX_KEYWORDS][MAX_KEYWORD_LEN];
    int builtin_count;
    
    // Language characteristics
    char single_line_comment[16];
    char multi_line_comment_start[16];
    char multi_line_comment_end[16];
    char string_delimiters[8];
    BOOL has_preprocessor;
    char preprocessor_char;
    BOOL case_sensitive;
} LanguageSpec;

typedef struct {
    LanguageID language;
    LanguageSpec spec;
    SH_TokenStyle styles[SH_TOKEN_COUNT];
    
    // Parsing state
    BOOL in_multiline_comment;
    BOOL in_multiline_string;
    char string_delimiter;
    int scope_depth;
} SyntaxHighlighter;

// Forward declarations
static void InitLanguageSpec(SyntaxHighlighter* hl, LanguageID lang);
static void InitDefaultStyles(SyntaxHighlighter* hl);
static BOOL IsKeyword(const SyntaxHighlighter* hl, const char* word);
static BOOL IsType(const SyntaxHighlighter* hl, const char* word);
static BOOL IsBuiltin(const SyntaxHighlighter* hl, const char* word);

// ============================================================================
// CREATION / DESTRUCTION
// ============================================================================

__declspec(dllexport)
SyntaxHighlighter* SyntaxHL_Create(LanguageID language) {
    SyntaxHighlighter* hl = (SyntaxHighlighter*)calloc(1, sizeof(SyntaxHighlighter));
    if (!hl) return NULL;
    
    hl->language = language;
    InitLanguageSpec(hl, language);
    InitDefaultStyles(hl);
    
    return hl;
}

__declspec(dllexport)
void SyntaxHL_Destroy(SyntaxHighlighter* hl) {
    if (hl) free(hl);
}

__declspec(dllexport)
void SyntaxHL_SetLanguage(SyntaxHighlighter* hl, LanguageID language) {
    if (!hl) return;
    hl->language = language;
    InitLanguageSpec(hl, language);
    hl->in_multiline_comment = FALSE;
    hl->in_multiline_string = FALSE;
}

__declspec(dllexport)
LanguageID SyntaxHL_DetectLanguage(const wchar_t* filename) {
    if (!filename) return LANG_NONE;
    
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (!ext) return LANG_NONE;
    ext++;  // Skip the dot
    
    // C/C++
    if (_wcsicmp(ext, L"c") == 0) return LANG_C;
    if (_wcsicmp(ext, L"h") == 0) return LANG_C;
    if (_wcsicmp(ext, L"cpp") == 0 || _wcsicmp(ext, L"cc") == 0 || 
        _wcsicmp(ext, L"cxx") == 0) return LANG_CPP;
    if (_wcsicmp(ext, L"hpp") == 0 || _wcsicmp(ext, L"hxx") == 0) return LANG_CPP;
    
    // Scripting
    if (_wcsicmp(ext, L"py") == 0) return LANG_PYTHON;
    if (_wcsicmp(ext, L"js") == 0) return LANG_JAVASCRIPT;
    if (_wcsicmp(ext, L"jsx") == 0) return LANG_JAVASCRIPT;
    if (_wcsicmp(ext, L"ts") == 0) return LANG_TYPESCRIPT;
    if (_wcsicmp(ext, L"tsx") == 0) return LANG_TYPESCRIPT;
    
    // Systems
    if (_wcsicmp(ext, L"rs") == 0) return LANG_RUST;
    if (_wcsicmp(ext, L"go") == 0) return LANG_GO;
    
    // JVM
    if (_wcsicmp(ext, L"java") == 0) return LANG_JAVA;
    if (_wcsicmp(ext, L"cs") == 0) return LANG_CSHARP;
    
    // Web
    if (_wcsicmp(ext, L"php") == 0) return LANG_PHP;
    if (_wcsicmp(ext, L"rb") == 0) return LANG_RUBY;
    if (_wcsicmp(ext, L"html") == 0 || _wcsicmp(ext, L"htm") == 0) return LANG_HTML;
    if (_wcsicmp(ext, L"css") == 0 || _wcsicmp(ext, L"scss") == 0) return LANG_CSS;
    
    // Data
    if (_wcsicmp(ext, L"json") == 0) return LANG_JSON;
    if (_wcsicmp(ext, L"xml") == 0) return LANG_XML;
    if (_wcsicmp(ext, L"md") == 0) return LANG_MARKDOWN;
    
    // DB/Shell
    if (_wcsicmp(ext, L"sql") == 0) return LANG_SQL;
    if (_wcsicmp(ext, L"sh") == 0 || _wcsicmp(ext, L"bash") == 0) return LANG_SHELL;
    if (_wcsicmp(ext, L"ps1") == 0) return LANG_POWERSHELL;
    
    // Other
    if (_wcsicmp(ext, L"lua") == 0) return LANG_LUA;
    if (_wcsicmp(ext, L"asm") == 0 || _wcsicmp(ext, L"s") == 0) return LANG_ASM;
    
    return LANG_NONE;
}

// ============================================================================
// STYLE MANAGEMENT
// ============================================================================

__declspec(dllexport)
void SyntaxHL_SetStyle(SyntaxHighlighter* hl, SH_TokenType token, 
                        COLORREF fg, COLORREF bg, BOOL bold, BOOL italic) {
    if (!hl || token >= SH_TOKEN_COUNT) return;
    hl->styles[token].foreground = fg;
    hl->styles[token].background = bg;
    hl->styles[token].bold = bold;
    hl->styles[token].italic = italic;
}

__declspec(dllexport)
const SH_TokenStyle* SyntaxHL_GetStyle(const SyntaxHighlighter* hl, SH_TokenType token) {
    if (!hl || token >= SH_TOKEN_COUNT) return NULL;
    return &hl->styles[token];
}

static void InitDefaultStyles(SyntaxHighlighter* hl) {
    // Dark theme defaults
    hl->styles[SH_TOKEN_NONE].foreground = RGB(212, 212, 212);
    hl->styles[SH_TOKEN_KEYWORD].foreground = RGB(86, 156, 214);        // Blue
    hl->styles[SH_TOKEN_KEYWORD].bold = TRUE;
    hl->styles[SH_TOKEN_TYPE].foreground = RGB(78, 201, 176);           // Teal
    hl->styles[SH_TOKEN_FUNCTION].foreground = RGB(220, 220, 170);      // Yellow
    hl->styles[SH_TOKEN_STRING].foreground = RGB(206, 145, 120);        // Orange
    hl->styles[SH_TOKEN_CHAR].foreground = RGB(206, 145, 120);
    hl->styles[SH_TOKEN_NUMBER].foreground = RGB(181, 206, 168);        // Light green
    hl->styles[SH_TOKEN_COMMENT].foreground = RGB(106, 153, 85);        // Green
    hl->styles[SH_TOKEN_COMMENT].italic = TRUE;
    hl->styles[SH_TOKEN_PREPROCESSOR].foreground = RGB(155, 155, 155);  // Gray
    hl->styles[SH_TOKEN_OPERATOR].foreground = RGB(212, 212, 212);
    hl->styles[SH_TOKEN_BRACKET].foreground = RGB(255, 215, 0);         // Gold
    hl->styles[SH_TOKEN_IDENTIFIER].foreground = RGB(156, 220, 254);    // Light blue
    hl->styles[SH_TOKEN_ATTRIBUTE].foreground = RGB(78, 201, 176);
    hl->styles[SH_TOKEN_TAG].foreground = RGB(86, 156, 214);
    hl->styles[SH_TOKEN_ERROR].foreground = RGB(255, 0, 0);
    hl->styles[SH_TOKEN_ERROR].underline = TRUE;
    
    // Set backgrounds to transparent (use editor bg)
    for (int i = 0; i < SH_TOKEN_COUNT; i++) {
        hl->styles[i].background = CLR_NONE;
    }
}

// ============================================================================
// LANGUAGE SPECIFICATIONS
// ============================================================================

static void AddKeywords(LanguageSpec* spec, const char* keywords[], int count) {
    for (int i = 0; i < count && spec->keyword_count < MAX_KEYWORDS; i++) {
        strncpy_s(spec->keywords[spec->keyword_count++], MAX_KEYWORD_LEN, keywords[i], _TRUNCATE);
    }
}

static void AddTypes(LanguageSpec* spec, const char* types[], int count) {
    for (int i = 0; i < count && spec->type_count < MAX_KEYWORDS; i++) {
        strncpy_s(spec->types[spec->type_count++], MAX_KEYWORD_LEN, types[i], _TRUNCATE);
    }
}

static void InitLanguageSpec(SyntaxHighlighter* hl, LanguageID lang) {
    LanguageSpec* spec = &hl->spec;
    memset(spec, 0, sizeof(LanguageSpec));
    
    spec->case_sensitive = TRUE;
    strcpy_s(spec->string_delimiters, sizeof(spec->string_delimiters), "\"'");
    
    switch (lang) {
    case LANG_C:
    case LANG_CPP: {
        const char* keywords[] = {
            "auto", "break", "case", "const", "continue", "default", "do", "else",
            "enum", "extern", "for", "goto", "if", "inline", "register", "return",
            "sizeof", "static", "struct", "switch", "typedef", "union", "volatile",
            "while", "restrict", "_Atomic", "_Thread_local"
        };
        if (lang == LANG_CPP) {
            const char* cpp_kw[] = {
                "class", "public", "private", "protected", "virtual", "override",
                "final", "new", "delete", "try", "catch", "throw", "namespace",
                "using", "template", "typename", "this", "nullptr", "constexpr",
                "noexcept", "explicit", "friend", "mutable", "operator", "decltype",
                "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast"
            };
            AddKeywords(spec, keywords, sizeof(keywords)/sizeof(keywords[0]));
            AddKeywords(spec, cpp_kw, sizeof(cpp_kw)/sizeof(cpp_kw[0]));
        } else {
            AddKeywords(spec, keywords, sizeof(keywords)/sizeof(keywords[0]));
        }
        
        const char* types[] = {
            "void", "int", "char", "short", "long", "float", "double", "signed",
            "unsigned", "bool", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t", "BOOL", "BYTE", "WORD",
            "DWORD", "HANDLE", "LPSTR", "LPWSTR", "LPCSTR", "LPCWSTR", "HWND"
        };
        AddTypes(spec, types, sizeof(types)/sizeof(types[0]));
        
        strcpy_s(spec->single_line_comment, sizeof(spec->single_line_comment), "//");
        strcpy_s(spec->multi_line_comment_start, sizeof(spec->multi_line_comment_start), "/*");
        strcpy_s(spec->multi_line_comment_end, sizeof(spec->multi_line_comment_end), "*/");
        spec->has_preprocessor = TRUE;
        spec->preprocessor_char = '#';
        break;
    }
    
    case LANG_PYTHON: {
        const char* keywords[] = {
            "and", "as", "assert", "async", "await", "break", "class", "continue",
            "def", "del", "elif", "else", "except", "finally", "for", "from",
            "global", "if", "import", "in", "is", "lambda", "nonlocal", "not",
            "or", "pass", "raise", "return", "try", "while", "with", "yield"
        };
        AddKeywords(spec, keywords, sizeof(keywords)/sizeof(keywords[0]));
        
        const char* types[] = {
            "True", "False", "None", "int", "float", "str", "list", "dict",
            "tuple", "set", "bool", "bytes", "range", "object", "type"
        };
        AddTypes(spec, types, sizeof(types)/sizeof(types[0]));
        
        strcpy_s(spec->single_line_comment, sizeof(spec->single_line_comment), "#");
        strcpy_s(spec->string_delimiters, sizeof(spec->string_delimiters), "\"'`");
        break;
    }
    
    case LANG_JAVASCRIPT:
    case LANG_TYPESCRIPT: {
        const char* keywords[] = {
            "break", "case", "catch", "class", "const", "continue", "debugger",
            "default", "delete", "do", "else", "export", "extends", "finally",
            "for", "function", "if", "import", "in", "instanceof", "let", "new",
            "return", "super", "switch", "this", "throw", "try", "typeof", "var",
            "void", "while", "with", "yield", "async", "await", "of", "from"
        };
        AddKeywords(spec, keywords, sizeof(keywords)/sizeof(keywords[0]));
        
        if (lang == LANG_TYPESCRIPT) {
            const char* ts_kw[] = {
                "interface", "type", "enum", "implements", "namespace", "module",
                "declare", "as", "readonly", "keyof", "infer", "never", "unknown"
            };
            AddKeywords(spec, ts_kw, sizeof(ts_kw)/sizeof(ts_kw[0]));
        }
        
        const char* types[] = {
            "true", "false", "null", "undefined", "NaN", "Infinity", "string",
            "number", "boolean", "any", "void", "object", "Array", "Object",
            "String", "Number", "Boolean", "Promise", "Map", "Set"
        };
        AddTypes(spec, types, sizeof(types)/sizeof(types[0]));
        
        strcpy_s(spec->single_line_comment, sizeof(spec->single_line_comment), "//");
        strcpy_s(spec->multi_line_comment_start, sizeof(spec->multi_line_comment_start), "/*");
        strcpy_s(spec->multi_line_comment_end, sizeof(spec->multi_line_comment_end), "*/");
        strcpy_s(spec->string_delimiters, sizeof(spec->string_delimiters), "\"'`");
        break;
    }
    
    case LANG_RUST: {
        const char* keywords[] = {
            "as", "async", "await", "break", "const", "continue", "crate", "dyn",
            "else", "enum", "extern", "fn", "for", "if", "impl", "in", "let",
            "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
            "self", "Self", "static", "struct", "super", "trait", "type",
            "unsafe", "use", "where", "while"
        };
        AddKeywords(spec, keywords, sizeof(keywords)/sizeof(keywords[0]));
        
        const char* types[] = {
            "bool", "char", "str", "i8", "i16", "i32", "i64", "i128", "isize",
            "u8", "u16", "u32", "u64", "u128", "usize", "f32", "f64",
            "String", "Vec", "Box", "Option", "Result", "Some", "None", "Ok", "Err"
        };
        AddTypes(spec, types, sizeof(types)/sizeof(types[0]));
        
        strcpy_s(spec->single_line_comment, sizeof(spec->single_line_comment), "//");
        strcpy_s(spec->multi_line_comment_start, sizeof(spec->multi_line_comment_start), "/*");
        strcpy_s(spec->multi_line_comment_end, sizeof(spec->multi_line_comment_end), "*/");
        break;
    }
    
    case LANG_GO: {
        const char* keywords[] = {
            "break", "case", "chan", "const", "continue", "default", "defer",
            "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
            "interface", "map", "package", "range", "return", "select", "struct",
            "switch", "type", "var"
        };
        AddKeywords(spec, keywords, sizeof(keywords)/sizeof(keywords[0]));
        
        const char* types[] = {
            "bool", "byte", "complex64", "complex128", "error", "float32", "float64",
            "int", "int8", "int16", "int32", "int64", "rune", "string",
            "uint", "uint8", "uint16", "uint32", "uint64", "uintptr",
            "true", "false", "nil", "iota"
        };
        AddTypes(spec, types, sizeof(types)/sizeof(types[0]));
        
        strcpy_s(spec->single_line_comment, sizeof(spec->single_line_comment), "//");
        strcpy_s(spec->multi_line_comment_start, sizeof(spec->multi_line_comment_start), "/*");
        strcpy_s(spec->multi_line_comment_end, sizeof(spec->multi_line_comment_end), "*/");
        break;
    }
    
    default:
        // Generic defaults
        strcpy_s(spec->single_line_comment, sizeof(spec->single_line_comment), "//");
        strcpy_s(spec->multi_line_comment_start, sizeof(spec->multi_line_comment_start), "/*");
        strcpy_s(spec->multi_line_comment_end, sizeof(spec->multi_line_comment_end), "*/");
        break;
    }
}

// ============================================================================
// TOKENIZATION
// ============================================================================

static BOOL IsKeyword(const SyntaxHighlighter* hl, const char* word) {
    for (int i = 0; i < hl->spec.keyword_count; i++) {
        if (hl->spec.case_sensitive) {
            if (strcmp(word, hl->spec.keywords[i]) == 0) return TRUE;
        } else {
            if (_stricmp(word, hl->spec.keywords[i]) == 0) return TRUE;
        }
    }
    return FALSE;
}

static BOOL IsType(const SyntaxHighlighter* hl, const char* word) {
    for (int i = 0; i < hl->spec.type_count; i++) {
        if (hl->spec.case_sensitive) {
            if (strcmp(word, hl->spec.types[i]) == 0) return TRUE;
        } else {
            if (_stricmp(word, hl->spec.types[i]) == 0) return TRUE;
        }
    }
    return FALSE;
}

static BOOL IsBuiltin(const SyntaxHighlighter* hl, const char* word) {
    for (int i = 0; i < hl->spec.builtin_count; i++) {
        if (hl->spec.case_sensitive) {
            if (strcmp(word, hl->spec.builtins[i]) == 0) return TRUE;
        } else {
            if (_stricmp(word, hl->spec.builtins[i]) == 0) return TRUE;
        }
    }
    return FALSE;
}

__declspec(dllexport)
int SyntaxHL_TokenizeLine(SyntaxHighlighter* hl, const char* line, int line_len,
                           SH_SyntaxToken* tokens, int max_tokens) {
    if (!hl || !line || !tokens || max_tokens <= 0) return 0;
    
    int token_count = 0;
    int i = 0;
    LanguageSpec* spec = &hl->spec;
    
    while (i < line_len && token_count < max_tokens) {
        // Skip whitespace
        while (i < line_len && isspace((unsigned char)line[i])) i++;
        if (i >= line_len) break;
        
        int start = i;
        SH_TokenType type = SH_TOKEN_NONE;
        
        // Check for multiline comment continuation
        if (hl->in_multiline_comment) {
            char* end = strstr(line + i, spec->multi_line_comment_end);
            if (end) {
                i = (int)(end - line) + (int)strlen(spec->multi_line_comment_end);
                hl->in_multiline_comment = FALSE;
            } else {
                i = line_len;
            }
            type = SH_TOKEN_COMMENT;
        }
        // Single-line comment
        else if (spec->single_line_comment[0] && 
                 strncmp(line + i, spec->single_line_comment, strlen(spec->single_line_comment)) == 0) {
            i = line_len;
            type = SH_TOKEN_COMMENT;
        }
        // Multi-line comment start
        else if (spec->multi_line_comment_start[0] && 
                 strncmp(line + i, spec->multi_line_comment_start, strlen(spec->multi_line_comment_start)) == 0) {
            i += (int)strlen(spec->multi_line_comment_start);
            char* end = strstr(line + i, spec->multi_line_comment_end);
            if (end) {
                i = (int)(end - line) + (int)strlen(spec->multi_line_comment_end);
            } else {
                i = line_len;
                hl->in_multiline_comment = TRUE;
            }
            type = SH_TOKEN_COMMENT;
        }
        // Preprocessor
        else if (spec->has_preprocessor && line[i] == spec->preprocessor_char) {
            i = line_len;
            type = SH_TOKEN_PREPROCESSOR;
        }
        // String
        else if (strchr(spec->string_delimiters, line[i])) {
            char delim = line[i];
            i++;
            while (i < line_len) {
                if (line[i] == '\\' && i + 1 < line_len) {
                    i += 2;  // Skip escape sequence
                } else if (line[i] == delim) {
                    i++;
                    break;
                } else {
                    i++;
                }
            }
            type = (delim == '\'') ? SH_TOKEN_CHAR : SH_TOKEN_STRING;
        }
        // Number
        else if (isdigit((unsigned char)line[i]) || 
                 (line[i] == '.' && i + 1 < line_len && isdigit((unsigned char)line[i + 1]))) {
            // Hex/octal/binary prefix
            if (line[i] == '0' && i + 1 < line_len) {
                if (line[i + 1] == 'x' || line[i + 1] == 'X') {
                    i += 2;
                    while (i < line_len && isxdigit((unsigned char)line[i])) i++;
                } else if (line[i + 1] == 'b' || line[i + 1] == 'B') {
                    i += 2;
                    while (i < line_len && (line[i] == '0' || line[i] == '1')) i++;
                } else if (isdigit((unsigned char)line[i + 1])) {
                    i++;
                    while (i < line_len && line[i] >= '0' && line[i] <= '7') i++;
                } else {
                    i++;
                }
            } else {
                while (i < line_len && isdigit((unsigned char)line[i])) i++;
            }
            // Decimal part
            if (i < line_len && line[i] == '.') {
                i++;
                while (i < line_len && isdigit((unsigned char)line[i])) i++;
            }
            // Exponent
            if (i < line_len && (line[i] == 'e' || line[i] == 'E')) {
                i++;
                if (i < line_len && (line[i] == '+' || line[i] == '-')) i++;
                while (i < line_len && isdigit((unsigned char)line[i])) i++;
            }
            // Type suffix
            while (i < line_len && (line[i] == 'u' || line[i] == 'U' || 
                                    line[i] == 'l' || line[i] == 'L' ||
                                    line[i] == 'f' || line[i] == 'F')) i++;
            type = TOKEN_NUMBER;
        }
        // Identifier/keyword
        else if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            while (i < line_len && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            
            // Extract the word
            char word[MAX_KEYWORD_LEN];
            int word_len = i - start;
            if (word_len >= MAX_KEYWORD_LEN) word_len = MAX_KEYWORD_LEN - 1;
            strncpy_s(word, MAX_KEYWORD_LEN, line + start, word_len);
            word[word_len] = '\0';
            
            if (IsKeyword(hl, word)) {
                type = TOKEN_KEYWORD;
            } else if (IsType(hl, word)) {
                type = TOKEN_TYPE;
            } else if (i < line_len && line[i] == '(') {
                type = TOKEN_FUNCTION;
            } else {
                type = TOKEN_IDENTIFIER;
            }
        }
        // Operators
        else if (strchr("+-*/%=<>&|^!~?:.,;", line[i])) {
            // Handle multi-character operators
            if (i + 1 < line_len) {
                const char* ops[] = {
                    "==", "!=", "<=", ">=", "&&", "||", "++", "--", "<<", ">>",
                    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=",
                    "->", "::", "...", NULL
                };
                for (int j = 0; ops[j]; j++) {
                    int op_len = (int)strlen(ops[j]);
                    if (i + op_len <= line_len && strncmp(line + i, ops[j], op_len) == 0) {
                        i += op_len;
                        goto op_done;
                    }
                }
            }
            i++;
            op_done:
            type = TOKEN_OPERATOR;
        }
        // Brackets
        else if (strchr("()[]{}",line[i])) {
            i++;
            type = TOKEN_BRACKET;
        }
        // Unknown
        else {
            i++;
            type = TOKEN_NONE;
        }
        
        if (type != TOKEN_NONE && i > start) {
            tokens[token_count].start = start;
            tokens[token_count].length = i - start;
            tokens[token_count].type = type;
            token_count++;
        }
    }
    
    return token_count;
}

// ============================================================================
// RICH EDIT HIGHLIGHTING
// ============================================================================

__declspec(dllexport)
void SyntaxHL_ApplyToRichEdit(SyntaxHighlighter* hl, HWND hRichEdit, 
                               int line_start, int line_count) {
    if (!hl || !hRichEdit || !IsWindow(hRichEdit)) return;
    
    // Lock updates
    SendMessage(hRichEdit, WM_SETREDRAW, FALSE, 0);
    
    // Save selection
    CHARRANGE old_sel;
    SendMessage(hRichEdit, EM_EXGETSEL, 0, (LPARAM)&old_sel);
    
    // Get text buffer
    GETTEXTLENGTHEX gtlex = { GTL_DEFAULT, CP_ACP };
    int text_len = (int)SendMessage(hRichEdit, EM_GETTEXTLENGTHEX, (WPARAM)&gtlex, 0);
    
    char* text = (char*)malloc(text_len + 1);
    GETTEXTEX gtex = { (DWORD)(text_len + 1), GT_DEFAULT, CP_ACP, NULL, NULL };
    SendMessage(hRichEdit, EM_GETTEXTEX, (WPARAM)&gtex, (LPARAM)text);
    
    // Get line info
    int total_lines = (int)SendMessage(hRichEdit, EM_GETLINECOUNT, 0, 0);
    if (line_start < 0) line_start = 0;
    if (line_count <= 0 || line_start + line_count > total_lines)
        line_count = total_lines - line_start;
    
    // Reset parser state for full refresh
    if (line_start == 0) {
        hl->in_multiline_comment = FALSE;
        hl->in_multiline_string = FALSE;
    }
    
    // Process each line
    SyntaxToken tokens[256];
    
    for (int line = line_start; line < line_start + line_count; line++) {
        int line_idx = (int)SendMessage(hRichEdit, EM_LINEINDEX, line, 0);
        int line_len = (int)SendMessage(hRichEdit, EM_LINELENGTH, line_idx, 0);
        
        if (line_len <= 0) continue;
        
        char* line_text = text + line_idx;
        int token_count = SyntaxHL_TokenizeLine(hl, line_text, line_len, tokens, 256);
        
        for (int t = 0; t < token_count; t++) {
            // Select the token range
            CHARRANGE cr;
            cr.cpMin = line_idx + tokens[t].start;
            cr.cpMax = line_idx + tokens[t].start + tokens[t].length;
            SendMessage(hRichEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
            
            // Apply formatting
            const TokenStyle* style = &hl->styles[tokens[t].type];
            
            CHARFORMAT2 cf;
            memset(&cf, 0, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
            cf.crTextColor = style->foreground;
            
            if (style->bold) cf.dwEffects |= CFE_BOLD;
            if (style->italic) cf.dwEffects |= CFE_ITALIC;
            if (style->underline) cf.dwEffects |= CFE_UNDERLINE;
            
            SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }
    }
    
    free(text);
    
    // Restore selection
    SendMessage(hRichEdit, EM_EXSETSEL, 0, (LPARAM)&old_sel);
    
    // Unlock and repaint
    SendMessage(hRichEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hRichEdit, NULL, TRUE);
}

// ============================================================================
// BRACKET MATCHING
// ============================================================================

__declspec(dllexport)
int SyntaxHL_FindMatchingBracket(const char* text, int text_len, int position) {
    if (!text || position < 0 || position >= text_len) return -1;
    
    char ch = text[position];
    char match;
    int direction;
    
    switch (ch) {
        case '(': match = ')'; direction = 1; break;
        case ')': match = '('; direction = -1; break;
        case '[': match = ']'; direction = 1; break;
        case ']': match = '['; direction = -1; break;
        case '{': match = '}'; direction = 1; break;
        case '}': match = '{'; direction = -1; break;
        default: return -1;
    }
    
    int depth = 1;
    int i = position + direction;
    
    while (i >= 0 && i < text_len && depth > 0) {
        char c = text[i];
        if (c == ch) depth++;
        else if (c == match) depth--;
        if (depth == 0) return i;
        i += direction;
    }
    
    return -1;
}

// ============================================================================
// UTILITY
// ============================================================================

__declspec(dllexport)
const char* SyntaxHL_GetLanguageName(LanguageID lang) {
    const char* names[] = {
        "Plain Text", "C", "C++", "Python", "JavaScript", "TypeScript",
        "Rust", "Go", "Java", "C#", "PHP", "Ruby", "HTML", "CSS",
        "JSON", "XML", "Markdown", "SQL", "Shell", "PowerShell", "Lua", "Assembly"
    };
    if (lang >= 0 && lang < LANG_COUNT) return names[lang];
    return "Unknown";
}

__declspec(dllexport)
void SyntaxHL_ResetState(SyntaxHighlighter* hl) {
    if (!hl) return;
    hl->in_multiline_comment = FALSE;
    hl->in_multiline_string = FALSE;
    hl->scope_depth = 0;
}

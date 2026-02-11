#pragma once
#include "RawrXD_Win32_Foundation.h"
#include "RawrXD_Lexer.h"
#include "RawrXD_Renderer_D2D.h" // For Color, Font

namespace RawrXD {

struct TextStyle {
    Color color;
    bool bold;
    bool italic;
    
    TextStyle(Color c = Color::Black, bool b = false, bool i = false) 
        : color(c), bold(b), italic(i) {}
};

class StyleManager {
    std::unordered_map<TokenType, TextStyle> styles;
    Font baseFont; // We might need font ref caching
    
public:
    StyleManager();
    
    void setStyle(TokenType type, const TextStyle& style);
    const TextStyle& getStyle(TokenType type) const;
    
    // Pre-configured themes
    void loadDarkTheme();
    void loadLightTheme();
};

} // namespace RawrXD

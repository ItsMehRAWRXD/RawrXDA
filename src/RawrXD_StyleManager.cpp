#include "RawrXD_StyleManager.h"

namespace RawrXD {

StyleManager::StyleManager() {
    loadLightTheme(); // Default
}

void StyleManager::setStyle(TokenType type, const TextStyle& style) {
    styles[type] = style;
}

const TextStyle& StyleManager::getStyle(TokenType type) const {
    auto it = styles.find(type);
    if (it != styles.end()) return it->second;
    static TextStyle defaultStyle(Color::Black);
    return defaultStyle;
}

void StyleManager::loadLightTheme() {
    styles[TokenType::Default]      = TextStyle(Color(0, 0, 0));
    styles[TokenType::Keyword]      = TextStyle(Color(0, 0, 255), true);
    styles[TokenType::Instruction]  = TextStyle(Color(0, 0, 128), true);
    styles[TokenType::Register]     = TextStyle(Color(128, 0, 0));
    styles[TokenType::Number]       = TextStyle(Color(9, 134, 88));
    styles[TokenType::String]       = TextStyle(Color(163, 21, 21));
    styles[TokenType::Comment]      = TextStyle(Color(0, 128, 0), false, true);
    styles[TokenType::Operator]     = TextStyle(Color(0, 0, 0));
    styles[TokenType::Preprocessor] = TextStyle(Color(128, 128, 128));
    styles[TokenType::Label]        = TextStyle(Color(0, 0, 128), true);
    styles[TokenType::Directive]    = TextStyle(Color(0, 0, 255), true, true);
}

void StyleManager::loadDarkTheme() {
    styles[TokenType::Default]      = TextStyle(Color(220, 220, 220));
    styles[TokenType::Keyword]      = TextStyle(Color(86, 156, 214), true);
    styles[TokenType::Instruction]  = TextStyle(Color(86, 156, 214)); // VS Code Blue
    styles[TokenType::Register]     = TextStyle(Color(156, 220, 254)); // Light Blue
    styles[TokenType::Number]       = TextStyle(Color(181, 206, 168)); // Light Green
    styles[TokenType::String]       = TextStyle(Color(206, 145, 120)); // Orange/Red
    styles[TokenType::Comment]      = TextStyle(Color(106, 153, 85), false, true); // Green
    styles[TokenType::Operator]     = TextStyle(Color(220, 220, 220));
    styles[TokenType::Preprocessor] = TextStyle(Color(197, 134, 192)); // Purple
    styles[TokenType::Label]        = TextStyle(Color(220, 220, 170)); // Yellowish
    styles[TokenType::Directive]    = TextStyle(Color(197, 134, 192));
}

} // namespace RawrXD

#include "minimonaco.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <stack>

namespace MiniMonaco {

// =============================================================================
// TEXT BUFFER IMPLEMENTATION
// =============================================================================

TextBuffer::TextBuffer(size_t initialSize)
    : capacity_(initialSize)
    , length_(0)
    , gapStart_(0)
    , gapEnd_(initialSize)
{
    buffer_ = std::make_unique<wchar_t[]>(capacity_);
}

TextBuffer::~TextBuffer() = default;

void TextBuffer::moveGapTo(size_t pos) {
    if (pos == gapStart_) return;
    
    size_t gapLen = gapEnd_ - gapStart_;
    
    if (pos < gapStart_) {
        // Move gap left
        size_t moveLen = gapStart_ - pos;
        std::memmove(buffer_.get() + gapEnd_ - moveLen, 
                     buffer_.get() + pos, 
                     moveLen * sizeof(wchar_t));
        gapStart_ = pos;
        gapEnd_ = pos + gapLen;
    } else {
        // Move gap right
        size_t moveLen = pos - gapStart_;
        std::memmove(buffer_.get() + gapStart_,
                     buffer_.get() + gapEnd_,
                     moveLen * sizeof(wchar_t));
        gapStart_ = pos;
        gapEnd_ = pos + gapLen;
    }
}

void TextBuffer::ensureCapacity(size_t needed) {
    size_t currentLen = length_;
    size_t available = capacity_ - currentLen;
    
    if (available < needed) {
        // Grow capacity
        size_t newCapacity = std::max(capacity_ * 2, currentLen + needed);
        auto newBuffer = std::make_unique<wchar_t[]>(newCapacity);
        
        // Copy before gap
        std::memcpy(newBuffer.get(), buffer_.get(), gapStart_ * sizeof(wchar_t));
        
        // Copy after gap
        size_t afterGap = currentLen - gapStart_;
        std::memcpy(newBuffer.get() + newCapacity - afterGap,
                   buffer_.get() + gapEnd_,
                   afterGap * sizeof(wchar_t));
        
        buffer_ = std::move(newBuffer);
        gapEnd_ = newCapacity - afterGap;
        capacity_ = newCapacity;
    }
}

void TextBuffer::insert(size_t pos, const wchar_t* text, size_t len) {
    if (len == 0) return;
    
    moveGapTo(pos);
    ensureCapacity(len);
    
    std::memcpy(buffer_.get() + gapStart_, text, len * sizeof(wchar_t));
    gapStart_ += len;
    length_ += len;
    linesDirty_ = true;
    
    if (onChange_) {
        onChange_(pos, 0, len);
    }
}

void TextBuffer::erase(size_t pos, size_t len) {
    if (len == 0) return;
    
    moveGapTo(pos);
    gapEnd_ += len;
    length_ -= len;
    linesDirty_ = true;
    
    if (onChange_) {
        onChange_(pos, len, 0);
    }
}

wchar_t TextBuffer::at(size_t pos) const {
    if (pos >= length_) return L'\0';
    if (pos < gapStart_) return buffer_[pos];
    return buffer_[pos + (gapEnd_ - gapStart_)];
}

std::wstring_view TextBuffer::view(size_t pos, size_t len) const {
    // Note: This is simplified; real implementation needs to handle gap
    if (pos >= length_) return {};
    len = std::min(len, length_ - pos);
    
    if (pos + len <= gapStart_) {
        return std::wstring_view(buffer_.get() + pos, len);
    } else if (pos >= gapStart_) {
        return std::wstring_view(buffer_.get() + pos + (gapEnd_ - gapStart_), len);
    }
    // Cross-gap view not supported in this simplified version
    return {};
}

std::wstring TextBuffer::substr(size_t pos, size_t len) const {
    if (pos >= length_) return {};
    len = std::min(len, length_ - pos);
    
    std::wstring result;
    result.reserve(len);
    
    for (size_t i = 0; i < len; ++i) {
        result.push_back(at(pos + i));
    }
    
    return result;
}

void TextBuffer::setText(const std::wstring& text) {
    buffer_ = std::make_unique<wchar_t[]>(text.size() + 1);
    std::memcpy(buffer_.get(), text.data(), text.size() * sizeof(wchar_t));
    capacity_ = text.size();
    length_ = text.size();
    gapStart_ = length_;
    gapEnd_ = capacity_;
    linesDirty_ = true;
}

std::wstring TextBuffer::text() const {
    std::wstring result;
    result.reserve(length_);
    
    // Before gap
    result.append(buffer_.get(), gapStart_);
    
    // After gap
    result.append(buffer_.get() + gapEnd_, length_ - gapStart_);
    
    return result;
}

void TextBuffer::updateLines() const {
    if (!linesDirty_) return;
    
    lineStarts_.clear();
    lineStarts_.push_back(0);
    
    for (size_t i = 0; i < length_; ++i) {
        if (at(i) == L'\n') {
            lineStarts_.push_back(i + 1);
        }
    }
    
    linesDirty_ = false;
}

size_t TextBuffer::lineCount() const {
    updateLines();
    return lineStarts_.size();
}

size_t TextBuffer::lineStart(size_t line) const {
    updateLines();
    if (line >= lineStarts_.size()) return length_;
    return lineStarts_[line];
}

size_t TextBuffer::lineEnd(size_t line) const {
    updateLines();
    if (line + 1 >= lineStarts_.size()) return length_;
    return lineStarts_[line + 1];
}

size_t TextBuffer::lineFromPos(size_t pos) const {
    updateLines();
    auto it = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), pos);
    return std::distance(lineStarts_.begin(), it) - 1;
}

std::wstring TextBuffer::lineContent(size_t line) const {
    return substr(lineStart(line), lineEnd(line) - lineStart(line));
}

std::wstring_view TextBuffer::lineView(size_t line) const {
    // Simplified; real implementation handles gap
    return view(lineStart(line), lineEnd(line) - lineStart(line));
}

// =============================================================================
// UNDO STACK IMPLEMENTATION
// =============================================================================

UndoStack::UndoStack(size_t maxSize) : maxSize_(maxSize) {}

void UndoStack::pushInsert(size_t pos, const std::wstring& text) {
    if (inGroup_) {
        actions_.push_back({UndoAction::GroupEnd, pos, text, currentGroup_});
    }
    actions_.push_back({UndoAction::Insert, pos, text, currentGroup_});
    maxGroup_ = currentGroup_ + 1;
    
    // Trim if needed
    while (actions_.size() > maxSize_) {
        actions_.erase(actions_.begin());
    }
}

void UndoStack::pushErase(size_t pos, const std::wstring& text) {
    if (inGroup_) {
        actions_.push_back({UndoAction::GroupEnd, pos, text, currentGroup_});
    }
    actions_.push_back({UndoAction::Erase, pos, text, currentGroup_});
    maxGroup_ = currentGroup_ + 1;
    
    while (actions_.size() > maxSize_) {
        actions_.erase(actions_.begin());
    }
}

void UndoStack::beginGroup() {
    inGroup_ = true;
    actions_.push_back({UndoAction::GroupStart, 0, L"", currentGroup_});
}

void UndoStack::endGroup() {
    inGroup_ = false;
    actions_.push_back({UndoAction::GroupEnd, 0, L"", currentGroup_});
    currentGroup_++;
}

std::vector<UndoAction> UndoStack::undo() {
    if (!canUndo()) return {};
    
    std::vector<UndoAction> result;
    size_t group = currentGroup_ - 1;
    
    // Find all actions in this group
    for (auto it = actions_.rbegin(); it != actions_.rend(); ++it) {
        if (it->group == group && it->type != UndoAction::GroupStart && it->type != UndoAction::GroupEnd) {
            result.push_back(*it);
        }
        if (it->group < group) break;
    }
    
    // Reverse for correct order
    std::reverse(result.begin(), result.end());
    
    // Flip Insert/Erase for undo
    for (auto& action : result) {
        if (action.type == UndoAction::Insert) {
            action.type = UndoAction::Erase;
        } else {
            action.type = UndoAction::Insert;
        }
    }
    
    currentGroup_--;
    return result;
}

std::vector<UndoAction> UndoStack::redo() {
    if (!canRedo()) return {};
    
    std::vector<UndoAction> result;
    size_t group = currentGroup_;
    
    for (auto it = actions_.begin(); it != actions_.end(); ++it) {
        if (it->group == group && it->type != UndoAction::GroupStart && it->type != UndoAction::GroupEnd) {
            result.push_back(*it);
        }
        if (it->group > group) break;
    }
    
    currentGroup_++;
    return result;
}

void UndoStack::clear() {
    actions_.clear();
    currentGroup_ = 0;
    maxGroup_ = 0;
}

// =============================================================================
// SYNTAX HIGHLIGHTER IMPLEMENTATION
// =============================================================================

GenericHighlighter::GenericHighlighter(const std::string& lang) : language_(lang) {
    if (lang == "cpp" || lang == "c++" || lang == "c") {
        keywords_ = {L"auto", L"break", L"case", L"catch", L"class", L"const", L"continue",
                      L"default", L"delete", L"do", L"else", L"enum", L"explicit", L"extern",
                      L"false", L"for", L"friend", L"goto", L"if", L"inline", L"new",
                      L"operator", L"private", L"protected", L"public", L"return", L"sizeof",
                      L"static", L"struct", L"switch", L"template", L"this", L"throw",
                      L"true", L"try", L"typedef", L"typename", L"union", L"virtual",
                      L"while", L"namespace", L"using", L"void", L"volatile", L"nullptr",
                      L"constexpr", L"noexcept", L"override", L"final"};
        types_ = {L"int", L"char", L"short", L"long", L"float", L"double", L"bool",
                  L"void", L"size_t", L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
                  L"int8_t", L"int16_t", L"int32_t", L"int64_t", L"std::string", L"std::vector"};
        strings_ = true;
        comments_ = true;
        multiLineComments_ = true;
        numbers_ = true;
        functions_ = true;
    } else if (lang == "python" || lang == "py") {
        keywords_ = {L"and", L"as", L"assert", L"break", L"class", L"continue", L"def",
                      L"del", L"elif", L"else", L"except", L"finally", L"for", L"from",
                      L"global", L"if", L"import", L"in", L"is", L"lambda", L"nonlocal",
                      L"not", L"or", L"pass", L"raise", L"return", L"try", L"while",
                      L"with", L"yield", L"True", L"False", L"None"};
        types_ = {L"int", L"str", L"float", L"bool", L"list", L"dict", L"set", L"tuple"};
        strings_ = true;
        comments_ = true;
        multiLineComments_ = false;
        numbers_ = true;
        functions_ = true;
    }
}

std::vector<Token> GenericHighlighter::highlight(const std::wstring_view& text) const {
    if (language_ == "cpp" || language_ == "c++" || language_ == "c") {
        return highlightCpp(text);
    } else if (language_ == "python" || language_ == "py") {
        return highlightPython(text);
    }
    return highlightGeneric(text);
}

std::vector<Token> GenericHighlighter::highlightCpp(const std::wstring_view& text) const {
    std::vector<Token> tokens;
    size_t i = 0;
    
    while (i < text.size()) {
        // Skip whitespace
        while (i < text.size() && Utils::isWhitespace(text[i])) {
            i++;
        }
        if (i >= text.size()) break;
        
        size_t start = i;
        
        // Multi-line comment
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
            size_t end = i + 2;
            while (end + 1 < text.size() && !(text[end] == '*' && text[end + 1] == '/')) {
                end++;
            }
            if (end + 1 < text.size()) end += 2;
            tokens.push_back({start, end, "comment"});
            i = end;
            continue;
        }
        
        // Single-line comment
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
            size_t end = i + 2;
            while (end < text.size() && text[end] != '\n') {
                end++;
            }
            tokens.push_back({start, end, "comment"});
            i = end;
            continue;
        }
        
        // Preprocessor
        if (text[i] == '#') {
            size_t end = i + 1;
            while (end < text.size() && text[end] != '\n') {
                if (text[end] == '\\') end++;  // Line continuation
                end++;
            }
            tokens.push_back({start, end, "preprocessor"});
            i = end;
            continue;
        }
        
        // String
        if (text[i] == '"' || text[i] == '\'') {
            wchar_t quote = text[i];
            size_t end = i + 1;
            while (end < text.size() && text[end] != quote) {
                if (text[end] == '\\') end++;
                end++;
            }
            if (end < text.size()) end++;
            tokens.push_back({start, end, "string"});
            i = end;
            continue;
        }
        
        // Number
        if (std::isdigit(text[i]) || (text[i] == '.' && i + 1 < text.size() && std::isdigit(text[i + 1]))) {
            size_t end = i + 1;
            while (end < text.size() && (std::isalnum(text[end]) || text[end] == '.' || text[end] == '_')) {
                end++;
            }
            tokens.push_back({start, end, "number"});
            i = end;
            continue;
        }
        
        // Identifier or keyword
        if (std::isalpha(text[i]) || text[i] == '_') {
            size_t end = i + 1;
            while (end < text.size() && (std::isalnum(text[end]) || text[end] == '_')) {
                end++;
            }
            
            std::wstring word(text.substr(start, end - start));
            
            if (keywords_.count(word)) {
                tokens.push_back({start, end, "keyword"});
            } else if (types_.count(word)) {
                tokens.push_back({start, end, "type"});
            } else {
                // Check if function call
                size_t next = end;
                while (next < text.size() && Utils::isWhitespace(text[next])) {
                    next++;
                }
                if (next < text.size() && text[next] == '(') {
                    tokens.push_back({start, end, "function"});
                }
            }
            i = end;
            continue;
        }
        
        // Operator
        size_t end = i + 1;
        tokens.push_back({start, end, "operator"});
        i = end;
    }
    
    return tokens;
}

std::vector<Token> GenericHighlighter::highlightPython(const std::wstring_view& text) const {
    // Similar to C++ but with Python-specific rules
    std::vector<Token> tokens;
    size_t i = 0;
    
    while (i < text.size()) {
        while (i < text.size() && Utils::isWhitespace(text[i])) {
            i++;
        }
        if (i >= text.size()) break;
        
        size_t start = i;
        
        // Comment
        if (text[i] == '#') {
            size_t end = i + 1;
            while (end < text.size() && text[end] != '\n') {
                end++;
            }
            tokens.push_back({start, end, "comment"});
            i = end;
            continue;
        }
        
        // String
        if (text[i] == '"' || text[i] == '\'') {
            wchar_t quote = text[i];
            bool triple = (i + 2 < text.size() && text[i + 1] == quote && text[i + 2] == quote);
            size_t end = triple ? i + 3 : i + 1;
            
            while (end < text.size()) {
                if (triple) {
                    if (end + 2 < text.size() && text[end] == quote && text[end + 1] == quote && text[end + 2] == quote) {
                        end += 3;
                        break;
                    }
                } else {
                    if (text[end] == quote) {
                        end++;
                        break;
                    }
                    if (text[end] == '\\') end++;
                }
                end++;
            }
            tokens.push_back({start, end, "string"});
            i = end;
            continue;
        }
        
        // Number
        if (std::isdigit(text[i])) {
            size_t end = i + 1;
            while (end < text.size() && (std::isalnum(text[end]) || text[end] == '.')) {
                end++;
            }
            tokens.push_back({start, end, "number"});
            i = end;
            continue;
        }
        
        // Identifier
        if (std::isalpha(text[i]) || text[i] == '_') {
            size_t end = i + 1;
            while (end < text.size() && (std::isalnum(text[end]) || text[end] == '_')) {
                end++;
            }
            
            std::wstring word(text.substr(start, end - start));
            
            if (keywords_.count(word)) {
                tokens.push_back({start, end, "keyword"});
            } else if (types_.count(word)) {
                tokens.push_back({start, end, "type"});
            }
            i = end;
            continue;
        }
        
        i++;
    }
    
    return tokens;
}

std::vector<Token> GenericHighlighter::highlightGeneric(const std::wstring_view& text) const {
    std::vector<Token> tokens;
    // Basic word highlighting
    size_t i = 0;
    
    while (i < text.size()) {
        while (i < text.size() && Utils::isWhitespace(text[i])) {
            i++;
        }
        if (i >= text.size()) break;
        
        size_t start = i;
        
        if (std::isalpha(text[i]) || text[i] == '_') {
            while (i < text.size() && (std::isalnum(text[i]) || text[i] == '_')) {
                i++;
            }
            std::wstring word(text.substr(start, i - start));
            if (keywords_.count(word)) {
                tokens.push_back({start, i, "keyword"});
            }
        } else {
            i++;
        }
    }
    
    return tokens;
}

void GenericHighlighter::addKeywords(const std::vector<std::wstring>& keywords) {
    for (const auto& kw : keywords) {
        keywords_.insert(kw);
    }
}

void GenericHighlighter::addTypes(const std::vector<std::wstring>& types) {
    for (const auto& t : types) {
        types_.insert(t);
    }
}

void GenericHighlighter::addOperators(const std::vector<std::wstring>& ops) {
    for (const auto& op : ops) {
        operators_.insert(op);
    }
}

// =============================================================================
// FIND AND REPLACE IMPLEMENTATION
// =============================================================================

FindReplace::FindReplace() = default;

std::vector<SearchResult> FindReplace::find(const std::wstring& pattern,
                                            const TextBuffer& buffer,
                                            bool caseSensitive,
                                            bool wholeWord,
                                            bool useRegex) {
    std::vector<SearchResult> results;
    if (pattern.empty()) return results;
    
    std::wstring text = buffer.text();
    
    if (useRegex) {
        try {
            std::wregex re(pattern, caseSensitive ? std::regex_constants::ECMAScript : 
                          (std::regex_constants::ECMAScript | std::regex_constants::icase));
            auto begin = std::wsregex_iterator(text.begin(), text.end(), re);
            auto end = std::wsregex_iterator();
            
            for (auto it = begin; it != end; ++it) {
                SearchResult r;
                r.position = it->position();
                r.length = it->length();
                r.line = static_cast<int>(buffer.lineFromPos(r.position));
                results.push_back(r);
            }
        } catch (...) {
            return results;
        }
    } else {
        std::wstring search = caseSensitive ? pattern : pattern;  // Would convert case for insensitive
        std::wstring target = caseSensitive ? text : text;  // Would convert case for insensitive
        
        size_t pos = 0;
        while ((pos = target.find(search, pos)) != std::wstring::npos) {
            if (wholeWord) {
                bool valid = true;
                if (pos > 0 && Utils::isAlphaNumeric(target[pos - 1])) {
                    valid = false;
                }
                if (pos + search.size() < target.size() && Utils::isAlphaNumeric(target[pos + search.size()])) {
                    valid = false;
                }
                if (!valid) {
                    pos++;
                    continue;
                }
            }
            
            SearchResult r;
            r.position = pos;
            r.length = search.size();
            r.line = static_cast<int>(buffer.lineFromPos(pos));
            results.push_back(r);
            pos += search.size();
        }
    }
    
    lastPattern_ = pattern;
    lastResults_ = results;
    return results;
}

SearchResult FindReplace::findNext(const std::wstring& pattern,
                                    const TextBuffer& buffer,
                                    size_t fromPos,
                                    bool caseSensitive) {
    auto results = find(pattern, buffer, caseSensitive, false, false);
    
    for (const auto& r : results) {
        if (r.position > fromPos) {
            return r;
        }
    }
    
    if (!results.empty()) {
        return results[0];
    }
    
    return {0, 0, 0};
}

SearchResult FindReplace::findPrev(const std::wstring& pattern,
                                    const TextBuffer& buffer,
                                    size_t fromPos,
                                    bool caseSensitive) {
    auto results = find(pattern, buffer, caseSensitive, false, false);
    
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        if (it->position < fromPos) {
            return *it;
        }
    }
    
    if (!results.empty()) {
        return results.back();
    }
    
    return {0, 0, 0};
}

size_t FindReplace::replace(TextBuffer& buffer,
                             size_t pos,
                             size_t len,
                             const std::wstring& replacement) {
    buffer.erase(pos, len);
    buffer.insert(pos, replacement.c_str(), replacement.size());
    return replacement.size();
}

size_t FindReplace::replaceAll(TextBuffer& buffer,
                                const std::wstring& pattern,
                                const std::wstring& replacement,
                                bool caseSensitive) {
    auto results = find(pattern, buffer, caseSensitive, false, false);
    
    // Replace from end to start to maintain positions
    size_t count = 0;
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        replace(buffer, it->position, it->length, replacement);
        count++;
    }
    
    return count;
}

// =============================================================================
// EDITOR IMPLEMENTATION
// =============================================================================

Editor::Editor(HWND hwnd, Config config)
    : hwnd_(hwnd)
    , config_(std::move(config))
    , buffer_(std::make_unique<TextBuffer>())
    , undoStack_(std::make_unique<UndoStack>())
    , highlighter_(std::make_unique<GenericHighlighter>("cpp"))
    , findReplace_(std::make_unique<FindReplace>())
    , minimap_(std::make_unique<Minimap>())
{
    cursors_.push_back(Cursor{});
    createFont();
}

Editor::~Editor() {
    if (hFont_) {
        DeleteObject(hFont_);
    }
}

void Editor::createFont() {
    if (hFont_) {
        DeleteObject(hFont_);
    }
    
    LOGFONTW lf = {};
    lf.lfHeight = -config_.fontSize;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, config_.fontFamily.c_str());
    
    hFont_ = CreateFontIndirectW(&lf);
    
    // Measure
    HDC hdc = GetDC(hwnd_);
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont_);
    
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    lineHeight_ = tm.tmHeight + tm.tmExternalLeading;
    
    SIZE size;
    GetTextExtentPoint32W(hdc, L"M", 1, &size);
    charWidth_ = size.cx;
    
    SelectObject(hdc, oldFont);
    ReleaseDC(hwnd_, hdc);
}

void Editor::setText(const std::wstring& text) {
    buffer_->setText(text);
    cursors_.clear();
    cursors_.push_back(Cursor{});
    needsFullRebuild_ = true;
    invalidate();
}

std::wstring Editor::text() const {
    return buffer_->text();
}

std::wstring Editor::selection() const {
    if (cursors_.empty() || cursors_[0].selection.empty()) {
        return L"";
    }
    return buffer_->substr(cursors_[0].selection.start(), cursors_[0].selection.length());
}

void Editor::insert(const std::wstring& text) {
    if (text.empty()) return;
    
    undoStack_->beginGroup();
    
    // Delete selection if any
    for (auto& cursor : cursors_) {
        if (!cursor.selection.empty()) {
            size_t start = cursor.selection.start();
            size_t len = cursor.selection.length();
            buffer_->erase(start, len);
            cursor.position = start;
            cursor.selection.move(start);
        }
    }
    
    // Insert text at primary cursor
    size_t pos = cursors_[0].position;
    buffer_->insert(pos, text.c_str(), text.size());
    cursors_[0].position += text.size();
    cursors_[0].selection.move(cursors_[0].position);
    
    undoStack_->endGroup();
    needsFullRebuild_ = true;
    invalidate();
    
    if (onChange_) onChange_();
}

void Editor::insertNewline() {
    insert(L"\n");
}

void Editor::insertTab() {
    std::wstring spaces(config_.tabSize, L' ');
    insert(spaces);
}

void Editor::backspace() {
    if (cursors_.empty()) return;
    
    undoStack_->beginGroup();
    
    for (auto& cursor : cursors_) {
        if (!cursor.selection.empty()) {
            buffer_->erase(cursor.selection.start(), cursor.selection.length());
            cursor.position = cursor.selection.start();
            cursor.selection.move(cursor.position);
        } else if (cursor.position > 0) {
            buffer_->erase(cursor.position - 1, 1);
            cursor.position--;
            cursor.selection.move(cursor.position);
        }
    }
    
    undoStack_->endGroup();
    needsFullRebuild_ = true;
    invalidate();
    
    if (onChange_) onChange_();
}

void Editor::del() {
    if (cursors_.empty()) return;
    
    undoStack_->beginGroup();
    
    for (auto& cursor : cursors_) {
        if (!cursor.selection.empty()) {
            buffer_->erase(cursor.selection.start(), cursor.selection.length());
            cursor.position = cursor.selection.start();
            cursor.selection.move(cursor.position);
        } else if (cursor.position < buffer_->length()) {
            buffer_->erase(cursor.position, 1);
        }
    }
    
    undoStack_->endGroup();
    needsFullRebuild_ = true;
    invalidate();
    
    if (onChange_) onChange_();
}

void Editor::undo() {
    auto actions = undoStack_->undo();
    for (const auto& action : actions) {
        if (action.type == UndoAction::Insert) {
            buffer_->insert(action.pos, action.text.c_str(), action.text.size());
        } else {
            buffer_->erase(action.pos, action.text.size());
        }
    }
    needsFullRebuild_ = true;
    invalidate();
    
    if (onChange_) onChange_();
}

void Editor::redo() {
    auto actions = undoStack_->redo();
    for (const auto& action : actions) {
        if (action.type == UndoAction::Insert) {
            buffer_->insert(action.pos, action.text.c_str(), action.text.size());
        } else {
            buffer_->erase(action.pos, action.text.size());
        }
    }
    needsFullRebuild_ = true;
    invalidate();
    
    if (onChange_) onChange_();
}

void Editor::selectAll() {
    if (cursors_.empty()) return;
    cursors_[0].selection.selectAll(buffer_->length());
    cursors_[0].position = buffer_->length();
    invalidate();
}

void Editor::clearSelection() {
    for (auto& cursor : cursors_) {
        cursor.selection.move(cursor.position);
    }
    invalidate();
}

bool Editor::hasSelection() const {
    for (const auto& cursor : cursors_) {
        if (!cursor.selection.empty()) return true;
    }
    return false;
}

void Editor::moveLeft(bool shift) {
    for (auto& cursor : cursors_) {
        if (cursor.position > 0) {
            cursor.position--;
            if (shift) {
                cursor.selection.extend(cursor.position);
            } else {
                cursor.selection.move(cursor.position);
            }
        }
    }
    updateDesiredColumn();
    invalidate();
}

void Editor::moveRight(bool shift) {
    for (auto& cursor : cursors_) {
        if (cursor.position < buffer_->length()) {
            cursor.position++;
            if (shift) {
                cursor.selection.extend(cursor.position);
            } else {
                cursor.selection.move(cursor.position);
            }
        }
    }
    updateDesiredColumn();
    invalidate();
}

void Editor::moveUp(bool shift) {
    for (auto& cursor : cursors_) {
        size_t line = buffer_->lineFromPos(cursor.position);
        if (line > 0) {
            size_t lineStart = buffer_->lineStart(line);
            size_t col = cursor.position - lineStart;
            
            size_t prevLineStart = buffer_->lineStart(line - 1);
            size_t prevLineLen = buffer_->lineEnd(line - 1) - prevLineStart;
            
            cursor.position = prevLineStart + std::min(col, prevLineLen);
            
            if (shift) {
                cursor.selection.extend(cursor.position);
            } else {
                cursor.selection.move(cursor.position);
            }
        }
    }
    invalidate();
    
    if (onCursorChange_) {
        onCursorChange_(currentLine(), currentColumn());
    }
}

void Editor::moveDown(bool shift) {
    for (auto& cursor : cursors_) {
        size_t line = buffer_->lineFromPos(cursor.position);
        if (line < buffer_->lineCount() - 1) {
            size_t lineStart = buffer_->lineStart(line);
            size_t col = cursor.position - lineStart;
            
            size_t nextLineStart = buffer_->lineStart(line + 1);
            size_t nextLineLen = buffer_->lineEnd(line + 1) - nextLineStart;
            
            cursor.position = nextLineStart + std::min(col, nextLineLen);
            
            if (shift) {
                cursor.selection.extend(cursor.position);
            } else {
                cursor.selection.move(cursor.position);
            }
        }
    }
    invalidate();
    
    if (onCursorChange_) {
        onCursorChange_(currentLine(), currentColumn());
    }
}

void Editor::moveHome(bool shift) {
    for (auto& cursor : cursors_) {
        size_t lineStart = buffer_->lineStart(buffer_->lineFromPos(cursor.position));
        cursor.position = lineStart;
        
        if (shift) {
            cursor.selection.extend(cursor.position);
        } else {
            cursor.selection.move(cursor.position);
        }
    }
    updateDesiredColumn();
    invalidate();
}

void Editor::moveEnd(bool shift) {
    for (auto& cursor : cursors_) {
        size_t lineEnd = buffer_->lineEnd(buffer_->lineFromPos(cursor.position));
        if (lineEnd > 0 && buffer_->at(lineEnd - 1) == '\n') {
            lineEnd--;
        }
        cursor.position = lineEnd;
        
        if (shift) {
            cursor.selection.extend(cursor.position);
        } else {
            cursor.selection.move(cursor.position);
        }
    }
    updateDesiredColumn();
    invalidate();
}

void Editor::movePageUp(bool shift) {
    for (int i = 0; i < visibleLineCount_ && !cursors_.empty(); ++i) {
        moveUp(shift);
    }
}

void Editor::movePageDown(bool shift) {
    for (int i = 0; i < visibleLineCount_ && !cursors_.empty(); ++i) {
        moveDown(shift);
    }
}

void Editor::moveWordLeft(bool shift) {
    for (auto& cursor : cursors_) {
        if (cursor.position > 0) {
            cursor.position = wordStart(cursor.position - 1);
            if (shift) {
                cursor.selection.extend(cursor.position);
            } else {
                cursor.selection.move(cursor.position);
            }
        }
    }
    updateDesiredColumn();
    invalidate();
}

void Editor::moveWordRight(bool shift) {
    for (auto& cursor : cursors_) {
        if (cursor.position < buffer_->length()) {
            cursor.position = wordEnd(cursor.position + 1);
            if (shift) {
                cursor.selection.extend(cursor.position);
            } else {
                cursor.selection.move(cursor.position);
            }
        }
    }
    updateDesiredColumn();
    invalidate();
}

void Editor::goToLine(int line) {
    if (line >= 0 && static_cast<size_t>(line) < buffer_->lineCount()) {
        cursors_[0].position = buffer_->lineStart(line);
        cursors_[0].selection.move(cursors_[0].position);
        scrollToLine(line);
        invalidate();
    }
}

void Editor::goToPosition(size_t pos) {
    if (pos <= buffer_->length()) {
        cursors_[0].position = pos;
        cursors_[0].selection.move(pos);
        scrollToPosition(pos);
        invalidate();
    }
}

void Editor::cut() {
    copy();
    if (hasSelection()) {
        del();
    }
}

void Editor::copy() {
    if (!hasSelection()) return;
    
    std::wstring sel = selection();
    if (!sel.empty()) {
        if (OpenClipboard(hwnd_)) {
            EmptyClipboard();
            
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sel.size() + 1) * sizeof(wchar_t));
            if (hMem) {
                wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
                if (pMem) {
                    wcscpy_s(pMem, sel.size() + 1, sel.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                GlobalFree(hMem);
            }
            
            CloseClipboard();
        }
    }
}

void Editor::paste() {
    if (!OpenClipboard(hwnd_)) return;
    
    HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
    if (hMem) {
        wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
        if (pMem) {
            std::wstring text(pMem);
            insert(text);
            GlobalUnlock(hMem);
        }
    }
    
    CloseClipboard();
}

void Editor::find(const std::wstring& pattern, bool caseSensitive) {
    findPattern_ = pattern;
    findResults_ = findReplace_->find(pattern, *buffer_, caseSensitive);
    currentFindResult_ = 0;
    invalidate();
}

void Editor::findNext() {
    if (findResults_.empty()) return;
    currentFindResult_ = (currentFindResult_ + 1) % findResults_.size();
    goToPosition(findResults_[currentFindResult_].position);
}

void Editor::findPrev() {
    if (findResults_.empty()) return;
    currentFindResult_ = (currentFindResult_ + findResults_.size() - 1) % findResults_.size();
    goToPosition(findResults_[currentFindResult_].position);
}

void Editor::replace(const std::wstring& replacement) {
    if (findResults_.empty() || currentFindResult_ >= findResults_.size()) return;
    
    auto& result = findResults_[currentFindResult_];
    findReplace_->replace(*buffer_, result.position, result.length, replacement);
    findResults_ = findReplace_->find(findPattern_, *buffer_, true);
    invalidate();
}

void Editor::replaceAll(const std::wstring& pattern, const std::wstring& replacement) {
    findReplace_->replaceAll(*buffer_, pattern, replacement);
    findResults_.clear();
    needsFullRebuild_ = true;
    invalidate();
}

void Editor::scrollToLine(int line) {
    int halfVisible = visibleLineCount_ / 2;
    int targetFirst = line - halfVisible;
    
    if (targetFirst < 0) targetFirst = 0;
    
    size_t maxLine = buffer_->lineCount();
    if (static_cast<size_t>(targetFirst + visibleLineCount_) > maxLine) {
        targetFirst = static_cast<int>(maxLine) - visibleLineCount_;
        if (targetFirst < 0) targetFirst = 0;
    }
    
    if (targetFirst != firstVisibleLine_) {
        firstVisibleLine_ = targetFirst;
        invalidate();
    }
}

void Editor::scrollToPosition(size_t pos) {
    size_t line = buffer_->lineFromPos(pos);
    scrollToLine(static_cast<int>(line));
}

void Editor::render(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    
    // Update visibility
    updateVisibleRange();
    
    // Rebuild if needed
    if (needsFullRebuild_) {
        rebuildRenderLines();
        needsFullRebuild_ = false;
    }
    
    // Background
    RECT bgRect = clientRect;
    HBRUSH bgBrush = CreateSolidBrush(config_.bgColor);
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);
    
    // Line numbers
    int lineNumberWidth = config_.showLineNumbers ? 60 : 0;
    
    if (config_.showLineNumbers) {
        RECT lineNumRect;
        lineNumRect.left = 0;
        lineNumRect.right = lineNumberWidth;
        lineNumRect.top = 0;
        lineNumRect.bottom = clientRect.bottom;
        
        HBRUSH lineNumBrush = CreateSolidBrush(config_.lineNumberBg);
        FillRect(hdc, &lineNumRect, lineNumBrush);
        DeleteObject(lineNumBrush);
    }
    
    // Main content area
    RECT contentRect = clientRect;
    contentRect.left = lineNumberWidth;
    
    // Draw lines
    int y = 0;
    size_t currentLineIndex = buffer_->lineFromPos(cursors_[0].position);
    
    for (size_t i = static_cast<size_t>(firstVisibleLine_); 
         i < std::min(renderLines_.size(), static_cast<size_t>(firstVisibleLine_ + visibleLineCount_ + 1));
         ++i) {
        
        bool isCurrentLine = (i == currentLineIndex);
        
        // Current line highlight
        if (isCurrentLine) {
            RECT highlightRect;
            highlightRect.left = lineNumberWidth;
            highlightRect.right = clientRect.right;
            highlightRect.top = y;
            highlightRect.bottom = y + lineHeight_;
            
            HBRUSH highlightBrush = CreateSolidBrush(config_.currentLineBg);
            FillRect(hdc, &highlightRect, highlightBrush);
            DeleteObject(highlightBrush);
        }
        
        // Line number
        if (config_.showLineNumbers) {
            drawLineNumber(hdc, static_cast<int>(i) + 1, y, isCurrentLine);
        }
        
        // Line content
        drawLine(hdc, renderLines_[i], y + lineNumberWidth, isCurrentLine);
        
        y += lineHeight_;
    }
    
    // Selection
    for (const auto& cursor : cursors_) {
        if (!cursor.selection.empty()) {
            drawSelection(hdc, cursor.selection.start(), cursor.selection.end());
        }
    }
    
    // Find highlights
    drawFindHighlights(hdc);
    
    // Cursor
    drawCursor(hdc);
    
    // Minimap
    if (config_.showMinimap) {
        drawMinimap(hdc);
    }
}

void Editor::invalidate() {
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Editor::onChar(wchar_t ch) {
    if (ch == '\r' || ch == '\n') {
        insertNewline();
    } else if (ch == '\t') {
        insertTab();
    } else if (ch == '\b') {
        backspace();
    } else if (ch >= ' ' || ch == '\t') {
        // Auto-close brackets
        if (config_.autoClose) {
            wchar_t closeChar = getCloseChar(ch);
            if (closeChar && autoClose(ch)) {
                return;
            }
        }
        
        insert(std::wstring(1, ch));
    }
}

void Editor::onKeyDown(int vk, bool shift, bool ctrl, bool alt) {
    (void)alt;  // Unused for now
    
    switch (vk) {
        case VK_LEFT:
            if (ctrl) moveWordLeft(shift);
            else moveLeft(shift);
            break;
        case VK_RIGHT:
            if (ctrl) moveWordRight(shift);
            else moveRight(shift);
            break;
        case VK_UP:
            moveUp(shift);
            break;
        case VK_DOWN:
            moveDown(shift);
            break;
        case VK_HOME:
            if (ctrl) goToPosition(0);
            else moveHome(shift);
            break;
        case VK_END:
            if (ctrl) goToPosition(buffer_->length());
            else moveEnd(shift);
            break;
        case VK_PRIOR:
            movePageUp(shift);
            break;
        case VK_NEXT:
            movePageDown(shift);
            break;
        case VK_DELETE:
            del();
            break;
        case VK_BACK:
            backspace();
            break;
        case 'A':
            if (ctrl) selectAll();
            break;
        case 'C':
            if (ctrl) copy();
            break;
        case 'V':
            if (ctrl) paste();
            break;
        case 'X':
            if (ctrl) cut();
            break;
        case 'Z':
            if (ctrl) {
                if (shift) redo();
                else undo();
            }
            break;
        case 'Y':
            if (ctrl) redo();
            break;
    }
}

void Editor::onMouseDown(int x, int y, bool left, bool right) {
    (void)right;
    if (!left) return;
    
    isSelecting_ = true;
    selectionStartX_ = x;
    selectionStartY_ = y;
    
    size_t pos = posFromPoint(x, y);
    
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        cursors_[0].selection.extend(pos);
    } else {
        cursors_[0].position = pos;
        cursors_[0].selection.move(pos);
    }
    
    invalidate();
}

void Editor::onMouseMove(int x, int y) {
    if (!isSelecting_) return;
    
    size_t pos = posFromPoint(x, y);
    cursors_[0].selection.extend(pos);
    cursors_[0].position = pos;
    
    // Scroll if near edges
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    
    if (y < lineHeight_) {
        if (firstVisibleLine_ > 0) {
            firstVisibleLine_--;
            invalidate();
        }
    } else if (y > clientRect.bottom - lineHeight_) {
        firstVisibleLine_++;
        invalidate();
    } else {
        invalidate();
    }
}

void Editor::onMouseUp(int x, int y) {
    (void)x;
    (void)y;
    isSelecting_ = false;
}

void Editor::onMouseWheel(int delta, bool shift) {
    (void)shift;
    
    int lines = delta / 120 * config_.scrollSpeed;
    firstVisibleLine_ -= lines;
    
    if (firstVisibleLine_ < 0) firstVisibleLine_ = 0;
    
    size_t maxLine = buffer_->lineCount();
    if (static_cast<size_t>(firstVisibleLine_) > maxLine) {
        firstVisibleLine_ = static_cast<int>(maxLine) - 1;
    }
    
    invalidate();
}

void Editor::onSize(int width, int height) {
    (void)width;
    (void)height;
    
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    
    visibleLineCount_ = clientRect.bottom / lineHeight_;
    
    invalidate();
}

int Editor::currentLine() const {
    return static_cast<int>(buffer_->lineFromPos(cursors_[0].position));
}

int Editor::currentColumn() const {
    size_t lineStart = buffer_->lineStart(buffer_->lineFromPos(cursors_[0].position));
    return static_cast<int>(cursors_[0].position - lineStart);
}

int Editor::lineCount() const {
    return static_cast<int>(buffer_->lineCount());
}

void Editor::setConfig(const Config& config) {
    config_ = config;
    createFont();
    invalidate();
}

void Editor::setHighlighter(std::unique_ptr<SyntaxHighlighter> highlighter) {
    highlighter_ = std::move(highlighter);
    needsFullRebuild_ = true;
    invalidate();
}

void Editor::setLanguage(const std::string& lang) {
    highlighter_ = std::make_unique<GenericHighlighter>(lang);
    needsFullRebuild_ = true;
    invalidate();
}

// Private methods

void Editor::rebuildRenderLines() {
    size_t totalLines = buffer_->lineCount();
    renderLines_.resize(totalLines);
    
    for (size_t i = 0; i < totalLines; ++i) {
        RenderLine& line = renderLines_[i];
        line.lineNumber = i;
        line.start = buffer_->lineStart(i);
        line.end = buffer_->lineEnd(i);
        line.text = buffer_->lineContent(i);
        line.dirty = true;
        
        if (highlighter_) {
            line.tokens = highlighter_->highlight(line.text);
        }
    }
}

void Editor::updateVisibleRange() {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    visibleLineCount_ = clientRect.bottom / lineHeight_;
}

void Editor::drawLine(HDC hdc, const RenderLine& line, int y, bool isCurrentLine) {
    (void)isCurrentLine;
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont_);
    SetBkMode(hdc, TRANSPARENT);
    
    int x = config_.showLineNumbers ? 60 : 0;
    
    if (!line.tokens.empty()) {
        size_t textPos = 0;
        for (const auto& token : line.tokens) {
            // Draw pre-token text
            if (token.start > textPos) {
                size_t len = token.start - textPos;
                std::wstring segment(line.text.substr(textPos, len));
                SetTextColor(hdc, config_.textColor);
                TextOutW(hdc, x, y, segment.c_str(), static_cast<int>(segment.length()));
                x += static_cast<int>(segment.length()) * charWidth_;
            }
            
            // Draw token
            size_t tokenLen = token.end - token.start;
            std::wstring tokenText(line.text.substr(token.start, tokenLen));
            
            auto it = config_.syntaxColors.find(token.type);
            uint32_t color = (it != config_.syntaxColors.end()) ? it->second : config_.textColor;
            SetTextColor(hdc, color);
            
            TextOutW(hdc, x, y, tokenText.c_str(), static_cast<int>(tokenText.length()));
            x += static_cast<int>(tokenText.length()) * charWidth_;
            
            textPos = token.end;
        }
        
        // Draw remaining text
        if (textPos < line.text.length()) {
            std::wstring remaining(line.text.substr(textPos));
            SetTextColor(hdc, config_.textColor);
            TextOutW(hdc, x, y, remaining.c_str(), static_cast<int>(remaining.length()));
        }
    } else {
        // No tokens, draw plain text
        SetTextColor(hdc, config_.textColor);
        TextOutW(hdc, x, y, line.text.c_str(), static_cast<int>(line.text.length()));
    }
    
    SelectObject(hdc, oldFont);
}

void Editor::drawSelection(HDC hdc, size_t start, size_t end) {
    if (start >= end) return;
    
    int lineNumberWidth = config_.showLineNumbers ? 60 : 0;
    size_t startLine = buffer_->lineFromPos(start);
    size_t endLine = buffer_->lineFromPos(end);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont_);
    
    for (size_t line = startLine; line <= endLine && line < renderLines_.size(); ++line) {
        size_t lineStart = buffer_->lineStart(line);
        size_t lineEnd = buffer_->lineEnd(line);
        
        size_t selStart = std::max(start, lineStart);
        size_t selEnd = std::min(end, lineEnd);
        
        if (selStart >= selEnd) continue;
        
        int y = yFromLine(static_cast<int>(line));
        int x1 = lineNumberWidth + xFromCol(static_cast<int>(selStart - lineStart), static_cast<int>(line));
        int x2 = lineNumberWidth + xFromCol(static_cast<int>(selEnd - lineStart), static_cast<int>(line));
        
        RECT rect;
        rect.left = x1;
        rect.right = x2;
        rect.top = y;
        rect.bottom = y + lineHeight_;
        
        HBRUSH brush = CreateSolidBrush(config_.selectionBg);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }
    
    SelectObject(hdc, oldFont);
}

void Editor::drawCursor(HDC hdc) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBlink_);
    
    if (elapsed.count() > config_.cursorBlinkMs) {
        cursorVisible_ = !cursorVisible_;
        lastBlink_ = now;
    }
    
    if (!cursorVisible_) return;
    
    size_t pos = cursors_[0].position;
    size_t line = buffer_->lineFromPos(pos);
    size_t lineStart = buffer_->lineStart(line);
    size_t col = pos - lineStart;
    
    int lineNumberWidth = config_.showLineNumbers ? 60 : 0;
    int y = yFromLine(static_cast<int>(line));
    int x = lineNumberWidth + xFromCol(static_cast<int>(col), static_cast<int>(line));
    
    RECT rect;
    rect.left = x;
    rect.right = x + 2;
    rect.top = y;
    rect.bottom = y + lineHeight_;
    
    HBRUSH brush = CreateSolidBrush(config_.cursorColor);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void Editor::drawMinimap(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    
    int minimapWidth = 100;
    int minimapX = clientRect.right - minimapWidth;
    int minimapY = 0;
    int minimapHeight = clientRect.bottom;
    
    minimap_->update(*buffer_, renderLines_, firstVisibleLine_, visibleLineCount_, clientRect.bottom);
    minimap_->render(hdc, minimapX, minimapY, minimapWidth, minimapHeight, config_.bgColor);
}

void Editor::drawLineNumber(HDC hdc, int line, int y, bool isCurrentLine) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont_);
    
    std::wstring num = std::to_wstring(line);
    
    SetTextColor(hdc, isCurrentLine ? config_.textColor : config_.lineNumberColor);
    SetBkMode(hdc, TRANSPARENT);
    
    RECT rect;
    rect.left = 0;
    rect.right = 55;
    rect.top = y;
    rect.bottom = y + lineHeight_;
    
    DrawTextW(hdc, num.c_str(), -1, &rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdc, oldFont);
}

void Editor::drawFindHighlights(HDC hdc) {
    if (findResults_.empty()) return;
    
    int lineNumberWidth = config_.showLineNumbers ? 60 : 0;
    
    for (const auto& result : findResults_) {
        int y = yFromLine(result.line);
        if (y < 0 || y > visibleLineCount_ * lineHeight_) continue;
        
        size_t lineStart = buffer_->lineStart(result.line);
        int x = lineNumberWidth + xFromCol(static_cast<int>(result.position - lineStart), result.line);
        int width = static_cast<int>(result.length) * charWidth_;
        
        RECT rect;
        rect.left = x;
        rect.right = x + width;
        rect.top = y;
        rect.bottom = y + lineHeight_;
        
        HBRUSH brush = CreateSolidBrush(config_.searchHighlightBg);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }
}

size_t Editor::posFromPoint(int x, int y) const {
    int lineNumberWidth = config_.showLineNumbers ? 60 : 0;
    
    if (x < lineNumberWidth) x = lineNumberWidth;
    x -= lineNumberWidth;
    
    int line = lineFromY(y);
    if (line < 0) line = 0;
    if (static_cast<size_t>(line) >= buffer_->lineCount()) {
        line = static_cast<int>(buffer_->lineCount()) - 1;
    }
    
    int col = colFromX(x, line);
    
    size_t lineStart = buffer_->lineStart(line);
    return lineStart + col;
}

int Editor::lineFromY(int y) const {
    return firstVisibleLine_ + y / lineHeight_;
}

int Editor::colFromX(int x, int line) const {
    if (line < 0 || static_cast<size_t>(line) >= renderLines_.size()) return 0;
    
    int col = x / charWidth_;
    size_t lineLen = buffer_->lineEnd(line) - buffer_->lineStart(line);
    if (static_cast<size_t>(col) > lineLen) col = static_cast<int>(lineLen);
    
    return col;
}

int Editor::xFromCol(int col, int line) const {
    (void)line;
    return col * charWidth_;
}

int Editor::yFromLine(int line) const {
    return (line - firstVisibleLine_) * lineHeight_;
}

bool Editor::isWordChar(wchar_t ch) const {
    return std::isalnum(ch) || ch == '_';
}

size_t Editor::wordStart(size_t pos) const {
    if (pos == 0) return 0;
    
    // Skip back over whitespace
    while (pos > 0 && Utils::isWhitespace(buffer_->at(pos))) {
        pos--;
    }
    
    // Find word start
    bool isWord = isWordChar(buffer_->at(pos));
    while (pos > 0) {
        wchar_t prev = buffer_->at(pos - 1);
        if (Utils::isWhitespace(prev)) break;
        if (isWordChar(prev) != isWord) break;
        pos--;
    }
    
    return pos;
}

size_t Editor::wordEnd(size_t pos) const {
    if (pos >= buffer_->length()) return buffer_->length();
    
    // Skip forward over whitespace
    while (pos < buffer_->length() && Utils::isWhitespace(buffer_->at(pos))) {
        pos++;
    }
    
    // Find word end
    bool isWord = isWordChar(buffer_->at(pos));
    while (pos < buffer_->length()) {
        wchar_t next = buffer_->at(pos);
        if (Utils::isWhitespace(next)) break;
        if (isWordChar(next) != isWord) break;
        pos++;
    }
    
    return pos;
}

void Editor::clampCursor() {
    for (auto& cursor : cursors_) {
        if (cursor.position > buffer_->length()) {
            cursor.position = buffer_->length();
        }
        if (cursor.selection.anchor > buffer_->length()) {
            cursor.selection.anchor = buffer_->length();
        }
        if (cursor.selection.cursor > buffer_->length()) {
            cursor.selection.cursor = buffer_->length();
        }
    }
}

void Editor::updateDesiredColumn() {
    size_t lineStart = buffer_->lineStart(buffer_->lineFromPos(cursors_[0].position));
    cursors_[0].desiredColumn = static_cast<int>(cursors_[0].position - lineStart);
}

bool Editor::autoClose(wchar_t ch) {
    wchar_t closeChar = getCloseChar(ch);
    if (!closeChar) return false;
    
    // Insert both characters and position cursor between
    std::wstring text;
    text.push_back(ch);
    text.push_back(closeChar);
    
    undoStack_->beginGroup();
    
    if (!cursors_[0].selection.empty()) {
        // Wrap selection
        size_t start = cursors_[0].selection.start();
        size_t end = cursors_[0].selection.end();
        
        buffer_->insert(start, &ch, 1);
        buffer_->insert(end + 1, &closeChar, 1);
        
        cursors_[0].position = end + 1;
        cursors_[0].selection.move(cursors_[0].position);
    } else {
        insert(text);
        cursors_[0].position--;
        cursors_[0].selection.move(cursors_[0].position);
    }
    
    undoStack_->endGroup();
    
    return true;
}

wchar_t Editor::getCloseChar(wchar_t open) const {
    switch (open) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '"': return '"';
        case '\'': return '\'';
        default: return 0;
    }
}

// =============================================================================
// MINIMAP IMPLEMENTATION
// =============================================================================

Minimap::Minimap() = default;

void Minimap::update(const TextBuffer& buffer,
                     const std::vector<RenderLine>& lines,
                     int firstVisibleLine,
                     int visibleLines,
                     int editorHeight) {
    lines_.clear();
    firstVisible_ = firstVisibleLine;
    visibleCount_ = visibleLines;
    editorHeight_ = editorHeight;
    
    size_t totalLines = buffer.lineCount();
    lines_.reserve(totalLines);
    
    for (size_t i = 0; i < totalLines && i < lines.size(); ++i) {
        MinimapLine ml;
        ml.length = static_cast<int>(lines[i].text.length());
        
        // Generate colored segments (simplified)
        for (const auto& token : lines[i].tokens) {
            ml.segments.push_back({static_cast<int>(token.start), static_cast<int>(token.end - token.start)});
        }
        
        lines_.push_back(std::move(ml));
    }
}

void Minimap::render(HDC hdc, int x, int y, int width, int height, uint32_t bgColor) const {
    // Background
    RECT rect;
    rect.left = x;
    rect.right = x + width;
    rect.top = y;
    rect.bottom = y + height;
    
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);
    
    // Visible area indicator
    float scale = static_cast<float>(height) / lines_.size();
    int indicatorY = y + static_cast<int>(firstVisible_ * scale);
    int indicatorHeight = static_cast<int>(visibleCount_ * scale);
    
    RECT indicatorRect;
    indicatorRect.left = x;
    indicatorRect.right = x + width;
    indicatorRect.top = indicatorY;
    indicatorRect.bottom = indicatorY + indicatorHeight;
    
    HBRUSH indicatorBrush = CreateSolidBrush(0x40404040);
    FillRect(hdc, &indicatorRect, indicatorBrush);
    DeleteObject(indicatorBrush);
    
    // Lines (simplified - just dots)
    int lineY = y;
    for (size_t i = 0; i < lines_.size() && lineY < y + height; ++i) {
        int lineWidth = static_cast<int>(lines_[i].length * scale * 2);
        lineWidth = std::min(lineWidth, width - 5);
        lineWidth = std::max(lineWidth, 2);
        
        RECT lineRect;
        lineRect.left = x + 2;
        lineRect.right = x + 2 + lineWidth;
        lineRect.top = lineY;
        lineRect.bottom = lineY + 1;
        
        HBRUSH lineBrush = CreateSolidBrush(0xFF808080);
        FillRect(hdc, &lineRect, lineBrush);
        DeleteObject(lineBrush);
        
        lineY++;
    }
}

int Minimap::lineFromY(int y, int height) const {
    float scale = static_cast<float>(height) / lines_.size();
    return static_cast<int>(y / scale);
}

// =============================================================================
// WINDOW CLASS IMPLEMENTATION
// =============================================================================

std::unordered_map<HWND, std::unique_ptr<Editor>> EditorWindow::editors_;

bool EditorWindow::Register(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_IBEAM));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = ClassName();
    
    return RegisterClassExW(&wc) != 0;
}

Editor* EditorWindow::Create(HWND parent, int x, int y, int width, int height, const Config& config) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    
    HWND hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        ClassName(),
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
        x, y, width, height,
        parent,
        nullptr,
        hInstance,
        nullptr
    );
    
    if (!hwnd) return nullptr;
    
    auto editor = std::make_unique<Editor>(hwnd, config);
    Editor* ptr = editor.get();
    editors_[hwnd] = std::move(editor);
    
    return ptr;
}

Editor* EditorWindow::FromHwnd(HWND hwnd) {
    auto it = editors_.find(hwnd);
    return it != editors_.end() ? it->second.get() : nullptr;
}

LRESULT CALLBACK EditorWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Editor* editor = FromHwnd(hwnd);
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (editor) {
                editor->render(hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_SIZE: {
            if (editor) {
                editor->onSize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }
        
        case WM_CHAR: {
            if (editor) {
                editor->onChar(static_cast<wchar_t>(wParam));
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (editor) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                editor->onKeyDown(static_cast<int>(wParam), shift, ctrl, alt);
            }
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            if (editor) {
                editor->onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true, false);
            }
            SetFocus(hwnd);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (editor && (wParam & MK_LBUTTON)) {
                editor->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            if (editor) {
                editor->onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;
        }
        
        case WM_MOUSEWHEEL: {
            if (editor) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                editor->onMouseWheel(delta, shift);
            }
            return 0;
        }
        
        case WM_SETFOCUS: {
            CreateCaret(hwnd, nullptr, 2, 20);
            ShowCaret(hwnd);
            return 0;
        }
        
        case WM_KILLFOCUS: {
            DestroyCaret();
            return 0;
        }
        
        case WM_DESTROY: {
            editors_.erase(hwnd);
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// =============================================================================
// UTILITY IMPLEMENTATION
// =============================================================================

namespace Utils {

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), size);
    return result;
}

std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::vector<std::wstring> split(const std::wstring& text, wchar_t delim) {
    std::vector<std::wstring> result;
    size_t start = 0;
    size_t end = text.find(delim);
    
    while (end != std::wstring::npos) {
        result.push_back(text.substr(start, end - start));
        start = end + 1;
        end = text.find(delim, start);
    }
    
    result.push_back(text.substr(start));
    return result;
}

std::wstring join(const std::vector<std::wstring>& parts, const std::wstring& delim) {
    std::wstring result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += delim;
        result += parts[i];
    }
    return result;
}

std::wstring trim(const std::wstring& text) {
    size_t start = text.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return {};
    size_t end = text.find_last_not_of(L" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool isWhitespace(wchar_t ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool isAlphaNumeric(wchar_t ch) {
    return std::isalnum(ch) || ch == '_';
}

} // namespace Utils

} // namespace MiniMonaco

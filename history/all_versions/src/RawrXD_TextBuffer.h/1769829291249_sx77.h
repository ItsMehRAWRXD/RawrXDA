#pragma once
#include "RawrXD_Win32_Foundation.h"
#include <vector>
#include <memory>

namespace RawrXD {

// A Rope data structure for efficient text editing of large files.
// Handles UTF-16 internally (wchar_t) for compatibility with Win32 APIs.
class TextBuffer {
public:
    static const int MAX_LEAF_SIZE = 128; // Tuning parameter

    struct Node {
        bool isLeaf;
        int weight;    // Total characters in left subtree
        int height;    // Tree height for balancing
        int lineBreakCount; // Total newlines in this node (leaf) or left subtree (internal)
        
        // Leaf specific
        std::wstring text;
        
        // Internal specific
        Node* left = nullptr;
        Node* right = nullptr;
        
        Node() : isLeaf(true), weight(0), height(0), lineBreakCount(0), left(nullptr), right(nullptr) {}
        ~Node();
    };

private:
    Node* root;
    int totalLength;
    int totalLineCount;

    // Helpers
    int length(const Node* node) const; // Traverse if needed, or helper
    int lines(const Node* node) const;
    Node* createLeaf(const std::wstring& s);
    Node* createInternal(Node* l, Node* r);
    int countNewlines(const std::wstring& s) const;
    
    // Core Rope operations
    Node* insert(Node* node, int pos, const std::wstring& s);
    Node* remove(Node* node, int pos, int len);
    Node* split(Node* node, int pos, Node** rightTree);
    Node* concat(Node* left, Node* right);
    Node* balance(Node* node);
    void rebuild(Node* node, std::vector<Node*>& leaves);
    
    wchar_t charAt(const Node* node, int pos) const;
    void getSubstring(const Node* node, int pos, int len, std::wstring& result) const;
    
    // Line ops
    int getLineStart(const Node* node, int targetLine) const;
    int getLineFromPos(const Node* node, int targetPos) const;

public:
    TextBuffer();
    ~TextBuffer();
    
    // No copy
    TextBuffer(const TextBuffer&) = delete;
    TextBuffer& operator=(const TextBuffer&) = delete;

    // Operations
    void setText(const String& text);
    void insert(int pos, const String& text);
    void remove(int pos, int len);
    void clear();
    
    String text() const; // Warning: creates full copy
    String substring(int pos, int len) const;
    wchar_t charAt(int pos) const;
    
    int length() const { return totalLength; }
    int lineCount() const { return totalLineCount + 1; } // +1 because 0 newlines = 1 line
    
    // Line management
    int getLineStart(int line) const;
    int getLineEnd(int line) const;
    int getLineLength(int line) const;
    
    int lineFromPosition(int pos) const;
    
    // Debug
    int height() const { return root ? root->height : 0; }
};

} // namespace RawrXD

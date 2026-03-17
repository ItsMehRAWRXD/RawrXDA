#include "RawrXD_TextBuffer.h"
#include <algorithm>
#include <stack>

namespace RawrXD {

TextBuffer::Node::~Node() {
    if (!isLeaf) {
        if (left) delete left;
        if (right) delete right;
    }
}

TextBuffer::TextBuffer() : root(nullptr), totalLength(0), totalLineCount(0) {
    clear();
}

TextBuffer::~TextBuffer() {
    if (root) delete root;
}

int TextBuffer::countNewlines(const std::wstring& s) const {
    int count = 0;
    for (wchar_t c : s) {
        if (c == L'\n') count++;
    }
    return count;
}

void TextBuffer::clear() {
    if (root) delete root;
    root = new Node(); // Empty leaf
    totalLength = 0;
    totalLineCount = 0;
}

TextBuffer::Node* TextBuffer::createLeaf(const std::wstring& s) {
    Node* n = new Node();
    n->isLeaf = true;
    n->text = s;
    n->weight = (int)s.length(); // For leaf, weight is just length
    n->lineBreakCount = countNewlines(s);
    n->height = 0;
    return n;
}

TextBuffer::Node* TextBuffer::createInternal(Node* l, Node* r) {
    Node* n = new Node();
    n->isLeaf = false;
    n->left = l;
    n->right = r;
    
    // Calculate weight (length of left subtree)
    if (l) {
        if (l->isLeaf) n->weight = (int)l->text.length();
        else n->weight = length(l); 
    } else {
        n->weight = 0;
    }
    
    n->lineBreakCount = (l ? l->lineBreakCount : 0) + (r ? r->lineBreakCount : 0);
    n->height = 1 + std::max(l ? l->height : 0, r ? r->height : 0);
    return n;
}

int TextBuffer::length(const Node* node) const {
    if (!node) return 0;
    if (node->isLeaf) return (int)node->text.length();
    return node->weight + length(node->right);
}

int TextBuffer::lines(const Node* node) const {
    if (!node) return 0;
    return node->lineBreakCount;
}

// ---- Operations ----

void TextBuffer::insert(int pos, const String& textStr) {
    if (pos < 0) pos = 0;
    if (pos > totalLength) pos = totalLength;
    if (textStr.isEmpty()) return;
    
    std::wstring s = textStr.toStdWString();
    root = insert(root, pos, s);
    
    totalLength += (int)s.length();
    totalLineCount += countNewlines(s);
    
    // Rebalance occasionally? Insert logic handles small rebalances usually.
    // Implementation details omitted for brevity, using simple recursive insert.
}

TextBuffer::Node* TextBuffer::insert(Node* node, int pos, const std::wstring& s) {
    // If empty root
    if (!node) return createLeaf(s);
    
    if (node->isLeaf) {
        // If small enough, just append
        if (node->text.length() + s.length() < MAX_LEAF_SIZE) {
            node->text.insert(pos, s);
            node->weight = (int)node->text.length(); // Leaf weight = length
            node->lineBreakCount = countNewlines(node->text);
            return node;
        }
        
        // Split leaf
        std::wstring leftText = node->text.substr(0, pos);
        std::wstring rightText = node->text.substr(pos);
        
        delete node; // Replaced by internal
        
        Node* l = createLeaf(leftText);
        Node* curr = l; // Insert new text here
        
        // Insert s
        Node* newPart = createLeaf(s);
        Node* r = createLeaf(rightText);
        
        // Combine (Left + New) + Right
        return createInternal(createInternal(l, newPart), r);
    }
    
    // Internal node
    if (pos < node->weight) {
        node->left = insert(node->left, pos, s);
        node->weight += (int)s.length();
        node->lineBreakCount = lines(node->left) + lines(node->right); // Update total lines
    } else {
        node->right = insert(node->right, pos - node->weight, s);
        node->lineBreakCount = lines(node->left) + lines(node->right);
    }
    
    // Rebalance check could go here (AVL/RedBlack style)
    // For simplicity, naive rope for now.
    return node;
}

void TextBuffer::remove(int pos, int len) {
    if (len <= 0) return;
    if (pos < 0) pos = 0;
    if (pos >= totalLength) return;
    if (pos + len > totalLength) len = totalLength - pos;
    
    // Split at pos
    Node *leftTree = nullptr, *rightTree = nullptr;
    root = split(root, pos, &rightTree);
    leftTree = root;
    
    // Split rightTree at len
    Node *midTree = nullptr, *finalRightTree = nullptr;
    rightTree = split(rightTree, len, &finalRightTree);
    midTree = rightTree; // This is the part to delete
    
    // Clean up deleted part
    delete midTree;
    
    // Concat left and finalRight
    root = concat(leftTree, finalRightTree);
    
    totalLength -= len;
    // totalLineCount need recalc or delta
    // Since we regenerated the tree, we should just read it from root usually, 
    // except root->lineBreakCount works if we maintained it correctly.
    totalLineCount = lines(root);
}

TextBuffer::Node* TextBuffer::split(Node* node, int pos, Node** rightTree) {
    if (!node) {
        *rightTree = nullptr;
        return nullptr;
    }
    
    if (node->isLeaf) {
        if (pos == 0) {
            *rightTree = node;
            return nullptr;
        }
        if (pos >= (int)node->text.length()) {
            *rightTree = nullptr;
            return node;
        }
        
        // Split leaf string
        std::wstring l = node->text.substr(0, pos);
        std::wstring r = node->text.substr(pos);
        
        delete node;
        *rightTree = createLeaf(r);
        return createLeaf(l);
    }
    
    // Internal
    if (pos < node->weight) {
        Node* rightChildSplits = nullptr;
        // The right child of this node definitely belongs to the right output tree
        Node* rightOfLeft = nullptr;
        
        node->left = split(node->left, pos, &rightOfLeft);
        
        // We need to concat rightOfLeft + node->right to form the *rightTree
        *rightTree = concat(rightOfLeft, node->right);
        
        // node itself is now broken, we just return node->left effectively
        // but we need to verify ownership.
        Node* newLeft = node->left;
        
        // node struct is essentially dissolved here since we separated its children
        node->left = nullptr;
        node->right = nullptr;
        delete node;
        
        return newLeft;
    } else {
        Node* leftOfRight = nullptr;
        Node* rightOfRight = nullptr;
        
        // we keep node->left, and split node->right
        node->right = split(node->right, pos - node->weight, &rightOfRight);
        
        *rightTree = rightOfRight;
        
        // Update this node
        node->weight = length(node->left);
        node->lineBreakCount = lines(node->left) + lines(node->right);
        // Balance/Height update?
        
        return node;
    }
}

TextBuffer::Node* TextBuffer::concat(Node* left, Node* right) {
    if (!left) return right;
    if (!right) return left;
    
    return createInternal(left, right);
    // Ideally check heights and rebalance
}

// ---- Access ----

wchar_t TextBuffer::charAt(const Node* node, int pos) const {
    if (node->isLeaf) {
        if (pos >= 0 && pos < (int)node->text.length()) return node->text[pos];
        return 0; 
    }
    if (pos < node->weight) return charAt(node->left, pos);
    return charAt(node->right, pos - node->weight);
}

wchar_t TextBuffer::charAt(int pos) const {
    if (pos < 0 || pos >= totalLength) return 0;
    return charAt(root, pos);
}

void TextBuffer::getSubstring(const Node* node, int pos, int len, std::wstring& result) const {
    if (!node || len <= 0) return;
    
    if (node->isLeaf) {
        int start = std::max(0, pos);
        int end = std::min((int)node->text.length(), pos + len);
        if (start < end) {
            result += node->text.substr(start, end - start);
        }
        return;
    }
    
    if (pos < node->weight) {
        getSubstring(node->left, pos, len, result);
    }
    if (pos + len > node->weight) {
        getSubstring(node->right, pos - node->weight, len - (std::max(0, node->weight - pos)), result);
    }
}

String TextBuffer::substring(int pos, int len) const {
    std::wstring res;
    getSubstring(root, pos, len, res);
    return String(res);
}

String TextBuffer::text() const {
    return substring(0, totalLength);
}

void TextBuffer::setText(const String& text) {
    clear();
    insert(0, text);
}

// ---- Line Management ----

// Get character index of start of line (0-indexed)
int TextBuffer::getLineStart(int line) const {
    if (line <= 0) return 0;
    if (line > totalLineCount) return totalLength; // End
    return getLineStart(root, line);
}

int TextBuffer::getLineStart(const Node* node, int targetLine) const {
    if (!node) return 0;
    if (node->isLeaf) {
        // Scan leaf for nth newline
        // This assumes we landed on the correct leaf that contains the targetLine-th newline relative to the start of the search
        // But the logic below passes "relative line count".
        
        // This is tricky for leaves. 
        // If we are looking for the start of line N (0-based is just start of document).
        // If query is "Start of Line 5", implies skipping 5 newlines.
        
        int found = 0;
        for (int i = 0; i < (int)node->text.length(); i++) {
            if (found == targetLine) return i; // Found the start of the requested line (actually this would be char AFTER newline? No)
            // Wait, "Start of line 1" means after 1 newline. 
            // Loop finds newlines.
            if (node->text[i] == L'\n') {
                found++;
                if (found == targetLine) return i + 1;
            }
        }
        return 0; // Should not happen if logic is correct
    }
    
    // Internal
    int leftLines = lines(node->left);
    if (targetLine <= leftLines) {
        return getLineStart(node->left, targetLine);
    } else {
        return node->weight + getLineStart(node->right, targetLine - leftLines);
    }
}

int TextBuffer::getLineFromPos(const Node* node, int targetPos) const {
    if (!node) return 0;
    if (node->isLeaf) {
        int l = 0;
        int p = 0;
        // Don't scan past targetPos inside leaf
        int limit = std::min(targetPos, (int)node->text.length());
        for (int i = 0; i < limit; i++) {
            if (node->text[i] == L'\n') l++;
        }
        return l;
    }
    
    if (targetPos < node->weight) {
        return getLineFromPos(node->left, targetPos);
    } else {
        return lines(node->left) + getLineFromPos(node->right, targetPos - node->weight);
    }
}

int TextBuffer::lineFromPosition(int pos) const {
    if (pos < 0) pos = 0;
    if (pos > totalLength) pos = totalLength;
    return getLineFromPos(root, pos);
}

int TextBuffer::getLineEnd(int line) const {
    // End of line N is Start of Line N+1 - 1 (usually, minus delimiter)
    // Actually, usually we mean the position of the newline character.
    int nextStart = getLineStart(line + 1);
    // If nextStart is 0/EOF handling?
    // If nextStart points to char after \n, then \n is at nextStart-1.
    return std::max(0, nextStart - 1); // This logic is slightly loose, refine later
}

int TextBuffer::getLineLength(int line) const {
    int start = getLineStart(line);
    int next = getLineStart(line + 1);
    // If last line, next == totalLength
    if (line == totalLineCount) next = totalLength; // Last line might not end with \n
    // Wait, getLineStart logic for "lines beyond count" needs work in recursive func
    // But assuming strict:
    
    // Correction:
    // Line 0 start: 0
    // Line 0 length: distance to first \n or end.
    
    // If we simply use getLineStart(line+1), we get the start of next line.
    // Length = nextStart - currentStart.
    // This includes the newline chars.
    return next - start;
}

} // namespace RawrXD

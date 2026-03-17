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
    
    return balance(node);
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
    
    Node* n = createInternal(left, right);
    return balance(n);
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
    if (line < 0) return 0;
    if (line >= totalLineCount) return totalLength; 
    
    int nextStart = getLineStart(line + 1);
    // getLineStart returns the index of the first character of the line.
    // The previous line technically ends at nextStart - 1 (the newline character).
    // If the user wants the end of the content (excluding newline), it would be nextStart - 1.
    // If we assume getLineEnd includes the newline (standard for "end of line range"), it's nextStart.
    // Let's assume standard behavior: position AFTER the last character of the line (including delimiter).
    return nextStart;
}

int TextBuffer::getLineLength(int line) const {
    if (line < 0 || line > totalLineCount) return 0;
    int start = getLineStart(line);
    int end = getLineEnd(line);
    return end - start;
}

// ---- Balancing ----

TextBuffer::Node* TextBuffer::buildTreeFromLeaves(const std::vector<Node*>& leaves, int start, int end) {
    if (start >= end) return nullptr;
    if (start == end - 1) return leaves[start];
    
    int mid = start + (end - start) / 2;
    Node* left = buildTreeFromLeaves(leaves, start, mid);
    Node* right = buildTreeFromLeaves(leaves, mid, end);
    return createInternal(left, right);
}

void TextBuffer::rebuild(Node* node, std::vector<Node*>& leaves) {
    if (!node) return;
    if (node->isLeaf) {
        leaves.push_back(node);
    } else {
        rebuild(node->left, leaves);
        rebuild(node->right, leaves);
        
        // Detach children to avoid double deletion when node is deleted
        node->left = nullptr;
        node->right = nullptr;
        delete node;
    }
}

TextBuffer::Node* TextBuffer::balance(Node* node) {
    if (!node) return nullptr;
    if (node->isLeaf) return node;

    // Recalculate local stats just in case
    int lh = node->left ? node->left->height : 0;
    int rh = node->right ? node->right->height : 0;
    
    // Update current node height
    node->height = 1 + std::max(lh, rh);
    
    // Rebalance if factor > threshold (e.g., 20)
    // Rope rebalancing is expensive, so we don't do it for every small detailed rotation.
    // Instead we do a full rebuild of the subtree if it gets too lop-sided.
    if (std::abs(lh - rh) > 20) {
        std::vector<Node*> leaves;
        rebuild(node, leaves);
        return buildTreeFromLeaves(leaves, 0, (int)leaves.size());
    }
    
    return node;
}

} // namespace RawrXD

// ast_graph_engine_impl.cpp
// Out-of-line implementations for ast_graph_engine.hpp API.
// This bridges rust_parser_v2.cpp (which uses .hpp) with the build system.

#include "ast_graph_engine.hpp"
#include <algorithm>

namespace RawrXD::AST {

std::atomic<uint64_t> ASTNode::next_version_{1};

ASTNode::ASTNode(NodeType type, SourceRange range, std::string text)
    : type_(type), range_(range), text_(std::move(text)),
      hash_(0), version_(next_version_++), parent_(), children_(),
      references_(), definitions_(), cache_mutex_() {}

ASTNode::Ptr ASTNode::withChildren(std::vector<Ptr> new_children) const {
    auto copy = std::make_shared<ASTNode>(type_, range_, text_);
    copy->hash_ = hash_;
    copy->version_ = version_;
    copy->parent_ = parent_;
    copy->children_ = std::move(new_children);
    copy->references_ = references_;
    copy->definitions_ = definitions_;
    copy->rust_meta_ = rust_meta_;
    for (auto& c : copy->children_) {
        const_cast<ASTNode*>(c.get())->parent_ = copy;
    }
    return copy;
}

ASTNode::Ptr ASTNode::withParent(Ptr new_parent) const {
    auto copy = std::make_shared<ASTNode>(type_, range_, text_);
    copy->hash_ = hash_;
    copy->version_ = version_;
    copy->parent_ = new_parent;
    copy->children_ = children_;
    copy->references_ = references_;
    copy->definitions_ = definitions_;
    copy->rust_meta_ = rust_meta_;
    return copy;
}

ASTNode::Ptr ASTNode::withHash(NodeHash new_hash) const {
    auto copy = std::make_shared<ASTNode>(type_, range_, text_);
    copy->hash_ = new_hash;
    copy->version_ = version_;
    copy->parent_ = parent_;
    copy->children_ = children_;
    copy->references_ = references_;
    copy->definitions_ = definitions_;
    copy->rust_meta_ = rust_meta_;
    return copy;
}

ASTNode::Ptr ASTNode::getChild(size_t index) const {
    return index < children_.size() ? children_[index] : nullptr;
}

bool ASTNode::isDeclaration() const {
    return type_ == NodeType::FunctionDecl || type_ == NodeType::VariableDecl ||
           type_ == NodeType::ClassDecl || type_ == NodeType::StructDecl ||
           type_ == NodeType::EnumDecl || type_ == NodeType::NamespaceDecl ||
           type_ == NodeType::TypedefDecl;
}

bool ASTNode::isStatement() const {
    return type_ == NodeType::VariableDecl || type_ == NodeType::FunctionDecl;
}

bool ASTNode::isExpression() const {
    return false;
}

bool ASTNode::isType() const {
    return type_ == NodeType::StructDecl || type_ == NodeType::EnumDecl ||
           type_ == NodeType::ClassDecl;
}

std::string ASTNode::getName() const {
    if (!cached_name_) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        if (!cached_name_) {
            size_t pos = text_.find(' ');
            if (pos != std::string::npos) {
                const_cast<std::optional<std::string>&>(cached_name_) = text_.substr(pos + 1);
            } else {
                const_cast<std::optional<std::string>&>(cached_name_) = text_;
            }
        }
    }
    return *cached_name_;
}

std::string ASTNode::getQualifiedName() const {
    if (!cached_qualified_name_) {
        std::lock_guard<std::shared_mutex> lock(cache_mutex_);
        if (!cached_qualified_name_) {
            std::string qname = getName();
            auto p = getParent();
            while (p) {
                qname = p->getName() + "::" + qname;
                p = p->getParent();
            }
            const_cast<std::optional<std::string>&>(cached_qualified_name_) = qname;
        }
    }
    return *cached_qualified_name_;
}

ASTNode::Ptr ASTNode::findNodeAt(const SourceLocation& loc) const {
    if (!range_.contains(loc)) return nullptr;
    for (auto& child : children_) {
        auto found = child->findNodeAt(loc);
        if (found) return found;
    }
    return shared_from_this();
}

void ASTNode::findNodesOfType(NodeType type, std::vector<Ptr>& results) const {
    if (type_ == type) results.push_back(shared_from_this());
    for (auto& child : children_) {
        child->findNodesOfType(type, results);
    }
}

size_t ASTNode::graphDistanceTo(Ptr other) const {
    if (!other) return SIZE_MAX;
    if (this == other.get()) return 0;
    std::vector<Ptr> queue;
    std::unordered_set<const ASTNode*> visited;
    queue.push_back(shared_from_this());
    visited.insert(this);
    size_t distance = 0;
    while (!queue.empty()) {
        size_t level_size = queue.size();
        for (size_t i = 0; i < level_size; ++i) {
            auto current = queue[i];
            if (current.get() == other.get()) return distance;
            for (auto& child : current->getChildren()) {
                if (visited.insert(child.get()).second) queue.push_back(child);
            }
            auto parent = current->getParent();
            if (parent && visited.insert(parent.get()).second) queue.push_back(parent);
        }
        queue.erase(queue.begin(), queue.begin() + level_size);
        ++distance;
    }
    return SIZE_MAX;
}

// ASTGraphEngine stubs
void ASTGraphEngine::registerFile(const std::string& path, const std::string& content) {
    (void)path; (void)content;
}

void ASTGraphEngine::updateFile(const std::string& path, const std::string& content) {
    (void)path; (void)content;
}

} // namespace RawrXD::AST

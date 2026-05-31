// ============================================================================
// ast_scope_awareness_test.cpp — Final Validation: AST Context Wiring
// ============================================================================
// Tests scope-aware completion on complex C++23 template class
// Verifies: access modifiers, nested namespaces, inheritance, method chains
// ============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <concepts>
#include <coroutine>

// Include AST completion bridge
#include "../src/ide/ast_completion_bridge.h"
#include "../src/core/ast_graph_engine.hpp"

namespace RawrXD {
namespace Tests {

// ============================================================================
// Test Case: Complex C++23 Template Hierarchy
// ============================================================================

namespace detail {
    template<typename T>
    concept Arithmetic = std::is_arithmetic_v<T>;
    
    template<typename T>
    concept Container = requires(T t) {
        { t.begin() } -> std::same_as<typename T::iterator>;
        { t.end() } -> std::same_as<typename T::iterator>;
        { t.size() } -> std::convertible_to<std::size_t>;
    };
}

namespace math {
    namespace linear {
        template<detail::Arithmetic T, std::size_t N>
        class Vector {
        public:
            using value_type = T;
            using size_type = std::size_t;
            static constexpr size_type dimensions = N;
            
            // Public interface
            Vector() = default;
            explicit Vector(std::initializer_list<T> values);
            
            T dot(const Vector& other) const;
            T magnitude() const;
            Vector normalized() const;
            
            // Accessors
            T& operator[](size_type index);
            const T& operator[](size_type index) const;
            
            // Iterator support
            auto begin() { return data_.begin(); }
            auto end() { return data_.end(); }
            auto begin() const { return data_.begin(); }
            auto end() const { return data_.end(); }
            
        private:
            std::array<T, N> data_;
            
            // Private helper
            T computeMagnitudeSquared() const;
            
        protected:
            // Protected for derived classes
            void validateIndex(size_type index) const;
        };
        
        template<detail::Arithmetic T, std::size_t N>
        class Matrix : public Vector<T, N * N> {
        public:
            using Base = Vector<T, N * N>;
            using typename Base::value_type;
            using typename Base::size_type;
            
            Matrix() = default;
            
            // Matrix-specific operations
            Matrix transpose() const;
            T determinant() const;
            Matrix inverse() const;
            
            // Access element at (row, col)
            T& at(size_type row, size_type col);
            const T& at(size_type row, size_type col) const;
            
        private:
            // Private matrix helpers
            T computeCofactor(size_type row, size_type col) const;
            Matrix computeMinor(size_type row, size_type col) const;
            
            using Base::data_;  // Bring base data into scope
        };
    }
}

namespace graphics {
    template<typename VertexType>
    class Mesh {
    public:
        using vertex_type = VertexType;
        using index_type = std::uint32_t;
        
        void addVertex(const VertexType& v);
        void addTriangle(index_type i0, index_type i1, index_type i2);
        
        // Public query
        std::size_t vertexCount() const { return vertices_.size(); }
        std::size_t triangleCount() const { return indices_.size() / 3; }
        
    protected:
        // Protected: allow derived classes to manipulate geometry
        std::vector<VertexType>& getVertices() { return vertices_; }
        std::vector<index_type>& getIndices() { return indices_; }
        
        // Protected helper
        void validateTopology();
        
    private:
        std::vector<VertexType> vertices_;
        std::vector<index_type> indices_;
        
        // Private: internal cache
        mutable std::optional<std::size_t> cachedHash_;
        std::size_t computeHash() const;
    };
    
    template<typename VertexType>
    class AnimatedMesh : public Mesh<VertexType> {
    public:
        using Base = Mesh<VertexType>;
        using typename Base::vertex_type;
        using typename Base::index_type;
        
        void updateAnimation(float deltaTime);
        
        // Test: Should have access to protected members from base
        void modifyGeometry() {
            // CURSOR HERE - Should see protected getVertices(), getIndices()
            // Should NOT see private vertices_, indices_
            auto& verts = this->getVertices();  // OK - protected
            this->validateTopology();           // OK - protected
            // this->vertices_;                 // ERROR - private
        }
        
    private:
        std::vector<float> boneWeights_;
        std::vector<std::size_t> boneIndices_;
    };
}

// ============================================================================
// Test Scenarios
// ============================================================================

class ASTScopeAwarenessTest {
public:
    bool runAllTests();
    
private:
    IDE::ASTCompletionBridge bridge_;
    
    // Test scenarios
    bool testPublicAccess();
    bool testPrivateAccess();
    bool testProtectedAccess();
    bool testNamespaceScope();
    bool testTemplateScope();
    bool testInheritanceScope();
    bool testMethodChainTypeAwareness();
};

bool ASTScopeAwarenessTest::runAllTests() {
    std::cout << "=== AST Scope Awareness Test Suite ===\n\n";
    
    bool allPassed = true;
    
    allPassed &= testPublicAccess();
    allPassed &= testPrivateAccess();
    allPassed &= testProtectedAccess();
    allPassed &= testNamespaceScope();
    allPassed &= testTemplateScope();
    allPassed &= testInheritanceScope();
    allPassed &= testMethodChainTypeAwareness();
    
    std::cout << "\n=== " << (allPassed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") 
              << " ===\n";
    
    return allPassed;
}

bool ASTScopeAwarenessTest::testPublicAccess() {
    std::cout << "Test: Public Access\n";
    
    // Simulate cursor inside Vector class, public section
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 45, 10);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // Should see public members: dot, magnitude, normalized, operator[]
    bool foundDot = false;
    bool foundMagnitude = false;
    
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "dot") foundDot = true;
        if (sym.name == "magnitude") foundMagnitude = true;
    }
    
    if (!foundDot || !foundMagnitude) {
        std::cout << "  [FAIL] Public members not visible\n";
        return false;
    }
    
    std::cout << "  [PASS] Public members visible\n";
    return true;
}

bool ASTScopeAwarenessTest::testPrivateAccess() {
    std::cout << "Test: Private Access (Negative)\n";
    
    // Simulate cursor outside Vector class
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 100, 5);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // Should NOT see private members: data_, computeMagnitudeSquared
    bool foundPrivateData = false;
    
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "data_" || sym.name == "computeMagnitudeSquared") {
            foundPrivateData = true;
            break;
        }
    }
    
    if (foundPrivateData) {
        std::cout << "  [FAIL] Private members visible outside class\n";
        return false;
    }
    
    std::cout << "  [PASS] Private members correctly hidden\n";
    return true;
}

bool ASTScopeAwarenessTest::testProtectedAccess() {
    std::cout << "Test: Protected Access in Derived Class\n";
    
    // Simulate cursor inside AnimatedMesh::modifyGeometry
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 145, 20);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // Should see protected members from base: getVertices, getIndices, validateTopology
    bool foundGetVertices = false;
    bool foundValidateTopology = false;
    
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "getVertices") foundGetVertices = true;
        if (sym.name == "validateTopology") foundValidateTopology = true;
    }
    
    if (!foundGetVertices || !foundValidateTopology) {
        std::cout << "  [FAIL] Protected members not visible in derived class\n";
        return false;
    }
    
    // Should NOT see private members from base
    bool foundPrivateBase = false;
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "vertices_" || sym.name == "indices_") {
            foundPrivateBase = true;
            break;
        }
    }
    
    if (foundPrivateBase) {
        std::cout << "  [FAIL] Private base members visible in derived\n";
        return false;
    }
    
    std::cout << "  [PASS] Protected access rules enforced\n";
    return true;
}

bool ASTScopeAwarenessTest::testNamespaceScope() {
    std::cout << "Test: Namespace Scope Resolution\n";
    
    // Simulate cursor in math::linear namespace
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 25, 5);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // Should see scope stack: global -> math -> linear
    bool foundMath = false;
    bool foundLinear = false;
    
    for (const auto& scope : ast.scope.scopeStack) {
        if (scope == "math") foundMath = true;
        if (scope == "linear") foundLinear = true;
    }
    
    if (!foundMath || !foundLinear) {
        std::cout << "  [FAIL] Namespace scope not captured\n";
        return false;
    }
    
    std::cout << "  [PASS] Namespace scope: " << ast.scope.currentScope << "\n";
    return true;
}

bool ASTScopeAwarenessTest::testTemplateScope() {
    std::cout << "Test: Template Parameter Scope\n";
    
    // Simulate cursor inside Vector<T, N> template
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 35, 15);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // Should see template parameters: T, N
    bool foundT = false;
    bool foundN = false;
    
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "T") foundT = true;
        if (sym.name == "N") foundN = true;
    }
    
    if (!foundT || !foundN) {
        std::cout << "  [FAIL] Template parameters not visible\n";
        return false;
    }
    
    std::cout << "  [PASS] Template parameters in scope\n";
    return true;
}

bool ASTScopeAwarenessTest::testInheritanceScope() {
    std::cout << "Test: Inheritance Hierarchy\n";
    
    // Simulate cursor in Matrix class (inherits from Vector)
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 75, 10);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // Should see inherited public members
    bool foundBaseMethods = false;
    
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "dot" || sym.name == "magnitude") {
            foundBaseMethods = true;
            break;
        }
    }
    
    if (!foundBaseMethods) {
        std::cout << "  [FAIL] Inherited members not visible\n";
        return false;
    }
    
    std::cout << "  [PASS] Inheritance hierarchy resolved\n";
    return true;
}

bool ASTScopeAwarenessTest::testMethodChainTypeAwareness() {
    std::cout << "Test: Method Chain Type Awareness\n";
    
    // Simulate cursor after "vec.normalized()."
    auto ast = bridge_.captureASTContext("test.cpp", "cpp", 50, 25);
    
    if (!ast.isValid) {
        std::cout << "  [FAIL] AST context not captured\n";
        return false;
    }
    
    // After normalized() returns Vector, should see Vector methods again
    bool foundChainableMethods = false;
    
    for (const auto& sym : ast.visibleSymbols) {
        if (sym.name == "dot" || sym.name == "normalized") {
            foundChainableMethods = true;
            break;
        }
    }
    
    if (!foundChainableMethods) {
        std::cout << "  [FAIL] Method chain type not resolved\n";
        return false;
    }
    
    std::cout << "  [PASS] Method chain types resolved\n";
    return true;
}

// ============================================================================
// Main Entry Point
// ============================================================================

} // namespace Tests
} // namespace RawrXD

int main() {
    RawrXD::Tests::ASTScopeAwarenessTest test;
    return test.runAllTests() ? 0 : 1;
}

// ============================================================================
// Expected Completion Behaviors (for manual verification)
// ============================================================================
/*

CURSOR POSITION 1: Inside Vector::magnitude() - Line ~45, Column ~10
---------------------------------------------------------------
Expected completions:
  ✓ data_                    (private member - accessible within class)
  ✓ computeMagnitudeSquared() (private method - accessible within class)
  ✓ validateIndex()          (protected method - accessible within class)
  ✓ N                        (template parameter)
  ✓ T                        (template parameter)
  ✓ dimensions               (static member)
  
Expected NO completions:
  ✗ vertices_                (private member of unrelated Mesh class)


CURSOR POSITION 2: Inside AnimatedMesh::modifyGeometry() - Line ~145, Column ~20
------------------------------------------------------------------------------
Expected completions:
  ✓ getVertices()            (protected from base Mesh)
  ✓ getIndices()             (protected from base Mesh)
  ✓ validateTopology()       (protected from base Mesh)
  ✓ boneWeights_             (private member of AnimatedMesh)
  
Expected NO completions:
  ✗ vertices_                (private in base Mesh - not accessible)
  ✗ indices_                 (private in base Mesh - not accessible)
  ✗ cachedHash_              (private in base Mesh - not accessible)


CURSOR POSITION 3: Outside any class - Line ~200, Column ~5
----------------------------------------------------------
Expected completions:
  ✓ math::linear::Vector     (public type)
  ✓ graphics::Mesh           (public type)
  
Expected NO completions:
  ✗ data_                    (private in Vector)
  ✗ computeMagnitudeSquared() (private in Vector)
  ✗ vertices_                (private in Mesh)


CURSOR POSITION 4: After "vec.normalized()." - Line ~50, Column ~25
------------------------------------------------------------------
Expected completions (method chain):
  ✓ dot()                    (returns T, chain ends)
  ✓ normalized()             (returns Vector, chain continues)
  ✓ magnitude()              (returns T, chain ends)
  ✓ operator[]               (returns T&, chain ends)

Type flow: Vector::normalized() -> Vector -> Vector::methods
*/

// C++20 Template Test for AST Scope-Awareness Validation
// Tests: Access modifiers, template instantiation, nested types
// Author: RawrXD Core Team
// Purpose: Final validation before v1.0.0-gold release

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace RawrXD::AST::Test {

// =============================================================================
// Test 1: Access Modifier Scope Sovereignty
// Private members should NOT be suggested outside class scope
// =============================================================================

class AccessControlTest {
public:
    void publicMethod();
    int publicMember;
    
protected:
    void protectedMethod();
    int protectedMember;
    
private:
    void privateMethod();      // Should NOT appear in completions outside class
    int privateMember;          // Should NOT appear in completions outside class
    static constexpr int SECRET = 42;  // Should NOT appear
};

// Test: Cursor here should only see publicMethod/publicMember
void testAccessSovereignty() {
    AccessControlTest obj;
    // obj. --> Only publicMethod/publicMember should appear
    // NOT: privateMethod, privateMember, SECRET
}

// =============================================================================
// Test 2: Template Instantiation Chain
// Tests deep type inference through chained method calls
// =============================================================================

template<typename T>
class Vector3 {
public:
    T x, y, z;
    
    Vector3<T> operator+(const Vector3<T>& other) const {
        return Vector3<T>{x + other.x, y + other.y, z + other.z};
    }
    
    T magnitude() const {
        return std::sqrt(x*x + y*y + z*z);
    }
    
    Vector3<T> normalized() const {
        T mag = magnitude();
        return Vector3<T>{x/mag, y/mag, z/mag};
    }
    
    template<typename U>
    auto dot(const Vector3<U>& other) const -> decltype(T{} * U{}) {
        return x * other.x + y * other.y + z * other.z;
    }
};

// Test: Type-aware ghost text through chained calls
void testChainedInference() {
    Vector3<double> v1{1.0, 2.0, 3.0};
    Vector3<double> v2{4.0, 5.0, 6.0};
    
    // Cursor here: (v1 + v2). --> Should suggest: magnitude(), normalized(), dot()
    auto result = (v1 + v2).normalized().magnitude();
    // Cursor here: result. --> Should suggest: double methods (sqrt, pow, etc)
}

// =============================================================================
// Test 3: Nested Type Hierarchy
// Tests scope resolution for nested classes and type aliases
// =============================================================================

template<typename T>
class Container {
public:
    using ValueType = T;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    
    class Iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = Pointer;
        using reference = Reference;
        
        Iterator& operator++();
        Iterator& operator--();
        Reference operator*() const;
        Pointer operator->() const;
        bool operator!=(const Iterator& other) const;
        
    private:
        Pointer m_ptr;
    };
    
    Iterator begin();
    Iterator end();
    
    template<typename... Args>
    decltype(auto) emplace(Args&&... args);
    
private:
    std::vector<T> m_data;
};

// Test: Nested type resolution
void testNestedTypes() {
    Container<int>::Iterator it;  // Should resolve Iterator type
    // it. --> Should suggest: operator++, operator--, operator*, operator->
    
    Container<std::string>::ValueType val;  // Should resolve to std::string
    // val. --> Should suggest: std::string methods
}

// =============================================================================
// Test 4: Concept Constraints and Requirements
// Tests AST understanding of C++20/23 concepts
// =============================================================================

template<typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

template<typename T>
concept Serializable = requires(T t, std::byte* buffer) {
    { t.serialize(buffer) } -> std::same_as<std::size_t>;
    { T::deserialize(buffer) } -> std::same_as<T>;
};

template<Hashable T>
class HashTable {
public:
    void insert(const T& key);
    bool contains(const T& key) const;
    void remove(const T& key);
    
private:
    std::vector<T> m_buckets[1024];
};

template<Serializable T>
class PersistentStore {
public:
    void save(const T& obj);
    T load(std::size_t id);
    
private:
    std::byte* m_buffer;
};

// Test: Concept-constrained completion
void testConceptConstraints() {
    HashTable<std::string> table;
    // table. --> Should suggest: insert, contains, remove
    // Should NOT suggest: save, load (those are for Serializable)
    
    PersistentStore<std::vector<int>> store;
    // store. --> Should suggest: save, load
}

// =============================================================================
// Test 5: CRTP (Curiously Recurring Template Pattern)
// Tests static polymorphism and mixin completion
// =============================================================================

template<typename Derived>
class Singleton {
public:
    static Derived& instance() {
        static Derived instance;
        return instance;
    }
    
protected:
    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
};

template<typename Derived>
class Observable {
public:
    void notifyObservers() {
        static_cast<Derived*>(this)->onNotify();
    }
    
protected:
    void attachObserver(auto* observer);
    void detachObserver(auto* observer);
};

class EventManager : public Singleton<EventManager>,
                     public Observable<EventManager> {
public:
    void dispatchEvent(int eventId);
    void registerHandler(int eventId, auto handler);
    
private:
    void onNotify();  // Required by Observable CRTP
    
    friend class Singleton<EventManager>;
    EventManager() = default;
};

// Test: CRTP completion
void testCRTP() {
    // EventManager:: --> Should suggest: instance(), dispatchEvent, registerHandler
    // Should NOT suggest: Singleton constructor (protected)
    
    auto& em = EventManager::instance();
    // em. --> Should suggest: dispatchEvent, registerHandler, notifyObservers
}

// =============================================================================
// Test 6: Variadic Templates and Fold Expressions
// Tests pack expansion and fold expression completion
// =============================================================================

template<typename... Args>
class Tuple {
public:
    static constexpr std::size_t size = sizeof...(Args);
    
    template<std::size_t I>
    decltype(auto) get() {
        return std::get<I>(m_data);
    }
    
    template<typename Func>
    void apply(Func&& func) {
        (func(std::get<Args>(m_data)), ...);  // Fold expression
    }
    
private:
    std::tuple<Args...> m_data;
};

// Test: Variadic template completion
void testVariadicTemplates() {
    Tuple<int, double, std::string> t;
    // t.get<0>() --> Should return int
    // t.get<1>() --> Should return double
    // t.get<2>() --> Should return std::string
    
    // t.apply([](auto& val) { ... }) --> val should have correct type in each invocation
}

// =============================================================================
// Test 7: Lambda and Closure Type Inference
// Tests completion within lambda bodies and for closure types
// =============================================================================

void testLambdaInference() {
    std::vector<int> data{1, 2, 3, 4, 5};
    
    // Lambda with auto parameter
    auto processor = [](auto&& val) -> decltype(auto) {
        // val. --> Should suggest methods based on actual type (int in this case)
        return val * 2;
    };
    
    // std::transform with lambda
    std::transform(data.begin(), data.end(), data.begin(), processor);
    
    // Generic lambda with multiple invocations
    auto generic = []<typename T>(T&& t) {
        // t. --> Should suggest T's methods
        return t + t;
    };
    
    generic(42);        // T = int
    generic(3.14);      // T = double
    generic(std::string{"test"});  // T = std::string
}

// =============================================================================
// Test 8: Module Interface (C++20 Modules)
// Tests cross-module AST resolution
// =============================================================================

// In a real module: export module RawrXD.AST.Test;
// export template<typename T> concept Numeric = ...;

// Test: Module import completion
// import RawrXD.AST.Test; --> Should expose: Numeric, Vector3, Container, etc.

// =============================================================================
// AST Scope-Awareness Validation Summary
// =============================================================================

/*
Expected AST Wiring Behavior:

1. Access Control:
   - Private members filtered outside class scope
   - Protected members visible in derived classes
   - Public members always visible

2. Template Instantiation:
   - Type propagation through chained calls
   - Concept constraints enforced in completions
   - Nested type resolution (Iterator -> Container::Iterator)

3. Scope Resolution:
   - Namespace scope respected
   - Class scope for static members
   - Block scope for local variables

4. Context Fingerprinting:
   - Cache hit for repeated AST queries
   - Incremental updates on character changes
   - Zero-latency for common patterns

5. FFI Integration:
   - C-API exposes: RawrXD_ast_completion_enrich
   - Returns: ASTEnrichedContext with scope info
   - Used by: Ghost text, LSP bridge, Agentic completion
*/

} // namespace RawrXD::AST::Test

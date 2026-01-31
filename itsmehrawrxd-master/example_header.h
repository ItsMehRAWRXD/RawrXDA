// example_header.h
// Example C++ header file for template generation
// This demonstrates the types of constructs that can be parsed

#ifndef EXAMPLE_HEADER_H
#define EXAMPLE_HEADER_H

#include <string>
#include <vector>
#include <memory>

namespace ExampleNamespace {

// Forward declarations
class ExampleClass;
struct ExampleStruct;

// Enums
enum class Status {
    SUCCESS = 0,
    FAILURE = 1,
    PENDING = 2
};

enum Priority {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2
};

// Struct definition
struct ExampleStruct {
    int id;
    std::string name;
    bool active;
    
    ExampleStruct();
    ExampleStruct(int id, const std::string& name);
    ~ExampleStruct();
    
    void setName(const std::string& name);
    std::string getName() const;
    bool isActive() const;
};

// Class definition
class ExampleClass {
public:
    // Constructors
    ExampleClass();
    ExampleClass(const std::string& name);
    ExampleClass(const ExampleClass& other);
    ~ExampleClass();
    
    // Assignment operator
    ExampleClass& operator=(const ExampleClass& other);
    
    // Public methods
    void setName(const std::string& name);
    std::string getName() const;
    void setValue(int value);
    int getValue() const;
    bool isValid() const;
    
    // Static methods
    static ExampleClass createDefault();
    static int getInstanceCount();
    
    // Virtual methods
    virtual void process();
    virtual std::string toString() const;
    
    // Template methods
    template<typename T>
    void setGenericValue(const T& value);
    
    template<typename T>
    T getGenericValue() const;

private:
    std::string m_name;
    int m_value;
    bool m_valid;
    static int s_instanceCount;
};

// Abstract base class
class AbstractBase {
public:
    virtual ~AbstractBase() = default;
    virtual void doSomething() = 0;
    virtual int calculate(int a, int b) = 0;
};

// Derived class
class ConcreteDerived : public AbstractBase {
public:
    ConcreteDerived();
    ~ConcreteDerived() override;
    
    void doSomething() override;
    int calculate(int a, int b) override;
    
private:
    int m_data;
};

// Function declarations
int processData(const std::vector<int>& data);
std::string formatMessage(const std::string& template_str, const std::vector<std::string>& args);
bool validateInput(const std::string& input);
void logMessage(const std::string& message, Priority priority = Priority::MEDIUM);

// Template function
template<typename T>
std::vector<T> filter(const std::vector<T>& data, bool (*predicate)(const T&));

// Function pointer type
typedef int (*CallbackFunction)(int value, void* context);

// Using declaration
using StringVector = std::vector<std::string>;
using SmartPointer = std::unique_ptr<ExampleClass>;

// Constants
const int MAX_SIZE = 1024;
const std::string DEFAULT_NAME = "default";

// Inline functions
inline bool isEmpty(const std::string& str) {
    return str.empty();
}

inline int square(int x) {
    return x * x;
}

} // namespace ExampleNamespace

// Global functions
extern "C" {
    int c_interface_function(int value);
    void c_interface_callback(void (*callback)(int));
}

#endif // EXAMPLE_HEADER_H

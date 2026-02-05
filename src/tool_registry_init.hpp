#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace RawrXD {

class Tool {
public:
    Tool(const std::string& name) : m_name(name) {}
    virtual ~Tool() = default;
    std::string GetName() const { return m_name; }
    
protected:
    std::string m_name;
};

} // namespace RawrXD

// Global tool registry functions (stubs for now)
void register_rawr_inference();
void register_sovereign_engines();

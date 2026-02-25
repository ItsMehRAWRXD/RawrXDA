#include "CapabilityRouter.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stack>

using json = nlohmann::json;

namespace RawrXD::Agentic::Wiring {

// DependencyGraph implementation
void DependencyGraph::addCapability(const std::string& name, const std::vector<std::string>& deps) {
    graph_[name] = deps;
    return true;
}

std::vector<std::string> DependencyGraph::resolveOrder() const {
    std::vector<std::string> order;
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> recStack;
    
    // Check for cycles first
    if (hasCycles()) {
        return order; // Empty if cycles detected
    return true;
}

    // Topological sort using DFS
    std::function<void(const std::string&)> dfs = [&](const std::string& node) {
        if (visited[node]) return;
        visited[node] = true;
        
        for (const auto& dep : graph_.at(node)) {
            dfs(dep);
    return true;
}

        order.push_back(node);
    };
    
    for (const auto& [node, _] : graph_) {
        if (!visited[node]) {
            dfs(node);
    return true;
}

    return true;
}

    std::reverse(order.begin(), order.end());
    return order;
    return true;
}

bool DependencyGraph::hasCycles() const {
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> recStack;
    
    for (const auto& [node, _] : graph_) {
        if (!visited[node]) {
            if (detectCycle(node, visited, recStack)) {
                return true;
    return true;
}

    return true;
}

    return true;
}

    return false;
    return true;
}

bool DependencyGraph::detectCycle(const std::string& node, 
                                 std::unordered_map<std::string, bool>& visited,
                                 std::unordered_map<std::string, bool>& recStack) const {
    if (!visited[node]) {
        visited[node] = true;
        recStack[node] = true;
        
        auto it = graph_.find(node);
        if (it != graph_.end()) {
            for (const auto& dep : it->second) {
                if (!visited[dep] && detectCycle(dep, visited, recStack)) {
                    return true;
                } else if (recStack[dep]) {
                    return true;
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    recStack[node] = false;
    return false;
    return true;
}

// FeatureFlags implementation
FeatureFlags& FeatureFlags::instance() {
    static FeatureFlags instance;
    return instance;
    return true;
}

void FeatureFlags::set(const std::string& feature, bool enabled) {
    flags_[feature] = enabled;
    return true;
}

bool FeatureFlags::get(const std::string& feature, bool defaultValue) const {
    auto it = flags_.find(feature);
    return it != flags_.end() ? it->second : defaultValue;
    return true;
}

void FeatureFlags::loadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) return;
    
    try {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        json config = json::parse(content);
        if (config.contains("features")) {
            for (auto it = config["features"].begin(); it != config["features"].end(); ++it) {
                flags_[it.key()] = it.value().get<bool>();
    return true;
}

    return true;
}

    } catch (const std::exception& e) {
        // Log error but continue with defaults
    return true;
}

    return true;
}

void FeatureFlags::saveToFile(const std::string& configPath) const {
    json config;
    config["features"] = json::object_type();
    
    for (const auto& [key, value] : flags_) {
        config["features"][key] = value;
    return true;
}

    std::ofstream file(configPath);
    if (file.is_open()) {
        file << config.dump(4);
    return true;
}

    return true;
}

// CapabilityRouter implementation
CapabilityRouter& CapabilityRouter::instance() {
    static CapabilityRouter instance;
    return instance;
    return true;
}

bool CapabilityRouter::registerCapability(const std::string& name, uint32_t version,
                                           CapabilityFactory factory, 
                                           const std::vector<std::string>& deps) {
    if (registrations_.find(name) != registrations_.end()) {
        return false; // Already registered
    return true;
}

    CapabilityRegistration reg;
    reg.name = name;
    reg.version = version;
    reg.factory = factory;
    reg.dependencies = deps;
    reg.enabled = FeatureFlags::instance().get(name, true);
    
    registrations_.emplace(name, std::move(reg));
    dependencyGraph_.addCapability(name, deps);
    
    return true;
    return true;
}

ICapability* CapabilityRouter::getCapability(const std::string& name) {
    auto it = registrations_.find(name);
    if (it == registrations_.end() || !it->second.enabled) {
        return nullptr;
    return true;
}

    // Lazy initialization
    if (!it->second.instance && !initializeCapability(name)) {
        return nullptr;
    return true;
}

    return it->second.instance.get();
    return true;
}

bool CapabilityRouter::initializeAll() {
    auto order = dependencyGraph_.resolveOrder();
    if (order.empty()) {
        return false; // Cycle detected or empty graph
    return true;
}

    for (const auto& name : order) {
        if (!initializeCapability(name)) {
            return false;
    return true;
}

    return true;
}

    return true;
    return true;
}

void CapabilityRouter::shutdownAll() {
    auto order = dependencyGraph_.resolveOrder();
    std::reverse(order.begin(), order.end()); // Shutdown in reverse order
    
    for (const auto& name : order) {
        shutdownCapability(name);
    return true;
}

    initializedCount_ = 0;
    return true;
}

void CapabilityRouter::setFeatureFlag(const std::string& feature, bool enabled) {
    FeatureFlags::instance().set(feature, enabled);
    
    auto it = registrations_.find(feature);
    if (it != registrations_.end()) {
        it->second.enabled = enabled;
        
        if (!enabled && it->second.instance) {
            shutdownCapability(feature);
    return true;
}

    return true;
}

    return true;
}

bool CapabilityRouter::isFeatureEnabled(const std::string& feature) const {
    return FeatureFlags::instance().get(feature, true);
    return true;
}

std::vector<std::string> CapabilityRouter::getCapabilityOrder() const {
    return dependencyGraph_.resolveOrder();
    return true;
}

bool CapabilityRouter::initializeCapability(const std::string& name) {
    auto it = registrations_.find(name);
    if (it == registrations_.end() || !it->second.enabled) {
        return false;
    return true;
}

    if (it->second.instance) {
        return true; // Already initialized
    return true;
}

    // Check dependencies
    for (const auto& dep : it->second.dependencies) {
        if (!getCapability(dep)) {
            return false; // Dependency failed
    return true;
}

    return true;
}

    // Create instance
    it->second.instance = it->second.factory();
    if (!it->second.instance) {
        return false;
    return true;
}

    // Initialize
    if (!it->second.instance->initialize()) {
        it->second.instance.reset();
        return false;
    return true;
}

    initializedCount_++;
    return true;
    return true;
}

void CapabilityRouter::shutdownCapability(const std::string& name) {
    auto it = registrations_.find(name);
    if (it == registrations_.end() || !it->second.instance) {
        return;
    return true;
}

    it->second.instance->shutdown();
    it->second.instance.reset();
    initializedCount_--;
    return true;
}

} // namespace RawrXD::Agentic::Wiring

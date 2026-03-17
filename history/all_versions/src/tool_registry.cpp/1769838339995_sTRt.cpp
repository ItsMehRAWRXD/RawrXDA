#include "tool_registry.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

namespace rawrxd {

    // =========================================================================================
    // ToolRegistry Implementation
    // =========================================================================================

    ToolRegistry& ToolRegistry::instance() {
        static ToolRegistry instance;
        return instance;
    }

    void ToolRegistry::registerTool(const ToolMetadata& metadata, ToolExecutor executor) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        
        if (tools_.find(metadata.name) != tools_.end()) {
            std::cerr << "[ToolRegistry] Warning: Overwriting existing tool definition for '" << metadata.name << "'" << std::endl;
        }

        ToolEntry entry;
        entry.metadata = metadata;
        entry.executor = executor;
        entry.stats = {0, 0, 0.0, 0.0};
        
        tools_[metadata.name] = entry;
        std::cout << "[ToolRegistry] Registered tool: " << metadata.name << " (v" << metadata.version << ")" << std::endl;
    }

    void ToolRegistry::unregisterTool(const std::string& name) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        if (tools_.erase(name)) {
             std::cout << "[ToolRegistry] Unregistered tool: " << name << std::endl;
        }
    }

    bool ToolRegistry::hasTool(const std::string& name) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        return tools_.find(name) != tools_.end();
    }

    std::vector<ToolMetadata> ToolRegistry::getAllTools() const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        std::vector<ToolMetadata> result;
        result.reserve(tools_.size());
        
        for (const auto& pair : tools_) {
            result.push_back(pair.second.metadata);
        }
        return result;
    }

    std::vector<ToolMetadata> ToolRegistry::getToolsByCategory(const std::string& category) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        std::vector<ToolMetadata> result;
        
        for (const auto& pair : tools_) {
            if (pair.second.metadata.category == category) {
                result.push_back(pair.second.metadata);
            }
        }
        return result;
    }

    std::optional<ToolMetadata> ToolRegistry::getToolMetadata(const std::string& name) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second.metadata;
        }
        return std::nullopt;
    }

    // =========================================================================================
    // Execution Logic
    // =========================================================================================

    ToolResult ToolRegistry::executeTool(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
        // 1. Discovery & Validation
        {
            std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
            if (tools_.find(name) == tools_.end()) {
                return {false, "", "Tool not found: " + name, 404, 0.0, {}};
            }
        }

        // 2. Pre-execution Hooks (Middleware)
        for (const auto& hook : pre_hooks_) {
            try {
                if (!hook(name, args)) {
                    return {false, "", "Execution cancelled by pre-execution hook", 403, 0.0, {}};
                }
            } catch (const std::exception& e) {
                return {false, "", std::string("Middleware exception: ") + e.what(), 500, 0.0, {}};
            }
        }

        // 3. Execution
        auto start = std::chrono::high_resolution_clock::now();
        ToolResult result = executeInternal(name, args, ctx);
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        result.execution_time_ms = elapsed.count();

        // 4. Telemetry Update
        updateStats(name, result);

        // 5. Post-execution Hooks (Middleware)
        for (const auto& hook : post_hooks_) {
            try {
                hook(name, result);
            } catch (const std::exception& e) {
                std::cerr << "[ToolRegistry] Post-execution hook failed: " << e.what() << std::endl;
            }
        }

        return result;
    }

    std::future<ToolResult> ToolRegistry::executeToolAsync(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
        // Enforce async policy or thread pool usage here if needed. For now, std::async default.
        return std::async(std::launch::async, [this, name, args, ctx]() {
            return this->executeTool(name, args, ctx);
        });
    }

    ToolResult ToolRegistry::executeInternal(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx) {
        ToolExecutor executor;
        ToolMetadata metadata;

        {
            std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
            auto it = tools_.find(name);
            if (it == tools_.end()) return {false, "", "Tool disappeared during execution setup", 500, 0.0, {}};
            executor = it->second.executor;
            metadata = it->second.metadata;
        }

        // Validate Arguments
        for (const auto& reqArg : metadata.arguments) {
            if (reqArg.required && args.find(reqArg.name) == args.end()) {
                 // Check if it has a default value
                 if (!reqArg.default_value.has_value()) {
                     return {false, "", "Missing required argument: " + reqArg.name, 400, 0.0, {}};
                 }
            }
            
            // Basic Type/Value Validation (Stub for string conversions)
            if (args.find(reqArg.name) != args.end()) {
                const std::string& val = args.at(reqArg.name);
                
                // Validate Allowed Values
                if (!reqArg.allowed_values.empty()) {
                    bool found = false;
                    for (const auto& allowed : reqArg.allowed_values) {
                        if (allowed == val) { found = true; break; }
                    }
                    if (!found) {
                        return {false, "", "Invalid value for argument '" + reqArg.name + "'. See allowlist.", 400, 0.0, {}};
                    }
                }

                // Validate Min/Max for numbers
                if ((reqArg.type == ToolArgType::INTEGER || reqArg.type == ToolArgType::FLOAT) && (reqArg.min_value.has_value() || reqArg.max_value.has_value())) {
                    try {
                        double numVal = std::stod(val);
                        if (reqArg.min_value.has_value() && numVal < reqArg.min_value.value()) {
                            return {false, "", "Argument '" + reqArg.name + "' is below minimum value", 400, 0.0, {}};
                        }
                        if (reqArg.max_value.has_value() && numVal > reqArg.max_value.value()) {
                            return {false, "", "Argument '" + reqArg.name + "' is above maximum value", 400, 0.0, {}};
                        }
                    } catch (...) {
                         return {false, "", "Argument '" + reqArg.name + "' must be a number", 400, 0.0, {}};
                    }
                }
            }
        }

        // Prepare context args (merging defaults if necessary)
        std::map<std::string, std::string> finalArgs = args;
        for (const auto& arg : metadata.arguments) {
            if (finalArgs.find(arg.name) == finalArgs.end() && arg.default_value.has_value()) {
                finalArgs[arg.name] = arg.default_value.value();
            }
        }

        try {
            if (ctx.verbose_logging) {
                std::cout << "[ToolExecution] Running " << name << "..." << std::endl;
            }
            return executor(finalArgs, ctx);
        } catch (const std::exception& e) {
            return {false, "", std::string("Unhandled exception in tool '") + name + "': " + e.what(), 500, 0.0, {}};
        } catch (...) {
            return {false, "", "Unknown fatal error in tool '" + name + "'", 500, 0.0, {}};
        }
    }

    // =========================================================================================
    // Middleware & Stats
    // =========================================================================================

    void ToolRegistry::addPreHook(PreExecutionHook hook) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        pre_hooks_.push_back(hook);
    }

    void ToolRegistry::addPostHook(PostExecutionHook hook) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        post_hooks_.push_back(hook);
    }

    void ToolRegistry::clearHooks() {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        pre_hooks_.clear();
        post_hooks_.clear();
    }

    void ToolRegistry::updateStats(const std::string& name, const ToolResult& result) {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            ToolStats& stats = it->second.stats;
            stats.call_count++;
            if (!result.success) {
                stats.failure_count++;
            }
            stats.total_execution_time += result.execution_time_ms;
            
            // Recalculate average
            if (stats.call_count > 0) {
                stats.avg_execution_time = stats.total_execution_time / stats.call_count;
            }
        }
    }

    ToolRegistry::ToolStats ToolRegistry::getToolStats(const std::string& name) const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second.stats;
        }
        return {0, 0, 0.0, 0.0};
    }

    std::map<std::string, ToolRegistry::ToolStats> ToolRegistry::getAllStats() const {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex_);
        std::map<std::string, ToolStats> result;
        for (const auto& pair : tools_) {
            result[pair.first] = pair.second.stats;
        }
        return result;
    }

} // namespace rawrxd



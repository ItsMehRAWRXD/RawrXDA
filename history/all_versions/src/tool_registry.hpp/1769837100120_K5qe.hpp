#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <future>
#include <variant>
#include <chrono>

namespace rawrxd {

    /**
     * @brief Represents the data type of a tool argument.
     */
    enum class ToolArgType {
        STRING,
        INTEGER,
        BOOLEAN,
        FLOAT,
        JSON_OBJECT,
        ARRAY
    };

    /**
     * @brief Definition of a single argument required by a tool.
     */
    struct ToolArgument {
        std::string name;
        std::string description;
        ToolArgType type;
        bool required = true;
        std::optional<std::string> default_value;

        // Validation constraints
        std::optional<double> min_value;
        std::optional<double> max_value;
        std::vector<std::string> allowed_values;
    };

    /**
     * @brief Comprehensive metadata for tool discovery and capabilities.
     */
    struct ToolMetadata {
        std::string name;
        std::string description;
        std::string category;
        std::string version = "1.0.0";
        std::string author = "System";
        
        std::vector<ToolArgument> arguments;
        
        // Capabilities
        bool is_idempotent = false;
        bool requires_network = false;
        bool requires_filesystem = false;
        bool requires_user_approval = false;
        
        // Performance hints
        size_t estimated_memory_usage_mb = 0;
        double estimated_execution_time_ms = 0.0;
    };

    /**
     * @brief Result of a tool execution.
     */
    struct ToolResult {
        bool success;
        std::string output;     // Standard output or return value
        std::string error;      // Error message if failed
        int error_code = 0;
        
        double execution_time_ms;
        std::map<std::string, std::string> artifacts; // File paths or extra data generated
    };

    /**
     * @brief Context passed to the tool during execution.
     */
    struct ToolContext {
        std::string execution_id;
        std::string user_id;
        std::string workspace_root;
        std::map<std::string, std::string> env_vars;
        bool verbose_logging = false;
    };

    // The function signature for the actual tool logic
    using ToolExecutor = std::function<ToolResult(const std::map<std::string, std::string>& args, const ToolContext& ctx)>;

    // Middleware hooks
    using PreExecutionHook = std::function<bool(const std::string& tool_name, const std::map<std::string, std::string>& args)>;
    using PostExecutionHook = std::function<void(const std::string& tool_name, const ToolResult& result)>;

    /**
     * @brief Central registry for autonomous agent tools.
     *        Provides thread-safe registration, discovery, and execution.
     */
    class ToolRegistry {
    public:
        // Singleton access
        static ToolRegistry& instance();

        // Registration
        void registerTool(const ToolMetadata& metadata, ToolExecutor executor);
        void unregisterTool(const std::string& name);
        bool hasTool(const std::string& name) const;

        // Discovery
        std::vector<ToolMetadata> getAllTools() const;
        std::vector<ToolMetadata> getToolsByCategory(const std::string& category) const;
        std::optional<ToolMetadata> getToolMetadata(const std::string& name) const;

        // Execution
        ToolResult executeTool(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx);
        std::future<ToolResult> executeToolAsync(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx);

        // Middleware
        void addPreHook(PreExecutionHook hook);
        void addPostHook(PostExecutionHook hook);
        void clearHooks();

        // Telemetry
        struct ToolStats {
            size_t call_count = 0;
            size_t failure_count = 0;
            double total_execution_time = 0.0;
            double avg_execution_time = 0.0;
        };
        
        ToolStats getToolStats(const std::string& name) const;
        std::map<std::string, ToolStats> getAllStats() const;

    private:
        ToolRegistry() = default;
        ~ToolRegistry() = default;
        ToolRegistry(const ToolRegistry&) = delete;
        ToolRegistry& operator=(const ToolRegistry&) = delete;

        struct ToolEntry {
            ToolMetadata metadata;
            ToolExecutor executor;
            ToolStats stats;
        };

        mutable std::recursive_mutex registry_mutex_;
        std::map<std::string, ToolEntry> tools_;
        
        std::vector<PreExecutionHook> pre_hooks_;
        std::vector<PostExecutionHook> post_hooks_;

        ToolResult executeInternal(const std::string& name, const std::map<std::string, std::string>& args, const ToolContext& ctx);
        void updateStats(const std::string& name, const ToolResult& result);
    };

    // Convenience macro for static registration
    #define REGISTER_TOOL(name, desc, cat, args, exec) \
        static bool _tool_reg_##name = []() { \
            rawrxd::ToolRegistry::instance().registerTool({#name, desc, cat, args}, exec); \
            return true; \
        }();

} // namespace rawrxd



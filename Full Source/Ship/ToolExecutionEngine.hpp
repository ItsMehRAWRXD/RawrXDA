// ToolExecutionEngine.hpp - Tool Registry and Execution Framework
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "QtReplacements.hpp"
#include <functional>
#include <chrono>
#include <any>

namespace RawrXD {

// Tool result status
enum class ToolStatus {
    Success,
    Error,
    Timeout,
    InvalidParams,
    NotFound,
    PermissionDenied
};

// Tool result
struct ToolResult {
    ToolStatus status = ToolStatus::Success;
    JsonValue output;
    QString errorMessage;
    int64_t executionTimeMs = 0;

    bool isSuccess() const { return status == ToolStatus::Success; }

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"status"] = static_cast<int64_t>(status);
        obj[L"success"] = status == ToolStatus::Success;
        obj[L"output"] = output;
        obj[L"error"] = errorMessage.toStdWString();
        obj[L"executionTimeMs"] = executionTimeMs;
        return obj;
    }

    static ToolResult Success(const JsonValue& output) {
        ToolResult r;
        r.status = ToolStatus::Success;
        r.output = output;
        return r;
    }

    static ToolResult Error(const QString& message) {
        ToolResult r;
        r.status = ToolStatus::Error;
        r.errorMessage = message;
        return r;
    }
};

// Tool parameter definition
struct ToolParameter {
    QString name;
    QString type;  // "string", "number", "boolean", "array", "object"
    QString description;
    bool required = false;
    JsonValue defaultValue;

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"name"] = name.toStdWString();
        obj[L"type"] = type.toStdWString();
        obj[L"description"] = description.toStdWString();
        obj[L"required"] = required;
        return obj;
    }
};

// Tool definition
struct ToolDefinition {
    QString name;
    QString description;
    QString category;
    Vector<ToolParameter> parameters;
    bool requiresConfirmation = false;
    bool isDangerous = false;
    int timeoutMs = 30000;

    JsonObject toJson() const {
        JsonObject obj;
        obj[L"name"] = name.toStdWString();
        obj[L"description"] = description.toStdWString();
        obj[L"category"] = category.toStdWString();
        obj[L"requiresConfirmation"] = requiresConfirmation;
        obj[L"isDangerous"] = isDangerous;
        obj[L"timeoutMs"] = static_cast<int64_t>(timeoutMs);

        JsonObject params;
        params[L"type"] = String(L"object");
        JsonObject properties;
        JsonArray required;

        for (const auto& p : parameters) {
            properties[p.name.toStdWString()] = p.toJson();
            if (p.required) {
                required.push_back(p.name.toStdWString());
            }
        }
        params[L"properties"] = properties;
        params[L"required"] = required;
        obj[L"parameters"] = params;

        return obj;
    }
};

// Tool executor function type
using ToolExecutor = std::function<ToolResult(const JsonObject& params)>;

// Tool entry in registry
struct ToolEntry {
    ToolDefinition definition;
    ToolExecutor executor;
    bool enabled = true;
};

// Tool execution context
struct ToolContext {
    QString workingDirectory;
    QMap<QString, QString> environment;
    bool dryRun = false;
    int maxOutputLength = 100000;
    std::function<void(const QString&)> onOutput;
    std::function<bool(const QString&, const QString&)> onConfirmation;
};

// Tool execution engine
class ToolExecutionEngine {
public:
    ToolExecutionEngine() = default;

    // Register a tool
    void registerTool(const ToolDefinition& def, ToolExecutor executor) {
        QMutexLocker lock(&m_mutex);
        ToolEntry entry;
        entry.definition = def;
        entry.executor = std::move(executor);
        entry.enabled = true;
        m_tools[def.name] = entry;
    }

    // Unregister a tool
    void unregisterTool(const QString& name) {
        QMutexLocker lock(&m_mutex);
        m_tools.erase(m_tools.find(name));
    }

    // Check if tool exists
    bool hasTool(const QString& name) const {
        return m_tools.contains(name);
    }

    // Get tool definition
    Optional<ToolDefinition> getToolDefinition(const QString& name) const {
        auto it = m_tools.find(name);
        if (it != m_tools.end()) {
            return it->second.definition;
        }
        return std::nullopt;
    }

    // Get all tool definitions
    Vector<ToolDefinition> getAllToolDefinitions() const {
        Vector<ToolDefinition> result;
        for (const auto& [name, entry] : m_tools) {
            if (entry.enabled) {
                result.push_back(entry.definition);
            }
        }
        return result;
    }

    // Get tools schema for LLM
    JsonArray getToolsSchema() const {
        JsonArray schema;
        for (const auto& [name, entry] : m_tools) {
            if (entry.enabled) {
                JsonObject tool;
                tool[L"type"] = String(L"function");
                JsonObject func;
                func[L"name"] = entry.definition.name.toStdWString();
                func[L"description"] = entry.definition.description.toStdWString();

                JsonObject params;
                params[L"type"] = String(L"object");
                JsonObject properties;
                JsonArray required;

                for (const auto& p : entry.definition.parameters) {
                    JsonObject prop;
                    prop[L"type"] = p.type.toStdWString();
                    prop[L"description"] = p.description.toStdWString();
                    properties[p.name.toStdWString()] = prop;
                    if (p.required) {
                        required.push_back(p.name.toStdWString());
                    }
                }

                params[L"properties"] = properties;
                params[L"required"] = required;
                func[L"parameters"] = params;
                tool[L"function"] = func;
                schema.push_back(tool);
            }
        }
        return schema;
    }

    // Execute a tool
    ToolResult execute(const QString& name, const JsonObject& params) {
        auto start = std::chrono::steady_clock::now();

        auto it = m_tools.find(name);
        if (it == m_tools.end()) {
            ToolResult r;
            r.status = ToolStatus::NotFound;
            r.errorMessage = QString("Tool not found: ") + name;
            return r;
        }

        const auto& entry = it->second;
        if (!entry.enabled) {
            ToolResult r;
            r.status = ToolStatus::Error;
            r.errorMessage = QString("Tool is disabled: ") + name;
            return r;
        }

        // Validate required parameters
        for (const auto& p : entry.definition.parameters) {
            if (p.required && params.find(p.name.toStdWString()) == params.end()) {
                ToolResult r;
                r.status = ToolStatus::InvalidParams;
                r.errorMessage = QString("Missing required parameter: ") + p.name;
                return r;
            }
        }

        // Check confirmation requirement
        if (entry.definition.requiresConfirmation && m_context.onConfirmation) {
            if (!m_context.onConfirmation(name, entry.definition.description)) {
                ToolResult r;
                r.status = ToolStatus::PermissionDenied;
                r.errorMessage = QString("User denied execution");
                return r;
            }
        }

        // Execute
        ToolResult result;
        try {
            if (m_context.dryRun) {
                result.status = ToolStatus::Success;
                JsonObject dryRunOutput;
                dryRunOutput[L"dryRun"] = true;
                dryRunOutput[L"tool"] = name.toStdWString();
                dryRunOutput[L"params"] = params;
                result.output = dryRunOutput;
            } else {
                result = entry.executor(params);
            }
        } catch (const std::exception& e) {
            result.status = ToolStatus::Error;
            result.errorMessage = QString::fromStdString(e.what());
        }

        auto end = std::chrono::steady_clock::now();
        result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        return result;
    }

    // Execute with timeout
    ToolResult executeWithTimeout(const QString& name, const JsonObject& params, int timeoutMs) {
        std::atomic<bool> completed{false};
        ToolResult result;

        std::thread worker([&]() {
            result = execute(name, params);
            completed = true;
        });

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!completed && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (worker.joinable()) {
            if (completed) {
                worker.join();
            } else {
                worker.detach();
                result.status = ToolStatus::Timeout;
                result.errorMessage = QString("Tool execution timed out after %1ms").arg(timeoutMs);
            }
        }

        return result;
    }

    // Set execution context
    void setContext(const ToolContext& ctx) {
        m_context = ctx;
    }

    ToolContext& context() {
        return m_context;
    }

    // Enable/disable tool
    void setToolEnabled(const QString& name, bool enabled) {
        auto it = m_tools.find(name);
        if (it != m_tools.end()) {
            it->second.enabled = enabled;
        }
    }

    // Get tool categories
    QStringList getCategories() const {
        QStringList categories;
        for (const auto& [name, entry] : m_tools) {
            if (!categories.contains(entry.definition.category)) {
                categories.push_back(entry.definition.category);
            }
        }
        return categories;
    }

    // Get tools by category
    Vector<ToolDefinition> getToolsByCategory(const QString& category) const {
        Vector<ToolDefinition> result;
        for (const auto& [name, entry] : m_tools) {
            if (entry.enabled && entry.definition.category == category) {
                result.push_back(entry.definition);
            }
        }
        return result;
    }

private:
    QMap<QString, ToolEntry> m_tools;
    ToolContext m_context;
    mutable QMutex m_mutex;
};

// Tool builder helper
class ToolBuilder {
public:
    ToolBuilder(const QString& name) {
        m_def.name = name;
    }

    ToolBuilder& description(const QString& desc) {
        m_def.description = desc;
        return *this;
    }

    ToolBuilder& category(const QString& cat) {
        m_def.category = cat;
        return *this;
    }

    ToolBuilder& param(const QString& name, const QString& type, const QString& desc, bool required = false) {
        ToolParameter p;
        p.name = name;
        p.type = type;
        p.description = desc;
        p.required = required;
        m_def.parameters.push_back(p);
        return *this;
    }

    ToolBuilder& requiresConfirmation(bool confirm = true) {
        m_def.requiresConfirmation = confirm;
        return *this;
    }

    ToolBuilder& dangerous(bool danger = true) {
        m_def.isDangerous = danger;
        return *this;
    }

    ToolBuilder& timeout(int ms) {
        m_def.timeoutMs = ms;
        return *this;
    }

    ToolDefinition build() const {
        return m_def;
    }

private:
    ToolDefinition m_def;
};

// Helper to get string from JsonValue
inline QString jsonToString(const JsonValue& val) {
    if (std::holds_alternative<String>(val)) {
        return QString(std::get<String>(val));
    }
    return QString();
}

// Helper to get int from JsonValue
inline int64_t jsonToInt(const JsonValue& val, int64_t defaultVal = 0) {
    if (std::holds_alternative<int64_t>(val)) {
        return std::get<int64_t>(val);
    }
    return defaultVal;
}

// Helper to get bool from JsonValue
inline bool jsonToBool(const JsonValue& val, bool defaultVal = false) {
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val);
    }
    return defaultVal;
}

// Helper to get array from JsonValue
inline JsonArray jsonToArray(const JsonValue& val) {
    if (std::holds_alternative<JsonArray>(val)) {
        return std::get<JsonArray>(val);
    }
    return {};
}

// Helper to get object from JsonValue
inline JsonObject jsonToObject(const JsonValue& val) {
    if (std::holds_alternative<JsonObject>(val)) {
        return std::get<JsonObject>(val);
    }
    return {};
}

// Parameter extraction helpers
inline QString getParam(const JsonObject& params, const String& key, const QString& defaultVal = QString()) {
    auto it = params.find(key);
    if (it != params.end()) {
        return jsonToString(it->second);
    }
    return defaultVal;
}

inline int64_t getParamInt(const JsonObject& params, const String& key, int64_t defaultVal = 0) {
    auto it = params.find(key);
    if (it != params.end()) {
        return jsonToInt(it->second, defaultVal);
    }
    return defaultVal;
}

inline bool getParamBool(const JsonObject& params, const String& key, bool defaultVal = false) {
    auto it = params.find(key);
    if (it != params.end()) {
        return jsonToBool(it->second, defaultVal);
    }
    return defaultVal;
}

} // namespace RawrXD

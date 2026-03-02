#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <algorithm>

namespace RawrXD::Core {

struct ToolArgSchema {
    std::string name;
    std::string type;
    bool required = false;
    std::string description;
};

struct ToolSchema {
    std::string name;
    std::string description;
    std::vector<ToolArgSchema> args;
};

class ToolSchemaRegistry {
public:
    void registerTool(const ToolSchema& schema) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = std::find_if(schemas_.begin(), schemas_.end(), [&](const ToolSchema& s) { return s.name == schema.name; });
        if (it != schemas_.end()) {
            *it = schema;
            return;
        }
        schemas_.push_back(schema);
    }

    bool hasTool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mu_);
        return std::any_of(schemas_.begin(), schemas_.end(), [&](const ToolSchema& s) { return s.name == name; });
    }

    std::string emitJsonSchema() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::ostringstream out;
        out << "{\n  \"version\": \"1.0\",\n  \"tools\": [\n";

        for (size_t i = 0; i < schemas_.size(); ++i) {
            const auto& t = schemas_[i];
            out << "    {\n";
            out << "      \"name\": \"" << escapeJson(t.name) << "\",\n";
            out << "      \"description\": \"" << escapeJson(t.description) << "\",\n";
            out << "      \"args\": [\n";
            for (size_t j = 0; j < t.args.size(); ++j) {
                const auto& a = t.args[j];
                out << "        {\"name\": \"" << escapeJson(a.name)
                    << "\", \"type\": \"" << escapeJson(a.type)
                    << "\", \"required\": " << (a.required ? "true" : "false")
                    << ", \"description\": \"" << escapeJson(a.description) << "\"}";
                if (j + 1 < t.args.size()) out << ",";
                out << "\n";
            }
            out << "      ]\n";
            out << "    }";
            if (i + 1 < schemas_.size()) out << ",";
            out << "\n";
        }

        out << "  ]\n}\n";
        return out.str();
    }

private:
    static std::string escapeJson(const std::string& in) {
        std::string out;
        out.reserve(in.size() + 16);
        for (char c : in) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    }

    mutable std::mutex mu_;
    std::vector<ToolSchema> schemas_;
};

static ToolSchemaRegistry g_registry;

static void registerDefaults() {
    if (g_registry.hasTool("read_file")) return;

    g_registry.registerTool({
        "read_file",
        "Read file contents from workspace",
        {{"path", "string", true, "Workspace-relative file path"}, {"startLine", "integer", false, "Start line"}, {"endLine", "integer", false, "End line"}}
    });

    g_registry.registerTool({
        "list_dir",
        "List directory children",
        {{"path", "string", true, "Directory path"}}
    });

    g_registry.registerTool({
        "run_terminal",
        "Execute command in terminal",
        {{"command", "string", true, "Command line"}, {"cwd", "string", false, "Working directory"}}
    });
}

} // namespace RawrXD::Core

extern "C" {

void RawrXD_Core_RegisterToolSchema(const char* toolName, const char* description) {
    if (!toolName || !description) return;
    RawrXD::Core::registerDefaults();
    RawrXD::Core::g_registry.registerTool({toolName, description, {}});
}

const char* RawrXD_Core_EmitToolSchemaJson() {
    static thread_local std::string json;
    RawrXD::Core::registerDefaults();
    json = RawrXD::Core::g_registry.emitJsonSchema();
    return json.c_str();
}

}

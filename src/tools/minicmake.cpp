#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mini {

namespace fs = std::filesystem;

struct FileOps {
    static bool exists(const fs::path& p) {
        std::error_code ec;
        return fs::exists(p, ec);
    }

    static std::string read_text(const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        if (!in) return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static bool write_text(const fs::path& p, const std::string& text) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        std::ofstream out(p, std::ios::binary);
        if (!out) return false;
        out << text;
        return out.good();
    }

    static std::string hash_text(const std::string& s) {
        // FNV-1a 64-bit.
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : s) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ULL;
        }
        std::ostringstream os;
        os << std::hex << h;
        return os.str();
    }

    static std::string hash_file(const fs::path& p) {
        return hash_text(read_text(p));
    }
};

struct Target {
    std::string name;
    std::string kind; // executable | library | object
    std::vector<fs::path> sources;
    std::vector<fs::path> include_dirs;
    std::vector<std::string> compile_flags;
    std::vector<std::string> link_flags;
    std::vector<std::string> depends;
};

class BuildGraph {
public:
    void add_target(Target t) {
        targets_[t.name] = std::move(t);
    }

    bool has(const std::string& name) const {
        return targets_.find(name) != targets_.end();
    }

    Target& get(const std::string& name) {
        return targets_.at(name);
    }

    const std::unordered_map<std::string, Target>& all() const {
        return targets_;
    }

    bool topological_order(std::vector<std::string>& out, std::string& err) const {
        std::unordered_map<std::string, int> indegree;
        std::unordered_map<std::string, std::vector<std::string>> reverse_edges;

        for (const auto& kv : targets_) {
            indegree[kv.first] = 0;
        }

        for (const auto& kv : targets_) {
            const std::string& node = kv.first;
            for (const auto& dep : kv.second.depends) {
                auto it = targets_.find(dep);
                if (it == targets_.end()) {
                    err = "Unknown dependency '" + dep + "' for target '" + node + "'";
                    return false;
                }
                indegree[node] += 1;
                reverse_edges[dep].push_back(node);
            }
        }

        std::queue<std::string> q;
        for (const auto& kv : indegree) {
            if (kv.second == 0) q.push(kv.first);
        }

        out.clear();
        while (!q.empty()) {
            auto n = q.front();
            q.pop();
            out.push_back(n);
            auto rev_it = reverse_edges.find(n);
            if (rev_it != reverse_edges.end()) {
                for (const auto& depn : rev_it->second) {
                    int& d = indegree[depn];
                    d -= 1;
                    if (d == 0) q.push(depn);
                }
            }
        }

        if (out.size() != targets_.size()) {
            err = "Dependency cycle detected";
            return false;
        }
        return true;
    }

private:
    std::unordered_map<std::string, Target> targets_;
};

struct CmdResult {
    int exit_code = -1;
    std::string output;
};

class CommandRunner {
public:
    static CmdResult run(const std::string& cmd) {
        CmdResult r;
#if defined(_WIN32)
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) {
            r.output = "Failed to start process";
            return r;
        }

        std::array<char, 512> buf{};
        while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
            r.output += buf.data();
        }

#if defined(_WIN32)
        r.exit_code = _pclose(pipe);
#else
        r.exit_code = pclose(pipe);
#endif
        return r;
    }

    static bool run_parallel(const std::vector<std::string>& cmds, int jobs, std::vector<CmdResult>& out) {
        out.assign(cmds.size(), CmdResult{});
        if (cmds.empty()) return true;
        if (jobs < 1) jobs = 1;
        jobs = std::min<int>(jobs, static_cast<int>(cmds.size()));

        std::atomic<size_t> next{0};
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(jobs));

        for (int i = 0; i < jobs; ++i) {
            workers.emplace_back([&]() {
                while (true) {
                    size_t idx = next.fetch_add(1);
                    if (idx >= cmds.size()) break;
                    out[idx] = run(cmds[idx]);
                }
            });
        }
        for (auto& t : workers) t.join();

        for (const auto& r : out) {
            if (r.exit_code != 0) return false;
        }
        return true;
    }
};

class BuildCache {
public:
    explicit BuildCache(fs::path root = ".mini_build")
        : root_(std::move(root)), manifest_(root_ / "manifest.txt") {
        std::error_code ec;
        fs::create_directories(root_, ec);
        load();
    }

    bool needs_rebuild(const Target& t) const {
        const std::string current = target_fingerprint(t);
        auto it = hashes_.find(t.name);
        if (it == hashes_.end()) return true;
        return it->second != current;
    }

    void mark_built(const Target& t) {
        hashes_[t.name] = target_fingerprint(t);
    }

    void save() const {
        std::ostringstream os;
        for (const auto& kv : hashes_) {
            os << kv.first << '=' << kv.second << '\n';
        }
        FileOps::write_text(manifest_, os.str());
    }

private:
    std::string target_fingerprint(const Target& t) const {
        std::string fp = t.kind;
        for (const auto& s : t.sources) {
            fp += "|s:" + s.string() + ":" + FileOps::hash_file(s);
        }
        for (const auto& d : t.include_dirs) {
            fp += "|i:" + d.string();
        }
        for (const auto& f : t.compile_flags) fp += "|c:" + f;
        for (const auto& f : t.link_flags) fp += "|l:" + f;
        for (const auto& d : t.depends) fp += "|d:" + d;
        return FileOps::hash_text(fp);
    }

    void load() {
        const std::string text = FileOps::read_text(manifest_);
        std::istringstream is(text);
        std::string line;
        while (std::getline(is, line)) {
            if (line.empty()) continue;
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            hashes_[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }

    fs::path root_;
    fs::path manifest_;
    std::unordered_map<std::string, std::string> hashes_;
};

class Compiler {
public:
    virtual ~Compiler() = default;
    virtual std::string object_ext() const = 0;
    virtual std::string lib_ext() const = 0;
    virtual std::string exe_ext() const = 0;
    virtual std::string compile_one(const Target& t, const fs::path& src, const fs::path& obj) const = 0;
    virtual std::string link_executable(const Target& t, const std::vector<fs::path>& objects, const fs::path& out) const = 0;
    virtual std::string link_library(const Target& t, const std::vector<fs::path>& objects, const fs::path& out) const = 0;
};

class MSVCCompiler final : public Compiler {
public:
    std::string object_ext() const override { return ".obj"; }
    std::string lib_ext() const override { return ".lib"; }
    std::string exe_ext() const override { return ".exe"; }

    std::string compile_one(const Target& t, const fs::path& src, const fs::path& obj) const override {
        std::ostringstream cmd;
        cmd << "cl.exe /nologo /std:c++17 /c";
        for (const auto& inc : t.include_dirs) cmd << " /I\"" << inc.string() << "\"";
        for (const auto& flag : t.compile_flags) cmd << ' ' << flag;
        cmd << " \"" << src.string() << "\" /Fo\"" << obj.string() << "\"";
        return cmd.str();
    }

    std::string link_executable(const Target& t, const std::vector<fs::path>& objects, const fs::path& out) const override {
        std::ostringstream cmd;
        cmd << "link.exe /nologo";
        for (const auto& obj : objects) cmd << " \"" << obj.string() << "\"";
        for (const auto& flag : t.link_flags) cmd << ' ' << flag;
        cmd << " /OUT:\"" << out.string() << "\"";
        return cmd.str();
    }

    std::string link_library(const Target& t, const std::vector<fs::path>& objects, const fs::path& out) const override {
        std::ostringstream cmd;
        cmd << "lib.exe /nologo";
        for (const auto& obj : objects) cmd << " \"" << obj.string() << "\"";
        for (const auto& flag : t.link_flags) cmd << ' ' << flag;
        cmd << " /OUT:\"" << out.string() << "\"";
        return cmd.str();
    }
};

class GCCCompiler final : public Compiler {
public:
    std::string object_ext() const override { return ".o"; }
    std::string lib_ext() const override { return ".a"; }
    std::string exe_ext() const override { return ""; }

    std::string compile_one(const Target& t, const fs::path& src, const fs::path& obj) const override {
        std::ostringstream cmd;
        cmd << "g++ -std=c++17 -c";
        for (const auto& inc : t.include_dirs) cmd << " -I\"" << inc.string() << "\"";
        for (const auto& flag : t.compile_flags) cmd << ' ' << flag;
        cmd << " \"" << src.string() << "\" -o \"" << obj.string() << "\"";
        return cmd.str();
    }

    std::string link_executable(const Target& t, const std::vector<fs::path>& objects, const fs::path& out) const override {
        std::ostringstream cmd;
        cmd << "g++";
        for (const auto& obj : objects) cmd << " \"" << obj.string() << "\"";
        for (const auto& flag : t.link_flags) cmd << ' ' << flag;
        cmd << " -o \"" << out.string() << "\"";
        return cmd.str();
    }

    std::string link_library(const Target&, const std::vector<fs::path>& objects, const fs::path& out) const override {
        std::ostringstream cmd;
        cmd << "ar rcs \"" << out.string() << "\"";
        for (const auto& obj : objects) cmd << " \"" << obj.string() << "\"";
        return cmd.str();
    }
};

static std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

class ConfigParser {
public:
    static bool parse(const fs::path& cfg_path, BuildGraph& out, std::string& err) {
        std::ifstream in(cfg_path);
        if (!in) {
            err = "Failed to open config: " + cfg_path.string();
            return false;
        }

        Target current;
        bool in_target = false;
        std::string line;
        size_t line_no = 0;

        while (std::getline(in, line)) {
            ++line_no;
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            std::istringstream ss(line);
            std::string key;
            ss >> key;

            if (key == "target") {
                if (in_target) {
                    err = "Nested target at line " + std::to_string(line_no);
                    return false;
                }
                ss >> current.name >> current.kind;
                if (current.name.empty() || current.kind.empty()) {
                    err = "Invalid target declaration at line " + std::to_string(line_no);
                    return false;
                }
                in_target = true;
                continue;
            }

            if (key == "end_target") {
                if (!in_target) {
                    err = "end_target without target at line " + std::to_string(line_no);
                    return false;
                }
                out.add_target(std::move(current));
                current = Target{};
                in_target = false;
                continue;
            }

            if (!in_target) {
                err = "Property outside target block at line " + std::to_string(line_no);
                return false;
            }

            std::string v;
            if (key == "sources") {
                while (ss >> v) current.sources.emplace_back(v);
            } else if (key == "include_dirs") {
                while (ss >> v) current.include_dirs.emplace_back(v);
            } else if (key == "compile_flags") {
                while (ss >> v) current.compile_flags.push_back(v);
            } else if (key == "link_flags") {
                while (ss >> v) current.link_flags.push_back(v);
            } else if (key == "depends") {
                while (ss >> v) current.depends.push_back(v);
            } else {
                err = "Unknown directive '" + key + "' at line " + std::to_string(line_no);
                return false;
            }
        }

        if (in_target) {
            err = "Unclosed target at end of file";
            return false;
        }

        return true;
    }
};

class Builder {
public:
    Builder(Compiler& compiler, fs::path root)
        : compiler_(compiler), cache_(root / ".mini_build"), build_root_(std::move(root)) {}

    bool build(const BuildGraph& graph, const std::string& only, int jobs, std::string& err) {
        std::vector<std::string> order;
        if (!graph.topological_order(order, err)) return false;

        for (const auto& name : order) {
            if (!only.empty() && name != only) continue;

            const Target& t = graph.all().at(name);
            if (t.sources.empty()) {
                std::cout << "[SKIP] " << name << " (no sources)\n";
                continue;
            }

            if (!cache_.needs_rebuild(t)) {
                std::cout << "[UP-TO-DATE] " << name << "\n";
                continue;
            }

            std::cout << "[BUILD] " << name << "\n";
            fs::path obj_dir = build_root_ / ".mini_build" / "obj" / name;
            std::error_code ec;
            fs::create_directories(obj_dir, ec);

            std::vector<fs::path> objects;
            std::vector<std::string> compile_cmds;
            compile_cmds.reserve(t.sources.size());

            for (const auto& src : t.sources) {
                fs::path obj = obj_dir / (src.filename().string() + compiler_.object_ext());
                objects.push_back(obj);
                compile_cmds.push_back(compiler_.compile_one(t, src, obj));
            }

            std::vector<CmdResult> compile_results;
            if (!CommandRunner::run_parallel(compile_cmds, jobs, compile_results)) {
                err = "Compilation failed for target '" + name + "'\n";
                for (const auto& r : compile_results) {
                    if (!r.output.empty()) err += r.output;
                }
                return false;
            }

            fs::path out = build_root_ / (name + artifact_extension(t.kind));
            std::string link_cmd;
            if (t.kind == "library") {
                link_cmd = compiler_.link_library(t, objects, out);
            } else {
                link_cmd = compiler_.link_executable(t, objects, out);
            }

            CmdResult link_result = CommandRunner::run(link_cmd);
            if (link_result.exit_code != 0) {
                err = "Link failed for target '" + name + "'\n" + link_result.output;
                return false;
            }

            cache_.mark_built(t);
            cache_.save();
            std::cout << "[OK] " << name << " -> " << out.string() << "\n";
        }

        return true;
    }

private:
    std::string artifact_extension(const std::string& kind) const {
        if (kind == "library") return compiler_.lib_ext();
        if (kind == "object") return compiler_.object_ext();
        return compiler_.exe_ext();
    }

    Compiler& compiler_;
    BuildCache cache_;
    fs::path build_root_;
};

} // namespace mini

static void print_usage() {
    std::cout
        << "minicmake - tiny zero-dependency C++ build driver\n"
        << "Usage:\n"
        << "  minicmake [--config <path>] [--target <name>] [--jobs <n>]\n"
        << "Defaults:\n"
        << "  --config Build.cfg\n"
        << "  --jobs   1\n";
}

int main(int argc, char** argv) {
    std::filesystem::path config = "Build.cfg";
    std::string target;
    int jobs = 1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            print_usage();
            return 0;
        }
        if (a == "--config" && i + 1 < argc) {
            config = argv[++i];
            continue;
        }
        if (a == "--target" && i + 1 < argc) {
            target = argv[++i];
            continue;
        }
        if (a == "--jobs" && i + 1 < argc) {
            jobs = std::max(1, std::atoi(argv[++i]));
            continue;
        }
        std::cerr << "Unknown argument: " << a << "\n";
        print_usage();
        return 2;
    }

    mini::BuildGraph graph;
    std::string err;
    if (!mini::ConfigParser::parse(config, graph, err)) {
        std::cerr << "Config parse error: " << err << "\n";
        return 1;
    }

    if (!target.empty() && !graph.has(target)) {
        std::cerr << "Unknown target: " << target << "\n";
        return 1;
    }

#if defined(_WIN32)
    mini::MSVCCompiler compiler;
#else
    mini::GCCCompiler compiler;
#endif

    mini::Builder b(compiler, std::filesystem::current_path());
    if (!b.build(graph, target, jobs, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    std::cout << "Build completed\n";
    return 0;
}

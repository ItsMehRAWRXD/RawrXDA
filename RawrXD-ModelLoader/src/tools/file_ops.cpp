#define NOMINMAX
#include "tools/file_ops.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Tools {

static FileOpResult ok(const std::string& msg = "ok", const std::optional<std::string>& path = std::nullopt) {
    return {true, msg, path};
}

static FileOpResult fail(const std::string& msg, const std::optional<std::string>& path = std::nullopt) {
    return {false, msg, path};
}

FileOpResult FileOps::readText(const std::string& path, std::string& out) {
    try {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) return fail("Unable to open file for reading", path);
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out = ss.str();
        return ok("Read text", path);
    } catch (const std::exception& e) {
        return fail(std::string("readText error: ") + e.what(), path);
    }
}

FileOpResult FileOps::writeText(const std::string& path, const std::string& content, bool create_dirs) {
    try {
        if (create_dirs) {
            fs::path p(path);
            if (p.has_parent_path()) fs::create_directories(p.parent_path());
        }
        std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs) return fail("Unable to open file for writing", path);
        ofs << content;
        return ok("Wrote text", path);
    } catch (const std::exception& e) {
        return fail(std::string("writeText error: ") + e.what(), path);
    }
}

FileOpResult FileOps::appendText(const std::string& path, const std::string& content) {
    try {
        std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::app);
        if (!ofs) return fail("Unable to open file for appending", path);
        ofs << content;
        return ok("Appended text", path);
    } catch (const std::exception& e) {
        return fail(std::string("appendText error: ") + e.what(), path);
    }
}

FileOpResult FileOps::remove(const std::string& path) {
    try {
        std::error_code ec;
        bool removed = fs::remove_all(path, ec) > 0 && !ec;
        if (!removed) return fail("Nothing removed or error", path);
        return ok("Removed", path);
    } catch (const std::exception& e) {
        return fail(std::string("remove error: ") + e.what(), path);
    }
}

FileOpResult FileOps::rename(const std::string& from, const std::string& to, bool create_dirs) {
    try {
        if (create_dirs) {
            fs::path p(to);
            if (p.has_parent_path()) fs::create_directories(p.parent_path());
        }
        fs::rename(from, to);
        return ok("Renamed", to);
    } catch (const std::exception& e) {
        return fail(std::string("rename error: ") + e.what(), to);
    }
}

FileOpResult FileOps::copy(const std::string& from, const std::string& to, const CopyOptions& opts) {
    try {
        fs::path dst(to);
        if (opts.create_dirs && dst.has_parent_path()) fs::create_directories(dst.parent_path());
        fs::copy_options c = fs::copy_options::none;
        if (opts.overwrite) c |= fs::copy_options::overwrite_existing;
        if (fs::is_directory(from)) c |= fs::copy_options::recursive;
        fs::copy(from, to, c);
        if (opts.preserve_timestamps) {
            std::error_code ec;
            auto ftime = fs::last_write_time(from, ec);
            if (!ec) fs::last_write_time(to, ftime, ec);
        }
        return ok("Copied", to);
    } catch (const std::exception& e) {
        return fail(std::string("copy error: ") + e.what(), to);
    }
}

FileOpResult FileOps::move(const std::string& from, const std::string& to, bool overwrite) {
    try {
        if (overwrite && fs::exists(to)) fs::remove_all(to);
        return rename(from, to, true);
    } catch (const std::exception& e) {
        return fail(std::string("move error: ") + e.what(), to);
    }
}

FileOpResult FileOps::ensureDir(const std::string& path) {
    try {
        fs::create_directories(path);
        return ok("Ensured directory", path);
    } catch (const std::exception& e) {
        return fail(std::string("ensureDir error: ") + e.what(), path);
    }
}

FileOpResult FileOps::list(const std::string& path, std::vector<std::string>& out, bool recursive) {
    try {
        if (!fs::exists(path)) return fail("Path does not exist", path);
        if (recursive) {
            for (auto& p : fs::recursive_directory_iterator(path)) out.push_back(p.path().string());
        } else {
            for (auto& p : fs::directory_iterator(path)) out.push_back(p.path().string());
        }
        return ok("Listed", path);
    } catch (const std::exception& e) {
        return fail(std::string("list error: ") + e.what(), path);
    }
}

bool FileOps::exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

} // namespace Tools
} // namespace RawrXD

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace rawrxd::agent::safety {

enum class CommandClass : uint8_t {
    Safe = 0,
    Destructive = 1,
    Network = 2,
    Privilege = 3,
    Unknown = 255,
};

constexpr uint32_t fnv1a_ci(std::string_view text) {
    uint32_t h = 0x811C9DC5u;
    for (char c : text) {
        const char folded = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
        h ^= static_cast<uint8_t>(folded);
        h *= 0x01000193u;
    }
    return h;
}

constexpr std::array<uint32_t, 8> kDestructive = {
    fnv1a_ci("rm"),
    fnv1a_ci("del"),
    fnv1a_ci("erase"),
    fnv1a_ci("format"),
    fnv1a_ci("rd"),
    fnv1a_ci("rmdir"),
    fnv1a_ci("diskpart"),
    fnv1a_ci("sdelete"),
};

constexpr std::array<uint32_t, 7> kNetwork = {
    fnv1a_ci("curl"),
    fnv1a_ci("wget"),
    fnv1a_ci("invoke-webrequest"),
    fnv1a_ci("invoke-restmethod"),
    fnv1a_ci("powershell"),
    fnv1a_ci("pwsh"),
    fnv1a_ci("bitsadmin"),
};

constexpr std::array<uint32_t, 4> kPrivilege = {
    fnv1a_ci("runas"),
    fnv1a_ci("psexec"),
    fnv1a_ci("schtasks"),
    fnv1a_ci("reg"),
};

constexpr bool hash_in(const auto& table, uint32_t h) {
    for (uint32_t v : table) {
        if (v == h) {
            return true;
        }
    }
    return false;
}

inline std::string_view basename(std::string_view cmd) {
    const size_t slash = cmd.find_last_of("/\\");
    if (slash != std::string_view::npos) {
        cmd = cmd.substr(slash + 1);
    }
    const size_t dot = cmd.find('.');
    if (dot != std::string_view::npos) {
        cmd = cmd.substr(0, dot);
    }
    return cmd;
}

inline CommandClass classify(std::string_view command) {
    command = basename(command);
    if (command.empty()) {
        return CommandClass::Unknown;
    }
    const uint32_t h = fnv1a_ci(command);
    if (hash_in(kDestructive, h)) {
        return CommandClass::Destructive;
    }
    if (hash_in(kNetwork, h)) {
        return CommandClass::Network;
    }
    if (hash_in(kPrivilege, h)) {
        return CommandClass::Privilege;
    }
    return CommandClass::Safe;
}

inline bool has_destructive_args(std::string_view args) {
    return args.find(":\\") != std::string_view::npos ||
           args.find(" /s") != std::string_view::npos ||
           args.find(" /q") != std::string_view::npos ||
           args.find(" -rf") != std::string_view::npos ||
           args.find(" *") != std::string_view::npos;
}

inline bool is_destructive(std::string_view command, std::string_view args) {
    const CommandClass cls = classify(command);
    if (cls == CommandClass::Destructive) {
        return true;
    }
    if (cls == CommandClass::Privilege) {
        return true;
    }
    if (cls == CommandClass::Network) {
        return args.find("http://") != std::string_view::npos ||
               args.find("https://") != std::string_view::npos ||
               args.find(" -o") != std::string_view::npos;
    }
    return has_destructive_args(args);
}

}  // namespace rawrxd::agent::safety

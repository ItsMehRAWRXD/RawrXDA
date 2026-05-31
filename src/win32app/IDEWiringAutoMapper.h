#pragma once

#include "../../include/enterprise_license.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace RawrXD::Wiring {

struct AutoMapperOptions {
    int reinspectPasses = 2;
    bool includeLegacy = true;
    bool checkCliGuiBoundaries = true;
};

struct FeatureCoverage {
    uint32_t featureIndex = 0;
    std::string featureName;
    std::string sourceFile;
    std::string resolvedSourcePath;
    bool sourceExists = false;

    bool manifestImplemented = false;
    bool manifestWiredToUI = false;
    bool manifestTested = false;

    bool cliReferenced = false;
    bool guiReferenced = false;

    bool cliHasStart = false;
    bool cliHasEnd = false;
    bool guiHasStart = false;
    bool guiHasEnd = false;
    bool sourceHasStart = false;
    bool sourceHasEnd = false;

    uint32_t reinspectHits = 0;
    std::vector<std::string> recommendations;
};

struct AutoMapperReport {
    std::string repoRoot;
    int reinspectPassesApplied = 1;
    size_t filesScanned = 0;
    size_t featuresScanned = 0;

    size_t missingSourceCount = 0;
    size_t boundaryGapCount = 0;
    size_t recommendationCount = 0;

    std::vector<FeatureCoverage> features;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["repo_root"] = repoRoot;
        j["reinspect_passes"] = reinspectPassesApplied;
        j["files_scanned"] = filesScanned;
        j["features_scanned"] = featuresScanned;
        j["missing_source_count"] = missingSourceCount;
        j["boundary_gap_count"] = boundaryGapCount;
        j["recommendation_count"] = recommendationCount;

        nlohmann::json items = nlohmann::json::array();
        for (const auto& f : features) {
            nlohmann::json item;
            item["feature_index"] = f.featureIndex;
            item["feature_name"] = f.featureName;
            item["source_file"] = f.sourceFile;
            item["resolved_source_path"] = f.resolvedSourcePath;
            item["source_exists"] = f.sourceExists;
            item["manifest_implemented"] = f.manifestImplemented;
            item["manifest_wired_to_ui"] = f.manifestWiredToUI;
            item["manifest_tested"] = f.manifestTested;
            item["cli_referenced"] = f.cliReferenced;
            item["gui_referenced"] = f.guiReferenced;
            item["cli_has_start"] = f.cliHasStart;
            item["cli_has_end"] = f.cliHasEnd;
            item["gui_has_start"] = f.guiHasStart;
            item["gui_has_end"] = f.guiHasEnd;
            item["source_has_start"] = f.sourceHasStart;
            item["source_has_end"] = f.sourceHasEnd;
            item["reinspect_hits"] = f.reinspectHits;
            item["recommendations"] = f.recommendations;
            items.push_back(std::move(item));
        }
        j["features"] = std::move(items);
        return j;
    }
};

class IDEWiringAutoMapper {
public:
    static AutoMapperReport run(const AutoMapperOptions& options) {
        const auto repoRoot = detectRepoRoot();
        std::vector<FileRecord> files = collectFiles(repoRoot, options.includeLegacy);

        AutoMapperReport report;
        report.repoRoot = repoRoot.generic_string();
        report.filesScanned = files.size();
        report.reinspectPassesApplied = std::max(1, options.reinspectPasses);

        for (uint32_t i = 0; i < RawrXD::License::TOTAL_FEATURES; ++i) {
            const auto& def = RawrXD::License::g_FeatureManifest[i];

            FeatureCoverage out;
            out.featureIndex = i;
            out.featureName = safeStr(def.name);
            out.sourceFile = safeStr(def.sourceFile);
            out.manifestImplemented = def.implemented;
            out.manifestWiredToUI = def.wiredToUI;
            out.manifestTested = def.tested;

            const auto sourcePath = resolveSourcePath(repoRoot, out.sourceFile, files);
            out.resolvedSourcePath = sourcePath.generic_string();
            out.sourceExists = (!sourcePath.empty() && std::filesystem::exists(sourcePath));

            std::vector<std::string> tokens = buildFeatureTokens(out.featureName, out.sourceFile);
            for (int pass = 0; pass < report.reinspectPassesApplied; ++pass) {
                const size_t minTokenLen = (pass == 0) ? 5u : 3u;
                for (const auto& fr : files) {
                    if (!containsFeatureToken(fr.lowerContent, tokens, minTokenLen)) {
                        continue;
                    }

                    out.reinspectHits += 1;

                    if (fr.isCliSurface) {
                        out.cliReferenced = true;
                        out.cliHasStart = out.cliHasStart || hasStartHook(fr.lowerContent);
                        out.cliHasEnd = out.cliHasEnd || hasEndHook(fr.lowerContent);
                    }
                    if (fr.isGuiSurface) {
                        out.guiReferenced = true;
                        out.guiHasStart = out.guiHasStart || hasStartHook(fr.lowerContent);
                        out.guiHasEnd = out.guiHasEnd || hasEndHook(fr.lowerContent);
                    }
                }
            }

            if (out.sourceExists) {
                const std::string sourceLower = loadLowerFile(sourcePath);
                out.sourceHasStart = hasStartHook(sourceLower);
                out.sourceHasEnd = hasEndHook(sourceLower);
            }

            fillRecommendations(out, options.checkCliGuiBoundaries);

            if (!out.sourceExists) {
                report.missingSourceCount += 1;
            }

            if (options.checkCliGuiBoundaries) {
                const bool cliGap = out.cliReferenced && (!out.cliHasStart || !out.cliHasEnd);
                const bool guiGap = out.guiReferenced && (!out.guiHasStart || !out.guiHasEnd);
                if (cliGap || guiGap) {
                    report.boundaryGapCount += 1;
                }
            }

            report.recommendationCount += out.recommendations.size();
            report.features.push_back(std::move(out));
        }

        report.featuresScanned = report.features.size();
        return report;
    }

    static bool writeJson(const AutoMapperReport& report, const std::string& path) {
        if (path.empty()) {
            return false;
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        out << report.toJson().dump(2);
        return out.good();
    }

    static bool writeSubmissionJson(const AutoMapperReport& report, const std::string& path) {
        if (path.empty()) {
            return false;
        }

        nlohmann::json submission;
        submission["type"] = "feature_wiring_submission";
        submission["summary"] = {
            {"features", report.featuresScanned},
            {"missing_sources", report.missingSourceCount},
            {"boundary_gaps", report.boundaryGapCount},
            {"recommendations", report.recommendationCount}
        };

        nlohmann::json actions = nlohmann::json::array();
        for (const auto& f : report.features) {
            if (f.recommendations.empty()) {
                continue;
            }

            nlohmann::json a;
            a["feature_index"] = f.featureIndex;
            a["feature_name"] = f.featureName;
            a["source_file"] = f.sourceFile;
            a["recommendations"] = f.recommendations;
            actions.push_back(std::move(a));
        }
        submission["actions"] = std::move(actions);

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        out << submission.dump(2);
        return out.good();
    }

private:
    struct FileRecord {
        std::filesystem::path path;
        std::string lowerContent;
        bool isCliSurface = false;
        bool isGuiSurface = false;
    };

    static std::filesystem::path detectRepoRoot() {
        std::error_code ec;
        std::filesystem::path p = std::filesystem::current_path(ec);
        if (ec) {
            p = std::filesystem::path("d:/rawrxd");
        }

        for (int i = 0; i < 8; ++i) {
            if (std::filesystem::exists(p / "include" / "enterprise_license.h") &&
                std::filesystem::exists(p / "src")) {
                return p;
            }
            if (!p.has_parent_path()) {
                break;
            }
            p = p.parent_path();
        }

        return std::filesystem::path("d:/rawrxd");
    }

    static bool isSourceLikeFile(const std::filesystem::path& p) {
        const std::string ext = lower(p.extension().string());
        return ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c" || ext == ".h" ||
               ext == ".hpp" || ext == ".hh" || ext == ".asm";
    }

    static std::vector<FileRecord> collectFiles(const std::filesystem::path& repoRoot, bool includeLegacy) {
        std::vector<FileRecord> out;
        std::error_code ec;
        const std::filesystem::path srcRoot = repoRoot / "src";
        const std::filesystem::path includeRoot = repoRoot / "include";

        auto collectFrom = [&](const std::filesystem::path& root) {
            if (!std::filesystem::exists(root)) {
                return;
            }

            for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
                 !ec && it != std::filesystem::recursive_directory_iterator(); ++it) {
                const auto& entry = *it;
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto path = entry.path();
                const std::string lowPath = lower(path.generic_string());
                if (!includeLegacy && lowPath.find("/legacy/") != std::string::npos) {
                    continue;
                }
                if (lowPath.find("/history/") != std::string::npos ||
                    lowPath.find("/full source/") != std::string::npos) {
                    continue;
                }
                if (!isSourceLikeFile(path)) {
                    continue;
                }

                FileRecord fr;
                fr.path = path;
                fr.lowerContent = loadLowerFile(path);
                fr.isCliSurface = isCliPath(lowPath);
                fr.isGuiSurface = isGuiPath(lowPath);
                out.push_back(std::move(fr));
            }
        };

        collectFrom(srcRoot);
        collectFrom(includeRoot);
        return out;
    }

    static std::string loadLowerFile(const std::filesystem::path& p) {
        std::ifstream in(p, std::ios::binary);
        if (!in.is_open()) {
            return std::string();
        }

        std::string data;
        in.seekg(0, std::ios::end);
        std::streamoff size = in.tellg();
        if (size < 0 || size > static_cast<std::streamoff>(2 * 1024 * 1024)) {
            return std::string();
        }
        data.resize(static_cast<size_t>(size));
        in.seekg(0, std::ios::beg);
        in.read(data.data(), static_cast<std::streamsize>(data.size()));

        return lower(data);
    }

    static std::filesystem::path resolveSourcePath(const std::filesystem::path& repoRoot,
                                                   const std::string& sourceFile,
                                                   const std::vector<FileRecord>& files) {
        if (sourceFile.empty()) {
            return {};
        }

        const std::filesystem::path sourcePath(sourceFile);
        std::error_code ec;
        if (sourcePath.is_absolute() && std::filesystem::exists(sourcePath, ec)) {
            return sourcePath;
        }

        const auto p1 = repoRoot / sourcePath;
        if (std::filesystem::exists(p1, ec)) {
            return p1;
        }

        const auto p2 = repoRoot / "src" / sourcePath;
        if (std::filesystem::exists(p2, ec)) {
            return p2;
        }

        const std::string want = lower(sourcePath.generic_string());
        for (const auto& fr : files) {
            const std::string have = lower(fr.path.generic_string());
            if (have.size() >= want.size() && have.compare(have.size() - want.size(), want.size(), want) == 0) {
                return fr.path;
            }
        }

        return p1;
    }

    static std::vector<std::string> buildFeatureTokens(const std::string& featureName,
                                                        const std::string& sourceFile) {
        std::vector<std::string> tokens;

        auto addTokens = [&](const std::string& text) {
            std::string t;
            for (char c : text) {
                unsigned char uc = static_cast<unsigned char>(c);
                if (std::isalnum(uc) || c == '_') {
                    t.push_back(static_cast<char>(std::tolower(uc)));
                } else {
                    if (!t.empty()) {
                        tokens.push_back(t);
                        t.clear();
                    }
                }
            }
            if (!t.empty()) {
                tokens.push_back(t);
            }
        };

        addTokens(featureName);
        addTokens(std::filesystem::path(sourceFile).stem().string());

        std::sort(tokens.begin(), tokens.end());
        tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
        return tokens;
    }

    static bool containsFeatureToken(const std::string& haystack,
                                     const std::vector<std::string>& tokens,
                                     size_t minLen) {
        if (haystack.empty()) {
            return false;
        }

        for (const auto& t : tokens) {
            if (t.size() < minLen) {
                continue;
            }
            if (haystack.find(t) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static bool hasStartHook(const std::string& text) {
        static const char* kStart[] = {
            "initialize", "init", "start", "create", "open", "enable", "register", "load", "run"
        };
        return hasAny(text, kStart, sizeof(kStart) / sizeof(kStart[0]));
    }

    static bool hasEndHook(const std::string& text) {
        static const char* kEnd[] = {
            "shutdown", "stop", "destroy", "close", "disable", "unregister", "unload", "cleanup", "teardown", "end"
        };
        return hasAny(text, kEnd, sizeof(kEnd) / sizeof(kEnd[0]));
    }

    static bool hasAny(const std::string& text, const char* const* words, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            if (text.find(words[i]) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static void fillRecommendations(FeatureCoverage& out, bool checkBoundaries) {
        if (!out.sourceExists) {
            out.recommendations.push_back("missing_source_file");
        }

        if (out.sourceExists && !out.manifestImplemented) {
            out.recommendations.push_back("manifest_implemented_can_be_rechecked");
        }

        if (out.guiReferenced && !out.manifestWiredToUI) {
            out.recommendations.push_back("manifest_wiredToUI_can_be_rechecked");
        }

        if ((out.cliReferenced || out.guiReferenced) && !out.manifestTested) {
            out.recommendations.push_back("add_or_link_smoke_test_coverage");
        }

        if (!checkBoundaries) {
            return;
        }

        if (out.cliReferenced && !out.cliHasStart) {
            out.recommendations.push_back("cli_missing_start_hook");
        }
        if (out.cliReferenced && !out.cliHasEnd) {
            out.recommendations.push_back("cli_missing_end_hook");
        }
        if (out.guiReferenced && !out.guiHasStart) {
            out.recommendations.push_back("gui_missing_start_hook");
        }
        if (out.guiReferenced && !out.guiHasEnd) {
            out.recommendations.push_back("gui_missing_end_hook");
        }
        if (out.sourceExists && !out.sourceHasStart) {
            out.recommendations.push_back("source_missing_start_semantic");
        }
        if (out.sourceExists && !out.sourceHasEnd) {
            out.recommendations.push_back("source_missing_end_semantic");
        }
    }

    static std::string safeStr(const char* s) {
        return (s != nullptr) ? std::string(s) : std::string();
    }

    static std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    static bool isCliPath(const std::string& lowPath) {
        return (lowPath.find("/cli/") != std::string::npos) ||
               (lowPath.find("cli_") != std::string::npos) ||
               (lowPath.find("headlesside") != std::string::npos) ||
               (lowPath.find("sovereigncliide") != std::string::npos);
    }

    static bool isGuiPath(const std::string& lowPath) {
        const bool win32Surface = (lowPath.find("/win32app/") != std::string::npos) ||
                                  (lowPath.find("/ui/") != std::string::npos) ||
                                  (lowPath.find("win32ide") != std::string::npos);
        return win32Surface && !isCliPath(lowPath);
    }
};

}  // namespace RawrXD::Wiring

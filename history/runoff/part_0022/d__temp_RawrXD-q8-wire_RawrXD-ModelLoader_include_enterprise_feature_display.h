// enterprise_feature_display.h — Consolidated Enterprise Feature Display
// ============================================================================
// Provides a single API to query and display ALL enterprise features,
// their wiring status, implementation phase, and license gate status.
//
// This header bridges:
//   - enterprise_license.h       (tier/feature definitions, key creation)
//   - feature_flags_runtime.h    (runtime toggles, dev-mode unlock)
//   - license_enforcement.h      (audit-logged enforcement gates)
//   - dual_engine_inference.h    (toggleable 800B dual-engine gate)
//
// Use this from IDE panels, CLI tools, or any subsystem that needs
// to display the complete feature landscape.
// ============================================================================

#pragma once

#include "enterprise_license.h"
#include "feature_flags_runtime.h"
#include "license_enforcement.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// ============================================================================
// Feature Display Entry — aggregated view of one feature
// ============================================================================
struct FeatureDisplayEntry {
    EnterpriseFeature id;
    const char* name;
    const char* description;
    LicenseTier requiredTier;

    // Implementation status
    bool implemented;          // Source file exists
    bool wiredToBuild;         // Linked into CMake target
    bool tested;               // Has test harness
    int  phase;                // 1=done, 2=wiring, 3=planned

    // Runtime status
    bool unlockedByLicense;    // Current license tier >= required
    bool enabledByFlags;       // Runtime feature flags allow it
    bool enforcementAllowed;   // Enforcement gate would pass

    const char* sourceFile;
    const char* headerFile;
    const char* notes;
};

// ============================================================================
// Enterprise Feature Display — consolidated query interface
// ============================================================================
class EnterpriseFeatureDisplay {
public:
    static EnterpriseFeatureDisplay& getInstance() {
        static EnterpriseFeatureDisplay instance;
        return instance;
    }

    // Build the full display list from all subsystems
    std::vector<FeatureDisplayEntry> getAllFeatures() const {
        auto& licMgr = EnterpriseLicenseManager::getInstance();
        auto& flagMgr = FeatureFlagRuntime::getInstance();
        auto& enfGate = LicenseEnforcementGate::getInstance();

        auto manifest = licMgr.getFullManifest();
        std::vector<FeatureDisplayEntry> result;

        for (const auto& m : manifest) {
            FeatureDisplayEntry entry;
            entry.id = m.id;
            entry.name = m.name;
            entry.description = m.description;
            entry.requiredTier = m.requiredTier;
            entry.implemented = m.implemented;
            entry.wiredToBuild = m.wired;
            entry.tested = m.tested;
            entry.sourceFile = m.sourceFile;
            entry.headerFile = m.headerFile;

            // Determine phase from implementation status
            if (m.implemented && m.wired) {
                entry.phase = 1;
            } else if (m.implemented && !m.wired) {
                entry.phase = 2;
            } else {
                entry.phase = 3;
            }

            // Runtime queries
            entry.unlockedByLicense = licMgr.isFeatureUnlocked(m.id);
            entry.enabledByFlags = flagMgr.isEnabled(m.id);
            entry.enforcementAllowed = enfGate.checkAccess(m.id, "FeatureDisplay");
            entry.notes = "";

            result.push_back(entry);
        }

        return result;
    }

    // Get only features at a specific tier
    std::vector<FeatureDisplayEntry> getFeaturesForTier(LicenseTier tier) const {
        auto all = getAllFeatures();
        std::vector<FeatureDisplayEntry> result;
        for (const auto& f : all) {
            if (f.requiredTier == tier) result.push_back(f);
        }
        return result;
    }

    // Get only missing features
    std::vector<FeatureDisplayEntry> getMissingFeatures() const {
        auto all = getAllFeatures();
        std::vector<FeatureDisplayEntry> result;
        for (const auto& f : all) {
            if (!f.implemented) result.push_back(f);
        }
        return result;
    }

    // Get only unlocked features
    std::vector<FeatureDisplayEntry> getUnlockedFeatures() const {
        auto all = getAllFeatures();
        std::vector<FeatureDisplayEntry> result;
        for (const auto& f : all) {
            if (f.unlockedByLicense) result.push_back(f);
        }
        return result;
    }

    // Counts
    struct FeatureCounts {
        int total;
        int implemented;
        int wired;
        int tested;
        int unlocked;
        int locked;
        int phase1;
        int phase2;
        int phase3;
    };

    FeatureCounts getCounts() const {
        auto all = getAllFeatures();
        FeatureCounts c = {};
        c.total = static_cast<int>(all.size());
        for (const auto& f : all) {
            if (f.implemented) c.implemented++;
            if (f.wiredToBuild) c.wired++;
            if (f.tested) c.tested++;
            if (f.unlockedByLicense) c.unlocked++;
            else c.locked++;
            switch (f.phase) {
                case 1: c.phase1++; break;
                case 2: c.phase2++; break;
                case 3: c.phase3++; break;
            }
        }
        return c;
    }

    // ── Display Formatters ──

    // Compact status line for status bar / dashboard
    std::string getStatusLine() const {
        auto c = getCounts();
        std::ostringstream ss;
        ss << "Features: " << c.implemented << "/" << c.total << " impl | "
           << c.unlocked << "/" << c.total << " unlocked | "
           << "Tier: " << LicenseTierToString(
               EnterpriseLicenseManager::getInstance().getCurrentTier());
        return ss.str();
    }

    // Full feature table for display panels
    std::string generateDisplayTable() const {
        auto all = getAllFeatures();
        std::ostringstream tbl;

        tbl << std::left
            << std::setw(35) << "Feature"
            << std::setw(14) << "Tier"
            << std::setw(6) << "Impl"
            << std::setw(7) << "Wired"
            << std::setw(6) << "Test"
            << std::setw(4) << "Ph"
            << std::setw(10) << "License"
            << std::setw(8) << "Flags"
            << "\n";
        tbl << std::string(90, '-') << "\n";

        for (const auto& f : all) {
            tbl << std::left
                << std::setw(35) << f.name
                << std::setw(14) << LicenseTierToString(f.requiredTier)
                << std::setw(6) << (f.implemented ? "[Y]" : "[N]")
                << std::setw(7) << (f.wiredToBuild ? "[Y]" : "[N]")
                << std::setw(6) << (f.tested ? "[Y]" : "[N]")
                << std::setw(4) << f.phase
                << std::setw(10) << (f.unlockedByLicense ? "UNLOCKED" : "LOCKED")
                << std::setw(8) << (f.enabledByFlags ? "ON" : "OFF")
                << "\n";
        }

        return tbl.str();
    }

    // Per-tier summary for license upgrade prompts
    std::string generateTierSummary() const {
        std::ostringstream ss;
        const LicenseTier tiers[] = { LicenseTier::Community, LicenseTier::Professional,
                                       LicenseTier::Enterprise, LicenseTier::Sovereign };
        for (auto tier : tiers) {
            auto features = getFeaturesForTier(tier);
            int impl = 0;
            for (const auto& f : features) if (f.implemented) impl++;
            ss << LicenseTierToString(tier) << ": "
               << impl << "/" << features.size() << " implemented\n";
        }
        return ss.str();
    }

    ~EnterpriseFeatureDisplay() = default;
    EnterpriseFeatureDisplay(const EnterpriseFeatureDisplay&) = delete;
    EnterpriseFeatureDisplay& operator=(const EnterpriseFeatureDisplay&) = delete;

private:
    EnterpriseFeatureDisplay() = default;
};

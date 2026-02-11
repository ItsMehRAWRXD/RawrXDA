/**
 * @file tool_registry_thermal.cpp
 * @brief Thermal management tools for Sovereign NVMe Oracle integration
 *
 * Provides tools for:
 * - Reading real NVMe temperatures from Oracle service
 * - Drive selection based on thermal/wear scoring
 * - Blacklist management for overheating drives
 * - Thermal throttling control
 *
 * Integrates with Global\SOVEREIGN_NVME_TEMPS MMF published by NVMe Oracle Service
 */

#include "tool_registry.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>

using json = nlohmann::json;

// ============================================================================
// MMF Constants and Structures
// ============================================================================

constexpr uint32_t kSidecarSignature = 0x534F5645;  // "SOVE" as written by service
constexpr uint32_t kSidecarVersion = 1;
constexpr uint32_t kMaxDrives = 16;
constexpr const char* kMMFGlobalName = "Global\\SOVEREIGN_NVME_TEMPS";
constexpr const char* kMMFLocalName = "Local\\SOVEREIGN_NVME_TEMPS";

#pragma pack(push, 1)
struct SovereignThermalMMF {
    uint32_t signature;        // 0x534F5645 "SOVE"
    uint32_t version;          // 1
    uint32_t driveCount;       // Number of drives
    uint32_t reserved;         // Reserved for future use
    int32_t temps[kMaxDrives]; // Temperatures in Celsius (-1=unknown, <-1=error)
    int32_t wear[kMaxDrives];  // Wear level 0-100% (-1=unknown)
    uint64_t timestampMs;      // GetTickCount64() when last updated
};
#pragma pack(pop)

// ============================================================================
// Thermal Data Reader Class
// ============================================================================

class ThermalDataReader {
public:
    struct DriveData {
        int32_t driveId;
        int32_t tempC;
        int32_t wearPct;
        bool blacklisted;
        std::string blacklistReason;
        double score;
    };

    ~ThermalDataReader() {
        close();
    }

    bool open() {
        if (m_view) return true;  // Already open
        
        // Try Global namespace first (service runs as SYSTEM)
        m_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, kMMFGlobalName);
        if (!m_mapping) {
            // Fallback to Local namespace
            m_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, kMMFLocalName);
            if (!m_mapping) {
                m_lastError = "NVMe Oracle service not running (MMF not found)";
                return false;
            }
            m_usingLocal = true;
        }
        
        m_view = static_cast<SovereignThermalMMF*>(
            MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, sizeof(SovereignThermalMMF))
        );
        
        if (!m_view) {
            CloseHandle(m_mapping);
            m_mapping = nullptr;
            m_lastError = "Failed to map thermal MMF view";
            return false;
        }
        
        // Validate signature and version
        if (m_view->signature != kSidecarSignature) {
            close();
            m_lastError = "Invalid MMF signature";
            return false;
        }
        
        if (m_view->version != kSidecarVersion) {
            close();
            m_lastError = "Unsupported MMF version";
            return false;
        }
        
        return true;
    }

    void close() {
        if (m_view) {
            UnmapViewOfFile(m_view);
            m_view = nullptr;
        }
        if (m_mapping) {
            CloseHandle(m_mapping);
            m_mapping = nullptr;
        }
    }

    bool readAllDrives(std::vector<DriveData>& drives, int32_t maxTempC = 70, int32_t maxWearPct = 95) {
        if (!open()) return false;
        
        drives.clear();
        uint32_t count = std::min(m_view->driveCount, kMaxDrives);
        
        for (uint32_t i = 0; i < count; i++) {
            DriveData d;
            d.driveId = static_cast<int32_t>(i);
            d.tempC = m_view->temps[i];
            d.wearPct = m_view->wear[i];
            d.blacklisted = false;
            d.blacklistReason.clear();
            d.score = 0.0;
            
            // Blacklist logic
            if (d.tempC < -1) {
                // IOCTL failure (not NVMe)
                d.blacklisted = true;
                d.blacklistReason = "invalid";
                d.score = INFINITY;
            } else if (d.tempC >= maxTempC) {
                // Overheating
                d.blacklisted = true;
                d.blacklistReason = "overheat";
                d.score = INFINITY;
            } else if (d.wearPct >= 0 && d.wearPct > maxWearPct) {
                // End of life
                d.blacklisted = true;
                d.blacklistReason = "wear";
                d.score = INFINITY;
            } else {
                // Calculate weighted score (lower = better)
                double effectiveTemp = (d.tempC >= 0) ? d.tempC : 50.0;
                double effectiveWear = (d.wearPct >= 0) ? d.wearPct : 50.0;
                d.score = effectiveTemp * 0.7 + effectiveWear * 0.3;
            }
            
            drives.push_back(d);
        }
        
        // Sort by score (best drives first)
        std::sort(drives.begin(), drives.end(), 
            [](const DriveData& a, const DriveData& b) { return a.score < b.score; });
        
        return true;
    }

    int32_t getDriveTemp(int32_t driveId) {
        if (!open()) return -999;
        if (driveId < 0 || driveId >= static_cast<int32_t>(m_view->driveCount)) return -999;
        return m_view->temps[driveId];
    }

    int32_t getBestDrive(int32_t maxTempC = 70, int32_t maxWearPct = 95) {
        std::vector<DriveData> drives;
        if (!readAllDrives(drives, maxTempC, maxWearPct)) return -1;
        
        for (const auto& d : drives) {
            if (!d.blacklisted) return d.driveId;
        }
        return -1;  // All drives blacklisted
    }

    uint64_t getLastUpdateMs() {
        if (!open()) return 0;
        return m_view->timestampMs;
    }

    std::string getLastError() const { return m_lastError; }
    bool isConnected() const { return m_view != nullptr; }
    bool isUsingLocalNamespace() const { return m_usingLocal; }

private:
    HANDLE m_mapping = nullptr;
    SovereignThermalMMF* m_view = nullptr;
    std::string m_lastError;
    bool m_usingLocal = false;
};

// Global thermal reader instance
static ThermalDataReader g_thermalReader;

// ============================================================================
// Thermal Tools Registration
// ============================================================================

int registerThermalTools(ToolRegistry* registry) {
    int count = 0;

    // ═══════════════════════════════════════════════════════════════════════
    // thermalStatus - Get current thermal status of all drives
    // ═══════════════════════════════════════════════════════════════════════
    {
        ToolDefinition toolDef;
        toolDef.name = "thermalStatus";
        toolDef.description = "Get real-time NVMe thermal status from Oracle service";
        toolDef.category = ToolCategory::Custom;
        toolDef.experimental = false;
        
        toolDef.config.toolName = "thermalStatus";
        toolDef.config.timeoutMs = 1000;
        toolDef.config.enableCaching = true;
        toolDef.config.cacheValidityMs = 500;  // 500ms cache for thermal data
        
        toolDef.inputSchema = json{
            {"type", "object"},
            {"properties", {
                {"maxTempC", {{"type", "integer"}, {"default", 70}, {"description", "Blacklist threshold temperature"}}},
                {"maxWearPct", {{"type", "integer"}, {"default", 95}, {"description", "Blacklist threshold wear level"}}}
            }}
        };

        toolDef.handler = [](const json& params) -> json {
            int32_t maxTempC = params.value("maxTempC", 70);
            int32_t maxWearPct = params.value("maxWearPct", 95);
            
            std::vector<ThermalDataReader::DriveData> drives;
            if (!g_thermalReader.readAllDrives(drives, maxTempC, maxWearPct)) {
                return json{
                    {"success", false},
                    {"error", g_thermalReader.getLastError()},
                    {"serviceRunning", false}
                };
            }
            
            json drivesJson = json::array_type();
            int32_t blacklistedCount = 0;
            int32_t validCount = 0;
            int32_t hottestTemp = -999;
            int32_t coolestTemp = 999;
            int32_t coolestDrive = -1;
            
            for (const auto& d : drives) {
                json driveJson = {
                    {"driveId", d.driveId},
                    {"tempC", d.tempC},
                    {"wearPct", d.wearPct},
                    {"score", std::isinf(d.score) ? -1.0 : d.score},
                    {"blacklisted", d.blacklisted}
                };
                if (d.blacklisted) {
                    driveJson["blacklistReason"] = d.blacklistReason;
                    blacklistedCount++;
                } else {
                    validCount++;
                    if (d.tempC >= 0) {
                        if (d.tempC > hottestTemp) hottestTemp = d.tempC;
                        if (d.tempC < coolestTemp) {
                            coolestTemp = d.tempC;
                            coolestDrive = d.driveId;
                        }
                    }
                }
                drivesJson.push_back(driveJson);
            }
            
            return json{
                {"success", true},
                {"serviceRunning", true},
                {"namespace", g_thermalReader.isUsingLocalNamespace() ? "Local" : "Global"},
                {"lastUpdateMs", g_thermalReader.getLastUpdateMs()},
                {"totalDrives", static_cast<int>(drives.size())},
                {"validDrives", validCount},
                {"blacklistedDrives", blacklistedCount},
                {"hottestTempC", hottestTemp > -999 ? hottestTemp : -1},
                {"coolestTempC", coolestTemp < 999 ? coolestTemp : -1},
                {"coolestDrive", coolestDrive},
                {"drives", drivesJson}
            };
        };

        if (registry->registerTool(toolDef)) count++;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // selectCoolestDrive - Get the coolest available NVMe drive
    // ═══════════════════════════════════════════════════════════════════════
    {
        ToolDefinition toolDef;
        toolDef.name = "selectCoolestDrive";
        toolDef.description = "Select the coolest non-blacklisted NVMe drive for I/O operations";
        toolDef.category = ToolCategory::Custom;
        toolDef.experimental = false;
        
        toolDef.config.toolName = "selectCoolestDrive";
        toolDef.config.timeoutMs = 500;
        toolDef.config.enableCaching = true;
        toolDef.config.cacheValidityMs = 1000;
        
        toolDef.inputSchema = json{
            {"type", "object"},
            {"properties", {
                {"maxTempC", {{"type", "integer"}, {"default", 70}}},
                {"maxWearPct", {{"type", "integer"}, {"default", 95}}}
            }}
        };

        toolDef.handler = [](const json& params) -> json {
            int32_t maxTempC = params.value("maxTempC", 70);
            int32_t maxWearPct = params.value("maxWearPct", 95);
            
            int32_t bestDrive = g_thermalReader.getBestDrive(maxTempC, maxWearPct);
            
            if (bestDrive < 0) {
                return json{
                    {"success", false},
                    {"error", "No valid drives available (all blacklisted or service not running)"},
                    {"driveId", -1}
                };
            }
            
            int32_t temp = g_thermalReader.getDriveTemp(bestDrive);
            
            return json{
                {"success", true},
                {"driveId", bestDrive},
                {"tempC", temp},
                {"drivePath", "\\\\.\\PhysicalDrive" + std::to_string(bestDrive)}
            };
        };

        if (registry->registerTool(toolDef)) count++;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // getDriveTemperature - Get temperature of a specific drive
    // ═══════════════════════════════════════════════════════════════════════
    {
        ToolDefinition toolDef;
        toolDef.name = "getDriveTemperature";
        toolDef.description = "Get real-time temperature of a specific NVMe drive";
        toolDef.category = ToolCategory::Custom;
        toolDef.experimental = false;
        
        toolDef.config.toolName = "getDriveTemperature";
        toolDef.config.timeoutMs = 500;
        
        toolDef.inputSchema = json{
            {"type", "object"},
            {"properties", {
                {"driveId", {{"type", "integer"}, {"description", "Physical drive ID (0, 1, 2, ...)"}}}
            }},
            {"required", {"driveId"}}
        };

        toolDef.handler = [](const json& params) -> json {
            if (!params.contains("driveId")) {
                return json{{"success", false}, {"error", "driveId is required"}};
            }
            
            int32_t driveId = params["driveId"];
            int32_t temp = g_thermalReader.getDriveTemp(driveId);
            
            if (temp == -999) {
                return json{
                    {"success", false},
                    {"error", g_thermalReader.getLastError()},
                    {"driveId", driveId}
                };
            }
            
            std::string status;
            if (temp < -1) {
                status = "invalid";
            } else if (temp == -1) {
                status = "unknown";
            } else if (temp >= 70) {
                status = "critical";
            } else if (temp >= 55) {
                status = "warm";
            } else {
                status = "normal";
            }
            
            return json{
                {"success", true},
                {"driveId", driveId},
                {"tempC", temp},
                {"status", status}
            };
        };

        if (registry->registerTool(toolDef)) count++;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // checkThermalHeadroom - Check if system has thermal headroom for heavy I/O
    // ═══════════════════════════════════════════════════════════════════════
    {
        ToolDefinition toolDef;
        toolDef.name = "checkThermalHeadroom";
        toolDef.description = "Check if NVMe thermal conditions allow heavy I/O operations";
        toolDef.category = ToolCategory::Custom;
        toolDef.experimental = false;
        
        toolDef.config.toolName = "checkThermalHeadroom";
        toolDef.config.timeoutMs = 500;
        
        toolDef.inputSchema = json{
            {"type", "object"},
            {"properties", {
                {"throttleThreshold", {{"type", "integer"}, {"default", 65}, {"description", "Temperature to start throttling"}}}
            }}
        };

        toolDef.handler = [](const json& params) -> json {
            int32_t throttleThreshold = params.value("throttleThreshold", 65);
            
            std::vector<ThermalDataReader::DriveData> drives;
            if (!g_thermalReader.readAllDrives(drives)) {
                return json{
                    {"success", false},
                    {"error", g_thermalReader.getLastError()},
                    {"allowHeavyIO", false}
                };
            }
            
            int32_t maxTemp = -999;
            int32_t drivesNearThrottle = 0;
            
            for (const auto& d : drives) {
                if (!d.blacklisted && d.tempC >= 0) {
                    if (d.tempC > maxTemp) maxTemp = d.tempC;
                    if (d.tempC >= throttleThreshold) drivesNearThrottle++;
                }
            }
            
            bool allowHeavyIO = (maxTemp >= 0 && maxTemp < throttleThreshold);
            int32_t headroomC = throttleThreshold - maxTemp;
            
            return json{
                {"success", true},
                {"allowHeavyIO", allowHeavyIO},
                {"maxTempC", maxTemp},
                {"throttleThreshold", throttleThreshold},
                {"headroomC", headroomC > 0 ? headroomC : 0},
                {"drivesNearThrottle", drivesNearThrottle},
                {"recommendation", allowHeavyIO ? "proceed" : "throttle_or_wait"}
            };
        };

        if (registry->registerTool(toolDef)) count++;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // rankDrivesForIO - Get ranked list of drives for I/O load balancing
    // ═══════════════════════════════════════════════════════════════════════
    {
        ToolDefinition toolDef;
        toolDef.name = "rankDrivesForIO";
        toolDef.description = "Get drives ranked by thermal/wear score for load balancing";
        toolDef.category = ToolCategory::Custom;
        toolDef.experimental = false;
        
        toolDef.config.toolName = "rankDrivesForIO";
        toolDef.config.timeoutMs = 500;
        
        toolDef.inputSchema = json{
            {"type", "object"},
            {"properties", {
                {"excludeBlacklisted", {{"type", "boolean"}, {"default", true}}},
                {"maxTempC", {{"type", "integer"}, {"default", 70}}},
                {"maxWearPct", {{"type", "integer"}, {"default", 95}}}
            }}
        };

        toolDef.handler = [](const json& params) -> json {
            bool excludeBlacklisted = params.value("excludeBlacklisted", true);
            int32_t maxTempC = params.value("maxTempC", 70);
            int32_t maxWearPct = params.value("maxWearPct", 95);
            
            std::vector<ThermalDataReader::DriveData> drives;
            if (!g_thermalReader.readAllDrives(drives, maxTempC, maxWearPct)) {
                return json{
                    {"success", false},
                    {"error", g_thermalReader.getLastError()}
                };
            }
            
            json rankedJson = json::array();
            int rank = 0;
            
            for (const auto& d : drives) {
                if (excludeBlacklisted && d.blacklisted) continue;
                
                rankedJson.push_back({
                    {"rank", rank++},
                    {"driveId", d.driveId},
                    {"tempC", d.tempC},
                    {"wearPct", d.wearPct},
                    {"score", std::isinf(d.score) ? -1.0 : d.score},
                    {"blacklisted", d.blacklisted},
                    {"drivePath", "\\\\.\\PhysicalDrive" + std::to_string(d.driveId)}
                });
            }
            
            return json{
                {"success", true},
                {"rankedDrives", rankedJson},
                {"availableCount", rank}
            };
        };

        if (registry->registerTool(toolDef)) count++;
    }

    return count;
}

// ============================================================================
// Tool Registration Entry Point
// ============================================================================

/**
 * @brief Register all thermal tools with the registry
 * @param registry The tool registry to register with
 * @return Number of tools registered
 *
 * Call this from tool_registry_init.cpp's initializeAllTools()
 */
extern "C" int initializeThermalTools(ToolRegistry* registry) {
    if (!registry) return 0;
    return registerThermalTools(registry);
}

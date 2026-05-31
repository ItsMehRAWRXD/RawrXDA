#include "gguf_tensor_loader.h"

#include "../logging/Logger.h"

#include <algorithm>

namespace RawrXD
{
namespace
{

static void logDecision_(bool enabled, const GGufTensorLoader::LoadPlanItem& it)
{
    if (!enabled)
        return;
    using RawrXD::Logging::Logger;
    Logger::instance().info("GGUF tensor plan: " + it.tensorName + " zone=" + it.zoneName +
                                " bytes=" + std::to_string(it.bytes) + " layer=" + std::to_string(it.layerIdx) +
                                " decision=" + std::to_string((int)it.decision) +
                                " ztype=" + std::to_string((int)it.zoneType) + " prio=" + std::to_string(it.priority) +
                                " zoneBytes=" + std::to_string(it.zoneTotalBytes),
                            "GGufTensorLoader");
}

}  // namespace

GGufTensorLoader::GGufTensorLoader(StreamingGGUFLoader& loader, Config config)
    : m_loader(loader), m_cfg(std::move(config))
{
}

int32_t GGufTensorLoader::extractLayerIdx_(const std::string& tensorName)
{
    // Canonical layer tensor prefix in this repo: "blk.<n>."
    const std::string needle = "blk.";
    const auto pos = tensorName.find(needle);
    if (pos == std::string::npos)
        return -1;
    const auto start = pos + needle.size();
    size_t end = start;
    while (end < tensorName.size() && tensorName[end] >= '0' && tensorName[end] <= '9')
        ++end;
    if (end == start)
        return -1;
    try
    {
        return std::stoi(tensorName.substr(start, end - start));
    }
    catch (...)
    {
        return -1;
    }
}

RawrXD::Expected<std::vector<GGufTensorLoader::LoadPlanItem>, GGufTensorLoadError> GGufTensorLoader::buildPlan()
{
    std::vector<LoadPlanItem> out;
    m_plan.clear();
    m_zoneTotalBytes.clear();

    const std::vector<TensorRef> idx = m_loader.GetTensorIndex();
    out.reserve(idx.size());

    const std::vector<ZoneClassRule>& rules = m_cfg.zoneRules.empty() ? getDefaultZoneRules() : m_cfg.zoneRules;

    // First pass: compute plan items + accumulate zone sizes.
    for (const auto& t : idx)
    {
        LoadPlanItem it;
        it.tensorName = t.name;
        it.zoneName = !t.zone_name.empty() ? t.zone_name : m_loader.GetTensorZone(t.name);
        it.layerIdx = extractLayerIdx_(t.name);
        it.bytes = t.size;

        uint8_t prio = 255;
        LoadDecision ruleDecision = m_cfg.filter.defaultDecision;
        it.zoneType = classifyTensor(it.tensorName, rules, prio, ruleDecision);
        it.priority = prio;

        // Start with classification default, but still allow allow/deny + layer range gates.
        // If filter gates say SKIP, it wins. Otherwise keep the rule decision.
        const LoadDecision gated = m_cfg.filter.decide(it.tensorName, it.zoneName, it.bytes, it.layerIdx);
        it.decision = (gated == LoadDecision::SKIP) ? LoadDecision::SKIP : ruleDecision;
        logDecision_(m_cfg.logDecisions, it);

        out.push_back(it);
        m_plan[it.tensorName] = it;
        m_zoneTotalBytes[it.zoneName] += it.bytes;
    }

    // Invariant: each tensor must have exactly one consistent zone assignment.
    // `TensorRef::zone_name` (if present) must match `GetTensorZone()` (if non-empty).
    for (const auto& t : idx)
    {
        const std::string zRef = t.zone_name;
        const std::string zFn = m_loader.GetTensorZone(t.name);

        if (!zRef.empty() && !zFn.empty() && zRef != zFn)
        {
            return RawrXD::unexpected(
                GGufTensorLoadError{GGufTensorLoadErrorCode::ZoneConflict,
                                    "Tensor '" + t.name + "' has conflicting zone assignments: TensorRef.zone_name='" +
                                        zRef + "' vs GetTensorZone()='" + zFn + "'"});
        }
        if (zRef.empty() && zFn.empty())
        {
            return RawrXD::unexpected(GGufTensorLoadError{GGufTensorLoadErrorCode::ZoneConflict,
                                                          "Tensor '" + t.name + "' has no zone assignment"});
        }
    }

    // Second pass: stamp zoneTotalBytes into each plan item.
    for (auto& it : out)
    {
        const auto zIt = m_zoneTotalBytes.find(it.zoneName);
        it.zoneTotalBytes = (zIt != m_zoneTotalBytes.end()) ? zIt->second : 0;
        auto pIt = m_plan.find(it.tensorName);
        if (pIt != m_plan.end())
            pIt->second.zoneTotalBytes = it.zoneTotalBytes;
    }

    return RawrXD::Expected<std::vector<LoadPlanItem>, GGufTensorLoadError>(out);
}

RawrXD::Expected<GGufTensorLoader::PreloadResult, GGufTensorLoadError> GGufTensorLoader::executePreload()
{
    PreloadResult r;

    if (m_plan.empty())
    {
        auto plan = buildPlan();
        if (!plan)
            return RawrXD::unexpected(plan.error());
    }

    // Build unique zone list for tensors whose decision is LOAD.
    struct ZoneLoadKey
    {
        std::string zone;
        uint8_t priority = 255;
        int32_t minLayer = 0x7fffffff;
    };

    std::unordered_map<std::string, ZoneLoadKey> zones;
    for (const auto& kv : m_plan)
    {
        const auto& it = kv.second;
        if (it.decision != LoadDecision::LOAD)
        {
            if (it.decision == LoadDecision::LAZY_LOAD)
                r.bytesLazy += it.bytes;
            continue;
        }

        auto& z = zones[it.zoneName];
        z.zone = it.zoneName;
        z.priority = std::min<uint8_t>(z.priority, it.priority);
        if (it.layerIdx >= 0)
            z.minLayer = std::min<int32_t>(z.minLayer, it.layerIdx);
    }

    std::vector<ZoneLoadKey> order;
    order.reserve(zones.size());
    for (const auto& kv : zones)
        order.push_back(kv.second);

    std::sort(order.begin(), order.end(),
              [](const ZoneLoadKey& a, const ZoneLoadKey& b)
              {
                  if (a.priority != b.priority)
                      return a.priority < b.priority;
                  return a.minLayer < b.minLayer;
              });

    // Budget-aware decisioning / graceful demotion.
    auto zoneBytes_ = [&](const std::string& zoneName) -> uint64_t
    {
        auto it = m_zoneTotalBytes.find(zoneName);
        if (it != m_zoneTotalBytes.end())
            return it->second;
        return m_loader.GetZoneInfo(zoneName).total_bytes;
    };

    auto demoteZone_ = [&](const std::string& zoneName)
    {
        for (auto& kv : m_plan)
        {
            auto& item = kv.second;
            if (item.zoneName == zoneName && item.decision == LoadDecision::LOAD)
            {
                item.decision = LoadDecision::LAZY_LOAD;
                r.demotedTensors.push_back(item.tensorName);
            }
        }
    };

    if (m_cfg.budget.singleZoneResident)
    {
        // Keep only the highest-priority zone (first in sorted order).
        for (size_t i = 1; i < order.size(); ++i)
            demoteZone_(order[i].zone);
    }
    else
    {
        uint64_t totalLoadBytes = 0;
        for (const auto& z : order)
            totalLoadBytes += zoneBytes_(z.zone);

        if (totalLoadBytes > m_cfg.budget.maxResidentBytes)
        {
            // Demote from the back to keep higher-priority zones.
            for (size_t i = order.size(); i-- > 0 && totalLoadBytes > m_cfg.budget.maxResidentBytes;)
            {
                const uint64_t zb = zoneBytes_(order[i].zone);
                demoteZone_(order[i].zone);
                totalLoadBytes = (zb <= totalLoadBytes) ? (totalLoadBytes - zb) : 0;
            }
        }
    }

    for (const auto& z : order)
    {
        // Skip zones that were fully demoted (no remaining LOAD tensors in that zone).
        bool stillLoad = false;
        for (const auto& kv : m_plan)
        {
            const auto& it = kv.second;
            if (it.zoneName == z.zone && it.decision == LoadDecision::LOAD)
            {
                stillLoad = true;
                break;
            }
        }
        if (!stillLoad)
            continue;

        const uint64_t zBytes = zoneBytes_(z.zone);

        auto ok = ensureZoneResident_(z.zone, zBytes);
        if (!ok)
        {
            r.zonesSkipped++;
            r.failedZones.push_back(z.zone + ": " + ok.error().message);
            continue;
        }

        r.zonesLoaded++;
        r.bytesResident = currentResidentBytes_();
    }

    return RawrXD::Expected<PreloadResult, GGufTensorLoadError>(r);
}

RawrXD::Expected<GGufTensorLoader::LoadPlanItem, GGufTensorLoadError> GGufTensorLoader::getPlanFor_(
    const std::string& tensorName)
{
    auto it = m_plan.find(tensorName);
    if (it != m_plan.end())
        return it->second;

    // Build a minimal on-demand plan entry if buildPlan() wasn't called.
    std::vector<TensorRef> idx = m_loader.GetTensorIndex();
    const auto found = std::find_if(idx.begin(), idx.end(), [&](const TensorRef& r) { return r.name == tensorName; });
    if (found == idx.end())
    {
        return RawrXD::unexpected(
            GGufTensorLoadError{GGufTensorLoadErrorCode::TensorNotFound, "tensor not found: " + tensorName});
    }

    LoadPlanItem p;
    p.tensorName = tensorName;
    p.zoneName = !found->zone_name.empty() ? found->zone_name : m_loader.GetTensorZone(tensorName);
    p.layerIdx = extractLayerIdx_(tensorName);
    p.bytes = found->size;
    p.decision = m_cfg.filter.decide(p.tensorName, p.zoneName, p.bytes, p.layerIdx);
    m_plan[tensorName] = p;
    return p;
}

uint64_t GGufTensorLoader::currentResidentBytes_() const
{
    uint64_t sum = 0;
    for (const auto& kv : m_residentZonesBytes)
        sum += kv.second;
    return sum;
}

RawrXD::Expected<void, GGufTensorLoadError> GGufTensorLoader::ensureZoneResident_(const std::string& zoneName,
                                                                                  uint64_t zoneBytes)
{
    // Respect strong bunny-hop: keep only a single zone resident if requested.
    if (m_cfg.budget.singleZoneResident)
    {
        for (const auto& kv : m_residentZonesBytes)
        {
            if (kv.first == zoneName)
                continue;
            (void)m_loader.UnloadZone(kv.first);
        }
        m_residentZonesBytes.clear();
    }

    // Evict until budget can fit.
    while (currentResidentBytes_() + zoneBytes > m_cfg.budget.maxResidentBytes)
    {
        if (m_residentZonesBytes.empty())
        {
            return RawrXD::unexpected(
                GGufTensorLoadError{GGufTensorLoadErrorCode::BudgetExceeded,
                                    "budget exceeded: zone=" + zoneName + " bytes=" + std::to_string(zoneBytes) +
                                        " maxResidentBytes=" + std::to_string(m_cfg.budget.maxResidentBytes)});
        }

        // Naive eviction: unload the first zone (streaming loader is itself zone-sized).
        auto victim = m_residentZonesBytes.begin();
        (void)m_loader.UnloadZone(victim->first);
        m_residentZonesBytes.erase(victim);
    }

    // If already resident, nothing to do.
    if (m_residentZonesBytes.find(zoneName) != m_residentZonesBytes.end())
        return {};

    const uint64_t maxZoneMb =
        std::max<uint64_t>(1, (m_cfg.budget.maxZoneBytes + (1024ull * 1024ull - 1)) / (1024ull * 1024ull));
    if (!m_loader.LoadZone(zoneName, maxZoneMb))
    {
        return RawrXD::unexpected(
            GGufTensorLoadError{GGufTensorLoadErrorCode::LoaderError, "LoadZone failed: " + zoneName});
    }

    m_residentZonesBytes[zoneName] = zoneBytes;
    return {};
}

RawrXD::Expected<void, GGufTensorLoadError> GGufTensorLoader::ensureTensorAvailable(const std::string& tensorName)
{
    auto p = getPlanFor_(tensorName);
    if (!p)
        return RawrXD::unexpected(p.error());

    if (p->decision == LoadDecision::SKIP)
    {
        return RawrXD::unexpected(
            GGufTensorLoadError{GGufTensorLoadErrorCode::FilteredOut, "filtered out: " + tensorName});
    }

    // LAZY_LOAD == allow on-demand; LOAD == allow eager. Both require residency for GetTensorData to be fast.
    // Zone size is taken from StreamingGGUFLoader's zone metadata.
    const auto zoneInfo = m_loader.GetZoneInfo(p->zoneName);
    const uint64_t zoneBytes = zoneInfo.total_bytes;
    return ensureZoneResident_(p->zoneName, zoneBytes);
}

RawrXD::Expected<std::vector<uint8_t>, GGufTensorLoadError> GGufTensorLoader::loadTensorBytes(
    const std::string& tensorName)
{
    auto ok = ensureTensorAvailable(tensorName);
    if (!ok)
        return RawrXD::unexpected(ok.error());

    std::vector<uint8_t> data;
    if (!m_loader.GetTensorData(tensorName, data))
    {
        return RawrXD::unexpected(
            GGufTensorLoadError{GGufTensorLoadErrorCode::LoaderError, "GetTensorData failed: " + tensorName});
    }
    return RawrXD::Expected<std::vector<uint8_t>, GGufTensorLoadError>(data);
}

}  // namespace RawrXD

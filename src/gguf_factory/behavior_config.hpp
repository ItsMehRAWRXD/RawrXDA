#pragma once
#include <string>
#include <unordered_map>
namespace RawrXD
{
// Minimal, serializable behavior config base.
// This is intentionally "data only": capabilities are implemented elsewhere.
struct BehaviorConfig
{
    virtual ~BehaviorConfig() = default;

    virtual std::unordered_map<std::string, std::string> to_kv_pairs() const { return {}; }

    // Base validate: always ok.
    virtual bool validate(std::string* error = nullptr) const
    {
        if (error)
            *error = "";
        return true;
    }
};
}  // namespace RawrXD

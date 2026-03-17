#include "feature_toggle.h"
#include "config_manager.h"

namespace RawrXD {

bool FeatureToggle::isEnabled(const QString& name, bool defaultValue) {
    const auto& cfg = ConfigManager::instance();
    const QJsonObject features = cfg.section("features");
    if (features.contains(name) && features.value(name).isBool()) {
        return features.value(name).toBool();
    }
    return defaultValue;
}

} // namespace RawrXD

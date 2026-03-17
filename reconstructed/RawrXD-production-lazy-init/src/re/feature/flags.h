#pragma once
#include <QString>

class REFeatureFlags {
public:
    static bool isREEnabled(); // Gated by RAWRXD_RE_ENABLED (default OFF)
};

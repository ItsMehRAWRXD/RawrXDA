#include "telemetry_singleton.h"
#include <mutex>

Telemetry& GetTelemetry() {
    static Telemetry* instance = nullptr;
    static std::once_flag once;
    std::call_once(once, [](){
        instance = new Telemetry();
    });
    return *instance;
}

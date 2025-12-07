#include <string>
namespace RawrXD { namespace Telemetry {
    enum ExportFormat { JSON = 0, CSV = 1 };
    struct AIMetricsCollector {
        struct DisplayMetrics { unsigned long long requests=0; unsigned long long tokens=0; unsigned long long tools=0; };
        void recordOllamaRequest(const std::string&, unsigned long long, bool, unsigned long long, unsigned long long) {}
        void recordToolInvocation(const std::string&, unsigned long long, bool) {}
        std::string exportMetrics(ExportFormat) const { return std::string(); }
        bool saveMetricsToFile(const std::string&, ExportFormat) const { return false; }
        void resetMetrics() {}
        DisplayMetrics getDisplayMetrics() const { return DisplayMetrics{}; }
    };
    static AIMetricsCollector g;
    AIMetricsCollector& GetMetricsCollector() { return g; }
} }

#include <string>
#include <atomic>
#include <mutex>
#include <fstream>

namespace RawrXD { namespace Telemetry {
    enum ExportFormat { JSON = 0, CSV = 1 };
    
    struct AIMetricsCollector {
        struct DisplayMetrics { unsigned long long requests=0; unsigned long long tokens=0; unsigned long long tools=0; };
        
        // Real storage
        std::atomic<unsigned long long> totalRequests{0};
        std::atomic<unsigned long long> totalTokens{0};
        std::atomic<unsigned long long> totalTools{0};
        
        void recordOllamaRequest(const std::string& model, unsigned long long latency, bool success, unsigned long long promptTokens, unsigned long long completionTokens) {
            totalRequests++;
            totalTokens += (promptTokens + completionTokens);
        }
        
        void recordToolInvocation(const std::string& tool, unsigned long long latency, bool success) {
            totalTools++;
        }
        
        std::string exportMetrics(ExportFormat fmt) const { 
            // Simple export
            if (fmt == JSON) return "{ \"requests\": " + std::to_string(totalRequests) + ", \"tokens\": " + std::to_string(totalTokens) + " }";
            return "requests,tokens\n" + std::to_string(totalRequests) + "," + std::to_string(totalTokens);
        }
        
        bool saveMetricsToFile(const std::string& path, ExportFormat fmt) const { 
            std::ofstream out(path, std::ios::app);
            if (!out.is_open()) return false;
            out << exportMetrics(fmt) << "\n";
            return true;
        }
        
        void resetMetrics() {
            totalRequests = 0;
            totalTokens = 0;
            totalTools = 0;
        }
        
        DisplayMetrics getDisplayMetrics() const { 
            return DisplayMetrics{ totalRequests.load(), totalTokens.load(), totalTools.load() }; 
        }
    };
    static AIMetricsCollector g;
    AIMetricsCollector& GetMetricsCollector() { return g; }
} }

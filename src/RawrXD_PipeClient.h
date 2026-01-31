#ifndef RAWRXD_PIPE_CLIENT_H
#define RAWRXD_PIPE_CLIENT_H

#include <string>
#include <vector>
#include <stdexcept>
#include <windows.h>

namespace RawrXD {
    struct PatternResult {
        std::string Pattern; // BUG, FIXME, etc.
        double Confidence;
        int Line;
        int Priority;
    };

    class PipeClient {
    public:
        PipeClient(const std::string& pipeName);
        ~PipeClient();
        
        bool // Connect removed;
        void Disconnect();
        
        // Main classification method
        PatternResult Classify(const std::string& text);
        
        // Check health
        bool Ping();

    private:
        std::string pipeName;
        HANDLE pipeHandle;
        
        // Helpers
        void SendCommand(const std::string& cmd, const std::string& data = "");
        std::string ReadResponse();
        PatternResult ParseJSON(const std::string& json);
    };
}

#endif // RAWRXD_PIPE_CLIENT_H


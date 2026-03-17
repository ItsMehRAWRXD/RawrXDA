#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace RawrXD {

struct AICompletion {
    std::string text;
};

class AIIntegrationHub {
public:
    virtual ~AIIntegrationHub() = default;
    
    // Virtual interface to allow mocking or different backends
    virtual std::vector<AICompletion> getCompletions(
        const std::string& bufferName,
        const std::string& prefix,
        const std::string& suffix,
        int cursorOffset
    ) {
        // Default implementation: Connect to RawrXD-Agent.exe via Named Pipe
        
        // Formulate prompt (FIM format)
        std::string prompt = "<PRE>" + prefix + " <SUF>" + suffix + " <MID>";
        
        // Connect to Pipe
        HANDLE hPipe = CreateFileW(
            L"\\\\.\\pipe\\RawrXD_IPC",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hPipe == INVALID_HANDLE_VALUE) {
            // Inference engine not running or busy
            return {}; 
        }
        
        // Set Pipe Mode to Message
        DWORD dwMode = PIPE_READMODE_MESSAGE; 
        BOOL fSuccess = SetNamedPipeHandleState( 
          hPipe,    // pipe handle 
          &dwMode,  // new pipe mode 
          NULL,     // don't set maximum bytes 
          NULL);    // don't set maximum time 
          
        if (!fSuccess) {
             CloseHandle(hPipe);
             return {};
        }
        
        // Write Prompt
        DWORD dwWritten;
        fSuccess = WriteFile(
            hPipe,
            prompt.c_str(),
            (DWORD)prompt.length(),
            &dwWritten,
            NULL
        );
        
        if (!fSuccess) {
            CloseHandle(hPipe);
            return {};
        }
        
        // Read Response
        std::string response;
        char buffer[4096];
        DWORD dwRead;
        
        fSuccess = ReadFile(
            hPipe,
            buffer,
            4096,
            &dwRead,
            NULL
        );
        
        if (fSuccess && dwRead > 0) {
            response.append(buffer, dwRead);
        }
        
        CloseHandle(hPipe);
        
        if (!response.empty()) {
            return {{response}};
        }
        
        return {};
    }
};

}

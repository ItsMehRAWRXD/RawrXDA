// Simple CLI backend for RawrXD IDE - Right pane
// Executes basic commands and prints results
#include <windows.h>
#include <cstdio>
#include <string>
#include <cstring>

int main() {
    printf("RawrXD CLI v1.0 - Type 'help' for commands\r\n");
    printf("cli> ");
    fflush(stdout);
    
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), stdin) != nullptr) {
        // Remove newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
        if (len > 1 && buffer[len-2] == '\r') buffer[len-2] = '\0';
        
        std::string cmd(buffer);
        
        if (cmd == "exit" || cmd == "quit") {
            printf("Exiting CLI...\r\n");
            break;
        }
        else if (cmd == "help") {
            printf("Commands:\r\n");
            printf("  help              Show this help\r\n");
            printf("  echo TEXT         Echo text\r\n");
            printf("  version           Show version\r\n");
            printf("  time              Show current time\r\n");
            printf("  exit              Exit CLI\r\n");
        }
        else if (cmd.substr(0, 4) == "echo") {
            printf("%s\r\n", cmd.substr(5).c_str());
        }
        else if (cmd == "version") {
            printf("RawrXD CLI v1.0\r\n");
        }
        else if (cmd == "time") {
            SYSTEMTIME st;
            GetLocalTime(&st);
            printf("%04d-%02d-%02d %02d:%02d:%02d\r\n", st.wYear, st.wMonth, st.wDay, 
                   st.wHour, st.wMinute, st.wSecond);
        }
        else if (!cmd.empty()) {
            printf("[CLIerror: Unknown command: %s\r\n", cmd.c_str());
        }
        
        printf("cli> ");
        fflush(stdout);
    }
    
    return 0;
}

/*
 * RawrXD_TestBeacon.exe — Test client for the RawrXD IDE Test Harness Backend
 * Connects to \\.\pipe\RawrXD_TestBeacon, sends commands, prints results.
 *
 * Usage:
 *   RawrXD_TestBeacon.exe                       # Interactive mode
 *   RawrXD_TestBeacon.exe PING                  # Single command
 *   RawrXD_TestBeacon.exe -script test.txt      # Run script file
 *   echo "PING" | RawrXD_TestBeacon.exe -stdin  # Pipe mode
 *
 * Commands:
 *   PING                    Heartbeat check
 *   HWND_MAP                Dump all HWND handles
 *   TREE_COUNT              Get file tree item count
 *   TREE_ITEMS              Get full file tree listing
 *   CMD <id>                Send WM_COMMAND with menu ID
 *   TITLE                   Get window title
 *   EDITOR_TEXT             Get editor content
 *   OUTPUT_TEXT             Get output panel content
 *   OPEN_FOLDER <path>      Open a folder in the file tree
 *   LOAD_FILE <path>        Load a file into the editor
 *   VISIBILITY              Get panel visibility states
 *   WORKSPACE               Get current workspace root
 *   QUIT                    Disconnect
 *
 * Build: g++ -std=c++17 -o RawrXD_TestBeacon.exe RawrXD_TestBeacon.cpp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

#define PIPE_NAME L"\\\\.\\pipe\\RawrXD_TestBeacon"
#define TIMEOUT_MS 10000

static HANDLE g_hPipe = INVALID_HANDLE_VALUE;

bool Connect() {
    for (int attempt = 0; attempt < 5; attempt++) {
        g_hPipe = CreateFileW(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, 0, nullptr);
        
        if (g_hPipe != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(g_hPipe, &mode, nullptr, nullptr);
            return true;
        }
        
        if (GetLastError() == ERROR_PIPE_BUSY) {
            if (!WaitNamedPipeW(PIPE_NAME, TIMEOUT_MS)) {
                fprintf(stderr, "[beacon] Pipe busy, attempt %d/5\n", attempt + 1);
                continue;
            }
        } else {
            fprintf(stderr, "[beacon] Pipe not found (is IDE running?), attempt %d/5\n", attempt + 1);
            Sleep(1000);
        }
    }
    return false;
}

std::string SendCommand(const std::string& cmd) {
    DWORD bytesWritten;
    if (!WriteFile(g_hPipe, cmd.c_str(), (DWORD)cmd.size(), &bytesWritten, nullptr)) {
        return "ERROR=write failed (" + std::to_string(GetLastError()) + ")\n";
    }

    std::string result;
    char buf[65536];
    DWORD bytesRead;
    while (true) {
        BOOL ok = ReadFile(g_hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        if (ok) {
            buf[bytesRead] = 0;
            result += buf;
            break;
        }
        DWORD err = GetLastError();
        if (err == ERROR_MORE_DATA) {
            buf[bytesRead] = 0;
            result += buf;
            continue;
        }
        return "ERROR=read failed (" + std::to_string(err) + ")\n";
    }
    return result;
}

void RunInteractive() {
    printf("RawrXD Test Beacon — Interactive Mode\n");
    printf("Type commands (PING, HWND_MAP, TREE_COUNT, etc.) or QUIT to exit.\n\n");
    
    char line[4096];
    while (true) {
        printf("beacon> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        
        // Trim
        std::string cmd(line);
        while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) cmd.pop_back();
        if (cmd.empty()) continue;
        
        if (cmd == "QUIT" || cmd == "EXIT") {
            SendCommand("QUIT");
            printf("Disconnected.\n");
            break;
        }
        
        if (cmd == "HELP" || cmd == "?") {
            printf("Commands:\n");
            printf("  PING                    Heartbeat\n");
            printf("  HWND_MAP                All window handles\n");
            printf("  TREE_COUNT              File tree item count\n");
            printf("  TREE_ITEMS              Full tree listing\n");
            printf("  CMD <id>                Send menu command (e.g. CMD 1020)\n");
            printf("  TITLE                   Window title\n");
            printf("  EDITOR_TEXT             Editor content\n");
            printf("  OUTPUT_TEXT             Output panel content\n");
            printf("  OPEN_FOLDER <path>      Open folder in tree\n");
            printf("  LOAD_FILE <path>        Load file in editor\n");
            printf("  VISIBILITY              Panel visibility\n");
            printf("  WORKSPACE               Current workspace\n");
            printf("  QUIT                    Disconnect\n");
            continue;
        }
        
        std::string result = SendCommand(cmd);
        printf("%s", result.c_str());
        if (!result.empty() && result.back() != '\n') printf("\n");
    }
}

void RunScript(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) { fprintf(stderr, "Cannot open script: %s\n", filename); return; }
    
    char line[4096];
    int testNum = 0, pass = 0, fail = 0;
    
    while (fgets(line, sizeof(line), f)) {
        std::string cmd(line);
        while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) cmd.pop_back();
        if (cmd.empty() || cmd[0] == '#') continue;  // Skip blanks and comments
        
        // Check for assertion lines: "EXPECT <pattern>"
        if (cmd.rfind("EXPECT ", 0) == 0) {
            std::string pattern = cmd.substr(7);
            // Check last result
            // (handled below)
            continue;
        }
        
        testNum++;
        printf("[%03d] >> %s\n", testNum, cmd.c_str());
        
        if (cmd == "QUIT" || cmd == "EXIT") {
            SendCommand("QUIT");
            printf("       Disconnected.\n");
            break;
        }
        
        std::string result = SendCommand(cmd);
        printf("       %s", result.c_str());
        if (!result.empty() && result.back() != '\n') printf("\n");
        
        // Read next line for optional EXPECT
        if (fgets(line, sizeof(line), f)) {
            std::string next(line);
            while (!next.empty() && (next.back() == '\n' || next.back() == '\r')) next.pop_back();
            if (next.rfind("EXPECT ", 0) == 0) {
                std::string expected = next.substr(7);
                if (result.find(expected) != std::string::npos) {
                    printf("       PASS: found '%s'\n", expected.c_str());
                    pass++;
                } else {
                    printf("       FAIL: expected '%s'\n", expected.c_str());
                    fail++;
                }
            } else {
                // Not an EXPECT line, seek back
                fseek(f, -(long)strlen(line), SEEK_CUR);
            }
        }
    }
    fclose(f);
    
    printf("\n========================================\n");
    printf("Script: %d commands, %d PASS, %d FAIL\n", testNum, pass, fail);
    printf("========================================\n");
}

void RunStdin() {
    char line[4096];
    while (fgets(line, sizeof(line), stdin)) {
        std::string cmd(line);
        while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) cmd.pop_back();
        if (cmd.empty() || cmd[0] == '#') continue;
        if (cmd == "QUIT") { SendCommand("QUIT"); break; }
        
        std::string result = SendCommand(cmd);
        printf("%s", result.c_str());
    }
}

int main(int argc, char* argv[]) {
    if (!Connect()) {
        fprintf(stderr, "Failed to connect to RawrXD IDE test beacon pipe.\n");
        fprintf(stderr, "Make sure the IDE is running.\n");
        return 1;
    }
    
    if (argc == 1) {
        RunInteractive();
    } else if (argc == 2 && std::string(argv[1]) == "-stdin") {
        RunStdin();
    } else if (argc == 3 && std::string(argv[1]) == "-script") {
        RunScript(argv[2]);
    } else if (argc >= 2) {
        // Single command mode: join all args
        std::string cmd;
        for (int i = 1; i < argc; i++) {
            if (i > 1) cmd += " ";
            cmd += argv[i];
        }
        std::string result = SendCommand(cmd);
        printf("%s", result.c_str());
    }
    
    CloseHandle(g_hPipe);
    return 0;
}

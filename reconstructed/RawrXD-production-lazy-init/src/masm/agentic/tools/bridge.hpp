/*
 * Agentic Tools Bridge - C++ interface to MASM Agentic Tools
 * 
 * This header provides extern "C" declarations to call the pure MASM
 * Agentic Tools implementation. For 32-bit builds only.
 *
 * Usage:
 *   #include "agentic_tools_bridge.hpp"
 *   
 *   // Execute a tool
 *   auto result = AgenticToolExecutor("readFile", "C:\\path\\to\\file.txt");
 *
 * Note: The MASM implementation is 32-bit (.386). For 64-bit builds,
 * use the C++ implementation in agentic_tools.cpp instead.
 *
 * Copyright (c) 2025 RawrXD Project
 */

#ifndef AGENTIC_TOOLS_BRIDGE_HPP
#define AGENTIC_TOOLS_BRIDGE_HPP

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Tool Result Structure
 * Matches the MASM ToolResult structure
 */
struct MasmToolResult {
    int32_t success;    // 1 = success, 0 = failure
    char* output;       // Output string pointer
    char* error;        // Error string pointer
    int32_t exitCode;   // Exit code for command execution
};

/**
 * @brief Execute an agentic tool
 * 
 * @param lpToolName Tool name: "readFile", "writeFile", "listDirectory", 
 *                   "deleteFile", "executeCommand"
 * @param lpParams Tool parameters (file path, command, etc.)
 * @return Result in EAX (pointer to output/error) and EBX (success flag)
 * 
 * Note: This function is implemented in agentic_tools.asm (32-bit only)
 */
void* __stdcall AgenticToolExecutor(const char* lpToolName, const char* lpParams);

/**
 * @brief Read file contents
 * @param lpPath Path to file
 * @return Pointer to file contents (caller must free with GlobalFree)
 */
void* __stdcall ReadFileTool(const char* lpPath);

/**
 * @brief Write content to file
 * @param lpPath Path to file
 * @param lpContent Content to write
 * @return Pointer to result message
 */
void* __stdcall WriteFileTool(const char* lpPath, const char* lpContent);

/**
 * @brief List directory contents
 * @param lpPath Path to directory
 * @return Pointer to directory listing
 */
void* __stdcall ListDirectoryTool(const char* lpPath);

/**
 * @brief Execute shell command
 * @param lpCommand Command to execute
 * @return Pointer to result message
 */
void* __stdcall ExecuteCommandTool(const char* lpCommand);

/**
 * @brief Delete a file
 * @param lpPath Path to file
 * @return Pointer to result message
 */
void* __stdcall DeleteFileTool(const char* lpPath);

/**
 * @brief Initialize the agentic kernel
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall AgenticKernelInit(void);

/**
 * @brief Register QT IDE handle
 * @param hQtWindow Handle to QT window
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall QTIdeRegister(void* hQtWindow);

/**
 * @brief Register CLI IDE handle
 * @param hConsole Handle to console
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall CLIIdeRegister(void* hConsole);

/**
 * @brief Send message to QT IDE
 * @param msgType Message type (0=info, 1=error, 2=success)
 * @param pData Pointer to data
 * @param dataLen Data length
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall QTIdeSendMessage(int32_t msgType, const void* pData, int32_t dataLen);

/**
 * @brief Receive command from QT IDE
 * @param pCommand Command string
 * @param commandLen Command length
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall QTIdeReceiveCommand(const char* pCommand, int32_t commandLen);

/**
 * @brief Handle drag and drop file
 * @param pFilePath Path to dropped file
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall HandleDragDrop(const char* pFilePath);

/**
 * @brief Process agentic command
 * @param pCommand Command string
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ProcessAgenticCommand(const char* pCommand);

/**
 * @brief Scaffold React + Vite project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldReact(const char* targetPath);

/**
 * @brief Scaffold Java project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldJava(const char* targetPath);

/**
 * @brief Scaffold C# project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldCSharp(const char* targetPath);

/**
 * @brief Scaffold Swift project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldSwift(const char* targetPath);

/**
 * @brief Scaffold Kotlin project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldKotlin(const char* targetPath);

/**
 * @brief Scaffold Ruby project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldRuby(const char* targetPath);

/**
 * @brief Scaffold PHP project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldPHP(const char* targetPath);

/**
 * @brief Scaffold Perl project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldPerl(const char* targetPath);

/**
 * @brief Scaffold Lua project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldLua(const char* targetPath);

/**
 * @brief Scaffold Elixir project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldElixir(const char* targetPath);

/**
 * @brief Scaffold Haskell project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldHaskell(const char* targetPath);

/**
 * @brief Scaffold OCaml project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldOCaml(const char* targetPath);

/**
 * @brief Scaffold Scala project
 * @param targetPath Target directory path
 * @return 1 on success, 0 on failure
 */
int32_t __stdcall ScaffoldScala(const char* targetPath);

/**
 * @brief String length (custom implementation)
 * @param lpString String pointer
 * @return String length
 */
int32_t __stdcall StringLength(const char* lpString);

/**
 * @brief String copy (custom implementation)
 * @param lpDest Destination buffer
 * @param lpSrc Source string
 * @return Destination pointer
 */
char* __stdcall StringCopy(char* lpDest, const char* lpSrc);

/**
 * @brief String concatenate (custom implementation)
 * @param lpDest Destination buffer
 * @param lpSrc String to append
 * @return Destination pointer
 */
char* __stdcall StringConcat(char* lpDest, const char* lpSrc);

#ifdef __cplusplus
}

// C++ convenience namespace
namespace rawrxd {
namespace masm {

/**
 * @brief Tool names for AgenticToolExecutor
 */
namespace tools {
    constexpr const char* READ_FILE = "readFile";
    constexpr const char* WRITE_FILE = "writeFile";
    constexpr const char* LIST_DIR = "listDirectory";
    constexpr const char* DELETE_FILE = "deleteFile";
    constexpr const char* EXECUTE_CMD = "executeCommand";
}

/**
 * @brief C++ wrapper for MASM Agentic Tools (32-bit only)
 */
class MasmAgenticTools {
public:
    static void* execute(const char* toolName, const char* params) {
        return AgenticToolExecutor(toolName, params);
    }
    
    static void* readFile(const char* path) {
        return ReadFileTool(path);
    }
    
    static void* writeFile(const char* path, const char* content) {
        return WriteFileTool(path, content);
    }
    
    static void* listDir(const char* path) {
        return ListDirectoryTool(path);
    }
    
    static void* executeCmd(const char* command) {
        return ExecuteCommandTool(command);
    }

    static void* deleteFile(const char* path) {
        return DeleteFileTool(path);
    }
    
    // Agentic kernel functions
    static int32_t initKernel() {
        return AgenticKernelInit();
    }
    
    static int32_t registerQT(void* hWindow) {
        return QTIdeRegister(hWindow);
    }
    
    static int32_t registerCLI(void* hConsole) {
        return CLIIdeRegister(hConsole);
    }
    
    static int32_t sendMessageQT(int32_t msgType, const void* data, int32_t len) {
        return QTIdeSendMessage(msgType, data, len);
    }
    
    static int32_t receiveCommandQT(const char* cmd, int32_t len) {
        return QTIdeReceiveCommand(cmd, len);
    }
    
    static int32_t handleDrop(const char* filePath) {
        return HandleDragDrop(filePath);
    }
    
    static int32_t processCommand(const char* command) {
        return ProcessAgenticCommand(command);
    }
    
    static int32_t scaffoldReact(const char* target) {
        return ScaffoldReact(target);
    }
    
    static int32_t scaffoldJava(const char* target) {
        return ScaffoldJava(target);
    }
    
    static int32_t scaffoldCSharp(const char* target) {
        return ScaffoldCSharp(target);
    }
    
    static int32_t scaffoldSwift(const char* target) {
        return ScaffoldSwift(target);
    }
    
    static int32_t scaffoldKotlin(const char* target) {
        return ScaffoldKotlin(target);
    }
    
    static int32_t scaffoldRuby(const char* target) {
        return ScaffoldRuby(target);
    }
    
    static int32_t scaffoldPHP(const char* target) {
        return ScaffoldPHP(target);
    }
    
    static int32_t scaffoldPerl(const char* target) {
        return ScaffoldPerl(target);
    }
    
    static int32_t scaffoldLua(const char* target) {
        return ScaffoldLua(target);
    }
    
    static int32_t scaffoldElixir(const char* target) {
        return ScaffoldElixir(target);
    }
    
    static int32_t scaffoldHaskell(const char* target) {
        return ScaffoldHaskell(target);
    }
    
    static int32_t scaffoldOCaml(const char* target) {
        return ScaffoldOCaml(target);
    }
    
    static int32_t scaffoldScala(const char* target) {
        return ScaffoldScala(target);
    }
    
    // Deleted to prevent instantiation
    MasmAgenticTools() = delete;
    MasmAgenticTools(const MasmAgenticTools&) = delete;
    MasmAgenticTools& operator=(const MasmAgenticTools&) = delete;
};

} // namespace masm
} // namespace rawrxd

#endif // __cplusplus

#endif // AGENTIC_TOOLS_BRIDGE_HPP

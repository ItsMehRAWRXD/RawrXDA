#include "agentic/agentic_command_executor.h"
#include <iostream>
#include <thread>
#include <algorithm>

AgenticCommandExecutor::AgenticCommandExecutor() : m_isRunning(false) {
    // Default allowed commands
    m_autoApproveList = {
        "npm test", "cargo check", "pytest", "python -m pytest",
        "cargo build", "make", "cmake --build", "dir", "ls", "echo"
    };
}

AgenticCommandExecutor::~AgenticCommandExecutor() {
    cancelCommand();
}

void AgenticCommandExecutor::setAutoApproveList(const std::vector<std::string> &commands) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_autoApproveList = commands;
}

void AgenticCommandExecutor::executeCommand(const std::string &command, const std::vector<std::string> &arguments, bool requireApproval) {
    std::string fullCommand = command;
    for (const auto& arg : arguments) {
        fullCommand += " " + arg;
    }

    if (requireApproval && !isAutoApproved(fullCommand)) {
        bool approved = false;
        if (m_approvalCb) {
             approved = m_approvalCb(fullCommand);
        } else {
             // Default deny if no callback
             approved = false; 
        }
        
        if (!approved) {
            if (m_finishedCb) m_finishedCb(false, -1);
            return;
        }
    }

    runProcess(fullCommand);
}

void AgenticCommandExecutor::runProcess(std::string cmdLine) {
    SECURITY_ATTRIBUTES saAttr; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    if ( ! CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0) ) 
        return; 

    if ( ! SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
        return; 

    PROCESS_INFORMATION piProcInfo; 
    STARTUPINFOA siStartInfo;
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdError = m_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = m_hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Create a mutable copy of the command line
    std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back(0);

    // Create the child process. 
    BOOL bSuccess = CreateProcessA(NULL, 
        cmd.data(),     // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        CREATE_NO_WINDOW,             // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

    if ( ! bSuccess ) return;

    m_hProcess = piProcInfo.hProcess;
    CloseHandle(piProcInfo.hThread);
    CloseHandle(m_hChildStd_OUT_Wr); // Close write end, only child needs it

    m_isRunning = true;
    m_output.clear();

    // Start a thread to read output
    std::thread([this, piProcInfo]() {
        DWORD dwRead; 
        CHAR chBuf[4096]; 
        BOOL bSuccess = FALSE;

        for (;;) { 
            bSuccess = ReadFile( m_hChildStd_OUT_Rd, chBuf, 4096, &dwRead, NULL);
            if( ! bSuccess || dwRead == 0 ) break; 

            std::string s(chBuf, dwRead);
            appendOutput(s);
        } 
        
        WaitForSingleObject(piProcInfo.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(piProcInfo.hProcess, &exitCode);
        
        CloseHandle(m_hChildStd_OUT_Rd);
        CloseHandle(piProcInfo.hProcess);
        
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_isRunning = false;
        }

        if (m_finishedCb) m_finishedCb(true, exitCode);

    }).detach();
}

void AgenticCommandExecutor::appendOutput(const std::string& str) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_output += str;
    if (m_outputCb) m_outputCb(str);
}

std::string AgenticCommandExecutor::getOutput() const {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_output;
}

void AgenticCommandExecutor::cancelCommand() {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_isRunning && m_hProcess) {
        TerminateProcess(m_hProcess, 1);
    }
}

void AgenticCommandExecutor::onOutputReceived(OutputCallback cb) { m_outputCb = cb; }
void AgenticCommandExecutor::onExecutionFinished(StatusCallback cb) { m_finishedCb = cb; }
void AgenticCommandExecutor::setApprovalCallback(ApprovalCallback cb) { m_approvalCb = cb; }

bool AgenticCommandExecutor::isAutoApproved(const std::string &command) {
    std::unique_lock<std::mutex> lock(m_mutex);
    // Rough contains check
    for (const auto& allowed : m_autoApproveList) {
        if (command.find(allowed) != std::string::npos) return true;
    }
    return false;
}


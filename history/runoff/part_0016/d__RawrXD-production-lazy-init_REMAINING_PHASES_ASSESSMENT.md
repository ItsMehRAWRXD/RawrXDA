# RawrXD IDE - Remaining Phases Assessment

**Date**: January 14, 2026  
**Current Status**: 60% Complete (6 of 10 phases, 9,946 lines)  
**Assessment Focus**: Phases 7-10 (4,054 estimated lines remaining)  
**Zero-Stub Policy**: Maintained across all 9,946 completed lines  

---

## Overview: Remaining Implementation

### Completion Summary
| Aspect | Status |
|--------|--------|
| Phases Completed | 6 of 10 (60%) |
| Lines Implemented | 9,946 |
| Lines Remaining | ~4,054 |
| Total Project | ~14,000 lines |
| Estimated Completion | 100% (4 phases) |

### Phases to Implement
1. **Phase 7**: Code Profiler (~1,500 lines)
2. **Phase 8**: Docker Integration (~1,000 lines)
3. **Phase 9**: SSH/Remote Development (~1,300 lines)
4. **Phase 10**: AI Assistant Integration (~2,000 lines)

---

## Phase 7: Code Profiler (~1,500 lines)

### Purpose
Real-time CPU and memory profiling with flamegraph visualization for performance analysis and optimization identification.

### Core Components (6 Files)

#### 1. ProfileData.h (~120 lines)
**Responsibility**: Data structures for profiling information

```cpp
struct StackFrame {
    QString functionName;
    QString fileName;
    int lineNumber;
    quint64 timeSpent;      // microseconds
    quint64 callCount;
    quint64 selfTime;       // time spent in this function only
    QList<quint64> childFrameIds;
};

struct CallStack {
    QList<StackFrame> frames;
    quint64 timestamp;      // microseconds since start
    quint64 memoryUsage;    // bytes
};

struct ProfileSession {
    QString processName;
    QDateTime startTime;
    quint64 totalRuntime;
    QList<CallStack> callStacks;
    QMap<QString, quint64> functionStats;  // function name -> total time
};

enum class SamplingRate {
    Low = 100,      // 100 samples/sec
    Normal = 1000,  // 1000 samples/sec
    High = 10000,   // 10000 samples/sec
};
```

**Key Methods**:
- `ProfileData()`, `~ProfileData()`
- `addCallStack(const CallStack& stack)`
- `getFunctionStats()`
- `getTopFunctions(int count)`
- `exportToJSON(const QString& path)`
- `calculateSelfTime()`

**Dependencies**: Qt Core

---

#### 2. ProfileData.cpp (~120 lines)
**Responsibility**: Implementation of profiling data structures

**Key Implementations**:
- `addCallStack()` - Append stack frame to profile
- `calculateSelfTime()` - Calculate time in function vs children
- `getTopFunctions()` - Sort functions by time spent
- `exportToJSON()` - Export profile data to JSON format

**Performance**: O(n log n) for sorting operations

---

#### 3. CPUProfiler.h (~150 lines)
**Responsibility**: CPU profiling interface

```cpp
class CPUProfiler {
public:
    explicit CPUProfiler(QObject* parent = nullptr);
    ~CPUProfiler();
    
    // Control methods
    void startProfiling(const QString& processName, SamplingRate rate = SamplingRate::Normal);
    void stopProfiling();
    bool isProfiling() const;
    
    // Configuration
    void setSamplingRate(SamplingRate rate);
    void setMaxStackDepth(int depth);
    void setFilterPatterns(const QStringList& patterns);
    
    // Data access
    ProfileData* getProfileData() const;
    QList<StackFrame> getCurrentStack() const;
    quint64 getTotalSamples() const;
    
signals:
    void profilingStarted();
    void profilingStopped();
    void sampleCollected(const CallStack& stack);
    void profileUpdated();
    void error(const QString& message);
    
private:
    void collectSample();
    void updateStack();
    
private:
    QThread* m_samplingThread;
    ProfileData* m_profileData;
    SamplingRate m_samplingRate;
    int m_maxStackDepth;
    QStringList m_filterPatterns;
    bool m_isProfiling;
    QTimer* m_samplingTimer;
};
```

**Key Methods**:
- `startProfiling()` - Start CPU sampling
- `stopProfiling()` - Stop and save profiling data
- `collectSample()` - Collect single stack sample
- `setSamplingRate()` - Configure sampling frequency
- `getProfileData()` - Access collected profile

**Platform-Specific**:
- Windows: Use Debug Help Library or Performance Monitor
- Linux: Use /proc/self/maps and stack unwinding
- macOS: Use sampling profiler API

---

#### 4. CPUProfiler.cpp (~250 lines)
**Responsibility**: CPU profiling implementation

**Key Implementations**:
- Thread-based sampling loop (no main thread blocking)
- Stack unwinding algorithm
- Symbol resolution from debug info
- Sample collection with timestamps
- Filter pattern matching for function filtering
- Signal emission for UI updates

**Stack Collection Algorithm**:
```cpp
void CPUProfiler::collectSample()
{
    // Capture current stack
    HANDLE hProcess = GetCurrentProcess();
    CONTEXT context;
    RtlCaptureContext(&context);
    
    // Unwind stack
    QList<StackFrame> stack = unwindStack(&context);
    
    // Apply filters
    stack = applyFilters(stack);
    
    // Limit depth
    if (stack.size() > m_maxStackDepth)
        stack.resize(m_maxStackDepth);
    
    // Create call stack entry
    CallStack entry;
    entry.frames = stack;
    entry.timestamp = getCurrentTimeUs();
    entry.memoryUsage = getMemoryUsage();
    
    // Add to profile
    m_profileData->addCallStack(entry);
    
    // Signal update
    emit sampleCollected(entry);
}
```

**Performance**: < 1ms per sample collection

---

#### 5. MemoryProfiler.h (~120 lines)
**Responsibility**: Memory profiling interface

```cpp
struct MemoryAllocation {
    quint64 address;
    size_t size;
    QString allocator;  // malloc, new, new[], custom
    QDateTime timestamp;
    QStringList callStack;
};

struct MemorySnapshot {
    quint64 timestamp;
    quint64 totalAllocated;
    quint64 totalFreed;
    quint64 peakUsage;
    QMap<size_t, int> allocationSizes;  // size -> count
    QList<MemoryAllocation> allocations;
};

class MemoryProfiler {
public:
    explicit MemoryProfiler(QObject* parent = nullptr);
    
    // Control
    void startProfiling();
    void stopProfiling();
    bool isProfiling() const;
    
    // Configuration
    void setTrackingMode(bool trackAllocs, bool trackFrees);
    void setMinAllocationSize(size_t minSize);
    
    // Data access
    MemorySnapshot takeSnapshot() const;
    QList<MemoryAllocation> getLeaks() const;
    quint64 getTotalAllocated() const;
    quint64 getCurrentUsage() const;
    
signals:
    void profilingStarted();
    void profilingStopped();
    void snapshotTaken(const MemorySnapshot& snapshot);
    void leakDetected(const MemoryAllocation& alloc);
    
private:
    void detectLeaks();
};
```

**Key Methods**:
- `startProfiling()` - Start memory tracking
- `takeSnapshot()` - Capture memory state
- `getLeaks()` - Detect memory leaks
- `getCurrentUsage()` - Get current memory usage
- `detectLeaks()` - Compare snapshots for leaks

---

#### 6. MemoryProfiler.cpp (~150 lines)
**Responsibility**: Memory profiling implementation

**Key Implementations**:
- Allocation interception via hooks
- Memory snapshot generation
- Leak detection algorithm (retained allocations)
- Memory usage calculation
- Call stack capture on allocation
- Signal emission for leaks

**Allocation Tracking**:
```cpp
void* hooked_malloc(size_t size)
{
    void* ptr = original_malloc(size);
    
    MemoryAllocation alloc;
    alloc.address = (quint64)ptr;
    alloc.size = size;
    alloc.allocator = "malloc";
    alloc.timestamp = QDateTime::currentDateTime();
    alloc.callStack = MemoryProfiler::getInstance()->captureStack();
    
    MemoryProfiler::getInstance()->recordAllocation(alloc);
    
    return ptr;
}
```

---

### ProfilerPanel.h (~150 lines)
**Responsibility**: Main profiler UI panel

```cpp
class ProfilerPanel : public QDockWidget {
public:
    explicit ProfilerPanel(QWidget* parent = nullptr);
    
    // Profiling control
    void startProfiling();
    void stopProfiling();
    void pauseProfiling();
    
    // Configuration
    void setSamplingRate(SamplingRate rate);
    void setProfileMode(ProfileMode mode);  // CPU, Memory, Both
    
    // Export
    void exportProfile(const QString& format);  // JSON, CSV, HTML
    
    // Display
    void updateFlamegraph();
    void updateMemoryChart();
    void updateCallTree();
    
signals:
    void profilingStateChanged(bool running);
    
private:
    void setupUI();
    void createToolbar();
    void createTabs();
    
    // UI Components
    QTabWidget* m_tabWidget;
    QWidget* m_cpuTab;
    QWidget* m_memoryTab;
    QWidget* m_flameTab;
    
    CPUProfiler* m_cpuProfiler;
    MemoryProfiler* m_memoryProfiler;
};
```

---

### ProfilerPanel.cpp (~300 lines)
**Responsibility**: Profiler UI implementation

**UI Structure**:
1. **Toolbar**: Start/Stop/Pause buttons, Mode selector, Sampling rate dropdown
2. **CPU Tab**: 
   - Call tree view (function name, time, %, calls)
   - Timeline graph of CPU usage
   - Top functions list
3. **Memory Tab**:
   - Memory timeline chart
   - Allocation table (address, size, allocator, stack)
   - Memory usage breakdown
4. **Flamegraph Tab**:
   - Interactive flamegraph visualization
   - Hover for details (time spent, call count)
   - Click to zoom/filter

**Key Implementations**:
- `setupUI()` - Create all UI elements
- `updateFlamegraph()` - Render interactive flamegraph
- `updateCallTree()` - Populate function tree
- `exportProfile()` - Export to JSON/CSV/HTML
- `onSampleCollected()` - Update displays in real-time

---

### Flamegraph Visualization
```cpp
void ProfilerPanel::updateFlamegraph()
{
    // Build rectangle hierarchy
    // x = function start offset
    // y = stack depth
    // width = time spent
    // height = constant
    
    // Each bar represents function call
    // Nested calls are stacked vertically
    // Width proportional to time spent
    // Color by function category
    
    // Interactions:
    // - Hover: show time, call count, file/line
    // - Click: zoom into function
    // - Right-click: filter by function
}
```

---

### Integration Points
- **Terminal Panel**: Display live profiling while running processes
- **Editor**: Jump to hot functions (file + line)
- **Build System**: Profile build performance
- **Source Code**: Annotate expensive functions

---

## Phase 8: Docker Integration (~1,000 lines)

### Purpose
Containerize IDE and projects, manage containers, build images, and run in Docker environments.

### Core Components (5 Files)

#### 1. DockerManager.h (~120 lines)
**Responsibility**: Docker API wrapper interface

```cpp
struct DockerImage {
    QString id;
    QString repository;
    QString tag;
    quint64 size;
    QDateTime createdAt;
    QStringList ports;
};

struct DockerContainer {
    QString id;
    QString name;
    QString image;
    QString status;
    QDateTime createdAt;
    QMap<QString, QString> environment;
};

struct Dockerfile {
    QString baseImage;
    QStringList instructions;
    QMap<QString, QString> environment;
    QStringList ports;
    QString workdir;
    QString entrypoint;
};

class DockerManager {
public:
    explicit DockerManager(QObject* parent = nullptr);
    
    // Connection
    bool connectToDocker();
    bool isConnected() const;
    
    // Image operations
    QList<DockerImage> listImages();
    void buildImage(const Dockerfile& file, const QString& tag);
    void removeImage(const QString& imageId);
    void pullImage(const QString& repository, const QString& tag);
    
    // Container operations
    QList<DockerContainer> listContainers(bool running = true);
    QString createContainer(const DockerImage& image, const QString& name);
    void startContainer(const QString& containerId);
    void stopContainer(const QString& containerId);
    void removeContainer(const QString& containerId);
    
    // Execution
    QString executeCommand(const QString& containerId, const QString& command);
    void attachTerminal(const QString& containerId);
    
signals:
    void dockerConnected();
    void dockerDisconnected();
    void imageBuilt(const DockerImage& image);
    void containerStarted(const DockerContainer& container);
    void commandOutput(const QString& output);
};
```

---

#### 2. DockerManager.cpp (~200 lines)
**Responsibility**: Docker API interaction implementation

**Key Implementations**:
- Docker daemon connection (Unix socket or TCP)
- JSON API communication
- Image building and management
- Container lifecycle management
- Command execution with I/O streaming
- Signal emission for UI updates

**Docker API Examples**:
```cpp
// List images
GET /images/json

// Build image
POST /build?t=myimage:tag
Body: tarball of Dockerfile + context

// Create container
POST /containers/create
Body: {
  "Image": "ubuntu:22.04",
  "Cmd": ["/bin/bash"],
  "Env": ["VAR=value"],
  "WorkingDir": "/app"
}

// Start container
POST /containers/{id}/start

// Execute command
POST /containers/{id}/exec
Body: {
  "Cmd": ["echo", "hello"],
  "AttachStdout": true,
  "AttachStderr": true
}
```

---

#### 3. DockerfileGenerator.h (~100 lines)
**Responsibility**: Generate Dockerfiles from project configuration

```cpp
class DockerfileGenerator {
public:
    explicit DockerfileGenerator(const QString& projectPath);
    
    // Configuration
    void setBaseImage(const QString& image);
    void addEnvironmentVariable(const QString& key, const QString& value);
    void addPort(const QString& port);
    void setWorkdir(const QString& dir);
    void addBuildCommand(const QString& command);
    void addEntrypoint(const QString& command);
    
    // Generation
    Dockerfile generate();
    void save(const QString& path);
    
    // Templates
    Dockerfile generateCppTemplate();
    Dockerfile generatePythonTemplate();
    Dockerfile generateNodeTemplate();
    
private:
    void detectProjectType();
    void detectDependencies();
};
```

---

#### 4. DockerfileGenerator.cpp (~150 lines)
**Responsibility**: Dockerfile generation from templates

**Templates**:

C++ Project:
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake qt6-base-dev
WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make
ENTRYPOINT ["./app"]
```

Python Project:
```dockerfile
FROM python:3.11-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install -r requirements.txt
COPY . .
ENTRYPOINT ["python", "main.py"]
```

---

#### 5. DockerPanel.h (~150 lines)
**Responsibility**: Docker management UI

```cpp
class DockerPanel : public QDockWidget {
public:
    explicit DockerPanel(QWidget* parent = nullptr);
    
    // Image management
    void buildImage();
    void pullImage();
    void removeImage();
    
    // Container management
    void createContainer();
    void startContainer();
    void stopContainer();
    void removeContainer();
    
    // Terminal
    void attachToContainer();
    
signals:
    void imageBuilt(const QString& name);
    void containerStarted(const QString& name);
};
```

---

#### 6. DockerPanel.cpp (~300 lines)
**Responsibility**: Docker UI implementation

**UI Structure**:
1. **Toolbar**: Connect, Build, Pull, Create, Start, Stop, Remove buttons
2. **Images Tab**:
   - Image list (name, tag, size, created date)
   - Build dialog (Dockerfile editor, base image selector)
   - Pull dialog (repository, tag selector)
3. **Containers Tab**:
   - Running containers list
   - Container details (name, image, status, ports, env)
   - Create dialog (image selector, name, environment variables)
4. **Logs Tab**:
   - Real-time container logs
   - Attach terminal to container

---

### Integration Points
- **Terminal Panel**: Execute commands in containers
- **File Manager**: Mount directories to containers
- **Build System**: Build projects in Docker
- **Editor**: Remote development in containers

---

## Phase 9: SSH/Remote Development (~1,300 lines)

### Purpose
Connect to remote servers via SSH, edit files remotely, execute commands, and synchronize files.

### Core Components (6 Files)

#### 1. SSHManager.h (~150 lines)
**Responsibility**: SSH connection management

```cpp
struct SSHHost {
    QString name;
    QString hostname;
    int port;
    QString username;
    QString password;
    QString keyFile;
    QMap<QString, QString> environment;
};

struct RemoteFile {
    QString path;
    QString owner;
    QString group;
    int permissions;
    quint64 size;
    QDateTime modified;
    bool isDirectory;
};

class SSHManager {
public:
    explicit SSHManager(QObject* parent = nullptr);
    
    // Connection management
    bool connect(const SSHHost& host);
    bool isConnected() const;
    void disconnect();
    
    // Authentication
    bool authenticateWithPassword(const QString& password);
    bool authenticateWithKey(const QString& keyPath);
    
    // File operations
    QList<RemoteFile> listDirectory(const QString& path);
    QString readFile(const QString& path);
    void writeFile(const QString& path, const QString& content);
    void deleteFile(const QString& path);
    void renameFile(const QString& oldPath, const QString& newPath);
    
    // Execution
    QString executeCommand(const QString& command);
    QString executeSudo(const QString& command, const QString& password);
    
    // File transfer
    void uploadFile(const QString& localPath, const QString& remotePath);
    void downloadFile(const QString& remotePath, const QString& localPath);
    void syncDirectory(const QString& localDir, const QString& remoteDir);
    
    signals:
    void connected();
    void disconnected();
    void commandOutput(const QString& output);
    void error(const QString& message);
};
```

---

#### 2. SSHManager.cpp (~250 lines)
**Responsibility**: SSH protocol implementation (using libssh2 or Qt SSH plugin)

**Key Implementations**:
- SSH connection establishment
- Authentication (password + public key)
- SFTP file operations
- Command execution
- Channel management
- Signal emission

---

#### 3. RemoteFileSystem.h (~120 lines)
**Responsibility**: Remote file system abstraction

```cpp
class RemoteFileSystem {
public:
    explicit RemoteFileSystem(SSHManager* sshManager);
    
    // File operations
    QString readFile(const QString& path);
    void writeFile(const QString& path, const QString& content);
    
    // Directory operations
    QStringList listDirectory(const QString& path);
    void createDirectory(const QString& path);
    void removeDirectory(const QString& path);
    
    // Caching
    void enableCaching(bool enable);
    void clearCache();
    
private:
    void cacheFile(const QString& path, const QString& content);
    QString getCachedFile(const QString& path);
};
```

---

#### 4. RemoteFileSystem.cpp (~150 lines)
**Responsibility**: Remote file operations with local caching

**Key Implementations**:
- File read/write via SFTP
- Directory operations
- Local cache for performance
- Sync detection (file changed on remote)
- Notification of remote changes

---

#### 5. RemotePanel.h (~150 lines)
**Responsibility**: Remote development UI

```cpp
class RemotePanel : public QDockWidget {
public:
    explicit RemotePanel(QWidget* parent = nullptr);
    
    // Connection
    void addHost();
    void editHost();
    void removeHost();
    void connectToHost();
    void disconnectFromHost();
    
    // File browsing
    void browseRemoteDirectory();
    void editRemoteFile();
    void openRemoteTerminal();
    
    // Synchronization
    void syncToRemote();
    void syncFromRemote();
    void setAutoSync(bool enable);
    
signals:
    void hostConnected(const SSHHost& host);
    void fileOpened(const QString& content);
    void outputReceived(const QString& output);
};
```

---

#### 6. RemotePanel.cpp (~300 lines)
**Responsibility**: Remote development UI implementation

**UI Structure**:
1. **Toolbar**: Add Host, Connect, Disconnect buttons
2. **Hosts Tab**:
   - List of configured SSH hosts
   - Host details (hostname, port, username)
   - Add/Edit/Delete dialogs
3. **File Browser Tab**:
   - Remote directory tree
   - File list with details
   - Right-click context menu (download, delete, open)
4. **Terminal Tab**:
   - SSH terminal session
   - Command history
5. **Sync Status Tab**:
   - Sync configuration
   - Last sync time
   - Changed files

---

#### 7. RemoteEditor Integration
**Responsibility**: Edit remote files as local buffers

**Key Features**:
- Open remote file in editor
- Auto-sync on save
- Conflict detection
- Merge UI for conflicts
- View changes before sync

---

### Integration Points
- **Editor**: Edit remote files
- **Terminal Panel**: Remote terminal
- **File Manager**: Remote file browser
- **Build System**: Build on remote server
- **Git Integration**: Git operations on remote

---

## Phase 10: AI Assistant Integration (~2,000 lines)

### Purpose
AI-powered code completion, refactoring suggestions, chat interface, and intelligent code analysis.

### Core Components (7 Files)

#### 1. AIProvider.h (~100 lines)
**Responsibility**: Abstract AI provider interface

```cpp
enum class AIProviderType {
    OpenAI,
    Anthropic,
    LocalLLM,
    Azure,
    GooglePaLM
};

struct AIRequest {
    QString prompt;
    int maxTokens;
    float temperature;
    float topP;
    QStringList stopSequences;
};

struct AIResponse {
    QString text;
    int tokenCount;
    QString model;
    QDateTime timestamp;
};

class AIProvider {
public:
    virtual ~AIProvider() = default;
    
    virtual bool initialize(const QString& apiKey) = 0;
    virtual AIResponse query(const AIRequest& request) = 0;
    virtual void setModel(const QString& model) = 0;
    
signals:
    virtual void responseReceived(const AIResponse& response) = 0;
    virtual void error(const QString& message) = 0;
};
```

---

#### 2. CodeCompleter.h (~150 lines)
**Responsibility**: AI-powered code completion

```cpp
struct CompletionSuggestion {
    QString text;
    QString description;
    QString type;  // function, variable, class, etc.
    QString icon;
    int score;
    QString documentation;
};

class CodeCompleter {
public:
    explicit CodeCompleter(AIProvider* provider, QObject* parent = nullptr);
    
    // Configuration
    void setEnabled(bool enabled);
    void setContext(const QString& code, int cursorPos);
    void setLanguage(const QString& language);
    
    // Completion
    QList<CompletionSuggestion> getCompletions();
    QString insertCompletion(const CompletionSuggestion& suggestion);
    
    // Context
    void setProjectContext(const QString& projectPath);
    void addCustomContext(const QString& context);
    
signals:
    void completionsReady(const QList<CompletionSuggestion>& suggestions);
    void error(const QString& message);
    
private:
    void buildContext();
    AIRequest buildPrompt();
};
```

---

#### 3. CodeCompleter.cpp (~200 lines)
**Responsibility**: Code completion implementation

**Completion Prompt**:
```
Language: Python
File: main.py
Line 42: def calculate_total(items):
    result = 0
    for item in items:
        result += item.

COMPLETE: [CURSOR]

Suggestions:
1. value (int) - Get item value
2. quantity (int) - Get item quantity
3. price (float) - Get item price
```

**Features**:
- Language detection
- Import suggestions
- Function signatures
- Type hints
- Documentation hints

---

#### 4. CodeRefactorer.h (~100 lines)
**Responsibility**: AI-powered code refactoring suggestions

```cpp
struct RefactoringSuggestion {
    QString description;
    QString oldCode;
    QString newCode;
    QString category;  // naming, performance, readability, etc.
    int severity;      // 1-5
    QString explanation;
};

class CodeRefactorer {
public:
    explicit CodeRefactorer(AIProvider* provider);
    
    // Analysis
    QList<RefactoringSuggestion> suggestRefactorings(const QString& code);
    QList<RefactoringSuggestion> suggestRefactorings(const QString& code, const QString& context);
    
    // Application
    QString applyRefactoring(const QString& code, const RefactoringSuggestion& suggestion);
    
    // Batching
    void analyzeFunctionForRefactoring(const QString& functionCode);
    void analyzeClassForRefactoring(const QString& classCode);
    
signals:
    void suggestionsReady(const QList<RefactoringSuggestion>& suggestions);
};
```

---

#### 5. CodeRefactorer.cpp (~150 lines)
**Responsibility**: Refactoring suggestion implementation

**Refactoring Categories**:
- **Naming**: Improve variable/function names
- **Performance**: Optimize loops, cache values
- **Readability**: Extract methods, simplify logic
- **Security**: Add input validation, fix vulnerabilities
- **Testing**: Add test cases, improve coverage

---

#### 6. AIAssistantPanel.h (~150 lines)
**Responsibility**: Chat UI for AI assistant

```cpp
struct ChatMessage {
    QString role;  // user, assistant, system
    QString content;
    QDateTime timestamp;
    QStringList codeBlocks;
    QStringList attachments;  // file references
};

class AIAssistantPanel : public QDockWidget {
public:
    explicit AIAssistantPanel(QWidget* parent = nullptr);
    
    // Chat
    void sendMessage(const QString& message);
    void sendWithContext();
    void clearHistory();
    
    // Context
    void attachCurrentFile();
    void attachSelection();
    void attachFile(const QString& path);
    
    // Features
    void explainSelectedCode();
    void suggestRefactorings();
    void generateDocumentation();
    void generateUnitTests();
    
    signals:
    void assistantResponse(const QString& response);
    void refactoringsSuggested(const QList<RefactoringSuggestion>& suggestions);
    void codeGenerated(const QString& code);
};
```

---

#### 7. AIAssistantPanel.cpp (~400 lines)
**Responsibility**: Chat UI implementation

**UI Structure**:
1. **Chat Area**:
   - Message history (user + assistant)
   - Code blocks with syntax highlighting
   - Clickable suggestions
2. **Input Area**:
   - Message input with markdown support
   - Attachment button
   - Context selector (file, selection, project)
3. **Toolbar**:
   - Explain code button
   - Suggest refactorings button
   - Generate docs button
   - Generate tests button
4. **Settings**:
   - Model selector
   - Temperature slider
   - Max tokens input
   - System prompt editor

**Chat Prompts**:

Explain Code:
```
Explain this code to a junior developer:

[selected code]

Be clear and concise, focusing on what it does and why.
```

Suggest Refactorings:
```
Suggest improvements for this code:

[code]

Consider:
- Performance optimizations
- Readability improvements
- Best practices
- Security concerns

Format: [Category] [Severity 1-5] [Description]
```

Generate Documentation:
```
Generate clear documentation for this function:

[function signature]
[function code]

Include:
- Purpose
- Parameters
- Return value
- Exceptions
- Example usage
```

Generate Tests:
```
Generate unit tests for this function:

[function code]

Test coverage:
- Happy path
- Edge cases
- Error conditions

Use: [testing framework]
```

---

#### 8. DocstringGenerator.h (~80 lines)
**Responsibility**: Generate docstrings for code

```cpp
class DocstringGenerator {
public:
    explicit DocstringGenerator(AIProvider* provider);
    
    // Generation
    QString generateFunctionDocstring(const QString& functionCode);
    QString generateClassDocstring(const QString& classCode);
    QString generateModuleDocstring(const QString& filePath);
    
    // Format
    void setFormat(const QString& format);  // Python, Java, C++, etc.
    void setStyle(const QString& style);    // Google, NumPy, Doxygen, etc.
};
```

---

### Integration Points
- **Editor**: Inline completion, inline refactoring suggestions
- **Terminal Panel**: Execute generated code
- **Build System**: Analyze build errors with AI
- **Git Integration**: Generate commit messages
- **File Manager**: Batch operations with AI

---

## Implementation Strategy

### Execution Order (Recommended)
1. **Phase 7** (Code Profiler) - ~1,500 lines - 1 week
   - Foundation for performance analysis
   - No external API dependencies
   - Complete implementation possible
   
2. **Phase 8** (Docker) - ~1,000 lines - 1 week
   - Container management
   - Builds on existing terminal
   - Docker daemon connection required
   
3. **Phase 9** (SSH/Remote) - ~1,300 lines - 1.5 weeks
   - Remote development workflow
   - SFTP + terminal integration
   - Cross-platform SSH support
   
4. **Phase 10** (AI Assistant) - ~2,000 lines - 2 weeks
   - API integration
   - Chat UI + code generation
   - Completion and refactoring

### Total Remaining
- **Lines**: 4,054 lines
- **Files**: ~20 files (headers + implementations)
- **Estimated Time**: 4-5 weeks at full implementation capacity

---

## Quality Assurance Plan

### Per-Phase Testing
1. ✅ Unit tests for core functionality
2. ✅ Integration tests with existing phases
3. ✅ UI tests for new panels
4. ✅ Performance benchmarks
5. ✅ Cross-platform validation

### Before Final Integration
1. ✅ Zero-stub verification (all methods implemented)
2. ✅ Memory leak detection
3. ✅ Error handling comprehensive review
4. ✅ Documentation completeness
5. ✅ Git history clean and clear

---

## Risk Assessment

### Phase 7 (Profiler)
- **Risk**: Stack unwinding complexity (platform-specific)
- **Mitigation**: Use platform APIs, extensive testing
- **Fallback**: Simplified profiler without stack unwinding

### Phase 8 (Docker)
- **Risk**: Docker daemon dependency
- **Mitigation**: Graceful degradation, error messaging
- **Fallback**: Docker daemon not required features

### Phase 9 (SSH)
- **Risk**: SSH library dependency, key management
- **Mitigation**: Use Qt SSH or libssh2, secure key storage
- **Fallback**: Basic command execution only

### Phase 10 (AI)
- **Risk**: API key management, rate limiting
- **Mitigation**: Environment variables, local caching
- **Fallback**: Offline mode with simpler suggestions

---

## Next Steps

**Ready to begin Phase 7 (Code Profiler) with full implementation (no stubs)?**

All 4 remaining phases are fully scoped with:
- Complete file structure
- Full method signatures
- Data structures defined
- Integration points identified
- UI layouts specified
- Algorithm pseudocode provided

**Estimated project completion**: ~4-5 weeks at accelerated pace

---

**Assessment Completed**: January 14, 2026  
**Status**: Ready for Phase 7 Implementation  
**Zero-Stub Policy**: Maintained and ready for extension  


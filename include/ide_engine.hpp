#ifndef RAWRXD_IDE_ENGINE_HPP
#define RAWRXD_IDE_ENGINE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD
{

class TextBuffer;
class EditorView;
class AIAgenticBridge;
class SyntaxHighlighter;

enum class IDEState
{
    UNINITIALIZED,
    INITIALIZING,
    READY,
    ACTIVE,
    SHUTTING_DOWN,
    ERROR_STATE
};

struct DocumentID
{
    uint64_t id;
    std::string path;
    std::string language;
    bool isDirty;
    bool isNew;
    DocumentID() : id(0), isDirty(false), isNew(true) {}
};

struct IDEConfig
{
    std::string theme = "dark";
    int fontSize = 14;
    std::string fontFamily = "Consolas";
    bool wordWrap = true;
    bool showLineNumbers = true;
    bool enableAI = true;
    std::string ollamaEndpoint = "http://localhost:11434";
    std::string defaultModel = "codellama";
    size_t maxBufferSize = 10 * 1024 * 1024;
    int threadPoolSize = 4;
};

enum class IDEEventType
{
    DOCUMENT_OPENED,
    DOCUMENT_CLOSED,
    DOCUMENT_MODIFIED,
    DOCUMENT_SAVED,
    SELECTION_CHANGED,
    CURSOR_MOVED,
    AI_COMPLETION_READY,
    AI_ERROR,
    BUILD_STARTED,
    BUILD_FINISHED
};

struct IDEEvent
{
    IDEEventType type;
    DocumentID doc;
    std::chrono::steady_clock::time_point timestamp;
    std::string data;
};

class IDEEngine
{
  public:
    static IDEEngine& Instance();
    bool Initialize(const IDEConfig& config);
    void Shutdown();
    IDEState GetState() const { return state_.load(); }
    DocumentID OpenDocument(const std::string& path);
    bool CloseDocument(uint64_t docId);
    bool SaveDocument(uint64_t docId);
    std::shared_ptr<TextBuffer> GetBuffer(uint64_t docId);
    std::vector<DocumentID> GetOpenDocuments() const;
    void SetActiveDocument(uint64_t docId);
    uint64_t GetActiveDocument() const { return activeDocId_.load(); }
    void RequestAICompletion(uint64_t docId, int line, int column);
    void RequestAIChat(const std::string& message);
    bool IsAIAvailable() const;
    using EventHandler = std::function<void(const IDEEvent&)>;
    void Subscribe(IDEEventType type, EventHandler handler);
    void Unsubscribe(IDEEventType type);
    IDEConfig& GetConfig() { return config_; }
    void ApplyConfig(const IDEConfig& config);
    std::string GetStatusMessage() const;
    double GetMemoryUsageMB() const;

  private:
    IDEEngine() = default;
    ~IDEEngine() = default;
    IDEEngine(const IDEEngine&) = delete;
    IDEEngine& operator=(const IDEEngine&) = delete;
    void ProcessEvents();
    void Notify(const IDEEvent& event);
    std::atomic<IDEState> state_{IDEState::UNINITIALIZED};
    IDEConfig config_;
    std::atomic<uint64_t> nextDocId_{1};
    std::atomic<uint64_t> activeDocId_{0};
    std::unordered_map<uint64_t, std::shared_ptr<TextBuffer>> buffers_;
    std::unordered_map<IDEEventType, std::vector<EventHandler>> handlers_;
    mutable std::mutex bufferMutex_;
    mutable std::mutex eventMutex_;
    std::unique_ptr<AIAgenticBridge> aiBridge_;
    std::thread eventThread_;
    std::atomic<bool> running_{false};
};

}  // namespace RawrXD
#endif  // RAWRXD_IDE_ENGINE_HPP

#include "ide_engine.hpp"
#include "agentic_bridge.hpp"
#include "text_buffer.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <psapi.h>
#include <windows.h>

namespace RawrXD
{

IDEEngine& IDEEngine::Instance()
{
    static IDEEngine instance;
    return instance;
}

bool IDEEngine::Initialize(const IDEConfig& config)
{
    IDEState expected = IDEState::UNINITIALIZED;
    if (!state_.compare_exchange_strong(expected, IDEState::INITIALIZING))
        return false;
    config_ = config;
    if (config.enableAI)
    {
        aiBridge_ = std::make_unique<AIAgenticBridge>();
        if (!aiBridge_->Initialize(config.nativeEndpoint, config.defaultModel))
            aiBridge_.reset();
    }
    running_ = true;
    eventThread_ = std::thread(&IDEEngine::ProcessEvents, this);
    state_.store(IDEState::READY);
    return true;
}

void IDEEngine::Shutdown()
{
    state_.store(IDEState::SHUTTING_DOWN);
    running_ = false;
    if (eventThread_.joinable())
        eventThread_.join();
    std::lock_guard<std::mutex> lock(bufferMutex_);
    buffers_.clear();
    aiBridge_.reset();
    state_.store(IDEState::UNINITIALIZED);
}

DocumentID IDEEngine::OpenDocument(const std::string& path)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    DocumentID doc;
    doc.id = nextDocId_++;
    doc.path = path;
    doc.isNew = !std::filesystem::exists(path);
    size_t dotPos = path.rfind('.');
    if (dotPos != std::string::npos)
    {
        std::string ext = path.substr(dotPos + 1);
        if (ext == "cpp" || ext == "hpp" || ext == "h" || ext == "cc")
            doc.language = "cpp";
        else if (ext == "c")
            doc.language = "c";
        else if (ext == "rs")
            doc.language = "rust";
        else if (ext == "py")
            doc.language = "python";
        else if (ext == "js" || ext == "ts")
            doc.language = "javascript";
        else if (ext == "asm" || ext == "masm")
            doc.language = "assembly";
        else
            doc.language = "text";
    }
    auto buffer = std::make_shared<TextBuffer>(doc.id);
    buffer->SetPath(path);
    if (!doc.isNew)
    {
        std::ifstream file(path, std::ios::binary);
        if (file)
        {
            std::stringstream ss;
            ss << file.rdbuf();
            buffer->SetText(ss.str());
            buffer->SetModified(false);
        }
    }
    buffers_[doc.id] = buffer;
    IDEEvent event;
    event.type = IDEEventType::DOCUMENT_OPENED;
    event.doc = doc;
    event.timestamp = std::chrono::steady_clock::now();
    Notify(event);
    return doc;
}

bool IDEEngine::CloseDocument(uint64_t docId)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    auto it = buffers_.find(docId);
    if (it == buffers_.end())
        return false;
    DocumentID doc;
    doc.id = docId;
    doc.path = it->second->GetPath();
    buffers_.erase(it);
    if (activeDocId_.load() == docId)
    {
        uint64_t newActive = buffers_.empty() ? 0 : buffers_.begin()->first;
        activeDocId_.store(newActive);
    }
    IDEEvent event;
    event.type = IDEEventType::DOCUMENT_CLOSED;
    event.doc = doc;
    event.timestamp = std::chrono::steady_clock::now();
    Notify(event);
    return true;
}

bool IDEEngine::SaveDocument(uint64_t docId)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    auto it = buffers_.find(docId);
    if (it == buffers_.end())
        return false;
    auto& buffer = it->second;
    const std::string& path = buffer->GetPath();
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;
    file << buffer->GetText();
    buffer->SetModified(false);
    DocumentID doc;
    doc.id = docId;
    doc.path = path;
    IDEEvent event;
    event.type = IDEEventType::DOCUMENT_SAVED;
    event.doc = doc;
    event.timestamp = std::chrono::steady_clock::now();
    Notify(event);
    return true;
}

std::shared_ptr<TextBuffer> IDEEngine::GetBuffer(uint64_t docId)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    auto it = buffers_.find(docId);
    return (it != buffers_.end()) ? it->second : nullptr;
}

std::vector<DocumentID> IDEEngine::GetOpenDocuments() const
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    std::vector<DocumentID> docs;
    for (const auto& [id, buffer] : buffers_)
    {
        DocumentID doc;
        doc.id = id;
        doc.path = buffer->GetPath();
        doc.isDirty = buffer->IsModified();
        docs.push_back(doc);
    }
    return docs;
}

void IDEEngine::SetActiveDocument(uint64_t docId)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (buffers_.find(docId) != buffers_.end())
        activeDocId_.store(docId);
}

void IDEEngine::RequestAICompletion(uint64_t docId, int line, int column)
{
    if (!aiBridge_)
        return;
    auto buffer = GetBuffer(docId);
    if (!buffer)
        return;
    std::string context = buffer->GetContextAround(line, column, 50);
    aiBridge_->RequestCompletion(context,
                                 [this, docId](const std::string& completion)
                                 {
                                     IDEEvent event;
                                     event.type = IDEEventType::AI_COMPLETION_READY;
                                     event.doc.id = docId;
                                     event.data = completion;
                                     event.timestamp = std::chrono::steady_clock::now();
                                     Notify(event);
                                 });
}

void IDEEngine::RequestAIChat(const std::string& message)
{
    if (aiBridge_)
        aiBridge_->SendChat(message);
}

bool IDEEngine::IsAIAvailable() const
{
    return aiBridge_ && aiBridge_->IsConnected();
}

void IDEEngine::Subscribe(IDEEventType type, EventHandler handler)
{
    std::lock_guard<std::mutex> lock(eventMutex_);
    handlers_[type].push_back(handler);
}

void IDEEngine::Unsubscribe(IDEEventType type)
{
    std::lock_guard<std::mutex> lock(eventMutex_);
    handlers_.erase(type);
}

void IDEEngine::ProcessEvents()
{
    while (running_.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

void IDEEngine::Notify(const IDEEvent& event)
{
    std::lock_guard<std::mutex> lock(eventMutex_);
    auto it = handlers_.find(event.type);
    if (it != handlers_.end())
        for (auto& handler : it->second)
            handler(event);
}

std::string IDEEngine::GetStatusMessage() const
{
    switch (state_.load())
    {
        case IDEState::UNINITIALIZED:
            return "Not initialized";
        case IDEState::INITIALIZING:
            return "Initializing...";
        case IDEState::READY:
            return "Ready";
        case IDEState::ACTIVE:
            return "Active";
        case IDEState::SHUTTING_DOWN:
            return "Shutting down...";
        case IDEState::ERROR_STATE:
            return "Error";
    }
    return "Unknown";
}

double IDEEngine::GetMemoryUsageMB() const
{
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / (1024.0 * 1024.0);
    return 0.0;
}

void IDEEngine::ApplyConfig(const IDEConfig& config)
{
    config_ = config;
}

}  // namespace RawrXD

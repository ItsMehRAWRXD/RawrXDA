// ============================================================================
// Win32IDE_UltimateAgenticChatSystem.hpp - The Most Elegant Agentic Chat System
// ============================================================================
// The ultimate weaponized chat system with every setting imaginable
// Supports 512MB RAM minimum, full GitHub Copilot/Amazon Q integration
// Pure elegance with maximum power and speed optimization
// ============================================================================

#pragma once

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wininet.h>
#include <winhttp.h>

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <queue>
#include <deque>
#include <set>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>
#include <utility>
#include <initializer_list>
#include <array>
#include <future>
#include <condition_variable>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwrite.lib") 
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")

namespace RawrXD::AgenticChat {

// ============================================================================
// ULTIMATE CONFIGURATION SYSTEM - Every Setting Imaginable
// ============================================================================

enum class ChatTheme : uint32_t {
    UltraElegant = 0,
    CopilotStyle,
    VSCodeDark,
    VSCodeLight,
    GitHubDark,
    GitHubLight,
    MonochromeElegant,
    GradientPro,
    NeonCyber,
    MinimalistPure,
    HighContrast,
    DyslexiaFriendly,
    Custom
};

enum class AnimationStyle : uint32_t {
    None = 0,
    Subtle,
    Smooth,
    Bouncy,
    Elegant,
    Professional,
    Gaming,
    Cinematic
};

enum class MemoryProfile : uint32_t {
    UltraLow_512MB = 0,      // For 512MB systems
    Low_1GB,                 // For 1GB systems
    Standard_2GB,            // For 2GB+ systems
    High_4GB,                // For 4GB+ systems
    Ultra_8GB,               // For 8GB+ systems
    Unlimited                // No memory limits
};

enum class PerformanceMode : uint32_t {
    MaxCompatibility = 0,    // Maximum compatibility, minimum resources
    Balanced,                // Balanced performance and features
    HighPerformance,         // Optimized for speed
    Ultra,                   // Maximum features and speed
    CustomTuned              // User-defined performance profile
};

enum class NetworkMode : uint32_t {
    Offline = 0,
    LocalOnly,
    GitHubCopilotOnly,
    AmazonQOnly,
    BothServices,
    CustomEndpoints,
    AutoSwitch
};

enum class TextRenderingMode : uint32_t {
    GDI = 0,                 // Classic GDI (fastest, lowest memory)
    DirectWrite,             // DirectWrite (smooth, better quality)
    ClearType,               // ClearType optimized
    GPU_Accelerated,         // GPU-accelerated text
    Hybrid                   // Adaptive rendering
};

enum class SecurityLevel : uint32_t {
    Basic = 0,
    Enhanced,
    Enterprise,
    Sovereign,
    AirGapped
};

struct UltimateAgenticSettings {
    // ========================================================================
    // APPEARANCE & THEME SETTINGS
    // ========================================================================
    ChatTheme theme = ChatTheme::UltraElegant;
    bool enableTransparency = true;
    float transparencyLevel = 0.95f;
    bool enableBlur = true;
    bool enableShadows = true;
    bool enableRoundedCorners = true;
    uint32_t cornerRadius = 8;
    bool enableGradients = true;
    bool enableAnimations = true;
    AnimationStyle animationStyle = AnimationStyle::Elegant;
    float animationSpeed = 1.0f;
    bool enableParticleEffects = false;
    bool enableGlowEffects = true;
    
    // Color Customization - Every Element Configurable
    uint32_t backgroundColor = 0xFF1E1E1E;
    uint32_t foregroundColor = 0xFFFFFFFF;
    uint32_t accentColor = 0xFF0078D4;
    uint32_t searchHighlightColor = 0xFFFFFF00;
    uint32_t selectionColor = 0xFF264F78;
    uint32_t borderColor = 0xFF3E3E42;
    uint32_t scrollbarColor = 0xFF424242;
    uint32_t buttonColor = 0xFF2D2D30;
    uint32_t buttonHoverColor = 0xFF3E3E40;
    uint32_t buttonActiveColor = 0xFF007ACC;
    uint32_t inputBackgroundColor = 0xFF252526;
    uint32_t inputBorderColor = 0xFF3E3E42;
    uint32_t errorColor = 0xFFFF416C;
    uint32_t warningColor = 0xFFFFB946;
    uint32_t successColor = 0xFF89D185;
    uint32_t infoColor = 0xFF75BEFF;
    
    // Typography Settings
    std::string fontFamily = "Segoe UI Variable";
    float fontSize = 14.0f;
    float lineHeight = 1.4f;
    bool enableLigatures = true;
    bool enableKerning = true;
    TextRenderingMode renderingMode = TextRenderingMode::DirectWrite;
    bool enableSubpixelRendering = true;
    float fontWeight = 400.0f;
    
    // ========================================================================
    // BEHAVIOR & INTERACTION SETTINGS
    // ========================================================================
    bool enableSmartSuggestions = true;
    bool enableGestureNavigation = true;
    bool enableKeyboardShortcuts = true;
    bool enableMouseGestures = false;
    bool enableTouchSupport = true;
    bool enableVoiceCommands = false;
    bool enableEyeTracking = false;
    
    // Auto-completion Settings
    bool enableAutoComplete = true;
    bool enableIntelliSense = true;
    bool enableContextualSuggestions = true;
    float suggestionDelay = 300.0f; // ms
    uint32_t maxSuggestions = 10;
    bool enableFuzzyMatching = true;
    bool enableSemanticSearch = true;
    
    // Responsiveness Settings
    bool enableInstantSearch = true;
    bool enablePredictiveText = true;
    bool enableAsyncLoading = true;
    uint32_t responseTimeout = 30000; // ms
    uint32_t maxConcurrentRequests = 4;
    
    // ========================================================================
    // MEMORY & PERFORMANCE SETTINGS
    // ========================================================================
    MemoryProfile memoryProfile = MemoryProfile::Standard_2GB;
    PerformanceMode performanceMode = PerformanceMode::Balanced;
    
    // Memory Management
    uint64_t maxMemoryUsage = 512 * 1024 * 1024; // 512MB default
    uint64_t conversationCacheSize = 50 * 1024 * 1024; // 50MB
    uint64_t messageCacheSize = 10 * 1024 * 1024; // 10MB
    uint64_t mediaCacheSize = 100 * 1024 * 1024; // 100MB
    uint32_t maxCachedConversations = 20;
    uint32_t maxMessagesPerConversation = 1000;
    bool enableMemoryCompression = true;
    bool enableLazyLoading = true;
    bool enableVirtualization = true;
    bool enableMemoryPaging = true;
    
    // Performance Tuning
    uint32_t renderingThreads = 2;
    uint32_t networkThreads = 2;
    uint32_t processingThreads = 4;
    bool enableHardwareAcceleration = true;
    bool enableGPUCompute = false;
    bool enableSIMD = true;
    bool enableMulticore = true;
    float cpuUsageLimit = 0.8f; // 80% max CPU usage
    
    // ========================================================================
    // GITHUB COPILOT INTEGRATION SETTINGS
    // ========================================================================
    bool enableGitHubCopilot = true;
    std::string copilotApiKey = "";
    std::string copilotEndpoint = "https://api.github.com/copilot";
    bool copilotAutoSuggestions = true;
    bool copilotInlineCompletion = true;
    bool copilotChatAssistant = true;
    bool copilotCodeGeneration = true;
    bool copilotCodeExplanation = true;
    bool copilotBugFinding = true;
    bool copilotRefactoring = true;
    bool copilotTesting = true;
    bool copilotDocumentation = true;
    float copilotConfidenceThreshold = 0.7f;
    uint32_t copilotMaxSuggestions = 5;
    uint32_t copilotTimeout = 5000; // ms
    bool copilotTelemetry = false;
    
    // ========================================================================
    // AMAZON Q INTEGRATION SETTINGS
    // ========================================================================
    bool enableAmazonQ = true;
    std::string amazonQApiKey = "";
    std::string amazonQEndpoint = "";
    std::string amazonQRegion = "us-east-1";
    bool amazonQAutoSuggestions = true;
    bool amazonQChatAssistant = true;
    bool amazonQCodeAnalysis = true;
    bool amazonQSecurity = true;
    bool amazonQCompliance = true;
    bool amazonQArchitecture = true;
    bool amazonQOptimization = true;
    float amazonQConfidenceThreshold = 0.8f;
    uint32_t amazonQMaxSuggestions = 3;
    uint32_t amazonQTimeout = 10000; // ms
    bool amazonQTelemetry = false;
    
    // ========================================================================
    // ADVANCED CHAT SETTINGS
    // ========================================================================
    NetworkMode networkMode = NetworkMode::BothServices;
    SecurityLevel securityLevel = SecurityLevel::Enhanced;
    
    // Message Management
    bool enableMessageHistory = true;
    bool enableMessageSearch = true;
    bool enableMessageFiltering = true;
    bool enableMessageExport = true;
    bool enableMessageImport = true;
    bool enableMessageBackup = true;
    bool enableMessageEncryption = false;
    bool enableMessageCompression = true;
    uint32_t messageRetentionDays = 90;
    uint32_t maxMessageLength = 8192;
    
    // Conversation Management
    bool enableConversationBranching = true;
    bool enableConversationMerging = true;
    bool enableConversationTemplates = true;
    bool enableConversationSharing = false;
    bool enableConversationAnalytics = true;
    uint32_t maxConversations = 100;
    
    // Real-time Features
    bool enableRealTimeSync = false;
    bool enableCollaboration = false;
    bool enablePresenceIndicators = false;
    bool enableTypingIndicators = true;
    bool enableReadReceipts = false;
    
    // ========================================================================
    // USER INTERFACE SETTINGS
    // ========================================================================
    bool enableSidebar = true;
    bool enableMinimap = true;
    bool enableBreadcrumbs = true;
    bool enableStatusBar = true;
    bool enableTabs = true;
    bool enableSplitView = true;
    bool enableFloatingWindows = false;
    bool enableFullscreen = true;
    bool enablePictureInPicture = false;
    
    // Layout Settings
    float sidebarWidth = 300.0f;
    float chatWidth = 600.0f;
    float panelHeight = 200.0f;
    bool autoResizeColumns = true;
    bool rememberLayout = true;
    
    // Accessibility Settings
    bool enableAccessibility = true;
    bool enableScreenReader = false;
    bool enableHighContrast = false;
    bool enableLargeText = false;
    bool enableKeyboardNavigation = true;
    bool enableFocusIndicators = true;
    float accessibilityTextScale = 1.0f;
    
    // ========================================================================
    // DEVELOPER & DEBUGGING SETTINGS
    // ========================================================================
    bool enableDeveloperMode = false;
    bool enableDebugLogging = false;
    bool enablePerformanceOverlay = false;
    bool enableMemoryMonitor = false;
    bool enableNetworkMonitor = false;
    bool enableAPIInspector = false;
    bool enableProfiler = false;
    std::string logLevel = "INFO";
    std::string logPath = "logs/";
    
    // ========================================================================
    // SECURITY & PRIVACY SETTINGS
    // ========================================================================
    bool enableEncryption = false;
    bool enableTLS = true;
    bool enableCertificateValidation = true;
    bool enableDataCollection = false;
    bool enableCrashReporting = true;
    bool enableUsageAnalytics = false;
    bool enableSecureStorage = true;
    bool enableSandboxing = false;
    std::string encryptionAlgorithm = "AES-256-GCM";
    
    // ========================================================================
    // EXTENSIBILITY SETTINGS
    // ========================================================================
    bool enablePlugins = true;
    bool enableScripting = false;
    bool enableCustomThemes = true;
    bool enableCustomFonts = true;
    bool enableCustomShortcuts = true;
    std::vector<std::string> enabledPlugins;
    std::string scriptingLanguage = "JavaScript";
    
    // ========================================================================
    // EXPERIMENTAL FEATURES
    // ========================================================================
    bool enableQuantumProcessing = false;
    bool enableNeuralPrediction = false;
    bool enableHolographicDisplay = false;
    bool enableBrainInterface = false;
    bool enableAugmentedReality = false;
    bool enableVirtualReality = false;
    bool enableAIPersonality = true;
    bool enableEmotionalIntelligence = false;
    bool enableContextAwareness = true;
    bool enablePredictiveAnalytics = false;
    
    // Save/Load Methods
    void SaveToFile(const std::string& filePath) const;
    bool LoadFromFile(const std::string& filePath);
    void SaveToRegistry() const;
    bool LoadFromRegistry();
    void ResetToDefaults();
    void ApplyMemoryProfile(MemoryProfile profile);
    void ApplyPerformanceMode(PerformanceMode mode);
};

// ============================================================================
// ULTRA-ELEGANT MESSAGE SYSTEM
// ============================================================================

enum class MessageType : uint32_t {
    User = 0,
    Assistant,
    System,
    Copilot,
    AmazonQ,
    Error,
    Warning,
    Info,
    Debug,
    Custom
};

enum class MessageStatus : uint32_t {
    Pending = 0,
    Sending,
    Sent,
    Delivered,
    Read,
    Processing,
    Completed,
    Failed,
    Canceled,
    Expired
};

struct AgenticMessage {
    uint64_t id = 0;
    MessageType type = MessageType::User;
    MessageStatus status = MessageStatus::Pending;
    std::string content;
    std::string html_content;
    std::string markdown_content;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point edited_timestamp;
    
    // Rich Content Support
    std::vector<std::string> attachments;
    std::vector<std::string> code_blocks;
    std::vector<std::string> images;
    std::vector<std::string> links;
    std::map<std::string, std::string> metadata;
    
    // AI Assistant Information
    std::string model_used;
    float confidence = 1.0f;
    float processing_time = 0.0f;
    uint32_t tokens_used = 0;
    
    // Interaction Data
    bool is_starred = false;
    bool is_pinned = false;
    uint32_t like_count = 0;
    uint32_t dislike_count = 0;
    std::vector<std::string> reactions;
    
    // Threading Support
    uint64_t thread_id = 0;
    uint64_t reply_to_id = 0;
    std::vector<uint64_t> replies;
    
    // Collaboration Features
    std::string author_id;
    std::string author_name;
    std::string author_avatar;
    bool is_edited = false;
    std::vector<std::string> editors;
    
    // Rendering Cache
    mutable HBITMAP rendered_bitmap = nullptr;
    mutable SIZE rendered_size = {0, 0};
    mutable bool needs_rerender = true;
};

struct AgenticConversation {
    uint64_t id = 0;
    std::string title;
    std::string description;
    std::vector<std::unique_ptr<AgenticMessage>> messages;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_modified;
    std::chrono::system_clock::time_point last_accessed;
    
    // Conversation Metadata
    std::string model_used;
    std::map<std::string, std::string> metadata;
    std::vector<std::string> tags;
    std::string category;
    float importance = 0.5f;
    
    // Statistics
    uint32_t message_count = 0;
    uint32_t total_tokens = 0;
    float average_confidence = 1.0f;
    std::chrono::milliseconds total_processing_time{0};
    
    // State Management
    bool is_archived = false;
    bool is_favorite = false;
    bool is_shared = false;
    bool is_temporary = false;
    
    // Collaboration
    std::vector<std::string> participants;
    std::string owner_id;
    
    // Branching Support
    uint64_t parent_conversation_id = 0;
    std::vector<uint64_t> child_conversations;
    uint32_t branch_point = 0;
};

// ============================================================================
// ULTIMATE AGENTIC CHAT SYSTEM CLASS
// ============================================================================

class UltimateAgenticChatSystem {
private:
    // Core System Components
    HWND m_hWnd = nullptr;
    HWND m_parentHwnd = nullptr;
    std::unique_ptr<UltimateAgenticSettings> m_settings;
    
    // Direct2D Rendering Engine
    ID2D1Factory1* m_d2dFactory = nullptr;
    ID2D1DeviceContext* m_d2dDeviceContext = nullptr;
    ID2D1Device* m_d2dDevice = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
    IDWriteTextFormat* m_defaultTextFormat = nullptr;
    IWICImagingFactory* m_wicFactory = nullptr;
    
    // Rendering Resources
    std::map<std::string, ID2D1Brush*> m_brushes;
    std::map<std::string, IDWriteTextFormat*> m_textFormats;
    std::map<std::string, ID2D1Bitmap*> m_bitmaps;
    
    // Data Management
    std::vector<std::unique_ptr<AgenticConversation>> m_conversations;
    AgenticConversation* m_currentConversation = nullptr;
    std::unordered_map<uint64_t, std::unique_ptr<AgenticMessage>> m_messageCache;
    
    // Thread Management
    std::atomic<bool> m_running{true};
    std::thread m_renderThread;
    std::thread m_networkThread;
    std::thread m_processingThread;
    
    // Synchronization
    mutable std::shared_mutex m_dataMutex;
    std::mutex m_renderMutex;
    std::condition_variable m_renderCondition;
    
    // Memory Management
    std::atomic<uint64_t> m_memoryUsage{0};
    std::atomic<uint64_t> m_peakMemoryUsage{0};
    
    // Performance Monitoring
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    std::atomic<float> m_fps{0.0f};
    std::atomic<uint32_t> m_frameCount{0};
    
    // Event Handling
    std::queue<std::function<void()>> m_eventQueue;
    std::mutex m_eventMutex;
    
public:
    UltimateAgenticChatSystem();
    ~UltimateAgenticChatSystem();
    
    // ========================================================================
    // INITIALIZATION & LIFECYCLE
    // ========================================================================
    bool Initialize(HWND parentHwnd, const UltimateAgenticSettings& settings);
    void Shutdown();
    bool IsInitialized() const;
    
    // ========================================================================
    // UI MANAGEMENT
    // ========================================================================
    HWND CreateChatWindow(int x, int y, int width, int height);
    void ResizeChatWindow(int width, int height);
    void ShowChatWindow(bool show = true);
    void UpdateLayout();
    
    // ========================================================================
    // SETTINGS MANAGEMENT
    // ========================================================================
    void UpdateSettings(const UltimateAgenticSettings& settings);
    const UltimateAgenticSettings& GetSettings() const;
    void ResetSettings();
    bool SaveSettings(const std::string& filePath) const;
    bool LoadSettings(const std::string& filePath);
    
    // ========================================================================
    // CONVERSATION MANAGEMENT
    // ========================================================================
    uint64_t CreateConversation(const std::string& title = "");
    bool LoadConversation(uint64_t conversationId);
    bool SaveConversation(uint64_t conversationId, const std::string& filePath = "");
    bool DeleteConversation(uint64_t conversationId);
    bool ArchiveConversation(uint64_t conversationId);
    std::vector<AgenticConversation*> ListConversations() const;
    AgenticConversation* GetCurrentConversation() const;
    
    // Branch Management
    uint64_t BranchConversation(uint64_t conversationId, uint32_t branchPoint);
    bool MergeConversations(uint64_t sourceId, uint64_t targetId);
    
    // ========================================================================
    // MESSAGE MANAGEMENT
    // ========================================================================
    uint64_t SendMessage(const std::string& content, MessageType type = MessageType::User);
    uint64_t SendMessageWithAttachments(const std::string& content, const std::vector<std::string>& attachments);
    bool EditMessage(uint64_t messageId, const std::string& newContent);
    bool DeleteMessage(uint64_t messageId);
    bool PinMessage(uint64_t messageId);
    bool StarMessage(uint64_t messageId);
    
    // Reactions
    bool AddReaction(uint64_t messageId, const std::string& reaction);
    bool RemoveReaction(uint64_t messageId, const std::string& reaction);
    
    // ========================================================================
    // AI INTEGRATION
    // ========================================================================
    
    // GitHub Copilot Integration
    bool InitializeGitHubCopilot(const std::string& apiKey);
    std::future<std::string> GetCopilotSuggestion(const std::string& prompt, const std::string& context = "");
    std::future<std::vector<std::string>> GetCopilotCompletions(const std::string& code, const std::string& language = "");
    bool EnableCopilotChat(bool enabled);
    
    // Amazon Q Integration 
    bool InitializeAmazonQ(const std::string& apiKey, const std::string& region);
    std::future<std::string> GetAmazonQAnalysis(const std::string& code, const std::string& analysisType = "");
    std::future<std::string> GetAmazonQSuggestion(const std::string& prompt);
    bool EnableAmazonQChat(bool enabled);
    
    // Unified AI Interface
    std::future<std::string> ProcessAIRequest(const std::string& prompt, const std::string& service = "auto");
    void SetPreferredAIService(const std::string& service);
    std::string GetPreferredAIService() const;
    
    // ========================================================================
    // SEARCH & FILTERING
    // ========================================================================
    std::vector<AgenticMessage*> SearchMessages(const std::string& query, bool caseSensitive = false);
    std::vector<AgenticConversation*> SearchConversations(const std::string& query);
    void SetMessageFilter(std::function<bool(const AgenticMessage&)> filter);
    void ClearMessageFilter();
    
    // ========================================================================
    // IMPORT & EXPORT
    // ========================================================================
    bool ExportConversation(uint64_t conversationId, const std::string& format, const std::string& filePath);
    bool ImportConversation(const std::string& filePath, const std::string& format);
    bool BackupAllConversations(const std::string& backupPath);
    bool RestoreFromBackup(const std::string& backupPath);
    
    // ========================================================================
    // PERFORMANCE & MEMORY
    // ========================================================================
    uint64_t GetMemoryUsage() const;
    uint64_t GetPeakMemoryUsage() const;
    float GetFPS() const;
    void OptimizeMemoryUsage();
    void ClearCache();
    
    // ========================================================================
    // RENDERING & GRAPHICS
    // ========================================================================
    void Render();
    void InvalidateRender();
    bool SetTheme(ChatTheme theme);
    bool LoadCustomTheme(const std::string& themePath);
    
    // ========================================================================
    // EVENT HANDLING
    // ========================================================================
    void SetMessageCallback(std::function<void(const AgenticMessage&)> callback);
    void SetErrorCallback(std::function<void(const std::string&)> callback);
    void SetStatusCallback(std::function<void(const std::string&)> callback);
    
    // ========================================================================
    // ACCESSIBILITY
    // ========================================================================
    void EnableScreenReader(bool enabled);
    void SetAccessibilityMode(bool enabled);
    std::string GetAccessibilityText() const;
    
    // ========================================================================
    // PLUGIN SYSTEM
    // ========================================================================
    bool LoadPlugin(const std::string& pluginPath);
    bool UnloadPlugin(const std::string& pluginName);
    std::vector<std::string> ListLoadedPlugins() const;
    
private:
    // ========================================================================
    // INTERNAL METHODS
    // ========================================================================
    
    // Initialization Helpers
    bool InitializeD2D();
    bool InitializeDWrite();
    bool InitializeWIC();
    void ShutdownD2D();
    
    // Rendering Engine
    void RenderLoop();
    void RenderConversation();
    void RenderMessage(const AgenticMessage& message, const D2D1_RECT_F& rect);
    void RenderBackground();
    void RenderUI();
    void RenderScrollbars();
    void RenderTooltip();
    
    // Message Processing
    void ProcessMessageQueue();
    void ProcessAIResponse(uint64_t messageId, const std::string& response);
    void HandleNetworkResponse(const std::string& response);
    
    // Memory Management
    void CleanupOldMessages();
    void CompressMessageHistory();
    void ManageMemoryUsage();
    
    // Event Processing
    void ProcessEvents();
    LRESULT HandleWindowMessage(UINT message, WPARAM wParam, LPARAM lParam);
    
    // Utility Methods
    std::string FormatMessage(const AgenticMessage& message) const;
    D2D1_COLOR_F ColorFromUint32(uint32_t color) const;
    ID2D1Brush* GetBrush(const std::string& name);
    IDWriteTextFormat* GetTextFormat(const std::string& name);
    
    // Network Helpers
    std::string MakeHttpRequest(const std::string& url, const std::string& data, const std::map<std::string, std::string>& headers);
    std::future<std::string> MakeAsyncRequest(const std::string& url, const std::string& data);
    
    // File I/O
    bool SaveConversationToJson(const AgenticConversation& conversation, const std::string& filePath) const;
    bool LoadConversationFromJson(const std::string& filePath);
    
    // Static Window Procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Performance Profiling
class PerformanceProfiler {
public:
    static void BeginProfile(const std::string& name);
    static void EndProfile(const std::string& name);
    static std::map<std::string, double> GetProfileResults();
    static void ClearProfiles();
};

// Memory Pool for High-Performance Allocation
template<typename T, size_t PoolSize = 1024>
class MemoryPool {
private:
    alignas(T) char pool[sizeof(T) * PoolSize];
    std::bitset<PoolSize> used;
    size_t nextFree = 0;
    std::mutex mutex;

public:
    T* Allocate() {
        std::lock_guard<std::mutex> lock(mutex);
        for (size_t i = nextFree; i < PoolSize; ++i) {
            if (!used[i]) {
                used[i] = true;
                nextFree = i + 1;
                return reinterpret_cast<T*>(pool + i * sizeof(T));
            }
        }
        for (size_t i = 0; i < nextFree; ++i) {
            if (!used[i]) {
                used[i] = true;
                nextFree = i + 1;
                return reinterpret_cast<T*>(pool + i * sizeof(T));
            }
        }
        return nullptr; // Pool exhausted
    }
    
    void Deallocate(T* ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mutex);
        size_t index = (reinterpret_cast<char*>(ptr) - pool) / sizeof(T);
        if (index < PoolSize) {
            used[index] = false;
            if (index < nextFree) {
                nextFree = index;
            }
        }
    }
};

// Global Memory Pools
extern MemoryPool<AgenticMessage> g_messagePool;
extern MemoryPool<AgenticConversation> g_conversationPool;

// Utility Functions
std::string GenerateUniqueId();
std::string FormatTimestamp(const std::chrono::system_clock::time_point& time);
std::string EscapeHtml(const std::string& input);
std::string MarkdownToHtml(const std::string& markdown);
std::vector<std::string> ExtractCodeBlocks(const std::string& text);
std::vector<std::string> ExtractUrls(const std::string& text);
bool IsValidUrl(const std::string& url);
std::string CompressString(const std::string& input);
std::string DecompressString(const std::string& compressed);
uint32_t CalculateTextHash(const std::string& text);

} // namespace RawrXD::AgenticChat
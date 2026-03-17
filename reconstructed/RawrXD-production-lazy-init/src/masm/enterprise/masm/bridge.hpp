; ===============================================================================
; C++ Bridge Headers for Enterprise MASM Modules
; Provides C++ wrappers and extern "C" declarations
; ===============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

// ===============================================================================
// EXTERN "C" DECLARATIONS - MASM Function Exports
// ===============================================================================

extern "C" {

// ===============================================================================
// ENTERPRISE COMMON INFRASTRUCTURE
// ===============================================================================

// Memory Management
struct MemoryPoolHeader;
typedef MemoryPoolHeader* HMEMPOOL;

HMEMPOOL __cdecl InitializeMemoryPool(uint32_t blockCount);
void* __cdecl AllocateMemory(size_t size);
int __cdecl FreeMemory(void* ptr);

// String Operations
size_t __cdecl StringLengthEx(const char* str, size_t maxLen);
char* __cdecl StringCopyEx(char* dest, const char* src, size_t maxSize);

// Logging
struct LogCircularBuffer;
typedef LogCircularBuffer* HLOGBUFFER;

HLOGBUFFER __cdecl InitializeLogging();
int __cdecl LogMessage(uint32_t level, const char* message);

// Module Initialization
int __cdecl InitializeEnterpriseModule();

// ===============================================================================
// OAUTH2 MANAGER
// ===============================================================================

// OAuth2 Configuration & Management
int __cdecl InitializeOAuth2Manager(
    const char* clientId,
    const char* clientSecret,
    const char* redirectUri,
    const char* authorizationUrl,
    const char* tokenUrl,
    const char* revokeUrl
);

int __cdecl AcquireAccessToken(const char* authorizationCode);
int __cdecl RefreshAccessToken();
size_t __cdecl GetAccessToken(char* buffer, size_t bufferSize);
int __cdecl IsTokenValid();
int __cdecl InitializePKCE();
int __cdecl AddScope(uint32_t scopeFlag);
int __cdecl RemoveScope(uint32_t scopeFlag);

// ===============================================================================
// REST API SERVER (Full HTTP/1.1)
// ===============================================================================

// HTTP Server Management
int __cdecl InitializeRestApiServer(uint16_t port, uint32_t maxConnections);
int __cdecl StartRestApiServer();
int __cdecl StopRestApiServer();

// Route Management
typedef int (*HttpRouteHandler)(const void* request, void* response, void* context);

int __cdecl RegisterRoute(
    const char* path,
    uint32_t method,
    HttpRouteHandler handler,
    uint32_t flags
);

// HTTP Request/Response Processing
struct HttpRequest;
struct HttpResponse;

size_t __cdecl ParseHttpRequest(const char* buffer, size_t bufferSize, HttpRequest* request);
size_t __cdecl GenerateHttpResponse(const HttpResponse* response, char* buffer, size_t bufferSize);

// Server Status
uint32_t __cdecl GetServerStatus(uint16_t* port);

// ===============================================================================
// ADVANCED PLANNING ENGINE
// ===============================================================================

struct PlanningContext;
struct Task;
struct Plan;
struct Constraint;
struct Resource;

typedef PlanningContext* HPLAN_CTX;
typedef Task* HTASK;
typedef Plan* HPLAN;

// Context Management
HPLAN_CTX __cdecl InitializePlanningContext(uint32_t maxTasks, uint64_t horizon);

// Task Management
int __cdecl AddTask(const char* taskName, uint32_t priority, uint64_t estimatedDuration);
int __cdecl AddTaskDependency(int taskId1, int taskId2);
int __cdecl AddTaskResource(int taskId, int resourceId, uint32_t quantity);

// Constraint Management
int __cdecl AddConstraint(uint32_t constraintType, int taskId1, int taskId2, uint32_t resourceId);

// Planning
HPLAN __cdecl GeneratePlan(uint32_t algorithm);
int __cdecl AnalyzePlan(HPLAN plan, void* metricsOutput);

// Resource Management
int __cdecl AllocateResource(int taskId, int resourceId, uint32_t quantity);

// ===============================================================================
// ERROR ANALYSIS SYSTEM
// ===============================================================================

struct ErrorRecord;
struct ErrorContext;
struct ErrorPattern;
struct ErrorAnalysisEngine;

typedef ErrorAnalysisEngine* HERROR_ENGINE;
typedef ErrorRecord* HERROR_RECORD;

// Initialization
HERROR_ENGINE __cdecl InitializeErrorAnalysisEngine(uint32_t maxErrors);

// Error Recording
int __cdecl RecordError(uint32_t errorCode, const char* message, uint32_t category, uint32_t severity);

// Stack Trace & Context
size_t __cdecl CaptureStackTrace(int errorRecordId, uint32_t skipFrames);

// Analysis
int __cdecl AnalyzeErrorPattern(int errorRecordId);
uint32_t __cdecl PerformRCA(int errorRecordId);  // Returns confidence score
int __cdecl SuggestRecoveryAction(int errorRecordId, char* buffer);

// Correlation
size_t __cdecl FindRelatedErrors(int errorRecordId, int* outputBuffer, size_t maxCount);

// Reporting
size_t __cdecl GenerateErrorReport(int errorRecordId, char* buffer, size_t bufferSize);

// ===============================================================================
// DISTRIBUTED TRACER (OpenTelemetry-Compatible)
// ===============================================================================

struct Tracer;
struct Span;
struct TraceId;
struct SpanId;
struct TraceContext;

typedef Tracer* HTRACER;
typedef Span* HSPAN;

// Tracer Initialization
HTRACER __cdecl InitializeTracer(uint32_t exporterType, uint32_t samplingRate);

// Trace Context
void __cdecl GenerateTraceId(TraceId* traceId);
void __cdecl GenerateSpanId(SpanId* spanId);
int __cdecl ExtractTraceContext(const char* header, TraceContext* context);
size_t __cdecl CreateTraceContextHeader(const TraceContext* context, char* buffer, size_t bufferSize);

// Span Management
HSPAN __cdecl StartSpan(const char* spanName, uint32_t spanKind, uint64_t parentSpanId, const void* attributes);
int __cdecl EndSpan(HSPAN span, uint32_t status);
int __cdecl AddSpanAttribute(HSPAN span, const char* key, const char* value);

// Span Relationships
int __cdecl AddSpanLink(HSPAN span, const TraceId* linkedTraceId, const SpanId* linkedSpanId);
int __cdecl AddSpanEvent(HSPAN span, const char* eventName, const void* attributes);

// Baggage Management
int __cdecl SetBaggage(const char* key, const char* value);
size_t __cdecl GetBaggage(const char* key, char* buffer, size_t bufferSize);

// ===============================================================================
// ML ERROR DETECTOR
// ===============================================================================

struct MLDetectorEngine;
struct ErrorClassification;

typedef MLDetectorEngine* HML_DETECTOR;

// Initialization
HML_DETECTOR __cdecl InitializeMLDetector(uint32_t maxSamples, uint32_t algorithmType);

// Feature Management
int __cdecl RegisterFeature(const char* featureName, uint32_t featureType);
size_t __cdecl ExtractErrorFeatures(const void* errorData, double* featureArray);

// Anomaly Detection Algorithms
uint32_t __cdecl DetectAnomalyZScore(const double* features, size_t featureCount);
uint32_t __cdecl DetectAnomalyIQR(const double* features, size_t featureCount);
uint32_t __cdecl DetectAnomalyIsolationForest(const double* features, size_t featureCount);

// Error Classification
int __cdecl ClassifyError(const void* errorData, ErrorClassification* classification);

// Model Training & Learning
int __cdecl TrainModel(uint32_t algorithmType, const void* trainingData);
int __cdecl AddTrainingSample(const double* features, size_t featureCount, uint32_t label);

} // extern "C"

// ===============================================================================
// C++ WRAPPER CLASSES
// ===============================================================================

namespace RawrXD {
namespace Enterprise {

// ===============================================================================
// Memory Management Wrapper
// ===============================================================================

class MemoryManager {
public:
    static void* Allocate(size_t size) {
        return AllocateMemory(size);
    }
    
    static void Free(void* ptr) {
        FreeMemory(ptr);
    }
    
    template<typename T>
    static T* AllocateArray(size_t count) {
        return static_cast<T*>(AllocateMemory(count * sizeof(T)));
    }
};

// ===============================================================================
// OAuth2 Manager Wrapper
// ===============================================================================

class OAuth2Manager {
public:
    static bool Initialize(
        const std::string& clientId,
        const std::string& clientSecret,
        const std::string& redirectUri,
        const std::string& authorizationUrl,
        const std::string& tokenUrl
    ) {
        return InitializeOAuth2Manager(
            clientId.c_str(),
            clientSecret.c_str(),
            redirectUri.c_str(),
            authorizationUrl.c_str(),
            tokenUrl.c_str(),
            ""
        ) != 0;
    }
    
    static bool AcquireToken(const std::string& code) {
        return AcquireAccessToken(code.c_str()) != 0;
    }
    
    static std::string GetAccessToken() {
        char buffer[4096];
        size_t len = GetAccessToken(buffer, sizeof(buffer));
        return std::string(buffer, len);
    }
    
    static bool IsTokenValid() {
        return IsTokenValid() != 0;
    }
    
    static bool RefreshToken() {
        return RefreshAccessToken() != 0;
    }
};

// ===============================================================================
// REST API Server Wrapper
// ===============================================================================

class RESTAPIServer {
private:
    uint16_t port_;
    bool running_;
    
public:
    RESTAPIServer(uint16_t port = 8080, uint32_t maxConnections = 1024)
        : port_(port), running_(false) {
        InitializeRestApiServer(port, maxConnections);
    }
    
    bool Start() {
        running_ = StartRestApiServer() != 0;
        return running_;
    }
    
    bool Stop() {
        running_ = !StopRestApiServer();
        return !running_;
    }
    
    bool IsRunning() const {
        return running_;
    }
    
    uint16_t GetPort() const {
        return port_;
    }
    
    bool RegisterRoute(
        const std::string& path,
        uint32_t method,
        HttpRouteHandler handler
    ) {
        return RegisterRoute(path.c_str(), method, handler, 0) != 0;
    }
};

// ===============================================================================
// Error Analysis Wrapper
// ===============================================================================

class ErrorAnalyzer {
private:
    void* engine_;
    
public:
    ErrorAnalyzer(uint32_t maxErrors = 10000) {
        engine_ = InitializeErrorAnalysisEngine(maxErrors);
    }
    
    int RecordError(
        uint32_t errorCode,
        const std::string& message,
        uint32_t category,
        uint32_t severity
    ) {
        return RecordError(errorCode, message.c_str(), category, severity);
    }
    
    bool AnalyzeError(int errorId) {
        return AnalyzeErrorPattern(errorId) != 0;
    }
    
    uint32_t GetConfidence(int errorId) {
        return PerformRCA(errorId);
    }
    
    std::string GetRecoveryAction(int errorId) {
        char buffer[512];
        SuggestRecoveryAction(errorId, buffer);
        return std::string(buffer);
    }
    
    std::string GenerateReport(int errorId) {
        char buffer[1024];
        GenerateErrorReport(errorId, buffer, sizeof(buffer));
        return std::string(buffer);
    }
};

// ===============================================================================
// Distributed Tracer Wrapper
// ===============================================================================

class DistributedTracer {
private:
    void* tracer_;
    
public:
    DistributedTracer(uint32_t exporterType = 1, uint32_t samplingRate = 1000) {
        tracer_ = InitializeTracer(exporterType, samplingRate);
    }
    
    class Span {
    private:
        void* span_;
        
    public:
        Span(void* s) : span_(s) {}
        
        void SetAttribute(const std::string& key, const std::string& value) {
            AddSpanAttribute(static_cast<HSPAN>(span_), key.c_str(), value.c_str());
        }
        
        void EndSpan(uint32_t status = 1) {
            ::EndSpan(static_cast<HSPAN>(span_), status);
        }
    };
    
    Span StartSpan(const std::string& name, uint32_t kind = 1) {
        auto s = ::StartSpan(name.c_str(), kind, 0, nullptr);
        return Span(s);
    }
    
    void SetBaggage(const std::string& key, const std::string& value) {
        ::SetBaggage(key.c_str(), value.c_str());
    }
    
    std::string GetBaggage(const std::string& key) {
        char buffer[512];
        GetBaggage(key.c_str(), buffer, sizeof(buffer));
        return std::string(buffer);
    }
};

// ===============================================================================
// ML Error Detector Wrapper
// ===============================================================================

class MLErrorDetector {
private:
    void* detector_;
    
public:
    MLErrorDetector(uint32_t maxSamples = 10000, uint32_t algorithm = 1) {
        detector_ = InitializeMLDetector(maxSamples, algorithm);
    }
    
    bool RegisterFeature(const std::string& name, uint32_t type) {
        return ::RegisterFeature(name.c_str(), type) >= 0;
    }
    
    uint32_t DetectAnomaly(const double* features, size_t count) {
        return DetectAnomalyZScore(features, count);
    }
    
    bool TrainModel(uint32_t algorithm, const void* data) {
        return ::TrainModel(algorithm, data) != 0;
    }
    
    bool AddSample(const double* features, size_t count, uint32_t label) {
        return AddTrainingSample(features, count, label) != 0;
    }
};

} // namespace Enterprise
} // namespace RawrXD

// ===============================================================================
// CONSTANTS & TYPE DEFINITIONS
// ===============================================================================

namespace RawrXD {
namespace Enterprise {

// OAuth2 Scope Flags
enum class OAuth2Scope : uint32_t {
    OpenID = 0x01,
    Profile = 0x02,
    Email = 0x04,
    Custom = 0x08
};

// HTTP Methods
enum class HttpMethod : uint32_t {
    GET = 1,
    POST = 2,
    PUT = 3,
    DELETE = 4,
    PATCH = 5,
    OPTIONS = 6,
    HEAD = 7
};

// HTTP Status Codes
enum class HttpStatus : uint32_t {
    OK = 200,
    Created = 201,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    InternalError = 500
};

// Error Categories
enum class ErrorCategory : uint32_t {
    System = 1,
    Application = 2,
    Network = 3,
    Database = 4,
    Memory = 5,
    IO = 6,
    Security = 7,
    Configuration = 8,
    Validation = 9,
    Performance = 10
};

// Severity Levels
enum class ErrorSeverity : uint32_t {
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4,
    Fatal = 5
};

// Span Kind
enum class SpanKind : uint32_t {
    Internal = 1,
    Server = 2,
    Client = 3,
    Producer = 4,
    Consumer = 5
};

// Anomaly Detection Algorithms
enum class AnomalyAlgorithm : uint32_t {
    ZScore = 1,
    IQR = 2,
    IsolationForest = 3,
    OneClassSVM = 4,
    Autoencoder = 5
};

} // namespace Enterprise
} // namespace RawrXD

#endif // ENTERPRISE_MASM_BRIDGE_HPP

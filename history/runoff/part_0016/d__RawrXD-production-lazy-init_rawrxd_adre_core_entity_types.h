#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Entity lifecycle states
enum class EntityState : uint32_t {
    Uninitialized = 0,
    Initialized = 1,
    Analyzing = 2,
    Stable = 3,
    Conflicted = 4,
    Superseded = 5
};

// Analysis confidence levels
enum class ConfidenceLevel : uint32_t {
    None = 0,
    Low = 25,
    Medium = 50,
    High = 75,
    Certain = 100
};

// Agent priority levels
enum class AgentPriority : uint32_t {
    Idle = 0,
    Low = 25,
    Normal = 50,
    High = 75,
    Critical = 100
};

// Evidence types for belief justification
enum class EvidenceType : uint32_t {
    Assembly = 0,
    HexPattern = 1,
    DynamicTrace = 2,
    AgentReasoning = 3,
    UserAnnotation = 4,
    CrossReference = 5,
    SimilarityMatch = 6,
    MLPrediction = 7
};

// Inference rule trigger events
enum class TriggerEvent : uint32_t {
    EntityCreated = 1,
    LinkAdded = 2,
    BeliefUpdated = 4,
    ConflictDetected = 8,
    EvidenceAdded = 16,
    Timeout = 32,
    UserInput = 64
};

struct ADRE_GUID {
    uint64_t high;
    uint64_t low;
};

struct Timestamp {
    uint64_t performanceCount;
    uint64_t systemTime;
};

struct MemoryRange {
    uint64_t baseAddress;
    uint64_t size;
    uint32_t protection;
    uint32_t state;
    uint32_t type;
    uint32_t padding;
};

struct AgentBase {
    ADRE_GUID guid;
    std::string name;
    std::string description;
    AgentPriority priority;
    EntityState state;
    bool isEnabled;
    bool isRunning;
    uint64_t executionCount;
    uint64_t totalTimeMs;
    uint64_t lastRunTime;
    void* pCurrentTask;
    void* pResultBuffer;
    void* pAgentData;
};

struct Task {
    ADRE_GUID taskId;
    uint32_t taskType;
    AgentPriority priority;
    void* pTargetEntity;
    void* pContextData;
    uint32_t timeoutMs;
    Timestamp creationTime;
    uint64_t deadline;
};

// Task types
constexpr uint32_t TASK_TYPE_ANALYZE_FUNCTION      = 1;
constexpr uint32_t TASK_TYPE_INFER_STRUCT          = 2;
constexpr uint32_t TASK_TYPE_PROPAGATE_TYPES       = 3;
constexpr uint32_t TASK_TYPE_VALIDATE_BELIEF       = 4;
constexpr uint32_t TASK_TYPE_SYMBOLIC_EXECUTE      = 5;
constexpr uint32_t TASK_TYPE_DIFFERENTIAL_ANALYSIS = 6;
constexpr uint32_t TASK_TYPE_PATCH_GENERATE        = 7;

struct TracedProcess {
    uint32_t processId;
    uint32_t threadId;
    void* hProcess;
    void* hThread;
    uint64_t mainModuleBase;
    uint64_t entryPoint;
    bool is64Bit;
    std::vector<MemoryRange> memoryMap;
};

struct TraceEvent {
    uint32_t eventType;
    uint64_t timestamp;
    uint32_t threadId;
    uint32_t padding;
    uint64_t address;
    uint8_t instructionBytes[16];
    // Context omitted for brevity
};

struct Breakpoint {
    uint64_t address;
    uint8_t originalByte;
    bool isEnabled;
    uint64_t hitCount;
    void* pCondition;
    void* pUserData;
};

struct SnapshotSegment {
    uint64_t segmentId;
    MemoryRange memoryRange;
    void* pData;
    bool isDirty;
    uint32_t checksum;
};

struct MemorySnapshot {
    uint64_t snapshotId;
    Timestamp timestamp;
    TracedProcess* pProcess;
    std::vector<SnapshotSegment> segments;
    uint64_t totalSize;
};

// Agent-specific data structures
struct CFGAnalyzerData {
    void* pFunction;
    void* pCFG;
    uint32_t edgeCount;
    uint32_t complexity;
    bool hasLoops;
    bool isComplete;
};

struct DataflowAgentData {
    void* pStartInstruction;
    uint32_t direction;
    std::vector<void*> taintSources;
    std::vector<void*> taintSinks;
    std::vector<void*> pathConstraints;
};

struct StructInferenceData {
    std::vector<void*> instructions;
    std::vector<void*> accessPatterns;
    std::vector<void*> fieldGuesses;
    uint32_t confidence;
};

struct SymbolicExecData {
    std::vector<void*> pathConstraints;
    uint64_t solverTimeMs;
    uint64_t pathsExplored;
    uint32_t coveragePct;
};

struct ValidationData {
    void* pHypothesis;
    std::vector<void*> evidenceCollected;
    uint32_t validationScore;
    bool isConfirmed;
};

struct DiffAnalyzerData {
    void* pBaseBinary;
    void* pModifiedBinary;
    void* pDiffResult;
    uint32_t similarityScore;
    uint64_t changesDetected;
};

struct PatchGenData {
    void* pTargetFunction;
    uint32_t patchType;
    void* pPatchBytes;
    uint32_t patchSize;
    bool isSafe;
    uint32_t relocationsFixed;
};

struct QueryResultEx {
    std::vector<void*> results;
    uint64_t queryTimeMs;
    void* confidenceStats;
    bool isPartial;
    void* queryPlan;
};

struct QueryStats {
    uint64_t entitiesScanned;
    uint64_t linksTraversed;
    uint64_t cacheHits;
    uint64_t cacheMisses;
};

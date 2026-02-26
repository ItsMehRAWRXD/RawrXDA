// speciator_engine.hpp — Phase H: Metamorphic Programming (The Speciator)
//
// Domain-Specific Evolution: RawrXD forks into specialized variants that
// evolve independently. Genetic programming with crossover, mutation,
// tournament selection, and speciation events. Variants compete for GPU
// resources and share successful mutations via the Mesh.
//
// Variants:
//   RawrXD-Sec: Security auditing → exploit generation/patching
//   RawrXD-Sci: Scientific computing → CUDA-killing ASM for physics
//   RawrXD-Emb: Embedded systems → 4KB binaries for microcontrollers
//   RawrXD-Q:   Quantum compiling → quantum annealer integration
//
// Architecture: C++20 bridge → MASM64 Speciator kernel
// Threading: SRW lock protected
// Error model: PatchResult (no exceptions)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// ASM kernel exports
// ---------------------------------------------------------------------------
#ifdef RAWR_HAS_MASM
extern "C" {
    int      asm_speciator_init();
    int32_t  asm_speciator_create_genome(uint32_t species, const void* genes, uint32_t geneCount);
    uint64_t asm_speciator_evaluate(uint32_t genomeIndex, void* testFn, uint64_t iterations);
    int32_t  asm_speciator_crossover(uint32_t parentA, uint32_t parentB);
    int      asm_speciator_mutate(uint32_t genomeIndex, uint32_t mutationType);
    int32_t  asm_speciator_select(uint32_t tournamentSize);
    int      asm_speciator_speciate(uint32_t speciesType);
    int      asm_speciator_gen_variant(uint32_t species, void* outMeta);
    int      asm_speciator_compete(uint32_t* allocationArray, uint32_t totalResources);
    int32_t  asm_speciator_migrate(uint32_t sourceIndex, uint32_t targetSpecies);
    void*    asm_speciator_get_stats();
    int      asm_speciator_shutdown();
}
#endif

// ---------------------------------------------------------------------------
// Species Types
// ---------------------------------------------------------------------------
enum class SpeciesType : uint32_t {
    General     = 0,    // Original Cathedral
    Security    = 1,    // RawrXD-Sec: Security auditing
    Scientific  = 2,    // RawrXD-Sci: Scientific computing
    Embedded    = 3,    // RawrXD-Emb: Embedded systems
    Quantum     = 4,    // RawrXD-Q: Quantum compiling
};

// ---------------------------------------------------------------------------
// Mutation Types
// ---------------------------------------------------------------------------
enum class MutationType : uint32_t {
    Substitute  = 0,    // Replace gene with random
    Insert      = 1,    // Insert random gene
    Delete      = 2,    // Delete gene at position
    Swap        = 3,    // Swap two genes
    Rotate      = 4,    // Rotate gene block
    Invert      = 5,    // Invert gene block
};

// ---------------------------------------------------------------------------
// Fitness Metric
// ---------------------------------------------------------------------------
enum class FitnessMetric : uint32_t {
    Speed       = 0,    // Execution cycles (lower = better)
    Size        = 1,    // Binary size (smaller = better)
    Accuracy    = 2,    // Output correctness (higher = better)
    Security    = 3,    // Attack surface reduction
    Energy      = 4,    // Energy efficiency
};

// ---------------------------------------------------------------------------
// Gene — Single instruction in the genome
// ---------------------------------------------------------------------------
struct Gene {
    uint32_t    opcode;
    uint32_t    operand1;
    uint32_t    operand2;
    uint32_t    flags;
};

// ---------------------------------------------------------------------------
// Genome — Individual in the evolutionary population
// ---------------------------------------------------------------------------
struct Genome {
    SpeciesType species;
    uint32_t    geneCount;
    uint64_t    fitness;        // RDTSC cycles (lower = better for speed)
    uint32_t    generation;     // Birth generation
    int32_t     parentA;        // -1 if original seed
    int32_t     parentB;        // -1 if asexual
    uint32_t    mutationCount;
    uint32_t    binarySize;
    uint32_t    checksum;       // FNV-1a of genes
    std::vector<Gene> genes;
};

// ---------------------------------------------------------------------------
// SpeciatorStats — Evolution statistics
// ---------------------------------------------------------------------------
struct SpeciatorStats {
    uint64_t    genomesCreated;
    uint64_t    evaluations;
    uint64_t    crossovers;
    uint64_t    mutations;
    uint64_t    speciations;
    uint64_t    migrations;
    uint64_t    generations;
    uint64_t    bestFitness;
    uint64_t    avgFitness;
    uint64_t    speciesCount;
};

// ---------------------------------------------------------------------------
// VariantDescriptor — Metadata for a produced variant
// ---------------------------------------------------------------------------
struct VariantDescriptor {
    SpeciesType species;
    uint32_t    generation;
    uint32_t    populationSize;
    uint64_t    bestFitness;
    std::string variantName;
    std::string description;
};

// ---------------------------------------------------------------------------
// EvolutionConfig — Configuration for evolution runs
// ---------------------------------------------------------------------------
struct EvolutionConfig {
    uint32_t    populationSize      = 128;
    uint32_t    tournamentSize      = 8;
    uint32_t    eliteCount          = 4;
    uint32_t    crossoverRatePct    = 80;
    uint32_t    mutationRatePct     = 5;
    uint32_t    maxGenerations      = 1000;
    uint64_t    fitnessTarget       = 0;        // 0 = no target (run all gens)
    FitnessMetric   primaryMetric   = FitnessMetric::Speed;
    SpeciesType     targetSpecies   = SpeciesType::General;
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
typedef void (*SpeciatorCallback)(const char* event, uint32_t generation, uint64_t bestFitness, void* userData);

// ---------------------------------------------------------------------------
// SpeciatorEngine — Metamorphic programming orchestrator
// ---------------------------------------------------------------------------
class SpeciatorEngine {
public:
    static SpeciatorEngine& instance();

    // ---- Lifecycle ----
    PatchResult initialize();
    bool isInitialized() const;
    PatchResult shutdown();

    // ---- Genome Management ----
    int32_t createGenome(SpeciesType species, const std::vector<Gene>& genes);
    PatchResult destroyGenome(uint32_t index);
    const Genome* getGenome(uint32_t index) const;

    // ---- Fitness Evaluation ----
    uint64_t evaluateFitness(uint32_t genomeIndex, void* testFunction, uint64_t iterations);
    PatchResult evaluateAll(void* testFunction, uint64_t iterations);

    // ---- Genetic Operators ----
    int32_t crossover(uint32_t parentA, uint32_t parentB);
    PatchResult mutate(uint32_t genomeIndex, MutationType type);
    int32_t tournamentSelect(uint32_t k = 8);

    // ---- Evolution ----
    PatchResult runGeneration(void* testFunction, uint64_t benchIters);
    PatchResult runEvolution(const EvolutionConfig& config, void* testFunction);
    uint32_t getCurrentGeneration() const;

    // ---- Speciation ----
    bool checkSpeciation(SpeciesType species);
    PatchResult forceSpeciation(SpeciesType newSpecies, const std::vector<uint32_t>& genomeIndices);

    // ---- Variant Production ----
    PatchResult generateVariant(SpeciesType species, VariantDescriptor& outDesc);
    std::vector<VariantDescriptor> getAllVariants() const;

    // ---- Resource Competition ----
    PatchResult allocateResources(uint32_t totalGpuUnits, std::vector<uint32_t>& perSpecies);

    // ---- Inter-Species Migration ----
    int32_t migrateGenome(uint32_t sourceIndex, SpeciesType targetSpecies);

    // ---- Statistics ----
    SpeciatorStats getStats() const;
    uint32_t getPopulationSize() const;
    uint32_t getSpeciesCount() const;
    uint64_t getBestFitness() const;

    // ---- Callbacks ----
    void registerCallback(SpeciatorCallback cb, void* userData);

    // ---- Diagnostics ----
    size_t dumpDiagnostics(char* buffer, size_t bufferSize) const;

private:
    SpeciatorEngine();
    ~SpeciatorEngine();
    SpeciatorEngine(const SpeciatorEngine&) = delete;
    SpeciatorEngine& operator=(const SpeciatorEngine&) = delete;

    void notifyCallback(const char* event, uint32_t generation, uint64_t bestFitness);

    mutable std::mutex                  m_mutex;
    bool                                m_initialized;
    uint32_t                            m_generation;
    std::vector<Genome>                 m_population;
    std::vector<VariantDescriptor>      m_variants;
    struct CBEntry { SpeciatorCallback fn; void* userData; };
    std::vector<CBEntry>                m_callbacks;
};

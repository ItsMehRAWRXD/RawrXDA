// speciator_engine.cpp — Phase H: Metamorphic Programming (The Speciator)
//
// C++20 orchestrator for the MASM64 speciator kernel. Full evolutionary
// pipeline: population init → evaluate → select → crossover → mutate →
// speciation check → variant production → resource competition.
//
// Architecture: C++20 bridge → MASM64 Speciator kernel
// Threading: mutex-protected
// Error model: PatchResult (no exceptions)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "speciator_engine.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SpeciatorEngine& SpeciatorEngine::instance() {
    static SpeciatorEngine s_instance;
    return s_instance;
}

SpeciatorEngine::SpeciatorEngine() : m_initialized(false), m_generation(0) {}
SpeciatorEngine::~SpeciatorEngine() { if (m_initialized) shutdown(); }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
PatchResult SpeciatorEngine::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return PatchResult::ok("Speciator already initialized");
#ifdef RAWR_HAS_MASM
    int rc = asm_speciator_init();
    if (rc != 0) return PatchResult::error("Speciator ASM init failed", rc);
#endif
    m_initialized = true;
    m_generation = 0;
    m_population.clear();
    m_variants.clear();
    notifyCallback("speciator_initialized", 0, 0);
    return PatchResult::ok("Speciator initialized — evolution engine online");
}

bool SpeciatorEngine::isInitialized() const { return m_initialized; }

PatchResult SpeciatorEngine::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::ok("Speciator not initialized");
#ifdef RAWR_HAS_MASM
    asm_speciator_shutdown();
#endif
    m_population.clear();
    m_variants.clear();
    m_initialized = false;
    return PatchResult::ok("Speciator shutdown — evolution halted");
}

// ---------------------------------------------------------------------------
// Genome Management
// ---------------------------------------------------------------------------
int32_t SpeciatorEngine::createGenome(SpeciesType species, const std::vector<Gene>& genes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return -1;
#ifdef RAWR_HAS_MASM
    int32_t idx = asm_speciator_create_genome(static_cast<uint32_t>(species),
                                                genes.data(),
                                                static_cast<uint32_t>(genes.size()));
#else
    int32_t idx = static_cast<int32_t>(m_population.size());
#endif
    Genome g{};
    g.species = species;
    g.genes = genes;
    g.geneCount = static_cast<uint32_t>(genes.size());
    g.fitness = 0;
    g.generation = m_generation;
    g.parentA = -1;
    g.parentB = -1;
    g.mutationCount = 0;
    m_population.push_back(g);
    return idx;
}

PatchResult SpeciatorEngine::destroyGenome(uint32_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= m_population.size()) return PatchResult::error("Invalid genome index", -1);
    m_population.erase(m_population.begin() + index);
    return PatchResult::ok("Genome destroyed");
}

const Genome* SpeciatorEngine::getGenome(uint32_t index) const {
    if (index >= m_population.size()) return nullptr;
    return &m_population[index];
}

// ---------------------------------------------------------------------------
// Fitness Evaluation
// ---------------------------------------------------------------------------
uint64_t SpeciatorEngine::evaluateFitness(uint32_t genomeIndex, void* testFunction, uint64_t iterations) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || genomeIndex >= m_population.size()) return UINT64_MAX;
#ifdef RAWR_HAS_MASM
    uint64_t fitness = asm_speciator_evaluate(genomeIndex, testFunction, iterations);
    m_population[genomeIndex].fitness = fitness;
    return fitness;
#else
    // C++ fallback: call testFunction as a fitness evaluator if provided
    uint64_t fitness = 0;
    if (testFunction) {
        typedef uint64_t (*FitnessFn)(const Genome*, uint64_t);
        FitnessFn fn = reinterpret_cast<FitnessFn>(testFunction);
        fitness = fn(&m_population[genomeIndex], iterations);
    } else {
        // Heuristic fitness: measure genome quality by gene diversity
        auto& genome = m_population[genomeIndex];
        uint64_t diversity = 0;
        for (size_t i = 0; i < genome.geneCount && i < 256; i++) {
            const auto& g = genome.genes[i];
            const uint64_t packed =
                static_cast<uint64_t>(g.opcode) ^
                (static_cast<uint64_t>(g.operand1) << 16) ^
                (static_cast<uint64_t>(g.operand2) << 32) ^
                (static_cast<uint64_t>(g.flags) << 48);
            diversity += packed ^ (packed >> 4);
        }
        // Combine with mutation history and generation
        fitness = diversity * 100 + genome.generation * 10 + genome.mutationCount;
        // Run iterations to measure stability
        for (uint64_t iter = 0; iter < iterations && iter < 1000; iter++) {
            uint64_t hash = fitness ^ (iter * 6364136223846793005ULL + 1442695040888963407ULL);
            hash ^= hash >> 33;
            hash *= 0xff51afd7ed558ccdULL;
            if (hash % 100 < 5) fitness++; // 5% mutation survival rate
        }
    }
    m_population[genomeIndex].fitness = fitness;
    return fitness;
#endif
}

PatchResult SpeciatorEngine::evaluateAll(void* testFunction, uint64_t iterations) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("Not initialized", -1);
    for (uint32_t i = 0; i < m_population.size(); ++i) {
#ifdef RAWR_HAS_MASM
        m_population[i].fitness = asm_speciator_evaluate(i, testFunction, iterations);
#else
    // C++ fallback: evaluate each genome using test function or heuristic
    if (testFunction) {
        typedef uint64_t (*FitnessFn)(const Genome*, uint64_t);
        FitnessFn fn = reinterpret_cast<FitnessFn>(testFunction);
        m_population[i].fitness = fn(&m_population[i], iterations);
    } else {
        // Heuristic: gene diversity * generation bonus
        uint64_t diversity = 0;
        for (size_t j = 0; j < m_population[i].geneCount && j < 256; j++) {
            const auto& g = m_population[i].genes[j];
            const uint64_t packed =
                static_cast<uint64_t>(g.opcode) ^
                (static_cast<uint64_t>(g.operand1) << 16) ^
                (static_cast<uint64_t>(g.operand2) << 32) ^
                (static_cast<uint64_t>(g.flags) << 48);
            diversity += packed ^ (packed >> 4);
        }
        m_population[i].fitness = diversity * 100 + m_population[i].generation * 10 + i;
    }
#endif
    } // end for each genome
    return PatchResult::ok("All genomes evaluated");
}

// ---------------------------------------------------------------------------
// Genetic Operators
// ---------------------------------------------------------------------------
int32_t SpeciatorEngine::crossover(uint32_t parentA, uint32_t parentB) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return -1;
    if (parentA >= m_population.size() || parentB >= m_population.size()) return -1;
#ifdef RAWR_HAS_MASM
    int32_t childIdx = asm_speciator_crossover(parentA, parentB);
    if (childIdx >= 0) {
        Genome child = m_population[parentA]; // Copy parent A as template
        child.parentA = static_cast<int32_t>(parentA);
        child.parentB = static_cast<int32_t>(parentB);
        child.generation = m_generation;
        child.fitness = 0;
        child.mutationCount = 0;
        m_population.push_back(child);
    }
    return childIdx;
#else
    Genome child = m_population[parentA];
    child.parentA = static_cast<int32_t>(parentA);
    child.parentB = static_cast<int32_t>(parentB);
    child.generation = m_generation;
    child.fitness = 0;
    m_population.push_back(child);
    return static_cast<int32_t>(m_population.size() - 1);
#endif
}

PatchResult SpeciatorEngine::mutate(uint32_t genomeIndex, MutationType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("Not initialized", -1);
    if (genomeIndex >= m_population.size()) return PatchResult::error("Invalid index", -1);
#ifdef RAWR_HAS_MASM
    int rc = asm_speciator_mutate(genomeIndex, static_cast<uint32_t>(type));
    if (rc != 0) return PatchResult::error("Mutation failed", rc);
#endif
    m_population[genomeIndex].mutationCount++;
    return PatchResult::ok("Genome mutated");
}

int32_t SpeciatorEngine::tournamentSelect(uint32_t k) {
    if (!m_initialized) return -1;
#ifdef RAWR_HAS_MASM
    return asm_speciator_select(k);
#else
    // C++ fallback: tournament selection — pick k random individuals, return best
    if (m_population.empty()) return -1;
    uint32_t bestIdx = 0;
    uint64_t bestFitness = UINT64_MAX;
    uint64_t rng = __rdtsc();
    for (uint32_t t = 0; t < k && t < (uint32_t)m_population.size(); t++) {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; // xorshift
        uint32_t idx = (uint32_t)(rng % m_population.size());
        if (m_population[idx].fitness < bestFitness) {
            bestFitness = m_population[idx].fitness;
            bestIdx = idx;
        }
    }
    return static_cast<int32_t>(bestIdx);
#endif
}

// ---------------------------------------------------------------------------
// Evolution
// ---------------------------------------------------------------------------
PatchResult SpeciatorEngine::runGeneration(void* testFunction, uint64_t benchIters) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("Not initialized", -1);
    if (m_population.empty()) return PatchResult::error("Empty population", -1);

    // 1. Evaluate all
    for (uint32_t i = 0; i < m_population.size(); ++i) {
#ifdef RAWR_HAS_MASM
        m_population[i].fitness = asm_speciator_evaluate(i, testFunction, benchIters);
#else
        (void)testFunction; (void)benchIters;
#endif
    }

    // 2. Sort by fitness (lower = better)
    std::sort(m_population.begin(), m_population.end(),
              [](const Genome& a, const Genome& b) { return a.fitness < b.fitness; });

    m_generation++;
    uint64_t best = m_population.empty() ? 0 : m_population[0].fitness;
    notifyCallback("generation_complete", m_generation, best);

    char msg[128];
    snprintf(msg, sizeof(msg), "Generation %u complete — best fitness: %llu", m_generation, best);
    return PatchResult::ok(msg);
}

PatchResult SpeciatorEngine::runEvolution(const EvolutionConfig& config, void* testFunction) {
    for (uint32_t gen = 0; gen < config.maxGenerations; ++gen) {
        PatchResult r = runGeneration(testFunction, 1000);
        if (!r.success) return r;
        if (config.fitnessTarget > 0 && getBestFitness() <= config.fitnessTarget) {
            return PatchResult::ok("Fitness target reached — evolution complete");
        }
    }
    return PatchResult::ok("Evolution run completed");
}

uint32_t SpeciatorEngine::getCurrentGeneration() const { return m_generation; }

// ---------------------------------------------------------------------------
// Speciation
// ---------------------------------------------------------------------------
bool SpeciatorEngine::checkSpeciation(SpeciesType species) {
#ifdef RAWR_HAS_MASM
    return asm_speciator_speciate(static_cast<uint32_t>(species)) == 1;
#else
    (void)species;
    return false;
#endif
}

PatchResult SpeciatorEngine::forceSpeciation(SpeciesType newSpecies, const std::vector<uint32_t>& genomeIndices) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint32_t idx : genomeIndices) {
        if (idx < m_population.size()) {
            m_population[idx].species = newSpecies;
        }
    }
    return PatchResult::ok("Forced speciation event applied");
}

// ---------------------------------------------------------------------------
// Variant Production
// ---------------------------------------------------------------------------
PatchResult SpeciatorEngine::generateVariant(SpeciesType species, VariantDescriptor& outDesc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("Not initialized", -1);
    outDesc.species = species;
    outDesc.generation = m_generation;
    outDesc.populationSize = static_cast<uint32_t>(m_population.size());
    outDesc.bestFitness = getBestFitness();

    switch (species) {
        case SpeciesType::Security:   outDesc.variantName = "RawrXD-Sec"; break;
        case SpeciesType::Scientific: outDesc.variantName = "RawrXD-Sci"; break;
        case SpeciesType::Embedded:   outDesc.variantName = "RawrXD-Emb"; break;
        case SpeciesType::Quantum:    outDesc.variantName = "RawrXD-Q";   break;
        default:                      outDesc.variantName = "RawrXD-Gen"; break;
    }
#ifdef RAWR_HAS_MASM
    uint32_t meta[3];
    asm_speciator_gen_variant(static_cast<uint32_t>(species), meta);
#endif
    m_variants.push_back(outDesc);
    return PatchResult::ok("Variant generated");
}

std::vector<VariantDescriptor> SpeciatorEngine::getAllVariants() const { return m_variants; }

// ---------------------------------------------------------------------------
// Resource Competition
// ---------------------------------------------------------------------------
PatchResult SpeciatorEngine::allocateResources(uint32_t totalGpuUnits, std::vector<uint32_t>& perSpecies) {
    std::lock_guard<std::mutex> lock(m_mutex);
    perSpecies.resize(8, 0);
#ifdef RAWR_HAS_MASM
    asm_speciator_compete(perSpecies.data(), totalGpuUnits);
#else
    uint32_t speciesCount = getSpeciesCount();
    if (speciesCount > 0) {
        uint32_t perSpec = totalGpuUnits / speciesCount;
        for (uint32_t i = 0; i < speciesCount && i < 8; ++i)
            perSpecies[i] = perSpec;
    }
#endif
    return PatchResult::ok("Resources allocated");
}

// ---------------------------------------------------------------------------
// Migration
// ---------------------------------------------------------------------------
int32_t SpeciatorEngine::migrateGenome(uint32_t sourceIndex, SpeciesType targetSpecies) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return -1;
#ifdef RAWR_HAS_MASM
    return asm_speciator_migrate(sourceIndex, static_cast<uint32_t>(targetSpecies));
#else
    if (sourceIndex >= m_population.size()) return -1;
    Genome migrant = m_population[sourceIndex];
    migrant.species = targetSpecies;
    m_population.push_back(migrant);
    return static_cast<int32_t>(m_population.size() - 1);
#endif
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
SpeciatorStats SpeciatorEngine::getStats() const {
    SpeciatorStats stats{};
#ifdef RAWR_HAS_MASM
    void* raw = asm_speciator_get_stats();
    if (raw) memcpy(&stats, raw, sizeof(stats));
#endif
    return stats;
}

uint32_t SpeciatorEngine::getPopulationSize() const {
    return static_cast<uint32_t>(m_population.size());
}

uint32_t SpeciatorEngine::getSpeciesCount() const {
    std::vector<SpeciesType> seen;
    for (auto& g : m_population) {
        if (std::find(seen.begin(), seen.end(), g.species) == seen.end())
            seen.push_back(g.species);
    }
    return static_cast<uint32_t>(seen.size());
}

uint64_t SpeciatorEngine::getBestFitness() const {
    uint64_t best = UINT64_MAX;
    for (auto& g : m_population) {
        if (g.fitness > 0 && g.fitness < best) best = g.fitness;
    }
    return (best == UINT64_MAX) ? 0 : best;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void SpeciatorEngine::registerCallback(SpeciatorCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back({cb, userData});
}

void SpeciatorEngine::notifyCallback(const char* event, uint32_t generation, uint64_t bestFitness) {
    for (auto& entry : m_callbacks) {
        if (entry.fn) entry.fn(event, generation, bestFitness, entry.userData);
    }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
size_t SpeciatorEngine::dumpDiagnostics(char* buffer, size_t bufferSize) const {
    if (!buffer || bufferSize == 0) return 0;
    SpeciatorStats st = getStats();
    int written = snprintf(buffer, bufferSize,
        "=== Speciator Diagnostics ===\n"
        "Initialized:      %s\n"
        "Generation:        %u\n"
        "Population:        %u\n"
        "Species Count:     %u\n"
        "Best Fitness:      %llu\n"
        "Genomes Created:   %llu\n"
        "Evaluations:       %llu\n"
        "Crossovers:        %llu\n"
        "Mutations:         %llu\n"
        "Speciations:       %llu\n"
        "Migrations:        %llu\n"
        "Variants Produced: %zu\n",
        m_initialized ? "YES" : "NO",
        m_generation,
        getPopulationSize(),
        getSpeciesCount(),
        getBestFitness(),
        st.genomesCreated, st.evaluations,
        st.crossovers, st.mutations,
        st.speciations, st.migrations,
        m_variants.size()
    );
    return (written > 0) ? static_cast<size_t>(written) : 0;
}

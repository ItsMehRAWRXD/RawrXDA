// rawrxd_speciator_bridge.cpp
// Evolutionary speciation engine bridge - 12 extern "C" symbols

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace
{
    std::mutex              g_specMtx;
    uint32_t                g_genomeSize   = 0;
    uint32_t                g_popSize      = 0;
    std::atomic<uint64_t>   g_evalCount{0};
    std::atomic<uint64_t>   g_mutCount{0};
    bool                    g_initialized  = false;
}

extern "C"
{

int asm_speciator_init(uint32_t genomeSize, uint32_t populationSize)
{
    std::lock_guard<std::mutex> lock(g_specMtx);
    g_genomeSize  = genomeSize;
    g_popSize     = populationSize;
    g_evalCount.store(0);
    g_mutCount.store(0);
    std::srand(42);
    g_initialized = true;
    return 1;
}

int asm_speciator_create_genome(void* genomeOut, uint32_t genomeSize)
{
    if (genomeOut == nullptr || genomeSize == 0)
    {
        return 0;
    }
    uint8_t* bytes = static_cast<uint8_t*>(genomeOut);
    for (uint32_t i = 0; i < genomeSize; ++i)
    {
        bytes[i] = static_cast<uint8_t>(std::rand() & 0xFF);
    }
    return 1;
}

float asm_speciator_evaluate(const void* genome, uint32_t genomeSize)
{
    if (genome == nullptr || genomeSize == 0)
    {
        return 0.0f;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(genome);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < genomeSize; ++i)
    {
        sum += bytes[i];
    }
    g_evalCount.fetch_add(1, std::memory_order_relaxed);
    return static_cast<float>(sum % 255) / 254.0f;
}

int asm_speciator_crossover(const void* parentA, const void* parentB, void* child, uint32_t size)
{
    if (parentA == nullptr || parentB == nullptr || child == nullptr || size == 0)
    {
        return 0;
    }
    const uint32_t half = size / 2;
    std::memcpy(child, parentA, half);
    std::memcpy(static_cast<uint8_t*>(child) + half,
                static_cast<const uint8_t*>(parentB) + half,
                size - half);
    return 1;
}

void asm_speciator_mutate(void* genome, uint32_t size, float mutationRate)
{
    if (genome == nullptr || size == 0 || mutationRate <= 0.0f)
    {
        return;
    }
    uint8_t* bytes    = static_cast<uint8_t*>(genome);
    const int flips   = static_cast<int>(mutationRate * 100.0f);
    for (int i = 0; i < flips; ++i)
    {
        const uint32_t idx = static_cast<uint32_t>(std::rand()) % size;
        const uint8_t  bit = static_cast<uint8_t>(1u << (std::rand() % 8));
        bytes[idx] ^= bit;
    }
    g_mutCount.fetch_add(1, std::memory_order_relaxed);
}

int asm_speciator_select(const void* population, uint32_t popSize, uint32_t genomeSize, void* selectedOut)
{
    if (population == nullptr || selectedOut == nullptr || popSize == 0 || genomeSize == 0)
    {
        return 0;
    }
    std::memcpy(selectedOut, population, genomeSize);
    return 1;
}

int asm_speciator_speciate(void* population, uint32_t popSize, uint32_t genomeSize, uint32_t nSpecies)
{
    if (population == nullptr || popSize == 0 || genomeSize == 0)
    {
        return 0;
    }
    return static_cast<int>(nSpecies);
}

int asm_speciator_gen_variant(const void* genome, uint32_t size, void* variantOut)
{
    if (genome == nullptr || variantOut == nullptr || size == 0)
    {
        return 0;
    }
    std::memcpy(variantOut, genome, size);
    const uint32_t idx                          = static_cast<uint32_t>(std::rand()) % size;
    static_cast<uint8_t*>(variantOut)[idx]     ^= static_cast<uint8_t>(std::rand() & 0xFF);
    return 1;
}

int asm_speciator_compete(void* pop, uint32_t popSize, uint32_t genomeSize)
{
    if (pop == nullptr || popSize == 0 || genomeSize == 0)
    {
        return 0;
    }
    return 1;
}

int asm_speciator_migrate(void* fromPop, void* toPop, uint32_t count, uint32_t genomeSize)
{
    if (fromPop == nullptr || toPop == nullptr || count == 0 || genomeSize == 0)
    {
        return 0;
    }
    std::memcpy(toPop, fromPop, static_cast<size_t>(count) * genomeSize);
    return 1;
}

void asm_speciator_get_stats(void* statsOut)
{
    if (statsOut == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_specMtx);
    uint64_t* out = static_cast<uint64_t*>(statsOut);
    out[0] = g_evalCount.load(std::memory_order_relaxed);
    out[1] = g_mutCount.load(std::memory_order_relaxed);
    out[2] = static_cast<uint64_t>(g_genomeSize);
    out[3] = static_cast<uint64_t>(g_popSize);
}

void asm_speciator_shutdown(void)
{
    std::lock_guard<std::mutex> lock(g_specMtx);
    g_genomeSize  = 0;
    g_popSize     = 0;
    g_evalCount.store(0);
    g_mutCount.store(0);
    g_initialized = false;
}

} // extern "C"

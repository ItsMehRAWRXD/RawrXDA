/**
 * DistributedTrainer unit tests — C++20 only (no Qt).
 * Uses DistributedTrainer from include/distributed_trainer.h.
 */
#include <cstdio>
#include <string>
#include <vector>

#if __has_include("distributed_trainer.h")
#include "distributed_trainer.h"
#else
#include "../../include/distributed_trainer.h"
#endif

#define TEST_VERIFY(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", #cond); ++g_fail; } } while(0)

static int g_fail = 0;

int main() {
    std::fprintf(stdout, "DistributedTrainer unit tests (C++20, no Qt)\n");
    DistributedTrainer trainer;
    DistributedTrainer::TrainerConfig config;
    config.backend = DistributedTrainer::Backend::Gloo;
    config.parallelism = DistributedTrainer::ParallelismType::DataParallel;
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.pgConfig.localRank = 0;
    bool ok = trainer.Initialize(config);
    if (ok) {
        TEST_VERIFY(trainer.isInitialized());
        auto cfg = trainer.getConfiguration();
        TEST_VERIFY(cfg.backend == DistributedTrainer::Backend::Gloo);
        trainer.Shutdown();
    }
    std::fprintf(stdout, "Done: %d failures\n", g_fail);
    return g_fail ? 1 : 0;
}

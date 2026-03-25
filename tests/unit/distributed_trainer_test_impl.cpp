#include "distributed_trainer.h"

bool DistributedTrainer::Initialize(const TrainerConfig& config) {
    m_config = config;
    m_initialized = (config.pgConfig.worldSize >= 1);
    return m_initialized;
}

void DistributedTrainer::Shutdown() {
    m_initialized = false;
}

bool DistributedTrainer::isInitialized() const {
    return m_initialized;
}

DistributedTrainer::TrainerConfig DistributedTrainer::getConfiguration() const {
    return m_config;
}

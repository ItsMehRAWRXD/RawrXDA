#include "ggml-threading_rxd_internal.h"
#include <mutex>

std::mutex ggml_rxd_critical_section_mutex;

void ggml_rxd_critical_section_start() {
    ggml_rxd_critical_section_mutex.lock();
}

void ggml_rxd_critical_section_end(void) {
    ggml_rxd_critical_section_mutex.unlock();
}

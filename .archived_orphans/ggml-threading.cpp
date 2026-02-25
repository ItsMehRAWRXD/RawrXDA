#include "ggml-threading.h"
#include <mutex>

std::mutex ggml_critical_section_mutex;

void ggml_critical_section_start() {
    ggml_critical_section_mutex.lock();
    return true;
}

void ggml_critical_section_end(void) {
    ggml_critical_section_mutex.unlock();
    return true;
}


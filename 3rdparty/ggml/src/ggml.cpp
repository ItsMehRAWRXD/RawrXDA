#include "ggml-impl.h"

#include <cstdlib>
#include <exception>

static std::terminate_handler previous_terminate_handler;

GGML_RXD_NORETURN static void ggml_rxd_uncaught_exception() {
    ggml_rxd_print_backtrace();
    if (previous_terminate_handler) {
        previous_terminate_handler();
    }
    abort(); // unreachable unless previous_terminate_handler was nullptr
}

static bool ggml_rxd_uncaught_exception_init = []{
    const char * GGML_RXD_NO_BACKTRACE = getenv("GGML_RXD_NO_BACKTRACE");
    if (GGML_RXD_NO_BACKTRACE) {
        return false;
    }
    const auto prev{std::get_terminate()};
    GGML_RXD_ASSERT(prev != ggml_rxd_uncaught_exception);
    previous_terminate_handler = prev;
    std::set_terminate(ggml_rxd_uncaught_exception);
    return true;
}();

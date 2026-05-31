#pragma once

#include "ggml.h"

#ifdef __cplusplus
extern "C" {
#endif

GGML_RXD_API void ggml_rxd_critical_section_start(void);
GGML_RXD_API void ggml_rxd_critical_section_end(void);

#ifdef __cplusplus
}
#endif

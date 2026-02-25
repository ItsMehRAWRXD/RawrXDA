# Logging and Queue Includes Audit

## Queue includes (`#include <queue>`)

| File | Line | Usage | Justified |
|------|------|--------|-----------|
| `src/cli/swarm_orchestrator.h` | 44 | `std::queue<SwarmInferenceRequest> m_pendingRequests` | Yes – request queue for swarm inference |
| `src/gpu/LayerPrefetchEngine.h` | 13 | `std::priority_queue` (DMARequest with `operator<`) | Yes – priority queue for layer prefetch |

Both uses are required: one for FIFO inference requests, one for priority-ordered DMA prefetch. No change needed.

---

## cout/cerr → Logger migration status

### Completed (replaced with `Logger`)

| File | Approx. count | Notes |
|------|----------------|--------|
| `src/core/accelerator_router.cpp` | 44 | All router/probe messages use `s_routerLog` |
| `src/model_source_resolver.cpp` | 46 | All resolve/download messages use `s_resolverLog` |
| `src/orchestra_integration.h` | 50 | CLI/usage/metrics use `s_orchestraLog` |
| `src/ggml-vulkan/ggml-vulkan.cpp` | ~30+ | VK_CHECK, timings, errors; macros VK_LOG_DEBUG/VK_LOG_MEMORY left as cerr for stream compatibility |

### Partially done

| File | Remaining | Notes |
|------|-----------|--------|
| `src/cli_shell.cpp` | **0** | **Complete.** All cout/cerr replaced with s_log (profile, subagent, chain, swarm, cot, search, analyze, status, help, route_command, main). |
| `src/ggml-vulkan/ggml-vulkan.cpp` | ~130+ | Long tensor-debug cerr lines and fprintf in debug dumps still present |

### High-count files not yet migrated (src, excl. qtapp)

Representative high counts (from earlier audit):

- `src/cli_shell.cpp` – finish migration
- `src/ggml-vulkan/ggml-vulkan.cpp` – main Vulkan backend; many debug cerr/fprintf
- `src/ggml-vulkan/vulkan-shaders/vulkan-shaders-gen.cpp` – 12
- `src/inference_engine_stub.cpp` – 26
- `src/masm/masm_cli_compiler.cpp` – 30
- `src/llm_adapter/llm_implementation_adapter.h` – 18
- Other core/orchestra/model files – see repo-wide grep for `std::cout`/`std::cerr`

---

## Logger usage (project standard)

- **Header**: `#include "logging/logger.h"` (or `#include "include/logging/logger.h"` depending on include path).
- **Instance**: One static per TU, e.g. `static Logger s_log("component_name");`
- **API**: `s_log.info("format {}", arg)`, `s_log.warn(...)`, `s_log.error(...)`, `s_log.debug(...)`.
- **Format**: `{}` placeholders; multiple args: `s_log.info("a {} b {}", x, y);`
- **Rules**: No `std::cout`/`std::cerr` for application logging; use `Logger` only (see `.cursorrules`).

---

*Last updated: 2026-02-19*

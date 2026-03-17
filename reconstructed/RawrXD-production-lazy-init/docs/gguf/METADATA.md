# GGUF Metadata Extraction and Tooltip Enrichment ✅

What changed:
- Added utilities to reliably extract model metadata from GGUF files and file names.
- Metadata extracted: model name, architecture, context length, parameter count (when available), quantization (heuristic and metadata), and file size in MB.

Files added:
- `src/qtapp/utils/model_metadata_utils.hpp` and `.cpp` — core extraction and heuristic parsing.
- `src/qtapp/utils/test_model_metadata_utils.cpp` — unit tests for filename parsing heuristics.

Behavior details:
- Extraction uses `StreamingGGUFLoaderQt` to read GGUF headers and metadata entries without loading weights.
- Where metadata is missing, heuristics parse quantization and parameter counts from the filename (e.g., `wizardLM-7B.gguf` => 7B).
- Results are intended for display in model selector tooltips and are cached when used by the UI.

Tooltip behavior:
- The model selector (both the main model selector and the Model Loader widget) now shows enriched tooltips for models. Tooltips include: model name, quantization (heuristic or GGUF metadata), parameter count (heuristic or GGUF metadata), architecture (if available) and file size.
- Tooltips are populated immediately with filename heuristics and are asynchronously enriched by parsing GGUF metadata when a local GGUF is present. Tooltips are cached to avoid repeated I/O.
- Tooltips use simple HTML for readability; screen readers will receive the tooltip text as-is. We will iterate on ARIA/accessible hooks based on platform feedback.
 - Tooltips use simple HTML for readability; screen readers will receive the tooltip text as-is. We set an additional item data role (`Qt::UserRole + 1`) as a fallback accessible text for platforms where tooltips are not exposed to assistive technologies.

Next steps:
- Integrate these utilities into the main model selector to show quantization and parameter counts for both local GGUF files and resolved Ollama models.
- Add integration/UI tests for the full discovery -> resolve -> load -> agent availability flow.

Implemented behavior:
- Model Loader widget now automatically enriches tooltips and will programmatically load a GGUF if an Ollama entry resolves to a local GGUF file (user selection). This makes discovery -> resolve -> load a seamless flow for local model testing.

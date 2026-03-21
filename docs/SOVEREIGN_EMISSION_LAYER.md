# Emission layer (PE32+): aligned images, metadata, **signed-ready**

This document defines the **emission layer** in the Sovereign stack: the stage that turns **resolved** sections + symbols + imports into a **loader-valid** PE image (bytes on disk or in memory).

**Tier G** describes what the in-repo emitters **can** aim for; **Tier P** (RawrXD IDE product) still uses **`link.exe`** + normal signing in CI — **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** **§6** / **§7**.

---

## What the emission layer does

| Responsibility | Meaning |
|----------------|--------|
| **Alignment** | **SectionAlignment** / **FileAlignment** — RVAs and raw offsets consistent; **SizeOfImage** padded per PE rules. |
| **Headers** | DOS stub, `PE\0\0`, COFF file header, PE32+ optional header, section table. |
| **Sections** | Map `.text` / `.data` / `.idata` / `.reloc` (etc.) with correct **characteristics** (execute/read/write). |
| **Data directories** | At minimum: **imports** if the image uses DLLs; **base reloc** if the image is not fixed at its preferred base; optional **exception** / **TLS** / **load config** when you implement them. |
| **Entry** | **AddressOfEntryPoint** points at the first instruction the loader runs. |

**In-repo references:** `include/rawrxd/sovereign_emit_formats.hpp` (`composePe64MinimalImage`, …), `toolchain/from_scratch/phase2_linker/pe_writer.c`, `docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`.

---

## “Signed-ready” (not “signed by the emitter”)

**Signed-ready** means the PE is **structurally valid** for the normal Windows signing pipeline:

- **Authenticode** — you run **`signtool`** (or your CI) **after** emission with a certificate; the emitter does **not** embed your private key.
- **Alignment / checksum fields** — optional header fields should follow PE conventions so tools that **append** signatures or **verify** layout behave predictably.

**Not included** in “emission” by default: purchasing a cert, **timestamping**, **catalog** signing, **WHQL**, **Store** packaging.

---

## “Full metadata” — honest meaning (§7)

| Metadata class | Typical role | Tier G / in-repo stance |
|----------------|--------------|---------------------------|
| **Loader-critical** | Imports, entry, sections, relocs, TLS/load config *if used* | Emitters should model these **correctly** for the subset they support. |
| **Debug / PDB-grade** | **`IMAGE_DEBUG_DIRECTORY`**, CodeView **`RSDS`**, full **`mspdb`** pipeline | **Not** promised as **MSVC-identical** **§7** product parity; phase2 may emit **stubs** for experiments — see **`toolchain/from_scratch/phase2_linker/README.md`**. |
| **Rich / vendor-specific** | XOR **Rich** blob between DOS and PE | **Not** a RawrXD **product** deliverable — **§7**. |
| **Full `IMAGE_LOAD_CONFIG_DIRECTORY64` parity** | CFG, stack cookie, etc. | **Not** a **maintained** hand-rolled **§7** guarantee. |

So: **“full metadata”** in marketing language must **not** silently mean **“byte-identical to `link.exe` + full PDB”** — that remains **Tier P / MSVC** or explicitly scoped lab work.

---

## Pipeline position

```text
… → link (resolve + patch) → EMISSION LAYER (PE headers + dirs + alignment) → optional signtool → ship
```

---

## Related

- **`docs/SOVEREIGN_GLOBAL_USE_CONTRACT.md`** — Tier G vs Tier P.  
- **`docs/SOVEREIGN_PE_MICRO_BUILDER_BLUEPRINT.md`** — link → emit checklist.  
- **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** — **§7** MSVC artifacts.  
- **`docs/SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`** — tri-format lab scope.

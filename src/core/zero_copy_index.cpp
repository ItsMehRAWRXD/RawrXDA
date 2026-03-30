// zero_copy_index.cpp — Zero-copy context search bridge (Vector 2).

#include "core/zero_copy_index.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD::Search {

namespace {

using InitFn = int (*)(const std::uint64_t*, std::size_t);
using SearchFn = std::size_t (*)(const void*, std::size_t, std::uint64_t*);

constexpr std::size_t kSymbolRegisterSlots = 64;

#if defined(RAWRXD_CONTEXT_SEARCH_KERNEL_LINKED)
extern "C" int rawrxd_context_search_init(const std::uint64_t* symbols, std::size_t symbolCount);
extern "C" std::size_t rawrxd_context_search_scan(const void* buffer, std::size_t len, std::uint64_t* results);
#endif

struct KernelFns {
	InitFn init = nullptr;
	SearchFn search = nullptr;
};

KernelFns getKernelFns() {
#if defined(RAWRXD_CONTEXT_SEARCH_KERNEL_LINKED)
	static const KernelFns fns {&rawrxd_context_search_init, &rawrxd_context_search_scan};
	return fns;
#elif defined(_WIN32)
	const HMODULE mod = ::GetModuleHandleA(nullptr);
	if (!mod) {
		return {};
	}
	static const KernelFns fns {
		reinterpret_cast<InitFn>(::GetProcAddress(mod, "rawrxd_context_search_init")),
		reinterpret_cast<SearchFn>(::GetProcAddress(mod, "rawrxd_context_search_scan"))};
	return fns;
#else
	return {};
#endif
}

std::array<std::uint64_t, kSymbolRegisterSlots> g_activeSymbols {};
std::size_t g_activeSymbolCount = 0;

std::size_t fallbackScan(const std::uint8_t* buffer, std::size_t bufferLength, std::uint64_t* outOffsets,
						 std::size_t maxOffsets) {
	if (!buffer || !outOffsets || maxOffsets == 0 || g_activeSymbolCount == 0) {
		return 0;
	}

	const std::size_t chunkCount = bufferLength / 64;
	std::size_t matchCount = 0;

	for (std::size_t chunk = 0; chunk < chunkCount; ++chunk) {
		const auto* qwords = reinterpret_cast<const std::uint64_t*>(buffer + (chunk * 64));
		bool hit = false;
		for (std::size_t lane = 0; lane < 8 && !hit; ++lane) {
			const std::uint64_t candidate = qwords[lane];
			for (std::size_t s = 0; s < g_activeSymbolCount; ++s) {
				if (candidate == g_activeSymbols[s]) {
					hit = true;
					break;
				}
			}
		}

		if (hit && matchCount < maxOffsets) {
			outOffsets[matchCount++] = static_cast<std::uint64_t>(chunk * 64);
		}
	}

	return matchCount;
}

}  // namespace

std::uint64_t packSymbolQword(const std::string& symbol) {
	std::uint64_t token = 0;
	const std::size_t len = std::min<std::size_t>(8, symbol.size());
	for (std::size_t i = 0; i < len; ++i) {
		token |= (static_cast<std::uint64_t>(static_cast<unsigned char>(symbol[i])) << (i * 8));
	}
	return token;
}

bool mapFileReadOnly(const std::string& path, MappedFileView& outView) {
#ifdef _WIN32
	outView = {};

	HANDLE hFile = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
								 FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}

	LARGE_INTEGER fileSize {};
	if (!::GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
		::CloseHandle(hFile);
		return false;
	}

	HANDLE hMap = ::CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!hMap) {
		::CloseHandle(hFile);
		return false;
	}

	void* pView = ::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	if (!pView) {
		::CloseHandle(hMap);
		::CloseHandle(hFile);
		return false;
	}

	outView.data = static_cast<const std::uint8_t*>(pView);
	outView.length = static_cast<std::size_t>(fileSize.QuadPart);
	outView.fileHandle = hFile;
	outView.mappingHandle = hMap;
	outView.viewHandle = pView;
	return true;
#else
	(void)path;
	(void)outView;
	return false;
#endif
}

void unmapFileView(MappedFileView& view) {
#ifdef _WIN32
	if (view.viewHandle) {
		::UnmapViewOfFile(view.viewHandle);
	}
	if (view.mappingHandle) {
		::CloseHandle(static_cast<HANDLE>(view.mappingHandle));
	}
	if (view.fileHandle) {
		::CloseHandle(static_cast<HANDLE>(view.fileHandle));
	}
#endif
	view = {};
}

bool initializeContextSearchSymbols(std::span<const std::uint64_t> symbolQwords) {
	std::fill(g_activeSymbols.begin(), g_activeSymbols.end(), 0);
	g_activeSymbolCount = std::min<std::size_t>(kSymbolRegisterSlots, symbolQwords.size());
	if (g_activeSymbolCount == 0) {
		return false;
	}

	std::memcpy(g_activeSymbols.data(), symbolQwords.data(), g_activeSymbolCount * sizeof(std::uint64_t));

	const KernelFns fns = getKernelFns();
	if (fns.init) {
		return fns.init(g_activeSymbols.data(), g_activeSymbolCount) != 0;
	}

	return true;
}

std::size_t findSymbolOffsets(const void* buffer, std::size_t bufferLength, std::uint64_t* outOffsets,
							  std::size_t maxOffsets, ContextSearchStats* outStats) {
	if (!buffer || !outOffsets || maxOffsets == 0 || bufferLength < 64 || g_activeSymbolCount == 0) {
		if (outStats) {
			*outStats = {};
		}
		return 0;
	}

	const std::size_t chunkCount = bufferLength / 64;
	const KernelFns fns = getKernelFns();

	std::size_t matchCount = 0;
	bool linked = false;
	if (fns.search) {
		linked = true;
		matchCount = fns.search(buffer, bufferLength, outOffsets);
		if (matchCount > maxOffsets) {
			matchCount = maxOffsets;
		}
	} else {
		matchCount = fallbackScan(static_cast<const std::uint8_t*>(buffer), bufferLength, outOffsets, maxOffsets);
	}

	if (outStats) {
		outStats->chunksScanned = chunkCount;
		outStats->matches = matchCount;
		outStats->kernelLinked = linked;
	}

	return matchCount;
}

std::size_t executeContextSearch(const std::string& path, std::span<const std::uint64_t> symbolQwords,
								 std::uint64_t* outOffsets, std::size_t maxOffsets,
								 ContextSearchStats* outStats) {
	if (!initializeContextSearchSymbols(symbolQwords)) {
		if (outStats) {
			*outStats = {};
		}
		return 0;
	}

	MappedFileView view;
	if (!mapFileReadOnly(path, view)) {
		if (outStats) {
			*outStats = {};
		}
		return 0;
	}

	const std::size_t matches = findSymbolOffsets(view.data, view.length, outOffsets, maxOffsets, outStats);
	unmapFileView(view);
	return matches;
}

}  // namespace RawrXD::Search

// rawrxd_native_log_fallback.cpp — minimal native log fallback symbols
#include <cstdarg>
#include <cstdio>

extern "C" void RawrXDNativeLogFallbackStub() {}

extern "C" void RawrXD_Native_Log(const char* fmt, ...)
{
	if (!fmt)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	std::vfprintf(stderr, fmt, args);
	std::fputc('\n', stderr);
	va_end(args);
}

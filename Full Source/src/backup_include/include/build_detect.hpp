// build_detect.hpp
// Build-time detection header for RawrXD PowerBuild system
#pragma once

#ifdef RAWXD_BUILD
    #define RAWR_BUILD_TIMESTAMP __DATE__ " " __TIME__
    #define RAWR_BUILD_CONFIG RELEASE
#else
    #define RAWR_BUILD_TIMESTAMP "unknown"
    #define RAWR_BUILD_CONFIG UNKNOWN
#endif

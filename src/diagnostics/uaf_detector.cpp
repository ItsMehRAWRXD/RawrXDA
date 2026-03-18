#include "uaf_detector.hpp"

std::unordered_map<void*, UAFDetector::BlockHeader*> UAFDetector::s_blocks;
std::mutex UAFDetector::s_mutex;
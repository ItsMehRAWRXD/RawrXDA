#include "init_order.hpp"

std::atomic<int> InitOrderDetector::s_sequence(0);
std::vector<InitOrderDetector::InitRecord> InitOrderDetector::s_records;
std::atomic<bool> InitOrderDetector::s_mainEntered(false);
std::mutex InitOrderDetector::s_mutex;
#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

struct TelemetrySnapshot {
	uint64_t timeMs = 0;

	bool cpuTempValid = false;
	double cpuTempC = 0.0;
	double cpuUsagePercent = 0.0;

	bool gpuTempValid = false;
	double gpuTempC = 0.0;
	double gpuUsagePercent = 0.0;
	std::string gpuVendor;
};

namespace telemetry {
using TelemetrySnapshot = ::TelemetrySnapshot;

bool Initialize();
bool InitializeHardware();
bool Poll(TelemetrySnapshot& out);
void Shutdown();
}

class Telemetry {
public:
	Telemetry();
	~Telemetry();

	void initializeHardware();
	void recordEvent(const std::string& event_name,
					 const nlohmann::json& metadata = nlohmann::json::object());
	bool saveTelemetry(const std::string& filepath);
	void enableTelemetry(bool enable);

private:
	bool is_enabled_ = true;
	nlohmann::json events_ = nlohmann::json::array();
};

void logEvent(const char* name, double v1, double v2);

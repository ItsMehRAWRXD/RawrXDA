// rawrxd_serve_inference_plugin.h — Optional DLL bridge for RawrXD-Serve
// Keeps rawrxd.exe minimal (httpapi only); real inference lives in a companion DLL.
#pragma once

#include <string>

#include "rawrxd_serve.h"

namespace RawrXD
{
namespace Serve
{
namespace InferencePlugin
{

/// Probe RAWRXD_SERVE_INFERENCE_DLL and well-known names next to the executable.
/// On success, detailOut lists which DLL was loaded.
bool tryLoad(std::string& detailOut);

void unloadDll();

bool hasPlugin();

/// Returns true if the plugin reports successful weight load.
bool loadModel(const std::string& pathUtf8, std::string& err);

void unloadModel();

/// Runs generation through the plugin. If no plugin, sets err and returns empty.
std::string generate(const GenerateRequest& req, StreamTokenFn onToken, std::string& err);

}  // namespace InferencePlugin
}  // namespace Serve
}  // namespace RawrXD

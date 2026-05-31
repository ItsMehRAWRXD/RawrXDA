#pragma once

namespace RawrXD::ExtensionKernel {

class AutocompleteKernel;

extern "C" __declspec(dllexport)
bool RawrXD_AutocompleteKernel_OnIdle(AutocompleteKernel* kernel);

} // namespace RawrXD::ExtensionKernel

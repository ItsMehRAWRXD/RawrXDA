#pragma once

#include "../rawrxd_com_min.h"

namespace wil {

template <typename T>
using com_ptr = rawrxd::com::ComPtr<T>;

template <typename T>
using com_ptr_nothrow = rawrxd::com::ComPtr<T>;

} // namespace wil


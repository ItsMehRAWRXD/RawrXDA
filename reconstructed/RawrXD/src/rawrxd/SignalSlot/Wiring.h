#pragma once
#define WIRE_SIGNAL(src, sig, dst, slot) (src).sig.connect([&](auto&&... args){ (dst).slot(std::forward<decltype(args)>(args)...); })
#define WIRE_SIGNAL_THREADSAFE(src, sig, dst, slot, mtx) (src).sig.connect([&](auto&&... args){ std::lock_guard<std::mutex> lk(mtx); (dst).slot(std::forward<decltype(args)>(args)...); })

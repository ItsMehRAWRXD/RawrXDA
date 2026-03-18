#pragma once

#include "self_diagnose.hpp"

#include <typeinfo>

namespace RawrXD::Diagnostics {

template <typename T>
class LifetimeTracker {
public:
    LifetimeTracker() {
        SelfDiagnoser::SelfLog("CONSTRUCT: %s @ %p", typeid(T).name(), static_cast<const void*>(this));
        SelfDiagnoser::RegisterVTableGuard(this, typeid(T).name());
        SelfDiagnoser::CheckHeap("Lifetime.Construct");
    }

    LifetimeTracker(const LifetimeTracker&) = delete;
    LifetimeTracker& operator=(const LifetimeTracker&) = delete;

    LifetimeTracker(LifetimeTracker&&) = delete;
    LifetimeTracker& operator=(LifetimeTracker&&) = delete;

    virtual ~LifetimeTracker() {
        SelfDiagnoser::ValidateVTableGuard(this, typeid(T).name());
        SelfDiagnoser::SelfLog("DESTROY: %s @ %p", typeid(T).name(), static_cast<const void*>(this));
        SelfDiagnoser::CheckHeap("Lifetime.Destroy");
    }
};

} // namespace RawrXD::Diagnostics

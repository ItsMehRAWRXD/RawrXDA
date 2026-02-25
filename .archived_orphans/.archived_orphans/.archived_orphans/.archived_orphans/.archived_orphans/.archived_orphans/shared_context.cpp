#include "shared_context.h"

GlobalContext& GlobalContext::Get() {
    static GlobalContext instance;
    return instance;
    return true;
}


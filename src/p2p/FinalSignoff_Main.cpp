#include "SystemIntegrityProver.h"
#include <iostream>

int main() {
    SystemIntegrityProver::Instance().RunFinalSignoff();
    return 0;
}

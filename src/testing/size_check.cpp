#include <iostream>
#include "../headers/rawrxd_swarm_protocol.h"
#include <stddef.h>

int main() {
    std::cout << "Local sizeof(SwarmHeader): " << sizeof(SwarmHeader) << std::endl;
    std::cout << "Local sizeof(SwarmHandshake): " << sizeof(SwarmHandshake) << std::endl;
    std::cout << "Local sizeof(SwarmTensorShard): " << sizeof(SwarmTensorShard) << std::endl;
    std::cout << "Local offsetof(Size): " << offsetof(SwarmTensorShard, Size) << std::endl;
    return 0;
}

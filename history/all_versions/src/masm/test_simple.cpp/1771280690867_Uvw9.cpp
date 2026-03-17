/*
 * test_simple.cpp — Simple test to verify engines initialize
 */

#include <stdio.h>
#include "SlolorisStreamLoader.h"
#include "BounceTPS.h"
#include "HotPatchTPS.h"
#include "TPSBridge.h"
#include "DirectionlessLoader.h"
#include "UnbraidPipeline.h"

int main() {
    printf("Testing engine initialization...\n");

    // Test each engine
    SlolorisContext slo = Sloloris_Init(0);
    printf("Sloloris: %s\n", slo ? "OK" : "FAIL");

    BounceContext bounce = Bounce_Init(0);
    printf("Bounce: %s\n", bounce ? "OK" : "FAIL");

    HotPatchContext hotpatch = HotPatch_Init(0);
    printf("HotPatch: %s\n", hotpatch ? "OK" : "FAIL");

    BridgeContext bridge = Bridge_Init(slo, bounce, hotpatch);
    printf("Bridge: %s\n", bridge ? "OK" : "FAIL");

    DirLoadContext dirload = DirLoad_Init(75);
    printf("DirLoad: %s\n", dirload ? "OK" : "FAIL");

    UnbraidCtx unbraid = Unbraid_Init(100ULL * 1024 * 1024);
    printf("Unbraid: %s\n", unbraid ? "OK" : "FAIL");

    // Cleanup
    if (unbraid) Unbraid_Destroy(unbraid);
    if (dirload) DirLoad_Destroy(dirload);
    if (bridge) Bridge_Destroy(bridge);
    if (hotpatch) HotPatch_Destroy(hotpatch);
    if (bounce) Bounce_Destroy(bounce);
    if (slo) Sloloris_Destroy(slo);

    printf("All engines initialized and destroyed successfully!\n");
    return 0;
}
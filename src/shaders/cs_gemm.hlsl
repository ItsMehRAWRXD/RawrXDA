// MatMul / GEMM compute kernel using ByteAddressBuffer
#define TILE_SIZE 16

ByteAddressBuffer BufferA : register(t0); // Matrix A (M x K)
ByteAddressBuffer BufferB : register(t1); // Matrix B (K x N)
RWByteAddressBuffer BufferC : register(u0); // Output Matrix C (M x N)

cbuffer GemmConstants : register(b0)
{
    uint M;
    uint K;
    uint N;
    uint unused; // padding
};

groupshared float tileA[TILE_SIZE][TILE_SIZE];
groupshared float tileB[TILE_SIZE][TILE_SIZE];

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint row = DTid.y;
    uint col = DTid.x;
    
    float acc = 0.0f;
    
    // Number of tiles
    uint numTiles = (K + TILE_SIZE - 1) / TILE_SIZE;
    
    for (uint t = 0; t < numTiles; t++)
    {
        // Load A into shared memory
        uint aCol = t * TILE_SIZE + GTid.x;
        if (row < M && aCol < K)
            tileA[GTid.y][GTid.x] = asfloat(BufferA.Load((row * K + aCol) * 4));
        else
            tileA[GTid.y][GTid.x] = 0.0f;
            
        // Load B into shared memory
        uint bRow = t * TILE_SIZE + GTid.y;
        if (bRow < K && col < N)
            tileB[GTid.y][GTid.x] = asfloat(BufferB.Load((bRow * N + col) * 4));
        else
            tileB[GTid.y][GTid.x] = 0.0f;
            
        GroupMemoryBarrierWithGroupSync();
        
        // Compute tile
        for (uint k = 0; k < TILE_SIZE; k++)
        {
            acc += tileA[GTid.y][k] * tileB[k][GTid.x];
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    if (row < M && col < N)
    {
        BufferC.Store((row * N + col) * 4, asuint(acc));
    }
}

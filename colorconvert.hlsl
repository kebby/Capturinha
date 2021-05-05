
// just so syntax highlighting works... :)
#ifndef OUTFORMAT
#define OUTFORMAT 1
#endif

Texture2D<float4> TexIn;
RWByteAddressBuffer Out;

cbuffer cb_csc : register(b0)
{
    float4x4 colormatrix; // needs to have bpp baked in
    uint pitch, height;
}

groupshared float4 tile[8 * 8];

[numthreads(8, 8, 1)]
void csc(uint3 dispid : SV_DispatchThreadID, uint3 threadid : SV_GroupThreadID, uint3 groupid : SV_GroupID)
{
    // convert 8x8 pixels to output color space and store in tile memory
    float4 pixel = TexIn.Load(int3(dispid.x, dispid.y, 0));
    tile[8 * threadid.y + threadid.x] = mul(pixel, colormatrix);
    GroupMemoryBarrierWithGroupSync();
    
#if OUTFORMAT == 1 	// NV12: 8bpp 4:2:0, plane 1: Y, plane 2: U/V interleaved
    
    if (!(threadid.x & 3))
    {
        uint tileaddr = 8 * threadid.y + threadid.x;

        // store y
        float4 values = float4(tile[tileaddr].x, tile[tileaddr + 1].x, tile[tileaddr + 2].x, tile[tileaddr + 3].x);
        uint addr = pitch * dispid.y + dispid.x;
        Out.Store4(addr, uint4(values));
        
        // store U/V (chroma sample location: top left)
        if (!(threadid.y & 1))
        {
            float4 values = float4(tile[tileaddr].y, tile[tileaddr].z, tile[tileaddr + 2].y, tile[tileaddr + 2].z);
            uint addr = pitch * (height + (dispid.y / 2)) + dispid.x;
            Out.Store4(addr, uint4(values));
        }
    }
            
#elif OUTFORMAT == 2 // whatever

#else
    #error Unknown output format
#endif

}

//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------

/*
        
        */
//
// Copyright (C) Tammo Hinrichs 2021-2022.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

// color space conversion shader
//-----------------------------------------------------------------------------------------

// just so syntax highlighting works... :)
#ifndef OUTFORMAT
#define OUTFORMAT 4
#define UPSCALE 0
#endif

#ifndef HDR
#define HDR 1
#endif

Texture2D<float4> TexIn;
RWByteAddressBuffer Out;

cbuffer cb_csc : register(b0)
{
    float4x4 yuvmatrix;    // convert from RGB to YUV and scale to integer
    uint4 pitch_height_scale;
    float4x4 colormatrix;  // convert to ST 2020 and normalize to 10000 nits
}

groupshared float4 tile[8 * 8];

//-----------------------------------------------------------------------------------------

// make uint with 4 bytes from 4 floats
uint getuint8(float4 p)
{
    return uint(round(p.x)) | (uint(round(p.y)) << 8) | (uint(round(p.z)) << 16) | (uint(round(p.w)) << 24);
}

// make uint with 2 words from 2 floats
uint getuint16(float2 p)
{
    return uint(round(p.x)) | (uint(round(p.y)) << 16);
}

// Get UV for 4:2:0 subsampling
float2 getuv420(uint addr)
{
    return (tile[addr].yz + tile[addr + 1].yz + tile[addr + 8].yz + tile[addr + 9].yz) / 4.0;
}

// convert linear (0..1, 1.0 = 10000 nits) to ST-2084 (0..1)
float3 lin2ST2084(float3 y)
{
    y = max(0, y);
    return saturate(pow((0.8359375f + 18.8515625f * pow(y, 0.1593017578f)) / (1.0f + 18.6875f * pow(y, 0.1593017578f)), 78.84375f));
}

//-----------------------------------------------------------------------------------------

// color space conversion
[numthreads(8, 8, 1)]
void csc(uint3 dispid : SV_DispatchThreadID, uint3 threadid : SV_GroupThreadID)
{
    // convert 8x8 pixels to output color space and store in tile
#if UPSCALE == 1
    float4 pixel = TexIn.Load(int3(dispid.x / pitch_height_scale.z, dispid.y / pitch_height_scale.z, 0));
#else
    float4 pixel = TexIn.Load(int3(dispid.x, dispid.y, 0));
#endif
    pixel.w = 1;
    
#if HDR == 1
    // convert from source color space to ST-2020, and apply the ST-2048 transfer curve
    pixel.xyz = lin2ST2084(mul(pixel, colormatrix).xyz);
#endif

    uint tileaddr = 8 * threadid.y + threadid.x;
    tile[tileaddr] = mul(pixel, yuvmatrix);

    GroupMemoryBarrierWithGroupSync();
    
#if OUTFORMAT == 0     // 8bpp BGRA
    
    uint addr = pitch_height_scale.x * dispid.y + 4 * dispid.x;
    Out.Store(addr, getuint8(tile[tileaddr].zyxw));
    
#elif OUTFORMAT == 1   // NV12: 8bpp YUV 4:2:0, plane 1: Y, plane 2: Cb/Cr interleaved
    
    if (!(threadid.x & 3))
    {
        // store Y for 4*1 pixels
        float4 values = float4(tile[tileaddr].x, tile[tileaddr + 1].x, tile[tileaddr + 2].x, tile[tileaddr + 3].x);
        uint addr = pitch_height_scale.x * dispid.y + dispid.x;
        Out.Store(addr, getuint8(values));
        
        // store U/V for 4*2 pixels
        if (!(threadid.y & 1))
        {
            uint addr = pitch_height_scale.x * (pitch_height_scale.y + (dispid.y / 2)) + dispid.x;
            Out.Store(addr, getuint8(float4(getuv420(tileaddr), getuv420(tileaddr + 2))));
        }
    }
            
#elif OUTFORMAT == 2 // 8bpp planar YUV 4:4:4
        
    if (!(threadid.x & 3))
    {
        // store Y for 4*1 pixels
        float4 values = float4(tile[tileaddr].x, tile[tileaddr + 1].x, tile[tileaddr + 2].x, tile[tileaddr + 3].x);
        uint addr = pitch_height_scale.x * dispid.y + dispid.x;
        Out.Store(addr, getuint8(values));
        
        // store U for 4*1 pixels
        values = float4(tile[tileaddr].y, tile[tileaddr + 1].y, tile[tileaddr + 2].y, tile[tileaddr + 3].y);
        addr = pitch_height_scale.x * (pitch_height_scale.y + dispid.y) + dispid.x;
        Out.Store(addr, getuint8(values));
        
        // store V for 4*1 pixels
        values = float4(tile[tileaddr].z, tile[tileaddr + 1].z, tile[tileaddr + 2].z, tile[tileaddr + 3].z);
        addr = pitch_height_scale.x * (2 * pitch_height_scale.y + dispid.y) + dispid.x;
        Out.Store(addr, getuint8(values));
    }    
    
#elif OUTFORMAT == 3 // 16bpp YUV 4:2:0, plane 1: Y, plane 2: Cb/Cr interleaved
    
    if (!(threadid.x & 1))
    {
        // store Y for 2*1 pixels
        float2 values = float2(tile[tileaddr].x, tile[tileaddr + 1].x);
        uint addr = pitch_height_scale.x * dispid.y + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
        
        // store U/V for 2*2 pixels
        if (!(threadid.y & 1))
        {
            uint addr = pitch_height_scale.x * (pitch_height_scale.y + (dispid.y / 2)) + 2 * dispid.x;
            Out.Store(addr, getuint16(getuv420(tileaddr)));
        }
    }
    
#elif OUTFORMAT == 4 // 16bpp planar YUV 4:4:4
    
    if (!(threadid.x & 1))
    {
        // store Y for 2*1 pixels
        float2 values = float2(tile[tileaddr].x, tile[tileaddr + 1].x);
        uint addr = pitch_height_scale.x * dispid.y + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
        
        // store U for 2*1 pixels
        values = float2(tile[tileaddr].y, tile[tileaddr + 1].y);
        addr = pitch_height_scale.x * (pitch_height_scale.y + dispid.y) + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
        
        // store V for 2*1 pixels
        values = float2(tile[tileaddr].z, tile[tileaddr + 1].z);
        addr = pitch_height_scale.x * (2 * pitch_height_scale.y + dispid.y) + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
    }
    
#else
#error Unknown output format
#endif

}


//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

// color space conversion shader
//-----------------------------------------------------------------------------------------

Texture2D<float4> TexIn;
RWByteAddressBuffer Out;

cbuffer cb_csc : register(b0)
{
    float4x4 colormatrix;    // needs to have bpp baked in (so eg. *255)
    uint4 pitch_height;
}

groupshared float4 tile[8 * 8];

//-----------------------------------------------------------------------------------------

// make uint with 4 bytes from 4 floats
uint getuint8(float4 p)
{
    return uint(round(p.x)) | (uint(round(p.y)) << 8) | (uint(round(p.z)) << 16) | (uint(round(p.w)) << 24);
}

// make uint2 with 2 words from 2 floats
uint getuint16(float2 p)
{
    return uint(round(p.x)) | (uint(round(p.y)) << 16);
}

// Get UV for 4:2:0 subsampling
float2 getuv420(uint addr)
{
    return (tile[addr].yz + tile[addr + 1].yz + tile[addr + 8].yz + tile[addr + 9].yz) / 4.0;
}

//-----------------------------------------------------------------------------------------

// just so syntax highlighting works... :)
#ifndef OUTFORMAT
#define OUTFORMAT 4
#endif

// color space conversion
[numthreads(8, 8, 1)]
void csc(uint3 dispid : SV_DispatchThreadID, uint3 threadid : SV_GroupThreadID, uint3 groupid : SV_GroupID)
{
    // convert 8x8 pixels to output color space and store in tile
    float4 pixel = TexIn.Load(int3(dispid.x, dispid.y, 0));
    pixel.w = 1;
    
    uint tileaddr = 8 * threadid.y + threadid.x;
    tile[tileaddr] = mul(pixel, colormatrix);
    GroupMemoryBarrierWithGroupSync();
    
#if OUTFORMAT == 0     // 8bpp BGRA
    
    uint addr = pitch_height.x * dispid.y + 4 * dispid.x;
    Out.Store(addr, getuint8(tile[tileaddr].zyxw));
    
#elif OUTFORMAT == 1   // NV12: 8bpp YUV 4:2:0, plane 1: Y, plane 2: Cb/Cr interleaved
    
    if (!(threadid.x & 3))
    {
        // store Y for 4*1 pixels
        float4 values = float4(tile[tileaddr].x, tile[tileaddr + 1].x, tile[tileaddr + 2].x, tile[tileaddr + 3].x);
        uint addr = pitch_height.x * dispid.y + dispid.x;
        Out.Store(addr, getuint8(values));
        
        // store U/V for 4*2 pixels
        if (!(threadid.y & 1))
        {
            uint addr = pitch_height.x * (pitch_height.y + (dispid.y / 2)) + dispid.x;
            Out.Store(addr, getuint8(float4(getuv420(tileaddr), getuv420(tileaddr + 2))));
        }
    }
            
#elif OUTFORMAT == 2 // 8bpp planar YUV 4:4:4
        
    if (!(threadid.x & 3))
    {
        // store Y for 4*1 pixels
        float4 values = float4(tile[tileaddr].x, tile[tileaddr + 1].x, tile[tileaddr + 2].x, tile[tileaddr + 3].x);
        uint addr = pitch_height.x * dispid.y + dispid.x;
        Out.Store(addr, getuint8(values));
        
        // store U for 4*1 pixels
        values = float4(tile[tileaddr].y, tile[tileaddr + 1].y, tile[tileaddr + 2].y, tile[tileaddr + 3].y);
        addr = pitch_height.x * (pitch_height.y + dispid.y) + dispid.x;
        Out.Store(addr, getuint8(values));
        
        // store V for 4*1 pixels
        values = float4(tile[tileaddr].z, tile[tileaddr + 1].z, tile[tileaddr + 2].z, tile[tileaddr + 3].z);
        addr = pitch_height.x * (2 * pitch_height.y + dispid.y) + dispid.x;
        Out.Store(addr, getuint8(values));
    }    
    
#elif OUTFORMAT == 3 // 16bpp YUV 4:2:0, plane 1: Y, plane 2: Cb/Cr interleaved
    
    if (!(threadid.x & 1))
    {
        // store Y for 2*1 pixels
        float2 values = float2(tile[tileaddr].x, tile[tileaddr + 1].x);
        uint addr = pitch_height.x * dispid.y + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
        
        // store U/V for 2*2 pixels
        if (!(threadid.y & 1))
        {
            uint addr = pitch_height.x * (pitch_height.y + (dispid.y / 2)) + 2 * dispid.x;
            Out.Store(addr, getuint16(getuv420(tileaddr)));
        }
    }
    
#elif OUTFORMAT == 4 // 16bpp planar YUV 4:4:4
    
    if (!(threadid.x & 1))
    {
        // store Y for 2*1 pixels
        float2 values = float2(tile[tileaddr].x, tile[tileaddr + 1].x);
        uint addr = pitch_height.x * dispid.y + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
        
        // store U for 2*1 pixels
        values = float2(tile[tileaddr].y, tile[tileaddr + 1].y);
        addr = pitch_height.x * (pitch_height.y + dispid.y) + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
        
        // store V for 2*1 pixels
        values = float2(tile[tileaddr].z, tile[tileaddr + 1].z);
        addr = pitch_height.x * (2 * pitch_height.y + dispid.y) + 2 * dispid.x;
        Out.Store(addr, getuint16(values));
    }
    
#else
#error Unknown output format
#endif

}


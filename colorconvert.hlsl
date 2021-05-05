
Texture2D TexIn;
RWByteAddressBuffer Out;

// just so syntax highlighting works... :)
#ifndef OUTFORMAT
#define OUTFORMAT 1
#endif

float3 lin2srgb(float3 color)
{
    return (color <= 0.0031308) ? (color * 12.92) : 1.055 * pow(max(0, color), 1 / 2.4) - 0.055;
}

cbuffer cb_csc : register(b0)
{
	float4x4 colormatrix;
}

[numthreads(8,8,1)]
void csc(uint3 dtid: SV_DispatchThreadID)
{
	
#if OUTFORMAT == 1 // NV12
	
	this shouldn't work
	
#elif OUTFORMAT == 2 // whatever

#else
	#error Unknown output format
#endif

}

//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------

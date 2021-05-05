
struct Particle
{
  float3 pos;
  float created; // timestamp
  float3 spd;
  float pad0;
};

StructuredBuffer<Particle> PBuf : register(t0);
RWStructuredBuffer<Particle> PBufOut : register(u0);

float3 lin2srgb(float3 color)
{
    return (color <= 0.0031308) ? (color * 12.92) : 1.055 * pow(max(0, color), 1 / 2.4) - 0.055;
}

cbuffer cb_particles : register(b0)
{
  float3 gravity;
  float time;
  float timeScale;
  float3 pad;
}

[numthreads(256,1,1)]
void particles(uint3 dtid: SV_DispatchThreadID)
{
  Particle p = PBuf[dtid.x];

  float t = timeScale * frac(time + p.created);
  p.pos = p.pos + t * p.spd + t * t * gravity;

  PBufOut[dtid.x] = p;
}


//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------

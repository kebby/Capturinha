#pragma once

#include "types.h"
#include "system.h"
#include "math3d.h"

struct IDXGIAdapter;
struct ID3D11Device;
struct ID3D11Texture2D;

//---------------------------------------------------------------------------
// shaders
//---------------------------------------------------------------------------

class Shader : public RCObj
{
public:
    enum class Type { None, Compute, Domain, Geometry, Hull, Pixel, Vertex };
    
    Shader(Type t, const void* buf, size_t size);
    ~Shader();

    struct Priv;
    Priv* P = nullptr;
};

class ShaderResource : public RCObj
{
public:
    struct SR;
    virtual SR& GetSR(bool write) = 0;
};

//---------------------------------------------------------------------------
// buffers
//---------------------------------------------------------------------------

class GpuBuffer : public ShaderResource
{
public:
    enum class Usage { Immutable, Dynamic, GpuOnly, };

    virtual ~GpuBuffer();

    SR& GetSR(bool write) override { return GetSR(write, 0); }

    struct Priv;
    Priv* P = nullptr;

protected:
    enum class Type { Vertex, Index, Constant, Structured };
    GpuBuffer(Type type, Usage usage);

    virtual void Commit() = 0;

    void Upload(const void* data, uint size, uint stride=0, uint totalsize=0);
    void Reset();
    SR& GetSR(bool write, uint count);
};

template <typename T> class TypedBuffer : public GpuBuffer
{
protected:
    uint num = 0;
    uint cur = 0;
    T* data = nullptr;

    TypedBuffer(Type type, uint n, GpuBuffer::Usage usage) : GpuBuffer(type, usage), num(n) {}
public:

    void Clear()
    {
        cur = 0;
        Reset();
    }

    T* BeginLoad()
    {
        if (!data && num) data = new T[num];
        return data + cur;
    }

    uint EndLoad(int n=-1)
    {
        if (n < 0) n = num;

        ASSERT(!num || data);
        ASSERT(cur + n <= num);
        cur += n;
        return cur - n;
    }

    uint Stride() const { return sizeof(T); }
    uint Count() const { return cur; }

    SR& GetSR(bool write) override { return GpuBuffer::GetSR(write,num); }

    void Commit() override { Upload(data, cur * sizeof(T), sizeof(T), num * sizeof(T)); }
};


template <typename TV> class VertexBuffer : public TypedBuffer<TV>
{
public:
    explicit VertexBuffer(int nv, GpuBuffer::Usage usage = GpuBuffer::Usage::Immutable) 
        : TypedBuffer<TV>(GpuBuffer::Type::Vertex, nv, usage) {}
};

class IndexBuffer : public TypedBuffer<uint16>
{
public:
    explicit IndexBuffer(int ni, Usage usage = Usage::Immutable) 
        : TypedBuffer<uint16>(GpuBuffer::Type::Index, ni, usage) {}
};

template <typename TS> class StructuredBuffer : public TypedBuffer<TS>
{
public:
    explicit StructuredBuffer(int nr, GpuBuffer::Usage usage = GpuBuffer::Usage::Immutable) 
        : TypedBuffer<TS>(GpuBuffer::Type::Structured, nr, usage) {}
};

template<class TCB> class CBuffer : public GpuBuffer
{
protected:
    void Commit() override { Upload(&data, sizeof(TCB)); }

public:
    TCB data;

    CBuffer() : GpuBuffer(Type::Constant, Usage::Immutable) {}
    CBuffer(const TCB& cb) : GpuBuffer(Type::Constant), data(cb) {}

    TCB* operator -> () { return &data; }
};

//---------------------------------------------------------------------------
// textures
//---------------------------------------------------------------------------

enum class PixelFormat : uint
{
    None,
    R8, R16, R16F, R16I, R32F, R32I,
    RG8, RG16, RG16F, RG16I, RG32F, RG32I,
    RGBA8, RGBA8sRGB, RGBA16, RGBA16F, RGBA16I, RGBA32F, RGBA32I,
    BGRA8, BGRA8sRGB,
    RGB10A2,
    MAX_FMT,

    // depth formats come last
    D32F, D24S8,
};

struct TexturePara
{
    uint sizeX = 0;
    uint sizeY = 0;
    uint mipmaps = 1;
    PixelFormat format = PixelFormat::RGBA8;

    bool Equals(const TexturePara& p) const;
    bool operator ==(const TexturePara& p) const { return Equals(p); }
    bool operator !=(const TexturePara& p) const { return !Equals(p); }
};

class Texture : public ShaderResource
{
public:
    struct Priv;
    Priv* P;

    TexturePara para;

    Texture();
    virtual ~Texture();

    void CopyFrom(RCPtr<Texture> texture);

    RCPtr<ID3D11Texture2D> GetTex2D();

protected:
    SR& GetSR(bool write) override;
};

class RenderTarget : public Texture
{
public:
    ~RenderTarget() override;
};

//---------------------------------------------------------------------------
// state and bindings
//---------------------------------------------------------------------------

// graphics

struct GState
{
    RCPtr<Shader> VS;
    RCPtr<Shader> PS;   
};

struct GBindings {
    GpuBuffer* vscb[4] = {};
    ShaderResource* vsres[16] = {};

    GpuBuffer* pscb[4] = {};
    ShaderResource* psres[16] = {};

    RenderTarget* target[4] = {};
    RenderTarget* depth = 0;
};

// compute

struct CBindings {
    ShaderResource* res[16] = {};
    GpuBuffer* cb[4] = {};
    ShaderResource* uav[16] = {};
};

//---------------------------------------------------------------------------
// vertex formats and geometry
//---------------------------------------------------------------------------

struct VertexC
{
    Vec3 pos;
    uint color;
};

struct VertexCT
{
    Vec3 pos;
    uint color;
    Vec2 uv;
};

struct CbVSBasic
{
    Mat44 mvp;
};

template<typename TV> class Geometry : public RCObj
{
    VertexBuffer<TV> vb;          
    IndexBuffer ib;

public:
    explicit Geometry(int nv, int ni = 0) : vb(nv), ib(ni) {}

    void BeginLoad(TV*& vp, uint16*& ip)
    {
        vp = vb.BeginLoad();
        ip = ib.BeginLoad();
    }

    void EndLoad(uint nv = -1, uint ni = -1)
    {
        vb.EndLoad(nv);
        ib.EndLoad(ni);
    }

    void Draw(GState &state, const GBindings& binds, int instances=1);
};

//---------------------------------------------------------------------------
// screen capturing
//---------------------------------------------------------------------------

struct CaptureInfo
{
    RCPtr<Texture> tex;
    uint sizeX;
    uint sizeY;
    uint rateNum;
    uint rateDen;
    uint64 frameCount;
    double time;
};

bool CaptureFrame(int timeoutMs, CaptureInfo &info);
void ReleaseFrame();

//---------------------------------------------------------------------------
// functions
//---------------------------------------------------------------------------

void InitD3D();
void ExitD3D();

RCPtr<IDXGIAdapter> GetAdapter();
RCPtr<ID3D11Device> GetDevice();

RCPtr<Texture> LoadImg(const char *filename);
RCPtr<Texture> CreateTexture(const TexturePara& para, const void* data);

RCPtr<Shader> CompileShader(Shader::Type type, const Buffer* buffer, const char* entryPoint, const char* name);
RCPtr<Shader> GetIntShader(Shader::Type type, const char* proc);

RCPtr<RenderTarget> AcquireRenderTarget(TexturePara para);
RCPtr<RenderTarget> AcquireBackBuffer();

void Clear(RenderTarget* rt, Vec4 color);
void ClearDepth(RenderTarget* rt, float d=1.0f);

void Dispatch(Shader* shader, const CBindings& binds, uint gx=1, uint gy=1, uint gz=1);

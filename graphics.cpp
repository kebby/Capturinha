//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

// Including SDKDDKVer.h defines the highest available Windows platform.
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files
#include <windows.h>
#include <stdio.h>

#include "system.h"
#include "graphics.h"
#include "math3d.h"

// from system.cpp
//static HWND hWnd;

#include <timeApi.h>
#pragma comment (lib, "winmm.lib")

// Direct3D 
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxguid.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")

// Windows Imaging component
#include <wincodec.h>
#pragma comment(lib, "Windowscodecs.lib")


extern const char* ErrorString(HRESULT id);
#if _DEBUG
#define DXERR(x) { HRESULT _hr=(x); if(FAILED(_hr)) Fatal("%s(%d): D3D call failed: %s\nCall: %s\n",__FILE__,__LINE__,ErrorString(_hr),#x); }
#else
#define DXERR(x) { HRESULT _hr=(x); if(FAILED(_hr)) Fatal("%s(%d): D3D call failed: %s\n",__FILE__,__LINE__,ErrorString(_hr)); }
#endif


struct OutputDef
{
    String DisplayName;
    RCPtr<IDXGIAdapter4> Adapter;
    RCPtr<IDXGIOutput6> Output;
};

RCPtr<IDXGIFactory6> Factory;
Array<OutputDef> AllOutputs;
OutputDef Output;

//RCPtr<IDXGISwapChain4> SwapChain;
RCPtr<ID3D11Device5> Dev;
RCPtr<ID3D11DeviceContext4> Ctx;
RCPtr<IDXGIOutputDuplication> Dupl;

D3D_FEATURE_LEVEL FeatureLevel;

//extern ScreenMode screenMode;

RCPtr<ID3D11SamplerState> SmplWrap;

static double captureFrameCount = 0;
static int64 lastFrameTime = 0;

static DXGI_FORMAT GetDXGIFormat(PixelFormat fmt)
{
    switch (fmt)
    {
    case PixelFormat::R8: return DXGI_FORMAT_R8_UNORM;
    case PixelFormat::R16: return DXGI_FORMAT_R16_UNORM;
    case PixelFormat::R16F: return DXGI_FORMAT_R16_FLOAT;
    case PixelFormat::R16I: return DXGI_FORMAT_R16_UINT;
    case PixelFormat::R32F: return DXGI_FORMAT_R32_FLOAT;
    case PixelFormat::R32I: return DXGI_FORMAT_R32_UINT;
    case PixelFormat::RG8: return DXGI_FORMAT_R8G8_UNORM;
    case PixelFormat::RG16: return DXGI_FORMAT_R16G16_UNORM;
    case PixelFormat::RG16F: return DXGI_FORMAT_R16G16_FLOAT;
    case PixelFormat::RG16I: return DXGI_FORMAT_R16G16_UINT;
    case PixelFormat::RG32F: return DXGI_FORMAT_R32G32_FLOAT;
    case PixelFormat::RG32I: return DXGI_FORMAT_R32G32_UINT;
    case PixelFormat::RGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::RGBA8sRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case PixelFormat::RGBA16: return DXGI_FORMAT_R16G16B16A16_UNORM;
    case PixelFormat::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case PixelFormat::RGBA16I: return DXGI_FORMAT_R16G16B16A16_UINT;
    case PixelFormat::RGBA32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case PixelFormat::RGBA32I: return DXGI_FORMAT_R32G32B32A32_UINT;
    case PixelFormat::BGRA8: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case PixelFormat::BGRA8sRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case PixelFormat::RGB10A2: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case PixelFormat::D32F: return DXGI_FORMAT_D32_FLOAT;
    case PixelFormat::D24S8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

static int GetBitsPerPixel(PixelFormat fmt)
{
    switch (fmt)
    {
    case PixelFormat::R8: return 8;
    case PixelFormat::R16: return 16;
    case PixelFormat::R16F: return 16;
    case PixelFormat::R16I: return 16;
    case PixelFormat::R32F: return 32;
    case PixelFormat::R32I: return 32;
    case PixelFormat::RG8: return 16;
    case PixelFormat::RG16: return 32;
    case PixelFormat::RG16F: return 32;
    case PixelFormat::RG16I: return 32;
    case PixelFormat::RG32F: return 64;
    case PixelFormat::RG32I: return 64;
    case PixelFormat::RGBA8: return 32;
    case PixelFormat::RGBA8sRGB: return 32;
    case PixelFormat::RGBA16: return 64;
    case PixelFormat::RGBA16F: return 64;
    case PixelFormat::RGBA16I: return 64;
    case PixelFormat::RGBA32F: return 128;
    case PixelFormat::RGBA32I: return 128;
    case PixelFormat::BGRA8: return 32;
    case PixelFormat::BGRA8sRGB: return 32;
    case PixelFormat::RGB10A2: return 32;
    case PixelFormat::D32F: return 32;
    case PixelFormat::D24S8: return 32;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

static PixelFormat GetPixelFormat(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_R8_UNORM: return PixelFormat::R8;
    case DXGI_FORMAT_R16_UNORM: return PixelFormat::R16;
    case DXGI_FORMAT_R16_FLOAT: return PixelFormat::R16F;
    case DXGI_FORMAT_R16_UINT: return PixelFormat::R16I;
    case DXGI_FORMAT_R32_FLOAT: return PixelFormat::R32F;
    case DXGI_FORMAT_R32_UINT: return PixelFormat::R32I;
    case DXGI_FORMAT_R8G8_UNORM: return PixelFormat::RG8;
    case DXGI_FORMAT_R16G16_UNORM: return PixelFormat::RG16;
    case DXGI_FORMAT_R16G16_FLOAT: return PixelFormat::RG16F;
    case DXGI_FORMAT_R16G16_UINT: return PixelFormat::RG16I;
    case DXGI_FORMAT_R32G32_FLOAT: return PixelFormat::RG32F;
    case DXGI_FORMAT_R32G32_UINT: return PixelFormat::RG32I;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return PixelFormat::RGBA8;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return PixelFormat::RGBA8sRGB;
    case DXGI_FORMAT_R16G16B16A16_UNORM: return PixelFormat::RGBA16;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return PixelFormat::RGBA16F;
    case DXGI_FORMAT_R16G16B16A16_UINT: return PixelFormat::RGBA16I;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return PixelFormat::RGBA32F;
    case DXGI_FORMAT_R32G32B32A32_UINT: return PixelFormat::RGBA32I;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return PixelFormat::BGRA8;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return PixelFormat::BGRA8sRGB;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return PixelFormat::RGB10A2;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return PixelFormat::D24S8;
    case DXGI_FORMAT_D32_FLOAT: return PixelFormat::D32F;
    default: return PixelFormat::None;
    }
}


static TexturePara GetTexPara(RCPtr<ID3D11Texture2D> tex)
{
    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);
    return {
        .sizeX = desc.Width,
        .sizeY = desc.Height,
        .format = GetPixelFormat(desc.Format),
    };
}


double Time = 0;

RCPtr<ID3D11Texture2D> backBuffer;
RCPtr<ID3D11RenderTargetView> bbRTV;


struct ShaderResource::SR
{
    RCPtr<ID3D11ShaderResourceView> srv;
    RCPtr<ID3D11UnorderedAccessView> uav;
};


struct Texture::Priv
{
    RCPtr<ID3D11Texture2D> tex;
    RCPtr<ID3D11RenderTargetView> rtv;   
    RCPtr<ID3D11DepthStencilView> dsv;
    RCPtr<ID3D11ShaderResourceView> srv_ui; // sRGB!

    SR sr;

    bool noPool = false;

    void Clear()
    {
        tex.Clear();
        rtv.Clear();
        srv_ui.Clear();
        sr.srv.Clear();
    }

    ID3D11RenderTargetView *GetRTV()
    {
        if (!rtv)
            DXERR(Dev->CreateRenderTargetView(tex, nullptr, rtv));
        return rtv;
    }

    ID3D11DepthStencilView* GetDSV()
    {
        if (!dsv)
            DXERR(Dev->CreateDepthStencilView(tex, nullptr, dsv));
        return dsv;
    }

    ID3D11ShaderResourceView* GetSRV_UI()
    {
        if (!srv_ui)
            DXERR(Dev->CreateShaderResourceView(tex, NULL, srv_ui));
        return srv_ui;
    }
};

Texture::Texture() : P(new Priv) {}
Texture::~Texture() { delete P; }

RCPtr<ID3D11Texture2D> Texture::GetTex2D() const { return P->tex; }

ShaderResource::SR& Texture::GetSR(bool write) 
{
    ASSERT(!write);
    if (!P->sr.srv)
        DXERR(Dev->CreateShaderResourceView(P->tex, NULL, P->sr.srv));
    return P->sr;
}

void Texture::CopyFrom(RCPtr<Texture> tex) const
{
    Ctx->CopyResource(P->tex, tex->P->tex);
}

struct Shader::Priv
{
    Type type = Type::None;
    RCPtr<Buffer> code; 

    RCPtr<ID3D11VertexShader> vs;
    RCPtr<ID3D11HullShader> hs;
    RCPtr<ID3D11DomainShader> ds;
    RCPtr<ID3D11GeometryShader> gs;
    RCPtr<ID3D11PixelShader> ps;
    RCPtr<ID3D11ComputeShader> cs;

    struct Layout
    {
        const void* key;
        RCPtr<ID3D11InputLayout> layout;
    };
    Array<Layout> Layouts;
};

Shader::Shader(Type t, RCPtr<Buffer> code)
{
    P = new Priv();
    P->code = code;
    P->type = t;

    switch (t)
    {
    case Type::Vertex: DXERR(Dev->CreateVertexShader(P->code->Ptr(), P->code->Len(), NULL, P->vs)); break;
    case Type::Hull: DXERR(Dev->CreateHullShader(P->code->Ptr(), P->code->Len(), NULL, P->hs)); break;
    case Type::Domain: DXERR(Dev->CreateDomainShader(P->code->Ptr(), P->code->Len(), NULL, P->ds)); break;
    case Type::Geometry: DXERR(Dev->CreateGeometryShader(P->code->Ptr(), P->code->Len(), NULL, P->gs)); break;
    case Type::Pixel: DXERR(Dev->CreatePixelShader(P->code->Ptr(), P->code->Len(), NULL, P->ps)); break;
    case Type::Compute: DXERR(Dev->CreateComputeShader(P->code->Ptr(), P->code->Len(), NULL, P->cs)); break;
    }    
}

Shader::~Shader()
{
    delete P;
}

RCPtr<Shader> CompileShader(Shader::Type type, ReadOnlySpan<char> source, const char* entryPoint, const char* name, ReadOnlySpan<ShaderDefine> macros)
{
    const char* target = nullptr;
    if (!name) name = entryPoint;

    switch (type)
    {
    case Shader::Type::Compute: target = "cs_5_0"; break;
    case Shader::Type::Domain: target = "ds_5_0"; break;
    case Shader::Type::Geometry: target = "gs_5_0"; break;
    case Shader::Type::Hull: target = "hs_5_0"; break;
    case Shader::Type::Pixel: target = "ps_5_0"; break;
    case Shader::Type::Vertex: target = "vs_5_0"; break;
    default: ASSERT0("unknown shader type");
    }

    Array<D3D_SHADER_MACRO> d3dmacros(macros.Len());
    for (auto& m : macros)
        d3dmacros += D3D_SHADER_MACRO{ m.name, m.value };
    d3dmacros += D3D_SHADER_MACRO{ NULL, NULL };

    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    RCPtr<ID3DBlob> code, errors;
    HRESULT hr = D3DCompile(source.Ptr(), source.Len(), name, &d3dmacros[0], NULL, entryPoint, target, flags, 0, code, errors);

    if (errors.IsValid())
        DPrintF("\n%s\n", errors->GetBufferPointer());

    if (FAILED(hr))
        Fatal("Shader compilation of %s failed", name);

    RCPtr<Buffer> buffer = new Buffer(code->GetBufferPointer(), code->GetBufferSize());
    RCPtr<Shader> shader(new Shader(type, buffer));

    return shader;
}


struct GpuBuffer::Priv
{
    Priv(GpuBuffer* b, Type k, Usage m) : gb(b), type(k), usage(m)
    {
        ASSERT(usage != Usage::Dynamic); // guess what you'll have to implement now, LOL
    }

    Type type;
    Usage usage;
    GpuBuffer* gb;
    RCPtr<ID3D11Buffer> buf;
    SR sr;

    operator ID3D11Buffer* () { if (!buf) gb->Commit(); return buf; }

    void GetFlags(uint &bind, uint &misc) const
    {
        switch (type)
        {
        case Type::Constant: bind = D3D11_BIND_CONSTANT_BUFFER; misc = 0; return;
        case Type::Vertex: bind = D3D11_BIND_VERTEX_BUFFER; misc = 0; return;
        case Type::Index: bind = D3D11_BIND_INDEX_BUFFER; misc = 0; return;
        case Type::Structured:
            bind = D3D11_BIND_SHADER_RESOURCE;
            if (usage != Usage::Immutable) bind |= D3D11_BIND_UNORDERED_ACCESS;
            misc = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            return;

        case Type::ByteBuffer:
            bind = D3D11_BIND_SHADER_RESOURCE;
            if (usage != Usage::Immutable) bind |= D3D11_BIND_UNORDERED_ACCESS;
            misc = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
            return;
        }

        ASSERT0("unknown buffer type");
    }

};

GpuBuffer::GpuBuffer(Type type, Usage usage) { P = new Priv(this, type, usage); }
GpuBuffer::~GpuBuffer() { delete P; }

void GpuBuffer::Reset() { P->buf.Clear(); }

void GpuBuffer::Upload(const void* data, uint size, uint stride, uint totalsize)
{
    uint bind, misc;
    P->GetFlags(bind, misc);
  
    D3D11_BUFFER_DESC desc =
    {
        .ByteWidth = size,
        .BindFlags = bind,
        .MiscFlags = misc,
        .StructureByteStride = stride,
    };

    if (P->type == Type::ByteBuffer)
        desc.ByteWidth = totalsize;

    switch (P->usage)
    {
    case Usage::Immutable: ASSERT(data);  desc.Usage = D3D11_USAGE_IMMUTABLE; break;
    case Usage::GpuOnly: ASSERT(!data);  desc.Usage = D3D11_USAGE_DEFAULT; break;
    case Usage::Dynamic: desc.Usage = D3D11_USAGE_DYNAMIC; desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; break;
    }

    D3D11_SUBRESOURCE_DATA id =
    {
        .pSysMem = data,
    };

    DXERR(Dev->CreateBuffer(&desc, data ? &id : nullptr, this->P->buf));
}

ShaderResource::SR& GpuBuffer::GetSR(bool write, uint count)
{
    if (count && !write && !P->sr.srv)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc = {
            .Format = (P->type == Type::ByteBuffer) ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
        };
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = (P->type == Type::ByteBuffer) ? (count + 3) / 4 : count;

        DXERR(Dev->CreateShaderResourceView(*P, &desc, P->sr.srv));
    }
    if (count && write && !P->sr.uav)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {
            .Format = (P->type == Type::ByteBuffer) ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
        };
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = (P->type == Type::ByteBuffer) ? (count + 3) / 4 : count;
        desc.Buffer.Flags = (P->type == Type::ByteBuffer) ? D3D11_BUFFER_UAV_FLAG_RAW : 0;
        DXERR(Dev->CreateUnorderedAccessView(*P, &desc, P->sr.uav));
    }
    return P->sr;
}

RCPtr<ID3D11Buffer> GpuBuffer::GetBuffer() const { return P->buf; }

template<typename T> uint MakeLayout(D3D11_INPUT_ELEMENT_DESC* desc);

static constexpr D3D11_INPUT_ELEMENT_DESC MakeVBDesc(const char* semantic, uint index, DXGI_FORMAT format, uint offset, uint slot = 0)
{
    return D3D11_INPUT_ELEMENT_DESC {
        .SemanticName = semantic,
        .SemanticIndex = index,
        .Format = format,
        .InputSlot = slot,
        .AlignedByteOffset = offset,
        .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
    };
}

//------------------------------------------------------------------------------------------------

template<> uint MakeLayout<VertexC>(D3D11_INPUT_ELEMENT_DESC* desc)
{
    desc[0] = MakeVBDesc("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, offsetof(VertexC, pos));
    desc[1] = MakeVBDesc("COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, offsetof(VertexC, color));
    return 2;
}

template void Geometry<VertexC>::Draw(GState& , const GBindings&, int);


template<> uint MakeLayout<VertexCT>(D3D11_INPUT_ELEMENT_DESC* desc)
{
    desc[0] = MakeVBDesc("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, offsetof(VertexCT, pos));
    desc[1] = MakeVBDesc("COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, offsetof(VertexCT, color));
    desc[2] = MakeVBDesc("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, offsetof(VertexCT, uv));
    return 3;
}

template void Geometry<VertexCT>::Draw(GState&, const GBindings&, int);

//------------------------------------------------------------------------------------------------

template <typename TV> void SetVertexLayout(RCPtr<Shader> vs)
{
    RCPtr<ID3D11InputLayout> layout;

    D3D11_INPUT_ELEMENT_DESC iedesc[16];
    uint niedesc = MakeLayout<TV>(iedesc);

    const void* key = &MakeLayout<TV>; // unique per vertex structure
    for (const auto &lay : vs->P->Layouts)
    {
        if (lay.key == key)
        {
            layout = lay.layout;
            break;
        }
    }
    if (!layout)
    {
        DXERR(Dev->CreateInputLayout(iedesc, niedesc, vs->P->code->Ptr(), vs->P->code->Len(), layout));
        Shader::Priv::Layout cl;
        cl.key = key;
        cl.layout = layout;
        vs->P->Layouts += cl;
    }
    
    Ctx->IASetInputLayout(layout);
}

static int GetSRVs(ShaderResource* const sr[], int n, ID3D11ShaderResourceView** srv)
{
    int maxpst = -1;
    for (int i = 0; i < n; i++)
    {
        auto tex = sr[i];
        if (tex)
        {
            srv[i] = tex->GetSR(false).srv;
            maxpst = i;
        }
        else
            break;
    }
    return maxpst + 1;
}

static void DrawInternal(GState& state, const GBindings& binds, int instances, int vc, int ic)
{
    Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11ShaderResourceView* srvs[16] = {};

    Ctx->VSSetShader(state.VS->P->vs, NULL, 0);

    int maxvst = GetSRVs(binds.vsres, 16, srvs);
    if (maxvst > 0)
        Ctx->VSSetShaderResources(0, maxvst, srvs);

    ID3D11Buffer* vscb[4] = {};
    for (int i = 0; i < 4; i++)
    {
        auto cb = binds.vscb[i];
        if (cb)
            vscb[i] = *cb->P;
    }
    Ctx->VSSetConstantBuffers(0, 4, vscb);

    ID3D11RenderTargetView* views[4] = {};
    for (int i = 0; i < 4; i++)
    {
        auto tgt = binds.target[i];
        if (tgt)
        {
            views[i] = tgt->P->GetRTV();
        }
    }

    ID3D11DepthStencilView* dsv = binds.depth ? binds.depth->P->GetDSV() : nullptr;
    Ctx->OMSetRenderTargets(4, views, dsv);

    auto& tp0 = binds.target[0]->para;

    D3D11_VIEWPORT viewport = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = (float)tp0.sizeX,
        .Height = (float)tp0.sizeY,
        .MinDepth = 0,
        .MaxDepth = 1,
    };

    Ctx->RSSetViewports(1, &viewport);
    Ctx->PSSetShader(state.PS->P->ps, NULL, 0);


    int maxpst = GetSRVs(binds.psres, 16, srvs);
    if (maxpst > 0)
        Ctx->PSSetShaderResources(0, maxpst, srvs);

    ID3D11SamplerState* pssmp[1] = { SmplWrap };
    Ctx->PSSetSamplers(0, 1, pssmp);

    ID3D11Buffer* pscb[4] = {};
    for (int i = 0; i < 4; i++)
    {
        auto cb = binds.pscb[i];
        if (cb)
            pscb[i] = *cb->P;
    }
    Ctx->PSSetConstantBuffers(0, 4, pscb);

    if (ic)
    {        
        if (instances > 1)
            Ctx->DrawIndexedInstanced(ic, instances, 0, 0, 0);
        else
            Ctx->DrawIndexed(ic, 0, 0);
    }
    else
    {
        if (instances > 1)
            Ctx->DrawInstanced(vc, instances, 0, 0);
        else
            Ctx->Draw(vc, 0);
    }

    static ID3D11ShaderResourceView* nullsrv[16] = {};
    if (maxvst > 0) Ctx->VSSetShaderResources(0, maxvst, nullsrv);
    if (maxpst > 0) Ctx->PSSetShaderResources(0, maxpst, nullsrv);
}


template <class TV> void Geometry<TV>::Draw(GState& state, const GBindings& binds, int instances)
{
    ID3D11Buffer* vbs[] = { *vb.P };
    UINT vbstrides[] = { vb.Stride() };
    UINT vboffs[] = { 0 };
    Ctx->IASetVertexBuffers(0, 1, vbs, vbstrides, vboffs);

    SetVertexLayout<TV>(state.VS);

    if (ib.Len())
        Ctx->IASetIndexBuffer(*ib.P, DXGI_FORMAT_R16_UINT, 0);

    DrawInternal(state, binds, instances, vb.Len(), ib.Len());
}


/*
ScreenMode GetScreenMode()
{
    return screenMode;
}
*/

static RCPtr<IWICImagingFactory> factory;

struct WICFormatTable
{
    GUID pfGuid;
    PixelFormat format;
};

static WICFormatTable WICFormats[] =
{
    GUID_WICPixelFormat32bppRGBA1010102, PixelFormat::RGB10A2,
    GUID_WICPixelFormat32bppRGBA1010102XR, PixelFormat::RGB10A2,
    GUID_WICPixelFormat32bppR10G10B10A2,PixelFormat::RGB10A2,
    GUID_WICPixelFormat32bppR10G10B10A2HDR10, PixelFormat::RGB10A2,
    GUID_WICPixelFormat32bppRGB, PixelFormat::RGBA8sRGB,
    GUID_WICPixelFormat32bppBGR, PixelFormat::BGRA8sRGB,
    GUID_WICPixelFormat32bppRGBA, PixelFormat::RGBA8sRGB,
    GUID_WICPixelFormat32bppBGRA, PixelFormat::BGRA8sRGB,
    GUID_WICPixelFormat64bppRGB, PixelFormat::RGBA16,
    GUID_WICPixelFormat64bppRGBA, PixelFormat::RGBA16,
    GUID_WICPixelFormat64bppRGBHalf, PixelFormat::RGBA16F,
    GUID_WICPixelFormat64bppRGBAHalf, PixelFormat::RGBA16F,
    GUID_WICPixelFormat128bppRGBFloat, PixelFormat::RGBA32F,
    GUID_WICPixelFormat128bppRGBAFloat, PixelFormat::RGBA32F,
    GUID_WICPixelFormat8bppGray, PixelFormat::R8,
    GUID_WICPixelFormat16bppGray, PixelFormat::R16,
    GUID_WICPixelFormat16bppGrayFixedPoint, PixelFormat::R16,
    GUID_WICPixelFormat16bppGrayHalf, PixelFormat::R16F,
    GUID_WICPixelFormat32bppGrayFloat, PixelFormat::R32F,
    GUID_WICPixelFormat32bppGrayFixedPoint, PixelFormat::R32I,
};

struct WICConvertFormatTable
{
    GUID pfGuid;
    GUID destGuid;
};

static WICConvertFormatTable WICConvertFormats[] =
{
  GUID_WICPixelFormat8bppGray, GUID_WICPixelFormat8bppGray
};

bool TexturePara::Equals(const TexturePara& p) const
{
    return sizeX == p.sizeX && sizeY == p.sizeY && mipmaps == p.mipmaps && format == p.format;
}

RCPtr<Texture> LoadImg(const char *filename)
{
    HRESULT hr;
    if (!factory)
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, factory);
        ASSERT(SUCCEEDED(hr));
    }

    RCPtr<IWICBitmapDecoder> decoder;
    wchar_t fn16[2048];
    MultiByteToWideChar(CP_UTF8, 0, filename,  -1, fn16, 2048);
    hr = factory->CreateDecoderFromFilename(fn16, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder);
    ASSERT(SUCCEEDED(hr));

    RCPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame);
    ASSERT(SUCCEEDED(hr));

    UINT w, h;
    frame->GetSize(&w, &h);
    PixelFormat fmt = PixelFormat::None;
    RCPtr<IWICBitmapSource> source = frame;

    for (;;)
    {
        WICPixelFormatGUID pfguid;
        source->GetPixelFormat(&pfguid);

        for (auto& f : WICFormats)
            if (pfguid == f.pfGuid)
                fmt = f.format;

        if (fmt != PixelFormat::None)
            break;

        GUID pfdest = GUID_WICPixelFormat32bppRGBA;
        for (auto& f : WICConvertFormats)
            if (pfguid == f.pfGuid)
                pfdest = f.destGuid;

        RCPtr<IWICBitmapSource> dest;
        hr = WICConvertBitmapSource(pfdest, source, dest);
        ASSERT(SUCCEEDED(hr));
        source = dest;
    }

    int bpp = GetBitsPerPixel(fmt);
    uint8* buffer = new uint8[2 * w * h * bpp];

    hr = source->CopyPixels(NULL, w * bpp, w * h * bpp, buffer);
    ASSERT(SUCCEEDED(hr));

    return CreateTexture({ .sizeX = w, .sizeY = h, .format = fmt }, buffer);
}

static RCPtr<Texture> CreateTexture(RCPtr<ID3D11Texture2D> &from)
{
    RCPtr<Texture> tex = new Texture();

    tex->P->tex = from;
    D3D11_TEXTURE2D_DESC tdesc;
    tex->P->tex->GetDesc(&tdesc);
    tex->para.sizeX = tdesc.Width;
    tex->para.sizeY = tdesc.Height;
    tex->para.format = GetPixelFormat(tdesc.Format);
    tex->para.mipmaps = tdesc.MipLevels;

    return tex;
}

RCPtr<Texture> CreateTexture(const TexturePara& para, const void* data)
{
    RCPtr<Texture> tex = new Texture();
    tex->para = para;

    D3D11_TEXTURE2D_DESC tdesc = {
        .Width = para.sizeX,
        .Height = para.sizeY,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = GetDXGIFormat(para.format),
        .SampleDesc = { .Count = 1 },
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };

    D3D11_SUBRESOURCE_DATA id =
    {
        .pSysMem = data,
        .SysMemPitch = para.sizeX * GetBitsPerPixel(para.format) / 8,
    };
    DXERR(Dev->CreateTexture2D(&tdesc, &id, tex->P->tex));
    return tex;
}

static Array<Texture::Priv> RTPool;
static Array<Texture::Priv> lastRTPool;

RenderTarget::~RenderTarget() {
    if (!P->noPool)
        RTPool += *P;
}


void GfxInit()
{
    CreateDXGIFactory1(__uuidof(IDXGIFactory6), Factory);

    Array<DISPLAYCONFIG_PATH_INFO> dcPaths;
    Array<DISPLAYCONFIG_MODE_INFO> dcModes;

    {
        // enumerate all display paths
        // Determine how many path and mode structures to allocate
        uint flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
        UINT32 pathCount, modeCount;
        GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount);

        // Allocate the path and mode arrays
        dcPaths.SetSize(pathCount);
        dcModes.SetSize(modeCount);

        // Get all active paths and their modes
        QueryDisplayConfig(flags, &pathCount, &dcPaths[0], &modeCount, &dcModes[0], nullptr);

        // The function may have returned fewer paths/modes than estimated
        dcPaths.SetSize(pathCount);
        dcModes.SetSize(modeCount);
    }

    // enumerate all adapters and outputs
    RCPtr<IDXGIAdapter4> adapter;

    // find first adapter with dedicated video memory. This should select the proper GPU on laptops. 
    for (UINT i = 0; Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC3 adesc;
        adapter->GetDesc3(&adesc);

        String adapterName = String::PrintF("%d: %S", i + 1, adesc.Description);
       
        RCPtr<IDXGIOutput> out0;
        for (UINT oi = 0; SUCCEEDED(adapter->EnumOutputs(oi, out0)); ++oi)
        {
            RCPtr<IDXGIOutput6> output = out0;
            DXGI_OUTPUT_DESC1 odesc;
            output->GetDesc1(&odesc);

            String name;

            for (auto& path : dcPaths)
            {
                // Find the target (monitor) friendly name
                DISPLAYCONFIG_SOURCE_DEVICE_NAME srcName = { 
                    .header = {
                        .type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,
                        .size = sizeof(srcName),
                        .adapterId = path.sourceInfo.adapterId,
                        .id = path.sourceInfo.id,
                    }
                };
                DisplayConfigGetDeviceInfo(&srcName.header);

                if (!wcscmp(srcName.viewGdiDeviceName, odesc.DeviceName))
                {
                    DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {
                        .header = {
                            .type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME,
                            .size = sizeof(targetName),
                            .adapterId = path.targetInfo.adapterId,
                            .id = path.targetInfo.id,
                        }
                    };
                    DisplayConfigGetDeviceInfo(&targetName.header);
                    name = targetName.monitorFriendlyDeviceName;
                    break;
                }
            }

            if (!name)
            {
                DISPLAY_DEVICEW dd = { .cb = sizeof(dd) };
                if (EnumDisplayDevicesW(odesc.DeviceName, 0, &dd, 0))
                    name = dd.DeviceString;
            }

            name = String::PrintF("%d: %s (%s)", oi+1, (const char*)name, (const char*)adapterName);
            AllOutputs += OutputDef{ .DisplayName = name, .Adapter = adapter, .Output = output, };
        }
    }
}

void GetVideoOutputs(Array<String>& into)
{
    into.Clear();
    for (auto& out : AllOutputs)
        into += out.DisplayName;
}

void InitD3D(int outputIndex)
{
    timeBeginPeriod(1);

    Output = AllOutputs[outputIndex];
   
    // create device and upgrade
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    UINT flags = 0;
 #ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
 #endif
    RCPtr<ID3D11Device> dev0;
    RCPtr<ID3D11DeviceContext> ctx0;
    DXERR(D3D11CreateDevice(Output.Adapter, Output.Adapter.IsValid() ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE , NULL, flags, levels, _countof(levels), D3D11_SDK_VERSION, dev0, &FeatureLevel, ctx0));
    Dev = dev0;
    Ctx = ctx0;

    captureFrameCount = 0;
    lastFrameTime = 0;
    
    /*
    // window description
    DXGI_SWAP_CHAIN_DESC1 sd = 
    {
        .Width = screenMode.width,
        .Height = screenMode.height,
        .Format = DXGI_FORMAT_R10G10B10A2_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {.Count = 1, .Quality = 0,},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = 0,
    };

    // get swapchain and upgrade
    RCPtr<IDXGISwapChain1> sc1;
    DXERR(Factory->CreateSwapChainForHwnd(Dev, hWnd, &sd, nullptr, nullptr, sc1));
    SwapChain = sc1;

    sc1->GetDesc1(&sd);
    screenMode.width = sd.Width;
    screenMode.height = sd.Height;

    D3D11_SAMPLER_DESC smpdesc = {
        .Filter = D3D11_FILTER_ANISOTROPIC,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .MaxAnisotropy = 16,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    DXERR(Dev->CreateSamplerState(&smpdesc, SmplWrap));
    */
}

void ExitD3D()
{
    RTPool.Clear();
    lastRTPool.Clear();
    timeEndPeriod(1);

    Output = {};
    SmplWrap.Clear();
    Dupl.Clear();
    Ctx.Clear();
    Dev.Clear();
}

RCPtr<IDXGIAdapter> GetAdapter() { return Output.Adapter; }

static RCPtr<Texture> capTex;
static DXGI_OUTPUT_DESC1 outdesc;
static DXGI_OUTDUPL_DESC odd;
static double totalError = 0;

static const DXGI_FORMAT scanoutFormats[] = {
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM,
};

bool CaptureFrame(int timeoutMs, CaptureInfo& ci)
{
    HRESULT hr;

    // get output duplication object
    if (!Dupl.IsValid())
    {
        hr = Output.Output->DuplicateOutput1(Dev, 0, _countof(scanoutFormats), scanoutFormats, Dupl);
        if (hr == DXGI_ERROR_UNSUPPORTED)
            hr = Output.Output->DuplicateOutput(Dev, Dupl);

        if (hr == E_ACCESSDENIED)
        {
            // so far this only happens in the middle of a mode switch, try again later
            Sleep(timeoutMs);
            return false;
        }
        DXERR(hr);

        Dupl->GetDesc(&odd);
        Output.Output->GetDesc1(&outdesc);
        totalError = 0;
        //printf("new dupl %dx%d @ %d:%d\n", odd.ModeDesc.Width, odd.ModeDesc.Height, odd.ModeDesc.RefreshRate.Numerator, odd.ModeDesc.RefreshRate.Denominator);
    }

    // try to get next frame
    RCPtr<IDXGIResource> frame;
    DXGI_OUTDUPL_FRAME_INFO info = {};

    static LARGE_INTEGER t1{};

    for (;;)
    { 
        if (!t1.QuadPart) QueryPerformanceCounter(&t1);
        hr = Dupl->AcquireNextFrame(timeoutMs, &info, frame);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
            return false;
        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL)
        {
            DPrintF("Lost display interface!\n");
            // we lost the interface or it has somehow become invalid, bail and try again next time
            capTex.Clear();
            Dupl.Clear();
            Sleep(timeoutMs);
            return false;
        }
        DXERR(hr);

        // have we got a frame?
        if (info.LastPresentTime.QuadPart)
            break;

        ReleaseFrame();
    }

    LARGE_INTEGER t2;
    QueryPerformanceCounter(&t2);

    LARGE_INTEGER qpf;
    QueryPerformanceFrequency(&qpf);
    if (!lastFrameTime)
        lastFrameTime = info.LastPresentTime.QuadPart;
    double delta = (double)(info.LastPresentTime.QuadPart - lastFrameTime) / (double)qpf.QuadPart;
    lastFrameTime = info.LastPresentTime.QuadPart;

#ifdef _DEBUG    
    static int frc = 0;
    static double lastt1 = 0, lastt2 = 0;
    double t1d = ((double)t1.QuadPart / (double)qpf.QuadPart);
    double t2d = ((double)t2.QuadPart / (double)qpf.QuadPart);
    DPrintF("%5d: t1 %.3f (%.3f), t2 %.3f (%.3f), delta %.3f ", frc++, t1d, t1d-lastt1, t2d, t2d-lastt2, delta);
    lastt1 = t1d;
    lastt2 = t2d;
#endif
    t1.QuadPart = 0;

    if (delta < 0)
    {
        DPrintF("Negative delta!\n");
        ReleaseFrame();
        return false;
    }

    double fdelta = delta * odd.ModeDesc.RefreshRate.Numerator / odd.ModeDesc.RefreshRate.Denominator;
    double fdi = round(fdelta);
    double error = fdelta - fdi;
    totalError += error;
    int comp = 0;
    if (totalError >= 0.75)
    {
        comp = 1;
        totalError -= 1;
    }
    if (totalError <= -0.75)
    {
        comp = -1;
        totalError += 1;
    }
    captureFrameCount += fdi + comp;

    DPrintF("fd %.3f (%.3f, total %.3f) comp %d\n", fdi, error, totalError, comp);

    // create/invalidate texture object
    RCPtr<ID3D11Texture2D> tex = frame;
    if (tex.IsValid() && capTex.IsValid() && (ID3D11Texture2D*)tex != (ID3D11Texture2D*)capTex->P->tex)
        capTex.Clear();
    if (!capTex.IsValid())
        capTex = CreateTexture(tex);

    ci.tex = capTex;
    ci.sizeX = ci.tex->para.sizeX;
    ci.sizeY = ci.tex->para.sizeY;
    ci.isHdr = (outdesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    ci.rateNum = odd.ModeDesc.RefreshRate.Numerator;
    ci.rateDen = odd.ModeDesc.RefreshRate.Denominator;
    ci.frameCount = (uint64)round(captureFrameCount);
    ci.time = (double)info.LastPresentTime.QuadPart / (double)qpf.QuadPart;
    return true;
}

void ReleaseFrame()
{
    Dupl->ReleaseFrame();
}

void Clear(RenderTarget* rt, Vec4 color)
{
    Ctx->ClearRenderTargetView(rt->P->GetRTV(), color);
}

void ClearDepth(RenderTarget* rt, float d)
{
    Ctx->ClearDepthStencilView(rt->P->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, d, 0);
}

RCPtr<RenderTarget> AcquireRenderTarget(const TexturePara &para)
{
    //if (!para.sizeX) para.sizeX = screenMode.width;
    //if (!para.sizeY) para.sizeY = screenMode.height;

    Texture::Priv tex;
    for (int i = 0; i < RTPool.Len(); i++)
        if (GetTexPara(RTPool[i].tex) == para)
        {
            tex = RTPool.RemAtUnordered(i);
            break;
        }

    if (!tex.tex)
        for (int i = 0; i < lastRTPool.Len(); i++)
            if (GetTexPara(lastRTPool[i].tex) == para)
            {
                tex = lastRTPool.RemAtUnordered(i);
                break;
            }

    if (!tex.tex)
    {
        D3D11_TEXTURE2D_DESC tdesc = {
            .Width = para.sizeX,
            .Height = para.sizeY,
            .MipLevels = para.mipmaps,
            .ArraySize = 1,
            .Format = GetDXGIFormat(para.format),
            .SampleDesc = { .Count = 1, },
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = (uint)((para.format > PixelFormat::MAX_FMT) ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS),
        };
        DXERR(Dev->CreateTexture2D(&tdesc, NULL, tex.tex));
    }

    RCPtr<RenderTarget> rt = new RenderTarget();
    *rt->P = tex;
    rt->para = para;
    return rt;
}

RCPtr<RenderTarget> AcquireBackBuffer()
{
    RCPtr<RenderTarget> rt = new RenderTarget();
    rt->para = GetTexPara(backBuffer);
    rt->P->tex = backBuffer;
    rt->P->rtv = bbRTV;
    rt->P->noPool = true;
    return rt;
}

void Dispatch(Shader* shader, const CBindings& binds, uint gx, uint gy, uint gz)
{
    ASSERT(shader && shader->P->type == Shader::Type::Compute);

    ID3D11ShaderResourceView* srvs[16] = {};

    Ctx->CSSetShader(shader->P->cs, NULL, 0);

    int maxcst = GetSRVs(binds.res, 16, srvs);
    if (maxcst > 0)
        Ctx->CSSetShaderResources(0, maxcst, srvs);

    ID3D11Buffer* vscb[4] = {};
    for (int i = 0; i < 4; i++)
    {
        auto cb = binds.cb[i];
        if (cb)
            vscb[i] = *cb->P;
    }
    Ctx->CSSetConstantBuffers(0, 4, vscb);

    ID3D11UnorderedAccessView* uavs[16] = {};
    UINT inicounts[16] = {};
    int maxuav = -1;
    for (int i = 0; i < 16; i++)
    {
        auto tex = binds.uav[i];
        if (tex)
        {
            uavs[i] = tex->GetSR(true).uav;
            maxuav = i;
        }
        else
            break;
    }
    maxuav++;
    Ctx->CSSetUnorderedAccessViews(0, maxuav, uavs, inicounts);

    Ctx->Dispatch(gx, gy, gz);

    static ID3D11ShaderResourceView* nullsrv[16] = {};
    static ID3D11UnorderedAccessView* nulluav[16] = {};
    if (maxcst > 0) Ctx->CSSetShaderResources(0, maxcst, nullsrv);
    if (maxuav > 0) Ctx->CSSetUnorderedAccessViews(0, maxuav, nulluav, nullptr);

}

//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include "graphics.h"
#include "encode.h"
#include "screencapture.h"

#pragma warning (disable: 4996) // deprecated GUIDs in nvEncodeAPI.h that are actually the only ones that work

#include <d3d11.h>
#include <nvEncodeAPI.h>
#include <cuda.h>
#include <cudaD3D11.h>
#pragma comment (lib, "cuda.lib")
#pragma comment (lib, "cudart.lib")

static bool Inited = false;
static NV_ENCODE_API_FUNCTION_LIST API = {};

#if _DEBUG
#define CUDAERR(x) { auto ret = (x); if(ret != CUDA_SUCCESS) { const char *err; cuGetErrorString(ret, &err); Fatal("%s(%d): CUDA call failed: %s\nCall: %s\n",__FILE__,__LINE__,err,#x); } }
#define NVERR(x) { if((x)!= NV_ENC_SUCCESS) Fatal("%s(%d): NVENC call failed: %s\nCall: %s\n",__FILE__,__LINE__,API.nvEncGetLastErrorString(Encoder),#x); }
#else
#define CUDAERR(x) { auto _ret = (x);  if(_ret != CUDA_SUCCESS) { const char *err; cuGetErrorString(x, &err); Fatal("%s(%d): CUDA call failed (%08x)",__FILE__,__LINE__,_ret); } }
#define NVERR(x) { auto _ret=(x);  if(_ret != NV_ENC_SUCCESS) Fatal("%s(%d): NVENC call failed (%08x)",__FILE__,__LINE__,_ret); }
#endif


struct ProfileDef
{
    GUID encodeGuid, profileGuid;
};

static const ProfileDef Profiles[] =
{
    { NV_ENC_CODEC_H264_GUID, NV_ENC_H264_PROFILE_MAIN_GUID },
    { NV_ENC_CODEC_H264_GUID, NV_ENC_H264_PROFILE_HIGH_GUID },
    { NV_ENC_CODEC_H264_GUID, NV_ENC_H264_PROFILE_HIGH_444_GUID },
    { NV_ENC_CODEC_HEVC_GUID, NV_ENC_HEVC_PROFILE_MAIN_GUID },
    { NV_ENC_CODEC_HEVC_GUID, NV_ENC_HEVC_PROFILE_MAIN10_GUID },
    { NV_ENC_CODEC_HEVC_GUID, NV_ENC_HEVC_PROFILE_MAIN_GUID },
    { NV_ENC_CODEC_HEVC_GUID, NV_ENC_HEVC_PROFILE_MAIN10_GUID },
};


class Encode_NVENC : public IEncode
{
    struct Frame
    {
        uint Used = 0;
        CUdeviceptr Buffer;
        double Time;

        NV_ENC_MAP_INPUT_RESOURCE Map = {};
    };

    struct OutBuffer
    {
        Frame* frame = nullptr;
        ThreadEvent event;
        NV_ENC_OUTPUT_PTR buffer = nullptr;
    };

    const VideoCodecConfig& Config;

    Queue<Frame*, 32> FreeFrames;
    Queue<OutBuffer*, 32> FreeBuffers;
    Queue<OutBuffer*, 32> EncodingBuffers;

    Frame* CurrentFrame = nullptr;
    OutBuffer* CurrentBuffer = nullptr;

    void* Encoder = nullptr;
    NV_ENC_BUFFER_FORMAT EncodeFormat = {};
    ThreadEvent EncodeEvent;

    uint SizeX = 0;
    uint SizeY = 0;
    uint FrameNo = 0;

    // intermediate texture (needed bc CUDA won't register shared textures)
    RCPtr<GpuByteBuffer> InBuffer;

    CUgraphicsResource TexResource = nullptr;
    CUcontext CudaContext = nullptr;

    Frame *AcquireFrame()
    {
        Frame* frame = nullptr;
        if (!FreeFrames.Dequeue(frame))
        {

            frame = new Frame
            {
                .Used = 1,
            };

            auto fi = GetFormatInfo(GetBufferFormat(), SizeX, SizeY);
            CUDAERR(cuMemAlloc(&frame->Buffer, (size_t)fi.pitch * fi.lines));

            NV_ENC_REGISTER_RESOURCE reg =
            {
                .version = NV_ENC_REGISTER_RESOURCE_VER,
                .resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR,
                .width = SizeX,
                .height = SizeY,
                .pitch = fi.pitch,
                .resourceToRegister = (void*)frame->Buffer,
                .bufferFormat = EncodeFormat,
                .bufferUsage = NV_ENC_INPUT_IMAGE,
            };
            NVERR(API.nvEncRegisterResource(Encoder, &reg));

            frame->Map.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
            frame->Map.registeredResource = reg.registeredResource;
        }

        if (frame->Map.mappedResource)
        {
            NVERR(API.nvEncUnmapInputResource(Encoder, frame->Map.mappedResource));
            frame->Map.mappedResource = nullptr;
        }

        return frame;
    }

    void ReleaseFrame(Frame*& frame)
    {
        if (!frame) return;

        if (!AtomicDec(frame->Used))
        {
            FreeFrames.Enqueue(frame);
        }
        frame = nullptr;
    }

    OutBuffer* AcquireOutBuffer()
    {
        OutBuffer* buffer;
        if (!FreeBuffers.Dequeue(buffer))
        {           
            NV_ENC_CREATE_BITSTREAM_BUFFER create
            {
                .version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER,
            };
            NVERR(API.nvEncCreateBitstreamBuffer(Encoder, &create));

            buffer = new OutBuffer
            {
                .buffer = create.bitstreamBuffer,
            };

        }
        return buffer;
    }

    void ReleaseOutBuffer(OutBuffer*& buffer)
    {
        if (!buffer) return;
        FreeBuffers.Enqueue(buffer);
        buffer = nullptr;
    }

    void EncodeFrame()
    {
        OutBuffer* ob = nullptr;

        if (!CurrentFrame) return;

        ob = AcquireOutBuffer();
        ob->frame = CurrentFrame;
        AtomicInc(CurrentFrame->Used);

        auto fi = GetFormatInfo(GetBufferFormat(), SizeX, SizeY);
        NV_ENC_PIC_PARAMS pic =
        {
            .version = NV_ENC_PIC_PARAMS_VER,
            .inputWidth = SizeX,
            .inputHeight = SizeY,
            .inputPitch = fi.pitch,
            .encodePicFlags = 0, // NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS
            .frameIdx = FrameNo,
            .inputTimeStamp = FrameNo,
            .inputDuration = 1,
            .inputBuffer = CurrentFrame->Map.mappedResource,
            .outputBitstream = ob->buffer,
            .completionEvent = ob->event.GetRawEvent(),
            .bufferFmt = EncodeFormat,
            .pictureStruct = NV_ENC_PIC_STRUCT_FRAME,
            .pictureType = NV_ENC_PIC_TYPE_UNKNOWN,
        };

        for (;;)
        {
            auto ret = API.nvEncEncodePicture(Encoder, &pic);
            if (ret == NV_ENC_ERR_ENCODER_BUSY)
            {
                Sleep(1);
                continue;
            }

            NVERR(ret);
            break;
        }

        EncodingBuffers.Enqueue(ob);
        EncodeEvent.Fire();
        FrameNo++;       
    }


public:

    Encode_NVENC(const VideoCodecConfig &cfg) : Config(cfg)
    {
        // init cuda/nvenc api on first run
        if (!Inited)
        {
            // init CUDA
            CUDAERR(cuInit(0));

            // init NVENC API
            auto nvenclib = LoadLibrary("nvEncodeAPI64.dll");
            ASSERT(nvenclib);

            typedef NVENCSTATUS(NVENCAPI* NvEncodeAPIGetMaxSupportedVersion_Type)(uint32_t*);
            typedef NVENCSTATUS(NVENCAPI* NvEncodeAPICreateInstance_Type)(NV_ENCODE_API_FUNCTION_LIST* functionList);
            auto NvEncodeAPIGetMaxSupportedVersion = (NvEncodeAPIGetMaxSupportedVersion_Type)GetProcAddress(nvenclib, "NvEncodeAPIGetMaxSupportedVersion");
            auto NvEncodeAPICreateInstance = (NvEncodeAPICreateInstance_Type)GetProcAddress(nvenclib, "NvEncodeAPICreateInstance");
            
            API.version = NV_ENCODE_API_FUNCTION_LIST_VER;
            NVERR(NvEncodeAPICreateInstance(&API));

            Inited = true;
        }

        // init CUDA
        CUdevice cudaDevice = 0;
        CUDAERR(cuD3D11GetDevice(&cudaDevice, (IDXGIAdapter*)GetAdapter()));
        CUDAERR(cuCtxCreate_v2(&CudaContext, 0, cudaDevice));

        // Create encoder session
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openparams = {
            .version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER,
            .deviceType = NV_ENC_DEVICE_TYPE_CUDA,
            .device = (void*)CudaContext,
            .apiVersion = NVENCAPI_VERSION,
        };
       
        NVERR(API.nvEncOpenEncodeSessionEx(&openparams, &Encoder));
    }

    ~Encode_NVENC()
    {
        Flush();
      
        OutBuffer* ob = nullptr;
        while (FreeBuffers.Dequeue(ob))
            delete ob;

        Frame* f = nullptr;
        while (FreeFrames.Dequeue(f))
        {
            cuMemFree(f->Buffer);
            delete f;
        }

        API.nvEncDestroyEncoder(Encoder);
        cuGraphicsUnregisterResource(TexResource);
        cuCtxDestroy_v2(CudaContext);
    }

    BufferFormat GetBufferFormat()
    {
        switch (Config.Profile)
        {
        case CodecProfile::H264_HIGH_444: case CodecProfile::HEVC_MAIN_444:
            return BufferFormat::YUV444_8;
        case CodecProfile::HEVC_MAIN10:
            return BufferFormat::YUV420_16;
        case CodecProfile::HEVC_MAIN10_444:
            return BufferFormat::YUV444_16;
        default:
            return BufferFormat::NV12;
        }
    }

    void Init(uint sizeX, uint sizeY, uint rateNum, uint rateDen, RCPtr<GpuByteBuffer> buffer) override
    {
        SizeX = sizeX;
        SizeY = sizeY;

        InBuffer = buffer;

        switch (GetBufferFormat())
        {
        case BufferFormat::BGRA8: EncodeFormat = NV_ENC_BUFFER_FORMAT_ARGB; break;
        case BufferFormat::NV12: EncodeFormat = NV_ENC_BUFFER_FORMAT_NV12; break;
        case BufferFormat::YUV444_8: EncodeFormat = NV_ENC_BUFFER_FORMAT_YUV444; break;
        case BufferFormat::YUV420_16: EncodeFormat = NV_ENC_BUFFER_FORMAT_YUV420_10BIT; break;
        case BufferFormat::YUV444_16: EncodeFormat = NV_ENC_BUFFER_FORMAT_YUV444_10BIT; break;
        default:
            ASSERT0("unsupported buffer format");
        }

        CUDAERR(cuGraphicsD3D11RegisterResource(&TexResource, (ID3D11Buffer*)InBuffer->GetBuffer(), CU_GRAPHICS_REGISTER_FLAGS_NONE));
        CUDAERR(cuGraphicsResourceSetMapFlags(TexResource, CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY));

        const ProfileDef profile = Profiles[(int)Config.Profile];

        GUID guids[50];
        uint codecGuidCount;
        NVERR(API.nvEncGetEncodeGUIDCount(Encoder, &codecGuidCount));
        NVERR(API.nvEncGetEncodeGUIDs(Encoder, guids, codecGuidCount, &codecGuidCount));
        // TODO: check if our encodeGuid is in there

        uint presetGuidCount;
        NVERR(API.nvEncGetEncodePresetCount(Encoder, profile.encodeGuid, &presetGuidCount));       
        NVERR(API.nvEncGetEncodePresetGUIDs(Encoder, profile.encodeGuid, guids, 50, &presetGuidCount));

        GUID presetGuid;
        if (profile.encodeGuid == NV_ENC_CODEC_HEVC_GUID)
        {
            if (sizeX <= 1920 && sizeY <= 1080)
                presetGuid = NV_ENC_PRESET_P5_GUID;
            else
                presetGuid = NV_ENC_PRESET_P5_GUID;
        }
        else
        {
            presetGuid = NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
        }
        bool found = false;
        for (uint i = 0; i < presetGuidCount; i++)
        {
            if (guids[i] == presetGuid)
            {
                found = true;
                break;
            }
        }
        if (!found)
            presetGuid = guids[0];
      
        // get preset config
        NV_ENC_PRESET_CONFIG presetConfig = 
        {
            .version = NV_ENC_PRESET_CONFIG_VER,
        };

        auto& enccfg = presetConfig.presetCfg;
        enccfg.version = NV_ENC_CONFIG_VER;
        NVERR(API.nvEncGetEncodePresetConfig(Encoder, profile.encodeGuid, presetGuid, &presetConfig));

        // configure
        enccfg.profileGUID = profile.profileGuid;
        enccfg.frameIntervalP = (int)Config.FrameCfg;
        switch (Config.UseBitrateControl)
        {
        case BitrateControl::CONSTQP:
            enccfg.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
            enccfg.rcParams.constQP.qpIntra = enccfg.rcParams.constQP.qpInterB = enccfg.rcParams.constQP.qpInterP = Clamp(Config.BitrateParameter, 1u, 52u);
            break;
        case BitrateControl::CBR:
            enccfg.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
            enccfg.rcParams.averageBitRate = Min(Config.BitrateParameter * 1000, 500u * 1000 * 1000);
            break;
        }

        if (profile.encodeGuid == NV_ENC_CODEC_HEVC_GUID)
        {
            enccfg.encodeCodecConfig.hevcConfig.idrPeriod = enccfg.gopLength = Config.GopSize;
        }
        else
        {
            enccfg.encodeCodecConfig.h264Config.idrPeriod = enccfg.gopLength = Config.GopSize;
        }
       
        // initialize encoder
        NV_ENC_INITIALIZE_PARAMS params =
        {
            .version = NV_ENC_INITIALIZE_PARAMS_VER,
            .encodeGUID = profile.encodeGuid,
            .presetGUID = presetGuid,
            .encodeWidth = SizeX,
            .encodeHeight = SizeY,
            .darWidth = SizeX,
            .darHeight = SizeY,
            .frameRateNum = rateNum,
            .frameRateDen = rateDen,
            .enableEncodeAsync = 1,
            .enablePTD = 1,
            .encodeConfig = &enccfg,
        };

        if (profile.encodeGuid == NV_ENC_CODEC_HEVC_GUID)
            params.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;

        switch (Config.Profile)
        {
        case CodecProfile::H264_HIGH_444:
            enccfg.encodeCodecConfig.h264Config.chromaFormatIDC = 3;
            //enccfg.encodeCodecConfig.h264Config.separateColourPlaneFlag = 1;
            break;
        case CodecProfile::HEVC_MAIN10:
            enccfg.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
            break;
        case CodecProfile::HEVC_MAIN_444:
            enccfg.encodeCodecConfig.hevcConfig.chromaFormatIDC = 3;
            break;
        case CodecProfile::HEVC_MAIN10_444:
            enccfg.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
            enccfg.encodeCodecConfig.hevcConfig.chromaFormatIDC = 3;
            break;
        }

        NVERR(API.nvEncInitializeEncoder(Encoder, &params));

        // prealloc a few frames and buffers
        for (int i = 0; i < 3; i++)
        {
            auto frame = AcquireFrame();
            ReleaseFrame(frame);
            auto buffer = AcquireOutBuffer();
            ReleaseOutBuffer(buffer);
        }
    }

    void SubmitFrame(double time) override
    {
        ReleaseFrame(CurrentFrame);

        // get a frame        
        CurrentFrame = AcquireFrame();
        CurrentFrame->Used = 1;
        CurrentFrame->Time = time;
       
        // copy intermediate texture -> frame
        auto fi = GetFormatInfo(GetBufferFormat(), SizeX, SizeY);
        CUDA_MEMCPY2D copy =
        {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .srcPitch = fi.pitch,
            .dstMemoryType = CU_MEMORYTYPE_DEVICE,
            .dstDevice = CurrentFrame->Buffer,
            .dstPitch = fi.pitch,
            .WidthInBytes = fi.pitch,
            .Height = fi.lines,
        };

        size_t size = 0;
        CUDAERR(cuGraphicsMapResources(1, &TexResource, nullptr));
        CUDAERR(cuGraphicsResourceGetMappedPointer(&copy.srcDevice, &size, TexResource));
        CUDAERR(cuMemcpy2DAsync(&copy, nullptr));
        CUDAERR(cuGraphicsUnmapResources(1, &TexResource, nullptr));

        // submit frame
        NVERR(API.nvEncMapInputResource(Encoder, &CurrentFrame->Map));

        EncodeFrame();
    }

    void DuplicateFrame() override
    {
        EncodeFrame();
    }

    void Flush() override
    {
        ReleaseFrame(CurrentFrame);

        OutBuffer* ob = nullptr;
        while (EncodingBuffers.Peek(ob) && ob->event.Wait(100))
        {
            EncodingBuffers.Dequeue(ob);
            ReleaseFrame(ob->frame);
            ReleaseOutBuffer(ob);
        }
    }

    bool BeginGetPacket(uint8*& data, uint& size, uint timeoutMs, double &time) override
    {
        ASSERT(!CurrentBuffer);
        if (EncodingBuffers.IsEmpty() && !EncodeEvent.Wait(timeoutMs))
            return false;

        EncodeEvent.Wait(0);

        OutBuffer* ob = nullptr;
        if (EncodingBuffers.Peek(ob) && ob->event.Wait(timeoutMs))
        {
            EncodingBuffers.Dequeue(CurrentBuffer);

            NV_ENC_LOCK_BITSTREAM lock
            {
                .version = NV_ENC_LOCK_BITSTREAM_VER,
                .outputBitstream = CurrentBuffer->buffer,
            };

            NVERR(API.nvEncLockBitstream(Encoder, &lock));
            data = (uint8*)lock.bitstreamBufferPtr;
            size = lock.bitstreamSizeInBytes;
            time = CurrentBuffer->frame->Time;
            return true;
        }

        return false;
    }

    void EndGetPacket() override
    {
        if (!CurrentBuffer) return;

        ReleaseFrame(CurrentBuffer->frame);
        NVERR(API.nvEncUnlockBitstream(Encoder, CurrentBuffer->buffer));
        ReleaseOutBuffer(CurrentBuffer);
    }

};

IEncode* CreateEncodeNVENC(const CaptureConfig &cfg) { return new Encode_NVENC(cfg.CodecCfg); }

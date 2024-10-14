//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include "graphics.h"
#include "encode.h"
#include "screencapture.h"

#pragma warning (disable: 4996) // deprecated GUIDs in nvEncodeAPI.h that are actually the only ones that work

#include <d3d11.h>

#include <ffnvcodec/nvEncodeAPI.h>
#include <ffnvcodec/dynlink_cuda.h>
#include <ffnvcodec/dynlink_loader.h>

static bool Inited = false;
static NV_ENCODE_API_FUNCTION_LIST Nvenc = {};
static CudaFunctions *Cuda;

#if _DEBUG
#define CUDAERR(x) { auto ret = (x); if(ret != CUDA_SUCCESS) { const char *err; Cuda->cuGetErrorString(ret, &err); Fatal("%s(%d): CUDA call failed: %s (%d)\nCall: %s\n",__FILE__,__LINE__,err,ret,#x); } }
#define NVERR(x) { auto _ret=(x); if(_ret!= NV_ENC_SUCCESS) Fatal("%s(%d): NVENC call failed: %s (%d)\nCall: %s\n",__FILE__,__LINE__,Nvenc.nvEncGetLastErrorString(Encoder),_ret,#x); }
#else
#define CUDAERR(x) { auto ret = (x); if(ret != CUDA_SUCCESS) { const char *err; Cuda->cuGetErrorString(ret, &err); Fatal("%s(%d): CUDA call failed: %s\n",__FILE__,__LINE__,err); } }
#define NVERR(x) { if((x)!= NV_ENC_SUCCESS) Fatal("%s(%d): NVENC call failed: %s\n",__FILE__,__LINE__,Nvenc.nvEncGetLastErrorString(Encoder)); }
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
    bool IsHDR;

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

    Frame *AcquireFrame(bool alloc = false)
    {
        Frame* frame = nullptr;
        if (alloc ||!FreeFrames.Dequeue(frame))
        {
            frame = new Frame
            {
                .Used = 1,
            };

            auto fi = GetFormatInfo(GetBufferFormat(), SizeX, SizeY);
            CUDAERR(Cuda->cuMemAlloc(&frame->Buffer, (size_t)fi.pitch * fi.lines));

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
            NVERR(Nvenc.nvEncRegisterResource(Encoder, &reg));

            frame->Map.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
            frame->Map.registeredResource = reg.registeredResource;
        }

        if (frame->Map.mappedResource)
        {
            NVERR(Nvenc.nvEncUnmapInputResource(Encoder, frame->Map.mappedResource));
            frame->Map.mappedResource = nullptr;
        }

        frame->Used = 1;
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

    OutBuffer* AcquireOutBuffer(bool alloc = false)
    {
        OutBuffer* buffer;
        if (alloc || !FreeBuffers.Dequeue(buffer))
        {           
            NV_ENC_CREATE_BITSTREAM_BUFFER create
            {
                .version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER,
            };
            NVERR(Nvenc.nvEncCreateBitstreamBuffer(Encoder, &create));

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
            auto ret = Nvenc.nvEncEncodePicture(Encoder, &pic);
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
    Encode_NVENC(const VideoCodecConfig &cfg, bool isHdr) : Config(cfg), IsHDR(isHdr)
    {
        // init cuda/nvenc api on first run
        if (!Inited)
        {
            ASSERT(!cuda_load_functions(&Cuda, nullptr));

            // init CUDA
            CUDAERR(Cuda->cuInit(0));

            NvencFunctions *funcs{};
            ASSERT(!nvenc_load_functions(&funcs, nullptr));

            Nvenc.version = NV_ENCODE_API_FUNCTION_LIST_VER;
            NVERR(funcs->NvEncodeAPICreateInstance(&Nvenc));

            Inited = true;
        }

        // init CUDA
        CUdevice cudaDevice = 0;
        CUDAERR(Cuda->cuD3D11GetDevice(&cudaDevice, (IDXGIAdapter*)GetAdapter()));
        CUDAERR(Cuda->cuCtxCreate(&CudaContext, 0, cudaDevice));

        // Create encoder session
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openparams = {
            .version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER,
            .deviceType = NV_ENC_DEVICE_TYPE_CUDA,
            .device = (void*)CudaContext,
            .apiVersion = NVENCAPI_VERSION,
        };
       
        NVERR(Nvenc.nvEncOpenEncodeSessionEx(&openparams, &Encoder));
    }

    ~Encode_NVENC()
    {
        Flush();

        Frame* f = nullptr;
        while (FreeFrames.Dequeue(f))
        {
            if (f->Map.mappedResource)
                NVERR(Nvenc.nvEncUnmapInputResource(Encoder, f->Map.mappedResource));
            NVERR(Nvenc.nvEncUnregisterResource(Encoder, f->Map.registeredResource));
            Cuda->cuMemFree(f->Buffer);
            delete f;
        }

        OutBuffer* ob = nullptr;
        while (FreeBuffers.Dequeue(ob))
        {
            Nvenc.nvEncDestroyBitstreamBuffer(Encoder, ob->buffer);
            delete ob;
        }

        Nvenc.nvEncDestroyEncoder(Encoder);
        Cuda->cuGraphicsUnregisterResource(TexResource);
        Cuda->cuCtxDestroy(CudaContext);
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

        CUDAERR(Cuda->cuGraphicsD3D11RegisterResource(&TexResource, (ID3D11Buffer*)InBuffer->GetBuffer(), CU_GRAPHICS_REGISTER_FLAGS_NONE));
        //CUDAERR(Cuda->cuGraphicsResourceSetMapFlags(TexResource, CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY));

        if (IsHDR && (Config.Profile != CodecProfile::HEVC_MAIN10 && Config.Profile != CodecProfile::HEVC_MAIN10_444))
        {
            ASSERT0("HDR capture is only supported when using a 10 bits per pixel profile");
        }

        const ProfileDef profile = Profiles[(int)Config.Profile];

        GUID guids[50];
        uint codecGuidCount;
        NVERR(Nvenc.nvEncGetEncodeGUIDCount(Encoder, &codecGuidCount));
        NVERR(Nvenc.nvEncGetEncodeGUIDs(Encoder, guids, codecGuidCount, &codecGuidCount));
        // TODO: check if our encodeGuid is in there

        uint presetGuidCount;
        NVERR(Nvenc.nvEncGetEncodePresetCount(Encoder, profile.encodeGuid, &presetGuidCount));       
        NVERR(Nvenc.nvEncGetEncodePresetGUIDs(Encoder, profile.encodeGuid, guids, 50, &presetGuidCount));

        GUID presetGuid;
        if (profile.encodeGuid == NV_ENC_CODEC_HEVC_GUID)
        {
            if (sizeX <= 1920 && sizeY <= 1080)
                presetGuid = NV_ENC_PRESET_P5_GUID;
            else
                presetGuid = NV_ENC_PRESET_P1_GUID;
        }
        else
        {
            presetGuid = NV_ENC_PRESET_P5_GUID;
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
      
        NV_ENC_PRESET_CONFIG presetConfig = 
        // get preset config
        {
            .version = NV_ENC_PRESET_CONFIG_VER,
        };

        auto& enccfg = presetConfig.presetCfg;
        enccfg.version = NV_ENC_CONFIG_VER;
        NVERR(Nvenc.nvEncGetEncodePresetConfigEx(Encoder, profile.encodeGuid, presetGuid, NV_ENC_TUNING_INFO_LOW_LATENCY, &presetConfig));

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
            auto& vuipara = enccfg.encodeCodecConfig.hevcConfig.hevcVUIParameters;
            vuipara.videoSignalTypePresentFlag = 1;
            vuipara.colourDescriptionPresentFlag = 1;
            if (IsHDR)
            {
                vuipara.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT2020;
                vuipara.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_SMPTE2084;
                vuipara.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT2020_NCL;
            }
            else
            {
                vuipara.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT709;
                vuipara.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_SRGB;
                vuipara.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT709;
            }
        }
        else
        {
            enccfg.encodeCodecConfig.h264Config.idrPeriod = enccfg.gopLength = Config.GopSize;        
            auto& vuipara = enccfg.encodeCodecConfig.h264Config.h264VUIParameters;
            vuipara.videoSignalTypePresentFlag = 1;
            vuipara.colourDescriptionPresentFlag = 1;
            vuipara.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT709;
            vuipara.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_SRGB;
            vuipara.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT709;
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

        NVERR(Nvenc.nvEncInitializeEncoder(Encoder, &params));

        // prealloc a few frames and buffers
        for (int i = 0; i < 3; i++)
        {
            auto frame = AcquireFrame(true);
            ReleaseFrame(frame);
            auto buffer = AcquireOutBuffer(true);
            ReleaseOutBuffer(buffer);
        }
    }

    void SubmitFrame(double time) override
    {
        ReleaseFrame(CurrentFrame);

        // get a frame        
        CurrentFrame = AcquireFrame();
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
        CUDAERR(Cuda->cuGraphicsMapResources(1, &TexResource, nullptr));
        CUDAERR(Cuda->cuGraphicsResourceGetMappedPointer(&copy.srcDevice, &size, TexResource));
        CUDAERR(Cuda->cuMemcpy2DAsync(&copy, nullptr));
        CUDAERR(Cuda->cuGraphicsUnmapResources(1, &TexResource, nullptr));

        // submit frame
        NVERR(Nvenc.nvEncMapInputResource(Encoder, &CurrentFrame->Map));

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

            NVERR(Nvenc.nvEncLockBitstream(Encoder, &lock));
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
        NVERR(Nvenc.nvEncUnlockBitstream(Encoder, CurrentBuffer->buffer));
        ReleaseOutBuffer(CurrentBuffer);
    }

};

IEncode* CreateEncodeNVENC(const CaptureConfig &cfg, bool isHdr) { return new Encode_NVENC(cfg.CodecCfg, isHdr); }

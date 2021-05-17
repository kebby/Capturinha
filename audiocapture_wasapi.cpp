//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include "system.h"
#include "audiocapture.h"
#include "screencapture.h"

#include <stdio.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>

extern const char* ErrorString(HRESULT id);
#if _DEBUG
#define CHECK(x) { HRESULT _hr=(x); if(FAILED(_hr)) Fatal("%s(%d): WASAPI call failed: %s\nCall: %s\n",__FILE__,__LINE__,ErrorString(_hr),#x); }
#else
#define CHECK(x) { HRESULT _hr=(x); if(FAILED(_hr)) Fatal("%s(%d): WASAPI call failed: %s\n",__FILE__,__LINE__,ErrorString(_hr)); }
#endif

static constexpr int REFPERSEC = 10000000;

static Array<RCPtr<IMMDevice>> Devices;

class AudioCapture_WASAPI : public IAudioCapture
{
    const CaptureConfig& Config;

    RCPtr<IAudioClient> Client;
    RCPtr<IAudioCaptureClient> CaptureClient;

    RCPtr<IAudioClient> PlaybackClient;

    WAVEFORMATEXTENSIBLE* Format = nullptr;
    uint BufferSize = 0;

    Thread* CaptureThread = nullptr;

    uint8* Ring = nullptr;
    uint RingSize = 0;
    uint RingRead = 0;
    uint RingWrite = 0;
    uint RingTimePos = 0;
    double RingTimeValue = 0;
    ThreadLock RingLock;

    uint BytesPerSample = 0;
    AudioInfo Info = {};

    void CaptureThreadFunc(Thread& thread)
    {
        const int bufferMs = 1000 * BufferSize / Format->Format.nSamplesPerSec;

        while (thread.Wait(bufferMs / 2))
        {
            uint packetSize = 0;
            CHECK(CaptureClient->GetNextPacketSize(&packetSize));
            while (packetSize)
            {
                uint8* data = nullptr;
                uint samples = 0;
                DWORD flags = 0;
                uint64 qpctime;
                CHECK(CaptureClient->GetBuffer(&data, &samples, &flags, nullptr, &qpctime));
                double time = (double)qpctime / REFPERSEC;

                uint bytes = samples * BytesPerSample;
                uint pos;
                {
                    ScopeLock lock(RingLock);
                    uint avail = RingSize - (RingWrite - RingRead);
                    if (bytes > avail)
                        RingRead += bytes-avail;

                    RingTimePos = RingWrite;
                    RingTimeValue = time;

                    pos = RingWrite % RingSize;
                    RingWrite += bytes;

                    if (RingRead > RingSize)
                    {
                        RingWrite -= RingSize;
                        RingRead -= RingSize;
                        RingTimePos -= RingSize;
                    }
                }

                uint chunk1 = Min(bytes, RingSize - pos);
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                {
                    memset(Ring + pos, 0, chunk1);
                    memset(Ring, 0, bytes - chunk1);
                }
                else
                {
                    memcpy(Ring + pos, data, chunk1);
                    memcpy(Ring, data + chunk1, bytes - chunk1);
                }

                CHECK(CaptureClient->ReleaseBuffer(samples));
                CHECK(CaptureClient->GetNextPacketSize(&packetSize));
            };

        }
    }

public:
    AudioCapture_WASAPI(const CaptureConfig& cfg) : Config(cfg)
    {
        const REFERENCE_TIME duration = REFERENCE_TIME(0.02 * REFPERSEC);

        // init COM
        CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED));

        auto device = Devices[cfg.AudioOutputIndex];
   
        // initialize dummy playback client to keep the device running
        WAVEFORMATEX* outFormat = nullptr;
        uint outBufferSize = 0;
        BYTE* outBuffer;
        CHECK(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, PlaybackClient));
        CHECK(PlaybackClient->GetMixFormat(&outFormat));
        CHECK(PlaybackClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, duration, 0, outFormat, NULL));
        CHECK(PlaybackClient->GetBufferSize(&outBufferSize));
        RCPtr<IAudioRenderClient> renderClient;
        CHECK(PlaybackClient->GetService(__uuidof(IAudioRenderClient), renderClient));
        CHECK(renderClient->GetBuffer(outBufferSize, &outBuffer));
        memset(outBuffer, 0, (size_t)outBufferSize * outFormat->nBlockAlign);
        CHECK(PlaybackClient->Start());

        //  initialize client for loopback recording
        CHECK(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, Client));
        CHECK(Client->GetMixFormat((WAVEFORMATEX**)&Format));

        // TODO? support for non-float samples and other channel configs than stereo
        ASSERT(Format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && Format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
        
        BytesPerSample = Format->Format.nChannels * Format->Format.wBitsPerSample / 8;
        RingSize = Format->Format.nSamplesPerSec * BytesPerSample; // 1 second for now
        Ring = new uint8[RingSize];

        CHECK(Client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, duration, 0, (WAVEFORMATEX*)Format, NULL));
        CHECK(Client->GetBufferSize(&BufferSize));
        CHECK(Client->GetService(__uuidof(IAudioCaptureClient), CaptureClient));

        // ... and go
        CaptureThread = new Thread(Bind(this, &AudioCapture_WASAPI::CaptureThreadFunc));
        CHECK(Client->Start());
    }

    ~AudioCapture_WASAPI()
    {
        delete CaptureThread;
        Client->Stop();
        PlaybackClient->Stop();

        CaptureClient.Clear();
        Client.Clear();
        PlaybackClient.Clear();

        delete[] Ring;

        CoTaskMemFree(Format);
        CoUninitialize();
    }

    AudioInfo GetInfo() const override
    {
        return AudioInfo
        {
            .Format = AudioFormat::F32,
            .Channels = Format->Format.nChannels,
            .SampleRate = Format->Format.nSamplesPerSec,
            .BytesPerSample = BytesPerSample,
        };
    }

    uint Read(uint8* dest, uint size, double &time) override
    {
        ScopeLock lock(RingLock);
        time = RingTimeValue + ((double)RingRead - RingTimePos) / (double)(BytesPerSample * Format->Format.nSamplesPerSec);

        size = Min(size, RingWrite - RingRead);
        uint pos = RingRead % RingSize;
        uint chunk1 = Min(size, RingSize - pos);
        memcpy(dest, Ring + pos, chunk1);
        memcpy(dest + chunk1, Ring, size - chunk1);
        RingRead += size;

        return size;
    }

    void JumpToTime(double time) override
    {
        ScopeLock lock(RingLock);
        int deltasamples = (int)round((time - RingTimeValue) * Format->Format.nSamplesPerSec);
        int destpos = RingTimePos + deltasamples * BytesPerSample;
        //ASSERT(destpos >= (int)RingRead && destpos < (int)RingWrite);
        RingRead = (uint)Clamp<int>(destpos, RingRead, RingWrite);
    }

    void Flush() override
    {
        ScopeLock lock(RingLock);
        RingRead = RingWrite;
    }
};

void InitAudioCapture()
{
    CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED));

    // Acquire Device enumerator
    RCPtr<IMMDeviceEnumerator> enumerator;
    CHECK(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), enumerator));

    // Get default endpoint
    RCPtr<IMMDevice> defltdev;
    CHECK(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, defltdev));
    Devices += defltdev;

    // Enumerate all other endpoints
    RCPtr<IMMDeviceCollection> collection;
    CHECK(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection));
    uint count = 0;
    CHECK(collection->GetCount(&count));
    for (uint i = 0; i < count; i++)
    {
        RCPtr<IMMDevice> dev;
        CHECK(collection->Item(i, dev));
        Devices += dev;
    }
}

void GetAudioDevices(Array<String> &into)
{
    into.Clear();
    bool dflt = true;
    for (auto device : Devices)
    {
        LPWSTR id = nullptr;
        device->GetId(&id);

        RCPtr<IPropertyStore> store;
        device->OpenPropertyStore(STGM_READ, store);
         
        PROPVARIANT varName;
        PropVariantInit(&varName);
        store->GetValue(PKEY_Device_FriendlyName, &varName);
        if (dflt)
        {
            into += "Default output";
            dflt = false;
        }
        else
            into += varName.pwszVal;

        PropVariantClear(&varName);
    }      
}

IAudioCapture* CreateAudioCaptureWASAPI(const CaptureConfig &config) { return new AudioCapture_WASAPI(config); }
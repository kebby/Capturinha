//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#include "system.h"
#include "screencapture.h"
#include "output.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
#include <libswresample\swresample.h>
}

#include <math.h>
#include <stdio.h>

static Array<String> Errors;

#if _DEBUG
static char averrbuf[1024];
#define AVERR(x) { auto _ret=(x); if(_ret<0) { Fatal("%s(%d): libav call failed: %s\n%s\nCall: %s\n",__FILE__,__LINE__,av_make_error_string(averrbuf, 1024, _ret),(const char*)String::Join(Errors,""),#x); } }
#else
#define AVERR(x) { auto _ret=(x); if(_ret<0) { Fatal("%s(%d): libav call failed: %08x\n%s\n",__FILE__,__LINE__,_ret,(const char*)String::Join(Errors,"")); } }
#endif

class Output_LibAV : public IOutput
{
private:

    OutputPara Para;

    AVFormatContext* Context = nullptr;

    AVStream* VideoStream = nullptr;

    AVCodec* AudioCodec = nullptr;
    AVStream* AudioStream = nullptr;
    AVCodecContext* AudioContext = nullptr;
    AVPacket* Packet = nullptr;
    AVFrame* Frame = nullptr;

    SwrContext* Resample = nullptr;
    uint ResampleBufferSize = 0;
    uint8* ResampleBuffer = nullptr;
    uint ResampleBytesPerSample = 0;
    uint ResampleFill = 0;

    int FrameNo = 0;
    int AudioWritten = 0;

    void InitVideo(const uint8 *firstFrame, int firstFrameSize)
    {
        VideoStream = avformat_new_stream(Context, 0);
        VideoStream->id = 0;
        VideoStream->time_base.den = VideoStream->avg_frame_rate.num = Para.RateNum;
        VideoStream->time_base.num = VideoStream->avg_frame_rate.den = Para.RateDen;

        auto codecpar = VideoStream->codecpar;
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->codec_id = Para.CConfig->CodecCfg.Profile >= CodecProfile::HEVC_MAIN ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
        codecpar->bit_rate = Para.CConfig->CodecCfg.UseBitrateControl == BitrateControl::CBR ? Para.CConfig->CodecCfg.BitrateParameter * 1000 : 0;
        codecpar->width = Para.SizeX;
        codecpar->height = Para.SizeY;
        codecpar->bits_per_coded_sample = 24;
        codecpar->color_range = AVCOL_RANGE_MPEG;
        codecpar->color_primaries = AVCOL_PRI_BT709;
        codecpar->color_trc = AVCOL_TRC_BT709;
        codecpar->color_space = AVCOL_SPC_BT709;
        codecpar->chroma_location = AVCHROMA_LOC_UNSPECIFIED;
        codecpar->sample_aspect_ratio.num = codecpar->sample_aspect_ratio.den = 1;
        codecpar->field_order = AV_FIELD_PROGRESSIVE;

        // For h.264 and HEVC, some of the muxers need the first frame
        // in the extradata during encode, so make a copy        
        codecpar->extradata = (uint8*)av_malloc(firstFrameSize);
        codecpar->extradata_size = firstFrameSize;
        memcpy(codecpar->extradata, firstFrame, firstFrameSize);
    }

    void InitAudio()
    {
        if (Para.Audio.Format == AudioFormat::None)
            return;

        // find the audio codec
        static const AVCodecID acodecs[] = { AV_CODEC_ID_PCM_S16LE, AV_CODEC_ID_PCM_F32LE, AV_CODEC_ID_MP3, AV_CODEC_ID_AAC };
        AudioCodec = avcodec_find_encoder(acodecs[(int)Para.CConfig->UseAudioCodec]);
        if (!AudioCodec)
            return;

        // find suitable sample format
        const AVSampleFormat sampleFmt = AudioCodec->sample_fmts[0];
        if (sampleFmt == AV_SAMPLE_FMT_NONE)
            return;

        // init audio stream and codec
        if (sampleFmt != AV_SAMPLE_FMT_NONE)
        {
            AudioContext = avcodec_alloc_context3(AudioCodec);
            AudioContext->sample_fmt = sampleFmt;
            AudioContext->sample_rate = Para.Audio.SampleRate;
            AudioContext->channels = Para.Audio.Channels;
            AudioContext->channel_layout = av_get_default_channel_layout(Para.Audio.Channels);

            if (Para.CConfig->UseAudioCodec >= AudioCodec::MP3)
                AudioContext->bit_rate = Clamp(Para.CConfig->AudioBitrate, 32u, 320u) * 1000;
            else
                AudioContext->bit_rate = Para.Audio.SampleRate * Para.Audio.Channels * av_get_bytes_per_sample(sampleFmt) * 8;

            AVERR(avcodec_open2(AudioContext, AudioCodec, 0));

            AudioStream = avformat_new_stream(Context, AudioCodec);
            AudioStream->id = 1;
            AVERR(avcodec_parameters_from_context(AudioStream->codecpar, AudioContext));
          
            AVSampleFormat sourceFmt = AV_SAMPLE_FMT_NONE;
            switch (Para.Audio.Format)
            {
            case AudioFormat::I16: sourceFmt = AV_SAMPLE_FMT_S16; break;
            case AudioFormat::F32: sourceFmt = AV_SAMPLE_FMT_FLT; break;
            }

            Resample = swr_alloc_set_opts(nullptr, AudioContext->channel_layout, sampleFmt, Para.Audio.SampleRate, AudioContext->channel_layout, sourceFmt, Para.Audio.SampleRate, 0, nullptr);
            ResampleBufferSize = Para.Audio.SampleRate;
            ResampleBytesPerSample = av_get_bytes_per_sample(sampleFmt);
            ResampleBuffer = new uint8[ResampleBufferSize * ResampleBytesPerSample * AudioContext->channels];
            AVERR(swr_init(Resample));
        }
    }

    void WriteAudio()
    {
        while (!avcodec_receive_packet(AudioContext, Packet))
        {
            Packet->pts = av_rescale_q(Packet->pts, AudioContext->time_base, AudioStream->time_base);
            Packet->dts = av_rescale_q(Packet->dts, AudioContext->time_base, AudioStream->time_base);
            Packet->duration = (int)av_rescale_q(Packet->duration, AudioContext->time_base, AudioStream->time_base);
            Packet->stream_index = AudioStream->index;

            // Write the compressed frame to the media file.
            AVERR(av_interleaved_write_frame(Context, Packet));
            av_packet_unref(Packet);
        }
    }

    static void OnLog(void*, int level, const char* format, va_list args)
    {
        static char buffer[4096];
        int len = vsnprintf_s(buffer, 4096, format, args);
        if (len < 0) len = 0;
        buffer[len] = 0;
        if (level <= AV_LOG_WARNING)
            Errors.PushTail(buffer);
        buffer[len] = '\n';
        buffer[len+1] = 0;
        DPrintF(buffer);
    }


public:

    Output_LibAV(const OutputPara& para) : Para(para)
    {       
        Errors.Clear();
        av_log_set_callback(OnLog);

        static const char* const formats[] = { "mp4", "mov", "matroska" };

        AVERR(avformat_alloc_output_context2(&Context, nullptr, formats[(int)para.CConfig->UseContainer] , para.filename));
        AVERR(avio_open(&Context->pb, para.filename, AVIO_FLAG_WRITE));

        Packet = av_packet_alloc();
        Frame = av_frame_alloc();     
    }

    ~Output_LibAV()
    {
        if (AudioContext)
        {
            AVERR(avcodec_send_frame(AudioContext, nullptr));
            WriteAudio();
            delete[] ResampleBuffer;
            swr_free(&Resample);
        }

        AVERR(av_interleaved_write_frame(Context, 0));
        if (!AudioContext || AudioWritten>0) // mkv muxer crashes otherwise...
            AVERR(av_write_trailer(Context));

        avio_close(Context->pb);

        avformat_free_context(Context);
        avcodec_free_context(&AudioContext);

        av_packet_free(&Packet);
        av_frame_free(&Frame);

        av_log_set_callback(nullptr);
    }

    void SubmitVideoPacket(const uint8* data, uint size) override
    {
        if (!VideoStream)
        {
            InitVideo(data, size);
            InitAudio();
            AVERR(avformat_write_header(Context, nullptr));
        }

        AVRational tb = { .num = (int)Para.RateDen, .den = (int)Para.RateNum };

        // set up packet
        Packet->stream_index = VideoStream->index;
        Packet->data = (uint8*)data;
        Packet->size = size;
        Packet->dts = Packet->pts = av_rescale_q(FrameNo, tb, VideoStream->time_base);
        Packet->duration = av_rescale_q(1, tb, VideoStream->time_base);

        // write packet
        AVERR(av_interleaved_write_frame(Context, Packet));
        av_packet_unref(Packet);

        FrameNo++;
    }

    void SubmitAudio(const uint8* data, uint size) override
    {
        if (!AudioContext) return;
     
        AVRational tb = { .num = 1, .den = (int)Para.Audio.SampleRate, };
        uint samples = size / Para.Audio.BytesPerSample;       
        int planar = av_sample_fmt_is_planar(AudioContext->sample_fmt);
        uint bytesPerChannel = ResampleBufferSize * ResampleBytesPerSample;
        uint rbsize = Para.Audio.Channels * bytesPerChannel;

        while (samples)
        {
            // fill up resample buffer
            uint avail =  ResampleBufferSize-ResampleFill;

            uint in = Min(samples, avail); // TODO: add sample rate conversion

            uint rbpos = ResampleFill * ResampleBytesPerSample;
            if (!planar)
                rbpos *= Para.Audio.Channels;

            uint8* buffers[8];
            for (int i = 0; i < 8; i++)
                buffers[i] = ResampleBuffer + rbpos + i * bytesPerChannel;

            int samplesOut = swr_convert(Resample, buffers, avail, &data, in);
            ResampleFill += in;
            samples -= in;
            data += in * Para.Audio.BytesPerSample;

            // cut buffer into frames and send
            uint frame = AudioContext->frame_size;
            if (!frame) frame = ResampleFill;
            uint written = 0;
            while ((ResampleFill-written) >= frame)
            {
                // make frame
                Frame->pts = av_rescale_q(AudioWritten + written, tb, AudioContext->time_base);
                Frame->format = AudioContext->sample_fmt;
                Frame->nb_samples = frame;
                Frame->channels = AudioContext->channels;
                Frame->channel_layout = AudioContext->channel_layout;
                AVERR(av_frame_get_buffer(Frame, 0));

                // copy audio data
                rbpos = written * ResampleBytesPerSample;
                if (!planar)
                    rbpos *= Para.Audio.Channels;
                for (int i = 0; i < 8; i++) 
                    if (Frame->data[i])
                        memcpy(Frame->data[i], ResampleBuffer + rbpos + i * bytesPerChannel, Frame->linesize[0]);

                // encode and send
                AVERR(avcodec_send_frame(AudioContext, Frame));
                WriteAudio();
                av_frame_unref(Frame);

                written += frame;
            }
            AudioWritten += written;

            // move remainder of resample buffer back to start
            if (written)
            {
                if (written < ResampleFill)
                {
                    if (planar)
                    {
                        for (uint i = 0; i < Para.Audio.Channels; i++)
                        {
                            uint8* buf = ResampleBuffer + i * bytesPerChannel;
                            memcpy(buf, buf + written * ResampleBytesPerSample, (ResampleFill - written) * ResampleBytesPerSample);
                        }
                    }
                    else
                    {
                        uint bps = ResampleBytesPerSample * Para.Audio.Channels;
                        memcpy(ResampleBuffer, ResampleBuffer + written * bps, (ResampleFill - written) * bps);
                    }
                }
                ResampleFill -= written;
            }
        }
    }

};

IOutput* CreateOutputLibAV(const OutputPara& para) { return new Output_LibAV(para); }
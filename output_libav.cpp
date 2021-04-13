#include "system.h"
#include "output.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libswresample\swresample.h>
}

#if _DEBUG
static char averrbuf[1024];
#define AVERR(x) { auto _ret=(x); if(_ret<0) { Fatal("%s(%d): libav call failed: %s\nCall: %s\n",__FILE__,__LINE__,av_make_error_string(averrbuf, 1024, _ret),#x); } }
#else
#define AVERR(x) { auto _ret=(x); if(_ret<0) { Fatal("%s(%d): libav call failed: %08x\n",__FILE__,__LINE__,_ret); } }
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
    AVFrame* AudioFrame = nullptr;

    SwrContext* Resample = nullptr;
    uint ResampleBufferSize = 0;
    uint8* ResampleBuffer = nullptr;

    int FrameNo = 0;
    int AudioWritten = 0;

public:

    Output_LibAV(const OutputPara& para) : Para(para)
    {
        AVERR(avformat_alloc_output_context2(&Context, nullptr, "mov", Para.filename));

        AVERR(avio_open(&Context->pb, Para.filename, AVIO_FLAG_WRITE));

        VideoStream = avformat_new_stream(Context, 0);
        VideoStream->id = 0;
        VideoStream->time_base.den = VideoStream->avg_frame_rate.num = Para.RateNum; 
        VideoStream->time_base.num = VideoStream->avg_frame_rate.den = Para.RateDen;

        auto codecpar = VideoStream->codecpar;
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->codec_id = AV_CODEC_ID_H264;
        codecpar->bit_rate = 0;
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

        if (Para.Audio.Format != AudioFormat::None)
        {
            static const AVSampleFormat sampleFmts[] = { AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };

            AudioCodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);

            // find suitable sample format
            const AVSampleFormat* sampleFmt;
            for (sampleFmt = sampleFmts; *sampleFmt != AV_SAMPLE_FMT_NONE; sampleFmt++)
            {
                bool found = false;
                for (const AVSampleFormat* cFmt = AudioCodec->sample_fmts; *cFmt != AV_SAMPLE_FMT_NONE; cFmt++)
                {
                    if (*cFmt == *sampleFmt)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
            if (*sampleFmt == AV_SAMPLE_FMT_NONE) sampleFmt = AudioCodec->sample_fmts;

            // init audio stream and codec
            if (*sampleFmt != AV_SAMPLE_FMT_NONE)
            {
                AudioContext = avcodec_alloc_context3(AudioCodec);
                AudioContext->sample_fmt = *sampleFmt;
                //AudioContext->bit_rate = Para.AudioBitrate;
                AudioContext->sample_rate = Para.Audio.SampleRate;
                AudioContext->channels = Para.Audio.Channels;
                AudioContext->channel_layout = av_get_default_channel_layout(Para.Audio.Channels);

                AudioStream = avformat_new_stream(Context, AudioCodec);
                AudioStream->id = 1;
                AVERR(avcodec_parameters_from_context(AudioStream->codecpar, AudioContext));

                AudioFrame = av_frame_alloc();

                AVSampleFormat sourceFmt = AV_SAMPLE_FMT_NONE;
                switch (Para.Audio.Format)
                {
                case AudioFormat::I16: sourceFmt = AV_SAMPLE_FMT_S16; break;
                case AudioFormat::F32: sourceFmt = AV_SAMPLE_FMT_FLT; break;
                }

                if (sourceFmt != *sampleFmt)
                {
                    Resample = swr_alloc_set_opts(nullptr, AudioContext->channel_layout, *sampleFmt, Para.Audio.SampleRate, AudioContext->channel_layout, sourceFmt, Para.Audio.SampleRate, 0, nullptr);
                    ResampleBufferSize = Para.Audio.Channels * Para.Audio.SampleRate * 4;
                    ResampleBuffer = new uint8[ResampleBufferSize];

                    AVERR(swr_init(Resample));
                }

                AVERR(avcodec_open2(AudioContext, AudioCodec, 0));
            }

        }

        AVERR(avformat_write_header(Context, nullptr));
    }

    ~Output_LibAV()
    {
        delete[] ResampleBuffer;
        swr_free(&Resample);

        //AVERR(avformat_flush(Context));
        AVERR(av_write_trailer(Context));

        avformat_free_context(Context);

        avcodec_free_context(&AudioContext);

    }

    void SubmitVideoPacket(const uint8* data, uint size) override
    {
        AVRational tb = { .num = (int)Para.RateDen, .den = (int)Para.RateNum };

        // set up packet
        AVPacket packet = {};
        av_init_packet(&packet);
        packet.stream_index = VideoStream->index;
        packet.data = (uint8*)data;
        packet.size = size;
        packet.dts = packet.pts = av_rescale_q(FrameNo, tb, VideoStream->time_base);
        packet.duration = av_rescale_q(1, tb, VideoStream->time_base);

        // write packet
        AVERR(av_interleaved_write_frame(Context, &packet));

        FrameNo++;
    }

    void SubmitAudio(const uint8* data, uint size) override
    {
        if (!AudioContext) return;

        AVRational tb = { .num = 1, .den = (int)Para.Audio.SampleRate, };

        int bps = 0;        
        switch (Para.Audio.Format)
        {
        case AudioFormat::I16: bps = 2; break;
        case AudioFormat::F32: bps = 4; break;
        }
        bps *= Para.Audio.Channels;

        int samples = size / bps;
        AudioFrame->nb_samples = samples;
        AudioFrame->pts = av_rescale_q(AudioWritten, tb, AudioContext->time_base);

        if (Resample)
        {            
            AudioFrame->nb_samples = swr_convert(Resample, &ResampleBuffer, ResampleBufferSize, &data, samples);
            AVERR(avcodec_fill_audio_frame(AudioFrame, Para.Audio.Channels, AudioContext->sample_fmt, ResampleBuffer, ResampleBufferSize, 0));
        }
        else
        {
            AudioFrame->nb_samples = samples;
            AVERR(avcodec_fill_audio_frame(AudioFrame, Para.Audio.Channels, AudioContext->sample_fmt, data, size, 0));
        }

        AVERR(avcodec_send_frame(AudioContext, AudioFrame));

        AVPacket packet = { };
        av_init_packet(&packet);
        while (!avcodec_receive_packet(AudioContext, &packet))
        {
            packet.pts = av_rescale_q(packet.pts, AudioContext->time_base, AudioStream->time_base);
            packet.dts = av_rescale_q(packet.dts, AudioContext->time_base, AudioStream->time_base);
            packet.duration = (int)av_rescale_q(packet.duration, AudioContext->time_base, AudioStream->time_base);
            packet.stream_index = AudioStream->index;

            // Write the compressed frame to the media file.
            AVERR(av_interleaved_write_frame(Context, &packet));

            av_packet_unref(&packet);
        }
    }
};

IOutput* CreateOutputLibAV(const OutputPara& para) { return new Output_LibAV(para); }
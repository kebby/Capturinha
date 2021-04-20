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

    SwrContext* Resample = nullptr;
    uint ResampleBufferSize = 0;
    uint8* ResampleBuffer = nullptr;
    uint ResampleBytesPerSample = 0;
    uint ResampleFill = 0;
    uint SkipAudio = 0; // bytes

    int FrameNo = 0;
    int AudioWritten = 0;

    void InitVideo()
    {
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
    }

    void InitAudio()
    {
        if (Para.Audio.Format == AudioFormat::None)
            return;

        // find the audio codec
        static const AVCodecID codecs[] = { AV_CODEC_ID_PCM_S16LE, AV_CODEC_ID_PCM_F32LE, AV_CODEC_ID_MP3, AV_CODEC_ID_AAC };
        AudioCodec = avcodec_find_encoder(codecs[(int)Para.CConfig->UseAudioCodec]);
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
            AudioContext->bit_rate = Para.CConfig->AudioBitrate;
            AudioContext->sample_rate = Para.Audio.SampleRate;
            AudioContext->channels = Para.Audio.Channels;
            AudioContext->channel_layout = av_get_default_channel_layout(Para.Audio.Channels);
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
        }
    }

public:

    Output_LibAV(const OutputPara& para) : Para(para)
    {          
        printf("Starting file %s\n", (const char*)para.filename);

        static const char* const formats[] = { "avi", "mp4", "mov", "matroska" };

        AVERR(avformat_alloc_output_context2(&Context, nullptr, formats[(int)para.CConfig->UseContainer] , para.filename));

        AVERR(avio_open(&Context->pb, para.filename, AVIO_FLAG_WRITE));

        InitVideo();
        InitAudio();

        AVERR(avformat_write_header(Context, nullptr));
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

    void SetAudioDelay(double delaySec)
    {
        // turn the delay into samples and then either push some silence
        // or tell SubmitAudio to skip some input
        uint samples = (uint)fabs(delaySec * Para.Audio.SampleRate);
        uint size = samples * Para.Audio.BytesPerSample;
        if (delaySec > 0)
        {
            uint8* zeroes = new uint8[size];
            memset(zeroes, 0, size);
            SubmitAudio(zeroes, size);
            delete[] zeroes;
        }
        else
        {
            SkipAudio = size;
        }
    }

    void SubmitAudio(const uint8* data, uint size) override
    {
        if (!AudioContext) return;

        uint skip = Min(SkipAudio, size);
        data += skip;
        size -= skip;
        SkipAudio -= skip;
        if (!size) return;

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
                AVFrame* audioFrame = av_frame_alloc();
                audioFrame->pts = av_rescale_q(AudioWritten + written, tb, AudioContext->time_base);
                audioFrame->nb_samples = frame;
                AVERR(avcodec_fill_audio_frame(audioFrame, Para.Audio.Channels, AudioContext->sample_fmt, ResampleBuffer, rbsize, 0));

                // avcodec_fill_audio_frame doesn't do inter-channel stride correctly, so fix up the pointers
                rbpos = written * ResampleBytesPerSample;
                if (!planar)
                    rbpos *= Para.Audio.Channels;
                for (int i = 0; i < 8; i++)
                {
                    audioFrame->data[i] = ResampleBuffer + rbpos + i * bytesPerChannel;
                    audioFrame->linesize[i] = audioFrame->linesize[0];
                }

                // encode and send
                AVERR(avcodec_send_frame(AudioContext, audioFrame));
                WriteAudio();

                written += frame;

                av_frame_free(&audioFrame);
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
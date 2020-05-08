#include "cfile.h" // this needs to be first to include the winsock2 before
#include "decoder_base.h"
#include "logger.h"
#include <vector>
#include <memory>
#include <SDL.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/base64.h>
#include <libswscale/swscale.h>
}

// sample conversion functions
template <typename T>
void planar_samples(const AVFrame *f, int channels, VideoDecoder *p)
{
#ifndef _MSC_VER
    T buffer[f->nb_samples * channels];
#else
    T *buffer = (T*)_alloca(sizeof(T) * f->nb_samples * channels);
#endif
    T *ptr = buffer;
    size_t delta = 0;
    for(int s = 0; s < f->nb_samples; ++s) {
        for(int c = 0; c < channels; ++c) {
            *ptr = *((T*)(((uint8_t*)f->extended_data[c]) + delta));
            ptr++;
        }
        delta += sizeof(T);
    }
    p->queue_audio(buffer, sizeof(buffer), (double)f->pts * p->audio_time_base());
}

template <typename T>
void copy_samples(const AVFrame *f, int channels, VideoDecoder *p)
{
    int data_size = av_samples_get_buffer_size(nullptr, channels, f->nb_samples, AV_SAMPLE_FMT_U8, 1);

    if (data_size > 0)
        p->queue_audio(f->data[0], data_size * sizeof(T), (double)f->pts * p->audio_time_base());
}

void VideoDecoder::
queue_audio(const void *p, size_t s, double pts)
{
    if (audio_pts_.load() == AV_NOPTS_VALUE)
        audio_pts_.store((int64_t)(pts * 1000000.0));
    SDL_QueueAudio(audio_, p, s);
    queued_audio_ += s;
    // ILOG << "Queued " << s << " bytes on audio device " << audio_ << " (pts " << pts <<"), base:" << audio_pts_.load();
}

int64_t VideoDecoder::
elapsed_samples_time() const
{
    int64_t queued = SDL_GetQueuedAudioSize(audio_);
    int64_t queued_audio = queued_audio_.load();
    // ILOG<< "EST:" << queued << " queued_audio:" << queued_audio;
    int64_t val = ((queued_audio - queued) * 1000) / samples_per_msec_;
    if (val < 0)
        return audio_pts_;
    return audio_pts_ + val;
}
double VideoDecoder::
queued_audio_seconds() const
{
    uint64_t queued = SDL_GetQueuedAudioSize(audio_);

    double q = (double)queued / (double)samples_per_msec_;

    // ILOG << "Querying audio queue: " << q << " msecs";
    return q / 1000.0;
}

double VideoDecoder::
next_pts() const {
    auto frame = frames_.next();
    if (frame)
        return ((double)frame->pts - start_time_) * time_base_;
    else
        return -1;
}

double VideoDecoder::
total_length() const {
    return ((double)ic_->duration / AV_TIME_BASE);
}
void VideoDecoder::
start_audio(bool flag) {
    if (audio_)
        SDL_PauseAudioDevice(audio_, flag ? 0 : 1);
}

VideoDecoder::
VideoDecoder(const std::string &filename, bool use_audio, double sec, int len) :
    Thread("VDEC"), length_(len), start_(sec), audio_pts_(AV_NOPTS_VALUE), queued_audio_(0) {

    if (sec < 0.0) {
        start_ = 0.0;
        do_not_seek_ = true;
    }

#ifndef __EMSCRIPTEN__
    static std::once_flag ffmpeg_registration;
    std::call_once(ffmpeg_registration, []()
    {
#endif
        avformat_network_init();
#ifndef __EMSCRIPTEN__
        ILOG << "Registered AV libraries";
    });
#endif
    DLOG << "Before context creation...";
    ic_ = avformat_alloc_context();

    if (!ic_)
        throw std::string("Unable to create AVFormat context");

    AVInputFormat *fmt = nullptr;
    AVDictionary *options = nullptr;

    // limit probesize to avoid being stuck when no audio is available
    ic_->probesize = 512 * 1024;
    ic_->max_analyze_duration = AV_TIME_BASE; // 1 sec max

    DLOG << "Opening " << filename;
    if (avformat_open_input(&ic_, filename.c_str(), fmt, &options) < 0) {
        avformat_free_context(ic_);
        throw std::string("Unable to open file: ") + filename;
    }
    ILOG << "Before find stream info...";

    //ic_->max_analyze_duration = 0;
    if (avformat_find_stream_info(ic_, NULL) < 0) {
        avformat_close_input(&ic_);
        throw std::string("Unable to read codec parameters for: ") + filename;
    }

    ILOG << "Default stream is " << av_find_default_stream_index(ic_);

    for (int i = 0; i < (int)ic_->nb_streams; i++) {
        AVStream *st = ic_->streams[i];
        AVCodecParameters *enc = st->codecpar;

        switch(enc->codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                {
                    if (!use_audio)
                        break;

                    if (audio_track_ == -1) {
                        audio_channels_ = enc->channels;
                        audio_sample_rate_ = enc->sample_rate;
                        audio_track_ = i;
                    }


                    ILOG << "audio(" << i << "): " << enc->sample_rate << '/' << enc->channels << ' ' << (audio_track_ == i ? "SELECTED" : "")
                         << " time base " << st->time_base.num << '/' << st->time_base.den << " start:" << st->start_time;

                    if (audio_channels_ == 0) {
                        WLOG << "Skipping broken audio track";
                        break;
                    }

                    if (audio_track_ != i)
                        break;

                    AVCodec *codec = avcodec_find_decoder(enc->codec_id);
                    if (!codec) {
                        ELOG << "Unsupported audio codec (id=" << enc->codec_id
                            << ",tag=" << std::string((char*)&enc->codec_tag, 4)
                            << ") for input stream " << i;

                        throw std::string("Unsupported audio codec");
                    }
                    actx_ = avcodec_alloc_context3(codec);
                    avcodec_parameters_to_context(actx_, enc);

                    if (avcodec_open2(actx_, codec, NULL) < 0) {
                        ELOG << "Error while opening audio codec for input stream " << i;
                        throw std::string("Unable to audio open codec");
                    }

                    std::string fmt = av_get_sample_fmt_name(actx_->sample_fmt);
                    ILOG << "Audio decoder fmt:" << fmt
                         << " rate:" << actx_->sample_rate
                         << " size:" << av_get_bytes_per_sample(actx_->sample_fmt)
                         << " channels:" << actx_->channels
                         << " planar:" << av_sample_fmt_is_planar(actx_->sample_fmt);

                    want_ = new SDL_AudioSpec();
                    SDL_zero(*want_);
                    want_->freq = audio_sample_rate_;
                    want_->channels = audio_channels_;
                    audio_time_base_ = (double)st->time_base.num / (double)st->time_base.den;
                    audio_start_time_ = next_audio_pts_ = st->start_time;
                    switch (av_get_bytes_per_sample(actx_->sample_fmt)) {
                        case 4:
                            want_->format = AUDIO_F32;
                            if (!av_sample_fmt_is_planar(actx_->sample_fmt))
                                queue_samples_ = copy_samples<float>;
                            else
                                queue_samples_ = planar_samples<float>;

                            samples_per_msec_ = (audio_sample_rate_ * audio_channels_ * 4) / 1000;
                            break;
                        case 2:
                            if (!av_sample_fmt_is_planar(actx_->sample_fmt))
                                queue_samples_ = copy_samples<int16_t>;
                            else
                                queue_samples_ = planar_samples<int16_t>;

                            want_->format = AUDIO_S16;
                            samples_per_msec_ = (audio_sample_rate_ * audio_channels_ * 2) / 1000;
                            break;
                        default:
                            want_->format = AUDIO_U8;
                            if (!av_sample_fmt_is_planar(actx_->sample_fmt))
                                queue_samples_ = copy_samples<int8_t>;
                            else
                                queue_samples_ = planar_samples<int8_t>;
                            samples_per_msec_ = (audio_sample_rate_ * audio_channels_ ) / 1000;

                            break;
                    }
                }
                break;
            case AVMEDIA_TYPE_VIDEO:
                if (vctx_) {
                    ILOG << "video(" << i << "): " << enc->width << 'x' << enc->height << ", time base " << st->time_base.num << '/' << st->time_base.den << " IGNORED";
                } else {
                    if ( st->avg_frame_rate.den && st->avg_frame_rate.num)
                        frame_rate_ =  (double)st->avg_frame_rate.num / (double)st->avg_frame_rate.den;
                    else if (st->r_frame_rate.den)
                        frame_rate_ = (double)st->r_frame_rate.num / (double)st->r_frame_rate.den;
                    else {
                        WLOG << "Unable to find a valid framerate, forcing 25 fps!";
                        frame_rate_ = 25.0;
                    }
                    ILOG << "STD guessed frame rate:" << frame_rate_;

                    AVRational fr = av_guess_frame_rate(ic_, st, nullptr);
                    if (fr.num != 0) {
                        frame_rate_ = (double)fr.num/(double)fr.den;
                        ILOG << "FFMPEG guessed frame rate:" << frame_rate_;
                    }
                    ILOG << "video(" << i << "): " << enc->width << 'x' << enc->height << ' ' << frame_rate_ << "fps, time base " << st->time_base.num << '/' << st->time_base.den
                         << " start:" << st->start_time;

                    height_ = enc->height;
                    width_ = enc->width;

                    AVCodec *codec = avcodec_find_decoder(enc->codec_id);

                    if (!codec) {
                        if (enc->codec_tag == 0x4d475844) { // DXGM, supporto divx blizz
                            codec = avcodec_find_decoder_by_name("mpeg4");
                        }
                        if (!codec) {
                            ELOG << "Unsupported video codec (id=" << enc->codec_id
                                << ",tag=" << std::string((char*)&enc->codec_tag, 4)
                                << ") for input stream " << i;

                            throw std::string("Unsupported video codec");
                        }
                    }
                    vctx_ = avcodec_alloc_context3(codec);
                    avcodec_parameters_to_context(vctx_, enc);
                    vctx_->thread_count = 0;
                    vctx_->thread_type = FF_THREAD_FRAME;
                    if (avcodec_open2(vctx_, codec, NULL) < 0) {
                        ELOG << "Error while opening video codec for input stream " << i;
                        throw std::string("Unable to video open codec");
                    }
                    video_track_ = i;
                    time_base_ = (double)st->time_base.num / (double)st->time_base.den;
                    start_time_ = st->start_time;
                    ILOG << "Video decoder for " << std::hex << enc->codec_tag << " (stream " << i << ") ready.";
                }
                break;
           default:
                WLOG << "Ignored stream with codec type: " << enc->codec_type;
        }

    }
    picture_ = av_frame_alloc();
    samples_ = av_frame_alloc();

    pkt_ = new AVPacket;
    av_init_packet(pkt_);
}

bool VideoDecoder::
audio_started() const {
    return audio_start_time_ != AV_NOPTS_VALUE && audio_; 
}

bool VideoDecoder::
open_audio()
{
    if (!want_ || !(audio_ = SDL_OpenAudioDevice(NULL, 0, want_, NULL, 0))) {
        ELOG << "Unable to open audio device: " << SDL_GetError();
        audio_track_ = -1;
    }
    delete want_;
    return audio_ != 0;
}

VideoDecoder::
~VideoDecoder()
{
    terminate();

    av_frame_free(&picture_);
    av_frame_free(&samples_);
    delete pkt_;

    avcodec_close(vctx_);
    avformat_close_input(&ic_);
}

void VideoDecoder::
flush() {
    frames_.clear();
    queued_audio_.store(0);
    audio_pts_.store(AV_NOPTS_VALUE);
    if (audio_)
        SDL_ClearQueuedAudio(audio_);
}

void VideoDecoder::
core_seek(double sec, bool to_iframe)
{
    frames_.clear();
    queued_audio_.store(0);
    audio_pts_.store(AV_NOPTS_VALUE);

    if (audio_)
        SDL_ClearQueuedAudio(audio_);

    DLOG << "Seeking to " << sec;
    seek_to_iframe_ = to_iframe;
    start_ = sec;
    eof_ = false;
}

void VideoDecoder::
nothread_seek(double sec, bool to_iframe) {
    core_seek(sec, to_iframe);

    avcodec_flush_buffers(vctx_);
    if (actx_) avcodec_flush_buffers(actx_);
    ts_ = int64_t(start_ / time_base_);
    frame_number_ = 0;
    if (av_seek_frame(ic_, video_track_, ts_, seek_to_iframe_ ? 0 : AVSEEK_FLAG_BACKWARD) < 0)
        WLOG << "Error seeking!";
}

void VideoDecoder::
seek(double sec, bool to_iframe) {
    if (running()) {
        terminate();
    }
    core_seek(sec, to_iframe);
    
    start();
}

std::string VideoDecoder::
png(AVFrame *pFrame) const {
    std::string result;
    if (AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_PNG)) {
        if (AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec)) {

            jpegContext->pix_fmt = AV_PIX_FMT_RGB24;
            jpegContext->height = pFrame->height;
            jpegContext->width = pFrame->width;
            jpegContext->time_base = vctx_->time_base;

            if (avcodec_open2(jpegContext, jpegCodec, NULL) >= 0) {
                AVPacket packet;
                av_init_packet(&packet);
                if (AVFrame *dest = av_frame_alloc()) {
                    dest->format = jpegContext->pix_fmt;
                    dest->width = pFrame->width;
                    dest->height = pFrame->height;
                    if (av_frame_get_buffer(dest, 0) == 0) {
                        // Convert the colour format and write directly to the opencv matrix
                        SwsContext* conversion = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat) pFrame->format,
                                                                dest->width, dest->height, (AVPixelFormat)dest->format, SWS_FAST_BILINEAR, NULL, NULL, NULL);
                        sws_scale(conversion, pFrame->data, pFrame->linesize, 0, pFrame->height, dest->data, dest->linesize);
                        sws_freeContext(conversion);
                        if (avcodec_send_frame(jpegContext, dest) == 0) {
                            if (avcodec_receive_packet(jpegContext, &packet) == 0) {
                                result.assign((const char*)packet.data, packet.size);
                                av_packet_unref(&packet);
                            }
                        }
                    }
                    av_frame_free(&dest);
                }
            }
            avcodec_close(jpegContext);
        }
    }
    return result;
}

std::string VideoDecoder::
jpeg(AVFrame *pFrame) const {
    std::string result;
    if (AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG)) {
        if (AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec)) {
            jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
            jpegContext->height = pFrame->height;
            jpegContext->width = pFrame->width;
            jpegContext->time_base = vctx_->time_base;

            if (avcodec_open2(jpegContext, jpegCodec, NULL) >= 0) {
                AVPacket packet;
                av_init_packet(&packet);
                if (avcodec_send_frame(jpegContext, pFrame) == 0) {
                    if (avcodec_receive_packet(jpegContext, &packet) == 0) {
                        result.assign((const char*)packet.data, packet.size);
                        av_packet_unref(&packet);
                    }
                }
            }
            avcodec_close(jpegContext);
        }
    }
    return result;
}

bool VideoDecoder::
jpeg(const std::string &name, AVFrame *pFrame) const {
    std::string jpg = jpeg(pFrame);
    if (!jpg.empty())
        return CFile(name, CFile::Write).write(jpg);
    return false;
}

bool VideoDecoder::
png(const std::string &name, AVFrame *pFrame) const {
    std::string jpg = png(pFrame);
    if (!jpg.empty())
        return CFile(name, CFile::Write).write(jpg);
    return false;
}

std::string VideoDecoder::base64(const std::string &src) {
    std::string result;
    result.resize(AV_BASE64_SIZE(src.length()));
    av_base64_encode(&result[0], static_cast<int>(result.length()), (const uint8_t *)&src[0], static_cast<int>(src.length()));
    result.pop_back(); // remove null termination
    return result;
}

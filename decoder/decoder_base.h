#pragma once

#include <string>
#include "thread.h"
#include "safequeue.h"

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SDL_AudioSpec;

class VideoDecoder : public Thread
{
    void core_seek(double, bool);
protected:
    AVFormatContext *ic_ = nullptr;
    AVCodecContext *vctx_ = nullptr, *actx_ = nullptr;
    AVFrame *picture_ = nullptr, *samples_ = nullptr;
    AVPacket *pkt_ = nullptr;
    long audio_ = 0;
    int width_, height_, length_, audio_track_ = -1;
    bool do_not_seek_ = false;
    int audio_channels_, audio_sample_rate_;
    double frame_rate_, time_base_, start_, audio_time_base_, start_time_, next_audio_pts_;
    int video_track_;
    int64_t samples_per_msec_, audio_start_time_ = 0;
    bool seek_to_iframe_ = false;
    SafeQueue<std::shared_ptr<AVFrame> > frames_;
    bool eof_ = false;
    void (*queue_samples_)(const AVFrame *, int channels, VideoDecoder *) = nullptr;
    std::atomic<int64_t> audio_pts_, queued_audio_;
    SDL_AudioSpec *want_ = nullptr;
    int64_t ts_ = 0;
    int64_t frame_number_ = 0;

public:
    VideoDecoder(const std::string &file, bool use_audio, double sec = 10.0, int length = 5);
    ~VideoDecoder();
    SafeQueue<std::shared_ptr<AVFrame> > &frames() { return frames_; }
    bool has_frames() const { return !frames_.empty(); }
    double frame_time(double pts) const { return (pts - start_time_) * time_base_; }
    double frame_rate() const { return frame_rate_; }
    bool jpeg(const std::string &name, AVFrame *) const;
    bool png(const std::string &name, AVFrame *) const;
    static std::string base64(const std::string &src);
    std::string jpeg(AVFrame *) const;
    std::string png(AVFrame *) const;
    void seek(double sec, bool to_iframe = false);
    bool eof() const { return eof_ && frames_.empty(); }
    double queued_audio_seconds() const;
    bool has_audio() const { return audio_ != 0; }
    void start_audio(bool flag);
    int64_t elapsed_samples_time() const;
    void queue_audio(const void *, size_t, double);
    double total_length() const;
    void flush();
    double audio_time_base() const { return audio_time_base_; }
    bool audio_started() const;
    bool open_audio();
    double next_pts() const;
    virtual bool decode() { return false; };
    virtual void nothread_seek(double sec, bool to_iframe = false);
};

#include "simple_decoder.h"
#include "logger.h"
#include <vector>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

const double BUFFER_SIZE = 1.0;

void SimpleDecoder::
worker_thread()
{
    avcodec_flush_buffers(vctx_);
    if (actx_) avcodec_flush_buffers(actx_);
    ts_ = int64_t(start_ / time_base_);

    if (!do_not_seek_) {
        DLOG << "Starting playback from " << ts_;

        int rc = av_seek_frame(ic_, video_track_, ts_, seek_to_iframe_ ? 0 : AVSEEK_FLAG_BACKWARD);

        if (rc < 0) {
            WLOG << "Seek to " << start_ << " (" << ts_ << ") failed!";
        }
    }
    frame_number_ = 0;

    while (running()) {
        if (!decode())
            break;
        
        while (frames_.size() > frame_rate_ * BUFFER_SIZE && running())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    DLOG << "Player loop terminated";
}

bool SimpleDecoder::
decode() {
    if (av_read_frame(ic_, pkt_) < 0) {
        WLOG << "EOF reached!";
        eof_ = true;
        return false;
    }
    eof_ = false;

    if (!pkt_->size) {
        av_packet_unref(pkt_);
        return false;
    }

    int index = pkt_->stream_index;

// ILOG << "stream #" << pkt_->stream_index << "(" << (ic_->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? "A" : "V")
//      << "), size=" << pkt_->size << ", pts=" << pkt_->pts << ", dts=" << pkt_->dts << ", key=" << pkt_->flags << ", dur=" << pkt_->duration;

    if (ic_->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

        avcodec_send_packet(vctx_, pkt_);

        //ILOG << "pkt_ pts:" << pkt_->pts << " dts:" << pkt_->dts;
        if (avcodec_receive_frame(vctx_, picture_) == 0) {
            //ILOG << "GOT FRAME pkt pts:" << pkt_->pts << " dts:" << pkt_->dts;
            ++frame_number_;
            // gli avi non hanno pts, ma hanno DTS e solo nel pacchetto, in caso di mancanza di pts, usiamo il dts
            int64_t pts =  picture_->best_effort_timestamp;
            if (pts == AV_NOPTS_VALUE) {
                if (pkt_->dts == AV_NOPTS_VALUE)
                    pts = int64_t(static_cast<double>(frame_number_) / frame_rate_ * time_base_);
                else
                    pts = pkt_->dts;
            }

            if (frame_number_ == 1) {
                DLOG << "ls0:" << picture_->linesize[0]
                    << " ls1:" << picture_->linesize[1]
                    << " ls2:" << picture_->linesize[2]
                    << " bpw:" << picture_->width
                    << " bph:" << picture_->height
                    << " ctxw:" << vctx_->width
                    << " ctxh:" << vctx_->height
                    << " pts:" << pts;
            }

            if (pts < ts_ && !seek_to_iframe_ && !do_not_seek_) {
                av_packet_unref(pkt_);
                return true;
            }

            auto frame = std::shared_ptr<AVFrame>(av_frame_clone(picture_), [](AVFrame* ptr) { av_frame_free(&ptr); });
            frames_.enqueue(frame);
            seek_to_iframe_ = false;
        }
    } else if (audio_ && ic_->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (audio_track_ == index) {
            avcodec_send_packet(actx_, pkt_);

            if (avcodec_receive_frame(actx_, samples_) == 0 && audio_ && queue_samples_ ) {
                //ILOG << "Received audio frame with " << samples_->pts << "pts, best effort:" << samples_->best_effort_timestamp;

                if (samples_->pts == AV_NOPTS_VALUE && next_audio_pts_ != AV_NOPTS_VALUE)
                    samples_->pts = next_audio_pts_;

                if (audio_start_time_ == 0 || audio_start_time_ == AV_NOPTS_VALUE)
                    audio_start_time_ = samples_->pts;

                if (samples_->pts != AV_NOPTS_VALUE)
                    next_audio_pts_ = samples_->pts + samples_->nb_samples;
                samples_->pts -= audio_start_time_;
                //ILOG << "final:" << samples_->pts;

                queue_samples_(samples_, actx_->channels, this);
            }
        }
    }
    av_packet_unref(pkt_);

    return true;
}

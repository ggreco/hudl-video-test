#include "reverse_decoder.h"
#include "logger.h"
#include <vector>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( disable : 4996 )
#endif

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

const double BUFFER_SIZE = 3.0;

void ReverseDecoder::
worker_thread()
{
    ILOG << "Entering reverse decoder worker thread with start: " << start_ << " and length:" << length_;
    double done = 0;
    int64_t next_final = int64_t(start_ / time_base_) + 1LL;
    for (int loops = 0; (length_ < 0 || done < length_) && running() && start_ > 0; ++loops) {
        const double buffer_size = std::min<double>(start_, BUFFER_SIZE);
        int64_t final_pts = next_final;

        avcodec_flush_buffers(vctx_);
        start_ -= buffer_size;
        done += buffer_size;
        int64_t ts = int64_t(start_ / time_base_);
        int rc = av_seek_frame(ic_, video_track_, ts, seek_to_iframe_ ? AVSEEK_FLAG_BACKWARD : 0);

        if (rc < 0) {
            WLOG << "Seek to " << start_ << " (" << ts << ") failed!";
        }
        long long frames = 0;

        std::vector< std::shared_ptr<AVFrame> > framevec;
        while (running()) {
            if (av_read_frame(ic_, pkt_) < 0) {
                WLOG << "EOF reached!";
                break; // fine file
            }
            eof_ = false;
            if (!pkt_->size) {
                av_packet_unref(pkt_);
                break;
            }

            int index = pkt_->stream_index;

        // ILOG << "stream #" << pkt_->stream_index << "(" << (ic_->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? "A" : "V")
        //      << "), size=" << pkt_->size << ", pts=" << pkt_->pts << ", dts=" << pkt_->dts << ", key=" << pkt_->flags << ", dur=" << pkt_->duration;

            if (ic_->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

                avcodec_send_packet(vctx_, pkt_);

                //ILOG << "pkt_ pts:" << pkt_->pts << " dts:" << pkt_->dts;
                if (avcodec_receive_frame(vctx_, picture_) == 0) {
                    //ILOG << "GOT FRAME pkt pts:" << pkt_->pts << " dts:" << pkt_->dts;
                    ++frames;
                    // gli avi non hanno pts, ma hanno DTS e solo nel pacchetto, in caso di mancanza di pts, usiamo il dts
                    int64_t pts =  picture_->best_effort_timestamp;
                    if (pts == AV_NOPTS_VALUE) {
                        if (pkt_->dts == AV_NOPTS_VALUE)
                            pts = int64_t(static_cast<double>(frames) / frame_rate_ * time_base_);
                        else
                            pts = pkt_->dts;
                    }

                    if (frames == 1) {
                        ILOG << "ls0:" << picture_->linesize[0]
                            << " ls1:" << picture_->linesize[1]
                            << " ls2:" << picture_->linesize[2]
                            << " bpw:" << picture_->width
                            << " bph:" << picture_->height
                            << " ctxw:" << vctx_->width
                            << " ctxh:" << vctx_->height
                            << " pts:" << pts;
                        next_final = pts;
                    }

                    if (pts >= final_pts && !seek_to_iframe_) {
                        av_packet_unref(pkt_);
                        break;
                    }
                    auto frame = std::shared_ptr<AVFrame>(av_frame_clone(picture_), [](AVFrame* ptr) { av_frame_free(&ptr); });
                    framevec.push_back(frame);
                    seek_to_iframe_ = false;
                }
            }
            av_packet_unref(pkt_);
        }

        for (auto it = framevec.rbegin(); it != framevec.rend(); ++it)
            frames_.enqueue(*it);

        while (frames_.size() > frame_rate_ * BUFFER_SIZE && running())
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (length_ <= 0.0 && running()) {
        ILOG << "EOF reached";
        eof_ = true;
    }
}

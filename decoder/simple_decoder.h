#pragma once

#include "decoder_base.h"

class SimpleDecoder : public VideoDecoder
{
    void worker_thread() override;
public:
    SimpleDecoder(const std::string &file, bool with_audio = false, double sec = 0.0) :
        VideoDecoder(file, with_audio, sec) {}
    bool decode() override;
};


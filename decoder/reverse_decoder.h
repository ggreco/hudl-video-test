#pragma once

#include "decoder_base.h"

class ReverseDecoder : public VideoDecoder
{
    int length_;
    void worker_thread() override;
public:
    ReverseDecoder(const std::string &file, double sec = 0.0, int length = -1) :
        VideoDecoder(file, false, sec), length_(length) {}
};

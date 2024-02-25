// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

#pragma once

//stdlib
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

//opus
#include <opus/opus.h>

//mumlib
#include "mumlib2/logger.h"
#include "mumlib2_private/audio_packet.h"

namespace mumlib2 {
    class AudioEncoder {
    public:
        //mark as non-copyable
        AudioEncoder(const AudioEncoder&) = delete;
        AudioEncoder& operator=(const AudioEncoder&) = delete;
        
        //ctor/dtor
        explicit AudioEncoder(uint32_t output_bitrate);
        ~AudioEncoder();

        std::vector<uint8_t> Encode(const int16_t* pcmData, size_t pcmLength, uint32_t target);

        void SetBitrate(uint32_t bitrate);

    private:
        void reset();

        void createOpus();
        void destroyOpus();


    private:
        Logger logger = Logger("mumlib/AudioEncoder");

        OpusEncoder* _encoder = nullptr;
        std::vector<uint8_t> _encoder_buf;
        
        uint32_t _channels = 0;

        std::chrono::high_resolution_clock::time_point _sequence_timestemp;
        uint32_t _sequence_number = 0;

    private:
        static constexpr std::chrono::seconds _sequence_reset_interval = std::chrono::seconds(5);
    };
}

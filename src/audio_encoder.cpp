//stdlib
#include <array>
#include <chrono>

//mumlib
#include "mumlib2/constants.h"
#include "mumlib2/exceptions.h"
#include "mumlib2_private/audio_encoder.h"

namespace mumlib2 {

    //
    // Ctor/Dtor
    //

    AudioEncoder::AudioEncoder(uint32_t output_bitrate) {
        _channels = MUMBLE_AUDIO_CHANNELS;

        createOpus();

        SetBitrate(output_bitrate);

        reset();
    }

    AudioEncoder::~AudioEncoder() {
        destroyOpus();
    }

    void AudioEncoder::createOpus()
    {
        destroyOpus();

        int status = 0;
        _encoder = opus_encoder_create(MUMBLE_AUDIO_SAMPLERATE, _channels, OPUS_APPLICATION_VOIP, &status);
        if (status != OPUS_OK) {
            throw AudioEncoderException(std::string("failed to initialize OPUS encoder: ") + opus_strerror(status));
        }
    }

    void AudioEncoder::destroyOpus()
    {
        if (_encoder) {
            opus_encoder_destroy(_encoder);
            _encoder = nullptr;
        }
    }

    void AudioEncoder::reset() {
        if (!_encoder) {
            throw AudioEncoderException("failed to reset encoder");
        }

        int status = opus_encoder_ctl(_encoder, OPUS_RESET_STATE, nullptr);
        if (status != OPUS_OK) {
            throw AudioEncoderException(std::string("failed to reset OPUS encoder: ") + opus_strerror(status));
        }

        _sequence_number = 0;
    }

    void AudioEncoder::SetBitrate(uint32_t bitrate)
    {
        _encoder_buf.resize(bitrate * MUMBLE_OPUS_MAXLENGTH / 1000);

        if (!_encoder) {
            throw AudioEncoderException("failed to reset encoder");
        }

        int error = opus_encoder_ctl(_encoder, OPUS_SET_VBR(0));
        if (error != OPUS_OK) {
            throw AudioEncoderException(std::string("failed to initialize variable bitrate:") + opus_strerror(error));
        }

        error = opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(bitrate));
        if (error != OPUS_OK) {
            throw AudioEncoderException(std::string("failed to initialize transmission bitrate:") + opus_strerror(error));
        }
    }

    std::vector<uint8_t> AudioEncoder::Encode(const int16_t* pcmData, size_t pcmLength, uint32_t target) {
        const int16_t* in_data = pcmData;
        int in_len = pcmLength;

        int out_len = 0;

        //check interval and reset encoder
        auto interval = std::chrono::high_resolution_clock::now() - _sequence_timestemp;
        if (interval > _sequence_reset_interval) {
            reset();
        }
        
        //resample and encode
        if (pcmData && pcmLength) {
            out_len = opus_encode(
                _encoder,
                in_data,
                in_len,
                _encoder_buf.data(),
                _encoder_buf.size()
            );

            if (out_len <= 0) {
                throw AudioEncoderException(std::string("failed to encode PCM data: %s") + opus_strerror(out_len));
            }
        }

        //create audiopacket
        auto encoded = AudioPacket::CreateAudioOpusPacket(
            target,
            _sequence_number,
            _encoder_buf.data(),
            out_len,
            out_len == 0).Encode();

        //update timestamp and sequence
        if (out_len > 0) {
            //1 per 10ms
            _sequence_number += 100 * in_len / MUMBLE_AUDIO_SAMPLERATE;
        }
        else {
            reset();
        }

        _sequence_timestemp = std::chrono::high_resolution_clock::now();

        return encoded;
    }
}

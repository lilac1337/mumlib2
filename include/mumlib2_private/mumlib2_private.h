// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

#pragma once

//stdlib
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

//mumlib
#include "mumlib2/callback.h"
#include "mumlib2/constants.h"
#include "mumlib2/structs.h"
#include "mumlib2_private/audio_decoder.h"
#include "mumlib2_private/audio_encoder.h"
#include "mumlib2_private/transport.h"
#include "mumble.pb.h"

namespace mumlib2 {
    class Mumlib2Private {
    public:
        //mark as non-copyable
        Mumlib2Private(const Mumlib2Private&) = delete;
        Mumlib2Private& operator=(const Mumlib2Private&) = delete;

        Mumlib2Private(Callback& callback);

        //Audio
        void AudioSend(const int16_t* pcmData, int pcmLength);
        void AudioSendTarget(const int16_t* pcmData, int pcmLength, uint32_t target);

        // ACL
        bool AclSetTokens(const std::vector<std::string>& tokens);

        // Channel
        [[nodiscard]] uint32_t ChannelGetCurrent() const;
        [[nodiscard]] std::vector<MumbleChannel> ChannelGetList() const;
        [[nodiscard]] bool ChannelExists(uint32_t channel_id) const;
        [[nodiscard]] int32_t ChannelFind(const std::string& channel_name) const;
        bool ChannelJoin(uint32_t channel_id);

        //Text
        bool TextSend(const std::string& message);

        // Transport
        bool TransportConnect(const std::string& host, uint16_t port, const std::string& user, const std::string& password);
        void TransportDisconnect();
        [[nodiscard]] ConnectionState TransportGetState() const;
        void TransportRun();
        void TransportSetCert(const std::string& cert);
        void TransportSetKey(const std::string& key);

        //User
        [[nodiscard]] std::optional<MumbleUser> UserGet(int32_t session_id);
        [[nodiscard]] std::vector<MumbleUser> UserGetList() const;
        [[nodiscard]] std::vector<MumbleUser> UserGetInChannel(int32_t channel_id) const;
        [[nodiscard]] bool UserExists(uint32_t user_id) const;
        [[nodiscard]] bool UserMuted(int32_t user_id);
        [[nodiscard]] int32_t UserFind(const std::string& user_name) const;
        bool UserMute(int32_t user_id, bool mute_state);
        bool UserSendState(UserState field, const std::string& val);
        bool UserSendState(UserState field, bool val);
		bool RequestUserStats(uint32_t user_id, bool stats_only = false);

        //Voicetarget
        bool VoicetargetSet(int targetId, VoiceTargetType type, int id);
        bool VoicetargetSet(int targetId, VoiceTargetType type, const std::string& name);

    private:
        // General
        void generalClear();

        // Audio
        void audioDecoderCreate(uint32_t output_samplerate);
        void audioEncoderCreate(uint32_t input_samplerate, uint32_t output_bitrate);

        // Channel
        void channelEmplace(MumbleChannel& channel);
        void channelErase(uint32_t channel_id);
        void channelSet(uint32_t channel_id);

        // Processing
        bool processControlPacket(MessageType messageType, const uint8_t* buffer, int length);
        bool processControlBanlistPacket(const uint8_t* buffer, int length);
        bool processControlChannelremovePacket(const uint8_t* buffer, int length);
        bool processControlChannelstatePacket(const uint8_t* buffer, int length);
        bool processControlCodecVersionPacket(const uint8_t* buffer, int length);
		bool processControlUserStats(const uint8_t* buffer, int length);
        bool processControlPermissionQueryPacket(const uint8_t* buffer, int length);
        bool processControlTextMessagePacket(const uint8_t* buffer, int length);
        bool processControlVersionPacket(const uint8_t* buffer, int length);
        bool processControlUserRemovePacket(const uint8_t* buffer, int length);
        bool processControlUserStatePacket(const uint8_t* buffer, int length);
        bool processControlServerconfigPacket(const uint8_t* buffer, int length);
        bool processControlServersyncPacket(const uint8_t* buffer, int length);
        bool processAudioPacket(AudioPacket& packet);

        // User
        void userClear();
        void userErase(uint32_t user_id);
        void userUpdate(MumbleUser& user);   

        //Session
        [[nodiscard]] uint32_t sessionGet() const;

        //Transport
        void transportCreate();
        bool transportSendAuthentication(const std::vector<std::string>& tokens);
        bool transportSendControl(MessageType type, google::protobuf::Message& message);
        bool transportSendAudio(const uint8_t* data, size_t len);

    private:
        //Audio
        std::unique_ptr<AudioDecoder> _audio_decoder;
        std::unique_ptr<AudioEncoder> _audio_encoder;
        uint32_t _audio_bitrate = MUMBLE_OPUS_BITRATE;

        //Callback
        Callback& _callback;

        //Channel
        std::vector<MumbleChannel> _channel_list;
        uint32_t _channel_current = 0;

        //Logger
        Logger _logger = Logger("");

        //Transport
        std::unique_ptr<Transport> _transport;
        std::string _transport_cert;
        std::string _transport_key;

        //User
        std::map<int32_t, MumbleUser> _user_map; //{session_id, MumbleUser}

        //Session
        uint32_t _session_id = 0;

        //Server
        uint32_t _server_maxbandwidth = 0;
        uint32_t _server_allowhtml = 0;
        uint32_t _server_imagemessagelength = 0;
        uint32_t _server_messagelength = 0;
        std::string _server_welcometext;

        //Voicetarget
        MumbleProto::VoiceTarget _voiceTarget;

    private:
        //audio
        static constexpr uint32_t _audio_rx_buffer_length = 60;
        static constexpr uint32_t _audio_tx_buffer_size = 8192;
    };
}

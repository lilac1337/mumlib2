// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

#pragma once

//stdlib
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

//mumlib
#include "mumlib2/callback.h"
#include "mumlib2/constants.h"
#include "mumlib2/enums.h"
#include "mumlib2/export.h"
#include "mumlib2/exceptions.h"
#include "mumlib2/logger.h"
#include "mumlib2/structs.h"

namespace mumlib2 {

    class Mumlib2Private;

    class MUMLIB2_EXPORT Mumlib2 {
    public:
        //mark as non-copyable
        Mumlib2(const Mumlib2&) = delete;
        Mumlib2& operator=(const Mumlib2&) = delete;

        explicit Mumlib2(Callback &callback);

        virtual ~Mumlib2();

        //acl
        bool AclSetTokens(const std::vector<std::string>& tokens);

        //channel
        std::string ChannelCurrentGetName();
        int32_t ChannelCurrentGetId();
        bool ChannelJoin(const std::string& channel_name);
        bool ChannelJoin(int channel_id);

        //user
        std::optional<MumbleUser> UserGet(int32_t session_id);
        std::vector<MumbleUser> UserGetInChannel(int32_t channel_id);
        std::vector<MumbleUser> UserGetInChannel(const std::string& channel_name);
        [[nodiscard]] bool UserMuted(const std::string& user_name);
        [[nodiscard]] bool UserMuted(int32_t user_id);
        bool UserMute(const std::string& user_name, bool mute_state);
        bool UserMute(int32_t user_id, bool mute_state);

        //
        bool connect(string host, int port, string user, string password);

        void disconnect();

        void run();

        ConnectionState getConnectionState();

        vector<MumbleUser> getListAllUser();

        vector<MumbleChannel> getListAllChannel();

        void sendAudioData(const int16_t *pcmData, int pcmLength);

        void sendAudioDataTarget(int targetId, const int16_t *pcmData, int pcmLength);

        void sendTextMessage(std::string message);

        void sendVoiceTarget(int targetId, VoiceTargetType type, int sessionId);

        void sendVoiceTarget(int targetId, VoiceTargetType type, std::string name, int &error);

        void sendUserState(UserState state, bool val);

        void sendUserState(UserState state, std::string value);

        bool isSessionIdValid(int sessionId);
    private:
        std::unique_ptr<Mumlib2Private> impl;

        int getChannelIdBy(std::string channelName);

        int getUserIdBy(std::string userName);

        bool isChannelIdValid(int channelId);
    };
}

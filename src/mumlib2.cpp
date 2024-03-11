// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

//mumlib
#include "mumlib2.h"
#include "mumlib2_private/mumlib2_private.h"

namespace mumlib2 {

    Mumlib2::Mumlib2(Callback& callback) {
        impl = std::make_unique<Mumlib2Private>(callback);
    }

    Mumlib2::~Mumlib2() {
        disconnect();
    }

    //
    // ACL
    //
    bool Mumlib2::AclSetTokens(const std::vector<std::string>& tokens)
    {
        return impl->AclSetTokens(tokens);
    }

    //
    // Audio
    //
    
    //
    // Channel
    //
    bool Mumlib2::ChannelJoin(int channelId) {
        return impl->ChannelJoin(channelId);
    }

    std::string Mumlib2::ChannelCurrentGetName()
    {
        auto current_id = ChannelCurrentGetId();
        for (const auto& chan : impl->ChannelGetList()) {
            if (chan.channelId == current_id) {
                return chan.name;
            }
        }

        return "";
    }

    int32_t Mumlib2::ChannelCurrentGetId()
    {
        return impl->ChannelGetCurrent();
    }

    bool Mumlib2::ChannelJoin(const std::string& channel_name) {
        auto id = getChannelIdBy(channel_name);
        if (id < 0) {
            return false;
        }

        return ChannelJoin(id);
    }

    //
    // User
    //
    std::optional<MumbleUser> Mumlib2::UserGet(int32_t session_id)
    {
        return impl->UserGet(session_id);
    }

    std::vector<MumbleUser> Mumlib2::UserGetInChannel(int32_t channel_id)
    {
        return impl->UserGetInChannel(channel_id);
    }

    std::vector<MumbleUser> Mumlib2::UserGetInChannel(const std::string& channel_name)
    {
        int32_t channel_id = impl->ChannelFind(channel_name);
        return impl->UserGetInChannel(channel_id);
    }

    bool Mumlib2::UserMuted(const std::string& user_name)
    {
        int32_t user_id = impl->UserFind(user_name);
        return impl->UserMuted(user_id);
    }

    bool Mumlib2::UserMuted(int32_t user_id)
    {
        return impl->UserMuted(user_id);
    }

    bool Mumlib2::UserMute(const std::string& user_name, bool mute_state)
    {
        int32_t user_id = impl->UserFind(user_name);
        return impl->UserMute(user_id, mute_state);
    }

    bool Mumlib2::UserMute(int32_t user_id, bool mute_state)
    {
        return impl->UserMute(user_id, mute_state);
    }

    ConnectionState Mumlib2::getConnectionState() {
        return impl->TransportGetState();
    }

    vector<MumbleUser> Mumlib2::getListAllUser() {
        return impl->UserGetList();
    }

    vector<MumbleChannel> Mumlib2::getListAllChannel() {
        return impl->ChannelGetList();
    }

    bool Mumlib2::connect(string host, int port, string user, string password) {
        return impl->TransportConnect(host, port, user, password);
    }

    void Mumlib2::disconnect() {
        impl->TransportDisconnect();
    }

    void Mumlib2::run() {
        impl->TransportRun();
    }

    void Mumlib2::sendAudioData(const int16_t *pcmData, int pcmLength) {
        impl->AudioSend(pcmData, pcmLength);
    }

    void Mumlib2::sendAudioDataTarget(int targetId, const int16_t *pcmData, int pcmLength) {
        impl->AudioSendTarget(pcmData, pcmLength, targetId);
    }

    void Mumlib2::sendTextMessage(string message) {
        impl->TextSend(message);
    }


    void Mumlib2::sendVoiceTarget(int targetId, VoiceTargetType type, int id) {
        impl->VoicetargetSet(targetId, type, id);
    }

    void Mumlib2::sendVoiceTarget(int targetId, VoiceTargetType type, string name, int &error) {
        error = impl->VoicetargetSet(targetId, type, name);
    }

    void Mumlib2::sendUserState(UserState field, bool val) {
        impl->UserSendState(field,val);
    }

    void Mumlib2::sendUserState(UserState field, std::string val) {
        impl->UserSendState(field, val);
    }

	void Mumlib2::requestUserStats(uint32_t user_id, bool stats_only) {
		impl->RequestUserStats(user_id, stats_only);
	}
	
    int Mumlib2::getChannelIdBy(string name) {
        return impl->ChannelFind(name);
    }

    int Mumlib2::getUserIdBy(string name) {
        return impl->UserFind(name);
    }

    bool Mumlib2::isSessionIdValid(int sessionId) {
        return impl->UserExists(sessionId);
    }

    bool Mumlib2::isChannelIdValid(int channelId) {
        return impl->ChannelExists(channelId);
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2015-2022 mumlib2 contributors

#pragma once

//stdlib
#include <cstdint>
#include <string>
#include <vector>

//mumlib2
#include "mumlib2/export.h"

namespace mumlib2 {

    using namespace std;

    class MUMLIB2_EXPORT Callback {
    public:
        virtual void version(
                uint16_t major,
                uint8_t minor,
                uint8_t patch,
                string release,
                string os,
                string os_version) { };

        virtual void audio(
                int target,
                int sessionId,
                int sequenceNumber,
                bool is_last,
                const int16_t* audio_buf,
                size_t samples_count) { };

        virtual void unsupportedAudio(
                int target,
                int sessionId,
                int sequenceNumber,
                const uint8_t *encoded_audio_data,
                uint32_t encoded_audio_data_size) { };

        virtual void serverSync(
                string welcome_text,
                int32_t session,
                int32_t max_bandwidth,
                int64_t permissions) { };

        virtual void channelRemove(uint32_t channel_id) { };

        virtual void channelState(
                string name,
                int32_t channel_id,
                int32_t parent,
                string description,
                vector<uint32_t> links,
                vector<uint32_t> inks_add,
                vector<uint32_t> links_remove,
                bool temporary,
                int32_t position) { };

        virtual void userRemove(
                uint32_t session,
                int32_t actor,
                string reason,
                bool ban) { };

        virtual void userState(
                int32_t session,
                int32_t actor,
                string name,
                int32_t user_id,
                int32_t channel_id,
                int32_t mute,
                int32_t deaf,
                int32_t suppress,
                int32_t self_mute,
                int32_t self_deaf,
                string comment,
                int32_t priority_speaker,
                int32_t recording) { };

        virtual void banList(
                const uint8_t *ip_data,
                uint32_t ip_data_size,
                uint32_t mask,
                string name,
                string hash,
                string reason,
                string start,
                int32_t duration) { };

        virtual void textMessage(
                uint32_t actor,
                std::vector<uint32_t> session,
                std::vector<uint32_t> channel_id,
                std::vector<uint32_t> tree_id,
                string message) { };

        virtual void permissionDenied(
                int32_t permission,
                int32_t channel_id,
                int32_t session,
                string reason,
                int32_t deny_type,
                string name) { };

        virtual void queryUsers(
                uint32_t n_ids,
                uint32_t *ids,
                uint32_t n_names,
                string *names) { };

        virtual void contextActionModify(
                string action,
                string text,
                uint32_t m_context,
                uint32_t operation) { };

        virtual void contextAction(
                int32_t session,
                int32_t channel_id,
                string action) { };

        virtual void userList(
                uint32_t user_id,
                string name,
                string last_seen,
                int32_t last_channel) { };

        virtual void permissionQuery(
                int32_t channel_id,
                uint32_t permissions,
                int32_t flush) { };

        virtual void codecVersion(
                int32_t alpha,
                int32_t beta,
                uint32_t prefer_alpha,
                int32_t opus) { };

        virtual void serverConfig(
                uint32_t max_bandwidth,
                string welcome_text,
                uint32_t allow_html,
                uint32_t message_length,
                uint32_t image_message_length) { };

        virtual void suggestConfig(
                uint32_t version,
                uint32_t positional,
                uint32_t push_to_talk) { };

		virtual void userStats(
            uint32_t sessionId,
			uint32_t onlinesecs,
            uint32_t idlesecs
			) { };

    };
}

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "server.h"

#include "constants.h"
#include "construct/construct_geometry.h"
#include "construct/construct_interaction_packets.h"
#include "construct/construct_packets.h"
#include "construct/construct_runtime.h"
#include "construct/construct_section.h"
#include "network/construct_replication_queue.h"
#include "log.h"
#include "network/networkpacket.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "serverenvironment.h"

#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::mutex g_interaction_mutex;
std::unordered_map<session_t, std::uint64_t> g_last_interaction_sequence;

navycraft::Vec3d toNodes(const v3f &value)
{
    return {value.X / BS, value.Y / BS, value.Z / BS};
}

void broadcastSection(Server *server, const navycraft::DynamicConstruct &construct,
    const navycraft::LocalNodePos &node_position)
{
    navycraft::ConstructSectionIndex index;
    index.rebuild(construct);
    const auto section_position = navycraft::ConstructSectionIndex::sectionFor(node_position);
    navycraft::ConstructSection section;
    if (const auto *existing = index.find(section_position))
        section = *existing;
    else
        section.position = section_position;
    section.revision = navycraft::nextRuntimeSectionSequence(construct.id());
    server->SendNavyCraftConstructMessage(PEER_ID_INEXISTENT, {
        navycraft::ConstructWireKind::Section,
        construct.id(),
        navycraft::ConstructPacketCodec::encodeSection(construct.id(), section),
        true,
    });
}

bool closeEnough(const navycraft::Vec3d &left, const navycraft::Vec3d &right,
    double maximum_distance) noexcept
{
    const auto delta = left - right;
    return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <=
        maximum_distance * maximum_distance;
}
}

void Server::handleCommand_NavyCraftInteraction(NetworkPacket *packet)
{
    const session_t peer_id = packet->getPeerId();
    if (!IsNavyCraftPeerReady(peer_id))
        return;
    RemotePlayer *player = m_env ? m_env->getPlayer(peer_id) : nullptr;
    PlayerSAO *player_sao = player ? player->getPlayerSAO() : nullptr;
    if (!player || !player_sao || player_sao->isDead())
        return;

    try {
        std::string payload;
        *packet >> payload;
        const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
        auto request = navycraft::ConstructInteractionPacketCodec::decode(bytes);
        {
            std::lock_guard<std::mutex> lock(g_interaction_mutex);
            const auto iterator = g_last_interaction_sequence.find(peer_id);
            if (iterator != g_last_interaction_sequence.end() &&
                    request.sequence <= iterator->second)
                return;
        }
        request.actor = player->getName();
        const auto construct = navycraft::runtimeConstructRegistry().find(
            request.construct_id);
        if (!construct)
            return;

        const navycraft::Vec3d eye = toNodes(player_sao->getEyePosition());
        if (!closeEnough(eye, request.world_point, 10.0)) {
            warningstream << "Rejected distant NavyCraft interaction from "
                << player->getName() << std::endl;
            return;
        }
        const navycraft::Vec3d eye_to_hit = request.world_point - eye;
        const double ray_length = std::sqrt(eye_to_hit.x * eye_to_hit.x +
            eye_to_hit.y * eye_to_hit.y + eye_to_hit.z * eye_to_hit.z);
        if (!(ray_length > 1e-6) || !std::isfinite(ray_length))
            return;
        const navycraft::Vec3d ray_end = request.world_point +
            eye_to_hit * (0.1 / ray_length);
        const auto verified = navycraft::ConstructGeometry::raycast(*construct, eye, ray_end);
        if (!verified || !(verified->node_position == request.node_position) ||
                !closeEnough(verified->world_point, request.world_point, 0.35)) {
            warningstream << "Rejected unverified NavyCraft interaction from "
                << player->getName() << std::endl;
            return;
        }

        request.local_point = verified->local_point;
        request.world_point = verified->world_point;
        request.world_normal = verified->world_normal;
        const auto &transform = construct->transform();
        const navycraft::Vec3d local_normal =
            transform.worldToLocal(verified->world_point + verified->world_normal) -
            transform.worldToLocal(verified->world_point);
        request.adjacent_position = {
            request.node_position.x + static_cast<std::int32_t>(std::llround(local_normal.x)),
            request.node_position.y + static_cast<std::int32_t>(std::llround(local_normal.y)),
            request.node_position.z + static_cast<std::int32_t>(std::llround(local_normal.z)),
        };

        const auto result = navycraft::runtimeConstructInteractionEngine().apply(
            *construct, request);
        if (!result.accepted)
            return;
        {
            std::lock_guard<std::mutex> lock(g_interaction_mutex);
            g_last_interaction_sequence[peer_id] = request.sequence;
        }
        if (result.node_changed)
            broadcastSection(this, *construct, result.changed_position);
    } catch (const std::exception &error) {
        warningstream << "Invalid NavyCraft interaction packet from peer "
            << peer_id << ": " << error.what() << std::endl;
    }
}

void Server::StepNavyCraftConstructTimers(float delta_seconds)
{
    if (!(delta_seconds > 0.0f) || !std::isfinite(delta_seconds))
        return;
    for (const auto &construct : navycraft::runtimeConstructRegistry().snapshot()) {
        if (construct)
            (void)navycraft::runtimeConstructInteractionEngine().stepTimers(
                *construct, delta_seconds);
    }
}

void Server::ClearNavyCraftInteractionState(session_t peer_id)
{
    std::lock_guard<std::mutex> lock(g_interaction_mutex);
    g_last_interaction_sequence.erase(peer_id);
}

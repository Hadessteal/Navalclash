// SPDX-License-Identifier: LGPL-2.1-or-later
#include "server.h"

#include "constants.h"
#include "construct/carried_body.h"
#include "construct/construct_geometry.h"
#include "construct/construct_packets.h"
#include "construct/construct_articulation_packets.h"
#include "construct/construct_articulated_motion.h"
#include "construct/construct_runtime.h"
#include "construct/construct_section.h"
#include "construct/rider_motion.h"
#include "construct/rider_packets.h"
#include "network/networkpacket.h"
#include "log.h"
#include "network/construct_replication_queue.h"
#include "remoteplayer.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "server/serveractiveobject.h"
#include "serverenvironment.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
struct ServerRiderRecord {
    navycraft::RiderStatePacket packet{};
    double received_at = 0.0;
    navycraft::Vec3d last_platform_anchor{};
    bool have_platform_anchor = false;
};

std::mutex g_rider_mutex;
std::unordered_map<session_t, ServerRiderRecord> g_riders;
constexpr double MAX_PACKET_ANCHOR_ERROR = 2.5;
constexpr double MAX_LOCAL_RIDER_SPEED = 16.0;
constexpr double MAX_RIDER_PREDICTION_SECONDS = 0.25;
constexpr double RIDER_TIMEOUT_SECONDS = 1.0;
navycraft::CarriedBodyManager g_carried_bodies;

navycraft::Vec3d toNodes(const v3f &value)
{
    return {value.X / BS, value.Y / BS, value.Z / BS};
}

v3f toScene(const navycraft::Vec3d &value)
{
    return v3f(
        static_cast<f32>(value.x * BS),
        static_cast<f32>(value.y * BS),
        static_cast<f32>(value.z * BS));
}

navycraft::Aabb3d toNodes(const aabb3f &box)
{
    return {
        {box.MinEdge.X / BS, box.MinEdge.Y / BS, box.MinEdge.Z / BS},
        {box.MaxEdge.X / BS, box.MaxEdge.Y / BS, box.MaxEdge.Z / BS},
    };
}

aabb3f toScene(const navycraft::Aabb3d &box, float expansion_nodes = 0.0f)
{
    const float expansion = expansion_nodes * BS;
    return aabb3f(
        static_cast<f32>(box.min.x * BS - expansion),
        static_cast<f32>(box.min.y * BS - expansion),
        static_cast<f32>(box.min.z * BS - expansion),
        static_cast<f32>(box.max.x * BS + expansion),
        static_cast<f32>(box.max.y * BS + expansion),
        static_cast<f32>(box.max.z * BS + expansion));
}

double distance(const navycraft::Vec3d &left, const navycraft::Vec3d &right)
{
    const auto delta = left - right;
    return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

double vectorLength(const navycraft::Vec3d &value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

navycraft::Vec3d platformAnchorWorld(
    const navycraft::DynamicConstruct &construct,
    const navycraft::RiderStatePacket &packet)
{
    if (packet.articulation_id != 0) {
        return navycraft::runtimeConstructArticulationEngine().transformPointWorld(
            packet.articulation_id, packet.local_anchor);
    }
    return construct.transform().localToWorld(packet.local_anchor);
}

navycraft::Vec3d predictedRiderWorld(
    const navycraft::DynamicConstruct &construct,
    const ServerRiderRecord &record,
    double now)
{
    if (record.packet.articulation_id != 0)
        return platformAnchorWorld(construct, record.packet);
    const double elapsed = std::clamp(now - record.received_at, 0.0,
        MAX_RIDER_PREDICTION_SECONDS);
    const navycraft::Vec3d predicted_local = record.packet.local_anchor +
        record.packet.local_velocity * elapsed;
    return construct.transform().localToWorld(predicted_local);
}

bool validateGroundedPacket(
    const navycraft::DynamicConstruct &construct,
    const navycraft::RiderStatePacket &packet)
{
    if (vectorLength(packet.local_velocity) > MAX_LOCAL_RIDER_SPEED)
        return false;
    navycraft::Vec3d expected{};
    if (packet.articulation_id != 0) {
        const auto joint = navycraft::runtimeConstructArticulationEngine().joint(
            packet.articulation_id);
        if (!joint || joint->definition.construct_id != construct.id())
            return false;
        try {
            expected = navycraft::runtimeConstructArticulationEngine().transformPointWorld(
                packet.articulation_id, packet.local_anchor);
        } catch (...) {
            return false;
        }
    } else {
        expected = construct.transform().localToWorld(packet.local_anchor);
    }
    if (distance(expected, packet.world_position) > MAX_PACKET_ANCHOR_ERROR)
        return false;
    const navycraft::Aabb3d player_box{
        {packet.world_position.x - 0.30, packet.world_position.y,
            packet.world_position.z - 0.30},
        {packet.world_position.x + 0.30, packet.world_position.y + 1.75,
            packet.world_position.z + 0.30},
    };
    if (packet.articulation_id != 0) {
        const auto support = navycraft::ConstructArticulatedMotion::findSupport(
            navycraft::runtimeConstructArticulationEngine(), player_box, 0.45, 0.20);
        return support && support->articulation_id == packet.articulation_id;
    }
    const auto support = navycraft::ConstructGeometry::findSupport(
        construct, player_box, 0.45, 0.20);
    return support.has_value();
}
}

void Server::handleCommand_NavyCraftRiderState(NetworkPacket *packet)
{
    const session_t peer_id = packet->getPeerId();
    if (!IsNavyCraftPeerReady(peer_id))
        return;
    try {
        std::string payload;
        *packet >> payload;
        const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
        const auto rider = navycraft::RiderPacketCodec::decode(bytes);

        std::lock_guard<std::mutex> lock(g_rider_mutex);
        const auto existing = g_riders.find(peer_id);
        if (existing != g_riders.end() && rider.sequence <= existing->second.packet.sequence)
            return;
        if (rider.construct_id == 0 || rider.jumping || !rider.grounded) {
            g_riders.erase(peer_id);
            return;
        }

        const auto construct = navycraft::runtimeConstructRegistry().find(rider.construct_id);
        if (!construct || !validateGroundedPacket(*construct, rider)) {
            g_riders.erase(peer_id);
            if (RemotePlayer *player = m_env->getPlayer(peer_id)) {
                if (PlayerSAO *playersao = player->getPlayerSAO())
                    SendMovePlayer(playersao);
            }
            return;
        }
        ServerRiderRecord accepted;
        accepted.packet = rider;
        accepted.received_at = getUptime();
        try {
            accepted.last_platform_anchor = platformAnchorWorld(*construct, rider);
            accepted.have_platform_anchor = true;
        } catch (...) {
            g_riders.erase(peer_id);
            return;
        }
        g_riders[peer_id] = accepted;
    } catch (const std::exception &error) {
        warningstream << "Invalid NavyCraft rider packet from peer " << peer_id
            << ": " << error.what() << std::endl;
        std::lock_guard<std::mutex> lock(g_rider_mutex);
        g_riders.erase(peer_id);
    }
}

bool Server::ReconcileNavyCraftRider(
    session_t peer_id, v3f &position, v3f &speed)
{
    if (!IsNavyCraftPeerReady(peer_id))
        return false;
    ServerRiderRecord record;
    {
        std::lock_guard<std::mutex> lock(g_rider_mutex);
        const auto iterator = g_riders.find(peer_id);
        if (iterator == g_riders.end())
            return false;
        if (getUptime() - iterator->second.received_at > RIDER_TIMEOUT_SECONDS) {
            g_riders.erase(iterator);
            return false;
        }
        record = iterator->second;
    }

    const auto construct = navycraft::runtimeConstructRegistry().find(record.packet.construct_id);
    if (!construct) {
        ClearNavyCraftRiderState(peer_id);
        return false;
    }
    navycraft::Vec3d expected{};
    try {
        expected = predictedRiderWorld(*construct, record, getUptime());
    } catch (...) {
        ClearNavyCraftRiderState(peer_id);
        return false;
    }
    const auto correction = navycraft::RiderReconciler::reconcile(
        toNodes(position), expected, 0.30, 4.0, 0.18);
    position = toScene(correction.position);
    if (correction.hard_snap)
        speed = toScene(record.packet.velocity);
    return true;
}

void Server::ClearNavyCraftRiderState(session_t peer_id)
{
    std::lock_guard<std::mutex> lock(g_rider_mutex);
    g_riders.erase(peer_id);
}

void Server::SendNavyCraftFullState(session_t peer_id)
{
    if (!IsNavyCraftPeerReady(peer_id))
        return;
    for (const auto &construct : navycraft::runtimeConstructRegistry().snapshot()) {
        navycraft::ConstructTransformSnapshot snapshot;
        snapshot.id = construct->id();
        snapshot.sequence = navycraft::nextRuntimeTransformSequence(construct->id());
        snapshot.server_time = getUptime();
        snapshot.transform = construct->transform();
        snapshot.linear_velocity = construct->linearVelocity();
        snapshot.yaw_velocity = construct->yawVelocity();
        SendNavyCraftConstructMessage(peer_id, {
            navycraft::ConstructWireKind::Transform,
            construct->id(),
            navycraft::ConstructPacketCodec::encodeTransform(snapshot),
            true,
        });

        navycraft::ConstructSectionIndex sections;
        sections.rebuild(*construct);
        for (const auto &position : sections.positions()) {
            auto section = *sections.find(position);
            section.revision = navycraft::nextRuntimeSectionSequence(construct->id());
            SendNavyCraftConstructMessage(peer_id, {
                navycraft::ConstructWireKind::Section,
                construct->id(),
                navycraft::ConstructPacketCodec::encodeSection(construct->id(), section),
                true,
            });
        }

        for (const auto &joint : navycraft::runtimeConstructArticulationEngine().joints(
                construct->id())) {
            SendNavyCraftConstructMessage(peer_id, {
                navycraft::ConstructWireKind::Articulation,
                construct->id(),
                navycraft::ConstructArticulationPacketCodec::encodeDefinition(
                    joint.definition),
                true,
            });
            navycraft::ConstructArticulationSnapshot joint_snapshot;
            joint_snapshot.construct_id = construct->id();
            joint_snapshot.articulation_id = joint.definition.id;
            joint_snapshot.sequence = std::max<std::uint64_t>(1, joint.revision);
            joint_snapshot.server_time = getUptime();
            joint_snapshot.position = joint.position;
            joint_snapshot.target_position = joint.target_position;
            joint_snapshot.velocity = joint.velocity;
            joint_snapshot.enabled = joint.definition.enabled;
            SendNavyCraftConstructMessage(peer_id, {
                navycraft::ConstructWireKind::Articulation,
                construct->id(),
                navycraft::ConstructArticulationPacketCodec::encodeState(joint_snapshot),
                false,
            });
        }
    }
}

void Server::StepNavyCraftCarriedObjects(float dtime)
{
    if (!m_env || dtime <= 0.0f)
        return;

    const auto construct_snapshot = navycraft::runtimeConstructRegistry().snapshot();
    std::vector<const navycraft::DynamicConstruct *> constructs;
    constructs.reserve(construct_snapshot.size());
    for (const auto &construct : construct_snapshot) {
        if (construct && !construct->empty())
            constructs.push_back(construct.get());
    }
    if (constructs.empty()) {
        g_carried_bodies.clear();
        return;
    }

    // Keep server-side player SAOs on the same transformed deck anchor that the
    // client reports. This makes other clients see smooth remote riders and
    // prevents normal anti-cheat reconciliation from pulling them off the ship.
    std::vector<std::pair<session_t, ServerRiderRecord>> riders;
    {
        std::lock_guard<std::mutex> lock(g_rider_mutex);
        for (auto iterator = g_riders.begin(); iterator != g_riders.end();) {
            if (getUptime() - iterator->second.received_at > RIDER_TIMEOUT_SECONDS) {
                iterator = g_riders.erase(iterator);
                continue;
            }
            riders.push_back(*iterator);
            ++iterator;
        }
    }
    for (const auto &[peer_id, record] : riders) {
        if (!IsNavyCraftPeerReady(peer_id)) {
            ClearNavyCraftRiderState(peer_id);
            continue;
        }
        const auto construct = navycraft::runtimeConstructRegistry().find(
            record.packet.construct_id);
        RemotePlayer *player = m_env->getPlayer(peer_id);
        PlayerSAO *playersao = player ? player->getPlayerSAO() : nullptr;
        if (!construct || !player || !playersao)
            continue;
        navycraft::Vec3d platform_anchor{};
        navycraft::Vec3d target{};
        try {
            platform_anchor = platformAnchorWorld(*construct, record.packet);
            target = predictedRiderWorld(*construct, record, getUptime());
        } catch (...) {
            ClearNavyCraftRiderState(peer_id);
            continue;
        }

        navycraft::Vec3d carried = toNodes(playersao->getBasePosition());
        if (record.have_platform_anchor)
            carried += platform_anchor - record.last_platform_anchor;

        // Carry the server object by the platform's actual displacement first,
        // then apply only a bounded correction toward the predicted local rider
        // path. This avoids the old exact setBasePosition snap every server tick.
        const double correction_fraction = std::clamp(
            static_cast<double>(dtime) * 6.0, 0.08, 0.45);
        const auto correction = navycraft::RiderReconciler::reconcile(
            carried, target, 0.20, 4.0, correction_fraction);
        playersao->setBasePosition(toScene(correction.position));
        if (correction.hard_snap)
            player->setSpeed(toScene(record.packet.velocity));

        {
            std::lock_guard<std::mutex> lock(g_rider_mutex);
            const auto current = g_riders.find(peer_id);
            if (current != g_riders.end() &&
                    current->second.packet.sequence == record.packet.sequence) {
                current->second.last_platform_anchor = platform_anchor;
                current->second.have_platform_anchor = true;
            }
        }
    }

    // Query Lua entities near any vessel. Dropped items are the builtin item
    // LuaEntity; every other unattached LuaEntity is treated as a creature or
    // movable object. The manager preserves per-object deck contacts.
    std::unordered_map<u16, LuaEntitySAO *> nearby_entities;
    for (const navycraft::DynamicConstruct *construct : constructs) {
        const navycraft::Aabb3d bounds = navycraft::ConstructGeometry::worldBounds(*construct);
        std::vector<ServerActiveObject *> objects;
        m_env->getObjectsInArea(objects, toScene(bounds, 3.0f),
            [](ServerActiveObject *object) {
                return object && !object->isGone() &&
                    object->getType() == ACTIVEOBJECT_TYPE_LUAENTITY;
            });
        for (ServerActiveObject *object : objects) {
            auto *entity = static_cast<LuaEntitySAO *>(object);
            if (!entity->isAttached())
                nearby_entities[entity->getId()] = entity;
        }
    }

    for (const auto &[id, entity] : nearby_entities) {
        aabb3f collision_box;
        if (!entity || !entity->getCollisionBox(&collision_box)) {
            g_carried_bodies.remove(id);
            continue;
        }
        navycraft::CarriedBodyInput input;
        input.id = id;
        input.kind = entity->getName() == "__builtin:item" ?
            navycraft::CarriedBodyKind::DroppedItem :
            navycraft::CarriedBodyKind::Creature;
        input.position = toNodes(entity->getBasePosition());
        input.velocity = toNodes(entity->getVelocity());
        input.world_box = toNodes(collision_box);
        input.delta_seconds = dtime;
        input.current_time = getUptime();
        const navycraft::CarriedBodyOutput output = g_carried_bodies.step(
            constructs, &navycraft::runtimeConstructArticulationEngine(), input);
        entity->setPos(toScene(output.position));
        entity->setVelocity(toScene(output.velocity));
    }

    g_carried_bodies.prune(getUptime(), 3.0);
}

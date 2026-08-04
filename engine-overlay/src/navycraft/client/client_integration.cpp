// SPDX-License-Identifier: LGPL-2.1-or-later
#include "client/client.h"
#include "client/localplayer.h"
#include "client_construct_scene.h"
#include "client_construct_effects.h"

#include "log.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "construct/rider_packets.h"
#include "construct/construct_handshake.h"
#include "construct/construct_interaction_packets.h"
#include "camera.h"
#include "constants.h"
#include "inventory.h"
#include "util/pointedthing.h"

#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::uint64_t nextHandshakeNonce() noexcept
{
    static std::atomic<std::uint64_t> nonce{1};
    const auto value = nonce.fetch_add(1, std::memory_order_relaxed);
    return value == 0 ? nonce.fetch_add(1, std::memory_order_relaxed) : value;
}

std::vector<std::uint8_t> readPayload(NetworkPacket *packet)
{
    std::string payload;
    *packet >> payload;
    return std::vector<std::uint8_t>(payload.begin(), payload.end());
}

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

navycraft::LocalPlayerMotionInput makeMotionInput(
    LocalPlayer *player, const v3f &position, const v3f &speed,
    bool touching_ground, float dtime)
{
    navycraft::LocalPlayerMotionInput input;
    input.position = toNodes(position);
    input.velocity = toNodes(speed);
    input.jump_pressed = player && player->control.jump;
    input.touching_ground = touching_ground;
    input.delta_seconds = dtime;
    if (player) {
        const aabb3f &box = player->getCollisionbox();
        input.world_box = {
            {static_cast<double>((position.X + box.MinEdge.X) / BS),
                static_cast<double>((position.Y + box.MinEdge.Y) / BS),
                static_cast<double>((position.Z + box.MinEdge.Z) / BS)},
            {static_cast<double>((position.X + box.MaxEdge.X) / BS),
                static_cast<double>((position.Y + box.MaxEdge.Y) / BS),
                static_cast<double>((position.Z + box.MaxEdge.Z) / BS)},
        };
    }
    return input;
}

navycraft::ConstructInteractionAction toConstructAction(InteractAction action)
{
    using Result = navycraft::ConstructInteractionAction;
    switch (action) {
    case INTERACT_START_DIGGING: return Result::StartDig;
    case INTERACT_STOP_DIGGING: return Result::StopDig;
    case INTERACT_DIGGING_COMPLETED: return Result::DigComplete;
    case INTERACT_PLACE: return Result::Place;
    case INTERACT_USE: return Result::Use;
    case INTERACT_ACTIVATE: return Result::Activate;
    }
    return Result::Use;
}
}

bool Client::isNavyCraftProtocolReady() const noexcept
{
    return m_navycraft_handshake_accepted;
}

void Client::sendNavyCraftHandshake()
{
    if (m_state != LC_Ready || m_navycraft_handshake_sent)
        return;
    m_navycraft_handshake_nonce = nextHandshakeNonce();
    const auto hello = navycraft::makeClientConstructHello(
        m_navycraft_handshake_nonce);
    const auto bytes = navycraft::ConstructHandshakeCodec::encode(hello);
    const std::string payload(bytes.begin(), bytes.end());
    NetworkPacket packet(TOSERVER_NAVYCRAFT_HANDSHAKE, payload.size() + 4U);
    packet << payload;
    Send(&packet);
    m_navycraft_handshake_sent = true;
}

void Client::ensureNavyCraftConstructScene()
{
    if (m_navycraft_construct_scene)
        return;
    m_navycraft_construct_scene = std::make_unique<navycraft::ClientConstructScene>(this);
    m_navycraft_construct_effects = std::make_unique<navycraft::ClientConstructEffects>(
        this, m_sound, m_particle_manager.get(), &m_navycraft_construct_scene->manager());
}

void Client::stepNavyCraftConstructScene(float dtime)
{
    m_navycraft_client_time += dtime;
    if (m_state == LC_Ready && !m_navycraft_handshake_sent)
        sendNavyCraftHandshake();
    if (!m_navycraft_handshake_accepted)
        return;
    ensureNavyCraftConstructScene();
    try {
        m_navycraft_construct_scene->step(m_navycraft_client_time);
        if (m_navycraft_construct_effects)
            m_navycraft_construct_effects->step(m_navycraft_client_time, dtime);
    } catch (const std::exception &error) {
        errorstream << "NavyCraft client scene step failed: " << error.what() << std::endl;
    }
}


void Client::beginNavyCraftLocalPlayerMove(
    LocalPlayer *player, float dtime, v3f &position, v3f &speed)
{
    if (!player || !m_navycraft_handshake_accepted || !m_navycraft_construct_scene || m_state != LC_Ready)
        return;
    try {
        const auto input = makeMotionInput(
            player, position, speed, player->touching_ground, dtime);
        const auto output = m_navycraft_construct_scene->beginLocalPlayerMove(
            input, m_navycraft_client_time);
        position = toScene(output.position);
        speed = toScene(output.velocity);
        if (output.touching_ground)
            player->touching_ground = true;
    } catch (const std::exception &error) {
        errorstream << "NavyCraft begin-player-move failed: "
            << error.what() << std::endl;
    }
}

void Client::finishNavyCraftLocalPlayerMove(
    LocalPlayer *player, float dtime, v3f &position, v3f &speed,
    bool &touching_ground)
{
    if (!player || !m_navycraft_handshake_accepted || !m_navycraft_construct_scene || m_state != LC_Ready)
        return;
    try {
        const auto input = makeMotionInput(
            player, position, speed, touching_ground, dtime);
        const auto finished = m_navycraft_construct_scene->finishLocalPlayerMove(
            input, m_navycraft_client_time);
        position = toScene(finished.motion.position);
        speed = toScene(finished.motion.velocity);
        touching_ground = finished.motion.touching_ground;
        if (!finished.rider_packet)
            return;
        const auto bytes = navycraft::RiderPacketCodec::encode(
            *finished.rider_packet);
        const std::string payload(bytes.begin(), bytes.end());
        NetworkPacket packet(TOSERVER_NAVYCRAFT_RIDER_STATE,
            payload.size() + 4U);
        packet << payload;
        Send(&packet);
    } catch (const std::exception &error) {
        errorstream << "NavyCraft finish-player-move failed: "
            << error.what() << std::endl;
    }
}


bool Client::sendNavyCraftInteraction(
    InteractAction action, const PointedThing &pointed)
{
    if (!m_navycraft_handshake_accepted || !m_navycraft_construct_scene || !m_camera || m_state != LC_Ready)
        return false;
    const v3f scene_start = m_camera->getPosition();
    const v3f direction = m_camera->getDirection();
    double range_nodes = 6.0;
    if (pointed.type != POINTEDTHING_NOTHING)
        range_nodes = std::sqrt(std::max(0.0f, pointed.distanceSq)) / BS + 0.02;
    const navycraft::Vec3d start = toNodes(scene_start);
    const navycraft::Vec3d ray_direction{direction.X, direction.Y, direction.Z};
    const navycraft::Vec3d end = start + ray_direction * range_nodes;
    const auto hit = m_navycraft_construct_scene->manager().raycast(start, end);
    if (!hit)
        return false;
    if (pointed.type != POINTEDTHING_NOTHING) {
        const double stock_distance = std::sqrt(std::max(0.0f, pointed.distanceSq)) / BS;
        if (hit->distance > stock_distance + 1e-4)
            return false;
    }
    const auto state = m_navycraft_construct_scene->manager().find(hit->construct_id);
    if (!state)
        return false;
    const navycraft::Vec3d local_normal =
        state->construct().transform().worldToLocal(hit->world_point + hit->world_normal) -
        state->construct().transform().worldToLocal(hit->world_point);
    navycraft::ConstructInteractionRequest request;
    request.sequence = ++m_navycraft_interaction_sequence;
    request.construct_id = hit->construct_id;
    request.action = toConstructAction(action);
    request.node_position = hit->node_position;
    request.adjacent_position = {
        hit->node_position.x + static_cast<std::int32_t>(std::llround(local_normal.x)),
        hit->node_position.y + static_cast<std::int32_t>(std::llround(local_normal.y)),
        hit->node_position.z + static_cast<std::int32_t>(std::llround(local_normal.z)),
    };
    request.local_point = hit->local_point;
    request.world_point = hit->world_point;
    request.world_normal = hit->world_normal;
    request.client_time = m_navycraft_client_time;
    if (LocalPlayer *player = m_env.getLocalPlayer()) {
        request.actor = player->getName();
        ItemStack selected;
        ItemStack hand;
        request.wielded_item = player->getWieldedItem(&selected, &hand).getItemString();
    }
    const auto bytes = navycraft::ConstructInteractionPacketCodec::encode(request);
    const std::string payload(bytes.begin(), bytes.end());
    NetworkPacket packet(TOSERVER_NAVYCRAFT_INTERACTION, payload.size() + 4U);
    packet << payload;
    Send(&packet);
    return true;
}

void Client::resetNavyCraftConstructScene()
{
    if (m_navycraft_construct_effects)
        m_navycraft_construct_effects->reset();
    if (m_navycraft_construct_scene)
        m_navycraft_construct_scene->reset();
    m_navycraft_client_time = 0.0;
    m_navycraft_interaction_sequence = 0;
}

void Client::resetNavyCraftConnection()
{
    resetNavyCraftConstructScene();
    m_navycraft_handshake_sent = false;
    m_navycraft_handshake_accepted = false;
    m_navycraft_handshake_nonce = 0;
}

void Client::handleCommand_NavyCraftHandshake(NetworkPacket *packet)
{
    try {
        const auto response = navycraft::ConstructHandshakeCodec::decode(
            readPayload(packet));
        if (!m_navycraft_handshake_sent || response.nonce != m_navycraft_handshake_nonce)
            throw std::runtime_error("unexpected NavyCraft handshake nonce");
        if (response.kind == navycraft::ConstructHandshakeKind::ServerReject) {
            m_navycraft_handshake_accepted = false;
            errorstream << "NavyCraft native engine rejected: "
                << response.reason << std::endl;
            return;
        }
        if (!response.accepted())
            throw std::runtime_error("server did not accept NavyCraft native protocol");
        const auto local = navycraft::makeClientConstructHello(response.nonce);
        const auto verified = navycraft::ConstructHandshakeCodec::evaluate(
            local, response);
        if (!verified.accepted())
            throw std::runtime_error("server accepted incompatible NavyCraft feature set");
        m_navycraft_handshake_accepted = true;
        ensureNavyCraftConstructScene();
        actionstream << "NavyCraft native protocol "
            << response.construct_protocol << " accepted ("
            << response.build_id << ")" << std::endl;
    } catch (const std::exception &error) {
        m_navycraft_handshake_accepted = false;
        errorstream << "Invalid NavyCraft handshake response: "
            << error.what() << std::endl;
    }
}

void Client::handleCommand_NavyCraftConstructSection(NetworkPacket *packet)
{
    if (!m_navycraft_handshake_accepted)
        return;
    try {
        ensureNavyCraftConstructScene();
        if (!m_navycraft_construct_scene->applySectionPacket(readPayload(packet)))
            warningstream << "NavyCraft rejected stale construct section packet" << std::endl;
    } catch (const std::exception &error) {
        errorstream << "Invalid NavyCraft construct section packet: " << error.what() << std::endl;
    }
}

void Client::handleCommand_NavyCraftConstructTransform(NetworkPacket *packet)
{
    if (!m_navycraft_handshake_accepted)
        return;
    try {
        ensureNavyCraftConstructScene();
        if (!m_navycraft_construct_scene->applyTransformPacket(
                readPayload(packet), m_navycraft_client_time))
            warningstream << "NavyCraft rejected stale construct transform packet" << std::endl;
    } catch (const std::exception &error) {
        errorstream << "Invalid NavyCraft construct transform packet: " << error.what() << std::endl;
    }
}

void Client::handleCommand_NavyCraftConstructRemove(NetworkPacket *packet)
{
    if (!m_navycraft_handshake_accepted)
        return;
    try {
        ensureNavyCraftConstructScene();
        const auto payload = readPayload(packet);
        const navycraft::ConstructId id = navycraft::ConstructPacketCodec::decodeRemove(payload);
        if (m_navycraft_construct_effects)
            m_navycraft_construct_effects->removeConstruct(id);
        (void)m_navycraft_construct_scene->applyRemovePacket(payload);
    } catch (const std::exception &error) {
        errorstream << "Invalid NavyCraft construct remove packet: " << error.what() << std::endl;
    }
}

void Client::handleCommand_NavyCraftConstructEffect(NetworkPacket *packet)
{
    if (!m_navycraft_handshake_accepted)
        return;
    try {
        ensureNavyCraftConstructScene();
        if (!m_navycraft_construct_effects->applyPacket(readPayload(packet)))
            warningstream << "NavyCraft rejected stale construct effect packet" << std::endl;
    } catch (const std::exception &error) {
        errorstream << "Invalid NavyCraft construct effect packet: " << error.what() << std::endl;
    }
}

void Client::handleCommand_NavyCraftConstructProjectile(NetworkPacket *packet)
{
    if (!m_navycraft_handshake_accepted)
        return;
    try {
        ensureNavyCraftConstructScene();
        if (!m_navycraft_construct_effects->applyProjectilePacket(readPayload(packet)))
            warningstream << "NavyCraft rejected stale construct projectile packet" << std::endl;
    } catch (const std::exception &error) {
        errorstream << "Invalid NavyCraft construct projectile packet: "
            << error.what() << std::endl;
    }
}

void Client::handleCommand_NavyCraftConstructArticulation(NetworkPacket *packet)
{
    if (!m_navycraft_handshake_accepted)
        return;
    try {
        ensureNavyCraftConstructScene();
        if (!m_navycraft_construct_scene->applyArticulationPacket(readPayload(packet)))
            warningstream << "NavyCraft rejected stale construct articulation packet" << std::endl;
    } catch (const std::exception &error) {
        errorstream << "Invalid NavyCraft construct articulation packet: "
            << error.what() << std::endl;
    }
}


void Client::handleCommand_NavyCraftConstructReset(NetworkPacket *packet)
{
    (void)packet;
    if (m_navycraft_handshake_accepted)
        resetNavyCraftConstructScene();
}

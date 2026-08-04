// SPDX-License-Identifier: LGPL-2.1-or-later
#include "client_construct_scene.h"

#include "client/client.h"
#include "client/localplayer.h"
#include "client/mapblock_mesh.h"
#include "constants.h"
#include "mapnode.h"
#include "nodedef.h"
#include "voxel.h"

#include <IMeshSceneNode.h>
#include <ISceneManager.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <unordered_set>

namespace navycraft {
namespace {
constexpr double RADIANS_TO_DEGREES = 57.2957795130823208768;

v3f toScenePosition(const Vec3d &position)
{
    return v3f(
        static_cast<f32>(position.x * BS),
        static_cast<f32>(position.y * BS),
        static_cast<f32>(position.z * BS));
}

v3s16 checkedSectionPosition(const ConstructSectionPos &position)
{
    const auto within = [](std::int32_t value) {
        return value >= std::numeric_limits<s16>::min() &&
            value <= std::numeric_limits<s16>::max();
    };
    if (!within(position.x) || !within(position.y) || !within(position.z))
        throw std::out_of_range("NavyCraft section position exceeds Luanti mesh coordinates");
    return v3s16(
        static_cast<s16>(position.x),
        static_cast<s16>(position.y),
        static_cast<s16>(position.z));
}

v3s16 checkedNodePosition(const LocalNodePos &position)
{
    const auto within = [](std::int32_t value) {
        return value >= std::numeric_limits<s16>::min() &&
            value <= std::numeric_limits<s16>::max();
    };
    if (!within(position.x) || !within(position.y) || !within(position.z))
        throw std::out_of_range("NavyCraft node position exceeds Luanti mesh coordinates");
    return v3s16(
        static_cast<s16>(position.x),
        static_cast<s16>(position.y),
        static_cast<s16>(position.z));
}

bool meshHasGeometry(scene::IMesh *mesh)
{
    if (!mesh)
        return false;
    for (u32 index = 0; index < mesh->getMeshBufferCount(); ++index) {
        const scene::IMeshBuffer *buffer = mesh->getMeshBuffer(index);
        if (buffer && buffer->getVertexCount() > 0 && buffer->getIndexCount() > 0)
            return true;
    }
    return false;
}

void fillConstructMeshData(
    MeshMakeData &data,
    const DynamicConstruct &construct,
    const NodeDefManager *node_def,
    const ClientConstructArticulationState *articulations,
    std::optional<ConstructArticulationId> only_joint = std::nullopt)
{
    const MapNode lit_air(CONTENT_AIR, 0xff, 0);
    const u32 volume = data.m_vmanip.m_area.getVolume();
    for (u32 index = 0; index < volume; ++index) {
        data.m_vmanip.m_data[index] = lit_air;
        data.m_vmanip.m_flags[index] &= ~VOXELFLAG_NO_DATA;
    }

    for (const auto &entry : construct.nodes()) {
        const auto owner = articulations ?
            articulations->nodeJoint(construct.id(), entry.position) : std::nullopt;
        if (only_joint) {
            if (!owner || *owner != *only_joint)
                continue;
        } else if (owner) {
            continue;
        }
        const v3s16 position = checkedNodePosition(entry.position);
        if (!data.m_vmanip.m_area.contains(position))
            continue;
        MapNode node(entry.node.content_id, entry.node.param1, entry.node.param2);
        if (node.getContent() == CONTENT_IGNORE ||
                node_def->get(node).name.empty()) {
            content_t resolved = CONTENT_IGNORE;
            if (!entry.node.node_name.empty() &&
                    node_def->getId(entry.node.node_name, resolved))
                node.setContent(resolved);
            else
                node.setContent(CONTENT_UNKNOWN);
        }
        data.m_vmanip.setNodeNoEmerge(position, node);
    }
}
}

struct ClientConstructScene::SceneConstruct {
    struct SectionScene {
        std::unique_ptr<MapBlockMesh> mapblock_mesh;
        std::vector<scene::IMeshSceneNode *> nodes;
    };
    struct ArticulationScene {
        ConstructArticulationDefinition definition{};
        scene::ISceneNode *root = nullptr;
        Vec3d base_offset{};
        std::unordered_map<ConstructSectionPos,
            SectionScene, ConstructSectionPosHash> sections;
    };

    scene::ISceneNode *root = nullptr;
    std::unordered_map<ConstructSectionPos,
        SectionScene, ConstructSectionPosHash> sections;
    std::unordered_map<ConstructArticulationId, ArticulationScene> articulations;
    std::uint64_t articulation_revision = 0;
    std::uint64_t node_revision = 0;
};

ClientConstructScene::ClientConstructScene(Client *client) :
    m_client(client), m_collision_articulations(m_collision_registry)
{
    if (!m_client || !m_client->getSceneManager())
        throw std::invalid_argument("NavyCraft client scene requires a live Luanti client");
}

ClientConstructScene::~ClientConstructScene()
{
    reset();
}

bool ClientConstructScene::applySectionPacket(const std::vector<std::uint8_t> &payload)
{
    return m_manager.applySectionPacket(payload);
}

bool ClientConstructScene::applyTransformPacket(
    const std::vector<std::uint8_t> &payload, double client_receive_time)
{
    return m_manager.applyTransformPacket(payload, client_receive_time);
}

bool ClientConstructScene::applyRemovePacket(const std::vector<std::uint8_t> &payload)
{
    const ConstructId id = ConstructPacketCodec::decodeRemove(payload);
    const bool removed = m_manager.remove(id);
    m_articulations.removeConstruct(id);
    removeSceneConstruct(id);
    return removed;
}

bool ClientConstructScene::applyArticulationPacket(
    const std::vector<std::uint8_t> &payload)
{
    return m_articulations.applyPacket(payload);
}

void ClientConstructScene::reset()
{
    for (auto &[id, construct] : m_scene_constructs) {
        (void)id;
        if (construct && construct->root)
            construct->root->remove();
    }
    m_scene_constructs.clear();
    m_manager.clear();
    m_articulations.clear();
    m_collision_articulations.clear();
    m_collision_registry.clear();
    m_collision_world.clear();
    m_local_articulated_rider_state = {};
    m_local_rider_state = {};
    m_rider_sequence = 0;
    m_last_rider_packet_time = -1.0;
    m_last_sent_rider_construct = 0;
    m_last_sent_local_anchor = {};
    m_last_sent_local_velocity = {};
    m_have_last_sent_rider_motion = false;
    m_pending_local_player_move = {};
}

void ClientConstructScene::step(double client_time)
{
    const u32 daynight_ratio = m_client->getEnv().getDayNightRatio();
    const float animation_time = static_cast<float>(std::fmod(client_time, 60.0));

    for (const ConstructId id : m_manager.ids()) {
        const auto state = m_manager.find(id);
        if (!state)
            continue;
        auto &scene_construct = ensureSceneConstruct(id);
        const bool rebuild_all =
            scene_construct.articulation_revision != m_articulations.revision() ||
            scene_construct.node_revision != state->construct().nodeRevision();
        const auto dirty = state->rebuildDirtyMeshes();
        if (rebuild_all) {
            for (auto &[position, section] : scene_construct.sections) {
                (void)position;
                for (auto *node : section.nodes) {
                    if (node)
                        node->remove();
                }
            }
            scene_construct.sections.clear();
            for (const auto &position : state->sectionPositions())
                rebuildSection(scene_construct, *state, position);
            rebuildArticulations(scene_construct, *state);
            scene_construct.articulation_revision = m_articulations.revision();
            scene_construct.node_revision = state->construct().nodeRevision();
        } else {
            for (const auto &position : dirty)
                rebuildSection(scene_construct, *state, position);
        }

        for (auto &[position, section] : scene_construct.sections) {
            (void)position;
            if (section.mapblock_mesh)
                section.mapblock_mesh->animate(false, animation_time, -1, daynight_ratio);
        }
        for (auto &[joint_id, articulation] : scene_construct.articulations) {
            (void)joint_id;
            for (auto &[position, section] : articulation.sections) {
                (void)position;
                if (section.mapblock_mesh)
                    section.mapblock_mesh->animate(false, animation_time, -1, daynight_ratio);
            }
        }
        updateArticulationPoses(scene_construct, client_time);

        const ConstructTransform transform = state->sampleTransform(
            m_manager.serverTime(client_time));
        state->setSampledTransform(transform);
        scene_construct.root->setPosition(toScenePosition(transform.position));
        scene_construct.root->setRotation(v3f(
            0.0f,
            static_cast<f32>(-transform.yaw_radians * RADIANS_TO_DEGREES),
            0.0f));
        scene_construct.root->setVisible(true);
    }

    // Rebuild the broad-phase after sampled transforms are committed so the
    // player movement phase sees the same poses that are rendered.
    m_collision_world.rebuild(m_manager.constructs(), 0.25);
}

void ClientConstructScene::rebuildCollisionArticulations(double client_time)
{
    m_collision_articulations.clear();
    m_collision_registry.clear();
    for (const auto id : m_manager.ids()) {
        const auto state = m_manager.find(id);
        if (!state)
            continue;
        auto clone = m_collision_registry.createWithId(id);
        clone->setOwner(state->construct().owner());
        clone->setTransform(state->construct().transform());
        clone->setLinearVelocity(state->construct().linearVelocity());
        clone->setYawVelocity(state->construct().yawVelocity());
        for (const auto &entry : state->construct().nodes())
            clone->setNode(entry.position, entry.node);
    }
    auto pending = m_articulations.definitions();
    std::size_t guard = 0;
    while (!pending.empty() && guard++ <= pending.size() + 8U) {
        bool progressed = false;
        for (auto iterator = pending.begin(); iterator != pending.end();) {
            if (iterator->parent_id != 0 &&
                    !m_collision_articulations.joint(iterator->parent_id)) {
                ++iterator;
                continue;
            }
            try {
                const auto id = m_collision_articulations.configureJoint(*iterator);
                (void)m_collision_articulations.setJointPosition(id,
                    m_articulations.samplePosition(id, client_time));
                iterator = pending.erase(iterator);
                progressed = true;
            } catch (...) {
                iterator = pending.erase(iterator);
            }
        }
        if (!progressed)
            break;
    }
}

LocalPlayerMotionOutput ClientConstructScene::beginLocalPlayerMove(
    const LocalPlayerMotionInput &input, double client_time)
{
    LocalPlayerMotionOutput result;
    result.position = input.position;
    result.velocity = input.velocity;
    result.touching_ground = input.touching_ground;

    if (input.delta_seconds <= 0.0 || !std::isfinite(input.delta_seconds) ||
            !input.world_box.valid()) {
        m_pending_local_player_move = {};
        return result;
    }

    rebuildCollisionArticulations(client_time);
    const ConstructId previous_construct = m_local_articulated_rider_state.contact.active ?
        m_local_articulated_rider_state.contact.construct_id :
        (m_local_rider_state.contact.active ?
            m_local_rider_state.contact.construct_id : 0);

    // The scene can be constructed before the first visual step. Rebuilding is
    // cheap when the list is small and guarantees that native movement never
    // falls back to an unindexed all-construct scan.
    if (m_collision_world.stats().construct_count == 0 && !m_manager.ids().empty())
        m_collision_world.rebuild(m_manager.constructs(), 0.25);
    const auto rider_candidates = m_collision_world.query(
        input.world_box, {}, input.delta_seconds, previous_construct);

    RiderMotionInput rider_input;
    rider_input.position = input.position;
    rider_input.velocity = input.velocity;
    rider_input.world_box = input.world_box;
    rider_input.jump_pressed = input.jump_pressed;
    rider_input.allow_contact_acquisition = false;
    rider_input.delta_seconds = input.delta_seconds;
    RiderMotionOutput rider = RiderMotionSolver::step(
        m_local_rider_state, rider_candidates, rider_input);

    ArticulatedMotionInput articulated_input;
    articulated_input.position = rider.position;
    articulated_input.velocity = rider.velocity;
    articulated_input.world_box = input.world_box.translated(
        rider.position - input.position);
    articulated_input.jump_pressed = input.jump_pressed;
    articulated_input.allow_contact_acquisition = false;
    articulated_input.delta_seconds = input.delta_seconds;
    const ArticulatedMotionOutput articulated = ConstructArticulatedMotion::stepRider(
        m_local_articulated_rider_state, m_collision_articulations,
        articulated_input);

    result.position = rider.position;
    result.velocity = rider.velocity;
    result.touching_ground = result.touching_ground || rider.grounded;
    result.jumped = rider.jumped;
    if (articulated.grounded || m_local_articulated_rider_state.contact.active ||
            articulated.contact_ended || articulated.jumped) {
        result.position = articulated.position;
        result.velocity = articulated.velocity;
        result.touching_ground = result.touching_ground || articulated.grounded;
        result.jumped = result.jumped || articulated.jumped;
        if (articulated.contact_started)
            PlatformContact::clear(m_local_rider_state.contact);
    }

    m_pending_local_player_move.position = result.position;
    m_pending_local_player_move.velocity = result.velocity;
    m_pending_local_player_move.world_box = input.world_box.translated(
        result.position - input.position);
    m_pending_local_player_move.previous_construct = previous_construct;
    m_pending_local_player_move.touching_ground = result.touching_ground;
    m_pending_local_player_move.jumped = result.jumped;
    m_pending_local_player_move.active = true;
    return result;
}

LocalPlayerMotionFinish ClientConstructScene::finishLocalPlayerMove(
    const LocalPlayerMotionInput &input, double client_time)
{
    LocalPlayerMotionFinish result;
    result.motion.position = input.position;
    result.motion.velocity = input.velocity;
    result.motion.touching_ground = input.touching_ground;

    if (!m_pending_local_player_move.active ||
            input.delta_seconds <= 0.0 || !std::isfinite(input.delta_seconds) ||
            !input.world_box.valid()) {
        m_pending_local_player_move = {};
        return result;
    }

    const PendingLocalPlayerMove pending = m_pending_local_player_move;
    m_pending_local_player_move = {};

    const Vec3d stock_delta = input.position - pending.position;
    const auto collision_candidates = m_collision_world.query(
        pending.world_box, stock_delta, input.delta_seconds,
        pending.previous_construct);
    HullCollisionInput collision_input;
    collision_input.world_box = pending.world_box;
    collision_input.velocity = input.velocity;
    collision_input.desired_delta = stock_delta;
    collision_input.delta_seconds = input.delta_seconds;
    const HullCollisionOutput collision = MovingHullCollisionSolver::move(
        collision_candidates, collision_input);

    Vec3d final_position = pending.position + collision.allowed_delta;
    Vec3d final_velocity = collision.velocity;
    bool grounded = pending.touching_ground || input.touching_ground ||
        collision.touching_ground;

    ArticulatedMotionInput articulated_collision_input;
    articulated_collision_input.position = pending.position;
    articulated_collision_input.velocity = final_velocity;
    articulated_collision_input.world_box = pending.world_box;
    articulated_collision_input.desired_delta = final_position - pending.position;
    articulated_collision_input.delta_seconds = input.delta_seconds;
    const ArticulatedMotionOutput articulated_collision =
        ConstructArticulatedMotion::move(m_collision_articulations,
            articulated_collision_input);
    final_position = articulated_collision.position;
    final_velocity = articulated_collision.velocity;
    grounded = grounded || articulated_collision.grounded;

    const Aabb3d final_box = pending.world_box.translated(
        final_position - pending.position);

    // Contact acquisition happens only after Luanti's stock map collision and
    // the moving-hull sweep have produced the frame's final position. This
    // avoids acquiring a deck contact from a pre-collision pose and then
    // correcting the player on the following frame.
    RiderMotionInput support_input;
    support_input.position = final_position;
    support_input.velocity = final_velocity;
    support_input.world_box = final_box;
    support_input.jump_pressed = input.jump_pressed;
    support_input.allow_contact_acquisition = true;
    support_input.delta_seconds = 0.0;
    const RiderMotionOutput support = RiderMotionSolver::step(
        m_local_rider_state, collision_candidates, support_input);
    final_position = support.position;
    final_velocity = support.velocity;
    grounded = grounded || support.grounded;

    ArticulatedMotionInput articulated_support_input;
    articulated_support_input.position = final_position;
    articulated_support_input.velocity = final_velocity;
    articulated_support_input.world_box = final_box.translated(
        final_position - support_input.position);
    articulated_support_input.jump_pressed = input.jump_pressed;
    articulated_support_input.allow_contact_acquisition = true;
    articulated_support_input.delta_seconds = 0.0;
    const ArticulatedMotionOutput articulated_support =
        ConstructArticulatedMotion::stepRider(
            m_local_articulated_rider_state, m_collision_articulations,
            articulated_support_input);
    if (articulated_support.grounded ||
            m_local_articulated_rider_state.contact.active ||
            articulated_support.contact_started) {
        final_position = articulated_support.position;
        final_velocity = articulated_support.velocity;
        grounded = grounded || articulated_support.grounded;
        if (articulated_support.contact_started)
            PlatformContact::clear(m_local_rider_state.contact);
    }

    result.motion.position = final_position;
    result.motion.velocity = final_velocity;
    result.motion.touching_ground = grounded;
    result.motion.jumped = pending.jumped || support.jumped || articulated_support.jumped;

    const bool articulated_contact = m_local_articulated_rider_state.contact.active;
    const ConstructId current_construct = articulated_contact ?
        m_local_articulated_rider_state.contact.construct_id :
        (m_local_rider_state.contact.active ?
            m_local_rider_state.contact.construct_id : 0);

    RiderStatePacket packet;
    packet.construct_id = current_construct;
    if (articulated_contact) {
        packet.articulation_id = m_local_articulated_rider_state.contact.articulation_id;
        packet.local_anchor = m_local_articulated_rider_state.contact.local_anchor;
        // Articulated local velocity is resolved by the joint state on the
        // server. Sending a second prediction term would double-count it.
        packet.local_velocity = {};
    } else if (m_local_rider_state.contact.active) {
        packet.local_anchor = m_local_rider_state.contact.local_anchor;
        if (const DynamicConstruct *construct = m_collision_world.find(current_construct)) {
            const Vec3d surface_velocity = ConstructGeometry::surfaceVelocity(
                *construct, final_position);
            const Vec3d relative_world_velocity = final_velocity - surface_velocity;
            const Vec3d local_origin = construct->transform().worldToLocal(final_position);
            packet.local_velocity = construct->transform().worldToLocal(
                final_position + relative_world_velocity) - local_origin;
        }
    }

    const auto changedEnough = [](const Vec3d &a, const Vec3d &b, double epsilon) {
        const Vec3d delta = a - b;
        return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z >
            epsilon * epsilon;
    };
    const bool contact_changed = current_construct != pending.previous_construct ||
        current_construct != m_last_sent_rider_construct;
    const bool local_motion_changed = !m_have_last_sent_rider_motion ||
        changedEnough(packet.local_anchor, m_last_sent_local_anchor, 0.02) ||
        changedEnough(packet.local_velocity, m_last_sent_local_velocity, 0.05);
    const bool periodic = m_last_rider_packet_time < 0.0 ||
        client_time - m_last_rider_packet_time >= 0.1;
    if (!contact_changed && !local_motion_changed &&
            !result.motion.jumped && !periodic)
        return result;

    packet.sequence = ++m_rider_sequence;
    packet.world_position = final_position;
    packet.velocity = final_velocity;
    packet.client_time = client_time;
    packet.grounded = grounded;
    packet.jumping = result.motion.jumped;
    m_last_rider_packet_time = client_time;
    m_last_sent_rider_construct = current_construct;
    m_last_sent_local_anchor = packet.local_anchor;
    m_last_sent_local_velocity = packet.local_velocity;
    m_have_last_sent_rider_motion = true;
    result.rider_packet = packet;
    return result;
}

ClientConstructManager &ClientConstructScene::manager() noexcept
{
    return m_manager;
}

const ClientConstructManager &ClientConstructScene::manager() const noexcept
{
    return m_manager;
}

ClientConstructArticulationState &ClientConstructScene::articulations() noexcept
{
    return m_articulations;
}

const ClientConstructArticulationState &ClientConstructScene::articulations() const noexcept
{
    return m_articulations;
}

ClientConstructScene::SceneConstruct &ClientConstructScene::ensureSceneConstruct(ConstructId id)
{
    const auto existing = m_scene_constructs.find(id);
    if (existing != m_scene_constructs.end())
        return *existing->second;

    auto result = std::make_unique<SceneConstruct>();
    result->root = m_client->getSceneManager()->addEmptySceneNode();
    if (!result->root)
        throw std::runtime_error("failed to create NavyCraft construct scene root");
    auto *raw = result.get();
    m_scene_constructs.emplace(id, std::move(result));
    return *raw;
}

void ClientConstructScene::removeSceneConstruct(ConstructId id)
{
    const auto iterator = m_scene_constructs.find(id);
    if (iterator == m_scene_constructs.end())
        return;
    if (iterator->second && iterator->second->root)
        iterator->second->root->remove();
    m_scene_constructs.erase(iterator);
}

void ClientConstructScene::rebuildSection(
    SceneConstruct &scene_construct,
    const ClientConstructState &state,
    const ConstructSectionPos &position)
{
    auto existing = scene_construct.sections.find(position);
    if (existing != scene_construct.sections.end()) {
        for (auto *node : existing->second.nodes) {
            if (node)
                node->remove();
        }
        scene_construct.sections.erase(existing);
    }

    if (!state.section(position))
        return;

    const v3s16 block_position = checkedSectionPosition(position);
    MeshMakeData data(m_client->getNodeDefManager(), CONSTRUCT_SECTION_SIZE, MeshGrid{1});
    data.fillBlockDataBegin(block_position);
    data.m_generate_minimap = false;
    data.m_smooth_lighting = true;
    fillConstructMeshData(data, state.construct(), m_client->getNodeDefManager(),
        &m_articulations);

    SceneConstruct::SectionScene rendered;
    rendered.mapblock_mesh = std::make_unique<MapBlockMesh>(m_client, &data);
    rendered.mapblock_mesh->materializeTransparentBuffersForSceneNode();

    const v3f section_offset = intToFloat(block_position * MAP_BLOCKSIZE, BS);
    for (u8 layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
        scene::IMesh *mesh = rendered.mapblock_mesh->getMesh(layer);
        if (!meshHasGeometry(mesh))
            continue;
        scene::IMeshSceneNode *mesh_node = m_client->getSceneManager()->addMeshSceneNode(
            mesh, scene_construct.root);
        if (!mesh_node)
            throw std::runtime_error("failed to create NavyCraft MapBlockMesh scene node");
        mesh_node->setPosition(section_offset);
        mesh_node->setAutomaticCulling(scene::EAC_BOX);
        rendered.nodes.push_back(mesh_node);
    }

    if (!rendered.nodes.empty())
        scene_construct.sections.emplace(position, std::move(rendered));
}

void ClientConstructScene::rebuildArticulations(
    SceneConstruct &scene_construct,
    const ClientConstructState &state)
{
    for (auto &[id, articulation] : scene_construct.articulations) {
        (void)id;
        if (articulation.root)
            articulation.root->remove();
    }
    scene_construct.articulations.clear();

    const auto definitions = m_articulations.definitions(state.id());
    std::unordered_map<ConstructArticulationId, ConstructArticulationDefinition> pending;
    for (const auto &definition : definitions) {
        if (definition.render_enabled)
            pending.emplace(definition.id, definition);
    }
    std::size_t guard = 0;
    while (!pending.empty() && guard++ <= definitions.size() + 1) {
        bool progressed = false;
        for (auto iterator = pending.begin(); iterator != pending.end();) {
            const auto &definition = iterator->second;
            scene::ISceneNode *parent_node = scene_construct.root;
            Vec3d parent_pivot{};
            if (definition.parent_id != 0) {
                const auto parent = scene_construct.articulations.find(definition.parent_id);
                if (parent == scene_construct.articulations.end()) {
                    ++iterator;
                    continue;
                }
                parent_node = parent->second.root;
                parent_pivot = parent->second.definition.pivot_local;
            }

            SceneConstruct::ArticulationScene rendered;
            rendered.definition = definition;
            rendered.base_offset = definition.pivot_local - parent_pivot;
            rendered.root = m_client->getSceneManager()->addEmptySceneNode(parent_node);
            if (!rendered.root)
                throw std::runtime_error("failed to create NavyCraft articulation scene root");
            rendered.root->setPosition(toScenePosition(rendered.base_offset));

            std::unordered_set<ConstructSectionPos, ConstructSectionPosHash> sections;
            for (const auto &node_position : definition.nodes)
                sections.insert(ConstructSectionIndex::sectionFor(node_position));
            for (const auto &position : sections) {
                const v3s16 block_position = checkedSectionPosition(position);
                MeshMakeData data(m_client->getNodeDefManager(),
                    CONSTRUCT_SECTION_SIZE, MeshGrid{1});
                data.fillBlockDataBegin(block_position);
                data.m_generate_minimap = false;
                data.m_smooth_lighting = true;
                fillConstructMeshData(data, state.construct(),
                    m_client->getNodeDefManager(), &m_articulations, definition.id);

                SceneConstruct::SectionScene section_scene;
                section_scene.mapblock_mesh = std::make_unique<MapBlockMesh>(m_client, &data);
                section_scene.mapblock_mesh->materializeTransparentBuffersForSceneNode();
                const Vec3d pivot = definition.pivot_local;
                const Vec3d section_nodes{
                    static_cast<double>(block_position.X * MAP_BLOCKSIZE),
                    static_cast<double>(block_position.Y * MAP_BLOCKSIZE),
                    static_cast<double>(block_position.Z * MAP_BLOCKSIZE),
                };
                const v3f section_offset = toScenePosition(section_nodes - pivot);
                for (u8 layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
                    scene::IMesh *mesh = section_scene.mapblock_mesh->getMesh(layer);
                    if (!meshHasGeometry(mesh))
                        continue;
                    scene::IMeshSceneNode *mesh_node =
                        m_client->getSceneManager()->addMeshSceneNode(mesh, rendered.root);
                    if (!mesh_node)
                        throw std::runtime_error(
                            "failed to create NavyCraft articulation mesh scene node");
                    mesh_node->setPosition(section_offset);
                    mesh_node->setAutomaticCulling(scene::EAC_BOX);
                    section_scene.nodes.push_back(mesh_node);
                }
                if (!section_scene.nodes.empty())
                    rendered.sections.emplace(position, std::move(section_scene));
            }
            scene_construct.articulations.emplace(definition.id, std::move(rendered));
            iterator = pending.erase(iterator);
            progressed = true;
        }
        if (!progressed)
            throw std::runtime_error("NavyCraft articulation hierarchy contains a missing parent");
    }
}

void ClientConstructScene::updateArticulationPoses(
    SceneConstruct &scene_construct, double client_time)
{
    for (auto &[id, articulation] : scene_construct.articulations) {
        if (!articulation.root)
            continue;
        const double position = m_articulations.samplePosition(id, client_time);
        Vec3d offset = articulation.base_offset;
        v3f rotation{};
        if (articulation.definition.kind == ConstructJointKind::Prismatic) {
            offset += articulation.definition.axis_local * position;
        } else {
            rotation = v3f(
                static_cast<f32>(-articulation.definition.axis_local.x * position *
                    RADIANS_TO_DEGREES),
                static_cast<f32>(-articulation.definition.axis_local.y * position *
                    RADIANS_TO_DEGREES),
                static_cast<f32>(-articulation.definition.axis_local.z * position *
                    RADIANS_TO_DEGREES));
        }
        articulation.root->setPosition(toScenePosition(offset));
        articulation.root->setRotation(rotation);
        articulation.root->setVisible(articulation.definition.enabled);
    }
}

} // namespace navycraft

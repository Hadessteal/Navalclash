// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct/client_construct_state.h"
#include "construct/construct_articulation_packets.h"
#include "construct/moving_hull_collision.h"
#include "construct/construct_collision_world.h"
#include "construct/rider_motion.h"
#include "construct/rider_packets.h"
#include "construct/construct_articulated_motion.h"
#include "construct/construct_registry.h"
#include "construct/construct_articulation.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

class Client;
class LocalPlayer;

namespace navycraft {

struct LocalPlayerMotionInput {
    Vec3d position{};
    Vec3d velocity{};
    Aabb3d world_box{};
    bool jump_pressed = false;
    bool touching_ground = false;
    double delta_seconds = 0.0;
};

struct LocalPlayerMotionOutput {
    Vec3d position{};
    Vec3d velocity{};
    bool touching_ground = false;
    bool jumped = false;
};

struct LocalPlayerMotionFinish {
    LocalPlayerMotionOutput motion{};
    std::optional<RiderStatePacket> rider_packet;
};

class ClientConstructScene final {
public:
    explicit ClientConstructScene(Client *client);
    ~ClientConstructScene();

    ClientConstructScene(const ClientConstructScene &) = delete;
    ClientConstructScene &operator=(const ClientConstructScene &) = delete;

    bool applySectionPacket(const std::vector<std::uint8_t> &payload);
    bool applyTransformPacket(const std::vector<std::uint8_t> &payload, double client_receive_time);
    bool applyRemovePacket(const std::vector<std::uint8_t> &payload);
    bool applyArticulationPacket(const std::vector<std::uint8_t> &payload);
    void reset();
    void step(double client_time);
    [[nodiscard]] LocalPlayerMotionOutput beginLocalPlayerMove(
        const LocalPlayerMotionInput &input, double client_time);
    [[nodiscard]] LocalPlayerMotionFinish finishLocalPlayerMove(
        const LocalPlayerMotionInput &input, double client_time);

    [[nodiscard]] ClientConstructManager &manager() noexcept;
    [[nodiscard]] const ClientConstructManager &manager() const noexcept;
    [[nodiscard]] ClientConstructArticulationState &articulations() noexcept;
    [[nodiscard]] const ClientConstructArticulationState &articulations() const noexcept;

private:
    struct SceneConstruct;

    SceneConstruct &ensureSceneConstruct(ConstructId id);
    void removeSceneConstruct(ConstructId id);
    void rebuildSection(
        SceneConstruct &scene_construct,
        const ClientConstructState &state,
        const ConstructSectionPos &position);
    void rebuildArticulations(SceneConstruct &scene_construct,
        const ClientConstructState &state);
    void updateArticulationPoses(SceneConstruct &scene_construct,
        double client_time);
    void rebuildCollisionArticulations(double client_time);

    Client *m_client = nullptr;
    ClientConstructManager m_manager;
    ClientConstructArticulationState m_articulations;
    ConstructRegistry m_collision_registry;
    ConstructCollisionWorld m_collision_world;
    ConstructArticulationEngine m_collision_articulations;
    std::unordered_map<ConstructId, std::unique_ptr<SceneConstruct>> m_scene_constructs;
    RiderMotionState m_local_rider_state{};
    ArticulatedRiderState m_local_articulated_rider_state{};
    std::uint64_t m_rider_sequence = 0;
    double m_last_rider_packet_time = -1.0;
    ConstructId m_last_sent_rider_construct = 0;
    Vec3d m_last_sent_local_anchor{};
    Vec3d m_last_sent_local_velocity{};
    bool m_have_last_sent_rider_motion = false;
    struct PendingLocalPlayerMove {
        Vec3d position{};
        Vec3d velocity{};
        Aabb3d world_box{};
        ConstructId previous_construct = 0;
        bool touching_ground = false;
        bool jumped = false;
        bool active = false;
    };

    PendingLocalPlayerMove m_pending_local_player_move{};
};

} // namespace navycraft

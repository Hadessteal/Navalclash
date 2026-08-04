// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_articulation.h"
#include "moving_hull_collision.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace navycraft {

struct ArticulatedSupportHit {
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    LocalNodePos node_position{};
    Vec3d local_anchor{};
    Vec3d world_point{};
    Vec3d world_normal{};
    Vec3d surface_velocity{};
    double vertical_gap = 0.0;
};

struct ArticulatedPlatformContactState {
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    Vec3d local_anchor{};
    Vec3d last_world_anchor{};
    bool active = false;
};

struct ArticulatedRiderState {
    ArticulatedPlatformContactState contact{};
    Vec3d last_surface_velocity{};
    bool previous_jump_pressed = false;
    double unsupported_seconds = 0.0;
};

struct ArticulatedMotionInput {
    Vec3d position{};
    Vec3d velocity{};
    Aabb3d world_box{};
    Vec3d desired_delta{};
    bool jump_pressed = false;
    bool detach_requested = false;
    bool inherit_platform_velocity_on_detach = true;
    bool allow_contact_acquisition = true;
    double delta_seconds = 0.0;
    double maximum_sweep_step = 0.12;
    double skin_width = 1e-4;
    std::size_t maximum_iterations = 5;
};

struct ArticulatedMotionOutput {
    Vec3d position{};
    Vec3d velocity{};
    Vec3d platform_displacement{};
    Vec3d inherited_velocity{};
    Vec3d allowed_delta{};
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    bool grounded = false;
    bool jumped = false;
    bool contact_started = false;
    bool contact_ended = false;
    bool collided = false;
    bool hit_wall = false;
    bool hit_ceiling = false;
    std::vector<HullCollisionContact> contacts;
};

class ConstructArticulatedMotion final {
public:
    [[nodiscard]] static std::optional<ArticulatedSupportHit> findSupport(
        const ConstructArticulationEngine &engine,
        const Aabb3d &world_box,
        double support_drop = 0.35,
        double support_penetration = 0.15) noexcept;

    [[nodiscard]] static ArticulatedMotionOutput stepRider(
        ArticulatedRiderState &state,
        const ConstructArticulationEngine &engine,
        const ArticulatedMotionInput &input,
        double support_drop = 0.35,
        double support_penetration = 0.15,
        double contact_grace = 0.12) noexcept;

    [[nodiscard]] static ArticulatedMotionOutput move(
        const ConstructArticulationEngine &engine,
        const ArticulatedMotionInput &input) noexcept;

    [[nodiscard]] static Vec3d contactWorldPoint(
        const ConstructArticulationEngine &engine,
        const ArticulatedPlatformContactState &contact) noexcept;
};

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_geometry.h"

#include <cstddef>
#include <vector>

namespace navycraft {

enum class HullContactKind {
    Floor,
    Wall,
    Ceiling,
};

struct HullCollisionContact {
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    Vec3d world_normal{};
    Vec3d world_point{};
    Vec3d surface_velocity{};
    HullContactKind kind = HullContactKind::Wall;
};

struct HullCollisionInput {
    Aabb3d world_box{};
    Vec3d velocity{};
    Vec3d desired_delta{};
    double delta_seconds = 0.0;
    double maximum_sweep_step = 0.20;
    double skin_width = 1e-4;
    std::size_t maximum_iterations = 5;
};

struct HullCollisionOutput {
    Vec3d allowed_delta{};
    Vec3d position_correction{};
    Vec3d velocity{};
    std::vector<HullCollisionContact> contacts;
    bool collided = false;
    bool touching_ground = false;
    bool hit_wall = false;
    bool hit_ceiling = false;
};

class MovingHullCollisionSolver final {
public:
    [[nodiscard]] static HullCollisionOutput move(
        const std::vector<const DynamicConstruct *> &constructs,
        const HullCollisionInput &input) noexcept;
};

} // namespace navycraft

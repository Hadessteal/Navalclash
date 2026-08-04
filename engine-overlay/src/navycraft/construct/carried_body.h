// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "moving_hull_collision.h"
#include "rider_motion.h"
#include "construct_articulated_motion.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace navycraft {

using CarriedBodyId = std::uint64_t;

enum class CarriedBodyKind {
    RemotePlayer,
    Creature,
    DroppedItem,
};

struct CarriedBodyInput {
    CarriedBodyId id = 0;
    CarriedBodyKind kind = CarriedBodyKind::Creature;
    Vec3d position{};
    Vec3d velocity{};
    Aabb3d world_box{};
    bool jump_pressed = false;
    bool detach_requested = false;
    double delta_seconds = 0.0;
    double current_time = -1.0;
};

struct CarriedBodyOutput {
    Vec3d position{};
    Vec3d velocity{};
    Vec3d displacement{};
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    bool grounded = false;
    bool jumped = false;
    bool collided = false;
    bool hit_wall = false;
    bool hit_ceiling = false;
    std::vector<HullCollisionContact> contacts;
};

class CarriedBodyManager final {
public:
    [[nodiscard]] CarriedBodyOutput step(
        const std::vector<const DynamicConstruct *> &constructs,
        const CarriedBodyInput &input) noexcept;
    [[nodiscard]] CarriedBodyOutput step(
        const std::vector<const DynamicConstruct *> &constructs,
        const ConstructArticulationEngine *articulations,
        const CarriedBodyInput &input) noexcept;

    void remove(CarriedBodyId id) noexcept;
    void clear() noexcept;
    void prune(double current_time, double maximum_age = 5.0) noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Record {
        RiderMotionState rider{};
        ArticulatedRiderState articulated_rider{};
        Vec3d last_position{};
        double last_seen_time = 0.0;
        bool have_last_position = false;
    };

    std::unordered_map<CarriedBodyId, Record> m_records;
    double m_time = 0.0;
};

} // namespace navycraft

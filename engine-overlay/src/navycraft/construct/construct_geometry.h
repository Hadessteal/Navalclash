// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_types.h"
#include "dynamic_construct.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace navycraft {

struct Aabb3d {
    Vec3d min{};
    Vec3d max{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] Vec3d centre() const noexcept;
    [[nodiscard]] Vec3d size() const noexcept;
    [[nodiscard]] Aabb3d translated(const Vec3d &delta) const noexcept;
    [[nodiscard]] bool intersects(const Aabb3d &other) const noexcept;
    [[nodiscard]] bool contains(const Vec3d &point) const noexcept;
};

struct ConstructRaycastHit {
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    Vec3d local_point{};
    Vec3d world_point{};
    Vec3d world_normal{};
    double distance = 0.0;
};

struct ConstructSupportHit {
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    Vec3d world_point{};
    Vec3d local_anchor{};
    double vertical_gap = 0.0;
};

struct ConstructOverlapHit {
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    Vec3d world_normal{};
    double penetration_depth = 0.0;
};

struct SweepResult {
    bool collided = false;
    double allowed_fraction = 1.0;
    Vec3d allowed_delta{};
};

struct ConstructSweepHit {
    bool collided = false;
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    double allowed_fraction = 1.0;
    Vec3d allowed_delta{};
    Vec3d world_normal{};
    Vec3d world_point{};
    Vec3d surface_velocity{};
};

class ConstructGeometry final {
public:
    [[nodiscard]] static Aabb3d localNodeBox(const LocalNodePos &position) noexcept;
    [[nodiscard]] static Aabb3d worldBounds(const DynamicConstruct &construct) noexcept;
    [[nodiscard]] static bool intersectsWorldAabb(
        const DynamicConstruct &construct, const Aabb3d &world_box) noexcept;
    [[nodiscard]] static std::optional<ConstructOverlapHit> overlapWorldAabb(
        const DynamicConstruct &construct, const Aabb3d &world_box) noexcept;
    [[nodiscard]] static std::optional<ConstructSupportHit> findSupport(
        const DynamicConstruct &construct,
        const Aabb3d &world_box,
        double maximum_drop = 0.35,
        double maximum_penetration = 0.15,
        std::optional<LocalNodePos> preferred_node = std::nullopt,
        double preferred_node_bias = 0.03) noexcept;
    [[nodiscard]] static SweepResult sweepWorldAabb(
        const DynamicConstruct &construct,
        const Aabb3d &world_box,
        const Vec3d &delta,
        double maximum_step = 0.25) noexcept;
    [[nodiscard]] static ConstructSweepHit sweepWorldAabbDetailed(
        const DynamicConstruct &construct,
        const Aabb3d &world_box,
        const Vec3d &delta,
        double delta_seconds = 0.0,
        double maximum_step = 0.25) noexcept;
    [[nodiscard]] static Vec3d surfaceVelocity(
        const DynamicConstruct &construct, const Vec3d &world_point) noexcept;
    [[nodiscard]] static std::optional<ConstructRaycastHit> raycast(
        const DynamicConstruct &construct,
        const Vec3d &world_start,
        const Vec3d &world_end) noexcept;
};

} // namespace navycraft

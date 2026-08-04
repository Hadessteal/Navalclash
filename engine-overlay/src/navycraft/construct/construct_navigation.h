// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_registry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

enum class ConstructNavigationMode : std::uint8_t {
    Manual = 0,
    Hold = 1,
    Route = 2,
    Formation = 3,
};

enum class ConstructNavigationDomain : std::uint8_t {
    Surface = 0,
    Submersible = 1,
    Air = 2,
    Ground = 3,
};

struct ConstructNavigationWaypoint {
    Vec3d position{};
    double arrival_radius = 3.0;
    double target_speed = -1.0;
    bool stop = false;
};

struct ConstructNavigationObstacle {
    std::uint64_t id = 0;
    ConstructId observer_construct_id = 0;
    ConstructId construct_id = 0;
    Vec3d position{};
    Vec3d velocity{};
    double radius = 1.0;
    double sample_time = 0.0;
    double expires_at = 0.0;
    bool hard = true;
};

struct ConstructNavigationConfig {
    ConstructId construct_id = 0;
    ConstructNavigationMode mode = ConstructNavigationMode::Manual;
    ConstructNavigationDomain domain = ConstructNavigationDomain::Surface;
    double maximum_speed = 4.0;
    double maximum_reverse_speed = 1.0;
    double maximum_acceleration = 1.0;
    double maximum_deceleration = 1.5;
    double maximum_yaw_rate = 0.5;
    double maximum_yaw_acceleration = 1.0;
    double maximum_vertical_speed = 1.5;
    double arrival_radius = 3.0;
    double lookahead_distance = 16.0;
    double obstacle_margin = 2.0;
    double separation_distance = 8.0;
    double avoidance_strength = 1.8;
    double heading_gain = 2.0;
    double vertical_gain = 0.5;
    double stuck_timeout = 8.0;
    double recovery_seconds = 3.0;
    bool route_loop = false;
    bool allow_reverse = true;
    bool apply_to_construct = false;
    ConstructId formation_leader_id = 0;
    Vec3d formation_local_offset{};
};

struct ConstructNavigationCommand {
    ConstructId construct_id = 0;
    std::uint64_t sequence = 0;
    ConstructNavigationMode mode = ConstructNavigationMode::Manual;
    Vec3d target_position{};
    Vec3d desired_velocity{};
    double desired_yaw_radians = 0.0;
    double yaw_rate = 0.0;
    double forward_speed = 0.0;
    double vertical_speed = 0.0;
    double distance_to_target = 0.0;
    double cross_track_error = 0.0;
    std::size_t waypoint_index = 0;
    std::uint64_t avoidance_obstacle_id = 0;
    bool avoiding = false;
    bool arrived = false;
    bool completed = false;
    bool recovering = false;
    std::string reason;
};

struct ConstructNavigationStatus {
    ConstructNavigationConfig config{};
    std::vector<ConstructNavigationWaypoint> route;
    std::size_t waypoint_index = 0;
    Vec3d hold_position{};
    bool enabled = false;
    bool completed = false;
    double stuck_time = 0.0;
    double recovery_remaining = 0.0;
    ConstructNavigationCommand last_command{};
};

class ConstructNavigationEngine final {
public:
    explicit ConstructNavigationEngine(ConstructRegistry &registry);

    bool configure(ConstructNavigationConfig config);
    bool setRoute(ConstructId construct_id,
        std::vector<ConstructNavigationWaypoint> route, bool loop);
    bool setHoldPosition(ConstructId construct_id, const Vec3d &position);
    bool setEnabled(ConstructId construct_id, bool enabled);
    bool remove(ConstructId construct_id);
    void clear();

    bool observeObstacle(const ConstructNavigationObstacle &obstacle);
    void expireObstacles(double current_time);
    void clearObstacles(ConstructId observer_construct_id = 0);

    [[nodiscard]] std::optional<ConstructNavigationStatus> status(
        ConstructId construct_id) const;
    [[nodiscard]] std::vector<ConstructNavigationStatus> statuses() const;
    [[nodiscard]] std::vector<ConstructNavigationObstacle> obstacles(
        ConstructId observer_construct_id = 0) const;

    [[nodiscard]] std::vector<ConstructNavigationCommand> step(
        double delta_seconds, double current_time);

private:
    struct ObstacleKey {
        ConstructId observer = 0;
        std::uint64_t id = 0;
        bool operator==(const ObstacleKey &other) const noexcept
        {
            return observer == other.observer && id == other.id;
        }
    };

    struct ObstacleKeyHash {
        std::size_t operator()(const ObstacleKey &key) const noexcept;
    };

    struct RuntimeState {
        ConstructNavigationStatus status{};
        double previous_distance = 0.0;
        bool has_previous_distance = false;
        int recovery_direction = 1;
        std::uint64_t sequence = 0;
    };

    [[nodiscard]] ConstructNavigationCommand solve(
        RuntimeState &runtime, const DynamicConstruct &construct,
        double delta_seconds, double current_time);
    [[nodiscard]] std::vector<ConstructNavigationObstacle> relevantObstacles(
        const RuntimeState &runtime, const DynamicConstruct &construct,
        double current_time) const;

    ConstructRegistry &m_registry;
    std::unordered_map<ConstructId, RuntimeState> m_states;
    std::unordered_map<ObstacleKey, ConstructNavigationObstacle, ObstacleKeyHash> m_obstacles;
    std::uint64_t m_next_obstacle_id = 1;
};

} // namespace navycraft

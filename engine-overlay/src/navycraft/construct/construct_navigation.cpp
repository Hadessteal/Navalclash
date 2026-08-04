// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_navigation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace navycraft {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double EPSILON = 1e-9;

bool finiteVec(const Vec3d &value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

double dot(const Vec3d &left, const Vec3d &right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

double lengthSquared(const Vec3d &value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d &value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

double horizontalLength(const Vec3d &value) noexcept
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

Vec3d normalised(const Vec3d &value) noexcept
{
    const double magnitude = length(value);
    return magnitude > EPSILON ? value * (1.0 / magnitude) : Vec3d{};
}

Vec3d horizontalNormalised(const Vec3d &value) noexcept
{
    const double magnitude = horizontalLength(value);
    return magnitude > EPSILON ? Vec3d{value.x / magnitude, 0.0, value.z / magnitude} : Vec3d{};
}

double clamp(double value, double minimum, double maximum) noexcept
{
    return std::max(minimum, std::min(maximum, value));
}

double moveTowards(double value, double target, double maximum_delta) noexcept
{
    if (value < target)
        return std::min(target, value + maximum_delta);
    return std::max(target, value - maximum_delta);
}

double normaliseAngle(double value) noexcept
{
    value = std::fmod(value + PI, TWO_PI);
    if (value < 0.0)
        value += TWO_PI;
    return value - PI;
}

bool validateConfig(const ConstructNavigationConfig &config) noexcept
{
    return config.construct_id != 0 && std::isfinite(config.maximum_speed) &&
        config.maximum_speed >= 0.0 && config.maximum_speed <= 1000.0 &&
        std::isfinite(config.maximum_reverse_speed) &&
        config.maximum_reverse_speed >= 0.0 && config.maximum_reverse_speed <= 1000.0 &&
        std::isfinite(config.maximum_acceleration) && config.maximum_acceleration > 0.0 &&
        std::isfinite(config.maximum_deceleration) && config.maximum_deceleration > 0.0 &&
        std::isfinite(config.maximum_yaw_rate) && config.maximum_yaw_rate >= 0.0 &&
        std::isfinite(config.maximum_yaw_acceleration) &&
        config.maximum_yaw_acceleration > 0.0 &&
        std::isfinite(config.maximum_vertical_speed) &&
        config.maximum_vertical_speed >= 0.0 &&
        std::isfinite(config.arrival_radius) && config.arrival_radius > 0.0 &&
        std::isfinite(config.lookahead_distance) && config.lookahead_distance > 0.0 &&
        std::isfinite(config.obstacle_margin) && config.obstacle_margin >= 0.0 &&
        std::isfinite(config.separation_distance) && config.separation_distance >= 0.0 &&
        std::isfinite(config.avoidance_strength) && config.avoidance_strength >= 0.0 &&
        std::isfinite(config.heading_gain) && config.heading_gain > 0.0 &&
        std::isfinite(config.vertical_gain) && config.vertical_gain >= 0.0 &&
        std::isfinite(config.stuck_timeout) && config.stuck_timeout >= 0.0 &&
        std::isfinite(config.recovery_seconds) && config.recovery_seconds >= 0.0 &&
        finiteVec(config.formation_local_offset);
}

bool validateWaypoint(const ConstructNavigationWaypoint &waypoint) noexcept
{
    return finiteVec(waypoint.position) && std::isfinite(waypoint.arrival_radius) &&
        waypoint.arrival_radius > 0.0 && std::isfinite(waypoint.target_speed) &&
        waypoint.target_speed <= 1000.0;
}

double constructRadius(const DynamicConstruct &construct) noexcept
{
    const auto bounds = construct.localBounds();
    if (!bounds.valid)
        return 1.0;
    const double x = static_cast<double>(bounds.max.x - bounds.min.x + 1) * 0.5;
    const double y = static_cast<double>(bounds.max.y - bounds.min.y + 1) * 0.5;
    const double z = static_cast<double>(bounds.max.z - bounds.min.z + 1) * 0.5;
    return std::max(1.0, std::sqrt(x * x + y * y + z * z));
}

Vec3d formationVelocity(const DynamicConstruct &leader, const Vec3d &local_offset) noexcept
{
    const double yaw = leader.transform().yaw_radians;
    const Vec3d world_offset{
        std::cos(yaw) * local_offset.x - std::sin(yaw) * local_offset.z,
        local_offset.y,
        std::sin(yaw) * local_offset.x + std::cos(yaw) * local_offset.z,
    };
    const Vec3d rotational{
        leader.yawVelocity() * world_offset.z,
        0.0,
        -leader.yawVelocity() * world_offset.x,
    };
    return leader.linearVelocity() + rotational;
}

Vec3d closestPointOnSegment(const Vec3d &point, const Vec3d &start, const Vec3d &end) noexcept
{
    const Vec3d segment = end - start;
    const double denominator = lengthSquared(segment);
    if (denominator <= EPSILON)
        return start;
    const double factor = clamp(dot(point - start, segment) / denominator, 0.0, 1.0);
    return start + segment * factor;
}

} // namespace

std::size_t ConstructNavigationEngine::ObstacleKeyHash::operator()(
    const ObstacleKey &key) const noexcept
{
    std::size_t seed = std::hash<ConstructId>{}(key.observer);
    seed ^= std::hash<std::uint64_t>{}(key.id) + 0x9e3779b97f4a7c15ULL +
        (seed << 6U) + (seed >> 2U);
    return seed;
}

ConstructNavigationEngine::ConstructNavigationEngine(ConstructRegistry &registry) :
    m_registry(registry)
{
}

bool ConstructNavigationEngine::configure(ConstructNavigationConfig config)
{
    if (!validateConfig(config) || !m_registry.find(config.construct_id))
        return false;
    auto &runtime = m_states[config.construct_id];
    const bool first = runtime.status.config.construct_id == 0;
    runtime.status.config = std::move(config);
    runtime.status.enabled = runtime.status.config.mode != ConstructNavigationMode::Manual;
    runtime.status.completed = false;
    runtime.status.stuck_time = 0.0;
    runtime.status.recovery_remaining = 0.0;
    runtime.has_previous_distance = false;
    runtime.status.last_command.construct_id = runtime.status.config.construct_id;
    if (first) {
        const auto construct = m_registry.find(runtime.status.config.construct_id);
        runtime.status.hold_position = construct ? construct->transform().position : Vec3d{};
    }
    return true;
}

bool ConstructNavigationEngine::setRoute(ConstructId construct_id,
    std::vector<ConstructNavigationWaypoint> route, bool loop)
{
    auto iterator = m_states.find(construct_id);
    if (iterator == m_states.end() || route.size() > 65535)
        return false;
    for (const auto &waypoint : route) {
        if (!validateWaypoint(waypoint))
            return false;
    }
    auto &runtime = iterator->second;
    runtime.status.route = std::move(route);
    runtime.status.waypoint_index = 0;
    runtime.status.completed = runtime.status.route.empty();
    runtime.status.config.route_loop = loop;
    runtime.has_previous_distance = false;
    return true;
}

bool ConstructNavigationEngine::setHoldPosition(
    ConstructId construct_id, const Vec3d &position)
{
    if (!finiteVec(position))
        return false;
    const auto iterator = m_states.find(construct_id);
    if (iterator == m_states.end())
        return false;
    iterator->second.status.hold_position = position;
    iterator->second.status.completed = false;
    iterator->second.has_previous_distance = false;
    return true;
}

bool ConstructNavigationEngine::setEnabled(ConstructId construct_id, bool enabled)
{
    const auto iterator = m_states.find(construct_id);
    if (iterator == m_states.end())
        return false;
    iterator->second.status.enabled = enabled;
    if (!enabled)
        iterator->second.status.last_command.reason = "navigation disabled";
    return true;
}

bool ConstructNavigationEngine::remove(ConstructId construct_id)
{
    clearObstacles(construct_id);
    return m_states.erase(construct_id) != 0;
}

void ConstructNavigationEngine::clear()
{
    m_states.clear();
    m_obstacles.clear();
}

bool ConstructNavigationEngine::observeObstacle(
    const ConstructNavigationObstacle &source)
{
    if (source.observer_construct_id == 0 || !finiteVec(source.position) ||
            !finiteVec(source.velocity) || !std::isfinite(source.radius) ||
            source.radius <= 0.0 || !std::isfinite(source.sample_time) ||
            !std::isfinite(source.expires_at) || source.expires_at < source.sample_time) {
        return false;
    }
    ConstructNavigationObstacle obstacle = source;
    if (obstacle.id == 0) {
        while (m_next_obstacle_id == 0)
            ++m_next_obstacle_id;
        obstacle.id = m_next_obstacle_id++;
    }
    const ObstacleKey key{obstacle.observer_construct_id, obstacle.id};
    const auto existing = m_obstacles.find(key);
    if (existing != m_obstacles.end() &&
            obstacle.sample_time + EPSILON < existing->second.sample_time) {
        return false;
    }
    m_obstacles[key] = std::move(obstacle);
    return true;
}

void ConstructNavigationEngine::expireObstacles(double current_time)
{
    if (!std::isfinite(current_time))
        return;
    for (auto iterator = m_obstacles.begin(); iterator != m_obstacles.end();) {
        if (iterator->second.expires_at + EPSILON < current_time)
            iterator = m_obstacles.erase(iterator);
        else
            ++iterator;
    }
}

void ConstructNavigationEngine::clearObstacles(ConstructId observer_construct_id)
{
    if (observer_construct_id == 0) {
        m_obstacles.clear();
        return;
    }
    for (auto iterator = m_obstacles.begin(); iterator != m_obstacles.end();) {
        if (iterator->second.observer_construct_id == observer_construct_id ||
                iterator->second.construct_id == observer_construct_id) {
            iterator = m_obstacles.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

std::optional<ConstructNavigationStatus> ConstructNavigationEngine::status(
    ConstructId construct_id) const
{
    const auto iterator = m_states.find(construct_id);
    if (iterator == m_states.end())
        return std::nullopt;
    return iterator->second.status;
}

std::vector<ConstructNavigationStatus> ConstructNavigationEngine::statuses() const
{
    std::vector<ConstructNavigationStatus> result;
    result.reserve(m_states.size());
    for (const auto &[id, runtime] : m_states) {
        (void)id;
        result.push_back(runtime.status);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.config.construct_id < right.config.construct_id;
    });
    return result;
}

std::vector<ConstructNavigationObstacle> ConstructNavigationEngine::obstacles(
    ConstructId observer_construct_id) const
{
    std::vector<ConstructNavigationObstacle> result;
    result.reserve(m_obstacles.size());
    for (const auto &[key, obstacle] : m_obstacles) {
        (void)key;
        if (observer_construct_id == 0 ||
                obstacle.observer_construct_id == observer_construct_id) {
            result.push_back(obstacle);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.id < right.id;
    });
    return result;
}

std::vector<ConstructNavigationObstacle> ConstructNavigationEngine::relevantObstacles(
    const RuntimeState &runtime, const DynamicConstruct &construct,
    double current_time) const
{
    std::vector<ConstructNavigationObstacle> result;
    for (const auto &[key, obstacle] : m_obstacles) {
        (void)key;
        if (obstacle.observer_construct_id != construct.id() ||
                obstacle.expires_at + EPSILON < current_time)
            continue;
        auto sampled = obstacle;
        sampled.position += sampled.velocity * std::max(0.0, current_time - sampled.sample_time);
        result.push_back(std::move(sampled));
    }

    const double own_radius = constructRadius(construct);
    for (const auto &other : m_registry.snapshot()) {
        if (!other || other->id() == construct.id())
            continue;
        ConstructNavigationObstacle obstacle;
        obstacle.id = (1ULL << 63U) | other->id();
        obstacle.observer_construct_id = construct.id();
        obstacle.construct_id = other->id();
        obstacle.position = other->transform().position;
        obstacle.velocity = other->linearVelocity();
        obstacle.radius = std::max(runtime.status.config.separation_distance * 0.5,
            own_radius + constructRadius(*other));
        obstacle.sample_time = current_time;
        obstacle.expires_at = current_time;
        obstacle.hard = true;
        result.push_back(std::move(obstacle));
    }
    return result;
}

ConstructNavigationCommand ConstructNavigationEngine::solve(
    RuntimeState &runtime, const DynamicConstruct &construct,
    double delta_seconds, double current_time)
{
    ConstructNavigationCommand command;
    command.construct_id = construct.id();
    command.sequence = ++runtime.sequence;
    command.mode = runtime.status.config.mode;
    command.waypoint_index = runtime.status.waypoint_index;

    auto &status = runtime.status;
    const auto &config = status.config;
    const Vec3d position = construct.transform().position;
    const Vec3d current_velocity = construct.linearVelocity();

    if (!status.enabled || config.mode == ConstructNavigationMode::Manual) {
        command.reason = "manual control";
        return command;
    }

    Vec3d target = status.hold_position;
    Vec3d target_velocity{};
    double target_speed_override = -1.0;
    bool stop_at_target = true;

    if (config.mode == ConstructNavigationMode::Route) {
        if (status.route.empty()) {
            command.completed = true;
            command.reason = "route is empty";
            status.completed = true;
            return command;
        }
        if (status.waypoint_index >= status.route.size())
            status.waypoint_index = config.route_loop ? 0 : status.route.size() - 1;
        const auto &waypoint = status.route[status.waypoint_index];
        target = waypoint.position;
        target_speed_override = waypoint.target_speed;
        stop_at_target = waypoint.stop ||
            (!config.route_loop && status.waypoint_index + 1 >= status.route.size());
    } else if (config.mode == ConstructNavigationMode::Formation) {
        const auto leader = m_registry.find(config.formation_leader_id);
        if (!leader || leader->id() == construct.id()) {
            command.reason = "formation leader unavailable";
            return command;
        }
        target = leader->transform().localToWorld(config.formation_local_offset);
        target_velocity = formationVelocity(*leader, config.formation_local_offset);
        stop_at_target = false;
    }

    command.target_position = target;
    Vec3d to_target = target - position;
    if (config.domain == ConstructNavigationDomain::Surface ||
            config.domain == ConstructNavigationDomain::Ground) {
        to_target.y = 0.0;
    }
    double distance = length(to_target);
    command.distance_to_target = distance;

    double arrival_radius = config.arrival_radius;
    if (config.mode == ConstructNavigationMode::Route)
        arrival_radius = status.route[status.waypoint_index].arrival_radius;

    if (distance <= arrival_radius) {
        command.arrived = true;
        if (config.mode == ConstructNavigationMode::Route) {
            if (status.waypoint_index + 1 < status.route.size()) {
                ++status.waypoint_index;
                command.waypoint_index = status.waypoint_index;
                runtime.has_previous_distance = false;
                const auto &next = status.route[status.waypoint_index];
                target = next.position;
                to_target = target - position;
                if (config.domain == ConstructNavigationDomain::Surface ||
                        config.domain == ConstructNavigationDomain::Ground)
                    to_target.y = 0.0;
                distance = length(to_target);
                command.distance_to_target = distance;
                command.target_position = target;
                target_speed_override = next.target_speed;
                stop_at_target = next.stop ||
                    (!config.route_loop && status.waypoint_index + 1 >= status.route.size());
            } else if (config.route_loop) {
                status.waypoint_index = 0;
                command.waypoint_index = 0;
                runtime.has_previous_distance = false;
                const auto &next = status.route.front();
                target = next.position;
                to_target = target - position;
                if (config.domain == ConstructNavigationDomain::Surface ||
                        config.domain == ConstructNavigationDomain::Ground)
                    to_target.y = 0.0;
                distance = length(to_target);
                command.distance_to_target = distance;
                command.target_position = target;
                target_speed_override = next.target_speed;
                stop_at_target = next.stop;
            } else {
                status.completed = true;
                command.completed = true;
                command.reason = "route completed";
                return command;
            }
        } else if (config.mode == ConstructNavigationMode::Hold &&
                horizontalLength(current_velocity) < 0.05) {
            command.reason = "holding position";
            return command;
        }
    }

    const double horizontal_distance = horizontalLength(to_target);
    Vec3d desired_direction = horizontalNormalised(to_target);
    if (horizontalLength(desired_direction) <= EPSILON)
        desired_direction = {std::sin(construct.transform().yaw_radians), 0.0,
            std::cos(construct.transform().yaw_radians)};

    // Blend toward the following route segment when near a corner. This avoids
    // the original plugin's grid-aligned stop-and-turn behaviour.
    if (config.mode == ConstructNavigationMode::Route &&
            status.waypoint_index + 1 < status.route.size() &&
            distance < config.lookahead_distance) {
        Vec3d next_direction = status.route[status.waypoint_index + 1].position - target;
        next_direction.y = 0.0;
        next_direction = horizontalNormalised(next_direction);
        const double blend = clamp(1.0 - distance / config.lookahead_distance, 0.0, 1.0);
        desired_direction = horizontalNormalised(
            desired_direction * (1.0 - blend) + next_direction * blend);
    }

    const double current_speed = horizontalLength(current_velocity);
    const double horizon = std::max(1.0,
        config.lookahead_distance / std::max(0.5, current_speed + config.maximum_speed * 0.25));
    const Vec3d projected_end = position + desired_direction * config.lookahead_distance;
    Vec3d avoidance{};
    double greatest_threat = 0.0;
    std::uint64_t threat_id = 0;

    for (const auto &obstacle : relevantObstacles(runtime, construct, current_time)) {
        Vec3d relative_position = obstacle.position - position;
        if (config.domain == ConstructNavigationDomain::Surface ||
                config.domain == ConstructNavigationDomain::Ground)
            relative_position.y = 0.0;
        const Vec3d relative_velocity = current_velocity - obstacle.velocity;
        const double velocity_sq = lengthSquared(relative_velocity);
        double closest_time = 0.0;
        if (velocity_sq > EPSILON)
            closest_time = clamp(dot(relative_position, relative_velocity) / velocity_sq,
                0.0, horizon);
        Vec3d separation = relative_position - relative_velocity * closest_time;
        if (config.domain == ConstructNavigationDomain::Surface ||
                config.domain == ConstructNavigationDomain::Ground)
            separation.y = 0.0;

        const Vec3d corridor_point = closestPointOnSegment(obstacle.position,
            position, projected_end);
        Vec3d corridor_separation = corridor_point - obstacle.position;
        if (config.domain == ConstructNavigationDomain::Surface ||
                config.domain == ConstructNavigationDomain::Ground)
            corridor_separation.y = 0.0;

        const double combined_radius = obstacle.radius + config.obstacle_margin;
        const double closest_distance = std::min(length(separation), length(corridor_separation));
        if (closest_distance >= combined_radius + config.lookahead_distance * 0.25)
            continue;

        const double urgency = clamp((combined_radius + config.lookahead_distance * 0.25 -
            closest_distance) / std::max(0.25, combined_radius), 0.0, 4.0);
        Vec3d away = normalised(separation * -1.0);
        // A hazard directly ahead should cause a port/starboard decision rather
        // than a 180-degree turn. The supplied Bukkit autocraft scan did the
        // same by steering away from the occupied half of its forward volume.
        if (lengthSquared(away) <= EPSILON || dot(away, desired_direction) < -0.35) {
            const double side = desired_direction.x * relative_position.z -
                desired_direction.z * relative_position.x;
            const double sign = side >= 0.0 ? -1.0 : 1.0;
            away = {desired_direction.z * sign, 0.0, -desired_direction.x * sign};
        }
        if (config.domain == ConstructNavigationDomain::Air ||
                config.domain == ConstructNavigationDomain::Submersible) {
            away.y *= 0.35;
        } else {
            away.y = 0.0;
        }
        avoidance += away * urgency;
        if (urgency > greatest_threat) {
            greatest_threat = urgency;
            threat_id = obstacle.id;
        }
    }

    if (greatest_threat > 0.0) {
        command.avoiding = true;
        command.avoidance_obstacle_id = threat_id;
        desired_direction = horizontalNormalised(desired_direction +
            avoidance * config.avoidance_strength);
    }

    const double desired_yaw = std::atan2(desired_direction.x, desired_direction.z);
    const double yaw_error = normaliseAngle(desired_yaw - construct.transform().yaw_radians);
    double requested_yaw_rate = clamp(yaw_error * config.heading_gain,
        -config.maximum_yaw_rate, config.maximum_yaw_rate);
    requested_yaw_rate = moveTowards(construct.yawVelocity(), requested_yaw_rate,
        config.maximum_yaw_acceleration * delta_seconds);

    double desired_speed = target_speed_override >= 0.0 ?
        std::min(config.maximum_speed, target_speed_override) : config.maximum_speed;
    // Air and submersible constructs may already be horizontally aligned with a
    // waypoint while still needing to climb or dive. Do not make them drift in
    // their previous heading just because the vertical leg is unfinished.
    if ((config.domain == ConstructNavigationDomain::Air ||
            config.domain == ConstructNavigationDomain::Submersible) &&
            horizontal_distance <= arrival_radius) {
        desired_speed = 0.0;
    }
    if (stop_at_target)
        desired_speed = std::min(desired_speed,
            std::sqrt(std::max(0.0, 2.0 * config.maximum_deceleration *
                std::max(0.0, distance - arrival_radius))));
    desired_speed *= clamp(1.0 - std::abs(yaw_error) / PI * 0.75, 0.20, 1.0);
    desired_speed *= clamp(1.0 - greatest_threat * 0.20, 0.10, 1.0);

    const double acceleration = desired_speed >= current_speed ?
        config.maximum_acceleration : config.maximum_deceleration;
    double forward_speed = moveTowards(current_speed, desired_speed,
        acceleration * delta_seconds);

    const double progress_epsilon = std::max(0.05, config.maximum_speed * 0.02);
    if (runtime.has_previous_distance &&
            runtime.previous_distance - distance < progress_epsilon &&
            current_speed < std::max(0.15, config.maximum_speed * 0.08) && distance > arrival_radius) {
        status.stuck_time += delta_seconds;
    } else {
        status.stuck_time = std::max(0.0, status.stuck_time - delta_seconds * 0.5);
    }
    runtime.previous_distance = distance;
    runtime.has_previous_distance = true;

    if (status.recovery_remaining > 0.0) {
        status.recovery_remaining = std::max(0.0, status.recovery_remaining - delta_seconds);
        command.recovering = true;
        forward_speed = config.allow_reverse ? -config.maximum_reverse_speed : 0.0;
        requested_yaw_rate = runtime.recovery_direction * config.maximum_yaw_rate;
        if (status.recovery_remaining <= 0.0)
            status.stuck_time = 0.0;
    } else if (config.stuck_timeout > 0.0 &&
            status.stuck_time >= config.stuck_timeout) {
        status.recovery_remaining = config.recovery_seconds;
        runtime.recovery_direction = runtime.recovery_direction > 0 ? -1 : 1;
        command.recovering = true;
        forward_speed = config.allow_reverse ? -config.maximum_reverse_speed : 0.0;
        requested_yaw_rate = runtime.recovery_direction * config.maximum_yaw_rate;
        command.reason = "stuck recovery";
    }

    double vertical_speed = 0.0;
    if (config.domain == ConstructNavigationDomain::Air ||
            config.domain == ConstructNavigationDomain::Submersible) {
        vertical_speed = clamp((target.y - position.y) * config.vertical_gain +
            target_velocity.y, -config.maximum_vertical_speed,
            config.maximum_vertical_speed);
    }

    const Vec3d forward{
        std::sin(construct.transform().yaw_radians), 0.0,
        std::cos(construct.transform().yaw_radians)};
    Vec3d desired_velocity = forward * forward_speed;
    desired_velocity.y = vertical_speed;
    if (config.mode == ConstructNavigationMode::Formation) {
        desired_velocity += target_velocity;
        const double speed_limit = config.maximum_speed + horizontalLength(target_velocity);
        const double horizontal = horizontalLength(desired_velocity);
        if (horizontal > speed_limit && horizontal > EPSILON) {
            desired_velocity.x *= speed_limit / horizontal;
            desired_velocity.z *= speed_limit / horizontal;
        }
    }

    command.desired_yaw_radians = desired_yaw;
    command.yaw_rate = requested_yaw_rate;
    command.forward_speed = forward_speed;
    command.vertical_speed = vertical_speed;
    command.desired_velocity = desired_velocity;
    command.cross_track_error = length(avoidance);
    if (command.reason.empty())
        command.reason = command.avoiding ? "avoiding obstacle" : "navigating";
    return command;
}

std::vector<ConstructNavigationCommand> ConstructNavigationEngine::step(
    double delta_seconds, double current_time)
{
    std::vector<ConstructNavigationCommand> result;
    if (!(delta_seconds > 0.0) || !std::isfinite(delta_seconds) ||
            !std::isfinite(current_time))
        return result;
    expireObstacles(current_time);

    for (auto iterator = m_states.begin(); iterator != m_states.end();) {
        const auto construct = m_registry.find(iterator->first);
        if (!construct) {
            clearObstacles(iterator->first);
            iterator = m_states.erase(iterator);
            continue;
        }
        auto command = solve(iterator->second, *construct, delta_seconds, current_time);
        iterator->second.status.last_command = command;
        iterator->second.status.completed = command.completed;
        if (iterator->second.status.config.apply_to_construct &&
                iterator->second.status.enabled) {
            construct->setLinearVelocity(command.desired_velocity);
            construct->setYawVelocity(command.yaw_rate);
        }
        result.push_back(std::move(command));
        ++iterator;
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.construct_id < right.construct_id;
    });
    return result;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_fire_control.h"

#include "construct_geometry.h"

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

Vec3d normalised(const Vec3d &value) noexcept
{
    const double magnitude = length(value);
    return magnitude > EPSILON ? value * (1.0 / magnitude) : Vec3d{};
}

double clamp01(double value) noexcept
{
    return std::max(0.0, std::min(1.0, value));
}

double normaliseAngle(double value) noexcept
{
    value = std::fmod(value + PI, TWO_PI);
    if (value < 0.0)
        value += TWO_PI;
    return value - PI;
}

double horizontalLength(const Vec3d &value) noexcept
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

struct InterceptCandidate {
    double time = 0.0;
    Vec3d relative_launch_velocity{};
    double residual = std::numeric_limits<double>::infinity();
};

std::optional<double> solveConstantSpeedIntercept(
    const Vec3d &relative_position,
    const Vec3d &relative_velocity,
    double speed,
    double maximum_time)
{
    const double a = dot(relative_velocity, relative_velocity) - speed * speed;
    const double b = 2.0 * dot(relative_position, relative_velocity);
    const double c = dot(relative_position, relative_position);

    if (c <= EPSILON)
        return 0.0;

    if (std::abs(a) <= EPSILON) {
        if (std::abs(b) <= EPSILON)
            return std::nullopt;
        const double time = -c / b;
        if (time > EPSILON && time <= maximum_time)
            return time;
        return std::nullopt;
    }

    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
        return std::nullopt;
    const double root = std::sqrt(discriminant);
    const double first = (-b - root) / (2.0 * a);
    const double second = (-b + root) / (2.0 * a);
    double result = std::numeric_limits<double>::infinity();
    if (first > EPSILON)
        result = first;
    if (second > EPSILON)
        result = std::min(result, second);
    if (!std::isfinite(result) || result > maximum_time)
        return std::nullopt;
    return result;
}

Vec3d targetOffsetAt(const Vec3d &relative_position,
    const Vec3d &relative_velocity,
    const Vec3d &target_acceleration,
    double time) noexcept
{
    return relative_position + relative_velocity * time +
        target_acceleration * (0.5 * time * time);
}

InterceptCandidate ballisticCandidate(
    const Vec3d &relative_position,
    const Vec3d &relative_velocity,
    const Vec3d &target_acceleration,
    double gravity,
    double drag,
    double muzzle_speed,
    double time)
{
    const Vec3d gravity_displacement{0.0, -0.5 * gravity * time * time, 0.0};
    const Vec3d target_offset = targetOffsetAt(
        relative_position, relative_velocity, target_acceleration, time);
    const Vec3d required_displacement = target_offset - gravity_displacement;
    const Vec3d required_velocity = required_displacement * (1.0 / time);
    const double speed_after_drag = muzzle_speed * std::exp(-0.5 * drag * time);
    return {time, required_velocity,
        length(required_velocity) - speed_after_drag};
}

std::optional<InterceptCandidate> solveBallisticIntercept(
    const Vec3d &relative_position,
    const Vec3d &relative_velocity,
    const Vec3d &target_acceleration,
    const FireControlWeaponSpec &weapon)
{
    const double minimum_time = 0.01;
    const double maximum_time = std::max(minimum_time, weapon.maximum_lead_time);
    constexpr int SAMPLE_COUNT = 384;
    std::vector<InterceptCandidate> roots;
    InterceptCandidate previous = ballisticCandidate(relative_position,
        relative_velocity, target_acceleration, weapon.gravity, weapon.drag,
        weapon.muzzle_speed, minimum_time);
    InterceptCandidate best = previous;

    for (int index = 1; index <= SAMPLE_COUNT; ++index) {
        const double fraction = static_cast<double>(index) /
            static_cast<double>(SAMPLE_COUNT);
        const double time = minimum_time + (maximum_time - minimum_time) * fraction;
        InterceptCandidate current = ballisticCandidate(relative_position,
            relative_velocity, target_acceleration, weapon.gravity, weapon.drag,
            weapon.muzzle_speed, time);
        if (std::abs(current.residual) < std::abs(best.residual))
            best = current;
        if ((previous.residual <= 0.0 && current.residual >= 0.0) ||
                (previous.residual >= 0.0 && current.residual <= 0.0)) {
            double low = previous.time;
            double high = current.time;
            InterceptCandidate middle = current;
            for (int iteration = 0; iteration < 48; ++iteration) {
                const double mid_time = (low + high) * 0.5;
                middle = ballisticCandidate(relative_position, relative_velocity,
                    target_acceleration, weapon.gravity, weapon.drag,
                    weapon.muzzle_speed, mid_time);
                const auto low_candidate = ballisticCandidate(relative_position,
                    relative_velocity, target_acceleration, weapon.gravity,
                    weapon.drag, weapon.muzzle_speed, low);
                if ((low_candidate.residual <= 0.0 && middle.residual >= 0.0) ||
                        (low_candidate.residual >= 0.0 && middle.residual <= 0.0)) {
                    high = mid_time;
                } else {
                    low = mid_time;
                }
            }
            middle = ballisticCandidate(relative_position, relative_velocity,
                target_acceleration, weapon.gravity, weapon.drag,
                weapon.muzzle_speed, (low + high) * 0.5);
            roots.push_back(middle);
        }
        previous = current;
    }

    if (!roots.empty()) {
        return weapon.arc_preference == FireControlArcPreference::High ?
            roots.back() : roots.front();
    }

    const double tolerance = std::max(0.05, weapon.muzzle_speed * 0.01);
    if (std::abs(best.residual) <= tolerance)
        return best;
    return std::nullopt;
}

Vec3d simulateProjectileRelative(const Vec3d &launch_velocity,
    double gravity, double drag, double time)
{
    const double step_size = 0.01;
    Vec3d position{};
    Vec3d velocity = launch_velocity;
    double remaining = time;
    while (remaining > EPSILON) {
        const double step = std::min(step_size, remaining);
        velocity.y -= gravity * step;
        if (drag > 0.0)
            velocity = velocity * std::max(0.0, 1.0 - drag * step);
        position += velocity * step;
        remaining -= step;
    }
    return position;
}

bool validateWeapon(const FireControlWeaponSpec &weapon) noexcept
{
    return std::isfinite(weapon.muzzle_speed) && weapon.muzzle_speed > 0.0 &&
        weapon.muzzle_speed <= 10000.0 && std::isfinite(weapon.gravity) &&
        weapon.gravity >= -128.0 && weapon.gravity <= 128.0 &&
        std::isfinite(weapon.drag) && weapon.drag >= 0.0 && weapon.drag <= 32.0 &&
        std::isfinite(weapon.minimum_range) && weapon.minimum_range >= 0.0 &&
        std::isfinite(weapon.maximum_range) &&
        weapon.maximum_range >= weapon.minimum_range &&
        weapon.maximum_range <= 100000.0 &&
        std::isfinite(weapon.maximum_lead_time) &&
        weapon.maximum_lead_time > 0.0 && weapon.maximum_lead_time <= 600.0 &&
        std::isfinite(weapon.minimum_pitch_radians) &&
        std::isfinite(weapon.maximum_pitch_radians) &&
        weapon.minimum_pitch_radians <= weapon.maximum_pitch_radians;
}

FireControlTargetClass inferClassification(const DynamicConstruct &construct) noexcept
{
    const auto bounds = construct.localBounds();
    if (!bounds.valid)
        return FireControlTargetClass::Unknown;
    const double altitude = construct.transform().position.y;
    const double vertical_speed = std::abs(construct.linearVelocity().y);
    if (altitude > 20.0 || vertical_speed > 2.5)
        return FireControlTargetClass::Air;
    if (altitude < -1.0)
        return FireControlTargetClass::Submarine;
    return FireControlTargetClass::Surface;
}

} // namespace

std::size_t ConstructFireControlEngine::TrackKeyHash::operator()(
    const TrackKey &key) const noexcept
{
    std::size_t seed = std::hash<ConstructId>{}(key.observer);
    seed ^= std::hash<ConstructId>{}(key.target) + 0x9e3779b97f4a7c15ULL +
        (seed << 6U) + (seed >> 2U);
    return seed;
}

ConstructFireControlEngine::ConstructFireControlEngine(ConstructRegistry &registry) :
    m_registry(registry)
{
}

bool ConstructFireControlEngine::observe(const FireControlObservation &observation)
{
    if (observation.observer_construct_id == 0 ||
            observation.target_construct_id == 0 ||
            observation.observer_construct_id == observation.target_construct_id ||
            !std::isfinite(observation.sample_time) ||
            !finiteVec(observation.position) || !finiteVec(observation.velocity) ||
            !std::isfinite(observation.confidence)) {
        return false;
    }

    const TrackKey key{observation.observer_construct_id,
        observation.target_construct_id};
    const double confidence = clamp01(observation.confidence);
    const auto iterator = m_tracks.find(key);
    if (iterator == m_tracks.end()) {
        FireControlTrack track;
        track.observer_construct_id = observation.observer_construct_id;
        track.target_construct_id = observation.target_construct_id;
        track.sample_time = observation.sample_time;
        track.position = observation.position;
        track.velocity = observation.velocity;
        track.classification = observation.classification;
        track.sensor = observation.sensor;
        track.confidence = confidence;
        track.sample_count = 1;
        m_tracks.emplace(key, track);
        return true;
    }

    FireControlTrack &track = iterator->second;
    if (observation.sample_time + EPSILON < track.sample_time)
        return false;
    const double delta = observation.sample_time - track.sample_time;
    if (delta <= EPSILON) {
        track.position = observation.position;
        track.velocity = observation.velocity;
        track.classification = observation.classification;
        track.sensor = observation.sensor;
        track.confidence = std::max(track.confidence, confidence);
        ++track.sample_count;
        return true;
    }

    const Vec3d old_velocity = track.velocity;
    const Vec3d predicted = track.position + track.velocity * delta +
        track.acceleration * (0.5 * delta * delta);
    const Vec3d residual = observation.position - predicted;
    const double alpha = 0.20 + 0.75 * confidence;
    const double beta = 0.05 + 0.55 * confidence;
    track.position = predicted + residual * alpha;
    const Vec3d measured_velocity = observation.velocity;
    const Vec3d corrected_velocity = track.velocity + residual * (beta / delta);
    const double velocity_weight = 0.25 + 0.65 * confidence;
    track.velocity = corrected_velocity * (1.0 - velocity_weight) +
        measured_velocity * velocity_weight;
    track.acceleration = (track.velocity - old_velocity) * (1.0 / delta);
    track.sample_time = observation.sample_time;
    track.classification = observation.classification;
    track.sensor = observation.sensor;
    track.confidence = clamp01(track.confidence * 0.55 + confidence * 0.45);
    ++track.sample_count;
    return true;
}

bool ConstructFireControlEngine::removeTrack(
    ConstructId observer_construct_id, ConstructId target_construct_id)
{
    return m_tracks.erase({observer_construct_id, target_construct_id}) != 0;
}

void ConstructFireControlEngine::clearTracks(ConstructId observer_construct_id)
{
    if (observer_construct_id == 0) {
        m_tracks.clear();
        return;
    }
    for (auto iterator = m_tracks.begin(); iterator != m_tracks.end();) {
        if (iterator->first.observer == observer_construct_id)
            iterator = m_tracks.erase(iterator);
        else
            ++iterator;
    }
}

void ConstructFireControlEngine::removeConstruct(ConstructId construct_id)
{
    if (construct_id == 0)
        return;
    for (auto iterator = m_tracks.begin(); iterator != m_tracks.end();) {
        if (iterator->first.observer == construct_id || iterator->first.target == construct_id)
            iterator = m_tracks.erase(iterator);
        else
            ++iterator;
    }
    for (auto iterator = m_batteries.begin(); iterator != m_batteries.end();) {
        if (iterator->second.shooter_construct_id == construct_id) {
            iterator = m_batteries.erase(iterator);
            continue;
        }
        if (iterator->second.designated_target_id == construct_id)
            iterator->second.designated_target_id = 0;
        ++iterator;
    }
}

void ConstructFireControlEngine::expireTracks(
    double current_time, double maximum_age_seconds)
{
    if (!std::isfinite(current_time) || !std::isfinite(maximum_age_seconds) ||
            maximum_age_seconds < 0.0) {
        return;
    }
    for (auto iterator = m_tracks.begin(); iterator != m_tracks.end();) {
        if (current_time - iterator->second.sample_time > maximum_age_seconds)
            iterator = m_tracks.erase(iterator);
        else
            ++iterator;
    }
}

void ConstructFireControlEngine::clear()
{
    m_tracks.clear();
    m_batteries.clear();
    m_next_battery_id = 1;
}

std::optional<FireControlTrack> ConstructFireControlEngine::findTrack(
    ConstructId observer_construct_id, ConstructId target_construct_id) const
{
    const auto iterator = m_tracks.find({observer_construct_id, target_construct_id});
    if (iterator == m_tracks.end())
        return std::nullopt;
    return iterator->second;
}

std::vector<FireControlTrack> ConstructFireControlEngine::tracks(
    ConstructId observer_construct_id) const
{
    std::vector<FireControlTrack> result;
    result.reserve(m_tracks.size());
    for (const auto &[key, track] : m_tracks) {
        (void)key;
        if (observer_construct_id == 0 ||
                track.observer_construct_id == observer_construct_id) {
            result.push_back(track);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.observer_construct_id != right.observer_construct_id)
            return left.observer_construct_id < right.observer_construct_id;
        return left.target_construct_id < right.target_construct_id;
    });
    return result;
}

FireControlTrack ConstructFireControlEngine::predictedTrack(
    const FireControlTrack &track, double time) const
{
    FireControlTrack result = track;
    const double delta = std::max(0.0, time - track.sample_time);
    result.position = track.position + track.velocity * delta +
        track.acceleration * (0.5 * delta * delta);
    result.velocity = track.velocity + track.acceleration * delta;
    result.confidence = clamp01(track.confidence * std::exp(-0.08 * delta));
    result.sample_time = time;
    return result;
}

std::optional<FireControlTrack> ConstructFireControlEngine::resolveTrack(
    ConstructId observer, ConstructId target, double time) const
{
    const auto existing = findTrack(observer, target);
    if (existing)
        return predictedTrack(*existing, time);
    const auto construct = m_registry.find(target);
    if (!construct)
        return std::nullopt;
    FireControlTrack fallback;
    fallback.observer_construct_id = observer;
    fallback.target_construct_id = target;
    fallback.sample_time = time;
    fallback.position = construct->transform().position;
    fallback.velocity = construct->linearVelocity();
    fallback.classification = inferClassification(*construct);
    fallback.sensor = FireControlSensorKind::DataLink;
    fallback.confidence = 0.20;
    fallback.sample_count = 0;
    return fallback;
}

FireControlSolution ConstructFireControlEngine::solve(
    const FireControlRequest &request) const
{
    FireControlSolution solution;
    solution.shooter_construct_id = request.shooter_construct_id;
    solution.target_construct_id = request.target_construct_id;

    if (request.shooter_construct_id == 0 || request.target_construct_id == 0) {
        solution.reason = "shooter and target ids are required";
        return solution;
    }
    if (!std::isfinite(request.solution_time) || !finiteVec(request.muzzle_local_position) ||
            !validateWeapon(request.weapon)) {
        solution.reason = "invalid fire-control request";
        return solution;
    }
    const auto shooter = m_registry.find(request.shooter_construct_id);
    if (!shooter) {
        solution.reason = "shooter construct not found";
        return solution;
    }
    const auto track = resolveTrack(request.shooter_construct_id,
        request.target_construct_id, request.solution_time);
    if (!track) {
        solution.reason = "target track not found";
        return solution;
    }

    solution.track_confidence = track->confidence;
    solution.muzzle_world_position = shooter->transform().localToWorld(
        request.muzzle_local_position);
    const Vec3d shooter_velocity = ConstructGeometry::surfaceVelocity(
        *shooter, solution.muzzle_world_position);
    const Vec3d relative_position = track->position - solution.muzzle_world_position;
    const Vec3d relative_velocity = track->velocity - shooter_velocity;
    solution.range = length(relative_position);
    if (solution.range < request.weapon.minimum_range) {
        solution.reason = "target inside minimum range";
        return solution;
    }
    if (solution.range > request.weapon.maximum_range) {
        solution.reason = "target outside maximum range";
        return solution;
    }
    if (solution.range > EPSILON)
        solution.closure_rate = -dot(relative_velocity,
            relative_position * (1.0 / solution.range));

    std::optional<InterceptCandidate> candidate;
    const bool torpedo = request.weapon.projectile_kind ==
        ConstructProjectileKind::Torpedo;
    if (torpedo) {
        const Vec3d horizontal_position{relative_position.x, 0.0, relative_position.z};
        const Vec3d horizontal_velocity{relative_velocity.x, 0.0, relative_velocity.z};
        const auto intercept_time = solveConstantSpeedIntercept(horizontal_position,
            horizontal_velocity, request.weapon.muzzle_speed,
            request.weapon.maximum_lead_time);
        if (intercept_time) {
            const Vec3d horizontal_aim = horizontal_position +
                horizontal_velocity * *intercept_time;
            Vec3d direction = normalised(horizontal_aim);
            if (lengthSquared(direction) <= EPSILON)
                direction = {0.0, 0.0, 1.0};
            candidate = InterceptCandidate{*intercept_time,
                direction * request.weapon.muzzle_speed, 0.0};
        }
    } else if (std::abs(request.weapon.gravity) <= EPSILON &&
            request.weapon.drag <= EPSILON) {
        const auto intercept_time = solveConstantSpeedIntercept(relative_position,
            relative_velocity, request.weapon.muzzle_speed,
            request.weapon.maximum_lead_time);
        if (intercept_time) {
            const Vec3d target_offset = targetOffsetAt(relative_position,
                relative_velocity, track->acceleration, *intercept_time);
            candidate = InterceptCandidate{*intercept_time,
                normalised(target_offset) * request.weapon.muzzle_speed, 0.0};
        }
    } else {
        candidate = solveBallisticIntercept(relative_position, relative_velocity,
            track->acceleration, request.weapon);
    }

    if (!candidate) {
        solution.reason = "no intercept solution";
        return solution;
    }

    const Vec3d relative_launch_direction = normalised(
        candidate->relative_launch_velocity);
    if (lengthSquared(relative_launch_direction) <= EPSILON) {
        solution.reason = "degenerate intercept direction";
        return solution;
    }
    const Vec3d relative_launch_velocity = relative_launch_direction *
        request.weapon.muzzle_speed;
    solution.launch_velocity = shooter_velocity + relative_launch_velocity;
    solution.intercept_time = candidate->time;
    solution.aim_point = track->position + track->velocity * candidate->time +
        track->acceleration * (0.5 * candidate->time * candidate->time);
    if (torpedo && request.weapon.preferred_depth != 0.0)
        solution.aim_point.y = request.weapon.preferred_depth;

    solution.yaw_radians = std::atan2(relative_launch_direction.x,
        relative_launch_direction.z);
    solution.pitch_radians = std::atan2(relative_launch_direction.y,
        horizontalLength(relative_launch_direction));
    if (solution.pitch_radians < request.weapon.minimum_pitch_radians - EPSILON ||
            solution.pitch_radians > request.weapon.maximum_pitch_radians + EPSILON) {
        solution.reason = "solution outside weapon elevation limits";
        return solution;
    }

    Vec3d projectile_relative;
    if (torpedo) {
        projectile_relative = relative_launch_velocity * candidate->time;
    } else {
        projectile_relative = simulateProjectileRelative(relative_launch_velocity,
            request.weapon.gravity, request.weapon.drag, candidate->time);
    }
    const Vec3d target_relative = targetOffsetAt(relative_position, relative_velocity,
        track->acceleration, candidate->time);
    solution.estimated_miss_distance = length(projectile_relative - target_relative);
    solution.valid = true;
    solution.reason = "solution valid";
    return solution;
}

FireControlBatteryId ConstructFireControlEngine::configureBattery(
    FireControlBattery battery)
{
    if (battery.shooter_construct_id == 0 || !validateWeapon(battery.weapon) ||
            !finiteVec(battery.muzzle_local_position) ||
            !std::isfinite(battery.minimum_track_confidence) ||
            !std::isfinite(battery.reload_seconds) || battery.reload_seconds < 0.0 ||
            !std::isfinite(battery.traverse_centre_radians) ||
            !std::isfinite(battery.traverse_half_width_radians) ||
            battery.traverse_half_width_radians < 0.0) {
        throw std::invalid_argument("invalid fire-control battery");
    }
    if (!m_registry.find(battery.shooter_construct_id))
        throw std::invalid_argument("battery shooter construct not found");
    if (battery.id == 0)
        battery.id = m_next_battery_id++;
    else
        m_next_battery_id = std::max(m_next_battery_id, battery.id + 1);
    battery.minimum_track_confidence = clamp01(battery.minimum_track_confidence);
    battery.traverse_half_width_radians = std::min(PI,
        battery.traverse_half_width_radians);
    m_batteries.insert_or_assign(battery.id, battery);
    return battery.id;
}

bool ConstructFireControlEngine::removeBattery(FireControlBatteryId id)
{
    return m_batteries.erase(id) != 0;
}

void ConstructFireControlEngine::clearBatteries(ConstructId shooter_construct_id)
{
    if (shooter_construct_id == 0) {
        m_batteries.clear();
        return;
    }
    for (auto iterator = m_batteries.begin(); iterator != m_batteries.end();) {
        if (iterator->second.shooter_construct_id == shooter_construct_id)
            iterator = m_batteries.erase(iterator);
        else
            ++iterator;
    }
}

std::optional<FireControlBattery> ConstructFireControlEngine::findBattery(
    FireControlBatteryId id) const
{
    const auto iterator = m_batteries.find(id);
    if (iterator == m_batteries.end())
        return std::nullopt;
    return iterator->second;
}

std::vector<FireControlBattery> ConstructFireControlEngine::batteries(
    ConstructId shooter_construct_id) const
{
    std::vector<FireControlBattery> result;
    result.reserve(m_batteries.size());
    for (const auto &[id, battery] : m_batteries) {
        (void)id;
        if (shooter_construct_id == 0 ||
                battery.shooter_construct_id == shooter_construct_id) {
            result.push_back(battery);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.id < right.id;
    });
    return result;
}

bool ConstructFireControlEngine::targetAllowed(
    const FireControlBattery &battery, const FireControlTrack &track) const noexcept
{
    return (battery.allowed_class_mask & fireControlClassMask(track.classification)) != 0 &&
        track.confidence + EPSILON >= battery.minimum_track_confidence;
}

ConstructId ConstructFireControlEngine::selectTarget(
    const FireControlBattery &battery, double current_time) const
{
    if (battery.designated_target_id != 0) {
        const auto designated = resolveTrack(battery.shooter_construct_id,
            battery.designated_target_id, current_time);
        if (designated && targetAllowed(battery, *designated))
            return battery.designated_target_id;
    }

    const auto shooter = m_registry.find(battery.shooter_construct_id);
    if (!shooter)
        return 0;
    ConstructId best_id = 0;
    double best_score = -std::numeric_limits<double>::infinity();
    for (const auto &[key, stored_track] : m_tracks) {
        if (key.observer != battery.shooter_construct_id)
            continue;
        const FireControlTrack track = predictedTrack(stored_track, current_time);
        if (!targetAllowed(battery, track))
            continue;
        const Vec3d offset = track.position - shooter->transform().position;
        const double range = length(offset);
        if (range < battery.weapon.minimum_range ||
                range > battery.weapon.maximum_range) {
            continue;
        }
        const Vec3d relative_velocity = track.velocity - shooter->linearVelocity();
        const double closing = range > EPSILON ?
            -dot(relative_velocity, offset * (1.0 / range)) : 0.0;
        double class_weight = 1.0;
        if (track.classification == FireControlTargetClass::Air)
            class_weight = battery.mode == FireControlBatteryMode::Defensive ? 5.0 : 2.0;
        else if (track.classification == FireControlTargetClass::Submarine)
            class_weight = 2.5;
        else if (track.classification == FireControlTargetClass::Surface)
            class_weight = 2.0;
        const double score = class_weight * 1000.0 + track.confidence * 500.0 +
            closing * 20.0 - range;
        if (score > best_score) {
            best_score = score;
            best_id = track.target_construct_id;
        }
    }
    return best_id;
}

bool ConstructFireControlEngine::insideTraverseArc(
    const FireControlBattery &battery, const FireControlSolution &solution) const
{
    const auto shooter = m_registry.find(battery.shooter_construct_id);
    if (!shooter)
        return false;
    const double relative_yaw = normaliseAngle(solution.yaw_radians -
        shooter->transform().yaw_radians - battery.traverse_centre_radians);
    return std::abs(relative_yaw) <= battery.traverse_half_width_radians + EPSILON;
}

std::vector<FireControlFireOrder> ConstructFireControlEngine::stepAutomatic(
    double current_time, double maximum_track_age)
{
    std::vector<FireControlFireOrder> orders;
    if (!std::isfinite(current_time) || !std::isfinite(maximum_track_age) ||
            maximum_track_age < 0.0) {
        return orders;
    }
    expireTracks(current_time, maximum_track_age);
    std::vector<FireControlBatteryId> ids;
    ids.reserve(m_batteries.size());
    for (const auto &[id, battery] : m_batteries) {
        (void)battery;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    for (const auto id : ids) {
        auto iterator = m_batteries.find(id);
        if (iterator == m_batteries.end())
            continue;
        FireControlBattery &battery = iterator->second;
        if (!battery.enabled || battery.mode == FireControlBatteryMode::Manual ||
                current_time + EPSILON < battery.next_ready_time) {
            continue;
        }
        const ConstructId target_id = selectTarget(battery, current_time);
        if (target_id == 0)
            continue;
        FireControlRequest request;
        request.shooter_construct_id = battery.shooter_construct_id;
        request.target_construct_id = target_id;
        request.muzzle_local_position = battery.muzzle_local_position;
        request.weapon = battery.weapon;
        request.solution_time = current_time;
        FireControlSolution solution = solve(request);
        if (!solution.valid || !insideTraverseArc(battery, solution))
            continue;
        orders.push_back({battery.id, battery.shooter_construct_id,
            target_id, solution});
        battery.next_ready_time = current_time + battery.reload_seconds;
    }
    return orders;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_projectiles.h"
#include "construct_registry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

using FireControlBatteryId = std::uint64_t;

enum class FireControlTargetClass : std::uint8_t {
    Unknown = 0,
    Surface = 1,
    Submarine = 2,
    Air = 3,
    Ground = 4,
};

enum class FireControlSensorKind : std::uint8_t {
    Visual = 0,
    Radar = 1,
    PassiveSonar = 2,
    ActiveSonar = 3,
    DataLink = 4,
};

enum class FireControlArcPreference : std::uint8_t {
    Low = 0,
    High = 1,
};

enum class FireControlBatteryMode : std::uint8_t {
    Manual = 0,
    Automatic = 1,
    Defensive = 2,
};

constexpr std::uint32_t fireControlClassMask(FireControlTargetClass value) noexcept
{
    return 1U << static_cast<std::uint8_t>(value);
}

struct FireControlObservation {
    ConstructId observer_construct_id = 0;
    ConstructId target_construct_id = 0;
    double sample_time = 0.0;
    Vec3d position{};
    Vec3d velocity{};
    FireControlTargetClass classification = FireControlTargetClass::Unknown;
    FireControlSensorKind sensor = FireControlSensorKind::Visual;
    double confidence = 1.0;
};

struct FireControlTrack {
    ConstructId observer_construct_id = 0;
    ConstructId target_construct_id = 0;
    double sample_time = 0.0;
    Vec3d position{};
    Vec3d velocity{};
    Vec3d acceleration{};
    FireControlTargetClass classification = FireControlTargetClass::Unknown;
    FireControlSensorKind sensor = FireControlSensorKind::Visual;
    double confidence = 0.0;
    std::size_t sample_count = 0;
};

struct FireControlWeaponSpec {
    ConstructProjectileKind projectile_kind = ConstructProjectileKind::Shell;
    double muzzle_speed = 50.0;
    double gravity = 0.0;
    double drag = 0.0;
    double minimum_range = 0.0;
    double maximum_range = 500.0;
    double maximum_lead_time = 30.0;
    double minimum_pitch_radians = -1.5707963267948966;
    double maximum_pitch_radians = 1.5707963267948966;
    double preferred_depth = 0.0;
    double guidance_turn_rate = 0.0;
    bool guided = false;
    FireControlArcPreference arc_preference = FireControlArcPreference::Low;
};

struct FireControlRequest {
    ConstructId shooter_construct_id = 0;
    ConstructId target_construct_id = 0;
    Vec3d muzzle_local_position{};
    FireControlWeaponSpec weapon{};
    double solution_time = 0.0;
};

struct FireControlSolution {
    bool valid = false;
    ConstructId shooter_construct_id = 0;
    ConstructId target_construct_id = 0;
    Vec3d muzzle_world_position{};
    Vec3d aim_point{};
    Vec3d launch_velocity{};
    double yaw_radians = 0.0;
    double pitch_radians = 0.0;
    double intercept_time = 0.0;
    double range = 0.0;
    double closure_rate = 0.0;
    double estimated_miss_distance = 0.0;
    double track_confidence = 0.0;
    std::string reason;
};

struct FireControlBattery {
    FireControlBatteryId id = 0;
    ConstructId shooter_construct_id = 0;
    Vec3d muzzle_local_position{};
    FireControlWeaponSpec weapon{};
    FireControlBatteryMode mode = FireControlBatteryMode::Manual;
    std::uint32_t allowed_class_mask = 0xFFFFFFFFU;
    ConstructId designated_target_id = 0;
    double minimum_track_confidence = 0.25;
    double reload_seconds = 1.0;
    double next_ready_time = 0.0;
    double traverse_centre_radians = 0.0;
    double traverse_half_width_radians = 3.14159265358979323846;
    bool enabled = true;
};

struct FireControlFireOrder {
    FireControlBatteryId battery_id = 0;
    ConstructId shooter_construct_id = 0;
    ConstructId target_construct_id = 0;
    FireControlSolution solution{};
};

class ConstructFireControlEngine final {
public:
    explicit ConstructFireControlEngine(ConstructRegistry &registry);

    bool observe(const FireControlObservation &observation);
    bool removeTrack(ConstructId observer_construct_id, ConstructId target_construct_id);
    void clearTracks(ConstructId observer_construct_id = 0);
    void removeConstruct(ConstructId construct_id);
    void expireTracks(double current_time, double maximum_age_seconds);

    [[nodiscard]] std::optional<FireControlTrack> findTrack(
        ConstructId observer_construct_id, ConstructId target_construct_id) const;
    [[nodiscard]] std::vector<FireControlTrack> tracks(
        ConstructId observer_construct_id = 0) const;

    [[nodiscard]] FireControlSolution solve(const FireControlRequest &request) const;

    FireControlBatteryId configureBattery(FireControlBattery battery);
    bool removeBattery(FireControlBatteryId id);
    void clearBatteries(ConstructId shooter_construct_id = 0);
    [[nodiscard]] std::optional<FireControlBattery> findBattery(
        FireControlBatteryId id) const;
    [[nodiscard]] std::vector<FireControlBattery> batteries(
        ConstructId shooter_construct_id = 0) const;

    [[nodiscard]] std::vector<FireControlFireOrder> stepAutomatic(
        double current_time, double maximum_track_age = 10.0);

private:
    struct TrackKey {
        ConstructId observer = 0;
        ConstructId target = 0;
        bool operator==(const TrackKey &other) const noexcept
        {
            return observer == other.observer && target == other.target;
        }
    };

    struct TrackKeyHash {
        std::size_t operator()(const TrackKey &key) const noexcept;
    };

    [[nodiscard]] FireControlTrack predictedTrack(
        const FireControlTrack &track, double time) const;
    [[nodiscard]] std::optional<FireControlTrack> resolveTrack(
        ConstructId observer, ConstructId target, double time) const;
    [[nodiscard]] ConstructId selectTarget(
        const FireControlBattery &battery, double current_time) const;
    [[nodiscard]] bool targetAllowed(const FireControlBattery &battery,
        const FireControlTrack &track) const noexcept;
    [[nodiscard]] bool insideTraverseArc(const FireControlBattery &battery,
        const FireControlSolution &solution) const;

    ConstructRegistry &m_registry;
    std::unordered_map<TrackKey, FireControlTrack, TrackKeyHash> m_tracks;
    std::unordered_map<FireControlBatteryId, FireControlBattery> m_batteries;
    FireControlBatteryId m_next_battery_id = 1;
};

} // namespace navycraft

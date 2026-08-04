// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_geometry.h"
#include "construct_registry.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

using ProjectileId = std::uint64_t;

enum class ConstructProjectileKind : std::uint8_t {
    Shell = 0,
    Fireball = 1,
    Torpedo = 2,
    DepthCharge = 3,
    Bomb = 4,
    AntiAircraft = 5,
};

enum class ConstructProjectileEventKind : std::uint8_t {
    Spawn = 0,
    Update = 1,
    Impact = 2,
    Remove = 3,
};

enum class ConstructImpactKind : std::uint8_t {
    None = 0,
    Construct = 1,
    Terrain = 2,
    Water = 3,
    Expired = 4,
};

struct ConstructProjectileSpec {
    ConstructProjectileKind kind = ConstructProjectileKind::Shell;
    double radius = 0.08;
    double gravity = 0.0;
    double drag = 0.0;
    double arming_time = 0.1;
    double maximum_age = 30.0;
    double maximum_range = 512.0;
    double blast_radius = 2.0;
    double blast_power = 8.0;
    double penetration = 1.0;
    double guidance_turn_rate = 0.0;
    double preferred_depth = 0.0;
    bool guided = false;
    bool detonate_on_expiry = false;
    bool requires_water = false;
};

struct ConstructProjectileState {
    ProjectileId id = 0;
    std::uint64_t sequence = 0;
    ConstructProjectileSpec spec{};
    ConstructId source_construct_id = 0;
    ConstructId target_construct_id = 0;
    std::string owner;
    Vec3d position{};
    Vec3d previous_position{};
    Vec3d velocity{};
    double age = 0.0;
    double distance_travelled = 0.0;
    bool armed = false;
};

struct ConstructNodeDamage {
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    std::string node_name;
    double distance = 0.0;
    double effective_power = 0.0;
    double armour = 1.0;
    bool destroyed = false;
    bool breached = false;
};

struct ConstructExplosionResult {
    Vec3d world_position{};
    ConstructId source_construct_id = 0;
    ConstructId directly_hit_construct_id = 0;
    std::vector<ConstructNodeDamage> node_damage;
    std::vector<ConstructId> affected_constructs;
    std::size_t destroyed_nodes = 0;
    std::size_t breaches = 0;
};

struct ConstructProjectileImpact {
    ProjectileId projectile_id = 0;
    ConstructImpactKind kind = ConstructImpactKind::None;
    Vec3d world_position{};
    Vec3d world_normal{};
    ConstructId construct_id = 0;
    LocalNodePos node_position{};
    ConstructExplosionResult explosion{};
};

struct ConstructProjectileEvent {
    ConstructProjectileEventKind kind = ConstructProjectileEventKind::Update;
    ConstructProjectileState projectile{};
    std::optional<ConstructProjectileImpact> impact;
};

struct ConstructWorldProjectileHit {
    ConstructImpactKind kind = ConstructImpactKind::Terrain;
    Vec3d position{};
    Vec3d normal{};
};

class ConstructProjectilePacketCodec final {
public:
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t MAX_STRING_BYTES = 4096;
    static constexpr std::size_t MAX_PACKET_BYTES = 64U * 1024U;

    [[nodiscard]] static std::vector<std::uint8_t> encode(
        const ConstructProjectileEvent &event);
    [[nodiscard]] static ConstructProjectileEvent decode(
        const std::vector<std::uint8_t> &bytes);
};


class ConstructProjectileClientState final {
public:
    bool apply(const ConstructProjectileEvent &event);
    void clear();

    [[nodiscard]] std::optional<ConstructProjectileState> find(ProjectileId id) const;
    [[nodiscard]] std::vector<ConstructProjectileState> activeProjectiles() const;
    [[nodiscard]] std::vector<ConstructProjectileImpact> drainImpacts();
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::unordered_map<ProjectileId, ConstructProjectileState> m_active;
    std::unordered_map<ProjectileId, std::uint64_t> m_last_sequences;
    std::vector<ConstructProjectileImpact> m_impacts;
};

class ConstructProjectileEngine final {
public:
    using WorldRaycast = std::function<std::optional<ConstructWorldProjectileHit>(
        const Vec3d &, const Vec3d &, double)>;
    using ArmourResolver = std::function<double(const ConstructNode &)>;

    explicit ConstructProjectileEngine(ConstructRegistry &registry);

    ProjectileId spawn(ConstructProjectileState projectile);
    bool remove(ProjectileId id);
    void clear();

    void setWorldRaycast(WorldRaycast callback);
    void setArmourResolver(ArmourResolver callback);

    [[nodiscard]] std::vector<ConstructProjectileEvent> step(double delta_seconds);
    [[nodiscard]] std::vector<ConstructProjectileState> snapshot() const;
    [[nodiscard]] std::optional<ConstructProjectileState> find(ProjectileId id) const;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] ConstructExplosionResult explode(const Vec3d &world_position,
        const ConstructProjectileState &projectile,
        ConstructId directly_hit_construct_id = 0);

private:
    [[nodiscard]] std::optional<ConstructProjectileImpact> findImpact(
        const ConstructProjectileState &projectile,
        const Vec3d &start,
        const Vec3d &end) const;
    void guideProjectile(ConstructProjectileState &projectile, double delta_seconds) const;
    [[nodiscard]] double armourFor(const ConstructNode &node) const;

    ConstructRegistry &m_registry;
    std::unordered_map<ProjectileId, ConstructProjectileState> m_projectiles;
    ProjectileId m_next_id = 1;
    WorldRaycast m_world_raycast;
    ArmourResolver m_armour_resolver;
};

} // namespace navycraft

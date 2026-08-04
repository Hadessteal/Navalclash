// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_projectiles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> MAGIC{{'N', 'C', 'P', 'J'}};
constexpr double EPSILON = 1e-9;

class Writer final {
public:
    template<typename T>
    void integer(T value)
    {
        static_assert(std::is_integral_v<T>);
        using Unsigned = std::make_unsigned_t<T>;
        const Unsigned raw = static_cast<Unsigned>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes.push_back(static_cast<std::uint8_t>(raw >> (i * 8U)));
    }

    void real(double value)
    {
        if (!std::isfinite(value))
            throw std::invalid_argument("projectile packet contains non-finite number");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    }

    void string(const std::string &value)
    {
        if (value.size() > ConstructProjectilePacketCodec::MAX_STRING_BYTES)
            throw std::length_error("projectile string exceeds safety limit");
        integer(static_cast<std::uint16_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    std::vector<std::uint8_t> finish()
    {
        if (bytes.size() > ConstructProjectilePacketCodec::MAX_PACKET_BYTES)
            throw std::length_error("projectile packet exceeds safety limit");
        return std::move(bytes);
    }

    std::vector<std::uint8_t> bytes;
};

class Reader final {
public:
    explicit Reader(const std::vector<std::uint8_t> &input) : bytes(input) {}

    template<typename T>
    T integer()
    {
        static_assert(std::is_integral_v<T>);
        require(sizeof(T));
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned result = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
            result |= static_cast<Unsigned>(bytes[offset++]) << (i * 8U);
        return static_cast<T>(result);
    }

    double real()
    {
        const auto bits = integer<std::uint64_t>();
        double result = 0.0;
        std::memcpy(&result, &bits, sizeof(result));
        if (!std::isfinite(result))
            throw std::runtime_error("projectile packet contains non-finite number");
        return result;
    }

    std::string string()
    {
        const auto count = integer<std::uint16_t>();
        if (count > ConstructProjectilePacketCodec::MAX_STRING_BYTES)
            throw std::runtime_error("projectile string exceeds safety limit");
        require(count);
        std::string result(reinterpret_cast<const char *>(bytes.data() + offset), count);
        offset += count;
        return result;
    }

    void magic()
    {
        require(MAGIC.size());
        for (const auto byte : MAGIC) {
            if (bytes[offset++] != byte)
                throw std::runtime_error("invalid projectile packet magic");
        }
    }

    void end() const
    {
        if (offset != bytes.size())
            throw std::runtime_error("trailing projectile packet bytes");
    }

private:
    void require(std::size_t count) const
    {
        if (offset > bytes.size() || count > bytes.size() - offset)
            throw std::runtime_error("truncated projectile packet");
    }

    const std::vector<std::uint8_t> &bytes;
    std::size_t offset = 0;
};

void writeVec3(Writer &writer, const Vec3d &value)
{
    writer.real(value.x);
    writer.real(value.y);
    writer.real(value.z);
}

Vec3d readVec3(Reader &reader)
{
    return {reader.real(), reader.real(), reader.real()};
}

double dot(const Vec3d &a, const Vec3d &b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vec3d &value) noexcept
{
    return std::sqrt(dot(value, value));
}

Vec3d normalised(const Vec3d &value) noexcept
{
    const double magnitude = length(value);
    return magnitude <= EPSILON ? Vec3d{} : value * (1.0 / magnitude);
}

Vec3d lerp(const Vec3d &a, const Vec3d &b, double factor) noexcept
{
    return a * (1.0 - factor) + b * factor;
}

void validateSpec(const ConstructProjectileSpec &spec)
{
    if (spec.radius < 0.0 || spec.radius > 8.0 || spec.gravity < -128.0 || spec.gravity > 128.0 ||
            spec.drag < 0.0 || spec.drag > 32.0 || spec.arming_time < 0.0 ||
            spec.maximum_age <= 0.0 || spec.maximum_age > 3600.0 ||
            spec.maximum_range <= 0.0 || spec.maximum_range > 100000.0 ||
            spec.blast_radius < 0.0 || spec.blast_radius > 128.0 ||
            spec.blast_power < 0.0 || spec.blast_power > 100000.0 ||
            spec.penetration < 0.0 || spec.penetration > 10000.0 ||
            spec.guidance_turn_rate < 0.0 || spec.guidance_turn_rate > 100.0)
        throw std::invalid_argument("projectile specification is outside allowed range");
}

void validateState(const ConstructProjectileState &state)
{
    if (state.id == 0)
        throw std::invalid_argument("projectile id 0 is reserved");
    validateSpec(state.spec);
    if (!std::isfinite(state.age) || !std::isfinite(state.distance_travelled) ||
            state.age < 0.0 || state.distance_travelled < 0.0)
        throw std::invalid_argument("projectile state contains invalid age or distance");
}

void writeSpec(Writer &writer, const ConstructProjectileSpec &spec)
{
    writer.integer(static_cast<std::uint8_t>(spec.kind));
    writer.integer<std::uint8_t>(spec.guided ? 1U : 0U);
    writer.integer<std::uint8_t>(spec.detonate_on_expiry ? 1U : 0U);
    writer.integer<std::uint8_t>(spec.requires_water ? 1U : 0U);
    writer.real(spec.radius);
    writer.real(spec.gravity);
    writer.real(spec.drag);
    writer.real(spec.arming_time);
    writer.real(spec.maximum_age);
    writer.real(spec.maximum_range);
    writer.real(spec.blast_radius);
    writer.real(spec.blast_power);
    writer.real(spec.penetration);
    writer.real(spec.guidance_turn_rate);
    writer.real(spec.preferred_depth);
}

ConstructProjectileSpec readSpec(Reader &reader)
{
    ConstructProjectileSpec spec;
    const auto kind = reader.integer<std::uint8_t>();
    if (kind > static_cast<std::uint8_t>(ConstructProjectileKind::AntiAircraft))
        throw std::runtime_error("invalid projectile kind");
    spec.kind = static_cast<ConstructProjectileKind>(kind);
    spec.guided = reader.integer<std::uint8_t>() != 0;
    spec.detonate_on_expiry = reader.integer<std::uint8_t>() != 0;
    spec.requires_water = reader.integer<std::uint8_t>() != 0;
    spec.radius = reader.real();
    spec.gravity = reader.real();
    spec.drag = reader.real();
    spec.arming_time = reader.real();
    spec.maximum_age = reader.real();
    spec.maximum_range = reader.real();
    spec.blast_radius = reader.real();
    spec.blast_power = reader.real();
    spec.penetration = reader.real();
    spec.guidance_turn_rate = reader.real();
    spec.preferred_depth = reader.real();
    validateSpec(spec);
    return spec;
}

void writeState(Writer &writer, const ConstructProjectileState &state)
{
    validateState(state);
    writer.integer(state.id);
    writer.integer(state.sequence);
    writeSpec(writer, state.spec);
    writer.integer(state.source_construct_id);
    writer.integer(state.target_construct_id);
    writer.string(state.owner);
    writeVec3(writer, state.position);
    writeVec3(writer, state.previous_position);
    writeVec3(writer, state.velocity);
    writer.real(state.age);
    writer.real(state.distance_travelled);
    writer.integer<std::uint8_t>(state.armed ? 1U : 0U);
}

ConstructProjectileState readState(Reader &reader)
{
    ConstructProjectileState state;
    state.id = reader.integer<ProjectileId>();
    state.sequence = reader.integer<std::uint64_t>();
    state.spec = readSpec(reader);
    state.source_construct_id = reader.integer<ConstructId>();
    state.target_construct_id = reader.integer<ConstructId>();
    state.owner = reader.string();
    state.position = readVec3(reader);
    state.previous_position = readVec3(reader);
    state.velocity = readVec3(reader);
    state.age = reader.real();
    state.distance_travelled = reader.real();
    state.armed = reader.integer<std::uint8_t>() != 0;
    validateState(state);
    return state;
}

bool nodeInsideRadius(const DynamicConstruct &construct, const LocalNodePos &position,
    const Vec3d &world_position, double radius, double &distance)
{
    const Vec3d centre = construct.transform().localToWorld({position.x + 0.5,
        position.y + 0.5, position.z + 0.5});
    distance = length(centre - world_position);
    return distance <= radius + 0.8660254037844386;
}
}

std::vector<std::uint8_t> ConstructProjectilePacketCodec::encode(
    const ConstructProjectileEvent &event)
{
    validateState(event.projectile);
    Writer writer;
    writer.bytes.insert(writer.bytes.end(), MAGIC.begin(), MAGIC.end());
    writer.integer(VERSION);
    writer.integer(static_cast<std::uint8_t>(event.kind));
    writer.integer<std::uint8_t>(event.impact.has_value() ? 1U : 0U);
    writeState(writer, event.projectile);
    if (event.impact) {
        const auto &impact = *event.impact;
        writer.integer(static_cast<std::uint8_t>(impact.kind));
        writeVec3(writer, impact.world_position);
        writeVec3(writer, impact.world_normal);
        writer.integer(impact.construct_id);
        writer.integer(impact.node_position.x);
        writer.integer(impact.node_position.y);
        writer.integer(impact.node_position.z);
        writer.integer(static_cast<std::uint32_t>(impact.explosion.destroyed_nodes));
        writer.integer(static_cast<std::uint32_t>(impact.explosion.breaches));
    }
    return writer.finish();
}

ConstructProjectileEvent ConstructProjectilePacketCodec::decode(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("projectile packet exceeds safety limit");
    Reader reader(bytes);
    reader.magic();
    if (reader.integer<std::uint16_t>() != VERSION)
        throw std::runtime_error("unsupported projectile packet version");
    const auto kind = reader.integer<std::uint8_t>();
    if (kind > static_cast<std::uint8_t>(ConstructProjectileEventKind::Remove))
        throw std::runtime_error("invalid projectile event kind");
    const bool has_impact = reader.integer<std::uint8_t>() != 0;
    ConstructProjectileEvent event;
    event.kind = static_cast<ConstructProjectileEventKind>(kind);
    event.projectile = readState(reader);
    if (has_impact) {
        ConstructProjectileImpact impact;
        impact.projectile_id = event.projectile.id;
        const auto impact_kind = reader.integer<std::uint8_t>();
        if (impact_kind > static_cast<std::uint8_t>(ConstructImpactKind::Expired))
            throw std::runtime_error("invalid projectile impact kind");
        impact.kind = static_cast<ConstructImpactKind>(impact_kind);
        impact.world_position = readVec3(reader);
        impact.world_normal = readVec3(reader);
        impact.construct_id = reader.integer<ConstructId>();
        impact.node_position.x = reader.integer<std::int32_t>();
        impact.node_position.y = reader.integer<std::int32_t>();
        impact.node_position.z = reader.integer<std::int32_t>();
        impact.explosion.destroyed_nodes = reader.integer<std::uint32_t>();
        impact.explosion.breaches = reader.integer<std::uint32_t>();
        event.impact = impact;
    }
    reader.end();
    return event;
}

bool ConstructProjectileClientState::apply(const ConstructProjectileEvent &event)
{
    validateState(event.projectile);
    auto &last = m_last_sequences[event.projectile.id];
    if (event.projectile.sequence <= last)
        return false;
    last = event.projectile.sequence;
    switch (event.kind) {
    case ConstructProjectileEventKind::Spawn:
    case ConstructProjectileEventKind::Update:
        m_active[event.projectile.id] = event.projectile;
        break;
    case ConstructProjectileEventKind::Impact:
        m_active.erase(event.projectile.id);
        if (event.impact)
            m_impacts.push_back(*event.impact);
        break;
    case ConstructProjectileEventKind::Remove:
        m_active.erase(event.projectile.id);
        break;
    }
    return true;
}

void ConstructProjectileClientState::clear()
{
    m_active.clear();
    m_last_sequences.clear();
    m_impacts.clear();
}

std::optional<ConstructProjectileState> ConstructProjectileClientState::find(
    ProjectileId id) const
{
    const auto iterator = m_active.find(id);
    if (iterator == m_active.end())
        return std::nullopt;
    return iterator->second;
}

std::vector<ConstructProjectileState> ConstructProjectileClientState::activeProjectiles() const
{
    std::vector<ConstructProjectileState> result;
    result.reserve(m_active.size());
    for (const auto &[id, projectile] : m_active) {
        (void)id;
        result.push_back(projectile);
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.id < b.id;
    });
    return result;
}

std::vector<ConstructProjectileImpact> ConstructProjectileClientState::drainImpacts()
{
    std::vector<ConstructProjectileImpact> result;
    result.swap(m_impacts);
    return result;
}

std::size_t ConstructProjectileClientState::size() const noexcept
{
    return m_active.size();
}

ConstructProjectileEngine::ConstructProjectileEngine(ConstructRegistry &registry) :
    m_registry(registry)
{
}

ProjectileId ConstructProjectileEngine::spawn(ConstructProjectileState projectile)
{
    validateSpec(projectile.spec);
    if (projectile.id == 0) {
        while (m_next_id == 0 || m_projectiles.find(m_next_id) != m_projectiles.end())
            ++m_next_id;
        projectile.id = m_next_id++;
    } else if (m_projectiles.find(projectile.id) != m_projectiles.end()) {
        throw std::invalid_argument("projectile id already exists");
    }
    projectile.sequence = std::max<std::uint64_t>(projectile.sequence, 1);
    projectile.previous_position = projectile.position;
    validateState(projectile);
    m_projectiles.emplace(projectile.id, projectile);
    return projectile.id;
}

bool ConstructProjectileEngine::remove(ProjectileId id)
{
    return m_projectiles.erase(id) != 0;
}

void ConstructProjectileEngine::clear()
{
    m_projectiles.clear();
}

void ConstructProjectileEngine::setWorldRaycast(WorldRaycast callback)
{
    m_world_raycast = std::move(callback);
}

void ConstructProjectileEngine::setArmourResolver(ArmourResolver callback)
{
    m_armour_resolver = std::move(callback);
}

void ConstructProjectileEngine::guideProjectile(
    ConstructProjectileState &projectile, double delta_seconds) const
{
    if (!projectile.spec.guided || projectile.target_construct_id == 0 ||
            projectile.spec.guidance_turn_rate <= 0.0)
        return;
    const auto target = m_registry.find(projectile.target_construct_id);
    if (!target)
        return;
    Vec3d target_position = ConstructGeometry::worldBounds(*target).centre();
    if (projectile.spec.kind == ConstructProjectileKind::Torpedo)
        target_position.y = projectile.spec.preferred_depth;
    const Vec3d desired = normalised(target_position - projectile.position);
    const double speed = length(projectile.velocity);
    if (speed <= EPSILON || length(desired) <= EPSILON)
        return;
    const Vec3d current = normalised(projectile.velocity);
    const double factor = std::clamp(projectile.spec.guidance_turn_rate * delta_seconds, 0.0, 1.0);
    projectile.velocity = normalised(lerp(current, desired, factor)) * speed;
}

std::optional<ConstructProjectileImpact> ConstructProjectileEngine::findImpact(
    const ConstructProjectileState &projectile, const Vec3d &start, const Vec3d &end) const
{
    std::optional<ConstructProjectileImpact> best;
    double best_distance = std::numeric_limits<double>::infinity();
    const Vec3d delta = end - start;
    const double travel_distance = length(delta);
    for (const auto &construct : m_registry.snapshot()) {
        if (!construct || construct->id() == projectile.source_construct_id)
            continue;

        ConstructProjectileImpact impact;
        double hit_distance = std::numeric_limits<double>::infinity();
        bool collided = false;
        if (projectile.spec.radius > EPSILON) {
            const Vec3d extent{projectile.spec.radius, projectile.spec.radius,
                projectile.spec.radius};
            const Aabb3d projectile_box{start - extent, start + extent};
            const auto sweep = ConstructGeometry::sweepWorldAabbDetailed(
                *construct, projectile_box, delta, 0.0,
                std::clamp(projectile.spec.radius * 0.5, 0.02, 0.25));
            if (sweep.collided) {
                collided = true;
                hit_distance = travel_distance * sweep.allowed_fraction;
                impact.world_position = sweep.world_point;
                impact.world_normal = sweep.world_normal;
                impact.construct_id = sweep.construct_id;
                impact.node_position = sweep.node_position;
            }
        } else {
            const auto ray_hit = ConstructGeometry::raycast(*construct, start, end);
            if (ray_hit) {
                collided = true;
                hit_distance = ray_hit->distance;
                impact.world_position = ray_hit->world_point;
                impact.world_normal = ray_hit->world_normal;
                impact.construct_id = ray_hit->construct_id;
                impact.node_position = ray_hit->node_position;
            }
        }
        if (!collided || hit_distance >= best_distance)
            continue;
        best_distance = hit_distance;
        impact.projectile_id = projectile.id;
        impact.kind = ConstructImpactKind::Construct;
        best = impact;
    }

    if (m_world_raycast) {
        const auto world_hit = m_world_raycast(start, end, projectile.spec.radius);
        if (world_hit) {
            const double distance = length(world_hit->position - start);
            if (distance < best_distance) {
                ConstructProjectileImpact impact;
                impact.projectile_id = projectile.id;
                impact.kind = world_hit->kind;
                impact.world_position = world_hit->position;
                impact.world_normal = world_hit->normal;
                best = impact;
            }
        }
    }
    return best;
}

std::vector<ConstructProjectileEvent> ConstructProjectileEngine::step(double delta_seconds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0 || delta_seconds > 10.0)
        throw std::invalid_argument("projectile step delta is outside allowed range");
    std::vector<ConstructProjectileEvent> events;
    std::vector<ProjectileId> remove_ids;
    for (auto &[id, projectile] : m_projectiles) {
        (void)id;
        projectile.previous_position = projectile.position;
        guideProjectile(projectile, delta_seconds);

        if (projectile.spec.kind == ConstructProjectileKind::Torpedo) {
            const double correction = std::clamp(
                (projectile.spec.preferred_depth - projectile.position.y) * 1.5, -4.0, 4.0);
            projectile.velocity.y = correction;
        } else {
            projectile.velocity.y -= projectile.spec.gravity * delta_seconds;
        }
        if (projectile.spec.drag > 0.0) {
            const double drag_factor = std::max(0.0, 1.0 - projectile.spec.drag * delta_seconds);
            projectile.velocity = projectile.velocity * drag_factor;
        }

        const Vec3d end = projectile.position + projectile.velocity * delta_seconds;
        projectile.age += delta_seconds;
        projectile.distance_travelled += length(end - projectile.position);
        projectile.armed = projectile.age >= projectile.spec.arming_time;

        auto impact = projectile.armed ? findImpact(projectile, projectile.position, end) :
            std::optional<ConstructProjectileImpact>{};
        projectile.position = impact ? impact->world_position : end;
        ++projectile.sequence;

        const bool expired = projectile.age >= projectile.spec.maximum_age ||
            projectile.distance_travelled >= projectile.spec.maximum_range;
        if (!impact && expired) {
            ConstructProjectileImpact expiry;
            expiry.projectile_id = projectile.id;
            expiry.kind = ConstructImpactKind::Expired;
            expiry.world_position = projectile.position;
            impact = expiry;
        }

        if (impact) {
            if (impact->kind != ConstructImpactKind::Expired || projectile.spec.detonate_on_expiry)
                impact->explosion = explode(impact->world_position, projectile, impact->construct_id);
            events.push_back({ConstructProjectileEventKind::Impact, projectile, impact});
            events.push_back({ConstructProjectileEventKind::Remove, projectile, impact});
            remove_ids.push_back(projectile.id);
        } else {
            events.push_back({ConstructProjectileEventKind::Update, projectile, std::nullopt});
        }
    }
    for (const auto id : remove_ids)
        m_projectiles.erase(id);
    return events;
}

std::vector<ConstructProjectileState> ConstructProjectileEngine::snapshot() const
{
    std::vector<ConstructProjectileState> result;
    result.reserve(m_projectiles.size());
    for (const auto &[id, projectile] : m_projectiles) {
        (void)id;
        result.push_back(projectile);
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.id < b.id;
    });
    return result;
}

std::optional<ConstructProjectileState> ConstructProjectileEngine::find(ProjectileId id) const
{
    const auto iterator = m_projectiles.find(id);
    if (iterator == m_projectiles.end())
        return std::nullopt;
    return iterator->second;
}

std::size_t ConstructProjectileEngine::size() const noexcept
{
    return m_projectiles.size();
}

double ConstructProjectileEngine::armourFor(const ConstructNode &node) const
{
    if (m_armour_resolver)
        return std::max(0.05, m_armour_resolver(node));
    const std::string &name = node.node_name;
    if (name.find("obsidian") != std::string::npos || name.find("armour") != std::string::npos)
        return 8.0;
    if (name.find("steel") != std::string::npos || name.find("iron") != std::string::npos)
        return 4.0;
    if (name.find("glass") != std::string::npos)
        return 0.5;
    if (name.find("wood") != std::string::npos)
        return 0.8;
    return 1.5;
}

ConstructExplosionResult ConstructProjectileEngine::explode(const Vec3d &world_position,
    const ConstructProjectileState &projectile, ConstructId directly_hit_construct_id)
{
    ConstructExplosionResult result;
    result.world_position = world_position;
    result.source_construct_id = projectile.source_construct_id;
    result.directly_hit_construct_id = directly_hit_construct_id;
    std::unordered_set<ConstructId> affected;

    const double radius = projectile.spec.blast_radius;
    if (radius <= 0.0 || projectile.spec.blast_power <= 0.0)
        return result;

    for (const auto &construct : m_registry.snapshot()) {
        if (!construct || construct->id() == projectile.source_construct_id)
            continue;
        const Aabb3d bounds = ConstructGeometry::worldBounds(*construct);
        const Aabb3d blast_box{{world_position.x - radius, world_position.y - radius,
                                   world_position.z - radius},
            {world_position.x + radius, world_position.y + radius,
                world_position.z + radius}};
        if (!bounds.valid() || !bounds.intersects(blast_box))
            continue;

        std::vector<LocalNodePos> destroy;
        for (const auto &entry : construct->nodes()) {
            double distance = 0.0;
            if (!nodeInsideRadius(*construct, entry.position, world_position, radius, distance))
                continue;
            const double falloff = std::clamp(1.0 - distance / std::max(radius, EPSILON), 0.0, 1.0);
            ConstructNodeDamage damage;
            damage.construct_id = construct->id();
            damage.node_position = entry.position;
            damage.node_name = entry.node.node_name;
            damage.distance = distance;
            damage.effective_power = projectile.spec.blast_power * falloff;
            if (directly_hit_construct_id == construct->id() && distance <= 1.0)
                damage.effective_power += projectile.spec.penetration;
            damage.armour = armourFor(entry.node);
            damage.destroyed = damage.effective_power >= damage.armour;
            damage.breached = damage.destroyed &&
                (projectile.spec.kind == ConstructProjectileKind::Torpedo ||
                    projectile.spec.kind == ConstructProjectileKind::DepthCharge ||
                    world_position.y <= construct->transform().position.y + 1.0);
            if (damage.destroyed)
                destroy.push_back(entry.position);
            if (damage.breached)
                ++result.breaches;
            result.node_damage.push_back(std::move(damage));
        }
        if (destroy.empty())
            continue;
        affected.insert(construct->id());
        for (const auto &position : destroy) {
            if (construct->removeNode(position))
                ++result.destroyed_nodes;
        }
        const Vec3d impulse_direction = normalised(ConstructGeometry::worldBounds(*construct).centre() -
            world_position);
        const double mass =
            static_cast<double>(std::max<std::size_t>(construct->nodeCount(), 1U));
        construct->setLinearVelocity(construct->linearVelocity() +
            impulse_direction * (projectile.spec.blast_power / static_cast<double>(mass)));
    }
    result.affected_constructs.assign(affected.begin(), affected.end());
    std::sort(result.affected_constructs.begin(), result.affected_constructs.end());
    return result;
}

} // namespace navycraft

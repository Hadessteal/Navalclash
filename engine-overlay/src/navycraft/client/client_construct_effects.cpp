// SPDX-License-Identifier: LGPL-2.1-or-later
#include "client_construct_effects.h"

#include "client/client.h"
#include "client/clientevent.h"
#include "client/localplayer.h"
#include "client/particles.h"
#include "client/sound.h"
#include "construct/client_construct_state.h"
#include "particles.h"
#include "sound_spec.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace navycraft {
namespace {
v3f toV3f(const Vec3d &value)
{
    return {static_cast<f32>(value.x), static_cast<f32>(value.y), static_cast<f32>(value.z)};
}

Vec3d normalised(const Vec3d &value)
{
    const double magnitude = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    return magnitude <= 1e-9 ? Vec3d{} : value * (1.0 / magnitude);
}

std::string defaultTexture(ConstructEffectPreset preset)
{
    switch (preset) {
    case ConstructEffectPreset::Wake:
    case ConstructEffectPreset::Flood:
    case ConstructEffectPreset::Splash:
        return "nc_splash.png";
    case ConstructEffectPreset::Exhaust:
    case ConstructEffectPreset::Smoke:
        return "nc_smoke.png";
    case ConstructEffectPreset::Fire:
        return "nc_fire.png";
    case ConstructEffectPreset::Torpedo:
    case ConstructEffectPreset::DepthCharge:
        return "nc_bubble.png";
    case ConstructEffectPreset::Muzzle:
    case ConstructEffectPreset::Explosion:
    case ConstructEffectPreset::DamageSparks:
        return "nc_spark.png";
    case ConstructEffectPreset::Engine:
    case ConstructEffectPreset::Generic:
        return "nc_frame.png";
    }
    return "nc_frame.png";
}
}

ClientConstructEffects::ClientConstructEffects(Client *client, ISoundManager *sound,
    ParticleManager *particles, ClientConstructManager *constructs) :
    m_client(client), m_sound(sound), m_particles(particles), m_constructs(constructs)
{
    if (!m_client || !m_sound || !m_particles || !m_constructs)
        throw std::invalid_argument("NavyCraft effects require live client services");
}

ClientConstructEffects::~ClientConstructEffects()
{
    reset();
}

std::size_t ClientConstructEffects::KeyHash::operator()(const Key &key) const noexcept
{
    std::size_t seed = std::hash<ConstructId>{}(key.construct_id);
    seed ^= std::hash<std::uint64_t>{}(key.effect_id) + 0x9e3779b9U +
        (seed << 6U) + (seed >> 2U);
    return seed;
}

bool ClientConstructEffects::applyPacket(const std::vector<std::uint8_t> &payload)
{
    const ConstructEffectEvent event = ConstructEffectPacketCodec::decode(payload);
    if (!m_state.apply(event))
        return false;
    const Key key{event.construct_id, event.effect_id};
    if (event.kind == ConstructEffectKind::SoundLoopStart)
        startLoop(event);
    else if (event.kind == ConstructEffectKind::SoundLoopStop)
        stopLoop(key);
    else if (event.kind == ConstructEffectKind::ParticleEmitterStop)
        m_emitter_accumulators.erase(key);
    else if (event.kind == ConstructEffectKind::ParticleEmitterStart)
        m_emitter_accumulators[key] = 0.0;
    return true;
}

bool ClientConstructEffects::applyProjectilePacket(
    const std::vector<std::uint8_t> &payload)
{
    const ConstructProjectileEvent event = ConstructProjectilePacketCodec::decode(payload);
    if (!m_projectile_state.apply(event))
        return false;
    processProjectileEvent(event);
    return true;
}

void ClientConstructEffects::removeConstruct(ConstructId construct_id)
{
    for (auto iterator = m_loop_sounds.begin(); iterator != m_loop_sounds.end();) {
        if (iterator->first.construct_id == construct_id) {
            if (iterator->second.handle > 0) {
                m_sound->stopSound(iterator->second.handle);
                m_sound->freeId(iterator->second.handle);
            }
            iterator = m_loop_sounds.erase(iterator);
        } else {
            ++iterator;
        }
    }
    for (auto iterator = m_emitter_accumulators.begin();
            iterator != m_emitter_accumulators.end();) {
        if (iterator->first.construct_id == construct_id)
            iterator = m_emitter_accumulators.erase(iterator);
        else
            ++iterator;
    }
    m_state.removeConstruct(construct_id);
}

void ClientConstructEffects::reset()
{
    for (auto &[key, loop] : m_loop_sounds) {
        (void)key;
        if (loop.handle > 0) {
            m_sound->stopSound(loop.handle);
            m_sound->freeId(loop.handle);
        }
    }
    m_loop_sounds.clear();
    m_emitter_accumulators.clear();
    m_state.clear();
    m_projectile_state.clear();
}

void ClientConstructEffects::step(double client_time, double delta_seconds)
{
    (void)client_time;
    if (delta_seconds <= 0.0 || !std::isfinite(delta_seconds))
        return;

    for (const ConstructEffectEvent &event : m_state.drainTransient())
        processTransient(event);

    for (auto iterator = m_loop_sounds.begin(); iterator != m_loop_sounds.end();) {
        const DynamicConstruct *construct = findConstruct(iterator->first.construct_id);
        if (!construct) {
            if (iterator->second.handle > 0) {
                m_sound->stopSound(iterator->second.handle);
                m_sound->freeId(iterator->second.handle);
            }
            iterator = m_loop_sounds.erase(iterator);
            continue;
        }
        const auto sampled = ConstructEffectState::sample(iterator->second.event, *construct);
        m_sound->updateSoundPosVel(iterator->second.handle,
            toV3f(sampled.world_position), toV3f(sampled.world_velocity));
        ++iterator;
    }

    const auto active = m_state.activeEffects();
    for (const auto &event : active) {
        if (event.kind != ConstructEffectKind::ParticleEmitterStart)
            continue;
        const DynamicConstruct *construct = findConstruct(event.construct_id);
        if (!construct)
            continue;
        const Key key{event.construct_id, event.effect_id};
        double &accumulator = m_emitter_accumulators[key];
        accumulator += delta_seconds * static_cast<double>(event.amount);
        const auto count = static_cast<std::uint16_t>(std::min(128.0, std::floor(accumulator)));
        if (count == 0)
            continue;
        accumulator -= count;
        spawnBurst(ConstructEffectState::sample(event, *construct), count);
    }
}

ConstructEffectState &ClientConstructEffects::state() noexcept
{
    return m_state;
}

const ConstructEffectState &ClientConstructEffects::state() const noexcept
{
    return m_state;
}

void ClientConstructEffects::processProjectileEvent(
    const ConstructProjectileEvent &projectile_event)
{
    const auto &projectile = projectile_event.projectile;
    SampledConstructEffect sampled;
    sampled.world_position = projectile.position;
    sampled.world_velocity = projectile.velocity;
    ConstructEffectEvent effect;
    effect.construct_id = projectile.source_construct_id;
    effect.local_direction = normalised(projectile.velocity);
    effect.velocity = projectile.velocity;
    effect.duration = 0.2;
    effect.amount = 1;
    effect.size_min = 0.12;
    effect.size_max = 0.28;
    effect.lifetime_min = 0.08;
    effect.lifetime_max = 0.25;
    effect.glow = projectile.spec.kind == ConstructProjectileKind::Torpedo ||
        projectile.spec.kind == ConstructProjectileKind::DepthCharge ? 0 : 8;
    switch (projectile.spec.kind) {
    case ConstructProjectileKind::Torpedo:
    case ConstructProjectileKind::DepthCharge:
        effect.preset = ConstructEffectPreset::Torpedo;
        effect.texture_name = "nc_bubble.png";
        break;
    case ConstructProjectileKind::Fireball:
        effect.preset = ConstructEffectPreset::Fire;
        effect.texture_name = "nc_fire.png";
        effect.size_min = 0.3;
        effect.size_max = 0.65;
        effect.glow = 12;
        break;
    case ConstructProjectileKind::Bomb:
        effect.preset = ConstructEffectPreset::Smoke;
        effect.texture_name = "nc_smoke.png";
        break;
    case ConstructProjectileKind::Shell:
    case ConstructProjectileKind::AntiAircraft:
        effect.preset = ConstructEffectPreset::DamageSparks;
        effect.texture_name = "nc_spark.png";
        break;
    }
    sampled.event = effect;

    if (projectile_event.kind == ConstructProjectileEventKind::Spawn ||
            projectile_event.kind == ConstructProjectileEventKind::Update) {
        spawnBurst(sampled, 1);
        return;
    }
    if (projectile_event.kind != ConstructProjectileEventKind::Impact ||
            !projectile_event.impact)
        return;

    effect.preset = projectile_event.impact->kind == ConstructImpactKind::Water ?
        ConstructEffectPreset::Splash : ConstructEffectPreset::Explosion;
    effect.texture_name = projectile_event.impact->kind == ConstructImpactKind::Water ?
        "nc_splash.png" : "nc_spark.png";
    effect.amount = static_cast<std::uint16_t>(std::clamp(
        12.0 + projectile.spec.blast_power * 2.0, 12.0, 128.0));
    effect.size_min = 0.5;
    effect.size_max = std::clamp(projectile.spec.blast_radius, 1.0, 8.0);
    effect.lifetime_min = 0.25;
    effect.lifetime_max = 1.2;
    effect.glow = 12;
    sampled.event = effect;
    sampled.world_position = projectile_event.impact->world_position;
    sampled.world_velocity = {};
    spawnBurst(sampled, effect.amount);

    const char *sound_name = projectile.spec.kind == ConstructProjectileKind::DepthCharge ?
        "nc_depth_charge" : "nc_hull_hit";
    SoundSpec sound(sound_name, 1.0f, false, 0.0f, 1.0f);
    m_sound->playSoundAt(0, sound, toV3f(sampled.world_position), v3f());
}

void ClientConstructEffects::processTransient(const ConstructEffectEvent &event)
{
    const DynamicConstruct *construct = findConstruct(event.construct_id);
    if (!construct)
        return;
    const auto sampled = ConstructEffectState::sample(event, *construct);
    if (event.kind == ConstructEffectKind::SoundOneShot && !event.sound_name.empty()) {
        SoundSpec spec(event.sound_name, static_cast<float>(event.gain), false, 0.0f,
            static_cast<float>(event.pitch));
        m_sound->playSoundAt(0, spec, toV3f(sampled.world_position),
            toV3f(sampled.world_velocity));
    } else if (event.kind == ConstructEffectKind::ParticleBurst ||
            event.kind == ConstructEffectKind::LightFlash) {
        spawnBurst(sampled, std::max<std::uint16_t>(1, event.amount));
    }
}

void ClientConstructEffects::startLoop(const ConstructEffectEvent &event)
{
    const Key key{event.construct_id, event.effect_id};
    stopLoop(key);
    const DynamicConstruct *construct = findConstruct(event.construct_id);
    if (!construct || event.sound_name.empty())
        return;
    const auto sampled = ConstructEffectState::sample(event, *construct);
    const int handle = m_sound->allocateId(1);
    SoundSpec spec(event.sound_name, static_cast<float>(event.gain), true, 0.0f,
        static_cast<float>(event.pitch));
    m_sound->playSoundAt(handle, spec, toV3f(sampled.world_position),
        toV3f(sampled.world_velocity));
    m_loop_sounds.emplace(key, LoopSound{handle, event});
}

void ClientConstructEffects::stopLoop(const Key &key)
{
    const auto iterator = m_loop_sounds.find(key);
    if (iterator == m_loop_sounds.end())
        return;
    if (iterator->second.handle > 0) {
        m_sound->stopSound(iterator->second.handle);
        m_sound->freeId(iterator->second.handle);
    }
    m_loop_sounds.erase(iterator);
}

void ClientConstructEffects::spawnBurst(
    const SampledConstructEffect &sampled, std::uint16_t amount)
{
    amount = std::min<std::uint16_t>(amount, 512);
    auto random01 = [&]() {
        m_random_state = m_random_state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>((m_random_state >> 11U) & 0x1fffffU) /
            static_cast<double>(0x1fffffU);
    };
    auto symmetric = [&]() { return random01() * 2.0 - 1.0; };

    const ConstructEffectEvent &event = sampled.event;
    const Vec3d direction = sampled.world_direction;
    const double direction_length = std::sqrt(direction.x * direction.x +
        direction.y * direction.y + direction.z * direction.z);
    const Vec3d normal = direction_length > 1e-6 ? direction * (1.0 / direction_length) :
        Vec3d{0.0, 1.0, 0.0};

    for (std::uint16_t index = 0; index < amount; ++index) {
        auto *parameters = new ParticleParameters();
        parameters->pos = toV3f(sampled.world_position + Vec3d{
            symmetric() * 0.35, symmetric() * 0.25, symmetric() * 0.35});
        Vec3d velocity = sampled.world_velocity;
        const double spread = event.preset == ConstructEffectPreset::Explosion ? 8.0 :
            event.preset == ConstructEffectPreset::Muzzle ? 5.0 :
            event.preset == ConstructEffectPreset::Wake ? 1.8 : 2.5;
        velocity += normal * (spread * (0.4 + random01()));
        velocity += Vec3d{symmetric() * spread, std::abs(symmetric()) * spread,
            symmetric() * spread} * 0.45;
        if (event.preset == ConstructEffectPreset::Smoke ||
                event.preset == ConstructEffectPreset::Exhaust ||
                event.preset == ConstructEffectPreset::Fire)
            velocity.y += 1.0 + random01() * 2.0;
        if (event.preset == ConstructEffectPreset::Wake ||
                event.preset == ConstructEffectPreset::Flood ||
                event.preset == ConstructEffectPreset::Splash)
            velocity.y = std::abs(velocity.y) + random01() * 2.0;
        parameters->vel = toV3f(velocity);
        parameters->acc = v3f(0.0f,
            event.preset == ConstructEffectPreset::Smoke ? 0.4f : -2.0f, 0.0f);
        parameters->expirationtime = static_cast<f32>(event.lifetime_min +
            (event.lifetime_max - event.lifetime_min) * random01());
        parameters->size = static_cast<f32>(event.size_min +
            (event.size_max - event.size_min) * random01());
        parameters->collisiondetection = event.collision;
        parameters->collision_removal = event.collision;
        parameters->glow = event.glow;
        parameters->texture.string = event.texture_name.empty() ?
            defaultTexture(event.preset) : event.texture_name;
        if (event.preset == ConstructEffectPreset::Fire ||
                event.preset == ConstructEffectPreset::Muzzle ||
                event.preset == ConstructEffectPreset::Explosion ||
                event.preset == ConstructEffectPreset::DamageSparks)
            parameters->texture.blendmode = ParticleParamTypes::BlendMode::add;

        ClientEvent client_event(CE_SPAWN_PARTICLE);
        client_event.spawn_particle = parameters;
        m_particles->handleParticleEvent(&client_event, m_client,
            m_client->getEnv().getLocalPlayer());
    }
}

const DynamicConstruct *ClientConstructEffects::findConstruct(ConstructId id) const
{
    const auto state = m_constructs->find(id);
    return state ? &state->construct() : nullptr;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_types.h"
#include "dynamic_construct.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

enum class ConstructEffectKind : std::uint8_t {
    SoundOneShot = 0,
    SoundLoopStart = 1,
    SoundLoopStop = 2,
    ParticleBurst = 3,
    ParticleEmitterStart = 4,
    ParticleEmitterStop = 5,
    LightFlash = 6,
};

enum class ConstructEffectPreset : std::uint8_t {
    Generic = 0,
    Engine = 1,
    Wake = 2,
    Exhaust = 3,
    Smoke = 4,
    Fire = 5,
    Flood = 6,
    Muzzle = 7,
    Explosion = 8,
    Torpedo = 9,
    DepthCharge = 10,
    Splash = 11,
    DamageSparks = 12,
};

struct ConstructEffectEvent {
    std::uint64_t sequence = 0;
    ConstructId construct_id = 0;
    std::uint64_t effect_id = 0;
    ConstructEffectKind kind = ConstructEffectKind::ParticleBurst;
    ConstructEffectPreset preset = ConstructEffectPreset::Generic;
    Vec3d local_position{};
    Vec3d local_direction{0.0, 0.0, 1.0};
    Vec3d velocity{};
    std::string sound_name;
    std::string texture_name;
    double gain = 1.0;
    double pitch = 1.0;
    double max_distance = 64.0;
    double duration = 0.5;
    std::uint16_t amount = 1;
    double size_min = 0.25;
    double size_max = 1.0;
    double lifetime_min = 0.2;
    double lifetime_max = 1.0;
    std::uint8_t glow = 0;
    bool collision = false;
};

struct SampledConstructEffect {
    ConstructEffectEvent event{};
    Vec3d world_position{};
    Vec3d world_direction{};
    Vec3d world_velocity{};
};

class ConstructEffectPacketCodec final {
public:
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t MAX_STRING_BYTES = 4096;
    static constexpr std::size_t MAX_PACKET_BYTES = 64U * 1024U;

    [[nodiscard]] static std::vector<std::uint8_t> encode(
        const ConstructEffectEvent &event);
    [[nodiscard]] static ConstructEffectEvent decode(
        const std::vector<std::uint8_t> &bytes);
};

class ConstructEffectState final {
public:
    bool apply(const ConstructEffectEvent &event);
    void removeConstruct(ConstructId construct_id);
    void clear();

    [[nodiscard]] std::vector<ConstructEffectEvent> drainTransient();
    [[nodiscard]] std::vector<ConstructEffectEvent> activeEffects() const;
    [[nodiscard]] std::optional<ConstructEffectEvent> findActive(
        ConstructId construct_id, std::uint64_t effect_id) const;

    [[nodiscard]] static SampledConstructEffect sample(
        const ConstructEffectEvent &event, const DynamicConstruct &construct);

private:
    struct Key {
        ConstructId construct_id = 0;
        std::uint64_t effect_id = 0;

        bool operator==(const Key &other) const noexcept
        {
            return construct_id == other.construct_id && effect_id == other.effect_id;
        }
    };

    struct KeyHash {
        std::size_t operator()(const Key &key) const noexcept;
    };

    std::unordered_map<ConstructId, std::uint64_t> m_last_sequences;
    std::unordered_map<Key, ConstructEffectEvent, KeyHash> m_active;
    std::vector<ConstructEffectEvent> m_transient;
};

} // namespace navycraft

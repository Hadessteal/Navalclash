// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct/construct_effects.h"
#include "construct/construct_projectiles.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class Client;
class ISoundManager;
class ParticleManager;
class LocalPlayer;

namespace navycraft {

class ClientConstructManager;

class ClientConstructEffects final {
public:
    ClientConstructEffects(Client *client, ISoundManager *sound,
        ParticleManager *particles, ClientConstructManager *constructs);
    ~ClientConstructEffects();

    ClientConstructEffects(const ClientConstructEffects &) = delete;
    ClientConstructEffects &operator=(const ClientConstructEffects &) = delete;

    bool applyPacket(const std::vector<std::uint8_t> &payload);
    bool applyProjectilePacket(const std::vector<std::uint8_t> &payload);
    void removeConstruct(ConstructId construct_id);
    void reset();
    void step(double client_time, double delta_seconds);

    [[nodiscard]] ConstructEffectState &state() noexcept;
    [[nodiscard]] const ConstructEffectState &state() const noexcept;

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
    struct LoopSound {
        int handle = 0;
        ConstructEffectEvent event{};
    };

    void processTransient(const ConstructEffectEvent &event);
    void processProjectileEvent(const ConstructProjectileEvent &event);
    void startLoop(const ConstructEffectEvent &event);
    void stopLoop(const Key &key);
    void spawnBurst(const SampledConstructEffect &sampled, std::uint16_t amount);
    [[nodiscard]] const DynamicConstruct *findConstruct(ConstructId id) const;

    Client *m_client = nullptr;
    ISoundManager *m_sound = nullptr;
    ParticleManager *m_particles = nullptr;
    ClientConstructManager *m_constructs = nullptr;
    ConstructEffectState m_state;
    ConstructProjectileClientState m_projectile_state;
    std::unordered_map<Key, LoopSound, KeyHash> m_loop_sounds;
    std::unordered_map<Key, double, KeyHash> m_emitter_accumulators;
    std::uint64_t m_random_state = 0x6e61767963726166ULL;
};

} // namespace navycraft

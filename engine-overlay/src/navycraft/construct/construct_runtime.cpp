// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_runtime.h"

#include <mutex>
#include <unordered_map>

namespace navycraft {
namespace {
std::mutex g_sequence_mutex;
std::unordered_map<ConstructId, std::uint64_t> g_transform_sequences;
std::unordered_map<ConstructId, std::uint64_t> g_section_sequences;
std::unordered_map<ConstructId, std::uint64_t> g_effect_sequences;
}

ConstructRegistry &runtimeConstructRegistry() noexcept
{
    static ConstructRegistry registry;
    return registry;
}

ConstructInteractionEngine &runtimeConstructInteractionEngine() noexcept
{
    static ConstructInteractionEngine engine;
    return engine;
}

ConstructProjectileEngine &runtimeConstructProjectileEngine() noexcept
{
    static ConstructProjectileEngine engine(runtimeConstructRegistry());
    return engine;
}

ConstructFireControlEngine &runtimeConstructFireControlEngine() noexcept
{
    static ConstructFireControlEngine engine(runtimeConstructRegistry());
    return engine;
}

ConstructNavigationEngine &runtimeConstructNavigationEngine() noexcept
{
    static ConstructNavigationEngine engine(runtimeConstructRegistry());
    return engine;
}

ConstructStructureEngine &runtimeConstructStructureEngine() noexcept
{
    static ConstructStructureEngine engine(runtimeConstructRegistry());
    return engine;
}

ConstructArticulationEngine &runtimeConstructArticulationEngine() noexcept
{
    static ConstructArticulationEngine engine(runtimeConstructRegistry());
    return engine;
}

ConstructLiquidEngine &runtimeConstructLiquidEngine() noexcept
{
    static ConstructLiquidEngine engine(runtimeConstructRegistry());
    return engine;
}

ConstructSpecialNodeEngine &runtimeConstructSpecialNodeEngine() noexcept
{
    static ConstructSpecialNodeEngine engine;
    return engine;
}

ConstructSimulation &runtimeConstructSimulation() noexcept
{
    static ConstructSimulation simulation;
    return simulation;
}

void clearRuntimeConstructState(ConstructId id) noexcept
{
    runtimeConstructInteractionEngine().removeConstruct(id);
    runtimeConstructFireControlEngine().removeConstruct(id);
    runtimeConstructNavigationEngine().remove(id);
    runtimeConstructStructureEngine().remove(id);
    runtimeConstructArticulationEngine().removeConstruct(id);
    runtimeConstructLiquidEngine().removeConstruct(id);
    clearRuntimeSequences(id);
}

void resetRuntimeConstructState() noexcept
{
    runtimeConstructInteractionEngine().clear();
    runtimeConstructProjectileEngine().clear();
    runtimeConstructFireControlEngine().clear();
    runtimeConstructNavigationEngine().clear();
    runtimeConstructStructureEngine().clear();
    runtimeConstructArticulationEngine().clear();
    runtimeConstructLiquidEngine().clear();
    runtimeConstructSpecialNodeEngine().clear();
    runtimeConstructSimulation().reset();
    runtimeConstructRegistry().clear();
    std::lock_guard<std::mutex> lock(g_sequence_mutex);
    g_transform_sequences.clear();
    g_section_sequences.clear();
    g_effect_sequences.clear();
}

std::uint64_t nextRuntimeTransformSequence(ConstructId id) noexcept
{
    std::lock_guard<std::mutex> lock(g_sequence_mutex);
    return ++g_transform_sequences[id];
}

std::uint64_t nextRuntimeSectionSequence(ConstructId id) noexcept
{
    std::lock_guard<std::mutex> lock(g_sequence_mutex);
    return ++g_section_sequences[id];
}

std::uint64_t nextRuntimeEffectSequence(ConstructId id) noexcept
{
    std::lock_guard<std::mutex> lock(g_sequence_mutex);
    return ++g_effect_sequences[id];
}

void clearRuntimeSequences(ConstructId id) noexcept
{
    std::lock_guard<std::mutex> lock(g_sequence_mutex);
    g_transform_sequences.erase(id);
    g_section_sequences.erase(id);
    g_effect_sequences.erase(id);
}

} // namespace navycraft

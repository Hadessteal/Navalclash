// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_registry.h"
#include "construct_interaction.h"
#include "construct_projectiles.h"
#include "construct_fire_control.h"
#include "construct_navigation.h"
#include "construct_structure.h"
#include "construct_articulation.h"
#include "construct_liquids.h"
#include "construct_special_nodes.h"
#include "construct_simulation.h"

#include <cstdint>

namespace navycraft {

ConstructRegistry &runtimeConstructRegistry() noexcept;
ConstructInteractionEngine &runtimeConstructInteractionEngine() noexcept;
ConstructProjectileEngine &runtimeConstructProjectileEngine() noexcept;
ConstructFireControlEngine &runtimeConstructFireControlEngine() noexcept;
ConstructNavigationEngine &runtimeConstructNavigationEngine() noexcept;
ConstructStructureEngine &runtimeConstructStructureEngine() noexcept;
ConstructArticulationEngine &runtimeConstructArticulationEngine() noexcept;
ConstructLiquidEngine &runtimeConstructLiquidEngine() noexcept;
ConstructSpecialNodeEngine &runtimeConstructSpecialNodeEngine() noexcept;
ConstructSimulation &runtimeConstructSimulation() noexcept;
std::uint64_t nextRuntimeTransformSequence(ConstructId id) noexcept;
std::uint64_t nextRuntimeSectionSequence(ConstructId id) noexcept;
std::uint64_t nextRuntimeEffectSequence(ConstructId id) noexcept;
void clearRuntimeSequences(ConstructId id) noexcept;

} // namespace navycraft

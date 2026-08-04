// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_liquids.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace navycraft {
namespace {
constexpr std::uint8_t CELL_CAPACITY = 8;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

std::vector<LocalNodePos> sortedCells(
    const std::unordered_set<LocalNodePos, LocalNodePosHash> &cells)
{
    std::vector<LocalNodePos> result(cells.begin(), cells.end());
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.y != right.y)
            return left.y < right.y;
        if (left.z != right.z)
            return left.z < right.z;
        return left.x < right.x;
    });
    return result;
}

LocalNodePos below(const LocalNodePos &position) noexcept
{
    return {position.x, position.y - 1, position.z};
}

std::array<LocalNodePos, 4> horizontal(const LocalNodePos &position) noexcept
{
    return {{{position.x + 1, position.y, position.z},
        {position.x - 1, position.y, position.z},
        {position.x, position.y, position.z + 1},
        {position.x, position.y, position.z - 1}}};
}
}

ConstructLiquidEngine::ConstructLiquidEngine(ConstructRegistry &registry) :
    m_registry(registry)
{
}

ConstructLiquidCompartmentId ConstructLiquidEngine::configureCompartment(
    ConstructLiquidCompartmentDefinition definition)
{
    if (definition.construct_id == 0 || !m_registry.find(definition.construct_id))
        throw std::invalid_argument("liquid compartment construct does not exist");
    if (definition.cells.empty())
        throw std::invalid_argument("liquid compartment has no cells");
    if (!finite(definition.horizontal_flow_rate) || definition.horizontal_flow_rate < 0.0 ||
            !finite(definition.vertical_flow_rate) || definition.vertical_flow_rate < 0.0)
        throw std::invalid_argument("liquid compartment flow rate is invalid");
    if (definition.id == 0)
        definition.id = m_next_compartment_id++;
    else
        m_next_compartment_id = std::max(m_next_compartment_id, definition.id + 1);
    CompartmentRuntime runtime;
    runtime.definition = std::move(definition);
    for (const auto &position : runtime.definition.cells)
        runtime.cell_set.insert(position);
    runtime.definition.cells = sortedCells(runtime.cell_set);
    m_compartments[runtime.definition.id] = std::move(runtime);
    return runtime.definition.id;
}

bool ConstructLiquidEngine::removeCompartment(ConstructLiquidCompartmentId id)
{
    for (auto iterator = m_ports.begin(); iterator != m_ports.end();) {
        if (iterator->second.definition.compartment_id == id)
            iterator = m_ports.erase(iterator);
        else
            ++iterator;
    }
    return m_compartments.erase(id) != 0;
}

ConstructLiquidPortId ConstructLiquidEngine::configurePort(
    ConstructLiquidPortDefinition definition)
{
    if (!compartmentFor(definition.compartment_id))
        throw std::invalid_argument("liquid port compartment does not exist");
    if (definition.liquid_name.empty() || !finite(definition.units_per_second) ||
            definition.units_per_second < 0.0)
        throw std::invalid_argument("liquid port definition is invalid");
    if (definition.id == 0)
        definition.id = m_next_port_id++;
    else
        m_next_port_id = std::max(m_next_port_id, definition.id + 1);
    m_ports[definition.id].definition = std::move(definition);
    return definition.id;
}

bool ConstructLiquidEngine::removePort(ConstructLiquidPortId id)
{
    return m_ports.erase(id) != 0;
}

bool ConstructLiquidEngine::setPortEnabled(ConstructLiquidPortId id, bool enabled)
{
    const auto iterator = m_ports.find(id);
    if (iterator == m_ports.end())
        return false;
    iterator->second.definition.enabled = enabled;
    return true;
}

bool ConstructLiquidEngine::setCell(ConstructId construct_id,
    const LocalNodePos &position, std::string liquid_name,
    std::uint8_t level, std::uint8_t flags)
{
    const auto construct = m_registry.find(construct_id);
    if (!construct)
        return false;
    if (level == 0)
        return construct->removeLiquid(position);
    return construct->setLiquid(position,
        {std::move(liquid_name), level, flags});
}

std::optional<ConstructLiquidCell> ConstructLiquidEngine::cell(
    ConstructId construct_id, const LocalNodePos &position) const
{
    const auto construct = m_registry.find(construct_id);
    if (!construct)
        return std::nullopt;
    const auto *liquid = construct->getLiquid(position);
    return liquid ? std::optional<ConstructLiquidCell>(*liquid) : std::nullopt;
}

ConstructLiquidEngine::CompartmentRuntime *ConstructLiquidEngine::compartmentFor(
    ConstructLiquidCompartmentId id)
{
    const auto iterator = m_compartments.find(id);
    return iterator == m_compartments.end() ? nullptr : &iterator->second;
}

const ConstructLiquidEngine::CompartmentRuntime *ConstructLiquidEngine::compartmentFor(
    ConstructLiquidCompartmentId id) const
{
    const auto iterator = m_compartments.find(id);
    return iterator == m_compartments.end() ? nullptr : &iterator->second;
}

ConstructLiquidCompartmentStatus ConstructLiquidEngine::status(
    const CompartmentRuntime &runtime) const
{
    ConstructLiquidCompartmentStatus result;
    result.definition = runtime.definition;
    result.capacity_units = runtime.cell_set.size() * CELL_CAPACITY;
    result.escaped_units = runtime.escaped_units;
    const auto construct = m_registry.find(runtime.definition.construct_id);
    if (!construct || result.capacity_units == 0)
        return result;
    Vec3d weighted{};
    std::string first_liquid;
    for (const auto &position : runtime.cell_set) {
        const auto *liquid = construct->getLiquid(position);
        if (!liquid)
            continue;
        result.total_units += liquid->level;
        weighted += Vec3d{static_cast<double>(position.x) + 0.5,
            static_cast<double>(position.y) + 0.5,
            static_cast<double>(position.z) + 0.5} * liquid->level;
        if (first_liquid.empty())
            first_liquid = liquid->liquid_name;
        else if (first_liquid != liquid->liquid_name)
            result.mixed = true;
    }
    result.fill_fraction = static_cast<double>(result.total_units) /
        static_cast<double>(result.capacity_units);
    if (result.total_units > 0)
        result.centre_of_mass_local = weighted *
            (1.0 / static_cast<double>(result.total_units));
    return result;
}

std::vector<ConstructLiquidCompartmentStatus> ConstructLiquidEngine::compartments(
    ConstructId construct_id) const
{
    std::vector<ConstructLiquidCompartmentStatus> result;
    for (const auto &[id, runtime] : m_compartments) {
        (void)id;
        if (construct_id == 0 || runtime.definition.construct_id == construct_id)
            result.push_back(status(runtime));
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.definition.id < right.definition.id;
    });
    return result;
}

ConstructLiquidStepResult ConstructLiquidEngine::step(double delta_seconds)
{
    if (!finite(delta_seconds) || delta_seconds < 0.0)
        throw std::invalid_argument("liquid step delta is invalid");
    ConstructLiquidStepResult result;
    if (delta_seconds == 0.0) {
        result.compartments = compartments();
        return result;
    }

    for (auto &[port_id, port] : m_ports) {
        (void)port_id;
        if (!port.definition.enabled || port.definition.units_per_second <= 0.0)
            continue;
        auto *compartment = compartmentFor(port.definition.compartment_id);
        if (!compartment || compartment->cell_set.find(port.definition.position) ==
                compartment->cell_set.end())
            continue;
        const auto construct = m_registry.find(compartment->definition.construct_id);
        if (!construct)
            continue;
        port.fractional_units += port.definition.units_per_second * delta_seconds;
        int units = static_cast<int>(std::floor(port.fractional_units));
        port.fractional_units -= units;
        if (units <= 0)
            continue;
        auto current = construct->getLiquid(port.definition.position) ?
            *construct->getLiquid(port.definition.position) : ConstructLiquidCell{};
        if (port.definition.kind == ConstructLiquidPortKind::Source ||
                port.definition.kind == ConstructLiquidPortKind::Breach) {
            if (current.level != 0 && current.liquid_name != port.definition.liquid_name &&
                    !compartment->definition.allow_mixing)
                continue;
            current.liquid_name = port.definition.liquid_name;
            current.level = static_cast<std::uint8_t>(std::min<int>(
                CELL_CAPACITY, current.level + units));
            construct->setLiquid(port.definition.position, current);
            result.changed_cells.push_back(port.definition.position);
        } else {
            const int removed = std::min<int>(current.level, units);
            current.level = static_cast<std::uint8_t>(current.level - removed);
            if (current.level == 0)
                construct->removeLiquid(port.definition.position);
            else
                construct->setLiquid(port.definition.position, current);
            result.changed_cells.push_back(port.definition.position);
        }
    }

    for (auto &[compartment_id, runtime] : m_compartments) {
        (void)compartment_id;
        const auto construct = m_registry.find(runtime.definition.construct_id);
        if (!construct)
            continue;
        const auto ordered = sortedCells(runtime.cell_set);
        const int vertical_budget = std::max(1, static_cast<int>(std::ceil(
            runtime.definition.vertical_flow_rate * delta_seconds)));
        const int horizontal_budget = std::max(1, static_cast<int>(std::ceil(
            runtime.definition.horizontal_flow_rate * delta_seconds)));

        for (const auto &position : ordered) {
            auto liquid_ptr = construct->getLiquid(position);
            if (!liquid_ptr || liquid_ptr->level == 0)
                continue;
            ConstructLiquidCell liquid = *liquid_ptr;
            const LocalNodePos target = below(position);
            if (runtime.cell_set.find(target) != runtime.cell_set.end() &&
                    !construct->getNode(target)) {
                auto target_liquid = construct->getLiquid(target) ?
                    *construct->getLiquid(target) : ConstructLiquidCell{};
                if (target_liquid.level == 0 || target_liquid.liquid_name == liquid.liquid_name ||
                        runtime.definition.allow_mixing) {
                    const int transfer = std::min({vertical_budget,
                        static_cast<int>(liquid.level),
                        static_cast<int>(CELL_CAPACITY - target_liquid.level)});
                    if (transfer > 0) {
                        if (target_liquid.liquid_name.empty())
                            target_liquid.liquid_name = liquid.liquid_name;
                        target_liquid.level = static_cast<std::uint8_t>(
                            target_liquid.level + transfer);
                        liquid.level = static_cast<std::uint8_t>(liquid.level - transfer);
                        construct->setLiquid(target, target_liquid);
                        if (liquid.level == 0)
                            construct->removeLiquid(position);
                        else
                            construct->setLiquid(position, liquid);
                        result.changed_cells.push_back(target);
                        result.changed_cells.push_back(position);
                    }
                }
            } else if (!runtime.definition.sealed && !construct->getNode(target)) {
                const int escaped = std::min(vertical_budget,
                    static_cast<int>(liquid.level));
                liquid.level = static_cast<std::uint8_t>(liquid.level - escaped);
                runtime.escaped_units += escaped;
                result.escaped_units += escaped;
                if (liquid.level == 0)
                    construct->removeLiquid(position);
                else
                    construct->setLiquid(position, liquid);
                result.changed_cells.push_back(position);
            }
        }

        for (const auto &position : ordered) {
            auto source_ptr = construct->getLiquid(position);
            if (!source_ptr || source_ptr->level <= 1)
                continue;
            for (const auto &target : horizontal(position)) {
                if (runtime.cell_set.find(target) == runtime.cell_set.end() ||
                        construct->getNode(target))
                    continue;
                ConstructLiquidCell source = *construct->getLiquid(position);
                auto target_liquid = construct->getLiquid(target) ?
                    *construct->getLiquid(target) : ConstructLiquidCell{};
                if (target_liquid.level != 0 &&
                        target_liquid.liquid_name != source.liquid_name &&
                        !runtime.definition.allow_mixing)
                    continue;
                const int difference = static_cast<int>(source.level) -
                    static_cast<int>(target_liquid.level);
                const int transfer = std::min(horizontal_budget,
                    std::max(0, difference / 2));
                if (transfer <= 0)
                    continue;
                if (target_liquid.liquid_name.empty())
                    target_liquid.liquid_name = source.liquid_name;
                source.level = static_cast<std::uint8_t>(source.level - transfer);
                target_liquid.level = static_cast<std::uint8_t>(target_liquid.level + transfer);
                construct->setLiquid(position, source);
                construct->setLiquid(target, target_liquid);
                result.changed_cells.push_back(position);
                result.changed_cells.push_back(target);
            }
        }
    }

    std::sort(result.changed_cells.begin(), result.changed_cells.end(),
        [](const auto &left, const auto &right) {
            if (left.y != right.y) return left.y < right.y;
            if (left.z != right.z) return left.z < right.z;
            return left.x < right.x;
        });
    result.changed_cells.erase(std::unique(result.changed_cells.begin(),
        result.changed_cells.end()), result.changed_cells.end());
    result.compartments = compartments();
    return result;
}

void ConstructLiquidEngine::removeConstruct(ConstructId construct_id)
{
    std::vector<ConstructLiquidCompartmentId> ids;
    for (const auto &[id, runtime] : m_compartments) {
        if (runtime.definition.construct_id == construct_id)
            ids.push_back(id);
    }
    for (const auto id : ids)
        (void)removeCompartment(id);
}

void ConstructLiquidEngine::clear()
{
    m_ports.clear();
    m_compartments.clear();
    m_next_port_id = 1;
    m_next_compartment_id = 1;
}

const char *constructLiquidPortKindName(ConstructLiquidPortKind kind) noexcept
{
    switch (kind) {
    case ConstructLiquidPortKind::Source: return "source";
    case ConstructLiquidPortKind::Drain: return "drain";
    case ConstructLiquidPortKind::Breach: return "breach";
    }
    return "source";
}

} // namespace navycraft

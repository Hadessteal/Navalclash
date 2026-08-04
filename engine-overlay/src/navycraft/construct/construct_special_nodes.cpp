// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_special_nodes.h"

#include "construct_geometry.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navycraft {
namespace {

bool finiteVec(const Vec3d &value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

double lengthSquared(const Vec3d &value) noexcept
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

Aabb3d nodeBoxWorld(const DynamicConstruct &construct,
    const LocalNodePos &position) noexcept
{
    const Vec3d corners[] = {
        construct.transform().localToWorld({static_cast<double>(position.x),
            static_cast<double>(position.y), static_cast<double>(position.z)}),
        construct.transform().localToWorld({static_cast<double>(position.x + 1),
            static_cast<double>(position.y + 1), static_cast<double>(position.z + 1)}),
        construct.transform().localToWorld({static_cast<double>(position.x + 1),
            static_cast<double>(position.y), static_cast<double>(position.z)}),
        construct.transform().localToWorld({static_cast<double>(position.x),
            static_cast<double>(position.y + 1), static_cast<double>(position.z + 1)}),
    };
    Aabb3d result;
    result.min = result.max = corners[0];
    for (const auto &corner : corners) {
        result.min.x = std::min(result.min.x, corner.x);
        result.min.y = std::min(result.min.y, corner.y);
        result.min.z = std::min(result.min.z, corner.z);
        result.max.x = std::max(result.max.x, corner.x);
        result.max.y = std::max(result.max.y, corner.y);
        result.max.z = std::max(result.max.z, corner.z);
    }
    return result;
}

bool overlaps(const Aabb3d &left, const Aabb3d &right) noexcept
{
    return left.valid() && right.valid() &&
        left.min.x < right.max.x && left.max.x > right.min.x &&
        left.min.y < right.max.y && left.max.y > right.min.y &&
        left.min.z < right.max.z && left.max.z > right.min.z;
}

double overlapVolume(const Aabb3d &left, const Aabb3d &right) noexcept
{
    if (!overlaps(left, right))
        return 0.0;
    return std::max(0.0, std::min(left.max.x, right.max.x) -
            std::max(left.min.x, right.min.x)) *
        std::max(0.0, std::min(left.max.y, right.max.y) -
            std::max(left.min.y, right.min.y)) *
        std::max(0.0, std::min(left.max.z, right.max.z) -
            std::max(left.min.z, right.min.z));
}

double volume(const Aabb3d &box) noexcept
{
    return box.valid() ? (box.max.x - box.min.x) *
        (box.max.y - box.min.y) * (box.max.z - box.min.z) : 0.0;
}

} // namespace

bool ConstructSpecialNodeEngine::registerDefinition(
    ConstructSpecialNodeDefinition definition)
{
    if (definition.node_name.empty() || !finiteVec(definition.conveyor_velocity_local) ||
            !std::isfinite(definition.damage_per_second) ||
            definition.damage_per_second < 0.0 ||
            !std::isfinite(definition.conveyor_acceleration) ||
            definition.conveyor_acceleration < 0.0)
        throw std::invalid_argument("special construct node definition is invalid");
    const bool inserted = m_definitions.find(definition.node_name) == m_definitions.end();
    m_definitions[definition.node_name] = std::move(definition);
    return inserted;
}

bool ConstructSpecialNodeEngine::removeDefinition(const std::string &node_name)
{
    return m_definitions.erase(node_name) != 0;
}

std::optional<ConstructSpecialNodeDefinition> ConstructSpecialNodeEngine::definition(
    const std::string &node_name) const
{
    const auto iterator = m_definitions.find(node_name);
    return iterator == m_definitions.end() ? std::nullopt :
        std::optional<ConstructSpecialNodeDefinition>(iterator->second);
}

std::vector<ConstructSpecialNodeDefinition> ConstructSpecialNodeEngine::definitions() const
{
    std::vector<ConstructSpecialNodeDefinition> result;
    result.reserve(m_definitions.size());
    for (const auto &[name, definition] : m_definitions) {
        (void)name;
        result.push_back(definition);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.node_name < right.node_name;
    });
    return result;
}

ConstructSpecialBodyOutput ConstructSpecialNodeEngine::evaluate(
    const std::vector<const DynamicConstruct *> &constructs,
    const ConstructArticulationEngine *articulations,
    const ConstructSpecialBodyInput &input) const noexcept
{
    ConstructSpecialBodyOutput output;
    output.velocity = input.velocity;
    if (!input.world_box.valid())
        return output;
    const double dt = std::isfinite(input.delta_seconds) ?
        std::max(0.0, input.delta_seconds) : 0.0;
    const double body_volume = std::max(1e-9, volume(input.world_box));
    double immersed_volume = 0.0;

    for (const DynamicConstruct *construct : constructs) {
        if (!construct)
            continue;
        for (const auto &entry : construct->nodes()) {
            const auto definition_iterator = m_definitions.find(entry.node.node_name);
            if (definition_iterator == m_definitions.end())
                continue;
            const auto articulation_id = articulations ?
                articulations->nodeJoint(construct->id(), entry.position) : std::nullopt;
            Aabb3d node_box;
            Vec3d world_velocity{};
            if (articulation_id && articulations) {
                try {
                    const Vec3d centre = articulations->transformPointWorld(
                        *articulation_id,
                        {static_cast<double>(entry.position.x) + 0.5,
                            static_cast<double>(entry.position.y) + 0.5,
                            static_cast<double>(entry.position.z) + 0.5});
                    node_box = {{centre.x - 0.5, centre.y - 0.5, centre.z - 0.5},
                        {centre.x + 0.5, centre.y + 0.5, centre.z + 0.5}};
                    world_velocity = articulations->pointVelocityWorld(
                        *articulation_id,
                        {static_cast<double>(entry.position.x) + 0.5,
                            static_cast<double>(entry.position.y) + 0.5,
                            static_cast<double>(entry.position.z) + 0.5});
                } catch (...) {
                    continue;
                }
            } else {
                node_box = nodeBoxWorld(*construct, entry.position);
                world_velocity = ConstructGeometry::surfaceVelocity(
                    *construct, node_box.centre());
            }
            if (!overlaps(input.world_box, node_box))
                continue;
            const auto &definition = definition_iterator->second;
            ConstructSpecialNodeContact contact;
            contact.construct_id = construct->id();
            contact.articulation_id = articulation_id.value_or(0);
            contact.position = entry.position;
            contact.definition = definition;
            contact.world_velocity = world_velocity;
            output.contacts.push_back(contact);
            output.damage += definition.damage_per_second * dt;
            output.breathable = output.breathable && definition.breathable;
            if (definition.climbable && input.climbing_input) {
                output.climbing = true;
                output.velocity.y = std::max(output.velocity.y, 2.0);
            }
            Vec3d conveyor_world = construct->transform().localToWorld(
                definition.conveyor_velocity_local) - construct->transform().position;
            if (articulation_id && articulations) {
                try {
                    conveyor_world = articulations->transformDirectionWorld(
                        *articulation_id, definition.conveyor_velocity_local);
                } catch (...) {
                }
            }
            if (lengthSquared(definition.conveyor_velocity_local) > 1e-12) {
                const Vec3d desired = conveyor_world + world_velocity;
                const double factor = std::min(1.0,
                    definition.conveyor_acceleration * dt);
                output.velocity += (desired - output.velocity) * factor;
                output.conveyor_delta += desired * dt;
            }
        }

        for (const auto &entry : construct->liquids()) {
            const Vec3d local_min{static_cast<double>(entry.position.x),
                static_cast<double>(entry.position.y),
                static_cast<double>(entry.position.z)};
            const double height = static_cast<double>(entry.liquid.level) / 8.0;
            const Vec3d local_max{local_min.x + 1.0, local_min.y + height,
                local_min.z + 1.0};
            const Vec3d world_min = construct->transform().localToWorld(local_min);
            const Vec3d world_max = construct->transform().localToWorld(local_max);
            Aabb3d liquid_box{{std::min(world_min.x, world_max.x),
                std::min(world_min.y, world_max.y),
                std::min(world_min.z, world_max.z)},
                {std::max(world_min.x, world_max.x),
                std::max(world_min.y, world_max.y),
                std::max(world_min.z, world_max.z)}};
            immersed_volume += overlapVolume(input.world_box, liquid_box);
        }
    }
    output.immersed_fraction = std::clamp(immersed_volume / body_volume, 0.0, 1.0);
    return output;
}

void ConstructSpecialNodeEngine::clear()
{
    m_definitions.clear();
}

} // namespace navycraft

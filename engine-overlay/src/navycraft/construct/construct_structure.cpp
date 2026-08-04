// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_structure.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace navycraft {
namespace {
constexpr LocalNodePos DIRECTIONS[] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
    {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};
constexpr double EPSILON = 1e-9;

LocalNodePos add(const LocalNodePos &left, const LocalNodePos &right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

LocalNodePos subtract(const LocalNodePos &left, const LocalNodePos &right) noexcept
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

bool positionLess(const LocalNodePos &left, const LocalNodePos &right) noexcept
{
    if (left.y != right.y)
        return left.y < right.y;
    if (left.z != right.z)
        return left.z < right.z;
    return left.x < right.x;
}

void extendBounds(LocalBounds &bounds, const LocalNodePos &position) noexcept
{
    if (!bounds.valid) {
        bounds.min = position;
        bounds.max = position;
        bounds.valid = true;
        return;
    }
    bounds.min.x = std::min(bounds.min.x, position.x);
    bounds.min.y = std::min(bounds.min.y, position.y);
    bounds.min.z = std::min(bounds.min.z, position.z);
    bounds.max.x = std::max(bounds.max.x, position.x);
    bounds.max.y = std::max(bounds.max.y, position.y);
    bounds.max.z = std::max(bounds.max.z, position.z);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool containsAny(const std::string &value, std::initializer_list<const char *> needles)
{
    for (const char *needle : needles) {
        if (value.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

ConstructMaterialProperties defaultMaterial(const ConstructNode &node)
{
    const std::string name = lower(node.node_name);
    ConstructMaterialProperties properties;
    properties.control = containsAny(name,
        {"helm", "pilot", "control", "wheel", "periscope", "rudder"});
    if (containsAny(name, {"sign", "torch", "ladder", "rail", "wire", "antenna",
            "smoke", "water", "air", "radio"})) {
        properties.structural = false;
        properties.mass = 0.08;
        properties.displaced_volume = 0.05;
        properties.structural_strength = 0.05;
        properties.watertight = false;
        return properties;
    }
    if (containsAny(name, {"obsidian", "titanium"})) {
        properties.mass = 2.1;
        properties.structural_strength = 8.0;
    } else if (containsAny(name, {"armour", "armor", "steel", "iron"})) {
        properties.mass = 1.45;
        properties.structural_strength = 4.0;
    } else if (containsAny(name, {"engine", "motor", "turbine", "reactor"})) {
        properties.mass = 2.2;
        properties.structural_strength = 2.5;
    } else if (containsAny(name, {"wood", "plank"})) {
        properties.mass = 0.45;
        properties.structural_strength = 0.8;
    } else if (name.find("glass") != std::string::npos) {
        properties.mass = 0.35;
        properties.structural_strength = 0.4;
    } else if (containsAny(name, {"wool", "cloth", "carpet"})) {
        properties.mass = 0.18;
        properties.structural_strength = 0.15;
        properties.watertight = false;
    }
    return properties;
}

Vec3d angularPointVelocity(const DynamicConstruct &construct, const Vec3d &world_point) noexcept
{
    const Vec3d offset = world_point - construct.transform().position;
    const double omega = construct.yawVelocity();
    // ConstructTransform uses a positive-yaw matrix whose derivative at yaw zero
    // is (-z, 0, +x), hence this sign convention.
    return {-omega * offset.z, 0.0, omega * offset.x};
}

struct ComponentWork {
    ConstructStructuralComponent component;
    std::unordered_map<std::int32_t, double> solid_layers;
    std::unordered_map<std::int32_t, double> enclosed_layers;
};

std::size_t choosePrimary(const std::vector<ComponentWork> &components)
{
    std::size_t best = 0;
    for (std::size_t index = 1; index < components.size(); ++index) {
        const auto &candidate = components[index].component;
        const auto &current = components[best].component;
        if (candidate.control_nodes != current.control_nodes) {
            if (candidate.control_nodes > current.control_nodes)
                best = index;
            continue;
        }
        if (std::abs(candidate.structural_strength - current.structural_strength) > EPSILON) {
            if (candidate.structural_strength > current.structural_strength)
                best = index;
            continue;
        }
        if (std::abs(candidate.mass - current.mass) > EPSILON) {
            if (candidate.mass > current.mass)
                best = index;
            continue;
        }
        if (candidate.nodes.size() != current.nodes.size()) {
            if (candidate.nodes.size() > current.nodes.size())
                best = index;
            continue;
        }
        const auto candidate_min = *std::min_element(candidate.nodes.begin(), candidate.nodes.end(),
            positionLess);
        const auto current_min = *std::min_element(current.nodes.begin(), current.nodes.end(),
            positionLess);
        if (positionLess(candidate_min, current_min))
            best = index;
    }
    return best;
}

void calculateEnclosure(ComponentWork &work,
    const std::unordered_set<LocalNodePos, LocalNodePosHash> &occupied,
    std::size_t maximum_cells)
{
    const LocalBounds &bounds = work.component.bounds;
    if (!bounds.valid)
        return;
    const std::int64_t size_x = static_cast<std::int64_t>(bounds.max.x) - bounds.min.x + 1;
    const std::int64_t size_y = static_cast<std::int64_t>(bounds.max.y) - bounds.min.y + 1;
    const std::int64_t size_z = static_cast<std::int64_t>(bounds.max.z) - bounds.min.z + 1;
    if (size_x <= 0 || size_y <= 0 || size_z <= 0)
        return;
    const std::uint64_t cells = static_cast<std::uint64_t>(size_x) *
        static_cast<std::uint64_t>(size_y) * static_cast<std::uint64_t>(size_z);
    if (cells > maximum_cells)
        return;

    std::unordered_set<LocalNodePos, LocalNodePosHash> outside;
    std::deque<LocalNodePos> queue;
    auto enqueue = [&](const LocalNodePos &position) {
        if (occupied.find(position) != occupied.end())
            return;
        if (outside.insert(position).second)
            queue.push_back(position);
    };
    for (std::int32_t x = bounds.min.x; x <= bounds.max.x; ++x) {
        for (std::int32_t y = bounds.min.y; y <= bounds.max.y; ++y) {
            enqueue({x, y, bounds.min.z});
            enqueue({x, y, bounds.max.z});
        }
    }
    for (std::int32_t z = bounds.min.z; z <= bounds.max.z; ++z) {
        for (std::int32_t y = bounds.min.y; y <= bounds.max.y; ++y) {
            enqueue({bounds.min.x, y, z});
            enqueue({bounds.max.x, y, z});
        }
    }
    for (std::int32_t x = bounds.min.x; x <= bounds.max.x; ++x) {
        for (std::int32_t z = bounds.min.z; z <= bounds.max.z; ++z) {
            enqueue({x, bounds.min.y, z});
            enqueue({x, bounds.max.y, z});
        }
    }
    while (!queue.empty()) {
        const LocalNodePos current = queue.front();
        queue.pop_front();
        for (const auto &direction : DIRECTIONS) {
            const LocalNodePos next = add(current, direction);
            if (next.x < bounds.min.x || next.x > bounds.max.x ||
                next.y < bounds.min.y || next.y > bounds.max.y ||
                next.z < bounds.min.z || next.z > bounds.max.z)
                continue;
            enqueue(next);
        }
    }
    for (std::int32_t x = bounds.min.x; x <= bounds.max.x; ++x) {
        for (std::int32_t y = bounds.min.y; y <= bounds.max.y; ++y) {
            for (std::int32_t z = bounds.min.z; z <= bounds.max.z; ++z) {
                const LocalNodePos position{x, y, z};
                if (occupied.find(position) == occupied.end() &&
                    outside.find(position) == outside.end()) {
                    work.component.enclosed_volume += 1.0;
                    work.enclosed_layers[y] += 1.0;
                }
            }
        }
    }
}

} // namespace

const char *constructStructuralRoleName(ConstructStructuralRole role) noexcept
{
    switch (role) {
    case ConstructStructuralRole::Vessel: return "vessel";
    case ConstructStructuralRole::Primary: return "primary";
    case ConstructStructuralRole::Fragment: return "fragment";
    case ConstructStructuralRole::Wreck: return "wreck";
    }
    return "vessel";
}

const char *constructStructuralEventName(ConstructStructuralEventKind kind) noexcept
{
    switch (kind) {
    case ConstructStructuralEventKind::Updated: return "updated";
    case ConstructStructuralEventKind::Split: return "split";
    case ConstructStructuralEventKind::Sinking: return "sinking";
    case ConstructStructuralEventKind::Sunk: return "sunk";
    }
    return "updated";
}

ConstructStructureEngine::ConstructStructureEngine(ConstructRegistry &registry) :
    m_registry(registry)
{
}

void ConstructStructureEngine::setMaterialResolver(MaterialResolver resolver)
{
    m_material_resolver = std::move(resolver);
}

ConstructMaterialProperties ConstructStructureEngine::materialFor(const ConstructNode &node) const
{
    ConstructMaterialProperties properties = m_material_resolver ?
        m_material_resolver(node) : defaultMaterial(node);
    properties.mass = std::max(0.001, properties.mass);
    properties.displaced_volume = std::max(0.0, properties.displaced_volume);
    properties.structural_strength = std::max(0.0, properties.structural_strength);
    return properties;
}

ConstructStructuralAnalysis ConstructStructureEngine::analyse(
    const DynamicConstruct &construct, const ConstructStructuralConfig &config) const
{
    ConstructStructuralAnalysis result;
    result.construct_id = construct.id();
    const auto entries = construct.nodes();
    if (entries.empty())
        return result;

    std::unordered_map<LocalNodePos, ConstructNode, LocalNodePosHash> nodes;
    std::unordered_map<LocalNodePos, ConstructMaterialProperties, LocalNodePosHash> properties;
    std::unordered_set<LocalNodePos, LocalNodePosHash> structural;
    nodes.reserve(entries.size());
    properties.reserve(entries.size());
    for (const auto &entry : entries) {
        nodes.emplace(entry.position, entry.node);
        const auto material = materialFor(entry.node);
        properties.emplace(entry.position, material);
        if (material.structural)
            structural.insert(entry.position);
    }
    if (structural.empty()) {
        for (const auto &entry : entries)
            structural.insert(entry.position);
    }

    std::vector<ComponentWork> work;
    std::unordered_map<LocalNodePos, std::size_t, LocalNodePosHash> assignments;
    std::unordered_set<LocalNodePos, LocalNodePosHash> visited;
    for (const auto &start : structural) {
        if (!visited.insert(start).second)
            continue;
        work.emplace_back();
        const std::size_t component_index = work.size() - 1;
        std::deque<LocalNodePos> queue{start};
        while (!queue.empty()) {
            const LocalNodePos current = queue.front();
            queue.pop_front();
            work.back().component.nodes.push_back(current);
            assignments[current] = component_index;
            for (const auto &direction : DIRECTIONS) {
                const LocalNodePos next = add(current, direction);
                if (structural.find(next) != structural.end() && visited.insert(next).second)
                    queue.push_back(next);
            }
        }
    }

    // Decorative/non-load-bearing clusters attach to one adjacent structural
    // component, but cannot bridge two structural components together.
    std::unordered_set<LocalNodePos, LocalNodePosHash> nonstructural_visited;
    for (const auto &entry : entries) {
        if (structural.find(entry.position) != structural.end() ||
            !nonstructural_visited.insert(entry.position).second)
            continue;
        std::vector<LocalNodePos> cluster;
        std::set<std::size_t> adjacent_components;
        std::deque<LocalNodePos> queue{entry.position};
        while (!queue.empty()) {
            const LocalNodePos current = queue.front();
            queue.pop_front();
            cluster.push_back(current);
            for (const auto &direction : DIRECTIONS) {
                const LocalNodePos next = add(current, direction);
                const auto assigned = assignments.find(next);
                if (assigned != assignments.end()) {
                    adjacent_components.insert(assigned->second);
                    continue;
                }
                if (nodes.find(next) != nodes.end() && structural.find(next) == structural.end() &&
                    nonstructural_visited.insert(next).second)
                    queue.push_back(next);
            }
        }
        std::size_t target = 0;
        if (adjacent_components.empty()) {
            work.emplace_back();
            target = work.size() - 1;
        } else {
            target = *std::max_element(adjacent_components.begin(), adjacent_components.end(),
                [&work](std::size_t left, std::size_t right) {
                    return work[left].component.nodes.size() < work[right].component.nodes.size();
                });
        }
        for (const auto &position : cluster) {
            work[target].component.nodes.push_back(position);
            assignments[position] = target;
        }
    }

    for (auto &component_work : work) {
        auto &component = component_work.component;
        std::unordered_set<LocalNodePos, LocalNodePosHash> occupied;
        occupied.reserve(component.nodes.size());
        Vec3d weighted_position{};
        for (const auto &position : component.nodes) {
            occupied.insert(position);
            extendBounds(component.bounds, position);
            const auto &material = properties.at(position);
            component.mass += material.mass;
            component.solid_displaced_volume += material.displaced_volume;
            component.structural_strength += material.structural_strength;
            if (material.control)
                ++component.control_nodes;
            if (material.watertight)
                ++component.watertight_nodes;
            component_work.solid_layers[position.y] += material.displaced_volume;
            weighted_position += Vec3d{position.x + 0.5, position.y + 0.5, position.z + 0.5} *
                material.mass;
        }
        if (component.mass > EPSILON)
            component.centre_of_mass = weighted_position * (1.0 / component.mass);
        calculateEnclosure(component_work, occupied, config.maximum_enclosure_cells);
        std::sort(component.nodes.begin(), component.nodes.end(), positionLess);
    }
    if (!work.empty())
        result.primary_component = choosePrimary(work);
    result.components.reserve(work.size());
    for (auto &component : work)
        result.components.push_back(std::move(component.component));
    return result;
}

std::vector<ConstructStructuralEvent> ConstructStructureEngine::splitConstruct(
    DynamicConstruct &construct, const ConstructStructuralAnalysis &analysis,
    const ConstructStructuralConfig &config)
{
    std::vector<ConstructStructuralEvent> events;
    if (analysis.components.size() <= 1)
        return events;
    const std::size_t primary_index = std::min(analysis.primary_component,
        analysis.components.size() - 1);
    auto &parent_runtime = m_states[construct.id()];
    if (parent_runtime.public_state.source_construct_id == 0)
        parent_runtime.public_state.source_construct_id = construct.id();
    parent_runtime.public_state.role = ConstructStructuralRole::Primary;

    ConstructStructuralEvent event;
    event.kind = ConstructStructuralEventKind::Split;
    event.construct_id = construct.id();
    event.source_construct_id = parent_runtime.public_state.source_construct_id;

    for (std::size_t component_index = 0; component_index < analysis.components.size();
         ++component_index) {
        if (component_index == primary_index)
            continue;
        const auto &component = analysis.components[component_index];
        if (component.nodes.size() < config.minimum_fragment_nodes)
            continue;

        LocalNodePos pivot{};
        if (config.recenter_fragments) {
            pivot = {
                static_cast<std::int32_t>(std::llround(component.centre_of_mass.x - 0.5)),
                static_cast<std::int32_t>(std::llround(component.centre_of_mass.y - 0.5)),
                static_cast<std::int32_t>(std::llround(component.centre_of_mass.z - 0.5)),
            };
        }
        const Vec3d pivot_world = construct.transform().localToWorld(
            {static_cast<double>(pivot.x), static_cast<double>(pivot.y),
                static_cast<double>(pivot.z)});
        auto fragment = m_registry.create();
        fragment->setOwner(construct.owner());
        fragment->setTransform({pivot_world, construct.transform().yaw_radians});
        fragment->setLinearVelocity(construct.linearVelocity() +
            angularPointVelocity(construct, pivot_world));
        fragment->setYawVelocity(construct.yawVelocity());

        for (const auto &old_position : component.nodes) {
            const ConstructNode *node = construct.getNode(old_position);
            if (!node)
                continue;
            const LocalNodePos new_position = subtract(old_position, pivot);
            fragment->setNode(new_position, *node);
            if (const ConstructNodeState *state = construct.getNodeState(old_position))
                fragment->setNodeState(new_position, *state);
        }
        std::vector<LocalNodePos> moved_liquids;
        for (const auto &entry : construct.liquids()) {
            const auto &position = entry.position;
            if (!component.bounds.valid ||
                    position.x < component.bounds.min.x - 1 ||
                    position.x > component.bounds.max.x + 1 ||
                    position.y < component.bounds.min.y - 1 ||
                    position.y > component.bounds.max.y + 1 ||
                    position.z < component.bounds.min.z - 1 ||
                    position.z > component.bounds.max.z + 1)
                continue;
            if (fragment->setLiquid(subtract(position, pivot), entry.liquid))
                moved_liquids.push_back(position);
        }
        for (const auto &old_position : component.nodes) {
            if (construct.removeNode(old_position))
                ++event.moved_nodes;
        }
        for (const auto &position : moved_liquids)
            construct.removeLiquid(position);

        RuntimeState fragment_runtime;
        fragment_runtime.public_state.construct_id = fragment->id();
        fragment_runtime.public_state.source_construct_id = event.source_construct_id;
        fragment_runtime.public_state.role = component.control_nodes > 0 ?
            ConstructStructuralRole::Wreck : ConstructStructuralRole::Fragment;
        fragment_runtime.initial_strength = std::max(component.structural_strength, EPSILON);
        fragment_runtime.had_control = component.control_nodes > 0;
        m_states[fragment->id()] = fragment_runtime;
        event.fragment_ids.push_back(fragment->id());
    }
    if (!event.fragment_ids.empty())
        events.push_back(std::move(event));
    return events;
}

ConstructStructuralState ConstructStructureEngine::updateState(DynamicConstruct &construct,
    const ConstructStructuralAnalysis &analysis, double delta_seconds,
    const ConstructStructuralConfig &config)
{
    auto &runtime = m_states[construct.id()];
    auto &state = runtime.public_state;
    state.construct_id = construct.id();
    if (state.source_construct_id == 0)
        state.source_construct_id = construct.id();
    state.node_revision = construct.nodeRevision();
    state.component_count = analysis.components.size();
    state.node_count = construct.nodeCount();
    state.control_nodes = 0;
    state.total_mass = 0.0;
    state.solid_displaced_volume = 0.0;
    state.enclosed_volume = 0.0;
    double current_strength = 0.0;
    for (const auto &component : analysis.components) {
        state.control_nodes += component.control_nodes;
        state.total_mass += component.mass;
        state.solid_displaced_volume += component.solid_displaced_volume;
        state.enclosed_volume += component.enclosed_volume;
        current_strength += component.structural_strength;
    }
    if (runtime.initial_strength <= EPSILON)
        runtime.initial_strength = std::max(current_strength, EPSILON);
    runtime.had_control = runtime.had_control || state.control_nodes > 0;
    if (runtime.had_control && state.control_nodes == 0 &&
        state.role != ConstructStructuralRole::Fragment)
        state.role = ConstructStructuralRole::Wreck;
    state.integrity = std::clamp(current_strength / runtime.initial_strength, 0.0, 1.0);

    if (runtime.previous_enclosed_volume > state.enclosed_volume + 0.5 &&
        runtime.previous_enclosed_volume > EPSILON) {
        const double lost_fraction = (runtime.previous_enclosed_volume - state.enclosed_volume) /
            runtime.previous_enclosed_volume;
        state.flooded_fraction = std::max(state.flooded_fraction,
            std::clamp(lost_fraction, 0.0, 1.0));
    }
    runtime.previous_enclosed_volume = state.enclosed_volume;
    if (state.breach_count > 0 && delta_seconds > 0.0) {
        const double denominator = std::max(1.0,
            state.solid_displaced_volume + state.enclosed_volume);
        state.flooded_fraction = std::clamp(state.flooded_fraction +
            config.flooding_rate * static_cast<double>(state.breach_count) *
                delta_seconds / std::sqrt(denominator), 0.0, 1.0);
    }

    // Reconstruct per-y volume layers. analyse() intentionally returns a compact
    // public result, so this loop remains deterministic and cheap compared with
    // the connectivity pass.
    std::unordered_map<std::int32_t, double> solid_layers;
    for (const auto &entry : construct.nodes())
        solid_layers[entry.position.y] += materialFor(entry.node).displaced_volume;
    std::unordered_map<std::int32_t, double> enclosed_layers;
    if (analysis.components.size() == 1 && analysis.components.front().enclosed_volume > 0.0) {
        const auto &component = analysis.components.front();
        const std::int32_t layers = component.bounds.max.y - component.bounds.min.y + 1;
        const double per_layer = layers > 0 ? component.enclosed_volume / layers : 0.0;
        for (std::int32_t y = component.bounds.min.y; y <= component.bounds.max.y; ++y)
            enclosed_layers[y] = per_layer;
    }
    auto submergedFraction = [&](std::int32_t local_y) {
        const double bottom = construct.transform().position.y + static_cast<double>(local_y);
        return std::clamp(config.water_level - bottom, 0.0, 1.0);
    };
    double submerged_solid = 0.0;
    double submerged_enclosed = 0.0;
    for (const auto &[y, volume] : solid_layers)
        submerged_solid += volume * submergedFraction(y);
    for (const auto &[y, volume] : enclosed_layers)
        submerged_enclosed += volume * submergedFraction(y);
    state.submerged_volume = submerged_solid + submerged_enclosed;
    state.effective_displaced_volume = submerged_solid +
        submerged_enclosed * (1.0 - state.flooded_fraction);
    state.buoyancy_force = state.effective_displaced_volume *
        config.fluid_density * config.gravity;
    state.weight_force = state.total_mass * config.gravity;
    state.vertical_acceleration = state.total_mass > EPSILON ?
        (state.buoyancy_force - state.weight_force) / state.total_mass : -config.gravity;
    state.vertical_acceleration = std::clamp(state.vertical_acceleration,
        -config.maximum_vertical_acceleration, config.maximum_vertical_acceleration);
    state.afloat = state.buoyancy_force + EPSILON >= state.weight_force;
    state.sinking = !state.afloat || state.flooded_fraction >= 0.75;
    state.sunk = construct.transform().position.y < config.water_level - config.sink_depth;

    const bool fragment_physics = state.role == ConstructStructuralRole::Fragment ||
        state.role == ConstructStructuralRole::Wreck;
    const bool primary_physics = state.role == ConstructStructuralRole::Primary;
    if (delta_seconds > 0.0 && ((fragment_physics && config.apply_fragment_physics) ||
            (primary_physics && config.apply_primary_physics))) {
        Vec3d velocity = construct.linearVelocity();
        velocity.y += state.vertical_acceleration * delta_seconds;
        const double total_volume = std::max(1.0,
            state.solid_displaced_volume + state.enclosed_volume);
        const double submerged_ratio = std::clamp(state.submerged_volume / total_volume, 0.0, 1.0);
        const double drag = std::exp(-config.linear_water_drag * submerged_ratio * delta_seconds);
        velocity.x *= drag;
        velocity.z *= drag;
        velocity.y *= std::exp(-config.linear_water_drag * 0.5 * submerged_ratio * delta_seconds);
        construct.setLinearVelocity(velocity);
        construct.setYawVelocity(construct.yawVelocity() *
            std::exp(-config.angular_water_drag * submerged_ratio * delta_seconds));
        construct.step(delta_seconds);
    }
    return state;
}

std::vector<ConstructStructuralEvent> ConstructStructureEngine::processConstruct(
    const std::shared_ptr<DynamicConstruct> &construct, double delta_seconds,
    const ConstructStructuralConfig &config, bool force_split)
{
    std::vector<ConstructStructuralEvent> events;
    if (!construct || construct->empty())
        return events;
    auto analysis = analyse(*construct, config);
    auto &runtime = m_states[construct->id()];
    const bool revision_changed = runtime.public_state.node_revision != construct->nodeRevision();
    if (config.split_enabled && analysis.components.size() > 1 &&
        (force_split || revision_changed || runtime.public_state.component_count == 0)) {
        auto split_events = splitConstruct(*construct, analysis, config);
        events.insert(events.end(), split_events.begin(), split_events.end());
        analysis = analyse(*construct, config);
    }
    const bool was_sinking = runtime.public_state.sinking;
    const bool was_sunk = runtime.public_state.sunk;
    const auto state_value = updateState(*construct, analysis, delta_seconds, config);
    for (auto &event : events) {
        if (event.kind == ConstructStructuralEventKind::Split)
            event.state = state_value;
    }
    ConstructStructuralEvent update;
    update.kind = ConstructStructuralEventKind::Updated;
    update.construct_id = construct->id();
    update.source_construct_id = state_value.source_construct_id;
    update.state = state_value;
    events.push_back(update);
    if (!was_sinking && state_value.sinking) {
        update.kind = ConstructStructuralEventKind::Sinking;
        events.push_back(update);
    }
    if (!was_sunk && state_value.sunk) {
        update.kind = ConstructStructuralEventKind::Sunk;
        events.push_back(update);
    }
    return events;
}

std::vector<ConstructStructuralEvent> ConstructStructureEngine::step(
    double delta_seconds, const ConstructStructuralConfig &config)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0)
        throw std::invalid_argument("structural delta time must be finite and non-negative");
    std::vector<ConstructStructuralEvent> events;
    const auto constructs = m_registry.snapshot();
    for (const auto &construct : constructs) {
        auto construct_events = processConstruct(construct, delta_seconds, config, false);
        events.insert(events.end(), construct_events.begin(), construct_events.end());
    }
    for (auto iterator = m_states.begin(); iterator != m_states.end();) {
        if (!m_registry.find(iterator->first))
            iterator = m_states.erase(iterator);
        else
            ++iterator;
    }
    return events;
}

std::vector<ConstructStructuralEvent> ConstructStructureEngine::forceSplit(
    ConstructId construct_id, const ConstructStructuralConfig &config)
{
    const auto construct = m_registry.find(construct_id);
    if (!construct)
        return {};
    return processConstruct(construct, 0.0, config, true);
}

bool ConstructStructureEngine::recordBreaches(ConstructId construct_id, std::size_t count)
{
    if (!m_registry.find(construct_id) || count == 0)
        return false;
    auto &state_value = m_states[construct_id].public_state;
    state_value.construct_id = construct_id;
    if (state_value.source_construct_id == 0)
        state_value.source_construct_id = construct_id;
    state_value.breach_count += count;
    return true;
}

bool ConstructStructureEngine::setFloodedFraction(ConstructId construct_id, double fraction)
{
    if (!m_registry.find(construct_id) || !std::isfinite(fraction))
        return false;
    auto &state_value = m_states[construct_id].public_state;
    state_value.construct_id = construct_id;
    if (state_value.source_construct_id == 0)
        state_value.source_construct_id = construct_id;
    state_value.flooded_fraction = std::clamp(fraction, 0.0, 1.0);
    return true;
}

bool ConstructStructureEngine::remove(ConstructId construct_id)
{
    return m_states.erase(construct_id) != 0;
}

void ConstructStructureEngine::clear()
{
    m_states.clear();
}

std::optional<ConstructStructuralState> ConstructStructureEngine::state(
    ConstructId construct_id) const
{
    const auto iterator = m_states.find(construct_id);
    if (iterator == m_states.end())
        return std::nullopt;
    return iterator->second.public_state;
}

std::vector<ConstructStructuralState> ConstructStructureEngine::states() const
{
    std::vector<ConstructStructuralState> result;
    result.reserve(m_states.size());
    for (const auto &[id, runtime] : m_states) {
        (void)id;
        result.push_back(runtime.public_state);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.construct_id < right.construct_id;
    });
    return result;
}

} // namespace navycraft

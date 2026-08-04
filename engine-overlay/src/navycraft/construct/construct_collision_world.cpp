// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_collision_world.h"

#include <algorithm>
#include <cmath>

namespace navycraft {
namespace {

Aabb3d merged(const Aabb3d &a, const Aabb3d &b) noexcept
{
    return {
        {std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y),
            std::min(a.min.z, b.min.z)},
        {std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y),
            std::max(a.max.z, b.max.z)},
    };
}

Aabb3d expanded(const Aabb3d &box, double amount) noexcept
{
    amount = std::max(0.0, amount);
    return {
        {box.min.x - amount, box.min.y - amount, box.min.z - amount},
        {box.max.x + amount, box.max.y + amount, box.max.z + amount},
    };
}

double horizontalRadius(const Aabb3d &box, const Vec3d &origin) noexcept
{
    const double x = std::max(std::abs(box.min.x - origin.x),
        std::abs(box.max.x - origin.x));
    const double z = std::max(std::abs(box.min.z - origin.z),
        std::abs(box.max.z - origin.z));
    return std::sqrt(x * x + z * z);
}

Aabb3d predictedBounds(const DynamicConstruct &construct,
    const Aabb3d &current, double seconds) noexcept
{
    const double dt = std::max(0.0, seconds);
    const Vec3d linear_delta = construct.linearVelocity() * dt;
    Aabb3d result = merged(current, current.translated(linear_delta));

    // Rotation can move any point tangentially by radius * angle. Expanding
    // the horizontal bounds by that amount makes broad-phase rejection safe
    // without rebuilding per-node bounds for the predicted pose.
    const double radius = horizontalRadius(current, construct.transform().position);
    const double rotational_travel = radius * std::abs(construct.yawVelocity()) * dt;
    result.min.x -= rotational_travel;
    result.max.x += rotational_travel;
    result.min.z -= rotational_travel;
    result.max.z += rotational_travel;
    return result;
}

} // namespace

void ConstructCollisionWorld::clear() noexcept
{
    m_entries.clear();
    m_by_id.clear();
    m_stats = {};
}

void ConstructCollisionWorld::rebuild(
    const std::vector<const DynamicConstruct *> &constructs,
    double prediction_seconds)
{
    clear();
    prediction_seconds = std::clamp(prediction_seconds, 0.0, 1.0);
    m_entries.reserve(constructs.size());
    m_by_id.reserve(constructs.size());
    for (const DynamicConstruct *construct : constructs) {
        if (!construct || construct->empty())
            continue;
        Entry entry;
        entry.construct = construct;
        entry.current_bounds = ConstructGeometry::worldBounds(*construct);
        entry.predicted_bounds = predictedBounds(
            *construct, entry.current_bounds, prediction_seconds);
        m_entries.push_back(entry);
        m_by_id[construct->id()] = construct;
    }

    // Deterministic iteration keeps collision selection stable across clients.
    std::sort(m_entries.begin(), m_entries.end(),
        [](const Entry &a, const Entry &b) {
            return a.construct->id() < b.construct->id();
        });
    m_stats.construct_count = m_entries.size();
}

std::vector<const DynamicConstruct *> ConstructCollisionWorld::query(
    const Aabb3d &world_box,
    const Vec3d &desired_delta,
    double delta_seconds,
    ConstructId always_include) const
{
    std::vector<const DynamicConstruct *> result;
    if (!world_box.valid())
        return result;

    const double dt = std::clamp(
        std::isfinite(delta_seconds) ? delta_seconds : 0.0, 0.0, 1.0);
    Aabb3d query_bounds = merged(world_box, world_box.translated(desired_delta));
    const double motion_margin = std::sqrt(
        desired_delta.x * desired_delta.x + desired_delta.y * desired_delta.y +
        desired_delta.z * desired_delta.z) * 0.02 + dt * 0.05 + 0.02;
    query_bounds = expanded(query_bounds, motion_margin);

    result.reserve(m_entries.size());
    std::size_t rejected = 0;
    for (const Entry &entry : m_entries) {
        if (!entry.construct)
            continue;
        if (entry.construct->id() == always_include ||
                entry.predicted_bounds.intersects(query_bounds)) {
            result.push_back(entry.construct);
        } else {
            ++rejected;
        }
    }
    m_stats.last_query_candidates = result.size();
    m_stats.last_query_rejected = rejected;
    return result;
}

const DynamicConstruct *ConstructCollisionWorld::find(ConstructId id) const noexcept
{
    const auto found = m_by_id.find(id);
    return found == m_by_id.end() ? nullptr : found->second;
}

const ConstructCollisionWorldStats &ConstructCollisionWorld::stats() const noexcept
{
    return m_stats;
}

} // namespace navycraft

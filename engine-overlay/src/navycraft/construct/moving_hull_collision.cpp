// SPDX-License-Identifier: LGPL-2.1-or-later
#include "moving_hull_collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace navycraft {
namespace {
constexpr double EPSILON = 1e-8;

double dot(const Vec3d &left, const Vec3d &right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

double lengthSquared(const Vec3d &value) noexcept
{
    return dot(value, value);
}

HullContactKind classifyNormal(const Vec3d &normal) noexcept
{
    if (normal.y >= 0.55)
        return HullContactKind::Floor;
    if (normal.y <= -0.55)
        return HullContactKind::Ceiling;
    return HullContactKind::Wall;
}

void applyContactFlags(HullCollisionOutput &output, HullContactKind kind) noexcept
{
    switch (kind) {
    case HullContactKind::Floor:
        output.touching_ground = true;
        break;
    case HullContactKind::Ceiling:
        output.hit_ceiling = true;
        break;
    case HullContactKind::Wall:
        output.hit_wall = true;
        break;
    }
}

Vec3d removeInwardComponent(const Vec3d &value, const Vec3d &normal) noexcept
{
    const double inward = dot(value, normal);
    if (inward >= 0.0)
        return value;
    return value - normal * inward;
}

struct EarliestSweep {
    const DynamicConstruct *construct = nullptr;
    ConstructSweepHit hit{};
};

EarliestSweep findEarliestSweep(
    const std::vector<const DynamicConstruct *> &constructs,
    const Aabb3d &box,
    const Vec3d &delta,
    double delta_seconds,
    double maximum_step) noexcept
{
    EarliestSweep best;
    best.hit.allowed_fraction = 1.0;
    for (const DynamicConstruct *construct : constructs) {
        if (!construct || construct->empty())
            continue;
        const ConstructSweepHit candidate = ConstructGeometry::sweepWorldAabbDetailed(
            *construct, box, delta, delta_seconds, maximum_step);
        if (!candidate.collided)
            continue;
        if (!best.construct || candidate.allowed_fraction < best.hit.allowed_fraction) {
            best.construct = construct;
            best.hit = candidate;
        }
    }
    return best;
}

bool depenetrate(
    const std::vector<const DynamicConstruct *> &constructs,
    Aabb3d &box,
    HullCollisionOutput &output,
    double skin_width,
    std::size_t maximum_iterations) noexcept
{
    bool changed = false;
    for (std::size_t iteration = 0; iteration < maximum_iterations; ++iteration) {
        const DynamicConstruct *best_construct = nullptr;
        std::optional<ConstructOverlapHit> best_overlap;
        for (const DynamicConstruct *construct : constructs) {
            if (!construct || construct->empty())
                continue;
            const auto overlap = ConstructGeometry::overlapWorldAabb(*construct, box);
            if (!overlap)
                continue;
            if (!best_overlap || overlap->penetration_depth < best_overlap->penetration_depth) {
                best_construct = construct;
                best_overlap = overlap;
            }
        }
        if (!best_construct || !best_overlap)
            break;

        const Vec3d correction = best_overlap->world_normal *
            (best_overlap->penetration_depth + skin_width);
        box = box.translated(correction);
        output.position_correction += correction;
        output.allowed_delta += correction;
        output.collided = true;
        changed = true;

        HullCollisionContact contact;
        contact.construct_id = best_construct->id();
        contact.node_position = best_overlap->node_position;
        contact.world_normal = best_overlap->world_normal;
        contact.world_point = box.centre();
        contact.surface_velocity = ConstructGeometry::surfaceVelocity(
            *best_construct, contact.world_point);
        contact.kind = classifyNormal(contact.world_normal);
        output.contacts.push_back(contact);
        applyContactFlags(output, contact.kind);

        const Vec3d relative_velocity = output.velocity - contact.surface_velocity;
        output.velocity = removeInwardComponent(relative_velocity, contact.world_normal) +
            contact.surface_velocity;
    }
    return changed;
}

} // namespace

HullCollisionOutput MovingHullCollisionSolver::move(
    const std::vector<const DynamicConstruct *> &constructs,
    const HullCollisionInput &input) noexcept
{
    HullCollisionOutput output;
    output.velocity = input.velocity;
    if (!input.world_box.valid())
        return output;

    const double dt = std::isfinite(input.delta_seconds) ?
        std::max(0.0, input.delta_seconds) : 0.0;
    const double maximum_step = std::clamp(input.maximum_sweep_step, 0.02, 1.0);
    const double skin = std::clamp(input.skin_width, 0.0, 0.05);
    const std::size_t iterations = std::clamp<std::size_t>(
        input.maximum_iterations, 1U, 12U);

    Aabb3d box = input.world_box;
    (void)depenetrate(constructs, box, output, skin, iterations);

    Vec3d remaining = input.desired_delta;
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        if (lengthSquared(remaining) <= EPSILON * EPSILON)
            break;

        const EarliestSweep earliest = findEarliestSweep(
            constructs, box, remaining, dt, maximum_step);
        if (!earliest.construct) {
            output.allowed_delta += remaining;
            box = box.translated(remaining);
            remaining = {};
            break;
        }

        output.collided = true;
        const Vec3d travelled = remaining * earliest.hit.allowed_fraction;
        output.allowed_delta += travelled;
        box = box.translated(travelled);

        HullCollisionContact contact;
        contact.construct_id = earliest.hit.construct_id;
        contact.node_position = earliest.hit.node_position;
        contact.world_normal = earliest.hit.world_normal;
        contact.world_point = earliest.hit.world_point;
        contact.surface_velocity = earliest.hit.surface_velocity;
        contact.kind = classifyNormal(contact.world_normal);
        output.contacts.push_back(contact);
        applyContactFlags(output, contact.kind);

        const Vec3d relative_velocity = output.velocity - contact.surface_velocity;
        output.velocity = removeInwardComponent(relative_velocity, contact.world_normal) +
            contact.surface_velocity;

        const double leftover_fraction = std::max(0.0, 1.0 - earliest.hit.allowed_fraction);
        const Vec3d surface_delta = contact.surface_velocity * dt * leftover_fraction;
        const Vec3d relative_leftover = remaining * leftover_fraction - surface_delta;
        remaining = removeInwardComponent(relative_leftover, contact.world_normal) + surface_delta;

        if (skin > 0.0) {
            const Vec3d separation = contact.world_normal * skin;
            output.allowed_delta += separation;
            output.position_correction += separation;
            box = box.translated(separation);
        }
    }

    // A final depenetration catches a moving hull that entered the body during
    // the frame even when the body's own desired delta was zero.
    (void)depenetrate(constructs, box, output, skin, iterations);
    return output;
}

} // namespace navycraft

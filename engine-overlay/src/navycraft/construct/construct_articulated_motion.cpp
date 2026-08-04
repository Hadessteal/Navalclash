// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_articulated_motion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace navycraft {
namespace {
constexpr double EPSILON = 1e-8;

struct Obb {
    Vec3d centre{};
    std::array<Vec3d, 3> axis{};
    std::array<double, 3> half{{0.5, 0.5, 0.5}};
};

struct Overlap {
    Vec3d normal{};
    double depth = 0.0;
};

struct ArticulatedNode {
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    LocalNodePos node_position{};
    Obb box{};
};

double dot(const Vec3d &a, const Vec3d &b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3d cross(const Vec3d &a, const Vec3d &b) noexcept
{
    return {a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

double lengthSquared(const Vec3d &v) noexcept
{
    return dot(v, v);
}

Vec3d normalised(const Vec3d &v) noexcept
{
    const double magnitude = std::sqrt(lengthSquared(v));
    return magnitude > EPSILON ? v * (1.0 / magnitude) : Vec3d{};
}

Vec3d aabbCentre(const Aabb3d &box) noexcept
{
    return box.centre();
}

std::array<double, 3> aabbHalf(const Aabb3d &box) noexcept
{
    return {{(box.max.x - box.min.x) * 0.5,
        (box.max.y - box.min.y) * 0.5,
        (box.max.z - box.min.z) * 0.5}};
}

std::vector<ArticulatedNode> nodes(const ConstructArticulationEngine &engine) noexcept
{
    std::vector<ArticulatedNode> result;
    try {
        for (const auto &joint : engine.joints()) {
            if (!joint.definition.enabled || !joint.definition.collision_enabled)
                continue;
            const Vec3d axis_x = normalised(engine.transformDirectionWorld(
                joint.definition.id, {1.0, 0.0, 0.0}));
            const Vec3d axis_y = normalised(engine.transformDirectionWorld(
                joint.definition.id, {0.0, 1.0, 0.0}));
            const Vec3d axis_z = normalised(engine.transformDirectionWorld(
                joint.definition.id, {0.0, 0.0, 1.0}));
            for (const auto &position : joint.definition.nodes) {
                ArticulatedNode node;
                node.construct_id = joint.definition.construct_id;
                node.articulation_id = joint.definition.id;
                node.node_position = position;
                node.box.centre = engine.transformPointWorld(joint.definition.id,
                    {static_cast<double>(position.x) + 0.5,
                        static_cast<double>(position.y) + 0.5,
                        static_cast<double>(position.z) + 0.5});
                node.box.axis = {{axis_x, axis_y, axis_z}};
                result.push_back(node);
            }
        }
    } catch (...) {
        result.clear();
    }
    return result;
}

bool testAxis(const Vec3d &axis, const Vec3d &delta,
    const std::array<Vec3d, 3> &a_axes, const std::array<double, 3> &a_half,
    const std::array<Vec3d, 3> &b_axes, const std::array<double, 3> &b_half,
    double &best_depth, Vec3d &best_axis) noexcept
{
    const double magnitude_sq = lengthSquared(axis);
    if (magnitude_sq <= EPSILON * EPSILON)
        return true;
    const Vec3d n = axis * (1.0 / std::sqrt(magnitude_sq));
    double radius_a = 0.0;
    double radius_b = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        radius_a += a_half[i] * std::abs(dot(n, a_axes[i]));
        radius_b += b_half[i] * std::abs(dot(n, b_axes[i]));
    }
    const double separation = std::abs(dot(delta, n));
    const double depth = radius_a + radius_b - separation;
    if (depth < 0.0)
        return false;
    if (depth < best_depth) {
        best_depth = depth;
        best_axis = dot(delta, n) >= 0.0 ? n : n * -1.0;
    }
    return true;
}

std::optional<Overlap> overlapAabbObb(const Aabb3d &aabb, const Obb &obb) noexcept
{
    if (!aabb.valid())
        return std::nullopt;
    const std::array<Vec3d, 3> world_axes{{
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    const auto a_half = aabbHalf(aabb);
    const Vec3d delta = aabbCentre(aabb) - obb.centre;
    double best_depth = std::numeric_limits<double>::infinity();
    Vec3d best_axis{};
    for (const auto &axis : world_axes) {
        if (!testAxis(axis, delta, world_axes, a_half,
                obb.axis, obb.half, best_depth, best_axis))
            return std::nullopt;
    }
    for (const auto &axis : obb.axis) {
        if (!testAxis(axis, delta, world_axes, a_half,
                obb.axis, obb.half, best_depth, best_axis))
            return std::nullopt;
    }
    for (const auto &left : world_axes) {
        for (const auto &right : obb.axis) {
            if (!testAxis(cross(left, right), delta, world_axes, a_half,
                    obb.axis, obb.half, best_depth, best_axis))
                return std::nullopt;
        }
    }
    return Overlap{best_axis, best_depth};
}

HullContactKind classify(const Vec3d &normal) noexcept
{
    if (normal.y >= 0.55)
        return HullContactKind::Floor;
    if (normal.y <= -0.55)
        return HullContactKind::Ceiling;
    return HullContactKind::Wall;
}

Vec3d removeInward(const Vec3d &value, const Vec3d &normal) noexcept
{
    const double inward = dot(value, normal);
    return inward < 0.0 ? value - normal * inward : value;
}

bool segmentLocalCube(const Vec3d &start, const Vec3d &end,
    const LocalNodePos &position, double &entry_t, Vec3d &normal) noexcept
{
    const Vec3d minimum{static_cast<double>(position.x),
        static_cast<double>(position.y), static_cast<double>(position.z)};
    const Vec3d maximum{minimum.x + 1.0, minimum.y + 1.0, minimum.z + 1.0};
    const Vec3d direction = end - start;
    double t_min = 0.0;
    double t_max = 1.0;
    Vec3d hit_normal{};
    const std::array<double, 3> origin{{start.x, start.y, start.z}};
    const std::array<double, 3> delta{{direction.x, direction.y, direction.z}};
    const std::array<double, 3> mins{{minimum.x, minimum.y, minimum.z}};
    const std::array<double, 3> maxs{{maximum.x, maximum.y, maximum.z}};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (std::abs(delta[axis]) <= EPSILON) {
            if (origin[axis] < mins[axis] || origin[axis] > maxs[axis])
                return false;
            continue;
        }
        double first = (mins[axis] - origin[axis]) / delta[axis];
        double second = (maxs[axis] - origin[axis]) / delta[axis];
        double sign = -1.0;
        if (first > second) {
            std::swap(first, second);
            sign = 1.0;
        }
        if (first > t_min) {
            t_min = first;
            hit_normal = {};
            if (axis == 0) hit_normal.x = sign;
            if (axis == 1) hit_normal.y = sign;
            if (axis == 2) hit_normal.z = sign;
        }
        t_max = std::min(t_max, second);
        if (t_min > t_max)
            return false;
    }
    entry_t = t_min;
    normal = hit_normal;
    return t_min <= 1.0 && t_max >= 0.0;
}

std::optional<std::pair<ArticulatedNode, Overlap>> firstOverlap(
    const std::vector<ArticulatedNode> &all_nodes, const Aabb3d &box) noexcept
{
    std::optional<std::pair<ArticulatedNode, Overlap>> best;
    for (const auto &node : all_nodes) {
        const auto overlap = overlapAabbObb(box, node.box);
        if (!overlap)
            continue;
        if (!best || overlap->depth < best->second.depth)
            best = std::make_pair(node, *overlap);
    }
    return best;
}

} // namespace

std::optional<ArticulatedSupportHit> ConstructArticulatedMotion::findSupport(
    const ConstructArticulationEngine &engine,
    const Aabb3d &world_box,
    double support_drop,
    double support_penetration) noexcept
{
    if (!world_box.valid())
        return std::nullopt;
    support_drop = std::max(0.0, support_drop);
    support_penetration = std::max(0.0, support_penetration);
    const Vec3d ray_start{world_box.centre().x,
        world_box.min.y + support_penetration, world_box.centre().z};
    const Vec3d ray_end{ray_start.x,
        world_box.min.y - support_drop, ray_start.z};
    std::optional<ArticulatedSupportHit> best;
    try {
        for (const auto &joint : engine.joints()) {
            if (!joint.definition.enabled || !joint.definition.collision_enabled)
                continue;
            const Vec3d local_start = engine.inverseTransformPointWorld(
                joint.definition.id, ray_start);
            const Vec3d local_end = engine.inverseTransformPointWorld(
                joint.definition.id, ray_end);
            for (const auto &position : joint.definition.nodes) {
                double entry_t = 0.0;
                Vec3d local_normal{};
                if (!segmentLocalCube(local_start, local_end, position,
                        entry_t, local_normal))
                    continue;
                const Vec3d world_normal = normalised(engine.transformDirectionWorld(
                    joint.definition.id, local_normal));
                if (world_normal.y < 0.55)
                    continue;
                const Vec3d local_point = local_start +
                    (local_end - local_start) * entry_t;
                const Vec3d world_point = engine.transformPointWorld(
                    joint.definition.id, local_point);
                const double gap = world_box.min.y - world_point.y;
                if (gap < -support_penetration - 1e-5 || gap > support_drop + 1e-5)
                    continue;
                if (!best || std::abs(gap) < std::abs(best->vertical_gap)) {
                    ArticulatedSupportHit hit;
                    hit.construct_id = joint.definition.construct_id;
                    hit.articulation_id = joint.definition.id;
                    hit.node_position = position;
                    hit.local_anchor = local_point;
                    hit.world_point = world_point;
                    hit.world_normal = world_normal;
                    hit.surface_velocity = engine.pointVelocityWorld(
                        joint.definition.id, local_point);
                    hit.vertical_gap = gap;
                    best = hit;
                }
            }
        }
    } catch (...) {
        return std::nullopt;
    }
    return best;
}

Vec3d ConstructArticulatedMotion::contactWorldPoint(
    const ConstructArticulationEngine &engine,
    const ArticulatedPlatformContactState &contact) noexcept
{
    if (!contact.active || contact.articulation_id == 0)
        return {};
    try {
        return engine.transformPointWorld(contact.articulation_id, contact.local_anchor);
    } catch (...) {
        return {};
    }
}

ArticulatedMotionOutput ConstructArticulatedMotion::stepRider(
    ArticulatedRiderState &state,
    const ConstructArticulationEngine &engine,
    const ArticulatedMotionInput &input,
    double support_drop,
    double support_penetration,
    double contact_grace) noexcept
{
    ArticulatedMotionOutput output;
    output.position = input.position;
    output.velocity = input.velocity;
    const double dt = std::isfinite(input.delta_seconds) ?
        std::max(0.0, input.delta_seconds) : 0.0;
    const bool jump_edge = input.jump_pressed && !state.previous_jump_pressed;
    state.previous_jump_pressed = input.jump_pressed;

    if (state.contact.active) {
        const auto joint = engine.joint(state.contact.articulation_id);
        if (!joint || !joint->definition.enabled || !joint->definition.collision_enabled) {
            if (input.inherit_platform_velocity_on_detach) {
                output.velocity += state.last_surface_velocity;
                output.inherited_velocity = state.last_surface_velocity;
            }
            state.contact = {};
            state.last_surface_velocity = {};
            output.contact_ended = true;
        } else {
            const Vec3d world_anchor = contactWorldPoint(engine, state.contact);
            const Vec3d displacement = world_anchor - state.contact.last_world_anchor;
            const Vec3d surface_velocity = engine.pointVelocityWorld(
                state.contact.articulation_id, state.contact.local_anchor);
            output.position += displacement;
            output.platform_displacement = displacement;
            state.contact.last_world_anchor = world_anchor;
            state.last_surface_velocity = surface_velocity;
            if (jump_edge || input.detach_requested) {
                if (input.inherit_platform_velocity_on_detach) {
                    output.velocity += surface_velocity;
                    output.inherited_velocity = surface_velocity;
                }
                output.jumped = jump_edge;
                output.contact_ended = true;
                state.contact = {};
                state.last_surface_velocity = {};
                state.unsupported_seconds = 0.0;
                return output;
            }
            const Aabb3d moved_box = input.world_box.translated(displacement);
            const auto support = findSupport(engine, moved_box,
                support_drop, support_penetration);
            if (support && support->articulation_id == state.contact.articulation_id) {
                output.position.y -= support->vertical_gap;
                state.contact.local_anchor = support->local_anchor;
                state.contact.last_world_anchor = support->world_point;
                state.last_surface_velocity = support->surface_velocity;
                output.grounded = true;
                output.construct_id = support->construct_id;
                output.articulation_id = support->articulation_id;
                state.unsupported_seconds = 0.0;
                return output;
            }
            state.unsupported_seconds += dt;
            if (state.unsupported_seconds <= std::max(0.0, contact_grace)) {
                output.grounded = true;
                output.construct_id = state.contact.construct_id;
                output.articulation_id = state.contact.articulation_id;
                return output;
            }
            if (input.inherit_platform_velocity_on_detach) {
                output.velocity += surface_velocity;
                output.inherited_velocity = surface_velocity;
            }
            state.contact = {};
            state.last_surface_velocity = {};
            state.unsupported_seconds = 0.0;
            output.contact_ended = true;
        }
    }

    if (input.allow_contact_acquisition && !input.jump_pressed && input.velocity.y <= 0.5) {
        const auto support = findSupport(engine, input.world_box,
            support_drop, support_penetration);
        if (support) {
            output.position.y -= support->vertical_gap;
            state.contact.construct_id = support->construct_id;
            state.contact.articulation_id = support->articulation_id;
            state.contact.local_anchor = support->local_anchor;
            state.contact.last_world_anchor = support->world_point;
            state.contact.active = true;
            state.last_surface_velocity = support->surface_velocity;
            state.unsupported_seconds = 0.0;
            output.grounded = true;
            output.construct_id = support->construct_id;
            output.articulation_id = support->articulation_id;
            output.contact_started = true;
        }
    }
    return output;
}

ArticulatedMotionOutput ConstructArticulatedMotion::move(
    const ConstructArticulationEngine &engine,
    const ArticulatedMotionInput &input) noexcept
{
    ArticulatedMotionOutput output;
    output.position = input.position;
    output.velocity = input.velocity;
    if (!input.world_box.valid())
        return output;
    const auto all_nodes = nodes(engine);
    if (all_nodes.empty()) {
        output.allowed_delta = input.desired_delta;
        output.position += input.desired_delta;
        return output;
    }
    Aabb3d box = input.world_box;
    const double skin = std::clamp(input.skin_width, 0.0, 0.05);
    const std::size_t iterations = std::clamp<std::size_t>(
        input.maximum_iterations, 1U, 12U);

    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        const auto overlap = firstOverlap(all_nodes, box);
        if (!overlap)
            break;
        const Vec3d correction = overlap->second.normal *
            (overlap->second.depth + skin);
        box = box.translated(correction);
        output.position += correction;
        output.allowed_delta += correction;
        output.collided = true;
        HullCollisionContact contact;
        contact.construct_id = overlap->first.construct_id;
        contact.node_position = overlap->first.node_position;
        contact.world_normal = overlap->second.normal;
        contact.world_point = box.centre();
        try {
            const Vec3d local = engine.inverseTransformPointWorld(
                overlap->first.articulation_id, contact.world_point);
            contact.surface_velocity = engine.pointVelocityWorld(
                overlap->first.articulation_id, local);
        } catch (...) {
            contact.surface_velocity = {};
        }
        contact.kind = classify(contact.world_normal);
        output.contacts.push_back(contact);
        output.grounded = output.grounded || contact.kind == HullContactKind::Floor;
        output.hit_wall = output.hit_wall || contact.kind == HullContactKind::Wall;
        output.hit_ceiling = output.hit_ceiling || contact.kind == HullContactKind::Ceiling;
        output.velocity = removeInward(output.velocity - contact.surface_velocity,
            contact.world_normal) + contact.surface_velocity;
    }

    Vec3d remaining = input.desired_delta;
    const double distance = std::sqrt(lengthSquared(remaining));
    const double step_size = std::clamp(input.maximum_sweep_step, 0.02, 0.5);
    const std::size_t steps = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(distance / step_size)));
    for (std::size_t step = 0; step < steps; ++step) {
        const Vec3d increment = remaining * (1.0 / static_cast<double>(steps - step));
        const Aabb3d candidate = box.translated(increment);
        const auto overlap = firstOverlap(all_nodes, candidate);
        if (!overlap) {
            box = candidate;
            output.position += increment;
            output.allowed_delta += increment;
            remaining = remaining - increment;
            continue;
        }
        output.collided = true;
        HullCollisionContact contact;
        contact.construct_id = overlap->first.construct_id;
        contact.node_position = overlap->first.node_position;
        contact.world_normal = overlap->second.normal;
        contact.world_point = candidate.centre();
        try {
            const Vec3d local = engine.inverseTransformPointWorld(
                overlap->first.articulation_id, contact.world_point);
            contact.surface_velocity = engine.pointVelocityWorld(
                overlap->first.articulation_id, local);
        } catch (...) {
            contact.surface_velocity = {};
        }
        contact.kind = classify(contact.world_normal);
        output.contacts.push_back(contact);
        output.grounded = output.grounded || contact.kind == HullContactKind::Floor;
        output.hit_wall = output.hit_wall || contact.kind == HullContactKind::Wall;
        output.hit_ceiling = output.hit_ceiling || contact.kind == HullContactKind::Ceiling;
        output.velocity = removeInward(output.velocity - contact.surface_velocity,
            contact.world_normal) + contact.surface_velocity;
        const Vec3d relative = increment - contact.surface_velocity * input.delta_seconds;
        const Vec3d slide = removeInward(relative, contact.world_normal) +
            contact.surface_velocity * input.delta_seconds;
        if (lengthSquared(slide) < lengthSquared(increment) - EPSILON) {
            remaining = remaining - increment + slide;
        } else {
            remaining = remaining - increment;
        }
    }
    return output;
}

} // namespace navycraft

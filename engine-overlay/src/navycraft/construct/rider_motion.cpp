// SPDX-License-Identifier: LGPL-2.1-or-later
#include "rider_motion.h"

#include <algorithm>
#include <cmath>

namespace navycraft {
namespace {

double magnitude(const Vec3d &value) noexcept
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

const DynamicConstruct *findConstruct(
    const std::vector<const DynamicConstruct *> &constructs,
    ConstructId id) noexcept
{
    for (const auto &construct : constructs) {
        if (construct && construct->id() == id)
            return construct;
    }
    return nullptr;
}

std::optional<ConstructSupportHit> findBestSupport(
    const std::vector<const DynamicConstruct *> &constructs,
    const Aabb3d &world_box,
    double support_drop,
    double support_penetration) noexcept
{
    std::optional<ConstructSupportHit> best;
    for (const auto &construct : constructs) {
        if (!construct)
            continue;
        const auto support = ConstructGeometry::findSupport(
            *construct, world_box, support_drop, support_penetration);
        if (!support)
            continue;
        if (!best || std::abs(support->vertical_gap) < std::abs(best->vertical_gap))
            best = support;
    }
    return best;
}

} // namespace

RiderMotionOutput RiderMotionSolver::step(
    RiderMotionState &state,
    const std::vector<const DynamicConstruct *> &constructs,
    const RiderMotionInput &input,
    double support_drop,
    double support_penetration,
    double contact_grace) noexcept
{
    RiderMotionOutput output;
    output.position = input.position;
    output.velocity = input.velocity;

    const double dt = input.delta_seconds > 0.0 && std::isfinite(input.delta_seconds) ?
        input.delta_seconds : 0.0;
    const bool jump_edge = input.jump_pressed && !state.previous_jump_pressed;
    state.previous_jump_pressed = input.jump_pressed;

    const DynamicConstruct *active_construct = nullptr;
    if (state.contact.active)
        active_construct = findConstruct(constructs, state.contact.construct_id);
    if (state.contact.active && !active_construct) {
        if (input.inherit_platform_velocity_on_detach) {
            output.velocity += state.last_surface_velocity;
            output.inherited_velocity = state.last_surface_velocity;
        }
        PlatformContact::clear(state.contact);
        state.last_surface_velocity = {};
        state.unsupported_seconds = 0.0;
        state.has_last_support_node = false;
        output.contact_ended = true;
    }

    if (state.contact.active && active_construct) {
        const PlatformMotion platform = PlatformContact::update(
            state.contact, *active_construct, dt);
        output.platform_displacement = platform.displacement;
        state.last_surface_velocity = platform.surface_velocity;
        output.position += platform.displacement;
        const Aabb3d carried_box = input.world_box.translated(platform.displacement);

        if (jump_edge || input.detach_requested) {
            if (input.inherit_platform_velocity_on_detach) {
                output.velocity += platform.surface_velocity;
                output.inherited_velocity = platform.surface_velocity;
            }
            output.jumped = jump_edge;
            output.contact_ended = true;
            PlatformContact::clear(state.contact);
            state.last_surface_velocity = {};
            state.unsupported_seconds = 0.0;
            state.has_last_support_node = false;
            return output;
        }

        const auto support = ConstructGeometry::findSupport(
            *active_construct, carried_box, support_drop, support_penetration,
            state.has_last_support_node ?
                std::optional<LocalNodePos>(state.last_support_node) : std::nullopt);
        if (support) {
            double correction_y = -support->vertical_gap;
            if (std::abs(correction_y) <= SUPPORT_CORRECTION_SLOP) {
                correction_y = 0.0;
            } else {
                correction_y = std::clamp(correction_y,
                    -MAX_SUPPORT_CORRECTION_PER_FRAME,
                    MAX_SUPPORT_CORRECTION_PER_FRAME);
            }
            output.position.y += correction_y;
            const Vec3d foot_anchor{
                carried_box.centre().x,
                carried_box.min.y + correction_y,
                carried_box.centre().z,
            };
            PlatformContact::reanchor(state.contact, *active_construct, foot_anchor);
            output.grounded = true;
            output.construct_id = active_construct->id();
            state.unsupported_seconds = 0.0;
            state.last_support_node = support->node_position;
            state.has_last_support_node = true;
            return output;
        }

        state.unsupported_seconds += dt;
        if (state.unsupported_seconds <= std::max(0.0, contact_grace)) {
            output.grounded = true;
            output.construct_id = active_construct->id();
            return output;
        }
        if (input.inherit_platform_velocity_on_detach) {
            output.velocity += platform.surface_velocity;
            output.inherited_velocity = platform.surface_velocity;
        }
        PlatformContact::clear(state.contact);
        state.last_surface_velocity = {};
        state.unsupported_seconds = 0.0;
        state.has_last_support_node = false;
        output.contact_ended = true;
    }

    if (input.allow_contact_acquisition && !input.jump_pressed && input.velocity.y <= 0.5) {
        const auto support = findBestSupport(
            constructs, input.world_box, support_drop, support_penetration);
        if (support) {
            const DynamicConstruct *construct = findConstruct(constructs, support->construct_id);
            if (construct) {
                output.position.y -= support->vertical_gap;
                const Vec3d anchor{
                    input.world_box.centre().x,
                    input.world_box.min.y - support->vertical_gap,
                    input.world_box.centre().z,
                };
                state.contact = PlatformContact::begin(*construct, anchor);
                state.last_surface_velocity = ConstructGeometry::surfaceVelocity(*construct, anchor);
                state.unsupported_seconds = 0.0;
                state.last_support_node = support->node_position;
                state.has_last_support_node = true;
                output.grounded = true;
                output.construct_id = construct->id();
                output.contact_started = true;
            }
        }
    }
    return output;
}

RiderCorrection RiderReconciler::reconcile(
    const Vec3d &reported,
    const Vec3d &expected,
    double soft_distance,
    double hard_distance,
    double soft_fraction) noexcept
{
    RiderCorrection result;
    result.position = reported;
    const Vec3d error = expected - reported;
    const double distance = magnitude(error);
    soft_distance = std::max(0.0, soft_distance);
    hard_distance = std::max(soft_distance, hard_distance);
    soft_fraction = std::clamp(soft_fraction, 0.0, 1.0);
    if (distance <= soft_distance)
        return result;
    result.corrected = true;
    if (distance >= hard_distance) {
        result.position = expected;
        result.correction = error;
        result.hard_snap = true;
        return result;
    }
    result.correction = error * soft_fraction;
    result.position += result.correction;
    return result;
}

} // namespace navycraft

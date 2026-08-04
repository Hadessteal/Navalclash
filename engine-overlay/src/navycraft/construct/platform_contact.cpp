// SPDX-License-Identifier: LGPL-2.1-or-later
#include "platform_contact.h"

#include <cmath>

namespace navycraft {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = PI * 2.0;

double shortestYawDelta(double from, double to) noexcept
{
    double delta = std::fmod(to - from, TWO_PI);
    if (delta > PI)
        delta -= TWO_PI;
    else if (delta < -PI)
        delta += TWO_PI;
    return delta;
}
}

PlatformContactState PlatformContact::begin(
    const DynamicConstruct &construct, const Vec3d &world_anchor) noexcept
{
    PlatformContactState state;
    state.construct_id = construct.id();
    state.local_anchor = construct.transform().worldToLocal(world_anchor);
    state.previous_transform = construct.transform();
    state.active = true;
    return state;
}

PlatformMotion PlatformContact::update(
    PlatformContactState &state,
    const DynamicConstruct &construct,
    double delta_seconds) noexcept
{
    PlatformMotion motion;
    if (!state.active || state.construct_id != construct.id())
        return motion;
    const Vec3d previous_anchor = state.previous_transform.localToWorld(state.local_anchor);
    motion.world_anchor = construct.transform().localToWorld(state.local_anchor);
    motion.displacement = motion.world_anchor - previous_anchor;
    motion.yaw_delta = shortestYawDelta(
        state.previous_transform.yaw_radians, construct.transform().yaw_radians);

    const Vec3d radial = motion.world_anchor - construct.transform().position;
    motion.surface_velocity = construct.linearVelocity() + Vec3d{
        -construct.yawVelocity() * radial.z,
        0.0,
        construct.yawVelocity() * radial.x,
    };
    if (delta_seconds > 0.0 && std::isfinite(delta_seconds)) {
        // Prefer measured transform motion for collision-corrected vessels, but retain the
        // commanded surface velocity when interpolation produces no displacement this frame.
        const Vec3d measured = motion.displacement * (1.0 / delta_seconds);
        const double measured_sq = measured.x * measured.x + measured.y * measured.y +
            measured.z * measured.z;
        if (measured_sq > 1e-12)
            motion.surface_velocity = measured;
    }
    state.previous_transform = construct.transform();
    return motion;
}

void PlatformContact::reanchor(
    PlatformContactState &state,
    const DynamicConstruct &construct,
    const Vec3d &world_anchor) noexcept
{
    state.construct_id = construct.id();
    state.local_anchor = construct.transform().worldToLocal(world_anchor);
    state.previous_transform = construct.transform();
    state.active = true;
}

void PlatformContact::clear(PlatformContactState &state) noexcept
{
    state = {};
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_replication.h"

#include <algorithm>
#include <cmath>

namespace navycraft {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = PI * 2.0;

double normaliseYaw(double yaw) noexcept
{
    yaw = std::fmod(yaw, TWO_PI);
    if (yaw < 0.0)
        yaw += TWO_PI;
    return yaw;
}

double shortestYawDelta(double from, double to) noexcept
{
    double delta = normaliseYaw(to) - normaliseYaw(from);
    if (delta > PI)
        delta -= TWO_PI;
    if (delta < -PI)
        delta += TWO_PI;
    return delta;
}
}

ConstructTransform ConstructReplication::interpolate(
    const ConstructTransformSnapshot &older,
    const ConstructTransformSnapshot &newer,
    double render_time) noexcept
{
    const double duration = newer.server_time - older.server_time;
    if (duration <= 0.0)
        return newer.transform;
    const double alpha = std::clamp((render_time - older.server_time) / duration, 0.0, 1.0);
    const Vec3d delta = newer.transform.position - older.transform.position;
    ConstructTransform result;
    result.position = older.transform.position + delta * alpha;
    result.yaw_radians = normaliseYaw(
        older.transform.yaw_radians + shortestYawDelta(
            older.transform.yaw_radians, newer.transform.yaw_radians) * alpha);
    return result;
}

ConstructTransform ConstructReplication::extrapolate(
    const ConstructTransformSnapshot &snapshot,
    double render_time,
    double maximum_seconds) noexcept
{
    const double delta = std::clamp(render_time - snapshot.server_time, 0.0, maximum_seconds);
    ConstructTransform result = snapshot.transform;
    result.position += snapshot.linear_velocity * delta;
    result.yaw_radians = normaliseYaw(result.yaw_radians + snapshot.yaw_velocity * delta);
    return result;
}

} // namespace navycraft

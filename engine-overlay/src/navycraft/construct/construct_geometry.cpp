// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace navycraft {
namespace {
constexpr double EPSILON = 1e-9;

Vec3d rotateLocalToWorldDirection(const Vec3d &local, double yaw) noexcept
{
    const double cosine = std::cos(yaw);
    const double sine = std::sin(yaw);
    return {
        cosine * local.x - sine * local.z,
        local.y,
        sine * local.x + cosine * local.z,
    };
}

Vec3d rotateWorldToLocalDirection(const Vec3d &world, double yaw) noexcept
{
    const double cosine = std::cos(yaw);
    const double sine = std::sin(yaw);
    return {
        cosine * world.x + sine * world.z,
        world.y,
        -sine * world.x + cosine * world.z,
    };
}

double length(const Vec3d &value) noexcept
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3d normalised(const Vec3d &value) noexcept
{
    const double magnitude = length(value);
    if (magnitude <= EPSILON)
        return {};
    return value * (1.0 / magnitude);
}

std::array<Vec3d, 8> corners(const Aabb3d &box) noexcept
{
    return {{
        {box.min.x, box.min.y, box.min.z},
        {box.max.x, box.min.y, box.min.z},
        {box.min.x, box.max.y, box.min.z},
        {box.max.x, box.max.y, box.min.z},
        {box.min.x, box.min.y, box.max.z},
        {box.max.x, box.min.y, box.max.z},
        {box.min.x, box.max.y, box.max.z},
        {box.max.x, box.max.y, box.max.z},
    }};
}

Aabb3d transformedBounds(const Aabb3d &box, const ConstructTransform &transform) noexcept
{
    Aabb3d result;
    result.min = {std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    result.max = {-std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (const auto &corner : corners(box)) {
        const Vec3d point = transform.localToWorld(corner);
        result.min.x = std::min(result.min.x, point.x);
        result.min.y = std::min(result.min.y, point.y);
        result.min.z = std::min(result.min.z, point.z);
        result.max.x = std::max(result.max.x, point.x);
        result.max.y = std::max(result.max.y, point.y);
        result.max.z = std::max(result.max.z, point.z);
    }
    return result;
}

Aabb3d worldBoxInLocalAabb(const Aabb3d &box, const ConstructTransform &transform) noexcept
{
    Aabb3d result;
    result.min = {std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    result.max = {-std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (const auto &corner : corners(box)) {
        const Vec3d point = transform.worldToLocal(corner);
        result.min.x = std::min(result.min.x, point.x);
        result.min.y = std::min(result.min.y, point.y);
        result.min.z = std::min(result.min.z, point.z);
        result.max.x = std::max(result.max.x, point.x);
        result.max.y = std::max(result.max.y, point.y);
        result.max.z = std::max(result.max.z, point.z);
    }
    return result;
}

bool intervalOverlap(double a_min, double a_max, double b_min, double b_max) noexcept
{
    return a_max > b_min + EPSILON && b_max > a_min + EPSILON;
}

double dot(const Vec3d &left, const Vec3d &right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

struct NodeOverlap {
    Vec3d normal{};
    double depth = 0.0;
};

// Exact yaw-only OBB-versus-world-AABB SAT test. The returned normal points
// from the construct block toward the world AABB and is suitable for
// depenetration and collision response.
std::optional<NodeOverlap> nodeObbOverlapAabb(
    const LocalNodePos &position,
    const ConstructTransform &transform,
    const Aabb3d &world_box) noexcept
{
    const Vec3d local_centre{position.x + 0.5, position.y + 0.5, position.z + 0.5};
    const Vec3d centre = transform.localToWorld(local_centre);
    const Vec3d box_centre = world_box.centre();

    const double node_min_y = centre.y - 0.5;
    const double node_max_y = centre.y + 0.5;
    if (!intervalOverlap(node_min_y, node_max_y, world_box.min.y, world_box.max.y))
        return std::nullopt;

    NodeOverlap best;
    best.depth = std::min(node_max_y, world_box.max.y) -
        std::max(node_min_y, world_box.min.y);
    best.normal = {0.0, box_centre.y >= centre.y ? 1.0 : -1.0, 0.0};

    const double cosine = std::cos(transform.yaw_radians);
    const double sine = std::sin(transform.yaw_radians);
    const Vec3d local_x{cosine, 0.0, sine};
    const Vec3d local_z{-sine, 0.0, cosine};
    const std::array<Vec3d, 4> axes{{
        {1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0},
        local_x,
        local_z,
    }};
    const Vec3d box_half = world_box.size() * 0.5;
    const Vec3d from_node = box_centre - centre;
    for (const auto &axis : axes) {
        const double signed_distance = dot(from_node, axis);
        const double distance = std::abs(signed_distance);
        const double node_radius = 0.5 * std::abs(dot(axis, local_x)) +
            0.5 * std::abs(dot(axis, local_z));
        const double box_radius = box_half.x * std::abs(axis.x) +
            box_half.z * std::abs(axis.z);
        const double overlap = node_radius + box_radius - distance;
        if (overlap <= EPSILON)
            return std::nullopt;
        if (overlap < best.depth) {
            best.depth = overlap;
            const double direction = signed_distance >= 0.0 ? 1.0 : -1.0;
            best.normal = axis * direction;
        }
    }
    return best;
}

bool nodeObbIntersectsAabb(
    const LocalNodePos &position,
    const ConstructTransform &transform,
    const Aabb3d &world_box) noexcept
{
    return nodeObbOverlapAabb(position, transform, world_box).has_value();
}

int floorToInt(double value) noexcept
{
    return static_cast<int>(std::floor(value));
}

int stepSign(double value) noexcept
{
    return value > EPSILON ? 1 : (value < -EPSILON ? -1 : 0);
}

double firstBoundaryT(double coordinate, double direction, int step) noexcept
{
    if (step == 0)
        return std::numeric_limits<double>::infinity();
    const double boundary = step > 0 ? std::floor(coordinate) + 1.0 : std::floor(coordinate);
    return (boundary - coordinate) / direction;
}

Vec3d localFaceNormal(int axis, int step) noexcept
{
    Vec3d normal{};
    if (axis == 0)
        normal.x = static_cast<double>(-step);
    else if (axis == 1)
        normal.y = static_cast<double>(-step);
    else
        normal.z = static_cast<double>(-step);
    return normal;
}
}

bool Aabb3d::valid() const noexcept
{
    return min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

Vec3d Aabb3d::centre() const noexcept
{
    return (min + max) * 0.5;
}

Vec3d Aabb3d::size() const noexcept
{
    return max - min;
}

Aabb3d Aabb3d::translated(const Vec3d &delta) const noexcept
{
    return {min + delta, max + delta};
}

bool Aabb3d::intersects(const Aabb3d &other) const noexcept
{
    return intervalOverlap(min.x, max.x, other.min.x, other.max.x) &&
        intervalOverlap(min.y, max.y, other.min.y, other.max.y) &&
        intervalOverlap(min.z, max.z, other.min.z, other.max.z);
}

bool Aabb3d::contains(const Vec3d &point) const noexcept
{
    return point.x >= min.x && point.x <= max.x &&
        point.y >= min.y && point.y <= max.y &&
        point.z >= min.z && point.z <= max.z;
}

Aabb3d ConstructGeometry::localNodeBox(const LocalNodePos &position) noexcept
{
    return {{static_cast<double>(position.x), static_cast<double>(position.y),
                static_cast<double>(position.z)},
        {static_cast<double>(position.x + 1), static_cast<double>(position.y + 1),
            static_cast<double>(position.z + 1)}};
}

Aabb3d ConstructGeometry::worldBounds(const DynamicConstruct &construct) noexcept
{
    const LocalBounds local = construct.localBounds();
    if (!local.valid)
        return {};
    const Aabb3d local_box{{static_cast<double>(local.min.x), static_cast<double>(local.min.y),
                               static_cast<double>(local.min.z)},
        {static_cast<double>(local.max.x + 1), static_cast<double>(local.max.y + 1),
            static_cast<double>(local.max.z + 1)}};
    return transformedBounds(local_box, construct.transform());
}

bool ConstructGeometry::intersectsWorldAabb(
    const DynamicConstruct &construct, const Aabb3d &world_box) noexcept
{
    if (construct.empty() || !world_box.valid() || !worldBounds(construct).intersects(world_box))
        return false;

    const Aabb3d local_candidates = worldBoxInLocalAabb(world_box, construct.transform());
    const int min_x = floorToInt(local_candidates.min.x) - 1;
    const int min_y = floorToInt(local_candidates.min.y) - 1;
    const int min_z = floorToInt(local_candidates.min.z) - 1;
    const int max_x = floorToInt(local_candidates.max.x) + 1;
    const int max_y = floorToInt(local_candidates.max.y) + 1;
    const int max_z = floorToInt(local_candidates.max.z) + 1;
    for (int y = min_y; y <= max_y; ++y) {
        for (int z = min_z; z <= max_z; ++z) {
            for (int x = min_x; x <= max_x; ++x) {
                const LocalNodePos position{x, y, z};
                if (construct.getNode(position) &&
                        nodeObbIntersectsAabb(position, construct.transform(), world_box))
                    return true;
            }
        }
    }
    return false;
}

std::optional<ConstructOverlapHit> ConstructGeometry::overlapWorldAabb(
    const DynamicConstruct &construct, const Aabb3d &world_box) noexcept
{
    if (construct.empty() || !world_box.valid() || !worldBounds(construct).intersects(world_box))
        return std::nullopt;

    const Aabb3d local_candidates = worldBoxInLocalAabb(world_box, construct.transform());
    const int min_x = floorToInt(local_candidates.min.x) - 1;
    const int min_y = floorToInt(local_candidates.min.y) - 1;
    const int min_z = floorToInt(local_candidates.min.z) - 1;
    const int max_x = floorToInt(local_candidates.max.x) + 1;
    const int max_y = floorToInt(local_candidates.max.y) + 1;
    const int max_z = floorToInt(local_candidates.max.z) + 1;
    std::optional<ConstructOverlapHit> best;
    for (int y = min_y; y <= max_y; ++y) {
        for (int z = min_z; z <= max_z; ++z) {
            for (int x = min_x; x <= max_x; ++x) {
                const LocalNodePos position{x, y, z};
                if (!construct.getNode(position))
                    continue;
                const auto overlap = nodeObbOverlapAabb(
                    position, construct.transform(), world_box);
                if (!overlap)
                    continue;
                if (best && overlap->depth >= best->penetration_depth)
                    continue;
                ConstructOverlapHit hit;
                hit.construct_id = construct.id();
                hit.node_position = position;
                hit.world_normal = overlap->normal;
                hit.penetration_depth = overlap->depth;
                best = hit;
            }
        }
    }
    return best;
}

std::optional<ConstructSupportHit> ConstructGeometry::findSupport(
    const DynamicConstruct &construct,
    const Aabb3d &world_box,
    double maximum_drop,
    double maximum_penetration,
    std::optional<LocalNodePos> preferred_node,
    double preferred_node_bias) noexcept
{
    if (construct.empty() || !world_box.valid())
        return std::nullopt;
    maximum_drop = std::max(0.0, maximum_drop);
    maximum_penetration = std::max(0.0, maximum_penetration);
    preferred_node_bias = std::max(0.0, preferred_node_bias);

    Aabb3d search_box = world_box;
    search_box.min.y -= maximum_drop;
    search_box.max.y = world_box.min.y + maximum_penetration;
    if (!worldBounds(construct).intersects(search_box))
        return std::nullopt;

    const Aabb3d local_candidates = worldBoxInLocalAabb(search_box, construct.transform());
    const int min_x = floorToInt(local_candidates.min.x) - 1;
    const int min_y = floorToInt(local_candidates.min.y) - 1;
    const int min_z = floorToInt(local_candidates.min.z) - 1;
    const int max_x = floorToInt(local_candidates.max.x) + 1;
    const int max_y = floorToInt(local_candidates.max.y) + 1;
    const int max_z = floorToInt(local_candidates.max.z) + 1;

    std::optional<ConstructSupportHit> best;
    for (int y = min_y; y <= max_y; ++y) {
        for (int z = min_z; z <= max_z; ++z) {
            for (int x = min_x; x <= max_x; ++x) {
                const LocalNodePos position{x, y, z};
                if (!construct.getNode(position))
                    continue;
                const double top_y = construct.transform().position.y +
                    static_cast<double>(position.y + 1);
                const double gap = world_box.min.y - top_y;
                if (gap < -maximum_penetration || gap > maximum_drop)
                    continue;

                Aabb3d foot_slice = world_box;
                foot_slice.min.y = top_y - 0.02;
                foot_slice.max.y = top_y + 0.02;
                if (!nodeObbIntersectsAabb(position, construct.transform(), foot_slice))
                    continue;
                const double score = std::max(0.0, std::abs(gap) -
                    (preferred_node && position == *preferred_node ?
                        preferred_node_bias : 0.0));
                if (best) {
                    const double best_score = std::max(0.0,
                        std::abs(best->vertical_gap) -
                        (preferred_node && best->node_position == *preferred_node ?
                            preferred_node_bias : 0.0));
                    if (score >= best_score)
                        continue;
                }

                const Vec3d world_point{world_box.centre().x, top_y, world_box.centre().z};
                ConstructSupportHit hit;
                hit.construct_id = construct.id();
                hit.node_position = position;
                hit.world_point = world_point;
                hit.local_anchor = construct.transform().worldToLocal(world_point);
                hit.vertical_gap = gap;
                best = hit;
            }
        }
    }
    return best;
}

SweepResult ConstructGeometry::sweepWorldAabb(
    const DynamicConstruct &construct,
    const Aabb3d &world_box,
    const Vec3d &delta,
    double maximum_step) noexcept
{
    SweepResult result;
    result.allowed_delta = delta;
    const double distance = length(delta);
    if (distance <= EPSILON || construct.empty())
        return result;
    maximum_step = std::clamp(maximum_step, 0.02, 1.0);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / maximum_step)));
    double previous = 0.0;
    for (int index = 1; index <= steps; ++index) {
        const double fraction = static_cast<double>(index) / static_cast<double>(steps);
        if (!intersectsWorldAabb(construct, world_box.translated(delta * fraction))) {
            previous = fraction;
            continue;
        }
        double low = previous;
        double high = fraction;
        for (int iteration = 0; iteration < 14; ++iteration) {
            const double middle = (low + high) * 0.5;
            if (intersectsWorldAabb(construct, world_box.translated(delta * middle)))
                high = middle;
            else
                low = middle;
        }
        result.collided = true;
        result.allowed_fraction = std::max(0.0, low - 1e-5);
        result.allowed_delta = delta * result.allowed_fraction;
        return result;
    }
    return result;
}

Vec3d ConstructGeometry::surfaceVelocity(
    const DynamicConstruct &construct, const Vec3d &world_point) noexcept
{
    const Vec3d radial = world_point - construct.transform().position;
    return construct.linearVelocity() + Vec3d{
        -construct.yawVelocity() * radial.z,
        0.0,
        construct.yawVelocity() * radial.x,
    };
}

ConstructSweepHit ConstructGeometry::sweepWorldAabbDetailed(
    const DynamicConstruct &construct,
    const Aabb3d &world_box,
    const Vec3d &delta,
    double delta_seconds,
    double maximum_step) noexcept
{
    ConstructSweepHit hit;
    hit.allowed_delta = delta;
    const Vec3d platform_delta = surfaceVelocity(construct, world_box.centre()) *
        std::max(0.0, delta_seconds);
    const Vec3d relative_delta = delta - platform_delta;
    const SweepResult sweep = sweepWorldAabb(
        construct, world_box, relative_delta, maximum_step);
    if (!sweep.collided)
        return hit;

    hit.collided = true;
    hit.construct_id = construct.id();
    hit.allowed_fraction = sweep.allowed_fraction;
    hit.allowed_delta = delta * sweep.allowed_fraction;

    const double probe_fraction = std::min(1.0, sweep.allowed_fraction + 2e-4);
    const Aabb3d probe_box = world_box.translated(relative_delta * probe_fraction);
    const auto overlap = overlapWorldAabb(construct, probe_box);
    if (overlap) {
        hit.node_position = overlap->node_position;
        hit.world_normal = overlap->world_normal;
    } else {
        hit.world_normal = normalised(relative_delta * -1.0);
    }
    if (dot(relative_delta, hit.world_normal) > 0.0)
        hit.world_normal = hit.world_normal * -1.0;
    hit.world_point = world_box.translated(hit.allowed_delta).centre();
    hit.surface_velocity = surfaceVelocity(construct, hit.world_point);
    return hit;
}

std::optional<ConstructRaycastHit> ConstructGeometry::raycast(
    const DynamicConstruct &construct,
    const Vec3d &world_start,
    const Vec3d &world_end) noexcept
{
    if (construct.empty())
        return std::nullopt;
    const Vec3d world_delta = world_end - world_start;
    const double world_length = length(world_delta);
    if (world_length <= EPSILON)
        return std::nullopt;

    const Vec3d local_start = construct.transform().worldToLocal(world_start);
    const Vec3d local_direction = normalised(
        rotateWorldToLocalDirection(world_delta, construct.transform().yaw_radians));
    int x = floorToInt(local_start.x);
    int y = floorToInt(local_start.y);
    int z = floorToInt(local_start.z);
    const int step_x = stepSign(local_direction.x);
    const int step_y = stepSign(local_direction.y);
    const int step_z = stepSign(local_direction.z);
    double t_max_x = firstBoundaryT(local_start.x, local_direction.x, step_x);
    double t_max_y = firstBoundaryT(local_start.y, local_direction.y, step_y);
    double t_max_z = firstBoundaryT(local_start.z, local_direction.z, step_z);
    const double t_delta_x = step_x == 0 ? std::numeric_limits<double>::infinity() :
        std::abs(1.0 / local_direction.x);
    const double t_delta_y = step_y == 0 ? std::numeric_limits<double>::infinity() :
        std::abs(1.0 / local_direction.y);
    const double t_delta_z = step_z == 0 ? std::numeric_limits<double>::infinity() :
        std::abs(1.0 / local_direction.z);

    double travelled = 0.0;
    Vec3d entered_normal{};
    const std::size_t maximum_iterations = static_cast<std::size_t>(std::ceil(world_length * 3.0)) + 16U;
    for (std::size_t iteration = 0; iteration < maximum_iterations && travelled <= world_length + EPSILON;
            ++iteration) {
        const LocalNodePos position{x, y, z};
        if (construct.getNode(position)) {
            const Vec3d local_point = local_start + local_direction * travelled;
            ConstructRaycastHit hit;
            hit.construct_id = construct.id();
            hit.node_position = position;
            hit.local_point = local_point;
            hit.world_point = construct.transform().localToWorld(local_point);
            hit.world_normal = rotateLocalToWorldDirection(
                entered_normal, construct.transform().yaw_radians);
            hit.distance = travelled;
            return hit;
        }

        if (t_max_x <= t_max_y && t_max_x <= t_max_z) {
            travelled = std::max(0.0, t_max_x);
            t_max_x += t_delta_x;
            x += step_x;
            entered_normal = localFaceNormal(0, step_x);
        } else if (t_max_y <= t_max_z) {
            travelled = std::max(0.0, t_max_y);
            t_max_y += t_delta_y;
            y += step_y;
            entered_normal = localFaceNormal(1, step_y);
        } else {
            travelled = std::max(0.0, t_max_z);
            t_max_z += t_delta_z;
            z += step_z;
            entered_normal = localFaceNormal(2, step_z);
        }
    }
    return std::nullopt;
}

} // namespace navycraft

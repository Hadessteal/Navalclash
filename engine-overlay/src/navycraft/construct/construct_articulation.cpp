// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_articulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace navycraft {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double EPSILON = 1e-9;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finiteVec(const Vec3d &value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double dot(const Vec3d &left, const Vec3d &right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3d cross(const Vec3d &left, const Vec3d &right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

double lengthSquared(const Vec3d &value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d &value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

Vec3d normalised(const Vec3d &value)
{
    const double magnitude = length(value);
    if (!(magnitude > EPSILON) || !finite(magnitude))
        throw std::invalid_argument("articulation axis or direction has zero length");
    return value * (1.0 / magnitude);
}

Vec3d safeNormalised(const Vec3d &value) noexcept
{
    const double magnitude = length(value);
    return magnitude > EPSILON && finite(magnitude) ? value * (1.0 / magnitude) : Vec3d{};
}

double clamp(double value, double minimum, double maximum) noexcept
{
    return std::max(minimum, std::min(maximum, value));
}


Vec3d rotateYaw(const Vec3d &value, double yaw) noexcept
{
    const double cosine = std::cos(yaw);
    const double sine = std::sin(yaw);
    return {
        cosine * value.x - sine * value.z,
        value.y,
        sine * value.x + cosine * value.z,
    };
}

double signedAngle(const Vec3d &from, const Vec3d &to, const Vec3d &axis) noexcept
{
    const Vec3d n_axis = safeNormalised(axis);
    const Vec3d from_projected = from - n_axis * dot(from, n_axis);
    const Vec3d to_projected = to - n_axis * dot(to, n_axis);
    const Vec3d n_from = safeNormalised(from_projected);
    const Vec3d n_to = safeNormalised(to_projected);
    if (lengthSquared(n_from) <= EPSILON || lengthSquared(n_to) <= EPSILON)
        return 0.0;
    return std::atan2(dot(n_axis, cross(n_from, n_to)), clamp(dot(n_from, n_to), -1.0, 1.0));
}

bool segmentAabb(const Vec3d &start, const Vec3d &end,
    const Vec3d &minimum, const Vec3d &maximum,
    double &entry_t) noexcept
{
    const Vec3d delta = end - start;
    double t_min = 0.0;
    double t_max = 1.0;
    const std::array<double, 3> origins{start.x, start.y, start.z};
    const std::array<double, 3> directions{delta.x, delta.y, delta.z};
    const std::array<double, 3> mins{minimum.x, minimum.y, minimum.z};
    const std::array<double, 3> maxs{maximum.x, maximum.y, maximum.z};
    for (std::size_t axis = 0; axis < origins.size(); ++axis) {
        if (std::abs(directions[axis]) <= EPSILON) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis])
                return false;
            continue;
        }
        const double inverse = 1.0 / directions[axis];
        double first = (mins[axis] - origins[axis]) * inverse;
        double second = (maxs[axis] - origins[axis]) * inverse;
        if (first > second)
            std::swap(first, second);
        t_min = std::max(t_min, first);
        t_max = std::min(t_max, second);
        if (t_min > t_max)
            return false;
    }
    entry_t = t_min;
    return t_max >= 0.0 && t_min <= 1.0;
}
}

struct ConstructArticulationEngine::AffineTransform {
    double r[3][3]{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    Vec3d t{};

    [[nodiscard]] Vec3d point(const Vec3d &value) const noexcept
    {
        return {
            r[0][0] * value.x + r[0][1] * value.y + r[0][2] * value.z + t.x,
            r[1][0] * value.x + r[1][1] * value.y + r[1][2] * value.z + t.y,
            r[2][0] * value.x + r[2][1] * value.y + r[2][2] * value.z + t.z,
        };
    }

    [[nodiscard]] Vec3d direction(const Vec3d &value) const noexcept
    {
        return {
            r[0][0] * value.x + r[0][1] * value.y + r[0][2] * value.z,
            r[1][0] * value.x + r[1][1] * value.y + r[1][2] * value.z,
            r[2][0] * value.x + r[2][1] * value.y + r[2][2] * value.z,
        };
    }

    [[nodiscard]] AffineTransform inverse() const noexcept
    {
        AffineTransform result;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                result.r[row][column] = r[column][row];
        const Vec3d negative{-t.x, -t.y, -t.z};
        result.t = result.direction(negative);
        return result;
    }
};

namespace {
ConstructArticulationEngine::AffineTransform multiply(
    const ConstructArticulationEngine::AffineTransform &left,
    const ConstructArticulationEngine::AffineTransform &right) noexcept
{
    ConstructArticulationEngine::AffineTransform result;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            result.r[row][column] = 0.0;
            for (int index = 0; index < 3; ++index)
                result.r[row][column] += left.r[row][index] * right.r[index][column];
        }
    }
    result.t = left.point(right.t);
    return result;
}

ConstructArticulationEngine::AffineTransform jointTransform(
    const ConstructArticulationDefinition &definition, double position) noexcept
{
    ConstructArticulationEngine::AffineTransform result;
    const Vec3d axis = safeNormalised(definition.axis_local);
    if (definition.kind == ConstructJointKind::Prismatic) {
        result.t = axis * position;
        return result;
    }

    const double cosine = std::cos(position);
    const double sine = std::sin(position);
    const double one_minus = 1.0 - cosine;
    result.r[0][0] = cosine + axis.x * axis.x * one_minus;
    result.r[0][1] = axis.x * axis.y * one_minus - axis.z * sine;
    result.r[0][2] = axis.x * axis.z * one_minus + axis.y * sine;
    result.r[1][0] = axis.y * axis.x * one_minus + axis.z * sine;
    result.r[1][1] = cosine + axis.y * axis.y * one_minus;
    result.r[1][2] = axis.y * axis.z * one_minus - axis.x * sine;
    result.r[2][0] = axis.z * axis.x * one_minus - axis.y * sine;
    result.r[2][1] = axis.z * axis.y * one_minus + axis.x * sine;
    result.r[2][2] = cosine + axis.z * axis.z * one_minus;
    result.t = definition.pivot_local - result.direction(definition.pivot_local);
    return result;
}
}

ConstructArticulationEngine::ConstructArticulationEngine(ConstructRegistry &registry) :
    m_registry(registry)
{
}

std::size_t ConstructArticulationEngine::NodeKeyHash::operator()(const NodeKey &key) const noexcept
{
    std::size_t seed = std::hash<ConstructId>{}(key.construct_id);
    seed ^= LocalNodePosHash{}(key.position) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

ConstructArticulationId ConstructArticulationEngine::configureJoint(
    ConstructArticulationDefinition definition)
{
    if (definition.construct_id == 0 || !m_registry.find(definition.construct_id))
        throw std::invalid_argument("articulation construct does not exist");
    if (!finiteVec(definition.pivot_local) || !finiteVec(definition.axis_local))
        throw std::invalid_argument("articulation pivot or axis is not finite");
    definition.axis_local = normalised(definition.axis_local);
    if (!finite(definition.minimum_position) || !finite(definition.maximum_position) ||
            definition.minimum_position > definition.maximum_position ||
            !finite(definition.maximum_speed) || definition.maximum_speed < 0.0 ||
            !finite(definition.maximum_acceleration) || definition.maximum_acceleration < 0.0)
        throw std::invalid_argument("articulation limits are invalid");
    if (definition.parent_id != 0) {
        const auto parent = m_joints.find(definition.parent_id);
        if (parent == m_joints.end() ||
                parent->second.definition.construct_id != definition.construct_id)
            throw std::invalid_argument("articulation parent is invalid");
    }
    if (definition.nodes.size() > 65535)
        throw std::length_error("articulation owns too many nodes");
    if (definition.id == 0)
        definition.id = m_next_joint_id++;
    else
        m_next_joint_id = std::max(m_next_joint_id, definition.id + 1);
    if (definition.parent_id == definition.id)
        throw std::invalid_argument("articulation cannot parent itself");

    const auto previous = m_joints.find(definition.id);
    if (previous != m_joints.end()) {
        for (const auto &position : previous->second.definition.nodes)
            m_node_owners.erase({previous->second.definition.construct_id, position});
    }
    for (const auto &position : definition.nodes) {
        const NodeKey key{definition.construct_id, position};
        const auto owner = m_node_owners.find(key);
        if (owner != m_node_owners.end() && owner->second != definition.id)
            throw std::invalid_argument("construct node is already owned by another articulation");
    }

    ConstructArticulationState state;
    if (previous != m_joints.end())
        state = previous->second;
    state.definition = definition;
    state.position = clampPosition(definition, state.position);
    state.target_position = clampPosition(definition, state.target_position);
    ++state.revision;
    m_joints[definition.id] = state;
    for (const auto &position : definition.nodes)
        m_node_owners[{definition.construct_id, position}] = definition.id;
    return definition.id;
}

bool ConstructArticulationEngine::removeJoint(ConstructArticulationId id)
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end())
        return false;
    std::vector<ConstructArticulationId> descendants;
    for (const auto &[candidate_id, candidate] : m_joints) {
        (void)candidate;
        if (candidate_id != id && isDescendantOrSame(candidate_id, id))
            descendants.push_back(candidate_id);
    }
    for (const auto descendant : descendants)
        removeJoint(descendant);
    for (const auto &position : iterator->second.definition.nodes)
        m_node_owners.erase({iterator->second.definition.construct_id, position});
    for (auto turret = m_turrets.begin(); turret != m_turrets.end();) {
        if (turret->second.definition.yaw_joint_id == id ||
                turret->second.definition.pitch_joint_id == id)
            turret = m_turrets.erase(turret);
        else
            ++turret;
    }
    m_joints.erase(iterator);
    return true;
}

void ConstructArticulationEngine::clearConstruct(ConstructId construct_id)
{
    std::vector<ConstructArticulationId> ids;
    for (const auto &[id, state] : m_joints) {
        if (state.definition.construct_id == construct_id)
            ids.push_back(id);
    }
    for (const auto id : ids)
        removeJoint(id);
    for (auto turret = m_turrets.begin(); turret != m_turrets.end();) {
        if (turret->second.definition.construct_id == construct_id)
            turret = m_turrets.erase(turret);
        else
            ++turret;
    }
}

void ConstructArticulationEngine::clear()
{
    m_joints.clear();
    m_node_owners.clear();
    m_turrets.clear();
    m_next_joint_id = 1;
    m_next_turret_id = 1;
}

bool ConstructArticulationEngine::setJointTarget(ConstructArticulationId id, double target_position)
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end() || !finite(target_position))
        return false;
    iterator->second.target_position = clampPosition(iterator->second.definition, target_position);
    iterator->second.definition.control_mode = ConstructJointControlMode::Position;
    ++iterator->second.revision;
    return true;
}

bool ConstructArticulationEngine::setJointVelocity(ConstructArticulationId id, double velocity)
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end() || !finite(velocity))
        return false;
    iterator->second.commanded_velocity = clamp(velocity,
        -iterator->second.definition.maximum_speed,
        iterator->second.definition.maximum_speed);
    iterator->second.definition.control_mode = ConstructJointControlMode::Velocity;
    ++iterator->second.revision;
    return true;
}

bool ConstructArticulationEngine::setJointPosition(ConstructArticulationId id, double position)
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end() || !finite(position))
        return false;
    iterator->second.position = clampPosition(iterator->second.definition, position);
    iterator->second.target_position = iterator->second.position;
    iterator->second.velocity = 0.0;
    ++iterator->second.revision;
    return true;
}

bool ConstructArticulationEngine::setJointEnabled(ConstructArticulationId id, bool enabled)
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end())
        return false;
    iterator->second.definition.enabled = enabled;
    if (!enabled)
        iterator->second.velocity = 0.0;
    ++iterator->second.revision;
    return true;
}

std::optional<ConstructArticulationState> ConstructArticulationEngine::joint(
    ConstructArticulationId id) const
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end())
        return std::nullopt;
    return iterator->second;
}

std::vector<ConstructArticulationState> ConstructArticulationEngine::joints(
    ConstructId construct_id) const
{
    std::vector<ConstructArticulationState> result;
    for (const auto &[id, state] : m_joints) {
        (void)id;
        if (construct_id == 0 || state.definition.construct_id == construct_id)
            result.push_back(state);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.definition.id < right.definition.id;
    });
    return result;
}

std::optional<ConstructArticulationId> ConstructArticulationEngine::nodeJoint(
    ConstructId construct_id, const LocalNodePos &position) const
{
    const auto iterator = m_node_owners.find({construct_id, position});
    if (iterator == m_node_owners.end())
        return std::nullopt;
    return iterator->second;
}

ConstructArticulationEngine::AffineTransform ConstructArticulationEngine::parentTransform(
    ConstructArticulationId id,
    const std::unordered_map<ConstructArticulationId, double> *overrides) const
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end())
        throw std::out_of_range("articulation joint was not found");
    if (iterator->second.definition.parent_id == 0)
        return {};
    return localTransform(iterator->second.definition.parent_id, overrides);
}

ConstructArticulationEngine::AffineTransform ConstructArticulationEngine::localTransform(
    ConstructArticulationId id,
    const std::unordered_map<ConstructArticulationId, double> *overrides) const
{
    const auto iterator = m_joints.find(id);
    if (iterator == m_joints.end())
        throw std::out_of_range("articulation joint was not found");
    double position = iterator->second.position;
    if (overrides) {
        const auto replacement = overrides->find(id);
        if (replacement != overrides->end())
            position = replacement->second;
    }
    return multiply(parentTransform(id, overrides),
        jointTransform(iterator->second.definition, position));
}

Vec3d ConstructArticulationEngine::transformPointLocal(
    ConstructArticulationId id, const Vec3d &point) const
{
    return localTransform(id).point(point);
}

Vec3d ConstructArticulationEngine::transformDirectionLocal(
    ConstructArticulationId id, const Vec3d &direction) const
{
    return localTransform(id).direction(direction);
}

Vec3d ConstructArticulationEngine::inverseTransformPointLocal(
    ConstructArticulationId id, const Vec3d &point) const
{
    return localTransform(id).inverse().point(point);
}

Vec3d ConstructArticulationEngine::transformPointWorld(
    ConstructArticulationId id, const Vec3d &point) const
{
    const auto state = joint(id);
    if (!state)
        throw std::out_of_range("articulation joint was not found");
    const auto construct = m_registry.find(state->definition.construct_id);
    if (!construct)
        throw std::out_of_range("articulation construct was not found");
    return construct->transform().localToWorld(transformPointLocal(id, point));
}

Vec3d ConstructArticulationEngine::transformDirectionWorld(
    ConstructArticulationId id, const Vec3d &direction) const
{
    const auto state = joint(id);
    if (!state)
        throw std::out_of_range("articulation joint was not found");
    const auto construct = m_registry.find(state->definition.construct_id);
    if (!construct)
        throw std::out_of_range("articulation construct was not found");
    return rotateYaw(transformDirectionLocal(id, direction),
        construct->transform().yaw_radians);
}

Vec3d ConstructArticulationEngine::inverseTransformPointWorld(
    ConstructArticulationId id, const Vec3d &point) const
{
    const auto state = joint(id);
    if (!state)
        throw std::out_of_range("articulation joint was not found");
    const auto construct = m_registry.find(state->definition.construct_id);
    if (!construct)
        throw std::out_of_range("articulation construct was not found");
    return inverseTransformPointLocal(id, construct->transform().worldToLocal(point));
}

std::vector<ConstructArticulationId> ConstructArticulationEngine::jointChain(
    ConstructArticulationId id) const
{
    std::vector<ConstructArticulationId> chain;
    std::size_t guard = 0;
    while (id != 0 && guard++ <= m_joints.size()) {
        const auto iterator = m_joints.find(id);
        if (iterator == m_joints.end())
            break;
        chain.push_back(id);
        id = iterator->second.definition.parent_id;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

Vec3d ConstructArticulationEngine::pointVelocityWorld(
    ConstructArticulationId id, const Vec3d &point) const
{
    const auto state = joint(id);
    if (!state)
        return {};
    const auto construct = m_registry.find(state->definition.construct_id);
    if (!construct)
        return {};
    constexpr double SAMPLE_DT = 1e-4;
    std::unordered_map<ConstructArticulationId, double> overrides;
    for (const auto chain_id : jointChain(id)) {
        const auto iterator = m_joints.find(chain_id);
        if (iterator != m_joints.end())
            overrides[chain_id] = iterator->second.position +
                iterator->second.velocity * SAMPLE_DT;
    }
    const Vec3d local_now = localTransform(id).point(point);
    const Vec3d local_next = localTransform(id, &overrides).point(point);
    const Vec3d world_now = construct->transform().localToWorld(local_now);
    ConstructTransform next_transform = construct->transform();
    next_transform.position += construct->linearVelocity() * SAMPLE_DT;
    next_transform.yaw_radians += construct->yawVelocity() * SAMPLE_DT;
    const Vec3d world_next = next_transform.localToWorld(local_next);
    return (world_next - world_now) * (1.0 / SAMPLE_DT);
}

ConstructTurretId ConstructArticulationEngine::configureTurret(
    ConstructTurretDefinition definition)
{
    if (definition.construct_id == 0 || !m_registry.find(definition.construct_id))
        throw std::invalid_argument("turret construct does not exist");
    const auto yaw = m_joints.find(definition.yaw_joint_id);
    if (yaw == m_joints.end() || yaw->second.definition.construct_id != definition.construct_id)
        throw std::invalid_argument("turret yaw articulation is invalid");
    if (definition.pitch_joint_id != 0) {
        const auto pitch = m_joints.find(definition.pitch_joint_id);
        if (pitch == m_joints.end() ||
                pitch->second.definition.construct_id != definition.construct_id ||
                !isDescendantOrSame(definition.pitch_joint_id, definition.yaw_joint_id))
            throw std::invalid_argument("turret pitch articulation is invalid");
    }
    if (!finiteVec(definition.muzzle_local) || !finiteVec(definition.forward_local) ||
            !finite(definition.alignment_tolerance_radians) ||
            definition.alignment_tolerance_radians < 0.0 ||
            !finite(definition.projectile_radius) || definition.projectile_radius < 0.0 ||
            !finite(definition.maximum_range) || definition.maximum_range <= 0.0)
        throw std::invalid_argument("turret definition is invalid");
    definition.forward_local = normalised(definition.forward_local);
    if (definition.id == 0)
        definition.id = m_next_turret_id++;
    else
        m_next_turret_id = std::max(m_next_turret_id, definition.id + 1);
    TurretRuntime runtime;
    const auto previous = m_turrets.find(definition.id);
    if (previous != m_turrets.end())
        runtime = previous->second;
    runtime.definition = definition;
    m_turrets[definition.id] = runtime;
    return definition.id;
}

bool ConstructArticulationEngine::removeTurret(ConstructTurretId id)
{
    return m_turrets.erase(id) != 0;
}

bool ConstructArticulationEngine::setTurretTarget(
    ConstructTurretId id, const Vec3d &target_world)
{
    const auto iterator = m_turrets.find(id);
    if (iterator == m_turrets.end() || !finiteVec(target_world))
        return false;
    iterator->second.target_world = target_world;
    iterator->second.has_target = true;
    return updateTurretTargets(iterator->second);
}

bool ConstructArticulationEngine::applyFireControlSolution(
    ConstructTurretId id, const FireControlSolution &solution)
{
    if (!solution.valid)
        return false;
    return setTurretTarget(id, solution.aim_point);
}

bool ConstructArticulationEngine::clearTurretTarget(ConstructTurretId id)
{
    const auto iterator = m_turrets.find(id);
    if (iterator == m_turrets.end())
        return false;
    iterator->second.has_target = false;
    return true;
}

std::optional<ConstructTurretStatus> ConstructArticulationEngine::turret(
    ConstructTurretId id) const
{
    const auto iterator = m_turrets.find(id);
    if (iterator == m_turrets.end())
        return std::nullopt;
    return calculateTurretStatus(iterator->second);
}

std::vector<ConstructTurretStatus> ConstructArticulationEngine::turrets(
    ConstructId construct_id) const
{
    std::vector<ConstructTurretStatus> result;
    for (const auto &[id, runtime] : m_turrets) {
        (void)id;
        if (construct_id == 0 || runtime.definition.construct_id == construct_id)
            result.push_back(calculateTurretStatus(runtime));
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.definition.id < right.definition.id;
    });
    return result;
}

bool ConstructArticulationEngine::updateTurretTargets(TurretRuntime &runtime)
{
    if (!runtime.has_target || !runtime.definition.enabled)
        return false;
    const auto construct = m_registry.find(runtime.definition.construct_id);
    const auto yaw_iterator = m_joints.find(runtime.definition.yaw_joint_id);
    if (!construct || yaw_iterator == m_joints.end())
        return false;
    const Vec3d target_construct = construct->transform().worldToLocal(runtime.target_world);

    const AffineTransform yaw_parent = parentTransform(runtime.definition.yaw_joint_id);
    const Vec3d target_yaw_parent = yaw_parent.inverse().point(target_construct);
    const auto &yaw_definition = yaw_iterator->second.definition;
    const Vec3d yaw_to_target = target_yaw_parent - yaw_definition.pivot_local;
    Vec3d base_forward = runtime.definition.forward_local;
    if (runtime.definition.pitch_joint_id != 0) {
        const auto pitch_iterator = m_joints.find(runtime.definition.pitch_joint_id);
        if (pitch_iterator != m_joints.end())
            base_forward = pitch_iterator->second.definition.parent_id ==
                    runtime.definition.yaw_joint_id ?
                runtime.definition.forward_local : yaw_parent.inverse().direction(base_forward);
    }
    const double desired_yaw = clampPosition(yaw_definition,
        signedAngle(base_forward, yaw_to_target, yaw_definition.axis_local));
    (void)setJointTarget(runtime.definition.yaw_joint_id, desired_yaw);

    if (runtime.definition.pitch_joint_id != 0) {
        const auto pitch_iterator = m_joints.find(runtime.definition.pitch_joint_id);
        if (pitch_iterator != m_joints.end()) {
            std::unordered_map<ConstructArticulationId, double> overrides;
            overrides[runtime.definition.yaw_joint_id] = desired_yaw;
            const AffineTransform pitch_parent = parentTransform(
                runtime.definition.pitch_joint_id, &overrides);
            const Vec3d target_pitch_parent = pitch_parent.inverse().point(target_construct);
            const auto &pitch_definition = pitch_iterator->second.definition;
            const Vec3d pitch_to_target = target_pitch_parent - pitch_definition.pivot_local;
            const double desired_pitch = clampPosition(pitch_definition,
                signedAngle(runtime.definition.forward_local, pitch_to_target,
                    pitch_definition.axis_local));
            (void)setJointTarget(runtime.definition.pitch_joint_id, desired_pitch);
        }
    }
    return true;
}

ConstructTurretStatus ConstructArticulationEngine::calculateTurretStatus(
    const TurretRuntime &runtime) const
{
    ConstructTurretStatus result;
    result.definition = runtime.definition;
    result.target_world = runtime.target_world;
    result.has_target = runtime.has_target;
    const auto construct = m_registry.find(runtime.definition.construct_id);
    const auto yaw = m_joints.find(runtime.definition.yaw_joint_id);
    if (!construct || yaw == m_joints.end())
        return result;
    result.current_yaw = yaw->second.position;
    result.desired_yaw = yaw->second.target_position;
    ConstructArticulationId terminal = runtime.definition.yaw_joint_id;
    if (runtime.definition.pitch_joint_id != 0) {
        const auto pitch = m_joints.find(runtime.definition.pitch_joint_id);
        if (pitch != m_joints.end()) {
            terminal = runtime.definition.pitch_joint_id;
            result.current_pitch = pitch->second.position;
            result.desired_pitch = pitch->second.target_position;
        }
    }
    const Vec3d muzzle_local = transformPointLocal(terminal, runtime.definition.muzzle_local);
    const Vec3d forward_local = safeNormalised(
        transformDirectionLocal(terminal, runtime.definition.forward_local));
    result.muzzle_world = construct->transform().localToWorld(muzzle_local);
    result.forward_world = safeNormalised(rotateYaw(forward_local,
        construct->transform().yaw_radians));
    if (runtime.has_target) {
        const Vec3d desired = safeNormalised(runtime.target_world - result.muzzle_world);
        if (lengthSquared(desired) > EPSILON && lengthSquared(result.forward_world) > EPSILON)
            result.angle_error = std::acos(clamp(dot(result.forward_world, desired), -1.0, 1.0));
        result.aligned = result.angle_error <= runtime.definition.alignment_tolerance_radians;
        result.obstructed = !lineOfFireClear(runtime.definition.id,
            runtime.target_world, &result.obstruction);
    }
    return result;
}

bool ConstructArticulationEngine::isDescendantOrSame(
    ConstructArticulationId candidate, ConstructArticulationId ancestor) const
{
    ConstructArticulationId current = candidate;
    std::size_t guard = 0;
    while (current != 0 && guard++ <= m_joints.size()) {
        if (current == ancestor)
            return true;
        const auto iterator = m_joints.find(current);
        if (iterator == m_joints.end())
            break;
        current = iterator->second.definition.parent_id;
    }
    return false;
}

std::vector<ConstructArticulationId> ConstructArticulationEngine::turretExclusions(
    const ConstructTurretDefinition &turret) const
{
    std::vector<ConstructArticulationId> result;
    for (const auto &[id, state] : m_joints) {
        if (state.definition.construct_id != turret.construct_id)
            continue;
        if (isDescendantOrSame(id, turret.yaw_joint_id))
            result.push_back(id);
    }
    return result;
}

std::optional<ConstructArticulationObstruction>
ConstructArticulationEngine::raycastConstructLocal(
    ConstructId construct_id,
    const Vec3d &local_start,
    const Vec3d &local_end,
    double radius,
    const std::vector<ConstructArticulationId> &excluded_joints) const
{
    if (construct_id == 0 || !finiteVec(local_start) || !finiteVec(local_end) ||
            !finite(radius) || radius < 0.0)
        return std::nullopt;
    const auto construct = m_registry.find(construct_id);
    if (!construct)
        return std::nullopt;
    std::unordered_set<ConstructArticulationId> excluded;
    for (const auto id : excluded_joints) {
        excluded.insert(id);
        for (const auto &[candidate_id, state] : m_joints) {
            (void)state;
            if (isDescendantOrSame(candidate_id, id))
                excluded.insert(candidate_id);
        }
    }

    const double segment_length = length(local_end - local_start);
    double best_t = std::numeric_limits<double>::infinity();
    std::optional<ConstructArticulationObstruction> result;
    for (const auto &entry : construct->nodes()) {
        ConstructArticulationId owner = 0;
        const auto owner_iterator = m_node_owners.find({construct_id, entry.position});
        if (owner_iterator != m_node_owners.end())
            owner = owner_iterator->second;
        if (owner != 0 && excluded.find(owner) != excluded.end())
            continue;
        if (owner != 0) {
            const auto joint_iterator = m_joints.find(owner);
            if (joint_iterator == m_joints.end() ||
                    !joint_iterator->second.definition.collision_enabled)
                continue;
        }
        Vec3d start = local_start;
        Vec3d end = local_end;
        if (owner != 0) {
            const AffineTransform inverse = localTransform(owner).inverse();
            start = inverse.point(start);
            end = inverse.point(end);
        }
        const Vec3d minimum{
            static_cast<double>(entry.position.x) - radius,
            static_cast<double>(entry.position.y) - radius,
            static_cast<double>(entry.position.z) - radius,
        };
        const Vec3d maximum{
            static_cast<double>(entry.position.x + 1) + radius,
            static_cast<double>(entry.position.y + 1) + radius,
            static_cast<double>(entry.position.z + 1) + radius,
        };
        double hit_t = 0.0;
        if (!segmentAabb(start, end, minimum, maximum, hit_t) || hit_t >= best_t)
            continue;
        best_t = hit_t;
        const Vec3d hit_local = local_start + (local_end - local_start) * hit_t;
        ConstructArticulationObstruction obstruction;
        obstruction.construct_id = construct_id;
        obstruction.articulation_id = owner;
        obstruction.node_position = entry.position;
        obstruction.local_point = hit_local;
        obstruction.world_point = construct->transform().localToWorld(hit_local);
        obstruction.distance = segment_length * hit_t;
        result = obstruction;
    }
    return result;
}

bool ConstructArticulationEngine::lineOfFireClear(
    ConstructTurretId turret_id,
    const Vec3d &target_world,
    std::optional<ConstructArticulationObstruction> *obstruction) const
{
    const auto runtime_iterator = m_turrets.find(turret_id);
    if (runtime_iterator == m_turrets.end() || !finiteVec(target_world))
        return false;
    const auto &definition = runtime_iterator->second.definition;
    const auto construct = m_registry.find(definition.construct_id);
    if (!construct)
        return false;
    ConstructArticulationId terminal = definition.pitch_joint_id != 0 ?
        definition.pitch_joint_id : definition.yaw_joint_id;
    const Vec3d muzzle_local = transformPointLocal(terminal, definition.muzzle_local);
    Vec3d target_local = construct->transform().worldToLocal(target_world);
    Vec3d direction = safeNormalised(target_local - muzzle_local);
    const double target_distance = length(target_local - muzzle_local);
    if (target_distance > definition.maximum_range)
        target_local = muzzle_local + direction * definition.maximum_range;
    const Vec3d start = muzzle_local + direction * (definition.projectile_radius + 1e-4);
    const auto hit = raycastConstructLocal(definition.construct_id, start, target_local,
        definition.projectile_radius, turretExclusions(definition));
    if (obstruction)
        *obstruction = hit;
    return !hit.has_value();
}

ConstructArticulationStepResult ConstructArticulationEngine::step(
    double delta_seconds, double server_time)
{
    if (!finite(delta_seconds) || delta_seconds < 0.0 || !finite(server_time))
        throw std::invalid_argument("articulation step time is invalid");
    for (auto &[id, runtime] : m_turrets) {
        (void)id;
        if (runtime.has_target && runtime.definition.stabilised)
            (void)updateTurretTargets(runtime);
    }

    ConstructArticulationStepResult result;
    for (auto &[id, state] : m_joints) {
        if (!state.definition.enabled)
            continue;
        const double before_position = state.position;
        const double before_velocity = state.velocity;
        double desired_velocity = 0.0;
        if (state.definition.control_mode == ConstructJointControlMode::Velocity) {
            desired_velocity = state.commanded_velocity;
        } else {
            if (state.definition.control_mode == ConstructJointControlMode::Oscillate) {
                state.target_position = state.oscillating_forward ?
                    state.definition.maximum_position : state.definition.minimum_position;
                if (std::abs(state.position - state.target_position) <= 1e-5)
                    state.oscillating_forward = !state.oscillating_forward;
            }
            const double error = state.target_position - state.position;
            if (std::abs(error) > 1e-8) {
                const double braking_speed = state.definition.maximum_acceleration > EPSILON ?
                    std::sqrt(2.0 * state.definition.maximum_acceleration * std::abs(error)) :
                    state.definition.maximum_speed;
                desired_velocity = (error > 0.0 ? 1.0 : -1.0) *
                    std::min(state.definition.maximum_speed, braking_speed);
            }
        }
        const double velocity_delta = desired_velocity - state.velocity;
        const double maximum_delta = state.definition.maximum_acceleration * delta_seconds;
        state.velocity += clamp(velocity_delta, -maximum_delta, maximum_delta);
        if (state.definition.maximum_acceleration <= EPSILON)
            state.velocity = desired_velocity;
        state.velocity = clamp(state.velocity,
            -state.definition.maximum_speed, state.definition.maximum_speed);
        state.position += state.velocity * delta_seconds;
        const double clamped_position = clampPosition(state.definition, state.position);
        if (clamped_position != state.position) {
            state.position = clamped_position;
            if ((state.position <= state.definition.minimum_position && state.velocity < 0.0) ||
                    (state.position >= state.definition.maximum_position && state.velocity > 0.0))
                state.velocity = 0.0;
        }
        if (state.definition.control_mode != ConstructJointControlMode::Velocity &&
                (state.target_position - before_position) *
                    (state.target_position - state.position) <= 0.0) {
            state.position = state.target_position;
            state.velocity = 0.0;
        }
        if (std::abs(state.position - before_position) > 1e-10 ||
                std::abs(state.velocity - before_velocity) > 1e-10) {
            ++state.revision;
            result.changed_joints.push_back({
                state.definition.construct_id,
                id,
                state.revision,
                server_time,
                state.position,
                state.target_position,
                state.velocity,
                state.definition.enabled,
            });
        }
    }
    result.turrets = turrets();
    return result;
}

void ConstructArticulationEngine::removeConstruct(ConstructId construct_id)
{
    clearConstruct(construct_id);
}

double ConstructArticulationEngine::clampPosition(
    const ConstructArticulationDefinition &definition, double value) noexcept
{
    return clamp(value, definition.minimum_position, definition.maximum_position);
}

const char *constructJointKindName(ConstructJointKind kind) noexcept
{
    switch (kind) {
    case ConstructJointKind::Revolute: return "revolute";
    case ConstructJointKind::Prismatic: return "prismatic";
    }
    return "unknown";
}

const char *constructJointControlModeName(ConstructJointControlMode mode) noexcept
{
    switch (mode) {
    case ConstructJointControlMode::Position: return "position";
    case ConstructJointControlMode::Velocity: return "velocity";
    case ConstructJointControlMode::Oscillate: return "oscillate";
    }
    return "unknown";
}

} // namespace navycraft

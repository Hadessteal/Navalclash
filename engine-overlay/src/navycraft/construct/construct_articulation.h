// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_fire_control.h"
#include "construct_registry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace navycraft {

using ConstructArticulationId = std::uint64_t;
using ConstructTurretId = std::uint64_t;

enum class ConstructJointKind : std::uint8_t {
    Revolute = 0,
    Prismatic = 1,
};

enum class ConstructJointControlMode : std::uint8_t {
    Position = 0,
    Velocity = 1,
    Oscillate = 2,
};

struct ConstructArticulationDefinition {
    ConstructArticulationId id = 0;
    ConstructId construct_id = 0;
    ConstructArticulationId parent_id = 0;
    std::string name;
    ConstructJointKind kind = ConstructJointKind::Revolute;
    ConstructJointControlMode control_mode = ConstructJointControlMode::Position;
    Vec3d pivot_local{};
    Vec3d axis_local{0.0, 1.0, 0.0};
    std::vector<LocalNodePos> nodes;
    double minimum_position = -3.14159265358979323846;
    double maximum_position = 3.14159265358979323846;
    double maximum_speed = 1.0;
    double maximum_acceleration = 2.0;
    bool enabled = true;
    bool render_enabled = true;
    bool collision_enabled = true;
};

struct ConstructArticulationState {
    ConstructArticulationDefinition definition{};
    double position = 0.0;
    double target_position = 0.0;
    double velocity = 0.0;
    double commanded_velocity = 0.0;
    std::uint64_t revision = 0;
    bool oscillating_forward = true;
};

struct ConstructArticulationSnapshot {
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    std::uint64_t sequence = 0;
    double server_time = 0.0;
    double position = 0.0;
    double target_position = 0.0;
    double velocity = 0.0;
    bool enabled = true;
};

struct ConstructArticulationObstruction {
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    LocalNodePos node_position{};
    Vec3d local_point{};
    Vec3d world_point{};
    double distance = 0.0;
};

struct ConstructTurretDefinition {
    ConstructTurretId id = 0;
    ConstructId construct_id = 0;
    ConstructArticulationId yaw_joint_id = 0;
    ConstructArticulationId pitch_joint_id = 0;
    std::string name;
    Vec3d muzzle_local{};
    Vec3d forward_local{0.0, 0.0, 1.0};
    double alignment_tolerance_radians = 0.017453292519943295;
    double projectile_radius = 0.05;
    double maximum_range = 500.0;
    bool stabilised = true;
    bool enabled = true;
};

struct ConstructTurretStatus {
    ConstructTurretDefinition definition{};
    Vec3d target_world{};
    Vec3d muzzle_world{};
    Vec3d forward_world{};
    double desired_yaw = 0.0;
    double desired_pitch = 0.0;
    double current_yaw = 0.0;
    double current_pitch = 0.0;
    double angle_error = 0.0;
    bool has_target = false;
    bool aligned = false;
    bool obstructed = false;
    std::optional<ConstructArticulationObstruction> obstruction;
};

struct ConstructArticulationStepResult {
    std::vector<ConstructArticulationSnapshot> changed_joints;
    std::vector<ConstructTurretStatus> turrets;
};

class ConstructArticulationEngine final {
public:
    struct AffineTransform;
    explicit ConstructArticulationEngine(ConstructRegistry &registry);

    ConstructArticulationId configureJoint(ConstructArticulationDefinition definition);
    bool removeJoint(ConstructArticulationId id);
    void clearConstruct(ConstructId construct_id);
    void clear();

    bool setJointTarget(ConstructArticulationId id, double target_position);
    bool setJointVelocity(ConstructArticulationId id, double velocity);
    bool setJointPosition(ConstructArticulationId id, double position);
    bool setJointEnabled(ConstructArticulationId id, bool enabled);

    [[nodiscard]] std::optional<ConstructArticulationState> joint(
        ConstructArticulationId id) const;
    [[nodiscard]] std::vector<ConstructArticulationState> joints(
        ConstructId construct_id = 0) const;
    [[nodiscard]] std::optional<ConstructArticulationId> nodeJoint(
        ConstructId construct_id, const LocalNodePos &position) const;

    [[nodiscard]] Vec3d transformPointLocal(
        ConstructArticulationId id, const Vec3d &point) const;
    [[nodiscard]] Vec3d transformDirectionLocal(
        ConstructArticulationId id, const Vec3d &direction) const;
    [[nodiscard]] Vec3d inverseTransformPointLocal(
        ConstructArticulationId id, const Vec3d &point) const;
    [[nodiscard]] Vec3d transformPointWorld(
        ConstructArticulationId id, const Vec3d &point) const;
    [[nodiscard]] Vec3d transformDirectionWorld(
        ConstructArticulationId id, const Vec3d &direction) const;
    [[nodiscard]] Vec3d inverseTransformPointWorld(
        ConstructArticulationId id, const Vec3d &point) const;
    [[nodiscard]] Vec3d pointVelocityWorld(
        ConstructArticulationId id, const Vec3d &point) const;
    [[nodiscard]] std::vector<ConstructArticulationId> jointChain(
        ConstructArticulationId id) const;

    ConstructTurretId configureTurret(ConstructTurretDefinition definition);
    bool removeTurret(ConstructTurretId id);
    bool setTurretTarget(ConstructTurretId id, const Vec3d &target_world);
    bool applyFireControlSolution(ConstructTurretId id,
        const FireControlSolution &solution);
    bool clearTurretTarget(ConstructTurretId id);
    [[nodiscard]] std::optional<ConstructTurretStatus> turret(
        ConstructTurretId id) const;
    [[nodiscard]] std::vector<ConstructTurretStatus> turrets(
        ConstructId construct_id = 0) const;

    [[nodiscard]] std::optional<ConstructArticulationObstruction> raycastConstructLocal(
        ConstructId construct_id,
        const Vec3d &local_start,
        const Vec3d &local_end,
        double radius = 0.0,
        const std::vector<ConstructArticulationId> &excluded_joints = {}) const;
    [[nodiscard]] bool lineOfFireClear(ConstructTurretId turret_id,
        const Vec3d &target_world,
        std::optional<ConstructArticulationObstruction> *obstruction = nullptr) const;

    [[nodiscard]] ConstructArticulationStepResult step(
        double delta_seconds, double server_time);
    void removeConstruct(ConstructId construct_id);

private:
    struct TurretRuntime {
        ConstructTurretDefinition definition{};
        Vec3d target_world{};
        bool has_target = false;
    };

    struct NodeKey {
        ConstructId construct_id = 0;
        LocalNodePos position{};
        bool operator==(const NodeKey &other) const noexcept
        {
            return construct_id == other.construct_id && position == other.position;
        }
    };

    struct NodeKeyHash {
        std::size_t operator()(const NodeKey &key) const noexcept;
    };

    [[nodiscard]] AffineTransform localTransform(ConstructArticulationId id,
        const std::unordered_map<ConstructArticulationId, double> *overrides = nullptr) const;
    [[nodiscard]] AffineTransform parentTransform(ConstructArticulationId id,
        const std::unordered_map<ConstructArticulationId, double> *overrides = nullptr) const;
    [[nodiscard]] bool isDescendantOrSame(ConstructArticulationId candidate,
        ConstructArticulationId ancestor) const;
    [[nodiscard]] std::vector<ConstructArticulationId> turretExclusions(
        const ConstructTurretDefinition &turret) const;
    [[nodiscard]] ConstructTurretStatus calculateTurretStatus(
        const TurretRuntime &runtime) const;
    bool updateTurretTargets(TurretRuntime &runtime);
    [[nodiscard]] static double clampPosition(
        const ConstructArticulationDefinition &definition, double value) noexcept;

    ConstructRegistry &m_registry;
    std::unordered_map<ConstructArticulationId, ConstructArticulationState> m_joints;
    std::unordered_map<NodeKey, ConstructArticulationId, NodeKeyHash> m_node_owners;
    std::unordered_map<ConstructTurretId, TurretRuntime> m_turrets;
    ConstructArticulationId m_next_joint_id = 1;
    ConstructTurretId m_next_turret_id = 1;
};

const char *constructJointKindName(ConstructJointKind kind) noexcept;
const char *constructJointControlModeName(ConstructJointControlMode mode) noexcept;

} // namespace navycraft

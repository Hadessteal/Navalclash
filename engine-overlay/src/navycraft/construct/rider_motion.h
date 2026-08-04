// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_geometry.h"
#include "platform_contact.h"

#include <vector>

namespace navycraft {

struct RiderMotionState {
    PlatformContactState contact{};
    Vec3d last_surface_velocity{};
    bool previous_jump_pressed = false;
    double unsupported_seconds = 0.0;
    LocalNodePos last_support_node{};
    bool has_last_support_node = false;
};

struct RiderMotionInput {
    Vec3d position{};
    Vec3d velocity{};
    Aabb3d world_box{};
    bool jump_pressed = false;
    bool detach_requested = false;
    bool inherit_platform_velocity_on_detach = true;
    bool allow_contact_acquisition = true;
    double delta_seconds = 0.0;
};

struct RiderMotionOutput {
    Vec3d position{};
    Vec3d velocity{};
    Vec3d platform_displacement{};
    Vec3d inherited_velocity{};
    ConstructId construct_id = 0;
    bool grounded = false;
    bool jumped = false;
    bool contact_started = false;
    bool contact_ended = false;
};

class RiderMotionSolver final {
public:
    static constexpr double DEFAULT_SUPPORT_DROP = 0.18;
    static constexpr double DEFAULT_SUPPORT_PENETRATION = 0.10;
    static constexpr double DEFAULT_CONTACT_GRACE = 0.10;
    static constexpr double SUPPORT_CORRECTION_SLOP = 0.015;
    static constexpr double MAX_SUPPORT_CORRECTION_PER_FRAME = 0.12;

    [[nodiscard]] static RiderMotionOutput step(
        RiderMotionState &state,
        const std::vector<const DynamicConstruct *> &constructs,
        const RiderMotionInput &input,
        double support_drop = DEFAULT_SUPPORT_DROP,
        double support_penetration = DEFAULT_SUPPORT_PENETRATION,
        double contact_grace = DEFAULT_CONTACT_GRACE) noexcept;
};

struct RiderCorrection {
    Vec3d position{};
    Vec3d correction{};
    bool corrected = false;
    bool hard_snap = false;
};

class RiderReconciler final {
public:
    [[nodiscard]] static RiderCorrection reconcile(
        const Vec3d &reported,
        const Vec3d &expected,
        double soft_distance = 0.35,
        double hard_distance = 3.0,
        double soft_fraction = 0.35) noexcept;
};

} // namespace navycraft

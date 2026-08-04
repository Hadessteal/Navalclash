// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "dynamic_construct.h"

namespace navycraft {

struct PlatformContactState {
    ConstructId construct_id = 0;
    Vec3d local_anchor{};
    ConstructTransform previous_transform{};
    bool active = false;
};

struct PlatformMotion {
    Vec3d world_anchor{};
    Vec3d displacement{};
    Vec3d surface_velocity{};
    double yaw_delta = 0.0;
};

class PlatformContact final {
public:
    [[nodiscard]] static PlatformContactState begin(
        const DynamicConstruct &construct, const Vec3d &world_anchor) noexcept;
    [[nodiscard]] static PlatformMotion update(
        PlatformContactState &state,
        const DynamicConstruct &construct,
        double delta_seconds) noexcept;
    static void reanchor(
        PlatformContactState &state,
        const DynamicConstruct &construct,
        const Vec3d &world_anchor) noexcept;
    static void clear(PlatformContactState &state) noexcept;
};

} // namespace navycraft

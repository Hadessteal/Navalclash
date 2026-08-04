// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_types.h"

#include <cstdint>

namespace navycraft {

struct ConstructTransformSnapshot {
    ConstructId id = 0;
    std::uint64_t sequence = 0;
    double server_time = 0.0;
    ConstructTransform transform{};
    Vec3d linear_velocity{};
    double yaw_velocity = 0.0;
};

class ConstructReplication final {
public:
    [[nodiscard]] static ConstructTransform interpolate(
        const ConstructTransformSnapshot &older,
        const ConstructTransformSnapshot &newer,
        double render_time) noexcept;

    [[nodiscard]] static ConstructTransform extrapolate(
        const ConstructTransformSnapshot &snapshot,
        double render_time,
        double maximum_seconds = 0.25) noexcept;
};

} // namespace navycraft

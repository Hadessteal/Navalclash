// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_types.h"

#include <cstdint>
#include <vector>

namespace navycraft {

struct RiderStatePacket {
    std::uint64_t sequence = 0;
    ConstructId construct_id = 0;
    std::uint64_t articulation_id = 0;
    Vec3d local_anchor{};
    Vec3d local_velocity{};
    Vec3d world_position{};
    Vec3d velocity{};
    double client_time = 0.0;
    bool grounded = false;
    bool jumping = false;
};

class RiderPacketCodec final {
public:
    static constexpr std::uint16_t VERSION = 3;
    static constexpr std::size_t MAX_PACKET_BYTES = 256;

    [[nodiscard]] static std::vector<std::uint8_t> encode(const RiderStatePacket &packet);
    [[nodiscard]] static RiderStatePacket decode(const std::vector<std::uint8_t> &bytes);
};

} // namespace navycraft

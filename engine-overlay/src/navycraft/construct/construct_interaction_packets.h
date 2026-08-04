// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_interaction.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace navycraft {

class ConstructInteractionPacketCodec final {
public:
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t MAX_PACKET_BYTES = 1024 * 1024;
    static constexpr std::size_t MAX_STRING_BYTES = 64 * 1024;
    static constexpr std::size_t MAX_FIELDS = 256;

    [[nodiscard]] static std::vector<std::uint8_t> encode(
        const ConstructInteractionRequest &request);
    [[nodiscard]] static ConstructInteractionRequest decode(
        const std::vector<std::uint8_t> &bytes);
};

} // namespace navycraft

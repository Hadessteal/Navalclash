// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_replication.h"
#include "construct_section.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace navycraft {

class ConstructPacketCodec final {
public:
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t MAX_SECTION_NODES = 4096;
    static constexpr std::size_t MAX_PACKET_STRING_BYTES = 1024U * 1024U;
    static constexpr std::size_t MAX_PACKET_BYTES = 16U * 1024U * 1024U;

    [[nodiscard]] static std::vector<std::uint8_t> encodeTransform(
        const ConstructTransformSnapshot &snapshot);
    [[nodiscard]] static ConstructTransformSnapshot decodeTransform(
        const std::vector<std::uint8_t> &bytes);

    [[nodiscard]] static std::vector<std::uint8_t> encodeSection(
        ConstructId construct_id, const ConstructSection &section);
    [[nodiscard]] static ConstructSection decodeSection(
        const std::vector<std::uint8_t> &bytes, ConstructId &construct_id);

    [[nodiscard]] static std::vector<std::uint8_t> encodeRemove(ConstructId construct_id);
    [[nodiscard]] static ConstructId decodeRemove(const std::vector<std::uint8_t> &bytes);
};

} // namespace navycraft

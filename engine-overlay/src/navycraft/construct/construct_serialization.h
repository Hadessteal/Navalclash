// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "dynamic_construct.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace navycraft {

class ConstructSerialization final {
public:
    static constexpr std::uint16_t FORMAT_VERSION = 3;
    static constexpr std::size_t MAX_NODES = 1000000;
    static constexpr std::size_t MAX_STRING_BYTES = 16 * 1024 * 1024;

    [[nodiscard]] static std::vector<std::uint8_t> encode(const DynamicConstruct &construct);
    [[nodiscard]] static std::shared_ptr<DynamicConstruct> decode(
        const std::vector<std::uint8_t> &bytes);
};

} // namespace navycraft

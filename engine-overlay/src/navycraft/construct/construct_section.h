// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "dynamic_construct.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace navycraft {

constexpr std::int32_t CONSTRUCT_SECTION_SIZE = 16;

struct ConstructSectionPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    bool operator==(const ConstructSectionPos &other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct ConstructSectionPosHash {
    std::size_t operator()(const ConstructSectionPos &value) const noexcept;
};

struct ConstructSection {
    ConstructSectionPos position{};
    std::vector<ConstructNodeEntry> nodes;
    std::uint64_t revision = 0;
};

class ConstructSectionIndex final {
public:
    void rebuild(const DynamicConstruct &construct);
    void updateNode(const LocalNodePos &position, const ConstructNode *node);
    void clear();
    [[nodiscard]] const ConstructSection *find(const ConstructSectionPos &position) const noexcept;
    [[nodiscard]] std::vector<ConstructSectionPos> positions() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::vector<ConstructSectionPos> consumeDirty();

    [[nodiscard]] static ConstructSectionPos sectionFor(const LocalNodePos &position) noexcept;

private:
    std::unordered_map<ConstructSectionPos, ConstructSection, ConstructSectionPosHash> m_sections;
    std::uint64_t m_revision = 0;
    std::unordered_map<ConstructSectionPos, bool, ConstructSectionPosHash> m_dirty;
};

} // namespace navycraft

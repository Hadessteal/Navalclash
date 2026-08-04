// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_section.h"

#include <algorithm>
#include <functional>

namespace navycraft {
namespace {
LocalNodePos sectionOrigin(const ConstructSectionPos &position) noexcept
{
    return {position.x * CONSTRUCT_SECTION_SIZE,
        position.y * CONSTRUCT_SECTION_SIZE,
        position.z * CONSTRUCT_SECTION_SIZE};
}

bool positionLess(const ConstructNodeEntry &left, const ConstructNodeEntry &right) noexcept
{
    if (left.position.y != right.position.y)
        return left.position.y < right.position.y;
    if (left.position.z != right.position.z)
        return left.position.z < right.position.z;
    return left.position.x < right.position.x;
}

std::int32_t floorDivide(std::int32_t value, std::int32_t divisor) noexcept
{
    const std::int32_t quotient = value / divisor;
    const std::int32_t remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}
}

std::size_t ConstructSectionPosHash::operator()(const ConstructSectionPos &value) const noexcept
{
    std::size_t seed = std::hash<std::int32_t>{}(value.x);
    seed ^= std::hash<std::int32_t>{}(value.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    seed ^= std::hash<std::int32_t>{}(value.z) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

ConstructSectionPos ConstructSectionIndex::sectionFor(const LocalNodePos &position) noexcept
{
    return {
        floorDivide(position.x, CONSTRUCT_SECTION_SIZE),
        floorDivide(position.y, CONSTRUCT_SECTION_SIZE),
        floorDivide(position.z, CONSTRUCT_SECTION_SIZE),
    };
}

void ConstructSectionIndex::rebuild(const DynamicConstruct &construct)
{
    m_sections.clear();
    m_dirty.clear();
    ++m_revision;
    for (const auto &entry : construct.nodes()) {
        const auto section_position = sectionFor(entry.position);
        auto &section = m_sections[section_position];
        section.position = section_position;
        section.revision = m_revision;
        section.nodes.push_back(entry);
        m_dirty[section_position] = true;
    }
    for (auto &[position, section] : m_sections) {
        (void)position;
        std::sort(section.nodes.begin(), section.nodes.end(), positionLess);
    }
}


void ConstructSectionIndex::updateNode(const LocalNodePos &position, const ConstructNode *node)
{
    const ConstructSectionPos section_position = sectionFor(position);
    ++m_revision;
    auto iterator = m_sections.find(section_position);
    if (node) {
        if (iterator == m_sections.end()) {
            ConstructSection section;
            section.position = section_position;
            iterator = m_sections.emplace(section_position, std::move(section)).first;
        }
        auto &entries = iterator->second.nodes;
        const auto existing = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) {
            return entry.position == position;
        });
        if (existing == entries.end())
            entries.push_back({position, *node});
        else
            existing->node = *node;
        std::sort(entries.begin(), entries.end(), positionLess);
        iterator->second.revision = m_revision;
    } else if (iterator != m_sections.end()) {
        auto &entries = iterator->second.nodes;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto &entry) {
            return entry.position == position;
        }), entries.end());
        if (entries.empty())
            m_sections.erase(iterator);
        else
            iterator->second.revision = m_revision;
    }

    m_dirty[section_position] = true;
    const LocalNodePos origin = sectionOrigin(section_position);
    const int local_x = position.x - origin.x;
    const int local_y = position.y - origin.y;
    const int local_z = position.z - origin.z;
    if (local_x == 0)
        m_dirty[{section_position.x - 1, section_position.y, section_position.z}] = true;
    if (local_x == CONSTRUCT_SECTION_SIZE - 1)
        m_dirty[{section_position.x + 1, section_position.y, section_position.z}] = true;
    if (local_y == 0)
        m_dirty[{section_position.x, section_position.y - 1, section_position.z}] = true;
    if (local_y == CONSTRUCT_SECTION_SIZE - 1)
        m_dirty[{section_position.x, section_position.y + 1, section_position.z}] = true;
    if (local_z == 0)
        m_dirty[{section_position.x, section_position.y, section_position.z - 1}] = true;
    if (local_z == CONSTRUCT_SECTION_SIZE - 1)
        m_dirty[{section_position.x, section_position.y, section_position.z + 1}] = true;
}

void ConstructSectionIndex::clear()
{
    m_sections.clear();
    m_dirty.clear();
    ++m_revision;
}

const ConstructSection *ConstructSectionIndex::find(const ConstructSectionPos &position) const noexcept
{
    const auto iterator = m_sections.find(position);
    return iterator == m_sections.end() ? nullptr : &iterator->second;
}

std::vector<ConstructSectionPos> ConstructSectionIndex::positions() const
{
    std::vector<ConstructSectionPos> result;
    result.reserve(m_sections.size());
    for (const auto &[position, section] : m_sections) {
        (void)section;
        result.push_back(position);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.y != right.y)
            return left.y < right.y;
        if (left.z != right.z)
            return left.z < right.z;
        return left.x < right.x;
    });
    return result;
}

std::size_t ConstructSectionIndex::size() const noexcept
{
    return m_sections.size();
}

std::uint64_t ConstructSectionIndex::revision() const noexcept
{
    return m_revision;
}


std::vector<ConstructSectionPos> ConstructSectionIndex::consumeDirty()
{
    std::vector<ConstructSectionPos> result;
    result.reserve(m_dirty.size());
    for (const auto &[position, dirty] : m_dirty) {
        if (dirty)
            result.push_back(position);
    }
    m_dirty.clear();
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.y != right.y)
            return left.y < right.y;
        if (left.z != right.z)
            return left.z < right.z;
        return left.x < right.x;
    });
    return result;
}

} // namespace navycraft

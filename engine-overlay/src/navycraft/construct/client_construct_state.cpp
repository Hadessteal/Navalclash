// SPDX-License-Identifier: LGPL-2.1-or-later
#include "client_construct_state.h"

#include "construct_geometry.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace navycraft {
namespace {
const std::array<ConstructSectionPos, 7> SELF_AND_NEIGHBOURS{{
    {0, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
}};

ConstructSectionPos added(
    const ConstructSectionPos &left, const ConstructSectionPos &right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

bool sectionPosLess(const ConstructSectionPos &left, const ConstructSectionPos &right) noexcept
{
    if (left.y != right.y)
        return left.y < right.y;
    if (left.z != right.z)
        return left.z < right.z;
    return left.x < right.x;
}
}

ClientConstructState::ClientConstructState(ConstructId id) : m_id(id), m_construct(id)
{
}

ConstructId ClientConstructState::id() const noexcept
{
    return m_id;
}

bool ClientConstructState::pushTransform(const ConstructTransformSnapshot &snapshot)
{
    if (snapshot.id != m_id || !m_snapshots.push(snapshot))
        return false;
    m_construct.setTransform(snapshot.transform);
    m_construct.setLinearVelocity(snapshot.linear_velocity);
    m_construct.setYawVelocity(snapshot.yaw_velocity);
    return true;
}

bool ClientConstructState::applySection(const ConstructSection &section)
{
    if (section.nodes.size() > ConstructPacketCodec::MAX_SECTION_NODES)
        return false;
    const auto existing = m_sections.find(section.position);
    if (existing != m_sections.end() && existing->second.revision >= section.revision)
        return false;
    if (existing != m_sections.end()) {
        for (const auto &entry : existing->second.nodes)
            m_construct.removeNode(entry.position);
    }
    m_sections[section.position] = section;
    for (const auto &entry : section.nodes)
        m_construct.setNode(entry.position, entry.node);
    markSectionAndNeighboursDirty(section.position);
    return true;
}

bool ClientConstructState::removeSection(const ConstructSectionPos &position)
{
    const auto iterator = m_sections.find(position);
    if (iterator == m_sections.end())
        return false;
    for (const auto &entry : iterator->second.nodes)
        m_construct.removeNode(entry.position);
    m_sections.erase(iterator);
    m_meshes.erase(position);
    markSectionAndNeighboursDirty(position);
    return true;
}

ConstructTransform ClientConstructState::sampleTransform(
    double client_time,
    double interpolation_delay,
    double maximum_extrapolation) const noexcept
{
    if (m_snapshots.empty())
        return m_construct.transform();
    return m_snapshots.sample(client_time, interpolation_delay, maximum_extrapolation);
}

void ClientConstructState::setSampledTransform(const ConstructTransform &transform) noexcept
{
    m_construct.setTransform(transform);
}

std::vector<ConstructSectionPos> ClientConstructState::rebuildDirtyMeshes()
{
    std::vector<ConstructSectionPos> rebuilt;
    rebuilt.reserve(m_dirty.size());
    for (const auto &[position, dirty] : m_dirty) {
        if (!dirty)
            continue;
        const auto section = m_sections.find(position);
        if (section == m_sections.end()) {
            m_meshes.erase(position);
            rebuilt.push_back(position);
            continue;
        }
        m_meshes[position] = ConstructMesher::buildSection(m_construct, section->second);
        rebuilt.push_back(position);
    }
    m_dirty.clear();
    std::sort(rebuilt.begin(), rebuilt.end(), sectionPosLess);
    return rebuilt;
}

const ConstructSectionMesh *ClientConstructState::mesh(
    const ConstructSectionPos &position) const noexcept
{
    const auto iterator = m_meshes.find(position);
    return iterator == m_meshes.end() ? nullptr : &iterator->second;
}

const ConstructSection *ClientConstructState::section(
    const ConstructSectionPos &position) const noexcept
{
    const auto iterator = m_sections.find(position);
    return iterator == m_sections.end() ? nullptr : &iterator->second;
}

const DynamicConstruct &ClientConstructState::construct() const noexcept
{
    return m_construct;
}

std::vector<ConstructSectionPos> ClientConstructState::sectionPositions() const
{
    std::vector<ConstructSectionPos> result;
    result.reserve(m_sections.size());
    for (const auto &[position, section] : m_sections) {
        (void)section;
        result.push_back(position);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.x != right.x) return left.x < right.x;
        if (left.y != right.y) return left.y < right.y;
        return left.z < right.z;
    });
    return result;
}

std::size_t ClientConstructState::sectionCount() const noexcept
{
    return m_sections.size();
}

std::size_t ClientConstructState::meshCount() const noexcept
{
    return m_meshes.size();
}

void ClientConstructState::markSectionAndNeighboursDirty(const ConstructSectionPos &position)
{
    for (const auto &offset : SELF_AND_NEIGHBOURS)
        m_dirty[added(position, offset)] = true;
}

std::shared_ptr<ClientConstructState> ClientConstructManager::ensure(ConstructId id)
{
    if (id == 0)
        throw std::invalid_argument("client construct id 0 is reserved");
    const auto iterator = m_states.find(id);
    if (iterator != m_states.end())
        return iterator->second;
    auto state = std::make_shared<ClientConstructState>(id);
    m_states.emplace(id, state);
    return state;
}

std::shared_ptr<ClientConstructState> ClientConstructManager::find(ConstructId id) const
{
    const auto iterator = m_states.find(id);
    return iterator == m_states.end() ? nullptr : iterator->second;
}

bool ClientConstructManager::remove(ConstructId id)
{
    return m_states.erase(id) != 0;
}

bool ClientConstructManager::applyTransformPacket(
    const std::vector<std::uint8_t> &packet, double client_receive_time)
{
    const auto snapshot = ConstructPacketCodec::decodeTransform(packet);
    m_network_clock.observe(snapshot.server_time, client_receive_time);
    return ensure(snapshot.id)->pushTransform(snapshot);
}

bool ClientConstructManager::applySectionPacket(const std::vector<std::uint8_t> &packet)
{
    ConstructId id = 0;
    const auto section = ConstructPacketCodec::decodeSection(packet, id);
    return ensure(id)->applySection(section);
}

bool ClientConstructManager::applyRemovePacket(const std::vector<std::uint8_t> &packet)
{
    return remove(ConstructPacketCodec::decodeRemove(packet));
}

void ClientConstructManager::clear() noexcept
{
    m_states.clear();
    m_network_clock.reset();
}

std::vector<ConstructId> ClientConstructManager::ids() const
{
    std::vector<ConstructId> result;
    result.reserve(m_states.size());
    for (const auto &[id, state] : m_states) {
        (void)state;
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<ConstructRaycastHit> ClientConstructManager::raycast(
    const Vec3d &world_start, const Vec3d &world_end) const noexcept
{
    std::optional<ConstructRaycastHit> closest;
    for (const auto &[id, state] : m_states) {
        (void)id;
        const auto hit = ConstructGeometry::raycast(state->construct(), world_start, world_end);
        if (hit && (!closest || hit->distance < closest->distance))
            closest = hit;
    }
    return closest;
}

std::vector<const DynamicConstruct *> ClientConstructManager::constructs() const
{
    std::vector<const DynamicConstruct *> result;
    result.reserve(m_states.size());
    for (const auto &[id, state] : m_states) {
        (void)id;
        if (state)
            result.push_back(&state->construct());
    }
    return result;
}

double ClientConstructManager::serverTime(double client_time) const noexcept
{
    return m_network_clock.serverTime(client_time);
}

const ConstructNetworkClock &ClientConstructManager::networkClock() const noexcept
{
    return m_network_clock;
}

} // namespace navycraft

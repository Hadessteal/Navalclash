// SPDX-License-Identifier: LGPL-2.1-or-later
#include "dynamic_construct.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace navycraft {
namespace {
constexpr double TWO_PI = 6.28318530717958647692;

double normaliseYaw(double value) noexcept
{
    value = std::fmod(value, TWO_PI);
    if (value < 0.0)
        value += TWO_PI;
    return value;
}
}

DynamicConstruct::DynamicConstruct(ConstructId id) : m_id(id)
{
    if (id == 0)
        throw std::invalid_argument("construct id 0 is reserved");
}

ConstructId DynamicConstruct::id() const noexcept
{
    return m_id;
}

const std::string &DynamicConstruct::owner() const noexcept
{
    return m_owner;
}

void DynamicConstruct::setOwner(std::string owner)
{
    m_owner = std::move(owner);
}

const ConstructTransform &DynamicConstruct::transform() const noexcept
{
    return m_transform;
}

void DynamicConstruct::setTransform(const ConstructTransform &transform) noexcept
{
    m_transform = transform;
    m_transform.yaw_radians = normaliseYaw(m_transform.yaw_radians);
}

const Vec3d &DynamicConstruct::linearVelocity() const noexcept
{
    return m_linear_velocity;
}

void DynamicConstruct::setLinearVelocity(const Vec3d &velocity) noexcept
{
    m_linear_velocity = velocity;
}

double DynamicConstruct::yawVelocity() const noexcept
{
    return m_yaw_velocity;
}

void DynamicConstruct::setYawVelocity(double radians_per_second) noexcept
{
    m_yaw_velocity = radians_per_second;
}

void DynamicConstruct::step(double delta_seconds) noexcept
{
    if (!(delta_seconds > 0.0) || !std::isfinite(delta_seconds))
        return;
    m_transform.position += m_linear_velocity * delta_seconds;
    m_transform.yaw_radians = normaliseYaw(
        m_transform.yaw_radians + m_yaw_velocity * delta_seconds);
}

bool DynamicConstruct::setNode(const LocalNodePos &position, ConstructNode node)
{
    (void)removeLiquid(position);
    auto [iterator, inserted] = m_nodes.insert_or_assign(position, std::move(node));
    (void)iterator;
    ++m_node_revision;
    return inserted;
}

bool DynamicConstruct::removeNode(const LocalNodePos &position)
{
    const bool removed = m_nodes.erase(position) != 0;
    if (removed) {
        m_node_states.remove(position);
        ++m_node_revision;
    }
    return removed;
}

const ConstructNode *DynamicConstruct::getNode(const LocalNodePos &position) const noexcept
{
    const auto iterator = m_nodes.find(position);
    return iterator == m_nodes.end() ? nullptr : &iterator->second;
}

std::vector<ConstructNodeEntry> DynamicConstruct::nodes() const
{
    std::vector<ConstructNodeEntry> result;
    result.reserve(m_nodes.size());
    for (const auto &[position, node] : m_nodes)
        result.push_back({position, node});
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.position.y != right.position.y)
            return left.position.y < right.position.y;
        if (left.position.z != right.position.z)
            return left.position.z < right.position.z;
        return left.position.x < right.position.x;
    });
    return result;
}

LocalBounds DynamicConstruct::localBounds() const noexcept
{
    LocalBounds result;
    for (const auto &[position, node] : m_nodes) {
        (void)node;
        if (!result.valid) {
            result.min = position;
            result.max = position;
            result.valid = true;
            continue;
        }
        result.min.x = std::min(result.min.x, position.x);
        result.min.y = std::min(result.min.y, position.y);
        result.min.z = std::min(result.min.z, position.z);
        result.max.x = std::max(result.max.x, position.x);
        result.max.y = std::max(result.max.y, position.y);
        result.max.z = std::max(result.max.z, position.z);
    }
    return result;
}

std::size_t DynamicConstruct::nodeCount() const noexcept
{
    return m_nodes.size();
}

std::uint64_t DynamicConstruct::nodeRevision() const noexcept
{
    return m_node_revision;
}

bool DynamicConstruct::empty() const noexcept
{
    return m_nodes.empty();
}

bool DynamicConstruct::setLiquid(
    const LocalNodePos &position, ConstructLiquidCell liquid)
{
    if (liquid.level == 0 || liquid.liquid_name.empty())
        return removeLiquid(position);
    if (liquid.level > 8)
        throw std::invalid_argument("construct liquid level exceeds eight units");
    if (getNode(position))
        throw std::invalid_argument("construct liquid cannot occupy a solid node");
    const auto existing = m_liquids.find(position);
    if (existing != m_liquids.end() && existing->second == liquid)
        return false;
    m_liquids[position] = std::move(liquid);
    ++m_liquid_revision;
    return true;
}

bool DynamicConstruct::removeLiquid(const LocalNodePos &position)
{
    const bool removed = m_liquids.erase(position) != 0;
    if (removed)
        ++m_liquid_revision;
    return removed;
}

const ConstructLiquidCell *DynamicConstruct::getLiquid(
    const LocalNodePos &position) const noexcept
{
    const auto iterator = m_liquids.find(position);
    return iterator == m_liquids.end() ? nullptr : &iterator->second;
}

std::vector<ConstructLiquidEntry> DynamicConstruct::liquids() const
{
    std::vector<ConstructLiquidEntry> result;
    result.reserve(m_liquids.size());
    for (const auto &[position, liquid] : m_liquids)
        result.push_back({position, liquid});
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.position.y != right.position.y)
            return left.position.y < right.position.y;
        if (left.position.z != right.position.z)
            return left.position.z < right.position.z;
        return left.position.x < right.position.x;
    });
    return result;
}

std::size_t DynamicConstruct::liquidCellCount() const noexcept
{
    return m_liquids.size();
}

std::uint64_t DynamicConstruct::liquidRevision() const noexcept
{
    return m_liquid_revision;
}

const ConstructNodeState *DynamicConstruct::getNodeState(
    const LocalNodePos &position) const noexcept
{
    return m_node_states.find(position);
}

ConstructNodeState *DynamicConstruct::getNodeState(const LocalNodePos &position) noexcept
{
    return m_node_states.find(position);
}

ConstructNodeState &DynamicConstruct::ensureNodeState(const LocalNodePos &position)
{
    if (!getNode(position))
        throw std::invalid_argument("cannot create state for a missing construct node");
    return m_node_states.ensure(position);
}

bool DynamicConstruct::setNodeMetadataField(
    const LocalNodePos &position, std::string key, std::string value)
{
    (void)ensureNodeState(position);
    return m_node_states.setField(position, std::move(key), std::move(value));
}

bool DynamicConstruct::eraseNodeMetadataField(
    const LocalNodePos &position, const std::string &key)
{
    return m_node_states.eraseField(position, key);
}

bool DynamicConstruct::setNodeInventory(
    const LocalNodePos &position, std::string list_name, ConstructInventoryList list)
{
    (void)ensureNodeState(position);
    return m_node_states.setInventory(position, std::move(list_name), std::move(list));
}

bool DynamicConstruct::eraseNodeInventory(
    const LocalNodePos &position, const std::string &list_name)
{
    return m_node_states.eraseInventory(position, list_name);
}

bool DynamicConstruct::startNodeTimer(
    const LocalNodePos &position, double timeout, double elapsed)
{
    (void)ensureNodeState(position);
    return m_node_states.startTimer(position, timeout, elapsed);
}

bool DynamicConstruct::stopNodeTimer(const LocalNodePos &position)
{
    return m_node_states.stopTimer(position);
}

std::vector<ConstructTimerEvent> DynamicConstruct::stepNodeTimers(double delta_seconds)
{
    return m_node_states.stepTimers(delta_seconds);
}

std::vector<std::pair<LocalNodePos, ConstructNodeState>> DynamicConstruct::nodeStates() const
{
    return m_node_states.entries();
}

bool DynamicConstruct::setNodeState(const LocalNodePos &position, ConstructNodeState state)
{
    if (!getNode(position))
        throw std::invalid_argument("cannot set state for a missing construct node");
    auto &current = m_node_states.ensure(position);
    if (current == state)
        return false;
    current = std::move(state);
    return true;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_node_state.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace navycraft {
namespace {
constexpr std::size_t MAX_NAME_BYTES = 1024;
constexpr std::size_t MAX_FIELD_VALUE_BYTES = 16 * 1024 * 1024;
constexpr std::size_t MAX_INVENTORY_SLOTS = 65535;
constexpr std::size_t MAX_STACK_STRING_BYTES = 1024 * 1024;
}

const ConstructNodeState *ConstructNodeStateStore::find(
    const LocalNodePos &position) const noexcept
{
    const auto iterator = m_states.find(position);
    return iterator == m_states.end() ? nullptr : &iterator->second;
}

ConstructNodeState *ConstructNodeStateStore::find(const LocalNodePos &position) noexcept
{
    const auto iterator = m_states.find(position);
    return iterator == m_states.end() ? nullptr : &iterator->second;
}

ConstructNodeState &ConstructNodeStateStore::ensure(const LocalNodePos &position)
{
    return m_states[position];
}

bool ConstructNodeStateStore::remove(const LocalNodePos &position) noexcept
{
    return m_states.erase(position) != 0;
}

void ConstructNodeStateStore::clear() noexcept
{
    m_states.clear();
}

void ConstructNodeStateStore::validateName(const std::string &value, const char *label)
{
    if (value.empty())
        throw std::invalid_argument(std::string(label) + " cannot be empty");
    if (value.size() > MAX_NAME_BYTES)
        throw std::length_error(std::string(label) + " is too long");
}

void ConstructNodeStateStore::validateList(const ConstructInventoryList &list)
{
    if (list.stacks.size() > MAX_INVENTORY_SLOTS)
        throw std::length_error("construct inventory has too many slots");
    if (list.width > list.stacks.size() && !list.stacks.empty())
        throw std::invalid_argument("construct inventory width exceeds list size");
    for (const auto &stack : list.stacks) {
        if (stack.size() > MAX_STACK_STRING_BYTES)
            throw std::length_error("construct item stack string is too large");
    }
}

void ConstructNodeStateStore::bump(ConstructNodeState &state) noexcept
{
    ++state.revision;
    if (state.revision == 0)
        ++state.revision;
}

bool ConstructNodeStateStore::setField(
    const LocalNodePos &position, std::string key, std::string value)
{
    validateName(key, "metadata field name");
    if (value.size() > MAX_FIELD_VALUE_BYTES)
        throw std::length_error("metadata field value is too large");
    auto &state = ensure(position);
    const auto iterator = state.fields.find(key);
    if (iterator != state.fields.end() && iterator->second == value)
        return false;
    state.fields[std::move(key)] = std::move(value);
    bump(state);
    return true;
}

bool ConstructNodeStateStore::eraseField(
    const LocalNodePos &position, const std::string &key)
{
    auto *state = find(position);
    if (!state || state->fields.erase(key) == 0)
        return false;
    bump(*state);
    return true;
}

bool ConstructNodeStateStore::setInventory(
    const LocalNodePos &position, std::string list_name, ConstructInventoryList list)
{
    validateName(list_name, "inventory list name");
    validateList(list);
    auto &state = ensure(position);
    const auto iterator = state.inventories.find(list_name);
    if (iterator != state.inventories.end() && iterator->second == list)
        return false;
    state.inventories[std::move(list_name)] = std::move(list);
    bump(state);
    return true;
}

bool ConstructNodeStateStore::eraseInventory(
    const LocalNodePos &position, const std::string &list_name)
{
    auto *state = find(position);
    if (!state || state->inventories.erase(list_name) == 0)
        return false;
    bump(*state);
    return true;
}

bool ConstructNodeStateStore::startTimer(
    const LocalNodePos &position, double timeout, double elapsed)
{
    if (!(timeout > 0.0) || !std::isfinite(timeout))
        throw std::invalid_argument("construct node timer timeout must be finite and positive");
    if (!(elapsed >= 0.0) || !std::isfinite(elapsed))
        throw std::invalid_argument("construct node timer elapsed must be finite and non-negative");
    auto &state = ensure(position);
    ConstructNodeTimer timer{timeout, std::min(elapsed, timeout), true};
    if (state.timer == timer)
        return false;
    state.timer = timer;
    bump(state);
    return true;
}

bool ConstructNodeStateStore::stopTimer(const LocalNodePos &position)
{
    auto *state = find(position);
    if (!state || !state->timer.active)
        return false;
    state->timer.active = false;
    state->timer.elapsed = 0.0;
    bump(*state);
    return true;
}

std::vector<ConstructTimerEvent> ConstructNodeStateStore::stepTimers(double delta_seconds)
{
    std::vector<ConstructTimerEvent> events;
    if (!(delta_seconds > 0.0) || !std::isfinite(delta_seconds))
        return events;
    for (auto &[position, state] : m_states) {
        if (!state.timer.active)
            continue;
        state.timer.elapsed += delta_seconds;
        if (state.timer.elapsed + 1e-12 < state.timer.timeout)
            continue;
        events.push_back({position, state.timer.elapsed, state.timer.timeout});
        state.timer.active = false;
        state.timer.elapsed = 0.0;
        bump(state);
    }
    std::sort(events.begin(), events.end(), [](const auto &left, const auto &right) {
        if (left.position.y != right.position.y)
            return left.position.y < right.position.y;
        if (left.position.z != right.position.z)
            return left.position.z < right.position.z;
        return left.position.x < right.position.x;
    });
    return events;
}

std::vector<std::pair<LocalNodePos, ConstructNodeState>>
ConstructNodeStateStore::entries() const
{
    std::vector<std::pair<LocalNodePos, ConstructNodeState>> result;
    result.reserve(m_states.size());
    for (const auto &entry : m_states)
        result.push_back(entry);
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.first.y != right.first.y)
            return left.first.y < right.first.y;
        if (left.first.z != right.first.z)
            return left.first.z < right.first.z;
        return left.first.x < right.first.x;
    });
    return result;
}

std::size_t ConstructNodeStateStore::size() const noexcept
{
    return m_states.size();
}

} // namespace navycraft

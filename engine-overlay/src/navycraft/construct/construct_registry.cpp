// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_registry.h"

#include <algorithm>
#include <mutex>
#include <limits>
#include <stdexcept>

namespace navycraft {

std::shared_ptr<DynamicConstruct> ConstructRegistry::create()
{
    for (;;) {
        const ConstructId id = m_next_id.fetch_add(1, std::memory_order_relaxed);
        if (id == 0)
            continue;
        std::unique_lock lock(m_mutex);
        if (m_constructs.find(id) != m_constructs.end())
            continue;
        auto construct = std::make_shared<DynamicConstruct>(id);
        m_constructs.emplace(id, construct);
        return construct;
    }
}

std::shared_ptr<DynamicConstruct> ConstructRegistry::createWithId(ConstructId id)
{
    if (id == 0)
        throw std::invalid_argument("construct id 0 is reserved");
    if (id == std::numeric_limits<ConstructId>::max())
        throw std::invalid_argument("maximum construct id cannot be imported");
    std::unique_lock lock(m_mutex);
    if (m_constructs.find(id) != m_constructs.end())
        return nullptr;
    auto construct = std::make_shared<DynamicConstruct>(id);
    m_constructs.emplace(id, construct);
    ConstructId expected = m_next_id.load(std::memory_order_relaxed);
    while (expected <= id && !m_next_id.compare_exchange_weak(
        expected, id + 1, std::memory_order_relaxed)) {
    }
    return construct;
}

std::shared_ptr<DynamicConstruct> ConstructRegistry::find(ConstructId id) const
{
    std::shared_lock lock(m_mutex);
    const auto iterator = m_constructs.find(id);
    return iterator == m_constructs.end() ? nullptr : iterator->second;
}

bool ConstructRegistry::importConstruct(
    std::shared_ptr<DynamicConstruct> construct, bool replace_existing)
{
    if (!construct || construct->id() == 0)
        return false;
    const ConstructId id = construct->id();
    std::unique_lock lock(m_mutex);
    const auto iterator = m_constructs.find(id);
    if (iterator != m_constructs.end() && !replace_existing)
        return false;
    m_constructs[id] = std::move(construct);
    ConstructId expected = m_next_id.load(std::memory_order_relaxed);
    while (expected <= id && !m_next_id.compare_exchange_weak(
        expected, id + 1, std::memory_order_relaxed)) {
    }
    return true;
}

bool ConstructRegistry::remove(ConstructId id)
{
    std::unique_lock lock(m_mutex);
    return m_constructs.erase(id) != 0;
}

void ConstructRegistry::clear()
{
    std::unique_lock lock(m_mutex);
    m_constructs.clear();
    m_next_id.store(1, std::memory_order_relaxed);
}

void ConstructRegistry::stepAll(double delta_seconds)
{
    const auto constructs = snapshot();
    for (const auto &construct : constructs)
        construct->step(delta_seconds);
}

std::vector<ConstructId> ConstructRegistry::ids() const
{
    std::shared_lock lock(m_mutex);
    std::vector<ConstructId> result;
    result.reserve(m_constructs.size());
    for (const auto &[id, construct] : m_constructs) {
        (void)construct;
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::shared_ptr<DynamicConstruct>> ConstructRegistry::snapshot() const
{
    std::shared_lock lock(m_mutex);
    std::vector<std::shared_ptr<DynamicConstruct>> result;
    result.reserve(m_constructs.size());
    for (const auto &[id, construct] : m_constructs) {
        (void)id;
        result.push_back(construct);
    }
    return result;
}

std::size_t ConstructRegistry::size() const
{
    std::shared_lock lock(m_mutex);
    return m_constructs.size();
}

} // namespace navycraft

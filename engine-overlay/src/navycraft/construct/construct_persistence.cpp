// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_persistence.h"

#include <stdexcept>
#include <utility>

namespace navycraft {

void ConstructPersistenceService::initialise(
    const std::string &path_value, ConstructRegistry &registry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto database = std::make_unique<ConstructDatabase>(path_value);
    auto constructs = database->loadAllConstructs();
    registry.clear();
    for (auto &construct : constructs) {
        if (!registry.importConstruct(std::move(construct), true))
            throw std::runtime_error("failed to import persisted NavyCraft construct");
    }
    m_database = std::move(database);
    m_path = path_value;
}

void ConstructPersistenceService::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_database)
        m_database->flush();
    m_database.reset();
    m_path.clear();
}

bool ConstructPersistenceService::initialised() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<bool>(m_database);
}

std::string ConstructPersistenceService::path() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_path;
}

void ConstructPersistenceService::syncAll(const ConstructRegistry &registry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_database)
        return;
    m_database->syncConstructs(registry.snapshot());
}

std::int64_t ConstructPersistenceService::recordMutation(
    const ConstructMutationRecord &mutation,
    const std::vector<std::uint8_t> &before_payload,
    const std::vector<std::uint8_t> &after_payload)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_database)
        return 0;
    return m_database->recordMutation(mutation, before_payload, after_payload);
}

std::vector<ConstructDatabaseAction> ConstructPersistenceService::recentActions(
    ConstructId construct_id, std::size_t limit) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_database ? m_database->recentActions(construct_id, limit) :
        std::vector<ConstructDatabaseAction>{};
}

std::shared_ptr<DynamicConstruct> ConstructPersistenceService::rollbackAction(
    std::int64_t action_id, ConstructRegistry &registry)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_database)
        return nullptr;
    auto construct = m_database->rollbackPayload(action_id);
    if (!construct)
        return nullptr;
    if (!registry.importConstruct(construct, true))
        return nullptr;
    m_database->saveConstruct(*construct);
    return construct;
}

std::size_t ConstructPersistenceService::constructCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_database ? m_database->constructCount() : 0;
}

std::size_t ConstructPersistenceService::actionCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_database ? m_database->actionCount() : 0;
}

void ConstructPersistenceService::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_database)
        m_database->flush();
}

ConstructPersistenceService &runtimeConstructPersistence() noexcept
{
    static ConstructPersistenceService service;
    return service;
}

} // namespace navycraft

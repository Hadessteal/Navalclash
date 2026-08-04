// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_database.h"
#include "construct_registry.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace navycraft {

class ConstructPersistenceService final {
public:
    void initialise(const std::string &path, ConstructRegistry &registry);
    void shutdown();
    [[nodiscard]] bool initialised() const noexcept;
    [[nodiscard]] std::string path() const;

    void syncAll(const ConstructRegistry &registry);
    [[nodiscard]] std::int64_t recordMutation(
        const ConstructMutationRecord &mutation,
        const std::vector<std::uint8_t> &before_payload,
        const std::vector<std::uint8_t> &after_payload);
    [[nodiscard]] std::vector<ConstructDatabaseAction> recentActions(
        ConstructId construct_id, std::size_t limit = 100) const;
    [[nodiscard]] std::shared_ptr<DynamicConstruct> rollbackAction(
        std::int64_t action_id, ConstructRegistry &registry);
    [[nodiscard]] std::size_t constructCount() const;
    [[nodiscard]] std::size_t actionCount() const;
    void flush();

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<ConstructDatabase> m_database;
    std::string m_path;
};

ConstructPersistenceService &runtimeConstructPersistence() noexcept;

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_interaction.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct sqlite3;

namespace navycraft {

struct ConstructDatabaseAction {
    std::int64_t action_id = 0;
    ConstructMutationRecord mutation{};
    std::vector<std::uint8_t> before_payload;
    std::vector<std::uint8_t> after_payload;
    std::int64_t created_at = 0;
};

class ConstructDatabase final {
public:
    explicit ConstructDatabase(const std::string &path);
    ~ConstructDatabase();

    ConstructDatabase(const ConstructDatabase &) = delete;
    ConstructDatabase &operator=(const ConstructDatabase &) = delete;

    void saveConstruct(const DynamicConstruct &construct);
    [[nodiscard]] std::shared_ptr<DynamicConstruct> loadConstruct(ConstructId id) const;
    [[nodiscard]] std::vector<std::shared_ptr<DynamicConstruct>> loadAllConstructs() const;
    bool deleteConstruct(ConstructId id);
    void syncConstructs(const std::vector<std::shared_ptr<DynamicConstruct>> &constructs);

    [[nodiscard]] std::int64_t recordMutation(
        const ConstructMutationRecord &mutation,
        const std::vector<std::uint8_t> &before_payload,
        const std::vector<std::uint8_t> &after_payload);
    [[nodiscard]] std::vector<ConstructDatabaseAction> recentActions(
        ConstructId construct_id, std::size_t limit = 100) const;
    [[nodiscard]] std::shared_ptr<DynamicConstruct> rollbackPayload(
        std::int64_t action_id) const;

    [[nodiscard]] std::size_t constructCount() const;
    [[nodiscard]] std::size_t actionCount() const;
    void flush();

private:
    void initialiseSchema();
    void execute(const char *sql) const;

    sqlite3 *m_database = nullptr;
};

} // namespace navycraft

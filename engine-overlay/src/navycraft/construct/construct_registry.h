// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "dynamic_construct.h"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace navycraft {

class ConstructRegistry final {
public:
    ConstructRegistry() = default;

    [[nodiscard]] std::shared_ptr<DynamicConstruct> create();
    [[nodiscard]] std::shared_ptr<DynamicConstruct> createWithId(ConstructId id);
    [[nodiscard]] std::shared_ptr<DynamicConstruct> find(ConstructId id) const;
    bool importConstruct(std::shared_ptr<DynamicConstruct> construct, bool replace_existing = false);
    bool remove(ConstructId id);
    void clear();
    void stepAll(double delta_seconds);
    [[nodiscard]] std::vector<ConstructId> ids() const;
    [[nodiscard]] std::vector<std::shared_ptr<DynamicConstruct>> snapshot() const;
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<ConstructId, std::shared_ptr<DynamicConstruct>> m_constructs;
    std::atomic<ConstructId> m_next_id{1};
};

} // namespace navycraft

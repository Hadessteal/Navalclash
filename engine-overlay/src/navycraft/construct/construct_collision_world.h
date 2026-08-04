// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_geometry.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace navycraft {

struct ConstructCollisionWorldStats {
    std::size_t construct_count = 0;
    std::size_t last_query_candidates = 0;
    std::size_t last_query_rejected = 0;
};

// Per-frame broad-phase index for moving constructs. The index owns no
// constructs; callers rebuild it after sampled transforms have been applied.
class ConstructCollisionWorld final {
public:
    void clear() noexcept;
    void rebuild(const std::vector<const DynamicConstruct *> &constructs,
        double prediction_seconds = 0.25);

    [[nodiscard]] std::vector<const DynamicConstruct *> query(
        const Aabb3d &world_box,
        const Vec3d &desired_delta = {},
        double delta_seconds = 0.0,
        ConstructId always_include = 0) const;

    [[nodiscard]] const DynamicConstruct *find(ConstructId id) const noexcept;
    [[nodiscard]] const ConstructCollisionWorldStats &stats() const noexcept;

private:
    struct Entry {
        const DynamicConstruct *construct = nullptr;
        Aabb3d current_bounds{};
        Aabb3d predicted_bounds{};
    };

    std::vector<Entry> m_entries;
    std::unordered_map<ConstructId, const DynamicConstruct *> m_by_id;
    mutable ConstructCollisionWorldStats m_stats{};
};

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_replication.h"

#include <cstddef>
#include <deque>

namespace navycraft {

class ConstructSnapshotBuffer final {
public:
    explicit ConstructSnapshotBuffer(std::size_t capacity = 32);

    bool push(const ConstructTransformSnapshot &snapshot);
    void clear() noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] ConstructTransform sample(
        double client_time,
        double interpolation_delay = 0.1,
        double maximum_extrapolation = 0.25) const noexcept;

private:
    std::size_t m_capacity;
    std::deque<ConstructTransformSnapshot> m_snapshots;
};

} // namespace navycraft

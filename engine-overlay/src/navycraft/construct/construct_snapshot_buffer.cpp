// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_snapshot_buffer.h"

#include <algorithm>

namespace navycraft {

ConstructSnapshotBuffer::ConstructSnapshotBuffer(std::size_t capacity) :
    m_capacity(std::max<std::size_t>(2, capacity))
{
}

bool ConstructSnapshotBuffer::push(const ConstructTransformSnapshot &snapshot)
{
    if (snapshot.id == 0)
        return false;
    if (!m_snapshots.empty()) {
        if (snapshot.id != m_snapshots.back().id ||
                snapshot.sequence <= m_snapshots.back().sequence ||
                snapshot.server_time < m_snapshots.back().server_time)
            return false;
    }
    m_snapshots.push_back(snapshot);
    while (m_snapshots.size() > m_capacity)
        m_snapshots.pop_front();
    return true;
}

void ConstructSnapshotBuffer::clear() noexcept
{
    m_snapshots.clear();
}

std::size_t ConstructSnapshotBuffer::size() const noexcept
{
    return m_snapshots.size();
}

bool ConstructSnapshotBuffer::empty() const noexcept
{
    return m_snapshots.empty();
}

ConstructTransform ConstructSnapshotBuffer::sample(
    double client_time,
    double interpolation_delay,
    double maximum_extrapolation) const noexcept
{
    if (m_snapshots.empty())
        return {};
    const double render_time = client_time - std::max(0.0, interpolation_delay);
    if (m_snapshots.size() == 1 || render_time <= m_snapshots.front().server_time)
        return m_snapshots.front().transform;

    for (std::size_t index = 1; index < m_snapshots.size(); ++index) {
        if (render_time <= m_snapshots[index].server_time) {
            return ConstructReplication::interpolate(
                m_snapshots[index - 1], m_snapshots[index], render_time);
        }
    }
    return ConstructReplication::extrapolate(
        m_snapshots.back(), render_time, std::max(0.0, maximum_extrapolation));
}

} // namespace navycraft

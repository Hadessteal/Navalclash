// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct/construct_packets.h"
#include "construct/construct_section.h"

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace navycraft {

enum class ConstructWireKind : std::uint8_t {
    Section,
    Transform,
    Remove,
    Reset,
    Effect,
    Projectile,
    Articulation,
};

struct ConstructWireMessage {
    ConstructWireKind kind = ConstructWireKind::Transform;
    ConstructId construct_id = 0;
    std::vector<std::uint8_t> payload;
    bool reliable = false;
};

class ConstructReplicationQueue final {
public:
    void queueFullConstruct(
        const DynamicConstruct &construct,
        const ConstructSectionIndex &sections,
        double server_time);
    void queueDirtySections(
        ConstructId construct_id,
        const ConstructSectionIndex &sections,
        const std::vector<ConstructSectionPos> &positions);
    void queueTransform(const DynamicConstruct &construct, double server_time);
    void queueRemove(ConstructId construct_id);
    void queueReset();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] ConstructWireMessage pop();
    void clear() noexcept;

private:
    std::uint64_t nextSequence(ConstructId construct_id);

    std::deque<ConstructWireMessage> m_messages;
    std::unordered_map<ConstructId, std::uint64_t> m_sequences;
};

} // namespace navycraft

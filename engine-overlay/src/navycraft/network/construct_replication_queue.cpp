// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_replication_queue.h"

#include <stdexcept>

namespace navycraft {

void ConstructReplicationQueue::queueFullConstruct(
    const DynamicConstruct &construct,
    const ConstructSectionIndex &sections,
    double server_time)
{
    queueTransform(construct, server_time);
    for (const auto &position : sections.positions()) {
        const auto *section = sections.find(position);
        if (!section)
            continue;
        m_messages.push_back({ConstructWireKind::Section, construct.id(),
            ConstructPacketCodec::encodeSection(construct.id(), *section), true});
    }
}

void ConstructReplicationQueue::queueDirtySections(
    ConstructId construct_id,
    const ConstructSectionIndex &sections,
    const std::vector<ConstructSectionPos> &positions)
{
    if (construct_id == 0)
        throw std::invalid_argument("construct id 0 is reserved");
    for (const auto &position : positions) {
        const auto *section = sections.find(position);
        if (!section)
            continue;
        m_messages.push_back({ConstructWireKind::Section, construct_id,
            ConstructPacketCodec::encodeSection(construct_id, *section), true});
    }
}

void ConstructReplicationQueue::queueTransform(
    const DynamicConstruct &construct, double server_time)
{
    ConstructTransformSnapshot snapshot;
    snapshot.id = construct.id();
    snapshot.sequence = nextSequence(construct.id());
    snapshot.server_time = server_time;
    snapshot.transform = construct.transform();
    snapshot.linear_velocity = construct.linearVelocity();
    snapshot.yaw_velocity = construct.yawVelocity();
    m_messages.push_back({ConstructWireKind::Transform, construct.id(),
        ConstructPacketCodec::encodeTransform(snapshot), false});
}

void ConstructReplicationQueue::queueRemove(ConstructId construct_id)
{
    m_messages.push_back({ConstructWireKind::Remove, construct_id,
        ConstructPacketCodec::encodeRemove(construct_id), true});
    m_sequences.erase(construct_id);
}

void ConstructReplicationQueue::queueReset()
{
    m_messages.push_back({ConstructWireKind::Reset, 0, {}, true});
    m_sequences.clear();
}

bool ConstructReplicationQueue::empty() const noexcept
{
    return m_messages.empty();
}

std::size_t ConstructReplicationQueue::size() const noexcept
{
    return m_messages.size();
}

ConstructWireMessage ConstructReplicationQueue::pop()
{
    if (m_messages.empty())
        throw std::out_of_range("construct replication queue is empty");
    auto message = std::move(m_messages.front());
    m_messages.pop_front();
    return message;
}

void ConstructReplicationQueue::clear() noexcept
{
    m_messages.clear();
    m_sequences.clear();
}

std::uint64_t ConstructReplicationQueue::nextSequence(ConstructId construct_id)
{
    auto &sequence = m_sequences[construct_id];
    return ++sequence;
}

} // namespace navycraft

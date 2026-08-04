// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_articulation.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace navycraft {

enum class ConstructArticulationPacketKind : std::uint8_t {
    Definition = 0,
    State = 1,
    Remove = 2,
    ResetConstruct = 3,
};

struct ConstructArticulationPacket {
    ConstructArticulationPacketKind kind = ConstructArticulationPacketKind::State;
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    ConstructArticulationDefinition definition{};
    ConstructArticulationSnapshot snapshot{};
};

class ConstructArticulationPacketCodec final {
public:
    static constexpr std::uint16_t VERSION = 1;
    static constexpr std::size_t MAX_NAME_BYTES = 4096;
    static constexpr std::size_t MAX_NODES = 65535;
    static constexpr std::size_t MAX_PACKET_BYTES = 2U * 1024U * 1024U;

    [[nodiscard]] static std::vector<std::uint8_t> encodeDefinition(
        const ConstructArticulationDefinition &definition);
    [[nodiscard]] static std::vector<std::uint8_t> encodeState(
        const ConstructArticulationSnapshot &snapshot);
    [[nodiscard]] static std::vector<std::uint8_t> encodeRemove(
        ConstructId construct_id, ConstructArticulationId articulation_id);
    [[nodiscard]] static std::vector<std::uint8_t> encodeResetConstruct(
        ConstructId construct_id);
    [[nodiscard]] static ConstructArticulationPacket decode(
        const std::vector<std::uint8_t> &bytes);
};

class ClientConstructArticulationState final {
public:
    bool applyPacket(const std::vector<std::uint8_t> &bytes);
    bool apply(const ConstructArticulationPacket &packet);
    void removeConstruct(ConstructId construct_id);
    void clear();

    [[nodiscard]] std::optional<ConstructArticulationDefinition> definition(
        ConstructArticulationId articulation_id) const;
    [[nodiscard]] std::optional<ConstructArticulationSnapshot> snapshot(
        ConstructArticulationId articulation_id) const;
    [[nodiscard]] std::vector<ConstructArticulationDefinition> definitions(
        ConstructId construct_id = 0) const;
    [[nodiscard]] std::vector<ConstructArticulationSnapshot> snapshots(
        ConstructId construct_id = 0) const;
    [[nodiscard]] double samplePosition(ConstructArticulationId articulation_id,
        double client_time, double interpolation_delay = 0.1,
        double maximum_extrapolation = 0.25) const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::optional<ConstructArticulationId> nodeJoint(
        ConstructId construct_id, const LocalNodePos &position) const;

private:
    struct SnapshotHistory {
        ConstructArticulationSnapshot older{};
        ConstructArticulationSnapshot newer{};
        bool have_older = false;
        bool have_newer = false;
    };
    struct NodeKey {
        ConstructId construct_id = 0;
        LocalNodePos position{};
        bool operator==(const NodeKey &other) const noexcept
        {
            return construct_id == other.construct_id && position == other.position;
        }
    };
    struct NodeKeyHash {
        std::size_t operator()(const NodeKey &key) const noexcept;
    };

    std::unordered_map<ConstructArticulationId, ConstructArticulationDefinition> m_definitions;
    std::unordered_map<ConstructArticulationId, SnapshotHistory> m_histories;
    std::unordered_map<NodeKey, ConstructArticulationId, NodeKeyHash> m_node_owners;
    std::uint64_t m_revision = 0;
};

} // namespace navycraft

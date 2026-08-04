// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_types.h"
#include "construct_node_state.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

struct ConstructNode {
    std::uint16_t content_id = 0;
    std::uint8_t param1 = 0;
    std::uint8_t param2 = 0;
    std::string node_name;
    std::string metadata_blob;

    bool operator==(const ConstructNode &other) const noexcept
    {
        return content_id == other.content_id && param1 == other.param1 &&
            param2 == other.param2 && node_name == other.node_name &&
            metadata_blob == other.metadata_blob;
    }
};

struct ConstructNodeEntry {
    LocalNodePos position{};
    ConstructNode node{};
};

struct ConstructLiquidCell {
    std::string liquid_name;
    std::uint8_t level = 0;
    std::uint8_t flags = 0;

    bool operator==(const ConstructLiquidCell &other) const noexcept
    {
        return liquid_name == other.liquid_name && level == other.level &&
            flags == other.flags;
    }
};

struct ConstructLiquidEntry {
    LocalNodePos position{};
    ConstructLiquidCell liquid{};
};

class DynamicConstruct final {
public:
    explicit DynamicConstruct(ConstructId id);

    [[nodiscard]] ConstructId id() const noexcept;
    [[nodiscard]] const std::string &owner() const noexcept;
    void setOwner(std::string owner);

    [[nodiscard]] const ConstructTransform &transform() const noexcept;
    void setTransform(const ConstructTransform &transform) noexcept;

    [[nodiscard]] const Vec3d &linearVelocity() const noexcept;
    void setLinearVelocity(const Vec3d &velocity) noexcept;
    [[nodiscard]] double yawVelocity() const noexcept;
    void setYawVelocity(double radians_per_second) noexcept;
    void step(double delta_seconds) noexcept;

    bool setNode(const LocalNodePos &position, ConstructNode node);
    bool removeNode(const LocalNodePos &position);

    [[nodiscard]] const ConstructNode *getNode(const LocalNodePos &position) const noexcept;
    [[nodiscard]] std::vector<ConstructNodeEntry> nodes() const;
    [[nodiscard]] LocalBounds localBounds() const noexcept;
    [[nodiscard]] std::size_t nodeCount() const noexcept;
    [[nodiscard]] std::uint64_t nodeRevision() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    bool setLiquid(const LocalNodePos &position, ConstructLiquidCell liquid);
    bool removeLiquid(const LocalNodePos &position);
    [[nodiscard]] const ConstructLiquidCell *getLiquid(
        const LocalNodePos &position) const noexcept;
    [[nodiscard]] std::vector<ConstructLiquidEntry> liquids() const;
    [[nodiscard]] std::size_t liquidCellCount() const noexcept;
    [[nodiscard]] std::uint64_t liquidRevision() const noexcept;

    [[nodiscard]] const ConstructNodeState *getNodeState(
        const LocalNodePos &position) const noexcept;
    [[nodiscard]] ConstructNodeState *getNodeState(const LocalNodePos &position) noexcept;
    [[nodiscard]] ConstructNodeState &ensureNodeState(const LocalNodePos &position);
    bool setNodeMetadataField(const LocalNodePos &position, std::string key, std::string value);
    bool eraseNodeMetadataField(const LocalNodePos &position, const std::string &key);
    bool setNodeInventory(const LocalNodePos &position, std::string list_name,
        ConstructInventoryList list);
    bool eraseNodeInventory(const LocalNodePos &position, const std::string &list_name);
    bool startNodeTimer(const LocalNodePos &position, double timeout, double elapsed = 0.0);
    bool stopNodeTimer(const LocalNodePos &position);
    [[nodiscard]] std::vector<ConstructTimerEvent> stepNodeTimers(double delta_seconds);
    [[nodiscard]] std::vector<std::pair<LocalNodePos, ConstructNodeState>> nodeStates() const;
    bool setNodeState(const LocalNodePos &position, ConstructNodeState state);

private:
    ConstructId m_id;
    std::string m_owner;
    ConstructTransform m_transform{};
    Vec3d m_linear_velocity{};
    double m_yaw_velocity = 0.0;
    std::unordered_map<LocalNodePos, ConstructNode, LocalNodePosHash> m_nodes;
    std::unordered_map<LocalNodePos, ConstructLiquidCell, LocalNodePosHash> m_liquids;
    ConstructNodeStateStore m_node_states;
    std::uint64_t m_node_revision = 0;
    std::uint64_t m_liquid_revision = 0;
};

} // namespace navycraft

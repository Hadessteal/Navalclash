// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_mesh.h"
#include "construct_geometry.h"
#include "construct_packets.h"
#include "construct_snapshot_buffer.h"
#include "construct_network_clock.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace navycraft {

class ClientConstructState final {
public:
    explicit ClientConstructState(ConstructId id);

    [[nodiscard]] ConstructId id() const noexcept;
    bool pushTransform(const ConstructTransformSnapshot &snapshot);
    bool applySection(const ConstructSection &section);
    bool removeSection(const ConstructSectionPos &position);
    [[nodiscard]] ConstructTransform sampleTransform(
        double client_time,
        double interpolation_delay = 0.1,
        double maximum_extrapolation = 0.25) const noexcept;
    void setSampledTransform(const ConstructTransform &transform) noexcept;
    [[nodiscard]] std::vector<ConstructSectionPos> rebuildDirtyMeshes();
    [[nodiscard]] const ConstructSectionMesh *mesh(
        const ConstructSectionPos &position) const noexcept;
    [[nodiscard]] const ConstructSection *section(
        const ConstructSectionPos &position) const noexcept;
    [[nodiscard]] const DynamicConstruct &construct() const noexcept;
    [[nodiscard]] std::vector<ConstructSectionPos> sectionPositions() const;
    [[nodiscard]] std::size_t sectionCount() const noexcept;
    [[nodiscard]] std::size_t meshCount() const noexcept;

private:
    void markSectionAndNeighboursDirty(const ConstructSectionPos &position);

    ConstructId m_id;
    DynamicConstruct m_construct;
    ConstructSnapshotBuffer m_snapshots;
    std::unordered_map<ConstructSectionPos, ConstructSection, ConstructSectionPosHash> m_sections;
    std::unordered_map<ConstructSectionPos, ConstructSectionMesh, ConstructSectionPosHash> m_meshes;
    std::unordered_map<ConstructSectionPos, bool, ConstructSectionPosHash> m_dirty;
};

class ClientConstructManager final {
public:
    [[nodiscard]] std::shared_ptr<ClientConstructState> ensure(ConstructId id);
    [[nodiscard]] std::shared_ptr<ClientConstructState> find(ConstructId id) const;
    bool remove(ConstructId id);
    bool applyTransformPacket(const std::vector<std::uint8_t> &packet, double client_receive_time);
    bool applySectionPacket(const std::vector<std::uint8_t> &packet);
    bool applyRemovePacket(const std::vector<std::uint8_t> &packet);
    void clear() noexcept;
    [[nodiscard]] std::vector<ConstructId> ids() const;
    [[nodiscard]] std::optional<ConstructRaycastHit> raycast(
        const Vec3d &world_start, const Vec3d &world_end) const noexcept;
    [[nodiscard]] std::vector<const DynamicConstruct *> constructs() const;
    [[nodiscard]] double serverTime(double client_time) const noexcept;
    [[nodiscard]] const ConstructNetworkClock &networkClock() const noexcept;

private:
    std::unordered_map<ConstructId, std::shared_ptr<ClientConstructState>> m_states;
    ConstructNetworkClock m_network_clock;
};

} // namespace navycraft

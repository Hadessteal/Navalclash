// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "dynamic_construct.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

enum class ConstructInteractionAction : std::uint8_t {
    StartDig = 0,
    StopDig = 1,
    DigComplete = 2,
    Place = 3,
    Use = 4,
    Activate = 5,
    ReceiveFields = 6,
    Timer = 7,
};

struct ConstructInteractionRequest {
    std::uint64_t sequence = 0;
    ConstructId construct_id = 0;
    ConstructInteractionAction action = ConstructInteractionAction::Use;
    LocalNodePos node_position{};
    LocalNodePos adjacent_position{};
    Vec3d local_point{};
    Vec3d world_point{};
    Vec3d world_normal{};
    std::string actor;
    std::string wielded_item;
    std::string form_name;
    std::vector<std::pair<std::string, std::string>> fields;
    double client_time = 0.0;
};

struct ConstructInteractionResult {
    bool accepted = false;
    bool node_changed = false;
    bool callback_required = false;
    LocalNodePos changed_position{};
    std::optional<ConstructNode> removed_node;
    std::string message;
};

struct ConstructCallbackEvent {
    std::uint64_t event_id = 0;
    ConstructInteractionRequest request{};
    std::string node_name;
    std::optional<ConstructNode> removed_node;
    double timer_elapsed = 0.0;
    double timer_timeout = 0.0;
};

struct ConstructMutationResolution {
    std::uint64_t event_id = 0;
    bool accepted = false;
    bool protected_violation = false;
    bool replace_existing = false;
    std::optional<ConstructNode> placed_node;
    std::optional<ConstructNodeState> placed_state;
    std::string wielded_item_after;
    std::vector<std::string> drops;
    std::string reason;
};

struct ConstructMutationRecord {
    std::uint64_t event_id = 0;
    ConstructId construct_id = 0;
    ConstructInteractionAction action = ConstructInteractionAction::Use;
    std::string actor;
    LocalNodePos position{};
    bool accepted = false;
    bool protected_violation = false;
    std::optional<ConstructNode> before_node;
    std::optional<ConstructNodeState> before_state;
    std::optional<ConstructNode> after_node;
    std::optional<ConstructNodeState> after_state;
    std::string wielded_item_before;
    std::string wielded_item_after;
    std::vector<std::string> drops;
    std::string reason;
};

class ConstructInteractionEngine final {
public:
    [[nodiscard]] ConstructInteractionResult apply(
        DynamicConstruct &construct, const ConstructInteractionRequest &request);
    [[nodiscard]] std::vector<ConstructCallbackEvent> stepTimers(
        DynamicConstruct &construct, double delta_seconds);
    [[nodiscard]] std::vector<ConstructCallbackEvent> drainEvents();
    bool resolveTimerEvent(
        DynamicConstruct &construct, std::uint64_t event_id, bool restart,
        double timeout_override = 0.0);
    [[nodiscard]] ConstructInteractionResult resolveMutationEvent(
        DynamicConstruct &construct, const ConstructMutationResolution &resolution);
    [[nodiscard]] std::vector<ConstructMutationRecord> drainMutationRecords();
    [[nodiscard]] std::size_t pendingEventCount() const noexcept;
    [[nodiscard]] std::size_t pendingMutationCount() const noexcept;

private:
    void queueEvent(ConstructCallbackEvent event);
    [[nodiscard]] std::uint64_t nextEventId() noexcept;

    std::uint64_t m_next_event_id = 1;
    std::vector<ConstructCallbackEvent> m_events;
    std::vector<ConstructCallbackEvent> m_unresolved_timer_events;
    std::unordered_map<std::uint64_t, ConstructCallbackEvent> m_pending_mutations;
    std::vector<ConstructMutationRecord> m_mutation_records;
};

} // namespace navycraft

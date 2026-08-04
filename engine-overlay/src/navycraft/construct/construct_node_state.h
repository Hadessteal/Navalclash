// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

struct ConstructInventoryList {
    std::uint16_t width = 0;
    std::vector<std::string> stacks;

    bool operator==(const ConstructInventoryList &other) const noexcept
    {
        return width == other.width && stacks == other.stacks;
    }
};

struct ConstructNodeTimer {
    double timeout = 0.0;
    double elapsed = 0.0;
    bool active = false;

    bool operator==(const ConstructNodeTimer &other) const noexcept
    {
        return timeout == other.timeout && elapsed == other.elapsed && active == other.active;
    }
};

struct ConstructNodeState {
    std::unordered_map<std::string, std::string> fields;
    std::unordered_map<std::string, ConstructInventoryList> inventories;
    ConstructNodeTimer timer{};
    std::uint64_t revision = 0;

    bool operator==(const ConstructNodeState &other) const noexcept
    {
        return fields == other.fields && inventories == other.inventories &&
            timer == other.timer && revision == other.revision;
    }
};

struct ConstructTimerEvent {
    LocalNodePos position{};
    double elapsed = 0.0;
    double timeout = 0.0;
};

class ConstructNodeStateStore final {
public:
    [[nodiscard]] const ConstructNodeState *find(const LocalNodePos &position) const noexcept;
    [[nodiscard]] ConstructNodeState *find(const LocalNodePos &position) noexcept;
    [[nodiscard]] ConstructNodeState &ensure(const LocalNodePos &position);
    bool remove(const LocalNodePos &position) noexcept;
    void clear() noexcept;

    bool setField(const LocalNodePos &position, std::string key, std::string value);
    bool eraseField(const LocalNodePos &position, const std::string &key);
    bool setInventory(const LocalNodePos &position, std::string list_name,
        ConstructInventoryList list);
    bool eraseInventory(const LocalNodePos &position, const std::string &list_name);
    bool startTimer(const LocalNodePos &position, double timeout, double elapsed = 0.0);
    bool stopTimer(const LocalNodePos &position);
    [[nodiscard]] std::vector<ConstructTimerEvent> stepTimers(double delta_seconds);
    [[nodiscard]] std::vector<std::pair<LocalNodePos, ConstructNodeState>> entries() const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    static void validateName(const std::string &value, const char *label);
    static void validateList(const ConstructInventoryList &list);
    static void bump(ConstructNodeState &state) noexcept;

    std::unordered_map<LocalNodePos, ConstructNodeState, LocalNodePosHash> m_states;
};

} // namespace navycraft

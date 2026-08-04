// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_registry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace navycraft {

using ConstructLiquidCompartmentId = std::uint64_t;
using ConstructLiquidPortId = std::uint64_t;

struct ConstructLiquidCompartmentDefinition {
    ConstructLiquidCompartmentId id = 0;
    ConstructId construct_id = 0;
    std::string name;
    std::vector<LocalNodePos> cells;
    bool sealed = true;
    bool allow_mixing = false;
    double horizontal_flow_rate = 8.0;
    double vertical_flow_rate = 16.0;
};

enum class ConstructLiquidPortKind : std::uint8_t {
    Source = 0,
    Drain = 1,
    Breach = 2,
};

struct ConstructLiquidPortDefinition {
    ConstructLiquidPortId id = 0;
    ConstructLiquidCompartmentId compartment_id = 0;
    LocalNodePos position{};
    ConstructLiquidPortKind kind = ConstructLiquidPortKind::Source;
    std::string liquid_name = "water";
    double units_per_second = 0.0;
    bool enabled = true;
};

struct ConstructLiquidCompartmentStatus {
    ConstructLiquidCompartmentDefinition definition{};
    std::uint64_t total_units = 0;
    std::uint64_t capacity_units = 0;
    double fill_fraction = 0.0;
    double escaped_units = 0.0;
    Vec3d centre_of_mass_local{};
    bool mixed = false;
};

struct ConstructLiquidStepResult {
    std::vector<ConstructLiquidCompartmentStatus> compartments;
    std::vector<LocalNodePos> changed_cells;
    double escaped_units = 0.0;
};

class ConstructLiquidEngine final {
public:
    explicit ConstructLiquidEngine(ConstructRegistry &registry);

    ConstructLiquidCompartmentId configureCompartment(
        ConstructLiquidCompartmentDefinition definition);
    bool removeCompartment(ConstructLiquidCompartmentId id);
    ConstructLiquidPortId configurePort(ConstructLiquidPortDefinition definition);
    bool removePort(ConstructLiquidPortId id);
    bool setPortEnabled(ConstructLiquidPortId id, bool enabled);

    bool setCell(ConstructId construct_id, const LocalNodePos &position,
        std::string liquid_name, std::uint8_t level, std::uint8_t flags = 0);
    [[nodiscard]] std::optional<ConstructLiquidCell> cell(
        ConstructId construct_id, const LocalNodePos &position) const;
    [[nodiscard]] std::vector<ConstructLiquidCompartmentStatus> compartments(
        ConstructId construct_id = 0) const;
    [[nodiscard]] ConstructLiquidStepResult step(double delta_seconds);

    void removeConstruct(ConstructId construct_id);
    void clear();

private:
    struct CompartmentRuntime {
        ConstructLiquidCompartmentDefinition definition{};
        std::unordered_set<LocalNodePos, LocalNodePosHash> cell_set;
        double escaped_units = 0.0;
    };

    struct PortRuntime {
        ConstructLiquidPortDefinition definition{};
        double fractional_units = 0.0;
    };

    [[nodiscard]] ConstructLiquidCompartmentStatus status(
        const CompartmentRuntime &runtime) const;
    [[nodiscard]] CompartmentRuntime *compartmentFor(
        ConstructLiquidCompartmentId id);
    [[nodiscard]] const CompartmentRuntime *compartmentFor(
        ConstructLiquidCompartmentId id) const;

    ConstructRegistry &m_registry;
    std::unordered_map<ConstructLiquidCompartmentId, CompartmentRuntime> m_compartments;
    std::unordered_map<ConstructLiquidPortId, PortRuntime> m_ports;
    ConstructLiquidCompartmentId m_next_compartment_id = 1;
    ConstructLiquidPortId m_next_port_id = 1;
};

const char *constructLiquidPortKindName(ConstructLiquidPortKind kind) noexcept;

} // namespace navycraft

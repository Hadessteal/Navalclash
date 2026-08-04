// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_registry.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

enum class ConstructStructuralRole : std::uint8_t {
    Vessel = 0,
    Primary = 1,
    Fragment = 2,
    Wreck = 3,
};

enum class ConstructStructuralEventKind : std::uint8_t {
    Updated = 0,
    Split = 1,
    Sinking = 2,
    Sunk = 3,
};

struct ConstructMaterialProperties {
    double mass = 0.7;
    double displaced_volume = 1.0;
    double structural_strength = 1.0;
    bool structural = true;
    bool watertight = true;
    bool control = false;
};

struct ConstructStructuralConfig {
    double water_level = 0.0;
    double gravity = 9.81;
    double fluid_density = 1.0;
    double flooding_rate = 0.12;
    double linear_water_drag = 0.8;
    double angular_water_drag = 0.6;
    double maximum_vertical_acceleration = 9.81;
    double sink_depth = 128.0;
    std::size_t minimum_fragment_nodes = 1;
    std::size_t maximum_enclosure_cells = 500000;
    bool split_enabled = true;
    bool apply_fragment_physics = true;
    bool apply_primary_physics = false;
    bool recenter_fragments = true;
};

struct ConstructStructuralComponent {
    std::vector<LocalNodePos> nodes;
    LocalBounds bounds{};
    Vec3d centre_of_mass{};
    double mass = 0.0;
    double solid_displaced_volume = 0.0;
    double enclosed_volume = 0.0;
    double structural_strength = 0.0;
    std::size_t control_nodes = 0;
    std::size_t watertight_nodes = 0;
};

struct ConstructStructuralAnalysis {
    ConstructId construct_id = 0;
    std::vector<ConstructStructuralComponent> components;
    std::size_t primary_component = 0;
};

struct ConstructStructuralState {
    ConstructId construct_id = 0;
    ConstructId source_construct_id = 0;
    ConstructStructuralRole role = ConstructStructuralRole::Vessel;
    std::uint64_t node_revision = 0;
    std::size_t component_count = 0;
    std::size_t node_count = 0;
    std::size_t control_nodes = 0;
    std::size_t breach_count = 0;
    double total_mass = 0.0;
    double solid_displaced_volume = 0.0;
    double enclosed_volume = 0.0;
    double submerged_volume = 0.0;
    double effective_displaced_volume = 0.0;
    double flooded_fraction = 0.0;
    double integrity = 1.0;
    double buoyancy_force = 0.0;
    double weight_force = 0.0;
    double vertical_acceleration = 0.0;
    bool afloat = false;
    bool sinking = false;
    bool sunk = false;
};

struct ConstructStructuralEvent {
    ConstructStructuralEventKind kind = ConstructStructuralEventKind::Updated;
    ConstructId construct_id = 0;
    ConstructId source_construct_id = 0;
    std::vector<ConstructId> fragment_ids;
    std::size_t moved_nodes = 0;
    ConstructStructuralState state{};
};

class ConstructStructureEngine final {
public:
    using MaterialResolver = std::function<ConstructMaterialProperties(const ConstructNode &)>;

    explicit ConstructStructureEngine(ConstructRegistry &registry);

    void setMaterialResolver(MaterialResolver resolver);
    [[nodiscard]] ConstructStructuralAnalysis analyse(const DynamicConstruct &construct,
        const ConstructStructuralConfig &config = {}) const;
    [[nodiscard]] std::vector<ConstructStructuralEvent> step(double delta_seconds,
        const ConstructStructuralConfig &config = {});
    [[nodiscard]] std::vector<ConstructStructuralEvent> forceSplit(ConstructId construct_id,
        const ConstructStructuralConfig &config = {});

    bool recordBreaches(ConstructId construct_id, std::size_t count = 1);
    bool setFloodedFraction(ConstructId construct_id, double fraction);
    bool remove(ConstructId construct_id);
    void clear();

    [[nodiscard]] std::optional<ConstructStructuralState> state(ConstructId construct_id) const;
    [[nodiscard]] std::vector<ConstructStructuralState> states() const;

private:
    struct RuntimeState {
        ConstructStructuralState public_state{};
        double initial_strength = 0.0;
        double previous_enclosed_volume = 0.0;
        bool had_control = false;
        bool sinking_reported = false;
        bool sunk_reported = false;
    };

    [[nodiscard]] ConstructMaterialProperties materialFor(const ConstructNode &node) const;
    [[nodiscard]] std::vector<ConstructStructuralEvent> processConstruct(
        const std::shared_ptr<DynamicConstruct> &construct,
        double delta_seconds, const ConstructStructuralConfig &config, bool force_split);
    [[nodiscard]] ConstructStructuralState updateState(DynamicConstruct &construct,
        const ConstructStructuralAnalysis &analysis, double delta_seconds,
        const ConstructStructuralConfig &config);
    [[nodiscard]] std::vector<ConstructStructuralEvent> splitConstruct(
        DynamicConstruct &construct, const ConstructStructuralAnalysis &analysis,
        const ConstructStructuralConfig &config);

    ConstructRegistry &m_registry;
    MaterialResolver m_material_resolver;
    std::unordered_map<ConstructId, RuntimeState> m_states;
};

const char *constructStructuralRoleName(ConstructStructuralRole role) noexcept;
const char *constructStructuralEventName(ConstructStructuralEventKind kind) noexcept;

} // namespace navycraft

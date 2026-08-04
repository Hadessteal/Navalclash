// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_articulated_motion.h"
#include "construct_registry.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace navycraft {

struct ConstructSpecialNodeDefinition {
    std::string node_name;
    bool climbable = false;
    bool breathable = true;
    bool liquid_permeable = false;
    bool attachable_platform = true;
    double damage_per_second = 0.0;
    Vec3d conveyor_velocity_local{};
    double conveyor_acceleration = 8.0;
};

struct ConstructSpecialNodeContact {
    ConstructId construct_id = 0;
    ConstructArticulationId articulation_id = 0;
    LocalNodePos position{};
    ConstructSpecialNodeDefinition definition{};
    Vec3d world_velocity{};
};

struct ConstructSpecialBodyInput {
    Aabb3d world_box{};
    Vec3d velocity{};
    bool climbing_input = false;
    double delta_seconds = 0.0;
};

struct ConstructSpecialBodyOutput {
    Vec3d velocity{};
    Vec3d conveyor_delta{};
    double damage = 0.0;
    double immersed_fraction = 0.0;
    bool climbing = false;
    bool breathable = true;
    std::vector<ConstructSpecialNodeContact> contacts;
};

class ConstructSpecialNodeEngine final {
public:
    bool registerDefinition(ConstructSpecialNodeDefinition definition);
    bool removeDefinition(const std::string &node_name);
    [[nodiscard]] std::optional<ConstructSpecialNodeDefinition> definition(
        const std::string &node_name) const;
    [[nodiscard]] std::vector<ConstructSpecialNodeDefinition> definitions() const;

    [[nodiscard]] ConstructSpecialBodyOutput evaluate(
        const std::vector<const DynamicConstruct *> &constructs,
        const ConstructArticulationEngine *articulations,
        const ConstructSpecialBodyInput &input) const noexcept;

    void clear();

private:
    std::unordered_map<std::string, ConstructSpecialNodeDefinition> m_definitions;
};

} // namespace navycraft

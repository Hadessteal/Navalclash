// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "construct_registry.h"

#include <cstddef>

namespace navycraft {

struct ConstructSimulationConfig {
    double fixed_step_seconds = 1.0 / 60.0;
    double maximum_frame_seconds = 0.25;
    std::size_t maximum_substeps = 16;
    double replication_interval_seconds = 1.0 / 20.0;
};

struct ConstructSimulationAdvance {
    std::size_t fixed_steps = 0;
    std::size_t replication_ticks = 0;
    double simulated_seconds = 0.0;
    double dropped_seconds = 0.0;
    double simulation_time = 0.0;
};

class ConstructSimulation final {
public:
    explicit ConstructSimulation(ConstructSimulationConfig config = {});

    [[nodiscard]] ConstructSimulationAdvance advance(
        ConstructRegistry &registry, double frame_seconds) noexcept;
    void reset(double simulation_time = 0.0) noexcept;

    [[nodiscard]] double simulationTime() const noexcept;
    [[nodiscard]] double interpolationAlpha() const noexcept;
    [[nodiscard]] const ConstructSimulationConfig &config() const noexcept;

private:
    ConstructSimulationConfig m_config;
    double m_accumulator = 0.0;
    double m_replication_accumulator = 0.0;
    double m_simulation_time = 0.0;
};

} // namespace navycraft

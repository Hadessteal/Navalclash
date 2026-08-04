// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_simulation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace navycraft {
namespace {
bool validPositive(double value) noexcept
{
    return value > 0.0 && std::isfinite(value);
}
}

ConstructSimulation::ConstructSimulation(ConstructSimulationConfig config) :
    m_config(config)
{
    if (!validPositive(m_config.fixed_step_seconds))
        throw std::invalid_argument("fixed construct step must be positive and finite");
    if (!validPositive(m_config.maximum_frame_seconds))
        throw std::invalid_argument("maximum construct frame must be positive and finite");
    if (m_config.maximum_frame_seconds < m_config.fixed_step_seconds)
        throw std::invalid_argument("maximum construct frame is smaller than fixed step");
    if (m_config.maximum_substeps == 0)
        throw std::invalid_argument("construct simulation requires at least one substep");
    if (!validPositive(m_config.replication_interval_seconds))
        throw std::invalid_argument("construct replication interval must be positive and finite");
}

ConstructSimulationAdvance ConstructSimulation::advance(
    ConstructRegistry &registry, double frame_seconds) noexcept
{
    ConstructSimulationAdvance result;
    result.simulation_time = m_simulation_time;
    if (!(frame_seconds > 0.0) || !std::isfinite(frame_seconds))
        return result;

    const double accepted = std::min(frame_seconds, m_config.maximum_frame_seconds);
    result.dropped_seconds = std::max(0.0, frame_seconds - accepted);
    m_accumulator += accepted;

    constexpr double epsilon = 1e-12;
    while (m_accumulator + epsilon >= m_config.fixed_step_seconds &&
            result.fixed_steps < m_config.maximum_substeps) {
        registry.stepAll(m_config.fixed_step_seconds);
        m_accumulator -= m_config.fixed_step_seconds;
        if (m_accumulator < 0.0 && m_accumulator > -epsilon)
            m_accumulator = 0.0;
        m_simulation_time += m_config.fixed_step_seconds;
        m_replication_accumulator += m_config.fixed_step_seconds;
        ++result.fixed_steps;
    }

    if (m_accumulator + epsilon >= m_config.fixed_step_seconds) {
        const double retained = std::fmod(m_accumulator, m_config.fixed_step_seconds);
        result.dropped_seconds += m_accumulator - retained;
        m_accumulator = retained;
    }

    while (m_replication_accumulator + epsilon >=
            m_config.replication_interval_seconds) {
        m_replication_accumulator -= m_config.replication_interval_seconds;
        if (m_replication_accumulator < 0.0 && m_replication_accumulator > -epsilon)
            m_replication_accumulator = 0.0;
        ++result.replication_ticks;
    }

    result.simulated_seconds =
        static_cast<double>(result.fixed_steps) * m_config.fixed_step_seconds;
    result.simulation_time = m_simulation_time;
    return result;
}

void ConstructSimulation::reset(double simulation_time) noexcept
{
    m_accumulator = 0.0;
    m_replication_accumulator = 0.0;
    m_simulation_time = std::isfinite(simulation_time) ? simulation_time : 0.0;
}

double ConstructSimulation::simulationTime() const noexcept
{
    return m_simulation_time;
}

double ConstructSimulation::interpolationAlpha() const noexcept
{
    return std::clamp(m_accumulator / m_config.fixed_step_seconds, 0.0, 1.0);
}

const ConstructSimulationConfig &ConstructSimulation::config() const noexcept
{
    return m_config;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_network_clock.h"

#include <algorithm>
#include <cmath>

namespace navycraft {

void ConstructNetworkClock::observe(double server_time, double client_receive_time) noexcept
{
    if (!std::isfinite(server_time) || !std::isfinite(client_receive_time))
        return;

    const double sample = client_receive_time - server_time;
    if (!m_ready) {
        m_ready = true;
        m_offset = sample;
        m_jitter = 0.0;
        m_samples = 1;
        return;
    }

    const double error = sample - m_offset;
    m_jitter += (std::abs(error) - m_jitter) * 0.1;

    // A lower sample represents a lower-latency observation and is accepted
    // immediately. Higher samples may contain queueing delay and move the
    // baseline only very slowly. This keeps the render clock stable without
    // assuming symmetric network latency.
    if (sample < m_offset)
        m_offset = sample;
    else
        m_offset += std::min(error * 0.01, 0.0005);
    ++m_samples;
}

void ConstructNetworkClock::reset() noexcept
{
    m_ready = false;
    m_offset = 0.0;
    m_jitter = 0.0;
    m_samples = 0;
}

bool ConstructNetworkClock::ready() const noexcept
{
    return m_ready;
}

double ConstructNetworkClock::serverTime(double client_time) const noexcept
{
    if (!m_ready || !std::isfinite(client_time))
        return client_time;
    return client_time - m_offset;
}

double ConstructNetworkClock::offset() const noexcept
{
    return m_offset;
}

double ConstructNetworkClock::jitter() const noexcept
{
    return m_jitter;
}

std::size_t ConstructNetworkClock::sampleCount() const noexcept
{
    return m_samples;
}

} // namespace navycraft

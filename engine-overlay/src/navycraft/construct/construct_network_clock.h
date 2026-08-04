// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <cstddef>

namespace navycraft {

class ConstructNetworkClock final {
public:
    void observe(double server_time, double client_receive_time) noexcept;
    void reset() noexcept;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] double serverTime(double client_time) const noexcept;
    [[nodiscard]] double offset() const noexcept;
    [[nodiscard]] double jitter() const noexcept;
    [[nodiscard]] std::size_t sampleCount() const noexcept;

private:
    bool m_ready = false;
    double m_offset = 0.0;
    double m_jitter = 0.0;
    std::size_t m_samples = 0;
};

} // namespace navycraft

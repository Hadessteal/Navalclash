#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
using session_t = std::uint16_t;
class NetworkPacket {
public:
    NetworkPacket() = default;
    NetworkPacket(std::uint16_t, std::size_t, session_t = 0) {}
    session_t getPeerId() const { return 1; }
    NetworkPacket &operator>>(std::string &) { return *this; }
    NetworkPacket &operator<<(const std::string &) { return *this; }
};

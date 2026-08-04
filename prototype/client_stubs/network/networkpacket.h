#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
class NetworkPacket {
public:
    NetworkPacket(std::uint16_t = 0, std::size_t = 0) {}
    NetworkPacket &operator>>(std::string &) { return *this; }
    NetworkPacket &operator<<(const std::string &) { return *this; }
};

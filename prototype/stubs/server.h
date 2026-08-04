#pragma once
#include <cstdint>

using session_t = std::uint16_t;
constexpr session_t PEER_ID_INEXISTENT = 0;

namespace navycraft { struct ConstructWireMessage; }

class Server {
public:
    double getUptime() const { return 0.0; }
    void SendNavyCraftConstructMessage(
        session_t, const navycraft::ConstructWireMessage &) {}
};

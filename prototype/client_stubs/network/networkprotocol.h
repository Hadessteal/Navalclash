#pragma once
#include <cstdint>
constexpr std::uint16_t TOSERVER_NAVYCRAFT_RIDER_STATE = 0x54;
constexpr std::uint16_t TOSERVER_NAVYCRAFT_INTERACTION = 0x55;
constexpr std::uint16_t TOSERVER_NAVYCRAFT_HANDSHAKE = 0x56;
enum InteractAction : std::uint8_t {
    INTERACT_START_DIGGING,
    INTERACT_STOP_DIGGING,
    INTERACT_DIGGING_COMPLETED,
    INTERACT_PLACE,
    INTERACT_USE,
    INTERACT_ACTIVATE,
};

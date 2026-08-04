// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <cstdint>

namespace navycraft {

// Reserved immediately after Luanti 5.16.1's stock 0x64 command.
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION = 0x65;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM = 0x66;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE = 0x67;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_RESET = 0x68;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT = 0x69;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE = 0x6A;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION = 0x6B;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_HANDSHAKE = 0x6C;
constexpr std::uint16_t TOCLIENT_NAVYCRAFT_NUM_MSG_TYPES = 0x6D;

// Reserved immediately after Luanti 5.16.1's stock 0x53 command.
constexpr std::uint16_t TOSERVER_NAVYCRAFT_RIDER_STATE = 0x54;
constexpr std::uint16_t TOSERVER_NAVYCRAFT_INTERACTION = 0x55;
constexpr std::uint16_t TOSERVER_NAVYCRAFT_HANDSHAKE = 0x56;
constexpr std::uint16_t TOSERVER_NAVYCRAFT_NUM_MSG_TYPES = 0x57;

} // namespace navycraft

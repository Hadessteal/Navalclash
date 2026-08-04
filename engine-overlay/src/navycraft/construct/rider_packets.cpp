// SPDX-License-Identifier: LGPL-2.1-or-later
#include "rider_packets.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace navycraft {
namespace {
constexpr std::uint32_t MAGIC = 0x4E435244U; // NCRD

template <typename T>
void writeValue(std::vector<std::uint8_t> &out, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

template <typename T>
T readValue(const std::vector<std::uint8_t> &bytes, std::size_t &offset)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset + sizeof(T) > bytes.size())
        throw std::runtime_error("truncated NavyCraft rider packet");
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

void writeVec(std::vector<std::uint8_t> &out, const Vec3d &value)
{
    writeValue(out, value.x);
    writeValue(out, value.y);
    writeValue(out, value.z);
}

Vec3d readVec(const std::vector<std::uint8_t> &bytes, std::size_t &offset)
{
    return {readValue<double>(bytes, offset), readValue<double>(bytes, offset),
        readValue<double>(bytes, offset)};
}

bool finite(const Vec3d &value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

std::vector<std::uint8_t> RiderPacketCodec::encode(const RiderStatePacket &packet)
{
    if (!finite(packet.local_anchor) || !finite(packet.local_velocity) ||
            !finite(packet.world_position) || !finite(packet.velocity) ||
            !std::isfinite(packet.client_time))
        throw std::invalid_argument("non-finite NavyCraft rider packet value");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(128);
    writeValue(bytes, MAGIC);
    writeValue(bytes, VERSION);
    writeValue(bytes, packet.sequence);
    writeValue(bytes, packet.construct_id);
    writeValue(bytes, packet.articulation_id);
    writeVec(bytes, packet.local_anchor);
    writeVec(bytes, packet.local_velocity);
    writeVec(bytes, packet.world_position);
    writeVec(bytes, packet.velocity);
    writeValue(bytes, packet.client_time);
    std::uint8_t flags = 0;
    if (packet.grounded)
        flags |= 0x01U;
    if (packet.jumping)
        flags |= 0x02U;
    writeValue(bytes, flags);
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::length_error("NavyCraft rider packet exceeds size limit");
    return bytes;
}

RiderStatePacket RiderPacketCodec::decode(const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::length_error("NavyCraft rider packet exceeds size limit");
    std::size_t offset = 0;
    if (readValue<std::uint32_t>(bytes, offset) != MAGIC)
        throw std::runtime_error("invalid NavyCraft rider packet magic");
    const auto version = readValue<std::uint16_t>(bytes, offset);
    if (version < 1 || version > VERSION)
        throw std::runtime_error("unsupported NavyCraft rider packet version");
    RiderStatePacket packet;
    packet.sequence = readValue<std::uint64_t>(bytes, offset);
    packet.construct_id = readValue<ConstructId>(bytes, offset);
    if (version >= 2)
        packet.articulation_id = readValue<std::uint64_t>(bytes, offset);
    packet.local_anchor = readVec(bytes, offset);
    if (version >= 3)
        packet.local_velocity = readVec(bytes, offset);
    packet.world_position = readVec(bytes, offset);
    packet.velocity = readVec(bytes, offset);
    packet.client_time = readValue<double>(bytes, offset);
    const std::uint8_t flags = readValue<std::uint8_t>(bytes, offset);
    packet.grounded = (flags & 0x01U) != 0;
    packet.jumping = (flags & 0x02U) != 0;
    if (offset != bytes.size())
        throw std::runtime_error("trailing data in NavyCraft rider packet");
    if (!finite(packet.local_anchor) || !finite(packet.local_velocity) ||
            !finite(packet.world_position) || !finite(packet.velocity) ||
            !std::isfinite(packet.client_time))
        throw std::runtime_error("non-finite NavyCraft rider packet value");
    return packet;
}

} // namespace navycraft

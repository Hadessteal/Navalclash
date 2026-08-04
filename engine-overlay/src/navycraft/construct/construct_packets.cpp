// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_packets.h"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> TRANSFORM_MAGIC{{'N', 'C', 'T', 'F'}};
constexpr std::array<std::uint8_t, 4> SECTION_MAGIC{{'N', 'C', 'S', 'C'}};
constexpr std::array<std::uint8_t, 4> REMOVE_MAGIC{{'N', 'C', 'R', 'M'}};

class Writer final {
public:
    template<typename T>
    void integer(T value)
    {
        static_assert(std::is_integral_v<T>);
        using Unsigned = std::make_unsigned_t<T>;
        const Unsigned unsigned_value = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            m_bytes.push_back(static_cast<std::uint8_t>(unsigned_value >> (index * 8U)));
    }

    void real(double value)
    {
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    }

    void string(const std::string &value)
    {
        if (value.size() > ConstructPacketCodec::MAX_PACKET_STRING_BYTES ||
                value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("packet string too large");
        integer(static_cast<std::uint32_t>(value.size()));
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    void magic(const std::array<std::uint8_t, 4> &value)
    {
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    [[nodiscard]] std::vector<std::uint8_t> finish()
    {
        if (m_bytes.size() > ConstructPacketCodec::MAX_PACKET_BYTES)
            throw std::length_error("NavyCraft packet exceeds total size limit");
        return std::move(m_bytes);
    }

private:
    std::vector<std::uint8_t> m_bytes;
};

class Reader final {
public:
    explicit Reader(const std::vector<std::uint8_t> &bytes) : m_bytes(bytes) {}

    template<typename T>
    T integer()
    {
        static_assert(std::is_integral_v<T>);
        require(sizeof(T));
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned result = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index)
            result |= static_cast<Unsigned>(m_bytes[m_offset++]) << (index * 8U);
        return static_cast<T>(result);
    }

    double real()
    {
        const auto bits = integer<std::uint64_t>();
        double result = 0.0;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::string string()
    {
        const auto count = integer<std::uint32_t>();
        if (count > ConstructPacketCodec::MAX_PACKET_STRING_BYTES)
            throw std::runtime_error("packet string exceeds safety limit");
        require(count);
        std::string result(reinterpret_cast<const char *>(m_bytes.data() + m_offset), count);
        m_offset += count;
        return result;
    }

    void magic(const std::array<std::uint8_t, 4> &value)
    {
        require(value.size());
        for (const auto byte : value) {
            if (m_bytes[m_offset++] != byte)
                throw std::runtime_error("invalid NavyCraft packet magic");
        }
    }

    void end() const
    {
        if (m_offset != m_bytes.size())
            throw std::runtime_error("trailing NavyCraft packet bytes");
    }

private:
    void require(std::size_t count) const
    {
        if (m_offset > m_bytes.size() || count > m_bytes.size() - m_offset)
            throw std::runtime_error("truncated NavyCraft packet");
    }

    const std::vector<std::uint8_t> &m_bytes;
    std::size_t m_offset = 0;
};

void writeHeader(Writer &writer, const std::array<std::uint8_t, 4> &magic)
{
    writer.magic(magic);
    writer.integer(ConstructPacketCodec::VERSION);
    writer.integer<std::uint16_t>(0);
}

void readHeader(Reader &reader, const std::array<std::uint8_t, 4> &magic)
{
    reader.magic(magic);
    if (reader.integer<std::uint16_t>() != ConstructPacketCodec::VERSION)
        throw std::runtime_error("unsupported NavyCraft packet version");
    (void)reader.integer<std::uint16_t>();
}
}

std::vector<std::uint8_t> ConstructPacketCodec::encodeTransform(
    const ConstructTransformSnapshot &snapshot)
{
    if (snapshot.id == 0)
        throw std::invalid_argument("transform packet construct id 0 is reserved");
    Writer writer;
    writeHeader(writer, TRANSFORM_MAGIC);
    writer.integer(snapshot.id);
    writer.integer(snapshot.sequence);
    writer.real(snapshot.server_time);
    writer.real(snapshot.transform.position.x);
    writer.real(snapshot.transform.position.y);
    writer.real(snapshot.transform.position.z);
    writer.real(snapshot.transform.yaw_radians);
    writer.real(snapshot.linear_velocity.x);
    writer.real(snapshot.linear_velocity.y);
    writer.real(snapshot.linear_velocity.z);
    writer.real(snapshot.yaw_velocity);
    return writer.finish();
}

ConstructTransformSnapshot ConstructPacketCodec::decodeTransform(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("transform packet exceeds total size limit");
    Reader reader(bytes);
    readHeader(reader, TRANSFORM_MAGIC);
    ConstructTransformSnapshot result;
    result.id = reader.integer<ConstructId>();
    if (result.id == 0)
        throw std::runtime_error("transform packet contains reserved id");
    result.sequence = reader.integer<std::uint64_t>();
    result.server_time = reader.real();
    result.transform.position.x = reader.real();
    result.transform.position.y = reader.real();
    result.transform.position.z = reader.real();
    result.transform.yaw_radians = reader.real();
    result.linear_velocity.x = reader.real();
    result.linear_velocity.y = reader.real();
    result.linear_velocity.z = reader.real();
    result.yaw_velocity = reader.real();
    reader.end();
    return result;
}

std::vector<std::uint8_t> ConstructPacketCodec::encodeSection(
    ConstructId construct_id, const ConstructSection &section)
{
    if (construct_id == 0)
        throw std::invalid_argument("section packet construct id 0 is reserved");
    if (section.nodes.size() > MAX_SECTION_NODES)
        throw std::length_error("section packet has too many nodes");
    Writer writer;
    writeHeader(writer, SECTION_MAGIC);
    writer.integer(construct_id);
    writer.integer(section.position.x);
    writer.integer(section.position.y);
    writer.integer(section.position.z);
    writer.integer(section.revision);
    writer.integer(static_cast<std::uint16_t>(section.nodes.size()));
    for (const auto &entry : section.nodes) {
        writer.integer(entry.position.x);
        writer.integer(entry.position.y);
        writer.integer(entry.position.z);
        writer.integer(entry.node.content_id);
        writer.integer(entry.node.param1);
        writer.integer(entry.node.param2);
        writer.string(entry.node.node_name);
        writer.string(entry.node.metadata_blob);
    }
    return writer.finish();
}

ConstructSection ConstructPacketCodec::decodeSection(
    const std::vector<std::uint8_t> &bytes, ConstructId &construct_id)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("section packet exceeds total size limit");
    Reader reader(bytes);
    readHeader(reader, SECTION_MAGIC);
    construct_id = reader.integer<ConstructId>();
    if (construct_id == 0)
        throw std::runtime_error("section packet contains reserved id");
    ConstructSection result;
    result.position.x = reader.integer<std::int32_t>();
    result.position.y = reader.integer<std::int32_t>();
    result.position.z = reader.integer<std::int32_t>();
    result.revision = reader.integer<std::uint64_t>();
    const auto count = reader.integer<std::uint16_t>();
    if (count > MAX_SECTION_NODES)
        throw std::runtime_error("section packet node count exceeds safety limit");
    result.nodes.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        ConstructNodeEntry entry;
        entry.position.x = reader.integer<std::int32_t>();
        entry.position.y = reader.integer<std::int32_t>();
        entry.position.z = reader.integer<std::int32_t>();
        entry.node.content_id = reader.integer<std::uint16_t>();
        entry.node.param1 = reader.integer<std::uint8_t>();
        entry.node.param2 = reader.integer<std::uint8_t>();
        entry.node.node_name = reader.string();
        entry.node.metadata_blob = reader.string();
        result.nodes.push_back(std::move(entry));
    }
    reader.end();
    return result;
}

std::vector<std::uint8_t> ConstructPacketCodec::encodeRemove(ConstructId construct_id)
{
    if (construct_id == 0)
        throw std::invalid_argument("remove packet construct id 0 is reserved");
    Writer writer;
    writeHeader(writer, REMOVE_MAGIC);
    writer.integer(construct_id);
    return writer.finish();
}

ConstructId ConstructPacketCodec::decodeRemove(const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("remove packet exceeds total size limit");
    Reader reader(bytes);
    readHeader(reader, REMOVE_MAGIC);
    const ConstructId id = reader.integer<ConstructId>();
    if (id == 0)
        throw std::runtime_error("remove packet contains reserved id");
    reader.end();
    return id;
}

} // namespace navycraft

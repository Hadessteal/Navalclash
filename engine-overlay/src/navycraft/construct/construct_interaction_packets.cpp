// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_interaction_packets.h"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> MAGIC{{'N', 'C', 'I', 'A'}};

class Writer final {
public:
    template<typename T>
    void integer(T value)
    {
        static_assert(std::is_integral_v<T>);
        using Unsigned = std::make_unsigned_t<T>;
        const Unsigned encoded = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            m_bytes.push_back(static_cast<std::uint8_t>(encoded >> (index * 8U)));
    }

    void real(double value)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    }

    void string(const std::string &value)
    {
        if (value.size() > ConstructInteractionPacketCodec::MAX_STRING_BYTES ||
                value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("construct interaction string is too large");
        integer(static_cast<std::uint32_t>(value.size()));
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    void pos(const LocalNodePos &value)
    {
        integer(value.x);
        integer(value.y);
        integer(value.z);
    }

    void vec(const Vec3d &value)
    {
        real(value.x);
        real(value.y);
        real(value.z);
    }

    [[nodiscard]] std::vector<std::uint8_t> finish()
    {
        if (m_bytes.size() > ConstructInteractionPacketCodec::MAX_PACKET_BYTES)
            throw std::length_error("construct interaction packet is too large");
        return std::move(m_bytes);
    }

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
        Unsigned value = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index)
            value |= static_cast<Unsigned>(m_bytes[m_offset++]) << (index * 8U);
        return static_cast<T>(value);
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
        if (count > ConstructInteractionPacketCodec::MAX_STRING_BYTES)
            throw std::runtime_error("construct interaction string exceeds safety limit");
        require(count);
        std::string result(reinterpret_cast<const char *>(m_bytes.data() + m_offset), count);
        m_offset += count;
        return result;
    }

    LocalNodePos pos()
    {
        return {integer<std::int32_t>(), integer<std::int32_t>(), integer<std::int32_t>()};
    }

    Vec3d vec()
    {
        return {real(), real(), real()};
    }

    void magic()
    {
        require(MAGIC.size());
        for (const auto byte : MAGIC) {
            if (m_bytes[m_offset++] != byte)
                throw std::runtime_error("invalid construct interaction packet magic");
        }
    }

    void end() const
    {
        if (m_offset != m_bytes.size())
            throw std::runtime_error("trailing construct interaction packet bytes");
    }

private:
    void require(std::size_t count) const
    {
        if (m_offset > m_bytes.size() || count > m_bytes.size() - m_offset)
            throw std::runtime_error("truncated construct interaction packet");
    }

    const std::vector<std::uint8_t> &m_bytes;
    std::size_t m_offset = 0;
};
}

std::vector<std::uint8_t> ConstructInteractionPacketCodec::encode(
    const ConstructInteractionRequest &request)
{
    if (request.construct_id == 0)
        throw std::invalid_argument("construct interaction id 0 is reserved");
    if (request.fields.size() > MAX_FIELDS)
        throw std::length_error("too many construct interaction fields");
    Writer writer;
    writer.m_bytes.insert(writer.m_bytes.end(), MAGIC.begin(), MAGIC.end());
    writer.integer(VERSION);
    writer.integer<std::uint16_t>(0);
    writer.integer(request.sequence);
    writer.integer(request.construct_id);
    writer.integer(static_cast<std::uint8_t>(request.action));
    writer.pos(request.node_position);
    writer.pos(request.adjacent_position);
    writer.vec(request.local_point);
    writer.vec(request.world_point);
    writer.vec(request.world_normal);
    writer.string(request.actor);
    writer.string(request.wielded_item);
    writer.string(request.form_name);
    writer.real(request.client_time);
    writer.integer(static_cast<std::uint16_t>(request.fields.size()));
    for (const auto &[key, value] : request.fields) {
        writer.string(key);
        writer.string(value);
    }
    return writer.finish();
}

ConstructInteractionRequest ConstructInteractionPacketCodec::decode(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("construct interaction packet exceeds safety limit");
    Reader reader(bytes);
    reader.magic();
    if (reader.integer<std::uint16_t>() != VERSION)
        throw std::runtime_error("unsupported construct interaction packet version");
    (void)reader.integer<std::uint16_t>();
    ConstructInteractionRequest result;
    result.sequence = reader.integer<std::uint64_t>();
    result.construct_id = reader.integer<ConstructId>();
    if (result.construct_id == 0)
        throw std::runtime_error("construct interaction packet contains reserved id");
    const auto action = reader.integer<std::uint8_t>();
    if (action > static_cast<std::uint8_t>(ConstructInteractionAction::Timer))
        throw std::runtime_error("invalid construct interaction action");
    result.action = static_cast<ConstructInteractionAction>(action);
    result.node_position = reader.pos();
    result.adjacent_position = reader.pos();
    result.local_point = reader.vec();
    result.world_point = reader.vec();
    result.world_normal = reader.vec();
    result.actor = reader.string();
    result.wielded_item = reader.string();
    result.form_name = reader.string();
    result.client_time = reader.real();
    const auto field_count = reader.integer<std::uint16_t>();
    if (field_count > MAX_FIELDS)
        throw std::runtime_error("construct interaction field count exceeds safety limit");
    result.fields.reserve(field_count);
    for (std::uint16_t index = 0; index < field_count; ++index) {
        std::string key = reader.string();
        std::string value = reader.string();
        result.fields.emplace_back(std::move(key), std::move(value));
    }
    reader.end();
    return result;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_handshake.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> MAGIC{{'N', 'C', 'H', 'S'}};

class Writer final {
public:
    template <typename T>
    void integer(T value)
    {
        static_assert(std::is_integral_v<T>);
        using Unsigned = std::make_unsigned_t<T>;
        const auto unsigned_value = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            m_bytes.push_back(static_cast<std::uint8_t>(unsigned_value >> (8U * index)));
    }

    void bytes(const std::array<std::uint8_t, 4> &value)
    {
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    void string(const std::string &value, std::size_t maximum)
    {
        if (value.size() > maximum || value.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::length_error("NavyCraft handshake string exceeds safety limit");
        integer(static_cast<std::uint16_t>(value.size()));
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    [[nodiscard]] std::vector<std::uint8_t> finish()
    {
        if (m_bytes.size() > ConstructHandshakeCodec::MAX_PACKET_BYTES)
            throw std::length_error("NavyCraft handshake packet exceeds safety limit");
        return std::move(m_bytes);
    }

private:
    std::vector<std::uint8_t> m_bytes;
};

class Reader final {
public:
    explicit Reader(const std::vector<std::uint8_t> &bytes) : m_bytes(bytes) {}

    template <typename T>
    [[nodiscard]] T integer()
    {
        static_assert(std::is_integral_v<T>);
        require(sizeof(T));
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned result = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index)
            result |= static_cast<Unsigned>(m_bytes[m_offset++]) << (8U * index);
        return static_cast<T>(result);
    }

    void bytes(const std::array<std::uint8_t, 4> &expected)
    {
        require(expected.size());
        for (const auto value : expected) {
            if (m_bytes[m_offset++] != value)
                throw std::runtime_error("invalid NavyCraft handshake magic");
        }
    }

    [[nodiscard]] std::string string(std::size_t maximum)
    {
        const auto count = integer<std::uint16_t>();
        if (count > maximum)
            throw std::runtime_error("NavyCraft handshake string exceeds safety limit");
        require(count);
        std::string result(reinterpret_cast<const char *>(m_bytes.data() + m_offset), count);
        m_offset += count;
        return result;
    }

    void finish() const
    {
        if (m_offset != m_bytes.size())
            throw std::runtime_error("trailing NavyCraft handshake data");
    }

private:
    void require(std::size_t count) const
    {
        if (m_offset > m_bytes.size() || count > m_bytes.size() - m_offset)
            throw std::runtime_error("truncated NavyCraft handshake packet");
    }

    const std::vector<std::uint8_t> &m_bytes;
    std::size_t m_offset = 0;
};

bool validKind(ConstructHandshakeKind kind) noexcept
{
    return kind == ConstructHandshakeKind::ClientHello ||
        kind == ConstructHandshakeKind::ServerAccept ||
        kind == ConstructHandshakeKind::ServerReject;
}

ConstructHandshakePacket rejection(
    const ConstructHandshakePacket &client,
    const ConstructHandshakePacket &server,
    std::string reason)
{
    ConstructHandshakePacket result = server;
    result.kind = ConstructHandshakeKind::ServerReject;
    result.nonce = client.nonce;
    result.reason = std::move(reason);
    return result;
}
} // namespace

std::vector<std::uint8_t> ConstructHandshakeCodec::encode(
    const ConstructHandshakePacket &packet)
{
    if (!validKind(packet.kind))
        throw std::invalid_argument("invalid NavyCraft handshake kind");
    if (packet.nonce == 0)
        throw std::invalid_argument("NavyCraft handshake nonce 0 is reserved");
    if (packet.build_id.empty())
        throw std::invalid_argument("NavyCraft handshake build id is required");
    if (packet.kind != ConstructHandshakeKind::ServerReject && !packet.reason.empty())
        throw std::invalid_argument("NavyCraft handshake reason is only valid for rejection");

    Writer writer;
    writer.bytes(MAGIC);
    writer.integer(WIRE_VERSION);
    writer.integer(static_cast<std::uint8_t>(packet.kind));
    writer.integer<std::uint8_t>(0);
    writer.integer(packet.construct_protocol);
    writer.integer(packet.engine_major);
    writer.integer(packet.engine_minor);
    writer.integer(packet.engine_patch);
    writer.integer(packet.supported_features);
    writer.integer(packet.required_features);
    writer.integer(packet.nonce);
    writer.string(packet.build_id, MAX_BUILD_ID_BYTES);
    writer.string(packet.reason, MAX_REASON_BYTES);
    return writer.finish();
}

ConstructHandshakePacket ConstructHandshakeCodec::decode(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::length_error("NavyCraft handshake packet exceeds safety limit");
    Reader reader(bytes);
    reader.bytes(MAGIC);
    if (reader.integer<std::uint16_t>() != WIRE_VERSION)
        throw std::runtime_error("unsupported NavyCraft handshake wire version");

    ConstructHandshakePacket result;
    result.kind = static_cast<ConstructHandshakeKind>(reader.integer<std::uint8_t>());
    if (!validKind(result.kind))
        throw std::runtime_error("invalid NavyCraft handshake kind");
    (void)reader.integer<std::uint8_t>();
    result.construct_protocol = reader.integer<std::uint16_t>();
    result.engine_major = reader.integer<std::uint16_t>();
    result.engine_minor = reader.integer<std::uint16_t>();
    result.engine_patch = reader.integer<std::uint16_t>();
    result.supported_features = reader.integer<std::uint64_t>();
    result.required_features = reader.integer<std::uint64_t>();
    result.nonce = reader.integer<std::uint64_t>();
    result.build_id = reader.string(MAX_BUILD_ID_BYTES);
    result.reason = reader.string(MAX_REASON_BYTES);
    reader.finish();

    if (result.nonce == 0)
        throw std::runtime_error("NavyCraft handshake contains reserved nonce");
    if (result.build_id.empty())
        throw std::runtime_error("NavyCraft handshake build id is empty");
    if (result.kind != ConstructHandshakeKind::ServerReject && !result.reason.empty())
        throw std::runtime_error("unexpected NavyCraft handshake reason");
    return result;
}

ConstructHandshakePacket ConstructHandshakeCodec::evaluate(
    const ConstructHandshakePacket &client,
    const ConstructHandshakePacket &server_identity)
{
    if (client.kind != ConstructHandshakeKind::ClientHello)
        return rejection(client, server_identity, "expected client hello");
    if (client.construct_protocol != server_identity.construct_protocol) {
        return rejection(client, server_identity,
            "construct protocol mismatch: client=" +
            std::to_string(client.construct_protocol) + " server=" +
            std::to_string(server_identity.construct_protocol));
    }
    const auto missing_on_server = client.required_features &
        ~server_identity.supported_features;
    if (missing_on_server != 0)
        return rejection(client, server_identity,
            "server lacks client-required native construct features");
    const auto missing_on_client = server_identity.required_features &
        ~client.supported_features;
    if (missing_on_client != 0)
        return rejection(client, server_identity,
            "client lacks server-required native construct features");

    ConstructHandshakePacket accepted = server_identity;
    accepted.kind = ConstructHandshakeKind::ServerAccept;
    accepted.nonce = client.nonce;
    accepted.supported_features &= client.supported_features;
    accepted.required_features = server_identity.required_features;
    accepted.reason.clear();
    return accepted;
}

ConstructHandshakePacket makeClientConstructHello(std::uint64_t nonce)
{
    ConstructHandshakePacket packet;
    packet.kind = ConstructHandshakeKind::ClientHello;
    packet.nonce = nonce;
    return packet;
}

ConstructHandshakePacket makeServerConstructIdentity()
{
    ConstructHandshakePacket packet;
    packet.kind = ConstructHandshakeKind::ServerAccept;
    packet.nonce = 1;
    return packet;
}

} // namespace navycraft

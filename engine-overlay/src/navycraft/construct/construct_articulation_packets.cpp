// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_articulation_packets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> MAGIC{{'N', 'C', 'A', 'R'}};

class Writer final {
public:
    template<typename T>
    void integer(T value)
    {
        static_assert(std::is_integral_v<T>);
        using Unsigned = std::make_unsigned_t<T>;
        const Unsigned converted = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            bytes.push_back(static_cast<std::uint8_t>(converted >> (index * 8U)));
    }

    void real(double value)
    {
        if (!std::isfinite(value))
            throw std::invalid_argument("NavyCraft articulation packet contains non-finite value");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    }

    void string(const std::string &value)
    {
        if (value.size() > ConstructArticulationPacketCodec::MAX_NAME_BYTES)
            throw std::length_error("NavyCraft articulation name exceeds safety limit");
        integer(static_cast<std::uint16_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    std::vector<std::uint8_t> finish()
    {
        if (bytes.size() > ConstructArticulationPacketCodec::MAX_PACKET_BYTES)
            throw std::length_error("NavyCraft articulation packet exceeds safety limit");
        return std::move(bytes);
    }

    std::vector<std::uint8_t> bytes;
};

class Reader final {
public:
    explicit Reader(const std::vector<std::uint8_t> &source) : bytes(source) {}

    template<typename T>
    T integer()
    {
        static_assert(std::is_integral_v<T>);
        require(sizeof(T));
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned result = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index)
            result |= static_cast<Unsigned>(bytes[offset++]) << (index * 8U);
        return static_cast<T>(result);
    }

    double real()
    {
        const auto bits = integer<std::uint64_t>();
        double result = 0.0;
        std::memcpy(&result, &bits, sizeof(result));
        if (!std::isfinite(result))
            throw std::runtime_error("NavyCraft articulation packet contains non-finite value");
        return result;
    }

    std::string string()
    {
        const auto size = integer<std::uint16_t>();
        if (size > ConstructArticulationPacketCodec::MAX_NAME_BYTES)
            throw std::runtime_error("NavyCraft articulation name exceeds safety limit");
        require(size);
        std::string result(reinterpret_cast<const char *>(bytes.data() + offset), size);
        offset += size;
        return result;
    }

    void expectMagic()
    {
        require(MAGIC.size());
        for (const auto byte : MAGIC) {
            if (bytes[offset++] != byte)
                throw std::runtime_error("invalid NavyCraft articulation packet magic");
        }
    }

    void end() const
    {
        if (offset != bytes.size())
            throw std::runtime_error("trailing NavyCraft articulation packet bytes");
    }

private:
    void require(std::size_t count) const
    {
        if (offset > bytes.size() || count > bytes.size() - offset)
            throw std::runtime_error("truncated NavyCraft articulation packet");
    }

    const std::vector<std::uint8_t> &bytes;
    std::size_t offset = 0;
};

void writeVec3(Writer &writer, const Vec3d &value)
{
    writer.real(value.x);
    writer.real(value.y);
    writer.real(value.z);
}

Vec3d readVec3(Reader &reader)
{
    const double x = reader.real();
    const double y = reader.real();
    const double z = reader.real();
    return {x, y, z};
}

void writeNodePos(Writer &writer, const LocalNodePos &position)
{
    writer.integer(position.x);
    writer.integer(position.y);
    writer.integer(position.z);
}

LocalNodePos readNodePos(Reader &reader)
{
    const auto x = reader.integer<std::int32_t>();
    const auto y = reader.integer<std::int32_t>();
    const auto z = reader.integer<std::int32_t>();
    return {x, y, z};
}

void writeHeader(Writer &writer, ConstructArticulationPacketKind kind)
{
    writer.bytes.insert(writer.bytes.end(), MAGIC.begin(), MAGIC.end());
    writer.integer(ConstructArticulationPacketCodec::VERSION);
    writer.integer(static_cast<std::uint8_t>(kind));
    writer.integer<std::uint8_t>(0);
}

void validateDefinition(const ConstructArticulationDefinition &definition)
{
    if (definition.id == 0 || definition.construct_id == 0)
        throw std::invalid_argument("NavyCraft articulation definition uses reserved id");
    if (definition.nodes.size() > ConstructArticulationPacketCodec::MAX_NODES)
        throw std::length_error("NavyCraft articulation definition has too many nodes");
    if (definition.minimum_position > definition.maximum_position ||
            definition.maximum_speed < 0.0 || definition.maximum_acceleration < 0.0)
        throw std::invalid_argument("NavyCraft articulation definition limits are invalid");
}

void validateSnapshot(const ConstructArticulationSnapshot &snapshot)
{
    if (snapshot.construct_id == 0 || snapshot.articulation_id == 0 || snapshot.sequence == 0)
        throw std::invalid_argument("NavyCraft articulation snapshot uses reserved id");
}
}

std::vector<std::uint8_t> ConstructArticulationPacketCodec::encodeDefinition(
    const ConstructArticulationDefinition &definition)
{
    validateDefinition(definition);
    Writer writer;
    writeHeader(writer, ConstructArticulationPacketKind::Definition);
    writer.integer(definition.construct_id);
    writer.integer(definition.id);
    writer.integer(definition.parent_id);
    writer.integer(static_cast<std::uint8_t>(definition.kind));
    writer.integer(static_cast<std::uint8_t>(definition.control_mode));
    writer.integer<std::uint8_t>(definition.enabled ? 1U : 0U);
    writer.integer<std::uint8_t>(definition.render_enabled ? 1U : 0U);
    writer.integer<std::uint8_t>(definition.collision_enabled ? 1U : 0U);
    writer.integer<std::uint8_t>(0);
    writer.string(definition.name);
    writeVec3(writer, definition.pivot_local);
    writeVec3(writer, definition.axis_local);
    writer.real(definition.minimum_position);
    writer.real(definition.maximum_position);
    writer.real(definition.maximum_speed);
    writer.real(definition.maximum_acceleration);
    writer.integer(static_cast<std::uint32_t>(definition.nodes.size()));
    for (const auto &position : definition.nodes)
        writeNodePos(writer, position);
    return writer.finish();
}

std::vector<std::uint8_t> ConstructArticulationPacketCodec::encodeState(
    const ConstructArticulationSnapshot &snapshot)
{
    validateSnapshot(snapshot);
    Writer writer;
    writeHeader(writer, ConstructArticulationPacketKind::State);
    writer.integer(snapshot.construct_id);
    writer.integer(snapshot.articulation_id);
    writer.integer(snapshot.sequence);
    writer.real(snapshot.server_time);
    writer.real(snapshot.position);
    writer.real(snapshot.target_position);
    writer.real(snapshot.velocity);
    writer.integer<std::uint8_t>(snapshot.enabled ? 1U : 0U);
    return writer.finish();
}

std::vector<std::uint8_t> ConstructArticulationPacketCodec::encodeRemove(
    ConstructId construct_id, ConstructArticulationId articulation_id)
{
    if (construct_id == 0 || articulation_id == 0)
        throw std::invalid_argument("NavyCraft articulation remove uses reserved id");
    Writer writer;
    writeHeader(writer, ConstructArticulationPacketKind::Remove);
    writer.integer(construct_id);
    writer.integer(articulation_id);
    return writer.finish();
}

std::vector<std::uint8_t> ConstructArticulationPacketCodec::encodeResetConstruct(
    ConstructId construct_id)
{
    if (construct_id == 0)
        throw std::invalid_argument("NavyCraft articulation reset uses reserved construct id");
    Writer writer;
    writeHeader(writer, ConstructArticulationPacketKind::ResetConstruct);
    writer.integer(construct_id);
    return writer.finish();
}

ConstructArticulationPacket ConstructArticulationPacketCodec::decode(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("NavyCraft articulation packet exceeds safety limit");
    Reader reader(bytes);
    reader.expectMagic();
    if (reader.integer<std::uint16_t>() != VERSION)
        throw std::runtime_error("unsupported NavyCraft articulation packet version");
    const auto raw_kind = reader.integer<std::uint8_t>();
    (void)reader.integer<std::uint8_t>();
    if (raw_kind > static_cast<std::uint8_t>(ConstructArticulationPacketKind::ResetConstruct))
        throw std::runtime_error("invalid NavyCraft articulation packet kind");
    ConstructArticulationPacket packet;
    packet.kind = static_cast<ConstructArticulationPacketKind>(raw_kind);
    switch (packet.kind) {
    case ConstructArticulationPacketKind::Definition: {
        auto &definition = packet.definition;
        definition.construct_id = reader.integer<ConstructId>();
        definition.id = reader.integer<ConstructArticulationId>();
        definition.parent_id = reader.integer<ConstructArticulationId>();
        const auto raw_joint = reader.integer<std::uint8_t>();
        const auto raw_control = reader.integer<std::uint8_t>();
        if (raw_joint > static_cast<std::uint8_t>(ConstructJointKind::Prismatic) ||
                raw_control > static_cast<std::uint8_t>(ConstructJointControlMode::Oscillate))
            throw std::runtime_error("invalid NavyCraft articulation enum value");
        definition.kind = static_cast<ConstructJointKind>(raw_joint);
        definition.control_mode = static_cast<ConstructJointControlMode>(raw_control);
        definition.enabled = reader.integer<std::uint8_t>() != 0;
        definition.render_enabled = reader.integer<std::uint8_t>() != 0;
        definition.collision_enabled = reader.integer<std::uint8_t>() != 0;
        (void)reader.integer<std::uint8_t>();
        definition.name = reader.string();
        definition.pivot_local = readVec3(reader);
        definition.axis_local = readVec3(reader);
        definition.minimum_position = reader.real();
        definition.maximum_position = reader.real();
        definition.maximum_speed = reader.real();
        definition.maximum_acceleration = reader.real();
        const auto count = reader.integer<std::uint32_t>();
        if (count > MAX_NODES)
            throw std::runtime_error("NavyCraft articulation node count exceeds safety limit");
        definition.nodes.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
            definition.nodes.push_back(readNodePos(reader));
        packet.construct_id = definition.construct_id;
        packet.articulation_id = definition.id;
        validateDefinition(definition);
        break;
    }
    case ConstructArticulationPacketKind::State: {
        auto &snapshot = packet.snapshot;
        snapshot.construct_id = reader.integer<ConstructId>();
        snapshot.articulation_id = reader.integer<ConstructArticulationId>();
        snapshot.sequence = reader.integer<std::uint64_t>();
        snapshot.server_time = reader.real();
        snapshot.position = reader.real();
        snapshot.target_position = reader.real();
        snapshot.velocity = reader.real();
        snapshot.enabled = reader.integer<std::uint8_t>() != 0;
        packet.construct_id = snapshot.construct_id;
        packet.articulation_id = snapshot.articulation_id;
        validateSnapshot(snapshot);
        break;
    }
    case ConstructArticulationPacketKind::Remove:
        packet.construct_id = reader.integer<ConstructId>();
        packet.articulation_id = reader.integer<ConstructArticulationId>();
        if (packet.construct_id == 0 || packet.articulation_id == 0)
            throw std::runtime_error("NavyCraft articulation remove uses reserved id");
        break;
    case ConstructArticulationPacketKind::ResetConstruct:
        packet.construct_id = reader.integer<ConstructId>();
        if (packet.construct_id == 0)
            throw std::runtime_error("NavyCraft articulation reset uses reserved id");
        break;
    }
    reader.end();
    return packet;
}

std::size_t ClientConstructArticulationState::NodeKeyHash::operator()(
    const NodeKey &key) const noexcept
{
    std::size_t seed = std::hash<ConstructId>{}(key.construct_id);
    seed ^= LocalNodePosHash{}(key.position) + 0x9e3779b9U +
        (seed << 6U) + (seed >> 2U);
    return seed;
}

bool ClientConstructArticulationState::applyPacket(const std::vector<std::uint8_t> &bytes)
{
    return apply(ConstructArticulationPacketCodec::decode(bytes));
}

bool ClientConstructArticulationState::apply(const ConstructArticulationPacket &packet)
{
    switch (packet.kind) {
    case ConstructArticulationPacketKind::Definition: {
        const auto previous = m_definitions.find(packet.articulation_id);
        if (previous != m_definitions.end()) {
            for (const auto &position : previous->second.nodes)
                m_node_owners.erase({previous->second.construct_id, position});
        }
        m_definitions[packet.articulation_id] = packet.definition;
        for (const auto &position : packet.definition.nodes)
            m_node_owners[{packet.construct_id, position}] = packet.articulation_id;
        ++m_revision;
        return true;
    }
    case ConstructArticulationPacketKind::State: {
        auto &history = m_histories[packet.articulation_id];
        if (history.have_newer && packet.snapshot.sequence <= history.newer.sequence)
            return false;
        if (history.have_newer) {
            history.older = history.newer;
            history.have_older = true;
        }
        history.newer = packet.snapshot;
        history.have_newer = true;
        ++m_revision;
        return true;
    }
    case ConstructArticulationPacketKind::Remove: {
        const auto definition_iterator = m_definitions.find(packet.articulation_id);
        if (definition_iterator != m_definitions.end()) {
            for (const auto &position : definition_iterator->second.nodes)
                m_node_owners.erase({packet.construct_id, position});
            m_definitions.erase(definition_iterator);
        }
        m_histories.erase(packet.articulation_id);
        ++m_revision;
        return true;
    }
    case ConstructArticulationPacketKind::ResetConstruct:
        removeConstruct(packet.construct_id);
        return true;
    }
    return false;
}

void ClientConstructArticulationState::removeConstruct(ConstructId construct_id)
{
    std::vector<ConstructArticulationId> ids;
    for (const auto &[id, definition] : m_definitions) {
        if (definition.construct_id == construct_id)
            ids.push_back(id);
    }
    for (const auto id : ids) {
        const auto definition = m_definitions.find(id);
        if (definition != m_definitions.end()) {
            for (const auto &position : definition->second.nodes)
                m_node_owners.erase({construct_id, position});
            m_definitions.erase(definition);
        }
        m_histories.erase(id);
    }
    if (!ids.empty())
        ++m_revision;
}

void ClientConstructArticulationState::clear()
{
    m_definitions.clear();
    m_histories.clear();
    m_node_owners.clear();
    ++m_revision;
}

std::optional<ConstructArticulationDefinition> ClientConstructArticulationState::definition(
    ConstructArticulationId articulation_id) const
{
    const auto iterator = m_definitions.find(articulation_id);
    if (iterator == m_definitions.end())
        return std::nullopt;
    return iterator->second;
}

std::optional<ConstructArticulationSnapshot> ClientConstructArticulationState::snapshot(
    ConstructArticulationId articulation_id) const
{
    const auto iterator = m_histories.find(articulation_id);
    if (iterator == m_histories.end() || !iterator->second.have_newer)
        return std::nullopt;
    return iterator->second.newer;
}

std::vector<ConstructArticulationDefinition> ClientConstructArticulationState::definitions(
    ConstructId construct_id) const
{
    std::vector<ConstructArticulationDefinition> result;
    for (const auto &[id, definition] : m_definitions) {
        (void)id;
        if (construct_id == 0 || definition.construct_id == construct_id)
            result.push_back(definition);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.id < right.id;
    });
    return result;
}

std::vector<ConstructArticulationSnapshot> ClientConstructArticulationState::snapshots(
    ConstructId construct_id) const
{
    std::vector<ConstructArticulationSnapshot> result;
    for (const auto &[id, history] : m_histories) {
        (void)id;
        if (history.have_newer &&
                (construct_id == 0 || history.newer.construct_id == construct_id))
            result.push_back(history.newer);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        return left.articulation_id < right.articulation_id;
    });
    return result;
}

double ClientConstructArticulationState::samplePosition(
    ConstructArticulationId articulation_id,
    double client_time,
    double interpolation_delay,
    double maximum_extrapolation) const noexcept
{
    const auto iterator = m_histories.find(articulation_id);
    if (iterator == m_histories.end() || !iterator->second.have_newer)
        return 0.0;
    const auto &history = iterator->second;
    const double render_time = client_time - std::max(0.0, interpolation_delay);
    if (history.have_older && history.newer.server_time > history.older.server_time &&
            render_time <= history.newer.server_time) {
        const double amount = std::max(0.0, std::min(1.0,
            (render_time - history.older.server_time) /
                (history.newer.server_time - history.older.server_time)));
        return history.older.position +
            (history.newer.position - history.older.position) * amount;
    }
    const double extrapolation = std::max(0.0, std::min(maximum_extrapolation,
        render_time - history.newer.server_time));
    return history.newer.position + history.newer.velocity * extrapolation;
}

std::uint64_t ClientConstructArticulationState::revision() const noexcept
{
    return m_revision;
}

std::optional<ConstructArticulationId> ClientConstructArticulationState::nodeJoint(
    ConstructId construct_id, const LocalNodePos &position) const
{
    const auto iterator = m_node_owners.find({construct_id, position});
    if (iterator == m_node_owners.end())
        return std::nullopt;
    return iterator->second;
}

} // namespace navycraft

// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_serialization.h"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> MAGIC{{'N', 'C', 'C', 'T'}};

class Writer final {
public:
    template<typename T>
    void integer(T value)
    {
        static_assert(std::is_integral_v<T>);
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned unsigned_value = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            m_bytes.push_back(static_cast<std::uint8_t>(unsigned_value >> (index * 8U)));
    }

    void real(double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t));
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    }

    void string(const std::string &value)
    {
        if (value.size() > ConstructSerialization::MAX_STRING_BYTES ||
                value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("construct string is too large");
        integer(static_cast<std::uint32_t>(value.size()));
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    void raw(const std::array<std::uint8_t, 4> &value)
    {
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    [[nodiscard]] std::vector<std::uint8_t> finish()
    {
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
        Unsigned value = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index)
            value |= static_cast<Unsigned>(m_bytes[m_offset++]) << (index * 8U);
        return static_cast<T>(value);
    }

    double real()
    {
        const std::uint64_t bits = integer<std::uint64_t>();
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::string string()
    {
        const auto length = integer<std::uint32_t>();
        if (length > ConstructSerialization::MAX_STRING_BYTES)
            throw std::runtime_error("construct string exceeds safety limit");
        require(length);
        std::string value(reinterpret_cast<const char *>(m_bytes.data() + m_offset), length);
        m_offset += length;
        return value;
    }

    void expectMagic()
    {
        require(MAGIC.size());
        for (const auto byte : MAGIC) {
            if (m_bytes[m_offset++] != byte)
                throw std::runtime_error("invalid construct magic");
        }
    }

    void expectEnd() const
    {
        if (m_offset != m_bytes.size())
            throw std::runtime_error("trailing bytes in construct payload");
    }

private:
    void require(std::size_t count) const
    {
        if (count > m_bytes.size() - m_offset)
            throw std::runtime_error("truncated construct payload");
    }

    const std::vector<std::uint8_t> &m_bytes;
    std::size_t m_offset = 0;
};
}

std::vector<std::uint8_t> ConstructSerialization::encode(const DynamicConstruct &construct)
{
    if (construct.nodeCount() > MAX_NODES ||
            construct.nodeCount() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("construct has too many nodes");

    Writer writer;
    writer.raw(MAGIC);
    writer.integer(FORMAT_VERSION);
    writer.integer<std::uint16_t>(0);
    writer.integer(construct.id());
    writer.string(construct.owner());

    const auto &transform = construct.transform();
    writer.real(transform.position.x);
    writer.real(transform.position.y);
    writer.real(transform.position.z);
    writer.real(transform.yaw_radians);

    const auto &velocity = construct.linearVelocity();
    writer.real(velocity.x);
    writer.real(velocity.y);
    writer.real(velocity.z);
    writer.real(construct.yawVelocity());

    const auto nodes = construct.nodes();
    writer.integer(static_cast<std::uint32_t>(nodes.size()));
    for (const auto &entry : nodes) {
        writer.integer(entry.position.x);
        writer.integer(entry.position.y);
        writer.integer(entry.position.z);
        writer.integer(entry.node.content_id);
        writer.integer(entry.node.param1);
        writer.integer(entry.node.param2);
        writer.string(entry.node.node_name);
        writer.string(entry.node.metadata_blob);
    }

    const auto states = construct.nodeStates();
    if (states.size() > MAX_NODES || states.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("construct has too many node states");
    writer.integer(static_cast<std::uint32_t>(states.size()));
    for (const auto &[position, state] : states) {
        writer.integer(position.x);
        writer.integer(position.y);
        writer.integer(position.z);
        writer.integer(state.revision);
        if (state.fields.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::length_error("construct node has too many metadata fields");
        writer.integer(static_cast<std::uint16_t>(state.fields.size()));
        for (const auto &[key, value] : state.fields) {
            writer.string(key);
            writer.string(value);
        }
        if (state.inventories.size() > std::numeric_limits<std::uint16_t>::max())
            throw std::length_error("construct node has too many inventory lists");
        writer.integer(static_cast<std::uint16_t>(state.inventories.size()));
        for (const auto &[name, list] : state.inventories) {
            writer.string(name);
            writer.integer(list.width);
            if (list.stacks.size() > std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("construct inventory list is too large");
            writer.integer(static_cast<std::uint32_t>(list.stacks.size()));
            for (const auto &stack : list.stacks)
                writer.string(stack);
        }
        writer.integer<std::uint8_t>(state.timer.active ? 1 : 0);
        writer.real(state.timer.timeout);
        writer.real(state.timer.elapsed);
    }

    const auto liquids = construct.liquids();
    if (liquids.size() > MAX_NODES || liquids.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("construct has too many liquid cells");
    writer.integer(static_cast<std::uint32_t>(liquids.size()));
    for (const auto &entry : liquids) {
        writer.integer(entry.position.x);
        writer.integer(entry.position.y);
        writer.integer(entry.position.z);
        writer.string(entry.liquid.liquid_name);
        writer.integer(entry.liquid.level);
        writer.integer(entry.liquid.flags);
    }
    return writer.finish();
}

std::shared_ptr<DynamicConstruct> ConstructSerialization::decode(
    const std::vector<std::uint8_t> &bytes)
{
    Reader reader(bytes);
    reader.expectMagic();
    const auto version = reader.integer<std::uint16_t>();
    if (version < 1 || version > FORMAT_VERSION)
        throw std::runtime_error("unsupported construct format version");
    (void)reader.integer<std::uint16_t>();

    const ConstructId id = reader.integer<ConstructId>();
    auto construct = std::make_shared<DynamicConstruct>(id);
    construct->setOwner(reader.string());

    ConstructTransform transform;
    transform.position.x = reader.real();
    transform.position.y = reader.real();
    transform.position.z = reader.real();
    transform.yaw_radians = reader.real();
    construct->setTransform(transform);

    Vec3d velocity;
    velocity.x = reader.real();
    velocity.y = reader.real();
    velocity.z = reader.real();
    construct->setLinearVelocity(velocity);
    construct->setYawVelocity(reader.real());

    const auto node_count = reader.integer<std::uint32_t>();
    if (node_count > MAX_NODES)
        throw std::runtime_error("construct node count exceeds safety limit");
    for (std::uint32_t index = 0; index < node_count; ++index) {
        LocalNodePos position;
        position.x = reader.integer<std::int32_t>();
        position.y = reader.integer<std::int32_t>();
        position.z = reader.integer<std::int32_t>();
        ConstructNode node;
        node.content_id = reader.integer<std::uint16_t>();
        node.param1 = reader.integer<std::uint8_t>();
        node.param2 = reader.integer<std::uint8_t>();
        node.node_name = reader.string();
        node.metadata_blob = reader.string();
        construct->setNode(position, std::move(node));
    }

    if (version >= 2) {
        const auto state_count = reader.integer<std::uint32_t>();
        if (state_count > MAX_NODES)
            throw std::runtime_error("construct node state count exceeds safety limit");
        for (std::uint32_t index = 0; index < state_count; ++index) {
            LocalNodePos position;
            position.x = reader.integer<std::int32_t>();
            position.y = reader.integer<std::int32_t>();
            position.z = reader.integer<std::int32_t>();
            if (!construct->getNode(position))
                throw std::runtime_error("construct state references a missing node");
            auto &state = construct->ensureNodeState(position);
            state.revision = reader.integer<std::uint64_t>();
            const auto field_count = reader.integer<std::uint16_t>();
            for (std::uint16_t field_index = 0; field_index < field_count; ++field_index) {
                std::string key = reader.string();
                std::string value = reader.string();
                state.fields.emplace(std::move(key), std::move(value));
            }
            const auto inventory_count = reader.integer<std::uint16_t>();
            for (std::uint16_t list_index = 0; list_index < inventory_count; ++list_index) {
                const std::string list_name = reader.string();
                ConstructInventoryList list;
                list.width = reader.integer<std::uint16_t>();
                const auto stack_count = reader.integer<std::uint32_t>();
                if (stack_count > 65535)
                    throw std::runtime_error("construct inventory slot count exceeds safety limit");
                list.stacks.reserve(stack_count);
                for (std::uint32_t stack_index = 0; stack_index < stack_count; ++stack_index)
                    list.stacks.push_back(reader.string());
                state.inventories.emplace(list_name, std::move(list));
            }
            state.timer.active = reader.integer<std::uint8_t>() != 0;
            state.timer.timeout = reader.real();
            state.timer.elapsed = reader.real();
        }
    }
    if (version >= 3) {
        const auto liquid_count = reader.integer<std::uint32_t>();
        if (liquid_count > MAX_NODES)
            throw std::runtime_error("construct liquid cell count exceeds safety limit");
        for (std::uint32_t index = 0; index < liquid_count; ++index) {
            LocalNodePos position;
            position.x = reader.integer<std::int32_t>();
            position.y = reader.integer<std::int32_t>();
            position.z = reader.integer<std::int32_t>();
            ConstructLiquidCell liquid;
            liquid.liquid_name = reader.string();
            liquid.level = reader.integer<std::uint8_t>();
            liquid.flags = reader.integer<std::uint8_t>();
            if (liquid.level == 0 || liquid.level > 8 || liquid.liquid_name.empty())
                throw std::runtime_error("invalid construct liquid cell");
            construct->setLiquid(position, std::move(liquid));
        }
    }
    reader.expectEnd();
    return construct;
}

} // namespace navycraft

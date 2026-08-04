// SPDX-License-Identifier: LGPL-2.1-or-later
#include "construct_effects.h"

#include "construct_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace navycraft {
namespace {
constexpr std::array<std::uint8_t, 4> MAGIC{{'N', 'C', 'F', 'X'}};

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
        if (!std::isfinite(value))
            throw std::invalid_argument("NavyCraft effect contains non-finite number");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    }

    void string(const std::string &value)
    {
        if (value.size() > ConstructEffectPacketCodec::MAX_STRING_BYTES)
            throw std::length_error("NavyCraft effect string exceeds safety limit");
        integer(static_cast<std::uint16_t>(value.size()));
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
    }

    std::vector<std::uint8_t> finish()
    {
        if (m_bytes.size() > ConstructEffectPacketCodec::MAX_PACKET_BYTES)
            throw std::length_error("NavyCraft effect packet exceeds safety limit");
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
        if (!std::isfinite(result))
            throw std::runtime_error("NavyCraft effect packet contains non-finite number");
        return result;
    }

    std::string string()
    {
        const auto size = integer<std::uint16_t>();
        if (size > ConstructEffectPacketCodec::MAX_STRING_BYTES)
            throw std::runtime_error("NavyCraft effect string exceeds safety limit");
        require(size);
        std::string result(reinterpret_cast<const char *>(m_bytes.data() + m_offset), size);
        m_offset += size;
        return result;
    }

    void expectMagic()
    {
        require(MAGIC.size());
        for (const auto byte : MAGIC) {
            if (m_bytes[m_offset++] != byte)
                throw std::runtime_error("invalid NavyCraft effect packet magic");
        }
    }

    void end() const
    {
        if (m_offset != m_bytes.size())
            throw std::runtime_error("trailing NavyCraft effect packet bytes");
    }

private:
    void require(std::size_t count) const
    {
        if (m_offset > m_bytes.size() || count > m_bytes.size() - m_offset)
            throw std::runtime_error("truncated NavyCraft effect packet");
    }

    const std::vector<std::uint8_t> &m_bytes;
    std::size_t m_offset = 0;
};

void writeVec3(Writer &writer, const Vec3d &value)
{
    writer.real(value.x);
    writer.real(value.y);
    writer.real(value.z);
}

Vec3d readVec3(Reader &reader)
{
    return {reader.real(), reader.real(), reader.real()};
}

void validate(const ConstructEffectEvent &event)
{
    if (event.construct_id == 0)
        throw std::invalid_argument("NavyCraft effect construct id 0 is reserved");
    if (event.sequence == 0)
        throw std::invalid_argument("NavyCraft effect sequence 0 is reserved");
    if (event.effect_id == 0 && (event.kind == ConstructEffectKind::SoundLoopStart ||
            event.kind == ConstructEffectKind::SoundLoopStop ||
            event.kind == ConstructEffectKind::ParticleEmitterStart ||
            event.kind == ConstructEffectKind::ParticleEmitterStop))
        throw std::invalid_argument("persistent NavyCraft effect requires an effect id");
    if (event.amount > 4096)
        throw std::invalid_argument("NavyCraft effect particle count exceeds safety limit");
    if (event.gain < 0.0 || event.gain > 16.0 || event.pitch <= 0.0 || event.pitch > 8.0 ||
            event.max_distance < 0.0 || event.max_distance > 4096.0 ||
            event.duration < 0.0 || event.duration > 3600.0 ||
            event.size_min < 0.0 || event.size_max < event.size_min || event.size_max > 128.0 ||
            event.lifetime_min < 0.0 || event.lifetime_max < event.lifetime_min ||
            event.lifetime_max > 3600.0)
        throw std::invalid_argument("NavyCraft effect numeric values are outside allowed range");
}

Vec3d rotateDirection(const ConstructTransform &transform, const Vec3d &local)
{
    const Vec3d origin = transform.localToWorld({});
    return transform.localToWorld(local) - origin;
}
}

std::vector<std::uint8_t> ConstructEffectPacketCodec::encode(
    const ConstructEffectEvent &event)
{
    validate(event);
    Writer writer;
    writer.m_bytes.insert(writer.m_bytes.end(), MAGIC.begin(), MAGIC.end());
    writer.integer(VERSION);
    writer.integer<std::uint16_t>(0);
    writer.integer(event.sequence);
    writer.integer(event.construct_id);
    writer.integer(event.effect_id);
    writer.integer(static_cast<std::uint8_t>(event.kind));
    writer.integer(static_cast<std::uint8_t>(event.preset));
    writer.integer(event.glow);
    writer.integer<std::uint8_t>(event.collision ? 1U : 0U);
    writeVec3(writer, event.local_position);
    writeVec3(writer, event.local_direction);
    writeVec3(writer, event.velocity);
    writer.string(event.sound_name);
    writer.string(event.texture_name);
    writer.real(event.gain);
    writer.real(event.pitch);
    writer.real(event.max_distance);
    writer.real(event.duration);
    writer.integer(event.amount);
    writer.real(event.size_min);
    writer.real(event.size_max);
    writer.real(event.lifetime_min);
    writer.real(event.lifetime_max);
    return writer.finish();
}

ConstructEffectEvent ConstructEffectPacketCodec::decode(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() > MAX_PACKET_BYTES)
        throw std::runtime_error("NavyCraft effect packet exceeds safety limit");
    Reader reader(bytes);
    reader.expectMagic();
    if (reader.integer<std::uint16_t>() != VERSION)
        throw std::runtime_error("unsupported NavyCraft effect packet version");
    (void)reader.integer<std::uint16_t>();
    ConstructEffectEvent event;
    event.sequence = reader.integer<std::uint64_t>();
    event.construct_id = reader.integer<ConstructId>();
    event.effect_id = reader.integer<std::uint64_t>();
    const auto kind = reader.integer<std::uint8_t>();
    const auto preset = reader.integer<std::uint8_t>();
    if (kind > static_cast<std::uint8_t>(ConstructEffectKind::LightFlash) ||
            preset > static_cast<std::uint8_t>(ConstructEffectPreset::DamageSparks))
        throw std::runtime_error("invalid NavyCraft effect enum value");
    event.kind = static_cast<ConstructEffectKind>(kind);
    event.preset = static_cast<ConstructEffectPreset>(preset);
    event.glow = reader.integer<std::uint8_t>();
    event.collision = reader.integer<std::uint8_t>() != 0;
    event.local_position = readVec3(reader);
    event.local_direction = readVec3(reader);
    event.velocity = readVec3(reader);
    event.sound_name = reader.string();
    event.texture_name = reader.string();
    event.gain = reader.real();
    event.pitch = reader.real();
    event.max_distance = reader.real();
    event.duration = reader.real();
    event.amount = reader.integer<std::uint16_t>();
    event.size_min = reader.real();
    event.size_max = reader.real();
    event.lifetime_min = reader.real();
    event.lifetime_max = reader.real();
    reader.end();
    validate(event);
    return event;
}

std::size_t ConstructEffectState::KeyHash::operator()(const Key &key) const noexcept
{
    std::size_t seed = std::hash<ConstructId>{}(key.construct_id);
    seed ^= std::hash<std::uint64_t>{}(key.effect_id) + 0x9e3779b9U +
        (seed << 6U) + (seed >> 2U);
    return seed;
}

bool ConstructEffectState::apply(const ConstructEffectEvent &event)
{
    validate(event);
    auto &last = m_last_sequences[event.construct_id];
    if (event.sequence <= last)
        return false;
    last = event.sequence;

    const Key key{event.construct_id, event.effect_id};
    switch (event.kind) {
    case ConstructEffectKind::SoundLoopStart:
    case ConstructEffectKind::ParticleEmitterStart:
        m_active[key] = event;
        break;
    case ConstructEffectKind::SoundLoopStop:
    case ConstructEffectKind::ParticleEmitterStop:
        m_active.erase(key);
        m_transient.push_back(event);
        break;
    case ConstructEffectKind::SoundOneShot:
    case ConstructEffectKind::ParticleBurst:
    case ConstructEffectKind::LightFlash:
        m_transient.push_back(event);
        break;
    }
    return true;
}

void ConstructEffectState::removeConstruct(ConstructId construct_id)
{
    m_last_sequences.erase(construct_id);
    for (auto iterator = m_active.begin(); iterator != m_active.end();) {
        if (iterator->first.construct_id == construct_id)
            iterator = m_active.erase(iterator);
        else
            ++iterator;
    }
    m_transient.erase(std::remove_if(m_transient.begin(), m_transient.end(),
        [construct_id](const ConstructEffectEvent &event) {
            return event.construct_id == construct_id;
        }), m_transient.end());
}

void ConstructEffectState::clear()
{
    m_last_sequences.clear();
    m_active.clear();
    m_transient.clear();
}

std::vector<ConstructEffectEvent> ConstructEffectState::drainTransient()
{
    std::vector<ConstructEffectEvent> result;
    result.swap(m_transient);
    return result;
}

std::vector<ConstructEffectEvent> ConstructEffectState::activeEffects() const
{
    std::vector<ConstructEffectEvent> result;
    result.reserve(m_active.size());
    for (const auto &[key, event] : m_active) {
        (void)key;
        result.push_back(event);
    }
    return result;
}

std::optional<ConstructEffectEvent> ConstructEffectState::findActive(
    ConstructId construct_id, std::uint64_t effect_id) const
{
    const auto iterator = m_active.find({construct_id, effect_id});
    if (iterator == m_active.end())
        return std::nullopt;
    return iterator->second;
}

SampledConstructEffect ConstructEffectState::sample(
    const ConstructEffectEvent &event, const DynamicConstruct &construct)
{
    SampledConstructEffect result;
    result.event = event;
    result.world_position = construct.transform().localToWorld(event.local_position);
    result.world_direction = rotateDirection(construct.transform(), event.local_direction);
    result.world_velocity = ConstructGeometry::surfaceVelocity(construct, result.world_position) +
        rotateDirection(construct.transform(), event.velocity);
    return result;
}

} // namespace navycraft

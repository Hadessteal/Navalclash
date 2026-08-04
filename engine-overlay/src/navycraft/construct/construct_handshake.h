// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace navycraft {

enum class ConstructHandshakeKind : std::uint8_t {
    ClientHello = 1,
    ServerAccept = 2,
    ServerReject = 3,
};

enum ConstructFeature : std::uint64_t {
    ConstructFeatureNativeScene = 1ULL << 0U,
    ConstructFeatureMovingHullCollision = 1ULL << 1U,
    ConstructFeatureRiderAuthority = 1ULL << 2U,
    ConstructFeatureInteractions = 1ULL << 3U,
    ConstructFeatureArticulation = 1ULL << 4U,
    ConstructFeatureFixedStepSimulation = 1ULL << 5U,
    ConstructFeatureClockSynchronizedSnapshots = 1ULL << 6U,
    ConstructFeatureFinalPlayerPose = 1ULL << 7U,
};

constexpr std::uint64_t CONSTRUCT_FEATURES_REQUIRED =
    ConstructFeatureNativeScene |
    ConstructFeatureMovingHullCollision |
    ConstructFeatureRiderAuthority |
    ConstructFeatureInteractions |
    ConstructFeatureFixedStepSimulation |
    ConstructFeatureClockSynchronizedSnapshots |
    ConstructFeatureFinalPlayerPose;

constexpr std::uint64_t CONSTRUCT_FEATURES_SUPPORTED =
    CONSTRUCT_FEATURES_REQUIRED | ConstructFeatureArticulation;

struct ConstructHandshakePacket {
    ConstructHandshakeKind kind = ConstructHandshakeKind::ClientHello;
    std::uint16_t construct_protocol = 16;
    std::uint16_t engine_major = 0;
    std::uint16_t engine_minor = 9;
    std::uint16_t engine_patch = 0;
    std::uint64_t supported_features = CONSTRUCT_FEATURES_SUPPORTED;
    std::uint64_t required_features = CONSTRUCT_FEATURES_REQUIRED;
    std::uint64_t nonce = 0;
    std::string build_id = "NavyCraft-Native-0.9.0";
    std::string reason;

    [[nodiscard]] bool accepted() const noexcept
    {
        return kind == ConstructHandshakeKind::ServerAccept;
    }
};

class ConstructHandshakeCodec final {
public:
    static constexpr std::uint16_t WIRE_VERSION = 1;
    static constexpr std::uint16_t CURRENT_PROTOCOL = 16;
    static constexpr std::size_t MAX_BUILD_ID_BYTES = 96;
    static constexpr std::size_t MAX_REASON_BYTES = 192;
    static constexpr std::size_t MAX_PACKET_BYTES = 512;

    [[nodiscard]] static std::vector<std::uint8_t> encode(
        const ConstructHandshakePacket &packet);
    [[nodiscard]] static ConstructHandshakePacket decode(
        const std::vector<std::uint8_t> &bytes);

    [[nodiscard]] static ConstructHandshakePacket evaluate(
        const ConstructHandshakePacket &client,
        const ConstructHandshakePacket &server_identity);
};

[[nodiscard]] ConstructHandshakePacket makeClientConstructHello(std::uint64_t nonce);
[[nodiscard]] ConstructHandshakePacket makeServerConstructIdentity();

} // namespace navycraft

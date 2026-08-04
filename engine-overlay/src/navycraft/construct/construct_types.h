// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <cmath>
#include <cstdint>
#include <functional>

namespace navycraft {

using ConstructId = std::uint64_t;

struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3d operator+(const Vec3d &other) const noexcept
    {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vec3d operator-(const Vec3d &other) const noexcept
    {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Vec3d operator*(double scalar) const noexcept
    {
        return {x * scalar, y * scalar, z * scalar};
    }

    Vec3d &operator+=(const Vec3d &other) noexcept
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
};

struct LocalNodePos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    bool operator==(const LocalNodePos &other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct LocalNodePosHash {
    std::size_t operator()(const LocalNodePos &value) const noexcept
    {
        std::size_t seed = std::hash<std::int32_t>{}(value.x);
        seed ^= std::hash<std::int32_t>{}(value.y) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= std::hash<std::int32_t>{}(value.z) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

struct ConstructTransform {
    Vec3d position{};
    double yaw_radians = 0.0;

    [[nodiscard]] Vec3d localToWorld(const Vec3d &local) const noexcept
    {
        const double cosine = std::cos(yaw_radians);
        const double sine = std::sin(yaw_radians);
        return {
            position.x + cosine * local.x - sine * local.z,
            position.y + local.y,
            position.z + sine * local.x + cosine * local.z,
        };
    }

    [[nodiscard]] Vec3d worldToLocal(const Vec3d &world) const noexcept
    {
        const Vec3d relative = world - position;
        const double cosine = std::cos(yaw_radians);
        const double sine = std::sin(yaw_radians);
        return {
            cosine * relative.x + sine * relative.z,
            relative.y,
            -sine * relative.x + cosine * relative.z,
        };
    }
};

struct LocalBounds {
    LocalNodePos min{};
    LocalNodePos max{};
    bool valid = false;
};

} // namespace navycraft

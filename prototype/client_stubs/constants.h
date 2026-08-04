#pragma once
#include <cstdint>
using f32 = float;
using u16 = std::uint16_t;
constexpr f32 BS = 10.0f;
struct v3f {
    f32 X = 0.0f;
    f32 Y = 0.0f;
    f32 Z = 0.0f;
    v3f() = default;
    v3f(f32 x, f32 y, f32 z) : X(x), Y(y), Z(z) {}
};
struct aabb3f {
    v3f MinEdge{};
    v3f MaxEdge{};
    aabb3f() = default;
    aabb3f(f32 min_x, f32 min_y, f32 min_z,
        f32 max_x, f32 max_y, f32 max_z) :
        MinEdge(min_x, min_y, min_z), MaxEdge(max_x, max_y, max_z) {}
};

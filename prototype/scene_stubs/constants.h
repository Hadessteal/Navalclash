#pragma once
#include <cstdint>
using f32 = float;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s16 = std::int16_t;
constexpr f32 BS = 10.0f;
constexpr s16 MAP_BLOCKSIZE = 16;
struct v3f {
    f32 X=0, Y=0, Z=0;
    v3f()=default;
    v3f(f32 x,f32 y,f32 z):X(x),Y(y),Z(z){}
};
struct v3s16 {
    s16 X=0, Y=0, Z=0;
    v3s16()=default;
    v3s16(s16 x,s16 y,s16 z):X(x),Y(y),Z(z){}
};
inline v3s16 operator*(const v3s16 &v, s16 s) { return {s16(v.X*s),s16(v.Y*s),s16(v.Z*s)}; }
inline v3f intToFloat(const v3s16 &v, f32 scale) { return {v.X*scale,v.Y*scale,v.Z*scale}; }
struct aabb3f {
    v3f MinEdge{}, MaxEdge{};
    aabb3f()=default;
    aabb3f(f32 a,f32 b,f32 c,f32 d,f32 e,f32 f):MinEdge(a,b,c),MaxEdge(d,e,f){}
};

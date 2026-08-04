#pragma once
#include <cstdint>
#include <string>
using f32=float; using u8=std::uint8_t; using u16=std::uint16_t; using u32=std::uint32_t;
struct v3f { f32 X=0,Y=0,Z=0; v3f()=default; v3f(f32 x,f32 y,f32 z):X(x),Y(y),Z(z){} };

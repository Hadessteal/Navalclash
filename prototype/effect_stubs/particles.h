#pragma once
#include "effect_types.h"
#include <string>
namespace ParticleParamTypes { enum class BlendMode : u8 { alpha, add, sub, screen, clip }; }
struct ParticleTextureStub { std::string string; ParticleParamTypes::BlendMode blendmode=ParticleParamTypes::BlendMode::alpha; };
struct ParticleParameters {
    v3f pos,vel,acc; f32 size=1,expirationtime=1; bool collisiondetection=false,collision_removal=false; u8 glow=0; ParticleTextureStub texture;
};

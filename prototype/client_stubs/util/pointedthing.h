#pragma once
#include "constants.h"
enum PointedThingType : unsigned char {
    POINTEDTHING_NOTHING,
    POINTEDTHING_NODE,
    POINTEDTHING_OBJECT,
};
struct PointedThing {
    PointedThingType type = POINTEDTHING_NOTHING;
    f32 distanceSq = 0.0f;
};

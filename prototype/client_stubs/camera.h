#pragma once
#include "constants.h"
class Camera {
public:
    v3f getPosition() const { return {}; }
    v3f getDirection() const { return {0.0f, 0.0f, 1.0f}; }
};

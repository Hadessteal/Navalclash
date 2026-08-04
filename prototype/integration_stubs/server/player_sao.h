#pragma once
#include "constants.h"
class PlayerSAO {
public:
    void setBasePosition(v3f) {}
    v3f getBasePosition() const { return {}; }
    bool isDead() const { return false; }
    v3f getEyePosition() const { return {}; }
};

#pragma once
#include "constants.h"
#include <string>
class PlayerSAO;
class RemotePlayer {
public:
    PlayerSAO *getPlayerSAO() { return nullptr; }
    void setSpeed(v3f) {}
    const std::string &getName() const { static const std::string value{"player"}; return value; }
};

#pragma once
#include "constants.h"
#include <functional>
#include <vector>
#include <cstdint>
using session_t = std::uint16_t;
class RemotePlayer;
class ServerActiveObject;
class ServerEnvironment {
public:
    RemotePlayer *getPlayer(session_t) { return nullptr; }
    void getObjectsInArea(std::vector<ServerActiveObject *> &, const aabb3f &,
        std::function<bool(ServerActiveObject *)>) {}
};

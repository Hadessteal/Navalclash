#pragma once
#include "constants.h"
#include "inventory.h"
#include <string>
struct PlayerControlStub { bool jump = false; };
class LocalPlayer {
public:
    const std::string &getName() const { static const std::string value; return value; }
    const ItemStack &getWieldedItem(ItemStack *, ItemStack *) const
    { static const ItemStack value; return value; }
    const aabb3f &getCollisionbox() const
    { static const aabb3f value(-3, 0, -3, 3, 17, 3); return value; }
    PlayerControlStub control;
    bool touching_ground = false;
};

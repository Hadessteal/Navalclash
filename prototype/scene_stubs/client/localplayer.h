#pragma once
#include "constants.h"
#include "inventory.h"
#include <string>
struct PlayerControlStub { bool jump=false; };
class LocalPlayer {
public:
    v3f getPosition() const {return {};}
    v3f getSpeed() const {return {};}
    const aabb3f &getCollisionbox() const { static aabb3f b(-3,0,-3,3,17,3); return b; }
    void setPosition(v3f){}
    void setSpeed(v3f){}
    const std::string &getName() const { static std::string n; return n; }
    const ItemStack &getWieldedItem(ItemStack*,ItemStack*) const { static ItemStack s; return s; }
    PlayerControlStub control;
    bool touching_ground=false;
};

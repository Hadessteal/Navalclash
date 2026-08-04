#pragma once
#include "server/serveractiveobject.h"
#include <string>

class LuaEntitySAO : public ServerActiveObject {
public:
    ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_LUAENTITY; }
    bool isAttached() const { return false; }
    bool getCollisionBox(aabb3f *box) const {
        if (box) *box = aabb3f(-3, 0, -3, 3, 10, 3);
        return true;
    }
    std::string getName() const { return "entity"; }
    v3f getBasePosition() const { return {}; }
    v3f getVelocity() const { return {}; }
    void setPos(const v3f &) {}
    void setVelocity(const v3f &) {}
};

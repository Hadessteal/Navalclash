#pragma once
#include "constants.h"

enum ActiveObjectType {
    ACTIVEOBJECT_TYPE_INVALID = 0,
    ACTIVEOBJECT_TYPE_LUAENTITY = 7,
    ACTIVEOBJECT_TYPE_PLAYER = 100,
};

class ServerActiveObject {
public:
    virtual ~ServerActiveObject() = default;
    virtual ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_INVALID; }
    virtual bool isGone() const { return false; }
    virtual u16 getId() const { return 1; }
};

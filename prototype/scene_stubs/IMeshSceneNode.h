#pragma once
#include "constants.h"
namespace scene {
class IMeshBuffer { public: virtual ~IMeshBuffer()=default; virtual u32 getVertexCount() const {return 1;} virtual u32 getIndexCount() const {return 1;} };
class IMesh { public: virtual ~IMesh()=default; virtual u32 getMeshBufferCount() const {return 1;} virtual IMeshBuffer *getMeshBuffer(u32) { static IMeshBuffer b; return &b; } };
enum E_CULLING_TYPE { EAC_OFF, EAC_BOX };
class ISceneNode { public: virtual ~ISceneNode()=default; virtual void remove(){} virtual void setPosition(v3f){} virtual void setRotation(v3f){} virtual void setVisible(bool){} };
class IMeshSceneNode: public ISceneNode { public: void setAutomaticCulling(E_CULLING_TYPE){} };
}

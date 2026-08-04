#pragma once
#include "IMeshSceneNode.h"
namespace scene {
class ISceneManager {
public:
    ISceneNode *addEmptySceneNode(ISceneNode * = nullptr){ static ISceneNode n; return &n; }
    IMeshSceneNode *addMeshSceneNode(IMesh *, ISceneNode *){ static IMeshSceneNode n; return &n; }
};
}

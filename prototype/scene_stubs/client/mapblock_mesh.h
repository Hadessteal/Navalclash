#pragma once
#include "constants.h"
#include "voxel.h"
#include "IMeshSceneNode.h"
class Client;
class NodeDefManager;
constexpr u8 MAX_TILE_LAYERS=2;
struct MeshGrid { u16 cell_size=1; };
struct MeshMakeData {
    VoxelManipulatorStub m_vmanip;
    bool m_generate_minimap=false;
    bool m_smooth_lighting=false;
    MeshMakeData(const NodeDefManager*,u16,MeshGrid){}
    void fillBlockDataBegin(v3s16){}
};
class MapBlockMesh {
public:
    MapBlockMesh(Client*,MeshMakeData*){}
    void animate(bool,float,int,u32){}
    void materializeTransparentBuffersForSceneNode(){}
    scene::IMesh *getMesh(u8){ static scene::IMesh m; return &m; }
};

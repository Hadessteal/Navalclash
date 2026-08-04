#pragma once
#include "mapnode.h"
#include <vector>
constexpr u8 VOXELFLAG_NO_DATA=1;
class VoxelArea {
public:
    v3s16 MinEdge{-64,-64,-64};
    v3s16 MaxEdge{64,64,64};
    u32 getVolume() const { return 32; }
    bool contains(v3s16) const { return true; }
};
class VoxelManipulatorStub {
public:
    VoxelArea m_area;
    std::vector<MapNode> m_data=std::vector<MapNode>(32);
    std::vector<u8> m_flags=std::vector<u8>(32,VOXELFLAG_NO_DATA);
    void setNodeNoEmerge(v3s16,const MapNode&) {}
};

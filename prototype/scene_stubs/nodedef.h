#pragma once
#include "mapnode.h"
#include <string>
struct ContentFeatures { std::string name="stub"; };
class NodeDefManager {
public:
    const ContentFeatures &get(const MapNode &) const { static ContentFeatures f; return f; }
    bool getId(const std::string &, content_t &out) const { out=1; return true; }
};

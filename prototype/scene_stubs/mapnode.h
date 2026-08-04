#pragma once
#include "constants.h"
#include <string>
using content_t = u16;
constexpr content_t CONTENT_UNKNOWN=125;
constexpr content_t CONTENT_AIR=126;
constexpr content_t CONTENT_IGNORE=127;
class MapNode {
public:
    MapNode(content_t c=CONTENT_AIR,u8 p1=0,u8 p2=0):m_c(c),m_p1(p1),m_p2(p2){}
    content_t getContent() const { return m_c; }
    void setContent(content_t c){m_c=c;}
private: content_t m_c; u8 m_p1,m_p2;
};

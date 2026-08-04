#pragma once
#include <string>
struct SoundSpec { SoundSpec(std::string_view n="",float g=1,bool l=false,float f=0,float p=1,float st=0):name(n),gain(g),loop(l),fade(f),pitch(p),start_time(st){} std::string name; float gain=1; bool loop=false; float fade=0,pitch=1,start_time=0; };

#pragma once
#include "effect_types.h"
struct SoundSpec;
class ISoundManager { public: int allocateId(u32){return 1;} void freeId(int,u32=1){} virtual void playSoundAt(int,const SoundSpec&,const v3f&,const v3f&){} virtual void stopSound(int){} virtual void updateSoundPosVel(int,const v3f&,const v3f&){} virtual ~ISoundManager()=default; };

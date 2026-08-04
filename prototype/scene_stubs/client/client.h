#pragma once
#include "constants.h"
#include "ISceneManager.h"
#include "nodedef.h"
class LocalPlayer;
class ClientEnvironmentStub {
public:
    u32 getDayNightRatio(){return 1000;}
    LocalPlayer *getLocalPlayer(){return nullptr;}
};
class Client {
public:
    scene::ISceneManager *getSceneManager(){ static scene::ISceneManager s; return &s; }
    const NodeDefManager *getNodeDefManager(){ static NodeDefManager n; return &n; }
    ClientEnvironmentStub &getEnv(){return m_env;}
private: ClientEnvironmentStub m_env;
};

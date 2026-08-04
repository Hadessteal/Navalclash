#pragma once
#include "client/localplayer.h"
class ClientEnvironmentEffectStub { public: LocalPlayer *getLocalPlayer(){return nullptr;} };
class Client { public: ClientEnvironmentEffectStub &getEnv(){return env;} private: ClientEnvironmentEffectStub env; };

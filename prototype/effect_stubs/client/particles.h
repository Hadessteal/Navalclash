#pragma once
class Client; class LocalPlayer; struct ClientEvent;
class ParticleManager { public: void handleParticleEvent(ClientEvent*,Client*,LocalPlayer*) {} };

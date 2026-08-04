#pragma once
struct ParticleParameters;
enum ClientEventType { CE_NONE, CE_SPAWN_PARTICLE };
struct ClientEvent { explicit ClientEvent(ClientEventType t):type(t){} ClientEventType type; ParticleParameters *spawn_particle=nullptr; };

#include "constants.h"
#include "network/networkprotocol.h"
#include "util/pointedthing.h"
#include <cstdint>
#pragma once
#include <memory>
using u64 = std::uint64_t;
class ITextureSource;
class Camera;
class LocalPlayer;
class NetworkPacket;
class ISoundManager;
class ParticleManager {};
namespace irr { namespace scene { class ISceneManager; } }
namespace navycraft { class ClientConstructScene; class ClientConstructEffects; }
enum LocalClientState { LC_Created, LC_Init, LC_Ready };
class ClientEnvironmentStub {
public:
    LocalPlayer *getLocalPlayer() { return nullptr; }
};
class Client {
public:
    irr::scene::ISceneManager *getSceneManager() { return nullptr; }
    ITextureSource *getTextureSource() { return nullptr; }
    Camera *getCamera() { return m_camera; }
    void Send(NetworkPacket *) {}
    bool isNavyCraftProtocolReady() const noexcept;
    void sendNavyCraftHandshake();
    void ensureNavyCraftConstructScene();
    void stepNavyCraftConstructScene(float dtime);
    void beginNavyCraftLocalPlayerMove(
        LocalPlayer *player, float dtime, v3f &position, v3f &speed);
    void finishNavyCraftLocalPlayerMove(
        LocalPlayer *player, float dtime, v3f &position, v3f &speed,
        bool &touching_ground);
    void resetNavyCraftConstructScene();
    void resetNavyCraftConnection();
    bool sendNavyCraftInteraction(InteractAction action, const PointedThing &pointed);
    void handleCommand_NavyCraftHandshake(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructSection(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructTransform(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructRemove(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructReset(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructEffect(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructProjectile(NetworkPacket *packet);
    void handleCommand_NavyCraftConstructArticulation(NetworkPacket *packet);
    std::unique_ptr<navycraft::ClientConstructScene> m_navycraft_construct_scene;
    std::unique_ptr<navycraft::ClientConstructEffects> m_navycraft_construct_effects;
    ISoundManager *m_sound = nullptr;
    std::unique_ptr<ParticleManager> m_particle_manager;
    double m_navycraft_client_time = 0.0;
    u64 m_navycraft_interaction_sequence = 0;
    u64 m_navycraft_handshake_nonce = 0;
    bool m_navycraft_handshake_sent = false;
    bool m_navycraft_handshake_accepted = false;
    LocalClientState m_state = LC_Ready;
    ClientEnvironmentStub m_env;
    Camera *m_camera = nullptr;
};

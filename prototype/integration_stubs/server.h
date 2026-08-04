#pragma once
#include "constants.h"
#include <cstdint>
#include <vector>
using session_t = std::uint16_t;
constexpr session_t PEER_ID_INEXISTENT = 0;
class NetworkPacket;
class RemotePlayer;
class PlayerSAO;
class ServerEnvironment;
namespace navycraft { struct ConstructWireMessage; }
class RemoteClientCollectionStub {
public:
    std::vector<session_t> getClientIDs() const { return {1}; }
    void sendCustom(session_t, int, NetworkPacket *, bool) {}
};
class Server {
public:
    double getUptime() const { return 0.0; }
    void Send(session_t, NetworkPacket *) {}
    void handleCommand_NavyCraftHandshake(NetworkPacket *packet);
    bool IsNavyCraftPeerReady(session_t peer_id);
    void ClearNavyCraftHandshakeState(session_t peer_id);
    void SendNavyCraftConstructMessage(session_t,
        const navycraft::ConstructWireMessage &);
    void SendMovePlayer(PlayerSAO *) {}
    void handleCommand_NavyCraftRiderState(NetworkPacket *packet);
    bool ReconcileNavyCraftRider(session_t peer_id, v3f &position, v3f &speed);
    void ClearNavyCraftRiderState(session_t peer_id);
    void SendNavyCraftFullState(session_t peer_id);
    void StepNavyCraftNativeSimulation(float dtime);
    void StepNavyCraftCarriedObjects(float dtime);
    void handleCommand_NavyCraftInteraction(NetworkPacket *packet);
    void StepNavyCraftConstructTimers(float dtime);
    void ClearNavyCraftInteractionState(session_t peer_id);
    ServerEnvironment *m_env = nullptr;
    RemoteClientCollectionStub m_clients;
};

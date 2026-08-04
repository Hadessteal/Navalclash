// SPDX-License-Identifier: LGPL-2.1-or-later
#include "server.h"

#include "construct/construct_handshake.h"
#include "log.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"

#include <exception>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
struct PeerHandshakeState {
    navycraft::ConstructHandshakePacket accepted{};
};

std::mutex g_handshake_mutex;
std::unordered_map<session_t, PeerHandshakeState> g_handshakes;

void sendHandshake(Server *server, session_t peer_id,
    const navycraft::ConstructHandshakePacket &packet)
{
    const auto bytes = navycraft::ConstructHandshakeCodec::encode(packet);
    const std::string payload(bytes.begin(), bytes.end());
    NetworkPacket response(TOCLIENT_NAVYCRAFT_HANDSHAKE,
        payload.size() + 4U, peer_id);
    response << payload;
    server->Send(peer_id, &response);
}
} // namespace

void Server::handleCommand_NavyCraftHandshake(NetworkPacket *packet)
{
    const session_t peer_id = packet->getPeerId();
    try {
        std::string payload;
        *packet >> payload;
        const std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
        const auto client = navycraft::ConstructHandshakeCodec::decode(bytes);
        const auto result = navycraft::ConstructHandshakeCodec::evaluate(
            client, navycraft::makeServerConstructIdentity());

        {
            std::lock_guard<std::mutex> lock(g_handshake_mutex);
            if (result.accepted())
                g_handshakes[peer_id] = {result};
            else
                g_handshakes.erase(peer_id);
        }
        sendHandshake(this, peer_id, result);
        if (result.accepted()) {
            actionstream << "NavyCraft native protocol "
                << result.construct_protocol << " accepted for peer "
                << peer_id << " (" << client.build_id << ")" << std::endl;
            SendNavyCraftFullState(peer_id);
        } else {
            warningstream << "NavyCraft native handshake rejected for peer "
                << peer_id << ": " << result.reason << std::endl;
        }
    } catch (const std::exception &error) {
        warningstream << "Invalid NavyCraft handshake from peer " << peer_id
            << ": " << error.what() << std::endl;
        ClearNavyCraftHandshakeState(peer_id);
    }
}

bool Server::IsNavyCraftPeerReady(session_t peer_id)
{
    std::lock_guard<std::mutex> lock(g_handshake_mutex);
    return g_handshakes.find(peer_id) != g_handshakes.end();
}

void Server::ClearNavyCraftHandshakeState(session_t peer_id)
{
    std::lock_guard<std::mutex> lock(g_handshake_mutex);
    g_handshakes.erase(peer_id);
}

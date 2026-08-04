// SPDX-License-Identifier: LGPL-2.1-or-later
#include "server.h"

#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/construct_replication_queue.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {
u16 commandFor(const navycraft::ConstructWireMessage &message)
{
    switch (message.kind) {
    case navycraft::ConstructWireKind::Section:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION;
    case navycraft::ConstructWireKind::Transform:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM;
    case navycraft::ConstructWireKind::Remove:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE;
    case navycraft::ConstructWireKind::Reset:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_RESET;
    case navycraft::ConstructWireKind::Effect:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT;
    case navycraft::ConstructWireKind::Projectile:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE;
    case navycraft::ConstructWireKind::Articulation:
        return TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION;
    }
    throw std::logic_error("unknown NavyCraft wire message kind");
}
}

void Server::SendNavyCraftConstructMessage(
    session_t peer_id,
    const navycraft::ConstructWireMessage &message)
{
    const u16 command = commandFor(message);
    const std::string payload(message.payload.begin(), message.payload.end());

    auto send_to = [&](session_t target) {
        if (!IsNavyCraftPeerReady(target))
            return;
        NetworkPacket packet(command, payload.size() + 4U, target);
        packet << payload;
        if (message.reliable)
            Send(target, &packet);
        else
            m_clients.sendCustom(target, 1, &packet, false);
    };

    if (peer_id == PEER_ID_INEXISTENT) {
        for (const session_t client_id : m_clients.getClientIDs())
            send_to(client_id);
        return;
    }

    send_to(peer_id);
}

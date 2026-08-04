// SPDX-License-Identifier: LGPL-2.1-or-later
#include "server.h"

#include "construct/construct_packets.h"
#include "construct/construct_runtime.h"
#include "network/construct_replication_queue.h"

#include <cmath>

void Server::StepNavyCraftNativeSimulation(float dtime)
{
    if (!(dtime > 0.0f) || !std::isfinite(dtime))
        return;

    auto &registry = navycraft::runtimeConstructRegistry();
    const auto advanced = navycraft::runtimeConstructSimulation().advance(registry, dtime);
    if (advanced.replication_ticks == 0 || registry.size() == 0)
        return;

    const double server_time = getUptime();
    for (const auto &construct : registry.snapshot()) {
        if (!construct || construct->empty())
            continue;

        navycraft::ConstructTransformSnapshot snapshot;
        snapshot.id = construct->id();
        snapshot.sequence = navycraft::nextRuntimeTransformSequence(construct->id());
        snapshot.server_time = server_time;
        snapshot.transform = construct->transform();
        snapshot.linear_velocity = construct->linearVelocity();
        snapshot.yaw_velocity = construct->yawVelocity();
        SendNavyCraftConstructMessage(PEER_ID_INEXISTENT, {
            navycraft::ConstructWireKind::Transform,
            construct->id(),
            navycraft::ConstructPacketCodec::encodeTransform(snapshot),
            false,
        });
    }
}

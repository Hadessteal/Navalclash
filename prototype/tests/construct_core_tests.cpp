#include "construct_registry.h"
#include "construct_simulation.h"
#include "construct_network_clock.h"
#include "construct_handshake.h"
#include "construct_serialization.h"
#include "construct_node_state.h"
#include "construct_interaction.h"
#include "construct_interaction_packets.h"
#if NAVYCRAFT_HAS_SQLITE
#include "construct_database.h"
#include "construct_persistence.h"
#endif
#include "construct_section.h"
#include "construct_replication.h"
#include "construct_geometry.h"
#include "construct_collision_world.h"
#include "construct_mesh.h"
#include "construct_packets.h"
#include "construct_effects.h"
#include "construct_projectiles.h"
#include "construct_fire_control.h"
#include "construct_navigation.h"
#include "construct_structure.h"
#include "construct_articulation.h"
#include "construct_articulation_packets.h"
#include "construct_articulated_motion.h"
#include "construct_liquids.h"
#include "construct_special_nodes.h"
#include "network/construct_protocol.h"
#include "construct_snapshot_buffer.h"
#include "platform_contact.h"
#include "rider_motion.h"
#include "rider_packets.h"
#include "moving_hull_collision.h"
#include "carried_body.h"
#include "client_construct_state.h"
#include "network/construct_replication_queue.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace navycraft;

namespace {
constexpr double PI = 3.14159265358979323846;

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-9)
{
    return std::abs(left - right) <= epsilon;
}
}

int main()
{
    try {

        // Permanent native protocol negotiation rejects mismatched or incomplete
        // client/server builds before construct state or rider packets are used.
        const std::uint64_t handshake_nonce = 0xA51C0FFEEULL;
        const auto client_hello = makeClientConstructHello(handshake_nonce);
        const auto hello_bytes = ConstructHandshakeCodec::encode(client_hello);
        const auto decoded_hello = ConstructHandshakeCodec::decode(hello_bytes);
        require(decoded_hello.kind == ConstructHandshakeKind::ClientHello &&
                decoded_hello.nonce == handshake_nonce &&
                decoded_hello.construct_protocol ==
                    ConstructHandshakeCodec::CURRENT_PROTOCOL,
            "construct client handshake round trip failed");

        const auto server_identity = makeServerConstructIdentity();
        const auto accepted_handshake = ConstructHandshakeCodec::evaluate(
            decoded_hello, server_identity);
        require(accepted_handshake.accepted() &&
                accepted_handshake.nonce == handshake_nonce &&
                (accepted_handshake.supported_features &
                    CONSTRUCT_FEATURES_REQUIRED) == CONSTRUCT_FEATURES_REQUIRED,
            "compatible construct handshake was not accepted");
        const auto accepted_bytes = ConstructHandshakeCodec::encode(
            accepted_handshake);
        require(ConstructHandshakeCodec::decode(accepted_bytes).accepted(),
            "construct handshake acceptance round trip failed");

        auto wrong_protocol = client_hello;
        --wrong_protocol.construct_protocol;
        const auto rejected_protocol = ConstructHandshakeCodec::evaluate(
            wrong_protocol, server_identity);
        require(rejected_protocol.kind == ConstructHandshakeKind::ServerReject &&
                rejected_protocol.reason.find("protocol mismatch") != std::string::npos,
            "construct protocol mismatch was not rejected");

        auto missing_collision = client_hello;
        missing_collision.supported_features &=
            ~static_cast<std::uint64_t>(ConstructFeatureMovingHullCollision);
        const auto rejected_features = ConstructHandshakeCodec::evaluate(
            missing_collision, server_identity);
        require(rejected_features.kind == ConstructHandshakeKind::ServerReject &&
                rejected_features.reason.find("client lacks") != std::string::npos,
            "missing moving-hull collision feature was not rejected");

        auto missing_final_pose = client_hello;
        missing_final_pose.supported_features &=
            ~static_cast<std::uint64_t>(ConstructFeatureFinalPlayerPose);
        const auto rejected_final_pose = ConstructHandshakeCodec::evaluate(
            missing_final_pose, server_identity);
        require(rejected_final_pose.kind == ConstructHandshakeKind::ServerReject &&
                rejected_final_pose.reason.find("client lacks") != std::string::npos,
            "missing final-player-pose feature was not rejected");

        bool rejected_bad_handshake = false;
        try {
            auto truncated_handshake = hello_bytes;
            truncated_handshake.pop_back();
            (void)ConstructHandshakeCodec::decode(truncated_handshake);
        } catch (const std::runtime_error &) {
            rejected_bad_handshake = true;
        }
        require(rejected_bad_handshake,
            "truncated construct handshake was not rejected");

        ConstructTransform transform{{100.0, 5.0, -30.0}, PI / 2.0};

        const Vec3d world = transform.localToWorld({2.0, 3.0, 0.0});
        require(close(world.x, 100.0), "yaw localToWorld x failed");
        require(close(world.y, 8.0), "yaw localToWorld y failed");
        require(close(world.z, -28.0), "yaw localToWorld z failed");

        const Vec3d round_trip = transform.worldToLocal(world);
        require(close(round_trip.x, 2.0), "worldToLocal x failed");
        require(close(round_trip.y, 3.0), "worldToLocal y failed");
        require(close(round_trip.z, 0.0), "worldToLocal z failed");

        // Permanent authoritative simulation uses a fixed 60 Hz step, so
        // different frame pacing produces the same ship transform.
        ConstructRegistry smooth_registry_a;
        auto smooth_a = smooth_registry_a.create();
        smooth_a->setLinearVelocity({6.0, 0.0, -3.0});
        smooth_a->setYawVelocity(0.6);
        ConstructSimulation smooth_simulation_a;
        for (int i = 0; i < 60; ++i)
            (void)smooth_simulation_a.advance(smooth_registry_a, 1.0 / 60.0);

        ConstructRegistry smooth_registry_b;
        auto smooth_b = smooth_registry_b.create();
        smooth_b->setLinearVelocity({6.0, 0.0, -3.0});
        smooth_b->setYawVelocity(0.6);
        ConstructSimulation smooth_simulation_b;
        const std::vector<double> jittered_frames{
            0.011, 0.027, 0.008, 0.019, 0.035, 0.006, 0.023, 0.014,
            0.031, 0.009, 0.017, 0.026, 0.012, 0.021, 0.016, 0.025,
            0.013, 0.029, 0.007, 0.018, 0.024, 0.010, 0.022, 0.015,
        };
        double jittered_total = 0.0;
        std::size_t frame_index = 0;
        while (jittered_total < 1.0 - 1e-12) {
            const double remaining = 1.0 - jittered_total;
            const double frame = std::min(
                jittered_frames[frame_index++ % jittered_frames.size()], remaining);
            (void)smooth_simulation_b.advance(smooth_registry_b, frame);
            jittered_total += frame;
        }
        require(close(smooth_a->transform().position.x,
                smooth_b->transform().position.x, 1e-9) &&
            close(smooth_a->transform().position.z,
                smooth_b->transform().position.z, 1e-9) &&
            close(smooth_a->transform().yaw_radians,
                smooth_b->transform().yaw_radians, 1e-9),
            "fixed-step construct simulation changed with frame pacing");
        require(close(smooth_simulation_a.simulationTime(), 1.0, 1e-9) &&
            close(smooth_simulation_b.simulationTime(), 1.0, 1e-9),
            "fixed-step construct simulation lost elapsed time");

        ConstructSimulationConfig overload_config;
        overload_config.maximum_frame_seconds = 0.25;
        overload_config.maximum_substeps = 4;
        ConstructSimulation overload_simulation(overload_config);
        ConstructRegistry overload_registry;
        auto overload_construct = overload_registry.create();
        overload_construct->setLinearVelocity({1.0, 0.0, 0.0});
        const auto overloaded = overload_simulation.advance(overload_registry, 1.0);
        require(overloaded.fixed_steps == 4 && overloaded.dropped_seconds > 0.9,
            "construct simulation did not cap overload deterministically");

        ConstructNetworkClock network_clock;
        network_clock.observe(100.0, 105.035);
        network_clock.observe(100.05, 105.100);
        network_clock.observe(100.10, 105.131);
        network_clock.observe(100.15, 105.190);
        require(network_clock.ready() && network_clock.sampleCount() == 4,
            "construct network clock did not accept samples");
        require(close(network_clock.serverTime(106.035), 101.0, 0.01),
            "construct network clock failed to map client time to server time");
        require(network_clock.jitter() > 0.0,
            "construct network clock failed to measure arrival jitter");

        ConstructRegistry registry;
        auto construct = registry.create();
        require(construct->id() != 0, "registry returned reserved id");
        require(registry.size() == 1, "registry size failed");
        construct->setOwner("droopystarfish");
        construct->setTransform({{10.0, 20.0, 30.0}, 0.25});
        construct->setLinearVelocity({2.0, -1.0, 4.0});
        construct->setYawVelocity(0.5);

        ConstructRegistry navigation_registry;
        auto navigator = navigation_registry.create();
        navigator->setTransform({{0.0, 0.0, 0.0}, 0.0});
        navigator->setLinearVelocity({0.0, 0.0, 0.0});
        navigator->setNode({0, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        ConstructNavigationEngine navigation(navigation_registry);
        ConstructNavigationConfig navigation_config;
        navigation_config.construct_id = navigator->id();
        navigation_config.mode = ConstructNavigationMode::Route;
        navigation_config.domain = ConstructNavigationDomain::Surface;
        navigation_config.maximum_speed = 8.0;
        navigation_config.maximum_acceleration = 2.0;
        navigation_config.maximum_deceleration = 2.0;
        navigation_config.maximum_yaw_rate = 0.75;
        navigation_config.stuck_timeout = 1.0;
        navigation_config.recovery_seconds = 0.5;
        require(navigation.configure(navigation_config),
            "navigation configuration failed");
        require(navigation.setRoute(navigator->id(), {
            {{0.0, 0.0, 40.0}, 1.0, 6.0, false},
            {{20.0, 0.0, 60.0}, 1.0, 4.0, true},
        }, false), "navigation route configuration failed");
        auto navigation_commands = navigation.step(0.5, 1.0);
        require(navigation_commands.size() == 1,
            "navigation did not produce one command");
        require(navigation_commands.front().forward_speed > 0.0 &&
            std::abs(navigation_commands.front().yaw_rate) < 0.01,
            "navigation straight route command failed");

        ConstructNavigationObstacle forward_obstacle;
        forward_obstacle.observer_construct_id = navigator->id();
        forward_obstacle.position = {0.0, 0.0, 8.0};
        forward_obstacle.radius = 3.0;
        forward_obstacle.sample_time = 1.0;
        forward_obstacle.expires_at = 5.0;
        require(navigation.observeObstacle(forward_obstacle),
            "navigation obstacle observation failed");
        navigation_commands = navigation.step(0.25, 1.25);
        require(navigation_commands.front().avoiding &&
            std::abs(navigation_commands.front().yaw_rate) > 0.01,
            "navigation obstacle avoidance failed");

        navigator->setTransform({{0.0, 0.0, 40.0}, 0.0});
        navigator->setLinearVelocity({0.0, 0.0, 0.0});
        navigation.clearObstacles(navigator->id());
        navigation_commands = navigation.step(0.1, 2.0);
        require(navigation_commands.front().waypoint_index == 1 &&
            navigation_commands.front().target_position.x == 20.0,
            "navigation waypoint advancement failed");
        navigator->setTransform({{20.0, 0.0, 60.0}, PI / 4.0});
        navigation_commands = navigation.step(0.1, 2.1);
        require(navigation_commands.front().completed,
            "navigation route completion failed");

        auto leader = navigation_registry.create();
        leader->setTransform({{50.0, 10.0, 50.0}, PI / 2.0});
        leader->setLinearVelocity({2.0, 0.0, 0.0});
        leader->setYawVelocity(0.1);
        leader->setNode({0, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        ConstructNavigationConfig formation_config = navigation_config;
        formation_config.mode = ConstructNavigationMode::Formation;
        formation_config.domain = ConstructNavigationDomain::Air;
        formation_config.formation_leader_id = leader->id();
        formation_config.formation_local_offset = {-10.0, 3.0, -5.0};
        formation_config.stuck_timeout = 0.0;
        require(navigation.configure(formation_config),
            "formation configuration failed");
        navigator->setTransform({{30.0, 8.0, 50.0}, 0.0});
        navigation_commands = navigation.step(0.25, 3.0);
        require(!navigation_commands.front().completed &&
            close(navigation_commands.front().target_position.x, 55.0, 1e-6) &&
            close(navigation_commands.front().target_position.y, 13.0, 1e-6) &&
            close(navigation_commands.front().target_position.z, 40.0, 1e-6),
            "formation slot transform failed");
        require(std::isfinite(navigation_commands.front().desired_velocity.x),
            "formation velocity command failed");

        ConstructNavigationConfig recovery_config = navigation_config;
        recovery_config.stuck_timeout = 0.2;
        recovery_config.recovery_seconds = 0.5;
        require(navigation.configure(recovery_config),
            "recovery navigation configuration failed");
        require(navigation.setRoute(navigator->id(), {
            {{0.0, 0.0, 100.0}, 1.0, 4.0, true},
        }, false), "recovery route setup failed");
        navigator->setTransform({{0.0, 0.0, 0.0}, 0.0});
        navigator->setLinearVelocity({0.0, 0.0, 0.0});
        navigation_commands = navigation.step(0.15, 4.0);
        navigation_commands = navigation.step(0.15, 4.15);
        navigation_commands = navigation.step(0.15, 4.30);
        require(navigation_commands.front().recovering &&
            navigation_commands.front().forward_speed < 0.0,
            "navigation stuck recovery failed");

        // Identical sensor-local obstacle IDs from different observers must not
        // overwrite each other. Each craft's six terrain rays intentionally use
        // the same small ID range.
        auto second_observer = navigation_registry.create();
        second_observer->setNode({0, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        ConstructNavigationConfig second_config = navigation_config;
        second_config.construct_id = second_observer->id();
        require(navigation.configure(second_config),
            "second navigation observer configuration failed");
        ConstructNavigationObstacle observer_one_obstacle;
        observer_one_obstacle.id = 7;
        observer_one_obstacle.observer_construct_id = navigator->id();
        observer_one_obstacle.position = {1.0, 0.0, 5.0};
        observer_one_obstacle.sample_time = 5.0;
        observer_one_obstacle.expires_at = 6.0;
        ConstructNavigationObstacle observer_two_obstacle = observer_one_obstacle;
        observer_two_obstacle.observer_construct_id = second_observer->id();
        observer_two_obstacle.position = {-1.0, 0.0, 5.0};
        require(navigation.observeObstacle(observer_one_obstacle) &&
            navigation.observeObstacle(observer_two_obstacle) &&
            navigation.obstacles(navigator->id()).size() == 1 &&
            navigation.obstacles(second_observer->id()).size() == 1,
            "navigation obstacle IDs leaked across observers");

        // A vertically aligned aircraft should climb without drifting along its
        // current heading.
        ConstructNavigationConfig vertical_config = navigation_config;
        vertical_config.mode = ConstructNavigationMode::Route;
        vertical_config.domain = ConstructNavigationDomain::Air;
        vertical_config.stuck_timeout = 0.0;
        require(navigation.configure(vertical_config),
            "vertical navigation configuration failed");
        require(navigation.setRoute(navigator->id(), {
            {{0.0, 20.0, 0.0}, 1.0, 4.0, true},
        }, false), "vertical navigation route setup failed");
        navigator->setTransform({{0.0, 0.0, 0.0}, 1.1});
        navigator->setLinearVelocity({0.0, 0.0, 0.0});
        navigation.clearObstacles(navigator->id());
        navigation_commands = navigation.step(0.25, 6.0);
        const auto vertical_command = std::find_if(navigation_commands.begin(),
            navigation_commands.end(), [navigator](const auto &candidate) {
                return candidate.construct_id == navigator->id();
            });
        require(vertical_command != navigation_commands.end() &&
            close(vertical_command->forward_speed, 0.0) &&
            vertical_command->vertical_speed > 0.0,
            "vertical-only navigation introduced horizontal drift");

        ConstructRegistry structure_registry;
        auto split_parent = structure_registry.create();
        split_parent->setOwner("droopystarfish");
        split_parent->setTransform({{10.0, -0.25, 5.0}, PI / 2.0});
        split_parent->setLinearVelocity({1.0, 0.0, 2.0});
        split_parent->setYawVelocity(0.5);
        split_parent->setNode({0, 0, 0}, {1, 0, 0, "nc_core:helm", ""});
        split_parent->setNode({1, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        split_parent->setNode({2, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        split_parent->setNode({3, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        split_parent->setNode({4, 0, 0}, {1, 0, 0, "nc_core:steel_fragment_a", ""});
        split_parent->setNode({5, 0, 0}, {1, 0, 0, "nc_core:steel_fragment_b", ""});
        split_parent->setNodeMetadataField({4, 0, 0}, "cargo", "survives split");
        require(split_parent->setLiquid({4, -1, 0}, {"water", 6, 0}),
            "fragment liquid setup failed");
        ConstructStructureEngine structure_engine(structure_registry);
        ConstructStructuralConfig structure_config;
        structure_config.water_level = 1.0;
        structure_config.apply_fragment_physics = true;
        structure_config.apply_primary_physics = false;
        auto structural_events = structure_engine.step(0.0, structure_config);
        require(structure_engine.state(split_parent->id()).has_value(),
            "initial structural state was not created");
        require(split_parent->removeNode({3, 0, 0}),
            "structural bridge removal failed");
        const Vec3d old_fragment_a_world = split_parent->transform().localToWorld({4.0, 0.0, 0.0});
        structural_events = structure_engine.step(0.0, structure_config);
        const auto split_event = std::find_if(structural_events.begin(), structural_events.end(),
            [](const auto &event) {
                return event.kind == ConstructStructuralEventKind::Split;
            });
        require(split_event != structural_events.end() &&
            split_event->fragment_ids.size() == 1 && split_event->moved_nodes == 2,
            "structural split did not detach the disconnected component");
        require(split_parent->nodeCount() == 3 &&
            split_parent->getNode({0, 0, 0}) != nullptr,
            "structural primary component selection failed");
        const auto fragment = structure_registry.find(split_event->fragment_ids.front());
        require(fragment && fragment->nodeCount() == 2,
            "detached fragment was not registered");
        const auto fragment_nodes_for_lookup = fragment->nodes();
        const auto fragment_a_lookup = std::find_if(fragment_nodes_for_lookup.begin(),
            fragment_nodes_for_lookup.end(), [](const auto &entry) {
                return entry.node.node_name == "nc_core:steel_fragment_a";
            });
        require(fragment_a_lookup != fragment_nodes_for_lookup.end(),
            "fragment node identity was lost");
        const Vec3d new_fragment_a_world = fragment->transform().localToWorld({
            static_cast<double>(fragment_a_lookup->position.x),
            static_cast<double>(fragment_a_lookup->position.y),
            static_cast<double>(fragment_a_lookup->position.z)});
        require(close(new_fragment_a_world.x, old_fragment_a_world.x, 1e-8) &&
            close(new_fragment_a_world.y, old_fragment_a_world.y, 1e-8) &&
            close(new_fragment_a_world.z, old_fragment_a_world.z, 1e-8),
            "fragment recentering changed world-space node positions");
        const auto *fragment_node_state = fragment->getNodeState(fragment_a_lookup->position);
        require(fragment_node_state &&
            fragment_node_state->fields.at("cargo") == "survives split",
            "fragment node metadata was not transferred");
        const LocalNodePos fragment_liquid_position{
            fragment_a_lookup->position.x,
            fragment_a_lookup->position.y - 1,
            fragment_a_lookup->position.z};
        const auto *fragment_liquid = fragment->getLiquid(fragment_liquid_position);
        require(fragment_liquid && fragment_liquid->liquid_name == "water" &&
            fragment_liquid->level == 6 && !split_parent->getLiquid({4, -1, 0}),
            "construct-local liquid was not transferred to detached fragment");
        require(close(fragment->linearVelocity().x, -1.5, 1e-8) &&
            close(fragment->linearVelocity().z, 2.0, 1e-8),
            "fragment did not inherit rotational point velocity");
        structural_events = structure_engine.step(0.25, structure_config);
        const auto fragment_state = structure_engine.state(fragment->id());
        require(fragment_state && fragment_state->role == ConstructStructuralRole::Fragment &&
            fragment_state->sinking && fragment->linearVelocity().y < 0.0,
            "heavy detached fragment did not receive independent sinking physics");

        DynamicConstruct enclosure_construct(9000);
        for (int x = 0; x < 3; ++x)
        for (int y = 0; y < 3; ++y)
        for (int z = 0; z < 3; ++z) {
            if (x == 1 && y == 1 && z == 1)
                continue;
            enclosure_construct.setNode({x, y, z},
                {1, 0, 0, "nc_core:hull_wood", ""});
        }
        const auto enclosure_analysis = structure_engine.analyse(enclosure_construct,
            structure_config);
        require(enclosure_analysis.components.size() == 1 &&
            close(enclosure_analysis.components.front().enclosed_volume, 1.0),
            "structural enclosed-air flood fill failed");

        DynamicConstruct decorative_bridge(9001);
        decorative_bridge.setNode({0, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        decorative_bridge.setNode({1, 0, 0}, {1, 0, 0, "nc_core:sign", ""});
        decorative_bridge.setNode({2, 0, 0}, {1, 0, 0, "nc_core:frame", ""});
        const auto decorative_analysis = structure_engine.analyse(decorative_bridge,
            structure_config);
        require(decorative_analysis.components.size() == 2,
            "decorative node incorrectly acted as a structural bridge");

        ConstructRegistry articulation_registry;
        auto articulated_construct = articulation_registry.create();
        articulated_construct->setTransform({{0.0, 0.0, 0.0}, 0.0});
        articulated_construct->setNode({0, 1, 0}, {1, 0, 0, "nc_core:turret_base", ""});
        articulated_construct->setNode({0, 1, 1}, {1, 0, 0, "nc_core:turret_cradle", ""});
        articulated_construct->setNode({0, 1, 2}, {1, 0, 0, "nc_core:barrel", ""});
        articulated_construct->setNode({0, 1, 3}, {1, 0, 0, "nc_core:barrel", ""});
        articulated_construct->setNode({0, 1, 6}, {1, 0, 0, "nc_core:superstructure", ""});
        articulated_construct->setNode({3, 0, 0}, {1, 0, 0, "nc_core:door", ""});

        ConstructArticulationEngine articulation_engine(articulation_registry);
        ConstructArticulationDefinition yaw_joint;
        yaw_joint.construct_id = articulated_construct->id();
        yaw_joint.name = "turret_yaw";
        yaw_joint.pivot_local = {0.5, 1.5, 0.5};
        yaw_joint.axis_local = {0.0, 1.0, 0.0};
        yaw_joint.nodes = {{0, 1, 0}, {0, 1, 1}};
        yaw_joint.minimum_position = -PI;
        yaw_joint.maximum_position = PI;
        yaw_joint.maximum_speed = 4.0;
        yaw_joint.maximum_acceleration = 12.0;
        const auto yaw_joint_id = articulation_engine.configureJoint(yaw_joint);

        ConstructArticulationDefinition pitch_joint;
        pitch_joint.construct_id = articulated_construct->id();
        pitch_joint.parent_id = yaw_joint_id;
        pitch_joint.name = "turret_pitch";
        pitch_joint.pivot_local = {0.5, 1.5, 1.5};
        pitch_joint.axis_local = {-1.0, 0.0, 0.0};
        pitch_joint.nodes = {{0, 1, 2}, {0, 1, 3}};
        pitch_joint.minimum_position = -0.25;
        pitch_joint.maximum_position = 0.75;
        pitch_joint.maximum_speed = 3.0;
        pitch_joint.maximum_acceleration = 10.0;
        const auto pitch_joint_id = articulation_engine.configureJoint(pitch_joint);

        ConstructArticulationDefinition door_joint;
        door_joint.construct_id = articulated_construct->id();
        door_joint.name = "sliding_door";
        door_joint.kind = ConstructJointKind::Prismatic;
        door_joint.pivot_local = {3.0, 0.0, 0.0};
        door_joint.axis_local = {1.0, 0.0, 0.0};
        door_joint.nodes = {{3, 0, 0}};
        door_joint.minimum_position = 0.0;
        door_joint.maximum_position = 2.0;
        door_joint.maximum_speed = 2.0;
        door_joint.maximum_acceleration = 8.0;
        const auto door_joint_id = articulation_engine.configureJoint(door_joint);
        require(articulation_engine.setJointTarget(door_joint_id, 2.0),
            "articulated sliding-door target failed");

        ConstructTurretDefinition turret_definition;
        turret_definition.construct_id = articulated_construct->id();
        turret_definition.yaw_joint_id = yaw_joint_id;
        turret_definition.pitch_joint_id = pitch_joint_id;
        turret_definition.name = "main_battery";
        turret_definition.muzzle_local = {0.5, 1.5, 4.0};
        turret_definition.forward_local = {0.0, 0.0, 1.0};
        turret_definition.alignment_tolerance_radians = 0.02;
        turret_definition.projectile_radius = 0.08;
        const auto turret_id = articulation_engine.configureTurret(turret_definition);
        require(articulation_engine.setTurretTarget(turret_id, {0.5, 8.0, 20.0}),
            "articulated turret target failed");
        for (int index = 0; index < 120; ++index)
            (void)articulation_engine.step(0.05, index * 0.05);
        const auto aimed_turret = articulation_engine.turret(turret_id);
        require(aimed_turret && aimed_turret->aligned &&
            aimed_turret->current_pitch > 0.1,
            "articulated turret did not converge on an elevated target");
        const auto opened_door = articulation_engine.joint(door_joint_id);
        require(opened_door && close(opened_door->position, 2.0, 1e-5),
            "prismatic articulation did not reach its target");

        require(articulation_engine.setJointPosition(yaw_joint_id, 0.0) &&
            articulation_engine.setJointPosition(pitch_joint_id, 0.0),
            "articulated turret reset failed");
        require(articulation_engine.setTurretTarget(turret_id, {0.5, 1.5, 20.0}),
            "articulated line-of-fire target failed");
        const auto obstructed_turret = articulation_engine.turret(turret_id);
        require(obstructed_turret && obstructed_turret->obstructed &&
            obstructed_turret->obstruction &&
            obstructed_turret->obstruction->node_position == LocalNodePos{0, 1, 6},
            "turret line-of-fire obstruction failed");
        require(articulation_engine.nodeJoint(articulated_construct->id(), {0, 1, 3}) ==
            std::optional<ConstructArticulationId>{pitch_joint_id},
            "articulated node ownership lookup failed");

        require(articulation_engine.setJointPosition(yaw_joint_id, PI / 2.0),
            "articulated yaw position failed");
        const Vec3d rotated_barrel = articulation_engine.transformPointLocal(
            pitch_joint_id, {0.5, 1.5, 4.0});
        require(rotated_barrel.x > 3.0 && close(rotated_barrel.z, 0.5, 1e-6),
            "hierarchical articulation point transform failed");

        const auto yaw_state = articulation_engine.joint(yaw_joint_id);
        require(yaw_state.has_value(), "articulation state lookup failed");
        const auto definition_packet = ConstructArticulationPacketCodec::encodeDefinition(
            yaw_state->definition);
        const auto decoded_definition_packet = ConstructArticulationPacketCodec::decode(
            definition_packet);
        require(decoded_definition_packet.kind == ConstructArticulationPacketKind::Definition &&
            decoded_definition_packet.definition.nodes.size() == 2,
            "articulation definition packet round trip failed");
        ConstructArticulationSnapshot articulation_snapshot{
            articulated_construct->id(), yaw_joint_id, 7, 10.0,
            PI / 2.0, PI / 2.0, 0.25, true};
        const auto articulation_state_packet =
            ConstructArticulationPacketCodec::encodeState(articulation_snapshot);
        ClientConstructArticulationState client_articulations;
        require(client_articulations.applyPacket(definition_packet) &&
            client_articulations.applyPacket(articulation_state_packet),
            "client articulation packet apply failed");
        require(!client_articulations.applyPacket(articulation_state_packet),
            "stale articulation state packet was accepted");
        require(close(client_articulations.samplePosition(yaw_joint_id, 10.2, 0.0, 0.25),
            PI / 2.0 + 0.05, 1e-6),
            "client articulation extrapolation failed");

        articulated_construct->setNode({8, 0, 0},
            {1, 0, 0, "nc_core:elevator_platform", ""});
        ConstructArticulationDefinition elevator_joint;
        elevator_joint.construct_id = articulated_construct->id();
        elevator_joint.name = "cargo_elevator";
        elevator_joint.kind = ConstructJointKind::Prismatic;
        elevator_joint.pivot_local = {8.0, 0.0, 0.0};
        elevator_joint.axis_local = {0.0, 1.0, 0.0};
        elevator_joint.nodes = {{8, 0, 0}};
        elevator_joint.minimum_position = 0.0;
        elevator_joint.maximum_position = 3.0;
        elevator_joint.maximum_speed = 2.0;
        elevator_joint.maximum_acceleration = 8.0;
        const auto elevator_joint_id = articulation_engine.configureJoint(elevator_joint);
        const Aabb3d elevator_rider_box{{8.15, 1.04, 0.15}, {8.85, 2.80, 0.85}};
        const auto elevator_support = ConstructArticulatedMotion::findSupport(
            articulation_engine, elevator_rider_box, 0.2, 0.1);
        require(elevator_support && elevator_support->articulation_id == elevator_joint_id,
            "articulated elevator support query failed");
        ArticulatedRiderState elevator_rider_state;
        ArticulatedMotionInput elevator_input;
        elevator_input.position = {8.5, 1.04, 0.5};
        elevator_input.velocity = {0.0, -0.1, 0.0};
        elevator_input.world_box = elevator_rider_box;
        elevator_input.delta_seconds = 0.05;
        const auto elevator_landed = ConstructArticulatedMotion::stepRider(
            elevator_rider_state, articulation_engine, elevator_input);
        require(elevator_landed.grounded && elevator_landed.contact_started,
            "rider did not acquire articulated elevator contact");
        require(articulation_engine.setJointPosition(elevator_joint_id, 1.5),
            "articulated elevator position failed");
        elevator_input.position = elevator_landed.position;
        elevator_input.world_box = elevator_rider_box.translated({0.0, -0.04, 0.0});
        elevator_input.velocity = {};
        elevator_input.delta_seconds = 0.1;
        const auto elevator_carried = ConstructArticulatedMotion::stepRider(
            elevator_rider_state, articulation_engine, elevator_input);
        require(elevator_carried.grounded && elevator_carried.platform_displacement.y > 1.4,
            "articulated elevator did not carry rider vertically");
        require(articulation_engine.pointVelocityWorld(elevator_joint_id,
            {8.5, 1.0, 0.5}).y >= 0.0,
            "articulated point velocity was invalid");

        ArticulatedMotionInput articulation_collision_input;
        articulation_collision_input.position = {7.8, 1.6, 0.5};
        articulation_collision_input.velocity = {3.0, 0.0, 0.0};
        articulation_collision_input.world_box = {{7.5, 1.5, 0.2}, {8.1, 2.3, 0.8}};
        articulation_collision_input.desired_delta = {1.0, 0.0, 0.0};
        articulation_collision_input.delta_seconds = 0.25;
        const auto articulation_collision = ConstructArticulatedMotion::move(
            articulation_engine, articulation_collision_input);
        require(articulation_collision.collided,
            "articulated moving-part collision failed");

        RiderStatePacket articulated_rider_packet;
        articulated_rider_packet.sequence = 99;
        articulated_rider_packet.construct_id = articulated_construct->id();
        articulated_rider_packet.articulation_id = elevator_joint_id;
        articulated_rider_packet.local_anchor = elevator_rider_state.contact.local_anchor;
        articulated_rider_packet.world_position = elevator_carried.position;
        articulated_rider_packet.velocity = elevator_carried.velocity;
        articulated_rider_packet.client_time = 12.5;
        articulated_rider_packet.grounded = true;
        const auto articulated_rider_bytes = RiderPacketCodec::encode(
            articulated_rider_packet);
        const auto decoded_articulated_rider = RiderPacketCodec::decode(
            articulated_rider_bytes);
        require(decoded_articulated_rider.articulation_id == elevator_joint_id,
            "articulated rider packet lost joint identity");

        ConstructRegistry liquid_registry;
        auto liquid_construct = liquid_registry.create();
        liquid_construct->setNode({0, -1, 0},
            {1, 0, 0, "nc_core:hull", ""});
        liquid_construct->setNode({1, -1, 0},
            {1, 0, 0, "nc_core:hull", ""});
        ConstructLiquidEngine liquid_engine(liquid_registry);
        ConstructLiquidCompartmentDefinition compartment_definition;
        compartment_definition.construct_id = liquid_construct->id();
        compartment_definition.name = "ballast_test";
        compartment_definition.cells = {{0, 0, 0}, {0, 1, 0},
            {1, 0, 0}, {1, 1, 0}};
        const auto compartment_id = liquid_engine.configureCompartment(
            compartment_definition);
        require(liquid_engine.setCell(liquid_construct->id(), {0, 1, 0},
            "water", 8), "construct liquid insertion failed");
        auto liquid_step = liquid_engine.step(0.5);
        const auto lower_water = liquid_engine.cell(
            liquid_construct->id(), {0, 0, 0});
        require(lower_water && lower_water->level > 0,
            "construct liquid did not flow downward");
        ConstructLiquidPortDefinition pump;
        pump.compartment_id = compartment_id;
        pump.position = {1, 1, 0};
        pump.kind = ConstructLiquidPortKind::Source;
        pump.liquid_name = "water";
        pump.units_per_second = 8.0;
        const auto pump_id = liquid_engine.configurePort(pump);
        require(pump_id != 0, "construct liquid source port failed");
        liquid_step = liquid_engine.step(1.0);
        require(!liquid_step.compartments.empty() &&
            liquid_step.compartments.front().total_units >= 16,
            "construct liquid source did not add volume");
        const auto liquid_status = liquid_engine.compartments(
            liquid_construct->id());
        require(liquid_status.size() == 1 &&
            liquid_status.front().fill_fraction > 0.0 &&
            liquid_status.front().fill_fraction <= 1.0,
            "construct liquid compartment status failed");

        ConstructSpecialNodeEngine special_nodes;
        ConstructSpecialNodeDefinition conveyor_definition;
        conveyor_definition.node_name = "nc_core:conveyor";
        conveyor_definition.conveyor_velocity_local = {2.0, 0.0, 0.0};
        conveyor_definition.conveyor_acceleration = 20.0;
        require(special_nodes.registerDefinition(conveyor_definition),
            "special conveyor registration failed");
        ConstructSpecialNodeDefinition ladder_definition;
        ladder_definition.node_name = "nc_core:ladder";
        ladder_definition.climbable = true;
        require(special_nodes.registerDefinition(ladder_definition),
            "special ladder registration failed");
        liquid_construct->setNode({2, 0, 0},
            {1, 0, 0, "nc_core:conveyor", ""});
        liquid_construct->setNode({3, 0, 0},
            {1, 0, 0, "nc_core:ladder", ""});
        std::vector<const DynamicConstruct *> special_constructs{liquid_construct.get()};
        ConstructSpecialBodyInput special_input;
        special_input.world_box = {{2.1, 0.1, 0.1}, {2.9, 1.7, 0.9}};
        special_input.velocity = {};
        special_input.delta_seconds = 0.1;
        const auto conveyor_result = special_nodes.evaluate(
            special_constructs, nullptr, special_input);
        require(conveyor_result.velocity.x > 0.5,
            "construct conveyor did not accelerate body");
        special_input.world_box = {{3.1, 0.1, 0.1}, {3.9, 1.7, 0.9}};
        special_input.climbing_input = true;
        const auto ladder_result = special_nodes.evaluate(
            special_constructs, nullptr, special_input);
        require(ladder_result.climbing && ladder_result.velocity.y >= 2.0,
            "construct climbable node behavior failed");

        ConstructEffectEvent effect;
        effect.sequence = 1;
        effect.construct_id = construct->id();
        effect.effect_id = 77;
        effect.kind = ConstructEffectKind::SoundLoopStart;
        effect.preset = ConstructEffectPreset::Engine;
        effect.local_position = {1.0, 2.0, 3.0};
        effect.local_direction = {0.0, 0.0, 1.0};
        effect.velocity = {0.5, 0.0, 0.0};
        effect.sound_name = "nc_engine_loop";
        effect.gain = 0.8;
        effect.pitch = 1.2;
        effect.amount = 12;
        const auto effect_bytes = ConstructEffectPacketCodec::encode(effect);
        const auto decoded_effect = ConstructEffectPacketCodec::decode(effect_bytes);
        require(decoded_effect.construct_id == effect.construct_id &&
            decoded_effect.effect_id == effect.effect_id &&
            decoded_effect.sound_name == effect.sound_name &&
            close(decoded_effect.pitch, effect.pitch),
            "construct effect packet round trip failed");
        ConstructEffectState effect_state;
        require(effect_state.apply(decoded_effect), "construct effect state apply failed");
        require(!effect_state.apply(decoded_effect), "stale construct effect was accepted");
        require(effect_state.findActive(effect.construct_id, effect.effect_id).has_value(),
            "construct loop effect was not activated");
        const auto sampled_effect = ConstructEffectState::sample(decoded_effect, *construct);
        require(std::isfinite(sampled_effect.world_position.x) &&
            std::isfinite(sampled_effect.world_velocity.z),
            "construct effect sampling failed");
        effect.sequence = 2;
        effect.kind = ConstructEffectKind::SoundLoopStop;
        require(effect_state.apply(effect), "construct loop stop failed");
        require(!effect_state.findActive(effect.construct_id, effect.effect_id).has_value(),
            "construct loop effect was not stopped");

        require(TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE == 0x6A &&
            TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION == 0x6B &&
            TOCLIENT_NAVYCRAFT_HANDSHAKE == 0x6C &&
            TOCLIENT_NAVYCRAFT_NUM_MSG_TYPES == 0x6D &&
            TOSERVER_NAVYCRAFT_HANDSHAKE == 0x56 &&
            TOSERVER_NAVYCRAFT_NUM_MSG_TYPES == 0x57,
            "native protocol command range is inconsistent");

        ConstructRegistry projectile_registry;
        auto firing_construct = projectile_registry.create();
        firing_construct->setTransform({{0.0, 0.0, 0.0}, 0.0});
        auto target_construct = projectile_registry.create();
        target_construct->setTransform({{10.0, 0.0, 0.0}, PI / 4.0});
        target_construct->setNode({0, 0, 0},
            ConstructNode{2, 0, 0, "nc_core:frame", {}});
        target_construct->setNode({0, 1, 0},
            ConstructNode{3, 0, 0, "nc_core:glass", {}});
        ConstructProjectileEngine projectile_engine(projectile_registry);
        ConstructProjectileState shell;
        shell.source_construct_id = firing_construct->id();
        shell.owner = "gunner";
        shell.spec.kind = ConstructProjectileKind::Shell;
        shell.spec.arming_time = 0.0;
        shell.spec.maximum_range = 100.0;
        shell.spec.maximum_age = 10.0;
        shell.spec.blast_radius = 2.5;
        shell.spec.blast_power = 12.0;
        shell.spec.penetration = 4.0;
        shell.position = {0.0, 0.5, 0.5};
        shell.velocity = {100.0, 0.0, 0.0};
        const ProjectileId shell_id = projectile_engine.spawn(shell);
        require(shell_id != 0 && projectile_engine.size() == 1,
            "native projectile spawn failed");
        const auto shell_events = projectile_engine.step(0.2);
        const auto impact_it = std::find_if(shell_events.begin(), shell_events.end(),
            [](const ConstructProjectileEvent &event) {
                return event.kind == ConstructProjectileEventKind::Impact;
            });
        require(impact_it != shell_events.end() && impact_it->impact.has_value(),
            "swept projectile did not hit rotated construct");
        require(impact_it->impact->construct_id == target_construct->id(),
            "projectile hit wrong construct");
        require(impact_it->impact->explosion.destroyed_nodes >= 1,
            "construct explosion removed no nodes");
        require(projectile_engine.size() == 0,
            "impacted projectile remained active");

        auto grazing_target = projectile_registry.create();
        grazing_target->setTransform({{10.0, 0.0, 5.0}, 0.0});
        grazing_target->setNode({0, 0, 0},
            ConstructNode{5, 0, 0, "nc_core:steel", {}});
        ConstructProjectileState grazing_shell;
        grazing_shell.source_construct_id = firing_construct->id();
        grazing_shell.spec.kind = ConstructProjectileKind::Shell;
        grazing_shell.spec.radius = 0.5;
        grazing_shell.spec.arming_time = 0.0;
        grazing_shell.spec.maximum_range = 100.0;
        grazing_shell.spec.maximum_age = 10.0;
        grazing_shell.spec.blast_radius = 1.0;
        grazing_shell.spec.blast_power = 10.0;
        grazing_shell.position = {0.0, 1.4, 5.5};
        grazing_shell.velocity = {100.0, 0.0, 0.0};
        (void)projectile_engine.spawn(grazing_shell);
        const auto grazing_events = projectile_engine.step(0.2);
        const auto grazing_impact = std::find_if(grazing_events.begin(), grazing_events.end(),
            [](const ConstructProjectileEvent &event) {
                return event.kind == ConstructProjectileEventKind::Impact;
            });
        require(grazing_impact != grazing_events.end() && grazing_impact->impact &&
            grazing_impact->impact->construct_id == grazing_target->id(),
            "projectile radius did not produce a swept grazing hit");
        const auto projectile_packet = ConstructProjectilePacketCodec::encode(*impact_it);
        const auto projectile_decoded = ConstructProjectilePacketCodec::decode(projectile_packet);
        require(projectile_decoded.projectile.id == shell_id &&
            projectile_decoded.impact.has_value() &&
            projectile_decoded.impact->kind == ConstructImpactKind::Construct,
            "projectile packet round trip failed");
        ConstructProjectileClientState projectile_client_state;
        ConstructProjectileEvent spawn_event{
            ConstructProjectileEventKind::Spawn, impact_it->projectile, std::nullopt};
        spawn_event.projectile.sequence = 1;
        require(projectile_client_state.apply(spawn_event) &&
            projectile_client_state.size() == 1,
            "client projectile spawn state failed");
        require(!projectile_client_state.apply(spawn_event),
            "client projectile accepted stale sequence");
        auto impact_event = *impact_it;
        impact_event.projectile.sequence = 2;
        require(projectile_client_state.apply(impact_event) &&
            projectile_client_state.size() == 0 &&
            projectile_client_state.drainImpacts().size() == 1,
            "client projectile impact state failed");

        auto torpedo_target = projectile_registry.create();
        torpedo_target->setTransform({{20.0, -2.0, 20.0}, 0.0});
        torpedo_target->setNode({0, 0, 0},
            ConstructNode{4, 0, 0, "nc_core:frame", {}});
        ConstructProjectileState torpedo;
        torpedo.source_construct_id = firing_construct->id();
        torpedo.target_construct_id = torpedo_target->id();
        torpedo.spec.kind = ConstructProjectileKind::Torpedo;
        torpedo.spec.guided = true;
        torpedo.spec.guidance_turn_rate = 2.0;
        torpedo.spec.preferred_depth = -2.0;
        torpedo.spec.arming_time = 1.0;
        torpedo.spec.maximum_age = 20.0;
        torpedo.spec.maximum_range = 500.0;
        torpedo.position = {0.0, 0.0, 0.0};
        torpedo.velocity = {0.0, 0.0, 12.0};
        const ProjectileId torpedo_id = projectile_engine.spawn(torpedo);
        (void)projectile_engine.step(0.25);
        const auto guided_torpedo = projectile_engine.find(torpedo_id);
        require(guided_torpedo.has_value() && guided_torpedo->velocity.x > 0.0 &&
            guided_torpedo->velocity.y < 0.0,
            "guided torpedo steering or depth keeping failed");
        projectile_engine.clear();

        ConstructRegistry fire_control_registry;
        auto fire_control_shooter = fire_control_registry.create();
        fire_control_shooter->setTransform({{0.0, 0.0, 0.0}, 0.0});
        fire_control_shooter->setLinearVelocity({2.0, 0.0, 0.0});
        auto fire_control_target = fire_control_registry.create();
        fire_control_target->setTransform({{0.0, 0.0, 100.0}, 0.0});
        fire_control_target->setLinearVelocity({10.0, 0.0, 0.0});
        ConstructFireControlEngine fire_control(fire_control_registry);
        FireControlObservation first_observation;
        first_observation.observer_construct_id = fire_control_shooter->id();
        first_observation.target_construct_id = fire_control_target->id();
        first_observation.sample_time = 10.0;
        first_observation.position = fire_control_target->transform().position;
        first_observation.velocity = fire_control_target->linearVelocity();
        first_observation.classification = FireControlTargetClass::Surface;
        first_observation.sensor = FireControlSensorKind::Radar;
        first_observation.confidence = 0.9;
        require(fire_control.observe(first_observation),
            "fire-control observation was rejected");
        FireControlRequest direct_request;
        direct_request.shooter_construct_id = fire_control_shooter->id();
        direct_request.target_construct_id = fire_control_target->id();
        direct_request.weapon.projectile_kind = ConstructProjectileKind::Shell;
        direct_request.weapon.muzzle_speed = 50.0;
        direct_request.weapon.maximum_range = 500.0;
        direct_request.weapon.maximum_lead_time = 10.0;
        direct_request.solution_time = 10.0;
        const auto direct_solution = fire_control.solve(direct_request);
        require(direct_solution.valid && direct_solution.aim_point.x > 10.0,
            "direct fire-control lead solution failed");
        require(direct_solution.intercept_time > 1.9 &&
            direct_solution.intercept_time < 2.2,
            "direct fire-control intercept time failed");
        require(direct_solution.estimated_miss_distance < 0.05,
            "direct fire-control miss estimate failed");

        auto ballistic_target = fire_control_registry.create();
        ballistic_target->setTransform({{80.0, 0.0, 0.0}, 0.0});
        FireControlObservation ballistic_observation = first_observation;
        ballistic_observation.target_construct_id = ballistic_target->id();
        ballistic_observation.position = ballistic_target->transform().position;
        ballistic_observation.velocity = {0.0, 0.0, 0.0};
        require(fire_control.observe(ballistic_observation),
            "ballistic target observation failed");
        FireControlRequest ballistic_request = direct_request;
        ballistic_request.target_construct_id = ballistic_target->id();
        ballistic_request.weapon.muzzle_speed = 40.0;
        ballistic_request.weapon.gravity = 9.81;
        ballistic_request.weapon.maximum_lead_time = 8.0;
        const auto ballistic_solution = fire_control.solve(ballistic_request);
        require(ballistic_solution.valid && ballistic_solution.pitch_radians > 0.0,
            "ballistic fire-control solution failed");
        require(ballistic_solution.estimated_miss_distance < 0.75,
            "ballistic fire-control accuracy failed");
        FireControlRequest high_arc_request = ballistic_request;
        high_arc_request.weapon.arc_preference = FireControlArcPreference::High;
        const auto high_arc_solution = fire_control.solve(high_arc_request);
        require(high_arc_solution.valid &&
            high_arc_solution.pitch_radians > ballistic_solution.pitch_radians,
            "high ballistic arc selection failed");

        auto torpedo_solution_target = fire_control_registry.create();
        torpedo_solution_target->setTransform({{60.0, -4.0, 80.0}, 0.0});
        torpedo_solution_target->setLinearVelocity({3.0, 0.0, 0.0});
        FireControlObservation torpedo_observation = first_observation;
        torpedo_observation.target_construct_id = torpedo_solution_target->id();
        torpedo_observation.position = torpedo_solution_target->transform().position;
        torpedo_observation.velocity = torpedo_solution_target->linearVelocity();
        torpedo_observation.classification = FireControlTargetClass::Submarine;
        torpedo_observation.sensor = FireControlSensorKind::ActiveSonar;
        require(fire_control.observe(torpedo_observation),
            "torpedo target observation failed");
        FireControlRequest torpedo_request = direct_request;
        torpedo_request.target_construct_id = torpedo_solution_target->id();
        torpedo_request.weapon.projectile_kind = ConstructProjectileKind::Torpedo;
        torpedo_request.weapon.muzzle_speed = 20.0;
        torpedo_request.weapon.preferred_depth = -4.0;
        torpedo_request.weapon.maximum_lead_time = 15.0;
        const auto torpedo_solution = fire_control.solve(torpedo_request);
        require(torpedo_solution.valid && torpedo_solution.aim_point.x > 60.0 &&
            close(torpedo_solution.aim_point.y, -4.0),
            "torpedo intercept solution failed");

        auto air_target = fire_control_registry.create();
        air_target->setTransform({{0.0, 40.0, 120.0}, 0.0});
        air_target->setLinearVelocity({-8.0, 0.0, 0.0});
        FireControlObservation air_observation = first_observation;
        air_observation.target_construct_id = air_target->id();
        air_observation.position = air_target->transform().position;
        air_observation.velocity = air_target->linearVelocity();
        air_observation.classification = FireControlTargetClass::Air;
        air_observation.confidence = 0.95;
        require(fire_control.observe(air_observation),
            "air target observation failed");
        FireControlBattery battery;
        battery.shooter_construct_id = fire_control_shooter->id();
        battery.weapon.projectile_kind = ConstructProjectileKind::AntiAircraft;
        battery.weapon.muzzle_speed = 90.0;
        battery.weapon.gravity = 9.81;
        battery.weapon.maximum_range = 400.0;
        battery.weapon.maximum_lead_time = 8.0;
        battery.mode = FireControlBatteryMode::Defensive;
        battery.allowed_class_mask = fireControlClassMask(FireControlTargetClass::Air);
        battery.minimum_track_confidence = 0.5;
        battery.reload_seconds = 2.0;
        const auto battery_id = fire_control.configureBattery(battery);
        const auto automatic_orders = fire_control.stepAutomatic(10.0);
        require(automatic_orders.size() == 1 &&
            automatic_orders.front().battery_id == battery_id &&
            automatic_orders.front().target_construct_id == air_target->id(),
            "automatic defensive target selection failed");
        require(fire_control.stepAutomatic(11.0).empty(),
            "automatic battery ignored reload time");
        battery.id = battery_id;
        battery.next_ready_time = 12.0;
        battery.traverse_centre_radians = 1.5707963267948966;
        battery.traverse_half_width_radians = 0.05;
        fire_control.configureBattery(battery);
        require(fire_control.stepAutomatic(12.0).empty(),
            "automatic battery ignored traverse limits");
        fire_control.removeConstruct(air_target->id());
        const auto retained_battery = fire_control.findBattery(battery_id);
        require(!fire_control.findTrack(fire_control_shooter->id(), air_target->id()) &&
            retained_battery && retained_battery->designated_target_id == 0,
            "removed target was retained by fire control");
        fire_control.removeConstruct(fire_control_shooter->id());
        require(fire_control.batteries().empty() && fire_control.tracks().empty(),
            "removed shooter retained fire-control state");

        const LocalNodePos helm{0, 1, 0};
        ConstructNode helm_node{42, 1, 2, "nc_core:helm", "owner=droopystarfish"};
        require(construct->setNode(helm, helm_node), "new node insert failed");
        require(construct->setNode({-2, 0, 3}, ConstructNode{43, 0, 0, "nc_core:frame", {}}),
            "second node insert failed");
        require(construct->nodeCount() == 2, "node count failed");
        const ConstructNode *node = construct->getNode(helm);
        require(node != nullptr && *node == helm_node, "node lookup failed");

        require(construct->setNodeMetadataField(helm, "owner", "droopystarfish"),
            "construct metadata insert failed");
        const auto *helm_state = construct->getNodeState(helm);
        require(helm_state != nullptr && helm_state->fields.at("owner") == "droopystarfish",
            "construct metadata lookup failed");
        const auto metadata_revision = helm_state->revision;
        require(!construct->setNodeMetadataField(helm, "owner", "droopystarfish"),
            "unchanged metadata modified state");
        require(construct->getNodeState(helm)->revision == metadata_revision,
            "unchanged metadata bumped revision");
        require(construct->setNodeInventory(helm, "main",
            ConstructInventoryList{2, {"nc_core:frame 4", "nc_core:helm 1"}}),
            "construct inventory insert failed");
        require(construct->startNodeTimer(helm, 5.0, 1.25),
            "construct timer start failed");

        DynamicConstruct interaction_construct(650);
        interaction_construct.setNode({0, 0, 0},
            ConstructNode{9, 0, 0, "nc_core:frame", {}});
        ConstructInteractionEngine interaction_engine;
        ConstructInteractionRequest rotated_use_request;
        rotated_use_request.sequence = 1;
        rotated_use_request.construct_id = interaction_construct.id();
        rotated_use_request.action = ConstructInteractionAction::Use;
        rotated_use_request.node_position = {0, 0, 0};
        rotated_use_request.world_normal = {std::sqrt(0.5), 0.0, std::sqrt(0.5)};
        rotated_use_request.client_time = 1.0;
        require(interaction_engine.apply(interaction_construct, rotated_use_request).accepted,
            "yaw-rotated construct interaction normal failed");
        (void)interaction_engine.drainEvents();

        ConstructInteractionRequest dig_request;
        dig_request.sequence = 2;
        dig_request.construct_id = interaction_construct.id();
        dig_request.action = ConstructInteractionAction::DigComplete;
        dig_request.node_position = {0, 0, 0};
        dig_request.adjacent_position = {1, 0, 0};
        dig_request.world_normal = {1.0, 0.0, 0.0};
        dig_request.actor = "droopystarfish";
        dig_request.client_time = 2.0;
        const auto before_dig_payload = ConstructSerialization::encode(interaction_construct);
        const auto dig_result = interaction_engine.apply(interaction_construct, dig_request);
        require(dig_result.accepted && !dig_result.node_changed &&
            dig_result.callback_required, "construct dig transaction routing failed");
        require(interaction_construct.getNode({0, 0, 0}) != nullptr,
            "construct dig mutated before callback resolution");
        require(interaction_engine.pendingMutationCount() == 1,
            "construct dig transaction was not retained");
        auto dig_events = interaction_engine.drainEvents();
        require(dig_events.size() == 1 && dig_events.front().removed_node.has_value(),
            "construct dig callback payload failed");
        ConstructMutationResolution dig_resolution;
        dig_resolution.event_id = dig_events.front().event_id;
        dig_resolution.accepted = true;
        dig_resolution.wielded_item_after = "nc_core:pick 1 250";
        dig_resolution.drops = {"nc_core:frame 1"};
        const auto resolved_dig = interaction_engine.resolveMutationEvent(
            interaction_construct, dig_resolution);
        require(resolved_dig.accepted && resolved_dig.node_changed,
            "construct dig callback resolution failed");
        require(interaction_construct.getNode({0, 0, 0}) == nullptr,
            "construct dig did not remove node after resolution");
        const auto after_dig_payload = ConstructSerialization::encode(interaction_construct);
        auto mutation_records = interaction_engine.drainMutationRecords();
        require(mutation_records.size() == 1 && mutation_records.front().accepted &&
            mutation_records.front().drops == dig_resolution.drops,
            "construct dig mutation record failed");
        const ConstructMutationRecord dig_mutation_record = mutation_records.front();

        interaction_construct.setNode({0, 0, 0},
            ConstructNode{9, 0, 0, "nc_core:frame", {}});
        ConstructInteractionRequest place_request = dig_request;
        place_request.sequence = 3;
        place_request.action = ConstructInteractionAction::Place;
        place_request.adjacent_position = {1, 0, 0};
        place_request.wielded_item = "nc_core:frame 3";
        const auto place_result = interaction_engine.apply(interaction_construct, place_request);
        require(place_result.accepted && !place_result.node_changed &&
            place_result.callback_required,
            "construct place callback routing failed");
        auto place_events = interaction_engine.drainEvents();
        require(place_events.size() == 1 && place_events.front().node_name == "nc_core:frame",
            "construct place callback event failed");
        ConstructMutationResolution place_resolution;
        place_resolution.event_id = place_events.front().event_id;
        place_resolution.accepted = true;
        place_resolution.placed_node = ConstructNode{9, 0, 1, "nc_core:frame", {}};
        place_resolution.wielded_item_after = "nc_core:frame 2";
        const auto resolved_place = interaction_engine.resolveMutationEvent(
            interaction_construct, place_resolution);
        require(resolved_place.accepted && resolved_place.node_changed &&
            interaction_construct.getNode({1, 0, 0}) != nullptr,
            "construct place callback resolution failed");
        mutation_records = interaction_engine.drainMutationRecords();
        require(mutation_records.size() == 1 && mutation_records.front().accepted,
            "construct place mutation record failed");

        require(interaction_construct.startNodeTimer({1, 0, 0}, 0.5),
            "interaction timer start failed");
        auto timer_events = interaction_engine.stepTimers(interaction_construct, 0.5);
        require(timer_events.size() == 1 &&
            timer_events.front().request.action == ConstructInteractionAction::Timer,
            "interaction timer callback failed");
        (void)interaction_engine.drainEvents();
        require(interaction_engine.resolveTimerEvent(interaction_construct,
            timer_events.front().event_id, true), "interaction timer restart failed");
        require(interaction_engine.stepTimers(interaction_construct, 0.5).size() == 1,
            "restarted interaction timer did not fire");
        (void)interaction_engine.drainEvents();

        ConstructInteractionRequest packet_request = place_request;
        packet_request.fields = {{"channel", "one"}, {"message", "ahead full"}};
        const auto interaction_packet = ConstructInteractionPacketCodec::encode(packet_request);
        const auto decoded_interaction = ConstructInteractionPacketCodec::decode(interaction_packet);
        require(decoded_interaction.sequence == packet_request.sequence,
            "construct interaction packet sequence failed");
        require(decoded_interaction.construct_id == packet_request.construct_id,
            "construct interaction packet id failed");
        require(decoded_interaction.action == packet_request.action,
            "construct interaction packet action failed");
        require(decoded_interaction.fields == packet_request.fields,
            "construct interaction packet fields failed");
        std::vector<std::uint8_t> truncated_interaction(
            interaction_packet.begin(), interaction_packet.end() - 1);
        bool interaction_rejected = false;
        try {
            (void)ConstructInteractionPacketCodec::decode(truncated_interaction);
        } catch (const std::runtime_error &) {
            interaction_rejected = true;
        }
        require(interaction_rejected, "truncated interaction packet was not rejected");

        const LocalBounds bounds = construct->localBounds();
        require(bounds.valid, "bounds should be valid");
        require(bounds.min.x == -2 && bounds.min.y == 0 && bounds.min.z == 0,
            "bounds minimum failed");
        require(bounds.max.x == 0 && bounds.max.y == 1 && bounds.max.z == 3,
            "bounds maximum failed");

        construct->step(2.0);
        require(close(construct->transform().position.x, 14.0), "kinematic x failed");
        require(close(construct->transform().position.y, 18.0), "kinematic y failed");
        require(close(construct->transform().position.z, 38.0), "kinematic z failed");
        require(close(construct->transform().yaw_radians, 1.25), "kinematic yaw failed");

        ConstructSectionIndex section_index;
        section_index.rebuild(*construct);
        require(section_index.size() == 2, "construct section partition failed");
        require(ConstructSectionIndex::sectionFor({-1, 0, -17}).x == -1,
            "negative section x floor division failed");
        require(ConstructSectionIndex::sectionFor({-1, 0, -17}).z == -2,
            "negative section z floor division failed");

        ConstructTransformSnapshot older{construct->id(), 10, 1.0,
            {{0.0, 0.0, 0.0}, PI * 1.9}, {10.0, 0.0, 0.0}, 0.2};
        ConstructTransformSnapshot newer{construct->id(), 11, 2.0,
            {{10.0, 0.0, 0.0}, PI * 0.1}, {10.0, 0.0, 0.0}, 0.2};
        const auto interpolated = ConstructReplication::interpolate(older, newer, 1.5);
        require(close(interpolated.position.x, 5.0), "snapshot interpolation position failed");
        require(close(interpolated.yaw_radians, 0.0, 1e-8) ||
            close(interpolated.yaw_radians, PI * 2.0, 1e-8),
            "snapshot interpolation shortest-yaw path failed");
        const auto extrapolated = ConstructReplication::extrapolate(newer, 3.0);
        require(close(extrapolated.position.x, 12.5), "snapshot extrapolation cap failed");

        ConstructSnapshotBuffer snapshot_buffer(3);
        require(snapshot_buffer.push(older), "snapshot buffer rejected first snapshot");
        require(snapshot_buffer.push(newer), "snapshot buffer rejected newer snapshot");
        require(!snapshot_buffer.push(older), "snapshot buffer accepted stale snapshot");
        const auto buffered = snapshot_buffer.sample(1.6, 0.1);
        require(close(buffered.position.x, 5.0), "snapshot buffer interpolation failed");
        const auto buffered_extrapolated = snapshot_buffer.sample(3.0, 0.0, 0.25);
        require(close(buffered_extrapolated.position.x, 12.5),
            "snapshot buffer extrapolation failed");

        DynamicConstruct mesh_construct(700);
        mesh_construct.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "steel", {}});
        mesh_construct.setNode({1, 0, 0}, ConstructNode{1, 0, 0, "steel", {}});
        ConstructSectionIndex mesh_sections;
        mesh_sections.rebuild(mesh_construct);
        const auto initial_dirty = mesh_sections.consumeDirty();
        require(initial_dirty.size() == 1, "section rebuild dirty tracking failed");
        const ConstructSection *mesh_section = mesh_sections.find({0, 0, 0});
        require(mesh_section != nullptr, "mesh section lookup failed");
        const auto section_mesh = ConstructMesher::buildSection(mesh_construct, *mesh_section);
        require(section_mesh.visible_faces == 10, "two-block exposed-face meshing failed");
        require(section_mesh.buffers.size() == 1, "mesh material batching failed");
        require(section_mesh.buffers.front().vertices.size() == 40,
            "mesh vertex count failed");
        require(section_mesh.buffers.front().indices.size() == 60,
            "mesh index count failed");

        ConstructNode boundary_node{2, 0, 0, "glass", {}};
        mesh_sections.updateNode({15, 0, 0}, &boundary_node);
        const auto boundary_dirty = mesh_sections.consumeDirty();
        require(boundary_dirty.size() >= 2, "boundary edit did not dirty neighbour section");
        mesh_sections.updateNode({15, 0, 0}, nullptr);
        require(!mesh_sections.consumeDirty().empty(), "section removal did not mark dirty");

        DynamicConstruct collision_construct(701);
        collision_construct.setTransform({{10.0, 0.0, 10.0}, PI / 4.0});
        collision_construct.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "steel", {}});
        const Aabb3d construct_bounds = ConstructGeometry::worldBounds(collision_construct);
        require(construct_bounds.valid(), "rotated construct world bounds invalid");
        require(ConstructGeometry::intersectsWorldAabb(collision_construct,
            {{9.8, 0.1, 10.2}, {10.2, 0.9, 10.6}}),
            "yaw-aware construct collision missed overlap");
        require(!ConstructGeometry::intersectsWorldAabb(collision_construct,
            {{20.0, 0.0, 20.0}, {21.0, 1.0, 21.0}}),
            "construct collision false positive");

        DynamicConstruct far_construct(704);
        far_construct.setTransform({{100.0, 0.0, 100.0}, 0.0});
        far_construct.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "far", {}});
        DynamicConstruct incoming_construct(705);
        incoming_construct.setTransform({{5.0, 0.0, 0.0}, 0.0});
        incoming_construct.setLinearVelocity({-20.0, 0.0, 0.0});
        incoming_construct.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "incoming", {}});
        ConstructCollisionWorld collision_world;
        collision_world.rebuild({&collision_construct, &far_construct,
            &incoming_construct}, 0.25);
        const auto broadphase_near = collision_world.query(
            {{-0.5, 0.0, -0.5}, {1.5, 2.0, 1.5}}, {}, 0.25);
        require(std::find(broadphase_near.begin(), broadphase_near.end(),
            &incoming_construct) != broadphase_near.end(),
            "collision broad-phase rejected an incoming moving construct");
        require(std::find(broadphase_near.begin(), broadphase_near.end(),
            &far_construct) == broadphase_near.end(),
            "collision broad-phase retained a distant construct");
        const auto broadphase_forced = collision_world.query(
            {{-0.5, 0.0, -0.5}, {1.5, 2.0, 1.5}}, {}, 0.0,
            far_construct.id());
        require(std::find(broadphase_forced.begin(), broadphase_forced.end(),
            &far_construct) != broadphase_forced.end(),
            "collision broad-phase dropped the active contact construct");

        const auto ray_hit = ConstructGeometry::raycast(collision_construct,
            {10.0, 0.5, 8.0}, {10.0, 0.5, 12.0});
        require(ray_hit.has_value(), "construct-local DDA raycast missed node");
        require(ray_hit->construct_id == collision_construct.id(), "raycast id failed");
        require(ray_hit->node_position == LocalNodePos{0, 0, 0}, "raycast node position failed");

        const Aabb3d moving_box{{8.0, 0.1, 10.2}, {8.5, 0.9, 10.7}};
        const SweepResult sweep = ConstructGeometry::sweepWorldAabb(
            collision_construct, moving_box, {4.0, 0.0, 0.0});
        require(sweep.collided, "swept collision failed to stop moving box");
        require(sweep.allowed_fraction > 0.0 && sweep.allowed_fraction < 1.0,
            "swept collision fraction invalid");

        DynamicConstruct platform_construct(702);
        platform_construct.setTransform({{0.0, 0.0, 0.0}, 0.0});
        platform_construct.setLinearVelocity({2.0, 0.0, 0.0});
        platform_construct.setYawVelocity(PI / 2.0);
        PlatformContactState contact = PlatformContact::begin(platform_construct, {1.0, 1.0, 0.0});
        platform_construct.step(0.5);
        const PlatformMotion platform_motion = PlatformContact::update(contact, platform_construct, 0.5);
        require(platform_motion.displacement.x > 0.6, "platform translation inheritance failed");
        require(platform_motion.displacement.z > 0.6, "platform yaw inheritance failed");
        require(close(platform_motion.yaw_delta, PI / 4.0), "platform yaw delta failed");

        DynamicConstruct deck_construct(703);
        deck_construct.setTransform({{0.0, 0.0, 0.0}, 0.0});
        deck_construct.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "deck", {}});
        const Aabb3d rider_box{{0.2, 1.05, 0.2}, {0.8, 2.80, 0.8}};
        const auto support = ConstructGeometry::findSupport(deck_construct, rider_box, 0.2, 0.1);
        require(support.has_value(), "construct support query missed deck");
        require(support->node_position == LocalNodePos{0, 0, 0},
            "construct support node failed");
        require(close(support->vertical_gap, 0.05), "construct support gap failed");

        deck_construct.setNode({1, 0, 0}, ConstructNode{1, 0, 0, "deck", {}});
        const Aabb3d seam_box{{0.70, 1.01, 0.2}, {1.30, 2.76, 0.8}};
        const auto preferred_support = ConstructGeometry::findSupport(
            deck_construct, seam_box, 0.18, 0.10, LocalNodePos{1, 0, 0}, 0.05);
        require(preferred_support &&
            preferred_support->node_position == LocalNodePos{1, 0, 0},
            "preferred support-node hysteresis failed at deck seam");

        RiderMotionState rider_state;
        std::vector<const DynamicConstruct *> rider_constructs{&deck_construct};
        RiderMotionInput rider_input;
        rider_input.position = {0.5, 1.05, 0.5};
        rider_input.velocity = {0.0, -0.1, 0.0};
        rider_input.world_box = rider_box;
        rider_input.delta_seconds = 0.1;
        const auto landed = RiderMotionSolver::step(rider_state, rider_constructs, rider_input);
        require(landed.grounded && landed.contact_started,
            "rider did not acquire deck contact");
        require(close(landed.position.y, 1.0), "rider landing snap failed");

        RiderMotionState phased_rider_state;
        RiderMotionInput phased_input = rider_input;
        phased_input.allow_contact_acquisition = false;
        const auto prephysics_phase = RiderMotionSolver::step(
            phased_rider_state, rider_constructs, phased_input);
        require(!prephysics_phase.grounded && !phased_rider_state.contact.active,
            "pre-physics rider phase acquired a new contact");
        phased_input.allow_contact_acquisition = true;
        phased_input.delta_seconds = 0.0;
        const auto postphysics_phase = RiderMotionSolver::step(
            phased_rider_state, rider_constructs, phased_input);
        require(postphysics_phase.grounded && phased_rider_state.contact.active,
            "post-physics rider phase failed to acquire support");
        PlatformContact::clear(phased_rider_state.contact);
        phased_rider_state.previous_jump_pressed = true;
        phased_input.jump_pressed = true;
        const auto held_jump_phase = RiderMotionSolver::step(
            phased_rider_state, rider_constructs, phased_input);
        require(!held_jump_phase.grounded && !phased_rider_state.contact.active,
            "held jump reacquired moving-deck contact");

        deck_construct.setLinearVelocity({2.0, 0.0, 0.0});
        deck_construct.step(0.5);
        rider_input.position = landed.position;
        rider_input.world_box = rider_box.translated({0.0, -0.05, 0.0});
        rider_input.velocity = {0.0, 0.0, 0.0};
        rider_input.delta_seconds = 0.5;
        const auto carried = RiderMotionSolver::step(rider_state, rider_constructs, rider_input);
        require(carried.grounded, "rider lost moving deck contact");
        require(close(carried.platform_displacement.x, 1.0),
            "rider platform displacement failed");

        rider_input.position = carried.position;
        rider_input.world_box = rider_input.world_box.translated(carried.platform_displacement);
        rider_input.jump_pressed = true;
        rider_input.delta_seconds = 0.1;
        const auto jumped = RiderMotionSolver::step(rider_state, rider_constructs, rider_input);
        require(jumped.jumped && jumped.contact_ended,
            "rider jump did not detach contact");
        require(jumped.inherited_velocity.x > 1.5,
            "rider jump did not inherit platform velocity");

        std::vector<const DynamicConstruct *> no_hull_constructs;
        HullCollisionInput pass_through_input;
        pass_through_input.world_box = {{0.2, 2.0, 0.2}, {0.8, 3.75, 0.8}};
        pass_through_input.velocity = {0.0, 3.0, 0.0};
        pass_through_input.desired_delta = {0.0, 0.4, 0.0};
        pass_through_input.delta_seconds = 0.1;
        const auto pass_through_collision = MovingHullCollisionSolver::move(
            no_hull_constructs, pass_through_input);
        require(!pass_through_collision.collided &&
                !pass_through_collision.touching_ground &&
                pass_through_collision.contacts.empty(),
            "empty moving-hull pass-through reported a contact");
        require(close(pass_through_collision.allowed_delta.y,
                pass_through_input.desired_delta.y) &&
                close(pass_through_collision.velocity.y,
                    pass_through_input.velocity.y),
            "empty moving-hull pass-through changed vertical motion");

        DynamicConstruct wall_construct(710);
        wall_construct.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "wall", {}});
        std::vector<const DynamicConstruct *> wall_constructs{&wall_construct};

        HullCollisionInput wall_input;
        wall_input.world_box = {{-1.6, 0.1, 0.2}, {-1.0, 0.9, 0.8}};
        wall_input.velocity = {3.0, 0.0, 0.0};
        wall_input.desired_delta = {1.5, 0.0, 0.0};
        wall_input.delta_seconds = 0.5;
        const auto wall_collision = MovingHullCollisionSolver::move(
            wall_constructs, wall_input);
        require(wall_collision.collided && wall_collision.hit_wall,
            "moving hull wall collision failed");
        require(wall_collision.allowed_delta.x > 0.95 &&
            wall_collision.allowed_delta.x < 1.02,
            "moving hull wall travel fraction failed");
        require(!wall_collision.contacts.empty() &&
            wall_collision.contacts.front().world_normal.x < -0.9,
            "moving hull wall normal failed");
        require(std::abs(wall_collision.velocity.x) < 1e-6,
            "moving hull wall velocity response failed");

        HullCollisionInput floor_input;
        floor_input.world_box = {{0.2, 1.2, 0.2}, {0.8, 2.95, 0.8}};
        floor_input.velocity = {0.0, -2.0, 0.0};
        floor_input.desired_delta = {0.0, -0.5, 0.0};
        floor_input.delta_seconds = 0.25;
        const auto floor_collision = MovingHullCollisionSolver::move(
            wall_constructs, floor_input);
        require(floor_collision.touching_ground,
            "moving hull floor contact failed");
        require(floor_collision.contacts.front().world_normal.y > 0.9,
            "moving hull floor normal failed");

        DynamicConstruct ceiling_construct(711);
        ceiling_construct.setNode({0, 2, 0}, ConstructNode{1, 0, 0, "ceiling", {}});
        std::vector<const DynamicConstruct *> ceiling_constructs{&ceiling_construct};
        HullCollisionInput ceiling_input;
        ceiling_input.world_box = {{0.2, 0.0, 0.2}, {0.8, 1.75, 0.8}};
        ceiling_input.velocity = {0.0, 3.0, 0.0};
        ceiling_input.desired_delta = {0.0, 0.8, 0.0};
        ceiling_input.delta_seconds = 0.25;
        const auto ceiling_collision = MovingHullCollisionSolver::move(
            ceiling_constructs, ceiling_input);
        require(ceiling_collision.hit_ceiling,
            "moving hull ceiling contact failed");
        require(ceiling_collision.contacts.front().world_normal.y < -0.9,
            "moving hull ceiling normal failed");

        DynamicConstruct rotated_wall(712);
        rotated_wall.setTransform({{0.0, 0.0, 0.0}, PI / 4.0});
        rotated_wall.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "rotated_wall", {}});
        const auto rotated_overlap = ConstructGeometry::overlapWorldAabb(
            rotated_wall, {{-0.25, 0.1, 0.35}, {0.25, 0.9, 0.85}});
        require(rotated_overlap.has_value(), "rotated hull overlap failed");
        require(std::abs(rotated_overlap->world_normal.x) > 0.4 ||
            std::abs(rotated_overlap->world_normal.z) > 0.4,
            "rotated hull collision normal failed");

        CarriedBodyManager body_manager;
        DynamicConstruct body_deck(713);
        body_deck.setNode({0, 0, 0}, ConstructNode{1, 0, 0, "deck", {}});
        std::vector<const DynamicConstruct *> body_constructs{&body_deck};
        CarriedBodyInput body_input;
        body_input.id = 99;
        body_input.kind = CarriedBodyKind::DroppedItem;
        body_input.position = {0.5, 1.05, 0.5};
        body_input.velocity = {0.0, -0.1, 0.0};
        body_input.world_box = {{0.35, 1.05, 0.35}, {0.65, 1.35, 0.65}};
        body_input.delta_seconds = 0.1;
        const auto body_landed = body_manager.step(body_constructs, body_input);
        require(body_landed.grounded && body_landed.construct_id == body_deck.id(),
            "dropped item did not acquire construct contact");
        body_deck.setLinearVelocity({1.0, 0.0, 0.0});
        body_deck.step(0.5);
        body_input.position = body_landed.position;
        body_input.velocity = body_landed.velocity;
        body_input.world_box = body_input.world_box.translated(
            body_landed.position - Vec3d{0.5, 1.05, 0.5});
        body_input.delta_seconds = 0.5;
        const auto body_carried = body_manager.step(body_constructs, body_input);
        require(body_carried.position.x > body_landed.position.x + 0.45,
            "dropped item did not inherit construct movement");
        require(body_manager.size() == 1, "carried body state tracking failed");

        const RiderCorrection soft_correction = RiderReconciler::reconcile(
            {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 0.25, 3.0, 0.5);
        require(soft_correction.corrected && !soft_correction.hard_snap,
            "soft rider reconciliation failed");
        require(close(soft_correction.position.x, 0.5),
            "soft rider correction amount failed");
        const RiderCorrection hard_correction = RiderReconciler::reconcile(
            {0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}, 0.25, 3.0, 0.5);
        require(hard_correction.hard_snap && close(hard_correction.position.x, 4.0),
            "hard rider reconciliation failed");

        RiderStatePacket rider_packet;
        rider_packet.sequence = 17;
        rider_packet.construct_id = deck_construct.id();
        rider_packet.local_anchor = {0.5, 1.0, 0.5};
        rider_packet.local_velocity = {0.75, 0.0, -0.25};
        rider_packet.world_position = {1.5, 1.0, 0.5};
        rider_packet.velocity = {2.0, 0.0, 0.0};
        rider_packet.client_time = 9.5;
        rider_packet.grounded = true;
        const auto rider_bytes = RiderPacketCodec::encode(rider_packet);
        const auto decoded_rider = RiderPacketCodec::decode(rider_bytes);
        require(decoded_rider.sequence == rider_packet.sequence &&
            decoded_rider.construct_id == rider_packet.construct_id,
            "rider packet identity failed");
        require(close(decoded_rider.local_anchor.y, 1.0),
            "rider packet anchor failed");
        require(close(decoded_rider.local_velocity.x, 0.75) &&
            close(decoded_rider.local_velocity.z, -0.25),
            "rider packet local-velocity prediction failed");

        const auto transform_packet = ConstructPacketCodec::encodeTransform(newer);
        const auto decoded_transform_packet = ConstructPacketCodec::decodeTransform(transform_packet);
        require(decoded_transform_packet.id == newer.id &&
            decoded_transform_packet.sequence == newer.sequence,
            "transform packet identity failed");
        require(close(decoded_transform_packet.transform.position.x, 10.0),
            "transform packet position failed");

        ConstructId decoded_section_id = 0;
        const auto section_packet = ConstructPacketCodec::encodeSection(
            mesh_construct.id(), *mesh_section);
        const auto decoded_section = ConstructPacketCodec::decodeSection(
            section_packet, decoded_section_id);
        require(decoded_section_id == mesh_construct.id(), "section packet id failed");
        require(decoded_section.nodes.size() == mesh_section->nodes.size(),
            "section packet node count failed");
        std::vector<std::uint8_t> bad_packet(transform_packet.begin(), transform_packet.end() - 2);
        bool bad_packet_rejected = false;
        try {
            (void)ConstructPacketCodec::decodeTransform(bad_packet);
        } catch (const std::runtime_error &) {
            bad_packet_rejected = true;
        }
        require(bad_packet_rejected, "truncated transform packet was not rejected");

        ClientConstructManager client_manager;
        ConstructTransformSnapshot mesh_snapshot{mesh_construct.id(), 1, 2.0,
            {{0.0, 0.0, 0.0}, 0.0}, {0.0, 0.0, 0.0}, 0.0};
        const auto mesh_transform_packet = ConstructPacketCodec::encodeTransform(mesh_snapshot);
        require(client_manager.applyTransformPacket(mesh_transform_packet, 12.05),
            "client manager rejected transform packet");
        require(client_manager.applySectionPacket(section_packet),
            "client manager rejected section packet");
        const auto client_state = client_manager.find(mesh_construct.id());
        require(client_state != nullptr, "client construct state missing");
        require(client_state->sectionCount() == 1, "client section count failed");
        const auto rebuilt_meshes = client_state->rebuildDirtyMeshes();
        require(!rebuilt_meshes.empty(), "client dirty mesh rebuild did no work");
        require(client_state->meshCount() == 1, "client mesh cache count failed");
        const auto *client_mesh = client_state->mesh({0, 0, 0});
        require(client_mesh != nullptr && client_mesh->visible_faces == 10,
            "client packet-to-mesh pipeline failed");
        const auto sampled_client_transform = client_state->sampleTransform(2.0, 0.0);
        require(close(sampled_client_transform.position.x, 0.0),
            "client sampled transform failed");
        const auto client_hit = client_manager.raycast({-2.0, 0.5, 0.5}, {4.0, 0.5, 0.5});
        require(client_hit.has_value(), "client manager raycast failed");

        const auto remove_packet = ConstructPacketCodec::encodeRemove(mesh_construct.id());
        require(ConstructPacketCodec::decodeRemove(remove_packet) == mesh_construct.id(),
            "remove packet identity failed");
        require(client_manager.applyRemovePacket(remove_packet),
            "client manager remove packet failed");
        require(client_manager.find(mesh_construct.id()) == nullptr,
            "client construct survived remove packet");

        ConstructReplicationQueue replication_queue;
        replication_queue.queueFullConstruct(mesh_construct, mesh_sections, 4.0);
        require(replication_queue.size() >= 2,
            "full construct replication did not queue transform and section");
        auto first_wire = replication_queue.pop();
        require(first_wire.kind == ConstructWireKind::Transform && !first_wire.reliable,
            "first full replication message was not an unreliable transform");
        replication_queue.queueTransform(mesh_construct, 4.1);
        const auto queued_transform = replication_queue.pop();
        require(queued_transform.kind == ConstructWireKind::Section ||
            queued_transform.kind == ConstructWireKind::Transform,
            "replication queue returned unexpected kind");
        while (!replication_queue.empty())
            (void)replication_queue.pop();
        replication_queue.queueRemove(mesh_construct.id());
        require(replication_queue.pop().reliable, "remove replication must be reliable");
        replication_queue.queueReset();
        require(replication_queue.pop().kind == ConstructWireKind::Reset,
            "reset replication kind failed");

        require(construct->setLiquid({4, 0, 0}, {"water", 6, 1}),
            "construct persistent liquid setup failed");
        const auto encoded = ConstructSerialization::encode(*construct);
        const auto decoded = ConstructSerialization::decode(encoded);
        require(decoded->id() == construct->id(), "serialized id failed");
        require(decoded->owner() == construct->owner(), "serialized owner failed");
        require(decoded->nodeCount() == construct->nodeCount(), "serialized node count failed");
        require(decoded->getNode(helm) != nullptr && *decoded->getNode(helm) == helm_node,
            "serialized node failed");
        require(close(decoded->transform().position.z, 38.0), "serialized transform failed");
        require(close(decoded->linearVelocity().x, 2.0), "serialized velocity failed");
        const auto *decoded_state = decoded->getNodeState(helm);
        require(decoded_state != nullptr &&
            decoded_state->fields.at("owner") == "droopystarfish",
            "serialized construct metadata failed");
        require(decoded_state->inventories.at("main") ==
            ConstructInventoryList{2, {"nc_core:frame 4", "nc_core:helm 1"}},
            "serialized construct inventory failed");
        require(decoded_state->timer.active && close(decoded_state->timer.timeout, 5.0) &&
            close(decoded_state->timer.elapsed, 1.25),
            "serialized construct timer failed");
        const auto *decoded_liquid = decoded->getLiquid({4, 0, 0});
        require(decoded_liquid && decoded_liquid->liquid_name == "water" &&
            decoded_liquid->level == 6 && decoded_liquid->flags == 1,
            "serialized construct liquid failed");

        #if NAVYCRAFT_HAS_SQLITE
        const std::string database_path =
            (std::filesystem::temp_directory_path() / "navycraft-m4f-test.sqlite").string();
        std::remove(database_path.c_str());
        std::remove((database_path + "-wal").c_str());
        std::remove((database_path + "-shm").c_str());
        {
            ConstructDatabase database(database_path);
            database.saveConstruct(*construct);
            require(database.constructCount() == 1,
                "construct database save count failed");
            const auto loaded_construct = database.loadConstruct(construct->id());
            require(loaded_construct && loaded_construct->owner() == construct->owner(),
                "construct database load failed");
            const auto action_id = database.recordMutation(dig_mutation_record,
                before_dig_payload, after_dig_payload);
            require(action_id > 0 && database.actionCount() == 1,
                "construct database action journal failed");
            const auto actions = database.recentActions(
                dig_mutation_record.construct_id, 10);
            require(actions.size() == 1 && actions.front().action_id == action_id &&
                actions.front().mutation.drops == dig_mutation_record.drops,
                "construct database action query failed");
            const auto rollback_construct = database.rollbackPayload(action_id);
            require(rollback_construct &&
                rollback_construct->getNode(dig_request.node_position) != nullptr,
                "construct database rollback payload failed");
            database.flush();
        }
        std::remove(database_path.c_str());
        std::remove((database_path + "-wal").c_str());
        std::remove((database_path + "-shm").c_str());
        #endif

        std::vector<std::uint8_t> truncated(encoded.begin(), encoded.end() - 1);
        bool rejected = false;
        try {
            (void)ConstructSerialization::decode(truncated);
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        require(rejected, "truncated serialization was not rejected");

        auto imported = registry.createWithId(500);
        require(imported != nullptr && imported->id() == 500, "createWithId failed");
        require(registry.create()->id() > 500, "next id did not advance after import");

        const ConstructId id = construct->id();
        require(registry.find(id) == construct, "registry find failed");
        require(registry.remove(id), "registry remove failed");
        require(registry.find(id) == nullptr, "removed construct still found");

        std::cout << "NavyCraft construct core tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

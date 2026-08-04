#include "construct_geometry.h"
#include "construct_collision_world.h"
#include "construct_mesh.h"
#include "construct_packets.h"
#include "construct_section.h"
#include "construct_navigation.h"
#include "construct_structure.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

using namespace navycraft;

int main()
{
    DynamicConstruct construct(9000);
    construct.setTransform({{100.0, 20.0, 100.0}, 0.35});
    const auto build_start = std::chrono::steady_clock::now();
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < 25; ++z)
            for (int x = 0; x < 20; ++x)
                construct.setNode({x, y, z}, ConstructNode{1, 0, 0, "benchmark:hull", {}});
    ConstructSectionIndex sections;
    sections.rebuild(construct);
    const auto partition_end = std::chrono::steady_clock::now();

    std::size_t faces = 0;
    std::size_t packet_bytes = 0;
    for (const auto &position : sections.positions()) {
        const auto *section = sections.find(position);
        if (!section)
            return EXIT_FAILURE;
        const auto mesh = ConstructMesher::buildSection(construct, *section);
        faces += mesh.visible_faces;
        packet_bytes += ConstructPacketCodec::encodeSection(construct.id(), *section).size();
    }
    const auto mesh_end = std::chrono::steady_clock::now();

    std::size_t ray_hits = 0;
    for (int index = 0; index < 10000; ++index) {
        const double z = 100.0 + static_cast<double>(index % 25);
        if (ConstructGeometry::raycast(construct, {80.0, 25.0, z}, {140.0, 25.0, z}))
            ++ray_hits;
    }
    const auto ray_end = std::chrono::steady_clock::now();

    // Gate 5 broad-phase benchmark: one fast incoming construct, one forced
    // support construct and a large set of distant vessels. Querying the
    // player sweep must retain the relevant vessels without scanning their
    // node geometry.
    std::vector<std::unique_ptr<DynamicConstruct>> broadphase_storage;
    std::vector<const DynamicConstruct *> broadphase_constructs;
    broadphase_storage.reserve(514);
    broadphase_constructs.reserve(514);
    for (int index = 0; index < 512; ++index) {
        auto vessel = std::make_unique<DynamicConstruct>(10000 + index);
        vessel->setTransform({{1000.0 + static_cast<double>(index % 32) * 40.0,
            static_cast<double>(index % 5) * 3.0,
            1000.0 + static_cast<double>(index / 32) * 40.0}, 0.0});
        vessel->setNode({0, 0, 0}, ConstructNode{1, 0, 0, "benchmark:hull", {}});
        broadphase_constructs.push_back(vessel.get());
        broadphase_storage.push_back(std::move(vessel));
    }
    auto incoming = std::make_unique<DynamicConstruct>(20001);
    incoming->setTransform({{18.0, 0.0, 0.0}, 0.0});
    incoming->setLinearVelocity({-72.0, 0.0, 0.0});
    incoming->setNode({0, 0, 0}, ConstructNode{1, 0, 0, "benchmark:hull", {}});
    const ConstructId incoming_id = incoming->id();
    broadphase_constructs.push_back(incoming.get());
    broadphase_storage.push_back(std::move(incoming));

    auto support = std::make_unique<DynamicConstruct>(20002);
    support->setTransform({{250.0, 0.0, 250.0}, 0.0});
    support->setNode({0, 0, 0}, ConstructNode{1, 0, 0, "benchmark:hull", {}});
    const ConstructId support_id = support->id();
    broadphase_constructs.push_back(support.get());
    broadphase_storage.push_back(std::move(support));

    ConstructCollisionWorld collision_world;
    const auto broadphase_start = std::chrono::steady_clock::now();
    collision_world.rebuild(broadphase_constructs, 0.25);
    std::size_t broadphase_candidates = 0;
    bool incoming_seen = false;
    bool support_seen = false;
    constexpr int broadphase_queries = 20000;
    const Aabb3d player_box{{-0.3, 0.0, -0.3}, {0.3, 1.8, 0.3}};
    for (int index = 0; index < broadphase_queries; ++index) {
        const bool force_support = (index & 1) != 0;
        const auto candidates = collision_world.query(
            player_box, {0.4, 0.0, 0.0}, 0.05,
            force_support ? support_id : 0);
        broadphase_candidates += candidates.size();
        for (const DynamicConstruct *candidate : candidates) {
            incoming_seen = incoming_seen || candidate->id() == incoming_id;
            support_seen = support_seen || candidate->id() == support_id;
        }
    }
    const auto broadphase_end = std::chrono::steady_clock::now();

    ConstructRegistry navigation_registry;
    ConstructNavigationEngine navigation(navigation_registry);
    for (int index = 0; index < 48; ++index) {
        auto vessel = navigation_registry.create();
        vessel->setTransform({{static_cast<double>((index % 8) * 40), 0.0,
            static_cast<double>((index / 8) * 40)}, 0.0});
        vessel->setNode({0, 0, 0}, ConstructNode{1, 0, 0, "benchmark:hull", {}});
        ConstructNavigationConfig config;
        config.construct_id = vessel->id();
        config.mode = ConstructNavigationMode::Route;
        config.maximum_speed = 8.0;
        config.stuck_timeout = 0.0;
        if (!navigation.configure(config) || !navigation.setRoute(vessel->id(), {
                {{vessel->transform().position.x, 0.0,
                    vessel->transform().position.z + 500.0}, 2.0, 8.0, false},
            }, false))
            return EXIT_FAILURE;
    }
    std::size_t navigation_commands = 0;
    for (int step = 0; step < 200; ++step)
        navigation_commands += navigation.step(0.05, step * 0.05).size();
    const auto navigation_end = std::chrono::steady_clock::now();

    ConstructRegistry structure_registry;
    auto structural_vessel = structure_registry.create();
    structural_vessel->setNode({0, 0, 0},
        ConstructNode{1, 0, 0, "benchmark:helm", {}});
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < 25; ++z)
            for (int x = 0; x < 20; ++x)
                if (!(x == 0 && y == 0 && z == 0))
                    structural_vessel->setNode({x, y, z},
                        ConstructNode{1, 0, 0, "benchmark:hull", {}});
    ConstructStructureEngine structure(structure_registry);
    ConstructStructuralConfig structure_config;
    structure_config.maximum_enclosure_cells = 100000;
    (void)structure.step(0.0, structure_config);
    for (int y = 0; y < 10; ++y)
        for (int z = 0; z < 25; ++z)
            structural_vessel->removeNode({10, y, z});
    const auto structure_events = structure.step(0.05, structure_config);
    std::size_t structural_fragments = 0;
    for (const auto &event : structure_events)
        if (event.kind == ConstructStructuralEventKind::Split)
            structural_fragments += event.fragment_ids.size();
    const auto structure_end = std::chrono::steady_clock::now();

    const auto partition_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        partition_end - build_start).count();
    const auto mesh_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        mesh_end - partition_end).count();
    const auto ray_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        ray_end - mesh_end).count();
    const auto broadphase_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        broadphase_end - broadphase_start).count();
    const auto navigation_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        navigation_end - broadphase_end).count();
    const auto structure_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        structure_end - navigation_end).count();

    std::cout << "nodes=" << construct.nodeCount()
              << " sections=" << sections.size()
              << " visible_faces=" << faces
              << " section_packet_bytes=" << packet_bytes
              << " build_partition_ms=" << partition_ms
              << " mesh_packet_ms=" << mesh_ms
              << " raycasts=10000"
              << " ray_hits=" << ray_hits
              << " raycast_ms=" << ray_ms
              << " broadphase_constructs=" << collision_world.stats().construct_count
              << " broadphase_queries=" << broadphase_queries
              << " broadphase_candidates=" << broadphase_candidates
              << " broadphase_ms=" << broadphase_ms
              << " navigation_vessels=48"
              << " navigation_steps=200"
              << " navigation_commands=" << navigation_commands
              << " navigation_ms=" << navigation_ms
              << " structural_nodes=5000"
              << " structural_fragments=" << structural_fragments
              << " structural_ms=" << structure_ms << '\n';
    return construct.nodeCount() == 5000 && ray_hits > 0 &&
        collision_world.stats().construct_count == 514 && incoming_seen &&
        support_seen && broadphase_candidates <= 30000 &&
        navigation_commands == 48U * 200U && structural_fragments == 1 ?
        EXIT_SUCCESS : EXIT_FAILURE;
}

#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
PATCHER = PROJECT / "scripts" / "apply-engine-overlay.py"

def assert_applied_overlay(root: Path) -> None:
    cmake = (root / "src" / "CMakeLists.txt").read_text(encoding="utf-8")
    main_cpp = (root / "src" / "main.cpp").read_text(encoding="utf-8")
    scripting = (root / "src" / "script" / "scripting_server.cpp").read_text(encoding="utf-8")
    protocol = (root / "src" / "network" / "networkprotocol.h").read_text(encoding="utf-8")
    client_opcodes = (root / "src" / "network" / "clientopcodes.cpp").read_text(encoding="utf-8")
    server_opcodes = (root / "src" / "network" / "serveropcodes.cpp").read_text(encoding="utf-8")
    client_h = (root / "src" / "client" / "client.h").read_text(encoding="utf-8")
    client_cpp = (root / "src" / "client" / "client.cpp").read_text(encoding="utf-8")
    localplayer_cpp = (root / "src" / "client" / "localplayer.cpp").read_text(encoding="utf-8")
    server_h = (root / "src" / "server.h").read_text(encoding="utf-8")
    server_packets = (root / "src" / "network" / "serverpackethandler.cpp").read_text(encoding="utf-8")
    server_cpp = (root / "src" / "server.cpp").read_text(encoding="utf-8")
    mapblock_mesh_h = (root / "src" / "client" / "mapblock_mesh.h").read_text(encoding="utf-8")
    mapblock_mesh_cpp = (root / "src" / "client" / "mapblock_mesh.cpp").read_text(encoding="utf-8")

    assert cmake.count("add_subdirectory(navycraft)") == 1
    assert cmake.count("${navycraft_SRCS}") == 1
    assert cmake.count("${navycraft_client_SRCS}") == 1
    assert main_cpp.count('#include "navycraft/construct/construct_handshake.h"') == 1
    assert main_cpp.count('"navycraft-version"') == 2
    assert main_cpp.count('"navycraft-protocol"') == 2
    assert main_cpp.count("makeServerConstructIdentity") == 1
    assert main_cpp.count("ConstructHandshakeCodec::CURRENT_PROTOCOL") == 1
    assert scripting.count('#include "navycraft/script_api.h"') == 1
    assert scripting.count("ModApiNavyCraft::Initialize(L, top);") == 1
    assert protocol.count("TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION") == 1
    assert protocol.count("TOSERVER_NAVYCRAFT_RIDER_STATE") == 1
    assert protocol.count("TOSERVER_NAVYCRAFT_INTERACTION") == 1
    assert "TOCLIENT_NAVYCRAFT_HANDSHAKE = 0x6C" in protocol
    assert "TOCLIENT_NUM_MSG_TYPES = 0x6D" in protocol
    assert protocol.count("TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION") == 1
    assert protocol.count("TOSERVER_NAVYCRAFT_HANDSHAKE") == 1
    assert "TOSERVER_NUM_MSG_TYPES = 0x57" in protocol
    assert client_opcodes.count("handleCommand_NavyCraftConstructTransform") == 1
    assert client_opcodes.count("handleCommand_NavyCraftConstructEffect") == 1
    assert client_opcodes.count("handleCommand_NavyCraftConstructProjectile") == 1
    assert client_opcodes.count("handleCommand_NavyCraftConstructArticulation") == 1
    assert client_opcodes.count("TOSERVER_NAVYCRAFT_RIDER_STATE") == 1
    assert client_opcodes.count("TOSERVER_NAVYCRAFT_INTERACTION") == 1
    assert client_opcodes.count("handleCommand_NavyCraftHandshake") == 1
    assert client_opcodes.count("TOSERVER_NAVYCRAFT_HANDSHAKE") == 1
    assert server_opcodes.count("handleCommand_NavyCraftRiderState") == 1
    assert server_opcodes.count("handleCommand_NavyCraftInteraction") == 1
    assert server_opcodes.count("handleCommand_NavyCraftHandshake") == 1
    assert server_opcodes.count("TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM") == 1
    assert server_opcodes.count("TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT") == 1
    assert server_opcodes.count("TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE") == 1
    assert server_opcodes.count("TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION") == 1
    assert server_opcodes.count("TOCLIENT_NAVYCRAFT_HANDSHAKE") == 1
    assert client_h.count("ClientConstructScene") == 2
    assert client_h.count("ClientConstructEffects") == 2
    assert client_h.count("beginNavyCraftLocalPlayerMove") == 1
    assert client_h.count("finishNavyCraftLocalPlayerMove") == 1
    assert "prepareNavyCraftLocalPlayerForPhysics" not in client_h
    assert client_h.count("sendNavyCraftInteraction") == 1
    assert client_h.count("sendNavyCraftHandshake") == 1
    assert client_h.count("handleCommand_NavyCraftHandshake") == 1
    assert client_h.count("m_navycraft_handshake_accepted") == 1
    assert client_cpp.count("stepNavyCraftConstructScene(dtime)") == 1
    assert client_cpp.count("sendNavyCraftInteraction(action, pointed)") == 1
    assert "prepareNavyCraftLocalPlayerForPhysics" not in client_cpp
    assert localplayer_cpp.count("beginNavyCraftLocalPlayerMove") == 1
    assert localplayer_cpp.count("finishNavyCraftLocalPlayerMove") == 1
    begin_index = localplayer_cpp.index("beginNavyCraftLocalPlayerMove")
    stock_collision_index = localplayer_cpp.index("collisionMoveSimple", begin_index)
    sneak_index = localplayer_cpp.index(
        "new_sneak_node_exists = updateSneakNode", stock_collision_index)
    finish_index = localplayer_cpp.index("finishNavyCraftLocalPlayerMove", sneak_index)
    set_position_index = localplayer_cpp.index("setPosition(position)", finish_index)
    assert begin_index < stock_collision_index < sneak_index < finish_index < set_position_index
    assert "result.touching_ground);" not in localplayer_cpp
    assert "touching_ground);" in localplayer_cpp
    assert server_h.count("SendNavyCraftConstructMessage") == 1
    assert server_h.count("handleCommand_NavyCraftHandshake") == 1
    assert server_h.count("IsNavyCraftPeerReady") == 1
    assert server_h.count("ClearNavyCraftHandshakeState") == 1
    assert server_h.count("ReconcileNavyCraftRider") == 1
    assert server_h.count("StepNavyCraftNativeSimulation") == 1
    assert server_h.count("StepNavyCraftCarriedObjects") == 1
    assert server_h.count("StepNavyCraftConstructTimers") == 1
    assert server_h.count("ClearNavyCraftInteractionState") == 1
    assert server_packets.count("SendNavyCraftFullState(peer_id)") == 0
    assert server_packets.count("ClearNavyCraftInteractionState(peer_id)") == 1
    assert server_packets.count("ClearNavyCraftHandshakeState(peer_id)") == 1
    assert server_packets.count("const bool navycraft_rider") == 1
    assert server_cpp.count("StepNavyCraftNativeSimulation(dtime)") == 1
    assert server_cpp.count("StepNavyCraftCarriedObjects(dtime)") == 1
    assert server_cpp.count("StepNavyCraftConstructTimers(dtime)") == 1
    assert mapblock_mesh_h.count("materializeTransparentBuffersForSceneNode") == 1
    assert mapblock_mesh_cpp.count("MapBlockMesh::materializeTransparentBuffersForSceneNode") == 1
    assert (root / "src" / "navycraft" / "client" / "client_construct_scene.cpp").exists()
    assert (root / "src" / "navycraft" / "client" / "client_construct_effects.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_effects.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_projectiles.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_fire_control.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_navigation.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_structure.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_articulation.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_articulation_packets.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_articulated_motion.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_liquids.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_special_nodes.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "rider_motion.cpp").exists()
    assert (root / "src" / "navycraft" / "server" / "rider_integration.cpp").exists()
    assert (root / "src" / "navycraft" / "server" / "simulation_integration.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_simulation.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_network_clock.cpp").exists()
    assert (root / "src" / "navycraft" / "server" / "interaction_integration.cpp").exists()
    assert (root / "src" / "navycraft" / "server" / "handshake_integration.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_handshake.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_database.cpp").exists()
    assert (root / "src" / "navycraft" / "construct" / "construct_persistence.cpp").exists()
    script_api = (root / "src" / "navycraft" / "script_api.cpp").read_text(encoding="utf-8")
    assert "resolve_dynamic_construct_mutation" in script_api
    assert "initialise_dynamic_construct_persistence" in script_api
    assert "rollback_dynamic_construct_action" in script_api
    assert "emit_dynamic_construct_effect" in script_api
    assert "spawn_dynamic_construct_projectile" in script_api
    assert "observe_dynamic_construct_target" in script_api
    assert "solve_dynamic_construct_fire_control" in script_api
    assert "configure_dynamic_construct_battery" in script_api
    assert "step_dynamic_construct_fire_control" in script_api
    assert "configure_dynamic_construct_navigation" in script_api
    assert "set_dynamic_construct_route" in script_api
    assert "observe_dynamic_construct_obstacle" in script_api
    assert "step_dynamic_construct_navigation" in script_api
    assert "step_dynamic_construct_structure" in script_api
    assert "get_dynamic_construct_structure" in script_api
    assert "force_dynamic_construct_split" in script_api
    assert "set_dynamic_construct_flooding" in script_api
    assert "configure_dynamic_construct_joint" in script_api
    assert "configure_dynamic_construct_turret" in script_api
    assert "dynamic_construct_line_of_fire" in script_api
    assert "configure_dynamic_construct_liquid_compartment" in script_api
    assert "configure_dynamic_construct_liquid_port" in script_api
    assert "set_dynamic_construct_liquid" in script_api
    assert "step_dynamic_construct_liquids" in script_api
    assert "get_dynamic_construct_liquids" in script_api
    assert "register_dynamic_construct_special_node" in script_api
    assert "lua_pushinteger(L, 16)" in script_api
    assert 'registerFunction(L, "step_dynamic_constructs"' not in script_api

with tempfile.TemporaryDirectory(prefix="navycraft-overlay-") as directory:
    root = Path(directory)
    (root / "src" / "script").mkdir(parents=True)
    (root / "src" / "network").mkdir(parents=True)
    (root / "src" / "client").mkdir(parents=True)

    (root / "src" / "CMakeLists.txt").write_text(
        "add_subdirectory(server)\n\n"
        "set(common_SRCS\n\t${common_HDRS}\n\tmain.cpp\n)\n\n"
        "list(APPEND client_SRCS\n\t${benchmark_client_SRCS}\n\t${common_SRCS}\n)\n",
        encoding="utf-8",
    )
    (root / "src" / "script" / "scripting_server.cpp").write_text(
        '#include "lua_api/l_ipc.h"\n\n'
        "void ServerScripting::InitializeModApi(lua_State *L, int top)\n{\n"
        "\tModApiIPC::Initialize(L, top);\n}\n",
        encoding="utf-8",
    )
    (root / "src" / "main.cpp").write_text(
        '#include "config.h"\n\n'
        'int main(int argc, char *argv[])\n'
        '{\n'
        '\tSettings cmd_args;\n'
        '\tif (cmd_args.getFlag("version")) {\n'
        '\t\tporting::attachOrCreateConsole();\n'
        '\t\tprint_version(std::cout);\n'
        '\t\treturn 0;\n'
        '\t}\n\n'
        '\treturn 0;\n'
        '}\n\n'
        'static void set_allowed_options(OptionList *allowed_options)\n'
        '{\n'
        '\tallowed_options->insert(std::make_pair("help", ValueSpec(VALUETYPE_FLAG,\n'
        '\t\t\t_("Show allowed options"))));\n'
        '\tallowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,\n'
        '\t\t\t_("Show version information"))));\n'
        '\tallowed_options->insert(std::make_pair("config", ValueSpec(VALUETYPE_STRING,\n'
        '\t\t\t_("Load configuration from specified file"))));\n'
        '}\n',
        encoding="utf-8",
    )
    (root / "src" / "network" / "networkprotocol.h").write_text(
        "enum ToClientCommand : unsigned short {\n"
        "\tTOCLIENT_SPAWN_PARTICLE_BATCH = 0x64,\n"
        "\t/*\n\t\tstd::string data, zstd-compressed, for each particle:\n"
        "\t\t\tu32 len\n\t\t\tu8[len] serialized ParticleParameters\n\t*/\n\n"
        "\tTOCLIENT_NUM_MSG_TYPES = 0x65,\n};\n\n"
        "enum ToServerCommand : unsigned short {\n"
        "\tTOSERVER_UPDATE_CLIENT_INFO = 0x53,\n"
        "\t/*\n\t\tv2s16 render_target_size\n\t\tf32 gui_scaling\n"
        "\t\tf32 hud_scaling\n\t\tv2f32 max_fs_info\n\t*/\n\n"
        "\tTOSERVER_NUM_MSG_TYPES = 0x54,\n};\n",
        encoding="utf-8",
    )
    (root / "src" / "network" / "clientopcodes.cpp").write_text(
        "const Handler table[] = {\n"
        "\t{ \"TOCLIENT_SPAWN_PARTICLE_BATCH\",     TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticleBatch }, // 0x64,\n"
        "};\n\n"
        "const Factory serverCommandFactoryTable[] = {\n"
        "\t{ \"TOSERVER_UPDATE_CLIENT_INFO\", 2, true }, // 0x53\n"
        "};\n",
        encoding="utf-8",
    )
    (root / "src" / "network" / "serveropcodes.cpp").write_text(
        "const Handler toServerCommandTable[] = {\n"
        "\t{ \"TOSERVER_UPDATE_CLIENT_INFO\",       TOSERVER_STATE_INGAME, &Server::handleCommand_UpdateClientInfo }, // 0x53\n"
        "};\n\n"
        "const Factory clientCommandFactoryTable[] = {\n"
        "\t{ \"TOCLIENT_SPAWN_PARTICLE_BATCH\",     0, true }, // 0x64\n"
        "};\n",
        encoding="utf-8",
    )
    (root / "src" / "client" / "client.h").write_text(
        "class SSCSMController;\n"
        "class Client {\npublic:\n"
        "\tvoid handleCommand_Camera(NetworkPacket* pkt);\n"
        "private:\n"
        "\tvoid loadMods();\n"
        "\tstd::unique_ptr<ModChannelMgr> m_modchannel_mgr;\n"
        "};\n",
        encoding="utf-8",
    )
    (root / "src" / "server.h").write_text(
        "class Settings;\n"
        "class Server {\npublic:\n"
        "\tvoid Send(NetworkPacket *pkt);\n"
        "\tvoid Send(session_t peer_id, NetworkPacket *pkt);\n"
        "};\n", encoding="utf-8")
    (root / "src" / "server.cpp").write_text(
        "void Server::AsyncRunStep(float dtime) {\n"
        "\t{\n\t\t// Step environment\n\t\tm_env->step(dtime);\n\t}\n}\n",
        encoding="utf-8",
    )
    (root / "src" / "client" / "client.cpp").write_text(
        "void Client::connect() {\n\tm_address_name = address_name;\n}\n"
        "void Client::step(float dtime) {\n\tm_env.step(dtime);\n\tm_sound->step(dtime);\n}\n"
        "void Client::interact(InteractAction action, const PointedThing& pointed)\n{\n}\n",
        encoding="utf-8",
    )
    (root / "src" / "client" / "localplayer.cpp").write_text(
        "void LocalPlayer::move(float dtime, Environment *env) {\n"
        "\tv3f position; v3f m_speed; float gravity = 1;\n"
        "\tv3f accel_f(0, -gravity, 0);\n"
        "\tconst v3f initial_position = position;\n"
        "\tconst v3f initial_speed = m_speed;\n"
        "\tStepUpMode step_up_mode = StepUpMode::LEGACY;\n"
        "\tcollisionMoveResult result = collisionMoveSimple(env, m_client,\n"
        "\t\tm_collisionbox, player_stepheight, dtime,\n"
        "\t\t&position, &m_speed, accel_f, m_cao, true, step_up_mode);\n"
        "\tbool touching_ground = result.touching_ground;\n"
        "\tbool new_sneak_node_exists = updateSneakNode(map, position, sneak_max);\n"
        "\t/*\n"
        "\t\tSet new position but keep sneak node set\n"
        "\t*/\n"
        "\tsetPosition(position);\n"
        "}\n",
        encoding="utf-8",
    )
    (root / "src" / "client" / "mapblock_mesh.h").write_text(
        "class MapBlockMesh {\npublic:\n"
        "\tvoid consolidateTransparentBuffers();\n"
        "};\n",
        encoding="utf-8",
    )
    (root / "src" / "client" / "mapblock_mesh.cpp").write_text(
        "void MapBlockMesh::consolidateTransparentBuffers()\n{\n}\n",
        encoding="utf-8",
    )
    (root / "src" / "network" / "serverpackethandler.cpp").write_text(
        "void Server::handleCommand_ClientReady(NetworkPacket *pkt) {\n"
        "\tsession_t peer_id = pkt->getPeerId();\n"
        "\tRemoteClient *client = getClient(peer_id, CS_Created);\n"
        "\tm_script->on_joinplayer(playersao, last_login);\n}\n\n"
        "void Server::process_PlayerPos(RemotePlayer *player, PlayerSAO *playersao, NetworkPacket *pkt) {\n"
        "\tv3f position; v3f speed;\n"
        "\tif (!playersao->isAttached()) {\n\t}\n"
        "\tif (playersao->checkMovementCheat()) {\n\t}\n}\n",
        encoding="utf-8",
    )

    for _ in range(2):
        subprocess.run([sys.executable, str(PATCHER), str(root)], check=True)

    assert_applied_overlay(root)

print("NavyCraft native Gate 9 overlay fixture test passed")

if len(sys.argv) > 2:
    raise SystemExit("usage: test-overlay.py [PATCHED_LUANTI_TREE]")
if len(sys.argv) == 2:
    applied = Path(sys.argv[1]).resolve()
    assert_applied_overlay(applied)
    print(f"NavyCraft native overlay verified on {applied}")

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
    gameui_cpp = (root / "src" / "client" / "gameui.cpp").read_text(encoding="utf-8")
    localplayer_cpp = (root / "src" / "client" / "localplayer.cpp").read_text(encoding="utf-8")
    chat_console_cpp = (root / "src" / "gui" / "guiChatConsole.cpp").read_text(encoding="utf-8")
    chat_console_h = (root / "src" / "gui" / "guiChatConsole.h").read_text(encoding="utf-8")
    server_h = (root / "src" / "server.h").read_text(encoding="utf-8")
    server_packets = (root / "src" / "network" / "serverpackethandler.cpp").read_text(encoding="utf-8")
    server_cpp = (root / "src" / "server.cpp").read_text(encoding="utf-8")
    mapblock_mesh_h = (root / "src" / "client" / "mapblock_mesh.h").read_text(encoding="utf-8")
    mapblock_mesh_cpp = (root / "src" / "client" / "mapblock_mesh.cpp").read_text(encoding="utf-8")

    assert cmake.count("add_subdirectory(navycraft)") == 1
    assert cmake.count("${navycraft_SRCS}") == 1
    assert cmake.count("${navycraft_client_SRCS}") == 1
    assert cmake.count("${PROJECT_SOURCE_DIR}/navycraft") == 1
    assert cmake.count("${PROJECT_SOURCE_DIR}/client") == 1
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
    assert client_cpp.count('#include "navycraft/client/client_construct_effects.h"') == 1
    assert client_cpp.count('#include "navycraft/client/client_construct_scene.h"') == 1
    assert "prepareNavyCraftLocalPlayerForPhysics" not in client_cpp
    assert '#include "navycraft/chat_style.h"' in gameui_cpp
    assert "navycraft::currentChatStyle()" in gameui_cpp
    assert "style.recent_font_mode == navycraft::ChatFontMode::Standard" in gameui_cpp
    assert "style.recent_anchor == navycraft::ChatAnchor::BottomLeft" in gameui_cpp
    assert '#include "navycraft/chat_style.h"' in chat_console_cpp
    assert "GUIChatConsole::applyNavyCraftChatStyle()" in chat_console_cpp
    assert "navycraftConsoleRect" in chat_console_cpp
    assert "navycraftFontMode(style.console_font_mode, FM_Mono)" in chat_console_cpp
    assert "AbsoluteRect.UpperLeftCorner" in chat_console_cpp
    assert "m_navycraft_style_revision" in chat_console_h
    assert "m_screensize.Y - m_height" not in chat_console_cpp
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
    client_scene = (root / "src" / "navycraft" / "client" / "client_construct_scene.cpp").read_text(encoding="utf-8")
    assert "result.touching_ground = input.touching_ground;" not in client_scene
    assert "pending.touching_ground ||" not in client_scene
    assert "bool grounded = input.touching_ground || collision.touching_ground;" in client_scene
    assert "MapNode lit_air(CONTENT_AIR, 0x0f, 0)" in client_scene
    assert "MapNode lit_air(CONTENT_AIR, 0xff, 0)" not in client_scene
    assert client_scene.count("setAutomaticCulling(scene::EAC_OFF)") >= 2
    assert "setAutomaticCulling(scene::EAC_BOX)" not in client_scene
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
    assert "set_chat_style" in script_api
    assert "get_chat_style" in script_api
    assert "fontModeName" in script_api
    assert "lua_pushinteger(L, 16)" in script_api
    assert 'registerFunction(L, "step_dynamic_constructs"' not in script_api

def assert_project_sources() -> None:
    native_state = (PROJECT / "game" / "navycraft" / "mods" / "nc_core" /
        "native_construct_state.lua").read_text(encoding="utf-8")
    assert "send_drive_velocity = function(construct,force)" in native_state
    assert "mark_motion_sent(construct)" in native_state
    assert "if math.abs(construct.yaw_rate or 0)>0.0001 and math.abs(construct.forward_speed or 0)>0.0001" not in native_state
    restore = native_state[native_state.index("local function restore_saved_constructs"):]
    assert "state=core.get_dynamic_construct(native_id,false)" in restore
    assert "native_id,error_message=create_native" in restore
    assert restore.index("native_id,error_message=create_native") < restore.index("state=core.get_dynamic_construct(native_id,false)")
    assert "send_drive_velocity(saved,true)" in restore
    assert "function M.find_at_world_node(world_pos)" in native_state
    assert "vector.distance(exact,rounded)<1.25" in native_state
    assert "local function clear_source_nodes(scan_result)" in native_state
    assert "core.remove_node(entry.pos)" in native_state
    assert "source blocks were not cleared" in native_state
    assert "function M.untracked_runtime_count()" in native_state
    assert "function M.purge_all(reason)" in native_state
    assert "core.list_dynamic_constructs()" in native_state
    assert "untracked_runtime_ids(native_id)" in native_state
    assert "core.sync_dynamic_construct_persistence()" in native_state
    step = native_state[native_state.index("core.register_globalstep"):]
    assert step.index("for _,callback in ipairs(step_hooks)") < step.index("if math.abs(construct.turn_remaining")
    assert step.index("if math.abs(construct.turn_remaining") < step.index("send_drive_velocity(construct,false)")

    runtime_h = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_runtime.h").read_text(encoding="utf-8")
    runtime_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_runtime.cpp").read_text(encoding="utf-8")
    persistence_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_persistence.cpp").read_text(encoding="utf-8")
    interaction_h = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_interaction.h").read_text(encoding="utf-8")
    interaction_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_interaction.cpp").read_text(encoding="utf-8")
    fire_control_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_fire_control.cpp").read_text(encoding="utf-8")
    projectiles_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_projectiles.cpp").read_text(encoding="utf-8")
    registry_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "construct" / "construct_registry.cpp").read_text(encoding="utf-8")
    assert "void resetRuntimeConstructState() noexcept;" in runtime_h
    assert "void clearRuntimeConstructState(ConstructId id) noexcept;" in runtime_h
    assert "void resetRuntimeConstructState() noexcept" in runtime_cpp
    assert "void clearRuntimeConstructState(ConstructId id) noexcept" in runtime_cpp
    assert "runtimeConstructInteractionEngine().removeConstruct(id);" in runtime_cpp
    for call in (
        "runtimeConstructInteractionEngine().clear();",
        "runtimeConstructProjectileEngine().clear();",
        "runtimeConstructFireControlEngine().clear();",
        "runtimeConstructNavigationEngine().clear();",
        "runtimeConstructStructureEngine().clear();",
        "runtimeConstructArticulationEngine().clear();",
        "runtimeConstructLiquidEngine().clear();",
        "runtimeConstructSpecialNodeEngine().clear();",
        "runtimeConstructSimulation().reset();",
        "runtimeConstructRegistry().clear();",
    ):
        assert call in runtime_cpp
    assert "resetRuntimeConstructState();" in persistence_cpp
    assert "void clear() noexcept;" in interaction_h
    assert "void removeConstruct(ConstructId construct_id);" in interaction_h
    assert "void ConstructInteractionEngine::clear() noexcept" in interaction_cpp
    assert "void ConstructInteractionEngine::removeConstruct(ConstructId construct_id)" in interaction_cpp
    assert "m_next_event_id = 1;" in interaction_cpp
    assert "void ConstructFireControlEngine::clear()" in fire_control_cpp
    assert "m_next_battery_id = 1;" in fire_control_cpp
    assert "m_next_id = 1;" in projectiles_cpp
    assert "m_next_id.store(1, std::memory_order_relaxed);" in registry_cpp

    script_api_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "script_api.cpp").read_text(encoding="utf-8")
    assert "clearRuntimeConstructState(id);" in script_api_cpp
    assert "clearRuntimeConstructState(construct->id());" in script_api_cpp
    client_scene_cpp = (PROJECT / "engine-overlay" / "src" / "navycraft" /
        "client" / "client_construct_scene.cpp").read_text(encoding="utf-8")
    assert "node_def->getId(entry.node.node_name, resolved)" in client_scene_cpp
    assert client_scene_cpp.index("node_def->getId(entry.node.node_name, resolved)") < client_scene_cpp.index("MapNode node(entry.node.content_id")

    core_init = (PROJECT / "game" / "navycraft" / "mods" / "nc_core" /
        "init.lua").read_text(encoding="utf-8")
    launch = core_init[core_init.index("local function launch_from_origin"):]
    assert "construct_state.find_at_world_node(origin)" in launch
    assert "construct_state.request_convert_to_blocks(active, name)" in launch
    assert "construct_state.untracked_runtime_count" in launch
    assert launch.index("construct_state.find_at_world_node(origin)") < launch.index("scan_from_origin(player, origin)")
    assert 'core.register_chatcommand("nc_purge_constructs"' in core_init

    item_tooltips = (PROJECT / "game" / "navycraft" / "mods" / "nc_core" /
        "item_tooltips.lua").read_text(encoding="utf-8")
    assert "local inventory_open = {}" in item_tooltips
    assert "local function toggle_inventory(player)" in item_tooltips
    assert "core.close_formspec(name, \"\")" in item_tooltips
    assert "toggle_inventory(player)" in item_tooltips

    definitions = (PROJECT / "game" / "navycraft" / "mods" / "nc_navycraft" /
        "definitions.lua").read_text(encoding="utf-8")
    assert 'ship = {drive_command="sail", min_blocks=50' in definitions
    assert 'freeship = {drive_command="sail", min_blocks=50' in definitions
    assert 'halfship = {drive_command="sail", min_blocks=50' in definitions
    systems = (PROJECT / "game" / "navycraft" / "mods" / "nc_navycraft" /
        "systems.lua").read_text(encoding="utf-8")
    assert "function S.speed_change(c,increase)" in systems
    assert "function S.gear_change(c,increase)" in systems
    assert "function S.rudder_order(c,order,turn)" in systems
    assert "movement_interval(craft_type,gear)" in systems
    controls = (PROJECT / "game" / "navycraft" / "mods" / "nc_navycraft" /
        "controls.lua").read_text(encoding="utf-8")
    assert "core.register_globalstep(function()" in controls
    assert "S.speed_change(c,true)" in controls
    assert "local function set_forward(c)" in controls
    assert "local function set_reverse(c)" in controls
    assert "S.set_gear(c,1)" in controls
    assert "S.set_gear(c,-1)" in controls
    assert "S.rudder_order(c,1,true)" in controls

assert_project_sources()

with tempfile.TemporaryDirectory(prefix="navycraft-overlay-") as directory:
    root = Path(directory)
    (root / "src" / "script").mkdir(parents=True)
    (root / "src" / "network").mkdir(parents=True)
    (root / "src" / "client").mkdir(parents=True)
    (root / "src" / "gui").mkdir(parents=True)

    (root / "src" / "CMakeLists.txt").write_text(
        "add_subdirectory(server)\n\n"
        "set(common_SRCS\n\t${common_HDRS}\n\tmain.cpp\n)\n\n"
        "list(APPEND client_SRCS\n\t${benchmark_client_SRCS}\n\t${common_SRCS}\n)\n\n"
        "include_directories(\n\t${PROJECT_BINARY_DIR}\n\t${PROJECT_SOURCE_DIR}\n\t${PROJECT_SOURCE_DIR}/script\n)\n",
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
        '#include "client/texturepaths.h"\n\n'
        "void Client::connect() {\n\tm_address_name = address_name;\n}\n"
        "void Client::step(float dtime) {\n\tm_env.step(dtime);\n\tm_sound->step(dtime);\n}\n"
        "void Client::interact(InteractAction action, const PointedThing& pointed)\n{\n}\n",
        encoding="utf-8",
    )
    (root / "src" / "client" / "gameui.cpp").write_text(
        '#include "version.h"\n\n'
        "void GameUI::updateChatSize()\n"
        "{\n"
        "\t// Update gui element size and position\n"
        "\ts32 chat_y = 5;\n\n"
        "\tif (m_flags.show_minimal_debug)\n"
        "\t\tchat_y += m_guitext->getTextHeight();\n"
        "\tif (m_flags.show_basic_debug)\n"
        "\t\tchat_y += m_guitext2->getTextHeight();\n\n"
        "\tconst v2u32 window_size = RenderingEngine::getWindowSize();\n\n"
        "\tcore::rect<s32> chat_size(10, chat_y, window_size.X - 20, 0);\n"
        "\tchat_size.LowerRightCorner.Y = std::min((s32)window_size.Y,\n"
        "\t\t\tm_guitext_chat->getTextHeight() + chat_y);\n\n"
        "\tif (chat_size == m_current_chat_size)\n"
        "\t\treturn;\n"
        "\tm_current_chat_size = chat_size;\n\n"
        "\tm_guitext_chat->setRelativePosition(chat_size);\n"
        "}\n",
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
    (root / "src" / "gui" / "guiChatConsole.cpp").write_text(
        '#include "util/string.h"\n\n'
        "inline u32 getScrollbarSize(IGUIEnvironment* env)\n"
        "{\n"
        "\treturn env->getSkin()->getSize(gui::EGDS_SCROLLBAR_SIZE);\n"
        "}\n\n"
        "void GUIChatConsole::setCursor(\n"
        "\tbool visible, bool blinking, f32 blink_speed, f32 relative_height)\n"
        "{\n"
        "\tif (visible)\n"
        "\t{\n"
        "\t\tif (blinking)\n"
        "\t\t{\n"
        "\t\t\t// leave m_cursor_blink unchanged\n"
        "\t\t\tm_cursor_blink_speed = blink_speed;\n"
        "\t\t}\n"
        "\t\telse\n"
        "\t\t{\n"
        "\t\t\tm_cursor_blink = 0x8000;  // on\n"
        "\t\t\tm_cursor_blink_speed = 0.0;\n"
        "\t\t}\n"
        "\t}\n"
        "\telse\n"
        "\t{\n"
        "\t\tm_cursor_blink = 0;  // off\n"
        "\t\tm_cursor_blink_speed = 0.0;\n"
        "\t}\n"
        "\tm_cursor_height = relative_height;\n"
        "}\n\n"
        "void GUIChatConsole::draw()\n"
        "{\n"
        "\tif(!IsVisible)\n"
        "\t\treturn;\n\n"
        "\tvideo::IVideoDriver* driver = Environment->getVideoDriver();\n"
        "\tv2u32 screensize = driver->getScreenSize();\n"
        "\tif (screensize != m_screensize)\n"
        "\t{\n"
        "\t\tm_screensize = screensize;\n"
        "\t\treformatConsole();\n"
        "\t} else if (!m_scrollbar->getAbsolutePosition().isPointInside(core::vector2di(screensize.X, m_height))) {\n"
        "\t\tupdateScrollbar(true);\n"
        "\t}\n\n"
        "\t// Animation\n"
        "\tu64 now = porting::getTimeMs();\n"
        "\tanimate(now - m_animate_time_old);\n"
        "}\n\n"
        "void GUIChatConsole::reformatConsole()\n"
        "{\n"
        "\ts32 cols = m_screensize.X / m_fontsize.X - 2; // make room for a margin (looks better)\n"
        "\ts32 rows = m_desired_height / m_fontsize.Y - 1; // make room for the input prompt\n"
        "\tif (cols <= 0 || rows <= 0)\n"
        "\t\tcols = rows = 0;\n\n"
        "\tupdateScrollbar(true);\n\n"
        "\trecalculateConsolePosition();\n"
        "\tm_chat_backend->reformat(cols, rows);\n"
        "}\n\n"
        "void GUIChatConsole::recalculateConsolePosition()\n"
        "{\n"
        "\tcore::rect<s32> rect(0, 0, m_screensize.X, m_height);\n"
        "\tDesiredRect = rect;\n"
        "\trecalculateAbsolutePosition(false);\n"
        "}\n\n"
        "void GUIChatConsole::drawBackground()\n"
        "{\n"
        "\tvideo::IVideoDriver* driver = Environment->getVideoDriver();\n"
        "\tif (m_background != NULL)\n"
        "\t{\n"
        "\t\tcore::rect<s32> sourcerect(0, -m_height, m_screensize.X, 0);\n"
        "\t\tdriver->draw2DImage(\n"
        "\t\t\tm_background,\n"
        "\t\t\tv2s32(0, 0),\n"
        "\t\t\tsourcerect,\n"
        "\t\t\t&AbsoluteClippingRect,\n"
        "\t\t\tm_background_color,\n"
        "\t\t\tfalse);\n"
        "\t}\n"
        "\telse\n"
        "\t{\n"
        "\t\tdriver->draw2DRectangle(\n"
        "\t\t\tm_background_color,\n"
        "\t\t\tcore::rect<s32>(0, 0, m_screensize.X, m_height),\n"
        "\t\t\t&AbsoluteClippingRect);\n"
        "\t}\n"
        "}\n\n"
        "void GUIChatConsole::drawText()\n"
        "{\n"
        "\tif (!m_font)\n"
        "\t\treturn;\n\n"
        "\tChatBuffer& buf = m_chat_backend->getConsoleBuffer();\n\n"
        "\tcore::recti rect;\n"
        "\tif (m_scrollbar->isVisible())\n"
        "\t\trect = core::rect<s32> (0, 0, m_screensize.X - getScrollbarSize(Environment), m_height);\n"
        "\telse\n"
        "\t\trect = AbsoluteClippingRect;\n\n"
        "\tfor (u32 row = 0; row < buf.getRows(); ++row)\n"
        "\t{\n"
        "\t\tconst ChatFormattedLine& line = buf.getFormattedLine(row);\n"
        "\t\ts32 line_height = m_fontsize.Y;\n"
        "\t\ts32 y = row * line_height + m_height - m_desired_height;\n"
        "\t\tif (y + line_height < 0)\n"
        "\t\t\tcontinue;\n"
        "\t\tfor (const ChatFormattedFragment &fragment : line.fragments) {\n"
        "\t\t\ts32 x = (fragment.column + 1) * m_fontsize.X;\n"
        "\t\t\tcore::rect<s32> destrect(\n"
        "\t\t\t\tx, y, x + m_fontsize.X * fragment.text.size(), y + m_fontsize.Y);\n"
        "\t\t\tm_font->draw(\n"
        "\t\t\t\tfragment.text.c_str(),\n"
        "\t\t\t\tdestrect,\n"
        "\t\t\t\tvideo::SColor(255, 255, 255, 255),\n"
        "\t\t\t\tfalse,\n"
        "\t\t\t\tfalse,\n"
        "\t\t\t\t&rect);\n"
        "\t\t}\n"
        "\t}\n"
        "}\n\n"
        "void GUIChatConsole::drawPrompt()\n"
        "{\n"
        "\tif (!m_font)\n"
        "\t\treturn;\n\n"
        "\tChatPrompt& prompt = m_chat_backend->getPrompt();\n"
        "\tstd::wstring prompt_text = prompt.getVisiblePortion();\n\n"
        "\tu32 font_width  = m_fontsize.X;\n"
        "\tu32 font_height = m_fontsize.Y;\n\n"
        "\tcore::dimension2d<u32> size = m_font->getDimension(prompt_text.c_str());\n"
        "\tu32 text_width = size.Width;\n"
        "\tif (size.Height > font_height)\n"
        "\t\tfont_height = size.Height;\n\n"
        "\tu32 row = m_chat_backend->getConsoleBuffer().getRows();\n"
        "\ts32 y = row * font_height + m_height - m_desired_height;\n\n"
        "\tcore::rect<s32> destrect(\n"
        "\t\tfont_width, y, font_width + text_width, y + font_height);\n"
        "\tm_font->draw(\n"
        "\t\tprompt_text.c_str(),\n"
        "\t\tdestrect,\n"
        "\t\tvideo::SColor(255, 255, 255, 255),\n"
        "\t\tfalse,\n"
        "\t\tfalse,\n"
        "\t\t&AbsoluteClippingRect);\n\n"
        "\ts32 cursor_pos = prompt.getVisibleCursorPosition();\n"
        "\tu32 text_to_cursor_pos_width = m_font->getDimension(prompt_text.substr(0, cursor_pos).c_str()).Width;\n"
        "\ts32 x = font_width + text_to_cursor_pos_width;\n"
        "}\n\n"
        "bool GUIChatConsole::OnEvent(const SEvent& event)\n"
        "{\n"
        "\tif (event.MouseInput.Y / m_fontsize.Y < (m_height / m_fontsize.Y) - 1 )\n"
        "\t{\n"
        "\t\t// Translate pixel position to font position\n"
        "\t\tweblinkClick(event.MouseInput.X / m_fontsize.X,\n"
        "\t\t\t\tevent.MouseInput.Y / m_fontsize.Y);\n"
        "\t}\n"
        "\treturn false;\n"
        "}\n\n"
        "void GUIChatConsole::updateScrollbar(bool update_size)\n"
        "{\n"
        "\tChatBuffer &buf = m_chat_backend->getConsoleBuffer();\n"
        "\tm_scrollbar->setPageSize(m_fontsize.Y * buf.getLineCount());\n"
        "\tif (update_size) {\n"
        "\t\tconst core::rect<s32> rect (m_screensize.X - getScrollbarSize(Environment), 0, m_screensize.X, m_height);\n"
        "\t\tm_scrollbar->setRelativePosition(rect);\n"
        "\t}\n"
        "}\n",
        encoding="utf-8",
    )
    (root / "src" / "gui" / "guiChatConsole.h").write_text(
        "class GUIChatConsole {\n"
        "private:\n"
        "\tvoid reformatConsole();\n"
        "\tvoid recalculateConsolePosition();\n"
        "\t// console open/close animation speed [screen height fraction / second]\n"
        "\tf32 m_height_speed = 5.0f;\n"
        "};\n",
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

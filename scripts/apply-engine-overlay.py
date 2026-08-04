#!/usr/bin/env python3
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply-engine-overlay.py PATH_TO_LUANTI")

project = Path(__file__).resolve().parents[1]
luanti = Path(sys.argv[1]).resolve()
overlay = project / "engine-overlay" / "src" / "navycraft"
target = luanti / "src" / "navycraft"

required = {
    "cmake": luanti / "src" / "CMakeLists.txt",
    "scripting_server": luanti / "src" / "script" / "scripting_server.cpp",
    "main_cpp": luanti / "src" / "main.cpp",
    "networkprotocol": luanti / "src" / "network" / "networkprotocol.h",
    "clientopcodes": luanti / "src" / "network" / "clientopcodes.cpp",
    "serveropcodes": luanti / "src" / "network" / "serveropcodes.cpp",
    "serverpackethandler": luanti / "src" / "network" / "serverpackethandler.cpp",
    "client_h": luanti / "src" / "client" / "client.h",
    "client_cpp": luanti / "src" / "client" / "client.cpp",
    "localplayer_cpp": luanti / "src" / "client" / "localplayer.cpp",
    "server_h": luanti / "src" / "server.h",
    "server_cpp": luanti / "src" / "server.cpp",
    "mapblock_mesh_h": luanti / "src" / "client" / "mapblock_mesh.h",
    "mapblock_mesh_cpp": luanti / "src" / "client" / "mapblock_mesh.cpp",
}
missing = [str(path) for path in required.values() if not path.exists()]
if missing:
    raise SystemExit("not a compatible Luanti 5.16.1 source tree; missing: " + ", ".join(missing))

if target.exists():
    shutil.rmtree(target)
shutil.copytree(overlay, target)


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"Luanti layout changed: {label} anchor missing")
    return text.replace(old, new, 1)

# Build wiring.
cmake = required["cmake"]
text = cmake.read_text(encoding="utf-8")
text = replace_once(text,
    "add_subdirectory(server)\n",
    "add_subdirectory(server)\nadd_subdirectory(navycraft)\n",
    "server subdirectory")
text = replace_once(text,
    "set(common_SRCS\n\t${common_HDRS}\n",
    "set(common_SRCS\n\t${common_HDRS}\n\t${navycraft_SRCS}\n",
    "common source list")
text = replace_once(text,
    "list(APPEND client_SRCS\n\t${benchmark_client_SRCS}\n",
    "list(APPEND client_SRCS\n\t${navycraft_client_SRCS}\n\t${benchmark_client_SRCS}\n",
    "client source list")
cmake.write_text(text, encoding="utf-8")

# Lua API registration.
scripting_server = required["scripting_server"]
text = scripting_server.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "lua_api/l_ipc.h"\n',
    '#include "lua_api/l_ipc.h"\n#include "navycraft/script_api.h"\n',
    "scripting include")
text = replace_once(text,
    "\tModApiIPC::Initialize(L, top);\n",
    "\tModApiIPC::Initialize(L, top);\n\tModApiNavyCraft::Initialize(L, top);\n",
    "scripting API initializer")
scripting_server.write_text(text, encoding="utf-8")

# Native binary identity probes. These run before normal engine startup so the
# package verifier can prove the compiled exe exposes the expected fork identity.
main_cpp = required["main_cpp"]
text = main_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "config.h"\n',
    '#include "config.h"\n#include "navycraft/construct/construct_handshake.h"\n',
    "NavyCraft main include")
text = replace_once(text,
    '\tallowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show version information"))));\n',
    '\tallowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show version information"))));\n'
    '\tallowed_options->insert(std::make_pair("navycraft-version", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show NavyCraft native engine version and exit"))));\n'
    '\tallowed_options->insert(std::make_pair("navycraft-protocol", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show NavyCraft native construct protocol and exit"))));\n',
    "NavyCraft main options")
text = replace_once(text,
    '\tif (cmd_args.getFlag("version")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tprint_version(std::cout);\n'
    '\t\treturn 0;\n'
    '\t}\n\n',
    '\tif (cmd_args.getFlag("version")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tprint_version(std::cout);\n'
    '\t\treturn 0;\n'
    '\t}\n'
    '\tif (cmd_args.getFlag("navycraft-version")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tconst auto identity = navycraft::makeServerConstructIdentity();\n'
    '\t\tstd::cout << identity.engine_major << "." << identity.engine_minor\n'
    '\t\t\t<< "." << identity.engine_patch << std::endl;\n'
    '\t\treturn 0;\n'
    '\t}\n'
    '\tif (cmd_args.getFlag("navycraft-protocol")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tstd::cout << navycraft::ConstructHandshakeCodec::CURRENT_PROTOCOL << std::endl;\n'
    '\t\treturn 0;\n'
    '\t}\n\n',
    "NavyCraft main probe handlers")
main_cpp.write_text(text, encoding="utf-8")

# Reserve eight server-to-client commands after stock 5.16.1's 0x64 command.
networkprotocol = required["networkprotocol"]
text = networkprotocol.read_text(encoding="utf-8")
protocol_old = '''\tTOCLIENT_SPAWN_PARTICLE_BATCH = 0x64,\n\t/*\n\t\tstd::string data, zstd-compressed, for each particle:\n\t\t\tu32 len\n\t\t\tu8[len] serialized ParticleParameters\n\t*/\n\n\tTOCLIENT_NUM_MSG_TYPES = 0x65,\n'''
protocol_new = '''\tTOCLIENT_SPAWN_PARTICLE_BATCH = 0x64,\n\t/*\n\t\tstd::string data, zstd-compressed, for each particle:\n\t\t\tu32 len\n\t\t\tu8[len] serialized ParticleParameters\n\t*/\n\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION = 0x65,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM = 0x66,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE = 0x67,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_RESET = 0x68,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT = 0x69,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE = 0x6A,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION = 0x6B,\n\tTOCLIENT_NAVYCRAFT_HANDSHAKE = 0x6C,\n\n\tTOCLIENT_NUM_MSG_TYPES = 0x6D,\n'''
text = replace_once(text, protocol_old, protocol_new, "network protocol command tail")
networkprotocol.write_text(text, encoding="utf-8")

# Reserve client-to-server rider, interaction and native handshake commands after stock 5.16.1's 0x53 command.
text = networkprotocol.read_text(encoding="utf-8")
toserver_old = '''	TOSERVER_UPDATE_CLIENT_INFO = 0x53,
	/*
		v2s16 render_target_size
		f32 gui_scaling
		f32 hud_scaling
		v2f32 max_fs_info
	*/

	TOSERVER_NUM_MSG_TYPES = 0x54,
'''
toserver_new = '''	TOSERVER_UPDATE_CLIENT_INFO = 0x53,
	/*
		v2s16 render_target_size
		f32 gui_scaling
		f32 hud_scaling
		v2f32 max_fs_info
	*/

	TOSERVER_NAVYCRAFT_RIDER_STATE = 0x54,
	TOSERVER_NAVYCRAFT_INTERACTION = 0x55,
	TOSERVER_NAVYCRAFT_HANDSHAKE = 0x56,

	TOSERVER_NUM_MSG_TYPES = 0x57,
'''
text = replace_once(text, toserver_old, toserver_new, "server protocol command tail")
networkprotocol.write_text(text, encoding="utf-8")

# Client opcode dispatch table.
clientopcodes = required["clientopcodes"]
text = clientopcodes.read_text(encoding="utf-8")
opcode_old = '''\t{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticleBatch }, // 0x64,\n};\n'''
opcode_new = '''\t{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticleBatch }, // 0x64,\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructSection }, // 0x65\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructTransform }, // 0x66\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructRemove }, // 0x67\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_RESET", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructReset }, // 0x68\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructEffect }, // 0x69\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructProjectile }, // 0x6A\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructArticulation }, // 0x6B\n\t{ "TOCLIENT_NAVYCRAFT_HANDSHAKE", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftHandshake }, // 0x6C\n};\n'''
text = replace_once(text, opcode_old, opcode_new, "client opcode table tail")
clientopcodes.write_text(text, encoding="utf-8")

# Client-to-server packet factory entry.
text = clientopcodes.read_text(encoding="utf-8")
client_factory_old = '''	{ "TOSERVER_UPDATE_CLIENT_INFO", 2, true }, // 0x53
};
'''
client_factory_new = '''	{ "TOSERVER_UPDATE_CLIENT_INFO", 2, true }, // 0x53
	{ "TOSERVER_NAVYCRAFT_RIDER_STATE", 0, false }, // 0x54
	{ "TOSERVER_NAVYCRAFT_INTERACTION", 0, true }, // 0x55
	{ "TOSERVER_NAVYCRAFT_HANDSHAKE", 0, true }, // 0x56
};
'''
text = replace_once(text, client_factory_old, client_factory_new, "client rider packet factory")
clientopcodes.write_text(text, encoding="utf-8")

# Client declarations and owned scene state.
client_h = required["client_h"]
text = client_h.read_text(encoding="utf-8")
text = replace_once(text,
    "class SSCSMController;\n",
    "class SSCSMController;\nnamespace navycraft { class ClientConstructScene; class ClientConstructEffects; }\n",
    "NavyCraft client forward declaration")
text = replace_once(text,
    "\tvoid handleCommand_Camera(NetworkPacket* pkt);\n",
    "\tvoid handleCommand_Camera(NetworkPacket* pkt);\n"
    "\tvoid handleCommand_NavyCraftHandshake(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructSection(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructTransform(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructRemove(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructReset(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructEffect(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructProjectile(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructArticulation(NetworkPacket *pkt);\n",
    "NavyCraft handler declarations")
text = replace_once(text,
    "\tvoid loadMods();\n",
    "\tvoid loadMods();\n"
    "\tbool isNavyCraftProtocolReady() const noexcept;\n"
    "\tvoid sendNavyCraftHandshake();\n"
    "\tvoid ensureNavyCraftConstructScene();\n"
    "\tvoid stepNavyCraftConstructScene(float dtime);\n"
    "\tvoid beginNavyCraftLocalPlayerMove(LocalPlayer *player, float dtime, "
    "v3f &position, v3f &speed);\n"
    "\tvoid finishNavyCraftLocalPlayerMove(LocalPlayer *player, float dtime, "
    "v3f &position, v3f &speed, bool &touching_ground);\n"
    "\tvoid resetNavyCraftConstructScene();\n"
    "\tvoid resetNavyCraftConnection();\n"
    "\tbool sendNavyCraftInteraction(InteractAction action, const PointedThing &pointed);\n",
    "NavyCraft client helper declarations")
text = replace_once(text,
    "\tstd::unique_ptr<ModChannelMgr> m_modchannel_mgr;\n",
    "\tstd::unique_ptr<ModChannelMgr> m_modchannel_mgr;\n\n"
    "\tstd::unique_ptr<navycraft::ClientConstructScene> m_navycraft_construct_scene;\n"
    "\tstd::unique_ptr<navycraft::ClientConstructEffects> m_navycraft_construct_effects;\n"
    "\tdouble m_navycraft_client_time = 0.0;\n"
    "\tu64 m_navycraft_interaction_sequence = 0;\n"
    "\tu64 m_navycraft_handshake_nonce = 0;\n"
    "\tbool m_navycraft_handshake_sent = false;\n"
    "\tbool m_navycraft_handshake_accepted = false;\n",
    "NavyCraft client members")
client_h.write_text(text, encoding="utf-8")


# Server packet sender declaration.
server_h = required["server_h"]
text = server_h.read_text(encoding="utf-8")
text = replace_once(text,
    "class Settings;\n",
    "class Settings;\nnamespace navycraft { struct ConstructWireMessage; }\n",
    "NavyCraft server forward declaration")
text = replace_once(text,
    "\tvoid Send(NetworkPacket *pkt);\n\tvoid Send(session_t peer_id, NetworkPacket *pkt);\n",
    "\tvoid Send(NetworkPacket *pkt);\n\tvoid Send(session_t peer_id, NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftHandshake(NetworkPacket *pkt);\n"
    "\tbool IsNavyCraftPeerReady(session_t peer_id);\n"
    "\tvoid ClearNavyCraftHandshakeState(session_t peer_id);\n"
    "\tvoid SendNavyCraftConstructMessage(\n"
    "\t\tsession_t peer_id, const navycraft::ConstructWireMessage &message);\n"
    "\tvoid handleCommand_NavyCraftRiderState(NetworkPacket *pkt);\n"
    "\tbool ReconcileNavyCraftRider(session_t peer_id, v3f &position, v3f &speed);\n"
    "\tvoid ClearNavyCraftRiderState(session_t peer_id);\n"
    "\tvoid SendNavyCraftFullState(session_t peer_id);\n"
    "\tvoid StepNavyCraftNativeSimulation(float dtime);\n"
    "\tvoid StepNavyCraftCarriedObjects(float dtime);\n"
    "\tvoid handleCommand_NavyCraftInteraction(NetworkPacket *pkt);\n"
    "\tvoid StepNavyCraftConstructTimers(float dtime);\n"
    "\tvoid ClearNavyCraftInteractionState(session_t peer_id);\n",
    "NavyCraft server sender declaration")
server_h.write_text(text, encoding="utf-8")

# Step and reset the native scene with the normal client lifecycle.
client_cpp = required["client_cpp"]
text = client_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    "\tm_address_name = address_name;\n",
    "\tresetNavyCraftConnection();\n\tm_address_name = address_name;\n",
    "client reconnect reset")
text = replace_once(text,
    "\tm_env.step(dtime);\n\tm_sound->step(dtime);\n",
    "\tstepNavyCraftConstructScene(dtime);\n"
    "\tm_env.step(dtime);\n"
    "\tm_sound->step(dtime);\n",
    "client scene step")
text = replace_once(text,
    "void Client::interact(InteractAction action, const PointedThing& pointed)\n{\n",
    "void Client::interact(InteractAction action, const PointedThing& pointed)\n{\n"
    "\tif (sendNavyCraftInteraction(action, pointed))\n"
    "\t\treturn;\n",
    "construct-local client interaction hook")
client_cpp.write_text(text, encoding="utf-8")

# Integrate native construct motion directly into LocalPlayer's normal movement.
localplayer_cpp = required["localplayer_cpp"]
text = localplayer_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    "\tv3f accel_f(0, -gravity, 0);\n\tconst v3f initial_position = position;\n",
    "\tv3f accel_f(0, -gravity, 0);\n"
    "\tm_client->beginNavyCraftLocalPlayerMove(this, dtime, position, m_speed);\n"
    "\tconst v3f initial_position = position;\n",
    "LocalPlayer pre-physics construct movement")
text = replace_once(text,
    "\t/*\n"
    "\t\tSet new position but keep sneak node set\n"
    "\t*/\n"
    "\tsetPosition(position);\n",
    "\t/*\n"
    "\t\tSet new position but keep sneak node set\n"
    "\t*/\n"
    "\t// Finalize moving-hull collision only after stock Luanti has finished\n"
    "\t// sneak/ledge corrections. The transmitted rider state and the local\n"
    "\t// position now describe the same final pose for this frame.\n"
    "\tm_client->finishNavyCraftLocalPlayerMove(this, dtime, position, m_speed,\n"
    "\t\ttouching_ground);\n"
    "\tsetPosition(position);\n",
    "LocalPlayer final-pose moving-hull collision")
localplayer_cpp.write_text(text, encoding="utf-8")

# Expose transparent triangle indices when a MapBlockMesh is attached to a
# normal Irrlicht scene node instead of being rendered through ClientMap.
mapblock_mesh_h = required["mapblock_mesh_h"]
text = mapblock_mesh_h.read_text(encoding="utf-8")
text = replace_once(text,
    "\tvoid consolidateTransparentBuffers();\n",
    "\tvoid consolidateTransparentBuffers();\n"
    "\tvoid materializeTransparentBuffersForSceneNode();\n",
    "MapBlockMesh scene-node transparency declaration")
mapblock_mesh_h.write_text(text, encoding="utf-8")

mapblock_mesh_cpp = required["mapblock_mesh_cpp"]
text = mapblock_mesh_cpp.read_text(encoding="utf-8")
transparent_anchor = "void MapBlockMesh::consolidateTransparentBuffers()\n{\n"
transparent_method = """void MapBlockMesh::materializeTransparentBuffersForSceneNode()
{
\tstd::vector<scene::SMeshBuffer *> buffers;
\tfor (const auto &triangle : m_transparent_triangles) {
\t\tauto *buffer = triangle.buffer;
\t\tif (std::find(buffers.begin(), buffers.end(), buffer) == buffers.end())
\t\t\tbuffers.push_back(buffer);
\t}
\tfor (auto *buffer : buffers)
\t\tbuffer->Indices.clear();
\tfor (const auto &triangle : m_transparent_triangles) {
\t\ttriangle.buffer->Indices.push_back(triangle.p1);
\t\ttriangle.buffer->Indices.push_back(triangle.p2);
\t\ttriangle.buffer->Indices.push_back(triangle.p3);
\t}
}

"""
if transparent_method not in text:
    if transparent_anchor not in text:
        raise SystemExit("Luanti layout changed: MapBlockMesh transparency anchor missing")
    text = text.replace(transparent_anchor, transparent_method + transparent_anchor, 1)
mapblock_mesh_cpp.write_text(text, encoding="utf-8")

# Server opcode dispatch and outgoing command factories.
serveropcodes = required["serveropcodes"]
text = serveropcodes.read_text(encoding="utf-8")
server_handler_old = '''	{ "TOSERVER_UPDATE_CLIENT_INFO",       TOSERVER_STATE_INGAME, &Server::handleCommand_UpdateClientInfo }, // 0x53
};
'''
server_handler_new = '''	{ "TOSERVER_UPDATE_CLIENT_INFO",       TOSERVER_STATE_INGAME, &Server::handleCommand_UpdateClientInfo }, // 0x53
	{ "TOSERVER_NAVYCRAFT_RIDER_STATE",   TOSERVER_STATE_INGAME, &Server::handleCommand_NavyCraftRiderState }, // 0x54
	{ "TOSERVER_NAVYCRAFT_INTERACTION",    TOSERVER_STATE_INGAME, &Server::handleCommand_NavyCraftInteraction }, // 0x55
	{ "TOSERVER_NAVYCRAFT_HANDSHAKE",      TOSERVER_STATE_INGAME, &Server::handleCommand_NavyCraftHandshake }, // 0x56
};
'''
text = replace_once(text, server_handler_old, server_handler_new, "server rider handler table")
server_factory_old = '''	{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     0, true }, // 0x64
};
'''
server_factory_new = '''	{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     0, true }, // 0x64
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION", 2, true }, // 0x65
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM", 1, false }, // 0x66
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE", 0, true }, // 0x67
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_RESET", 0, true }, // 0x68
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT", 1, true }, // 0x69
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE", 1, false }, // 0x6A
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION", 1, false }, // 0x6B
	{ "TOCLIENT_NAVYCRAFT_HANDSHAKE", 0, true }, // 0x6C
};
'''
text = replace_once(text, server_factory_old, server_factory_new, "server construct command factories")
serveropcodes.write_text(text, encoding="utf-8")

# Late-join synchronisation and authoritative rider reconciliation.
serverpackethandler = required["serverpackethandler"]
text = serverpackethandler.read_text(encoding="utf-8")
text = replace_once(text,
    "	session_t peer_id = pkt->getPeerId();\n	RemoteClient *client = getClient(peer_id, CS_Created);\n",
    "	session_t peer_id = pkt->getPeerId();\n"
    "	ClearNavyCraftHandshakeState(peer_id);\n"
    "	ClearNavyCraftRiderState(peer_id);\n"
    "	ClearNavyCraftInteractionState(peer_id);\n"
    "	RemoteClient *client = getClient(peer_id, CS_Created);\n",
    "client-ready rider reset")
# Full construct state is sent only after the native protocol handshake succeeds.
text = replace_once(text,
    "	if (!playersao->isAttached()) {\n",
    "	const bool navycraft_rider = ReconcileNavyCraftRider(pkt->getPeerId(), position, speed);\n\n"
    "	if (!playersao->isAttached()) {\n",
    "server rider reconciliation")
text = replace_once(text,
    "	if (playersao->checkMovementCheat()) {\n",
    "	if (!navycraft_rider && playersao->checkMovementCheat()) {\n",
    "rider movement cheat exemption")
serverpackethandler.write_text(text, encoding="utf-8")

# Advance authoritative constructs before normal object physics, then carry
# remote riders and other supported objects after the environment step.
server_cpp = required["server_cpp"]
text = server_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    "\t\t// Step environment\n\t\tm_env->step(dtime);\n",
    "\t\t// Advance NavyCraft's authoritative fixed-step simulation before normal object physics.\n"
    "\t\tStepNavyCraftNativeSimulation(dtime);\n"
    "\t\t// Step environment\n\t\tm_env->step(dtime);\n"
    "\t\tStepNavyCraftCarriedObjects(dtime);\n"
    "\t\tStepNavyCraftConstructTimers(dtime);\n",
    "server native simulation and carried-object step")
server_cpp.write_text(text, encoding="utf-8")

print(f"Applied NavyCraft native Gate 9 engine overlay to {luanti}")

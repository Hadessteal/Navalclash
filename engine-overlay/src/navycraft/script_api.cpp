// SPDX-License-Identifier: LGPL-2.1-or-later
#include "script_api.h"

#include "construct/construct_runtime.h"
#include "construct/construct_geometry.h"
#include "construct/construct_effects.h"
#include "construct/construct_projectiles.h"
#include "construct/construct_fire_control.h"
#include "construct/construct_navigation.h"
#include "construct/construct_structure.h"
#include "construct/construct_articulation.h"
#include "construct/construct_articulation_packets.h"
#include "construct/construct_section.h"
#include "construct/construct_packets.h"
#include "construct/construct_interaction.h"
#include "construct/construct_persistence.h"
#include "construct/construct_serialization.h"
#include "chat_style.h"
#include "network/construct_replication_queue.h"
#include "server.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <unordered_map>

namespace {
constexpr std::size_t MAX_LUA_CONSTRUCT_NODES = 100000;

void broadcastMessage(Server *server, navycraft::ConstructWireMessage message)
{
    if (server)
        server->SendNavyCraftConstructMessage(PEER_ID_INEXISTENT, message);
}

void broadcastTransform(Server *server, const navycraft::DynamicConstruct &construct)
{
    navycraft::ConstructTransformSnapshot snapshot;
    snapshot.id = construct.id();
    snapshot.sequence = navycraft::nextRuntimeTransformSequence(construct.id());
    snapshot.server_time = server ? server->getUptime() : 0.0;
    snapshot.transform = construct.transform();
    snapshot.linear_velocity = construct.linearVelocity();
    snapshot.yaw_velocity = construct.yawVelocity();
    broadcastMessage(server, {navycraft::ConstructWireKind::Transform, construct.id(),
        navycraft::ConstructPacketCodec::encodeTransform(snapshot), false});
}

void broadcastSection(Server *server, const navycraft::DynamicConstruct &construct,
    const navycraft::ConstructSectionPos &position)
{
    navycraft::ConstructSectionIndex index;
    index.rebuild(construct);
    navycraft::ConstructSection section;
    if (const auto *existing = index.find(position))
        section = *existing;
    else
        section.position = position;
    section.revision = navycraft::nextRuntimeSectionSequence(construct.id());
    broadcastMessage(server, {navycraft::ConstructWireKind::Section, construct.id(),
        navycraft::ConstructPacketCodec::encodeSection(construct.id(), section), true});
}

void broadcastFullConstruct(Server *server, const navycraft::DynamicConstruct &construct)
{
    broadcastTransform(server, construct);
    navycraft::ConstructSectionIndex sections;
    sections.rebuild(construct);
    for (const auto &position : sections.positions()) {
        auto section = *sections.find(position);
        section.revision = navycraft::nextRuntimeSectionSequence(construct.id());
        broadcastMessage(server, {navycraft::ConstructWireKind::Section, construct.id(),
            navycraft::ConstructPacketCodec::encodeSection(construct.id(), section), true});
    }
}

void broadcastArticulationDefinition(Server *server,
    const navycraft::ConstructArticulationDefinition &definition)
{
    broadcastMessage(server, {navycraft::ConstructWireKind::Articulation,
        definition.construct_id,
        navycraft::ConstructArticulationPacketCodec::encodeDefinition(definition), true});
}

void broadcastArticulationState(Server *server,
    const navycraft::ConstructArticulationState &state, double server_time)
{
    navycraft::ConstructArticulationSnapshot snapshot;
    snapshot.construct_id = state.definition.construct_id;
    snapshot.articulation_id = state.definition.id;
    snapshot.sequence = std::max<std::uint64_t>(1, state.revision);
    snapshot.server_time = server_time;
    snapshot.position = state.position;
    snapshot.target_position = state.target_position;
    snapshot.velocity = state.velocity;
    snapshot.enabled = state.definition.enabled;
    broadcastMessage(server, {navycraft::ConstructWireKind::Articulation,
        snapshot.construct_id,
        navycraft::ConstructArticulationPacketCodec::encodeState(snapshot), false});
}

void getField(lua_State *L, int table_index, const char *name)
{
    if (table_index < 0)
        table_index = lua_gettop(L) + table_index + 1;
    lua_getfield(L, table_index, name);
}

double readNumberField(lua_State *L, int table_index, const char *name, double fallback)
{
    getField(L, table_index, name);
    const double value = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : fallback;
    lua_pop(L, 1);
    return value;
}

std::string readStringField(lua_State *L, int table_index, const char *name)
{
    getField(L, table_index, name);
    std::string value;
    if (lua_isstring(L, -1))
        value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return value;
}

navycraft::Vec3d readVec3(lua_State *L, int index)
{
    if (!lua_istable(L, index))
        throw std::invalid_argument("expected a vector table");
    return {
        readNumberField(L, index, "x", 0.0),
        readNumberField(L, index, "y", 0.0),
        readNumberField(L, index, "z", 0.0),
    };
}

void pushVec3(lua_State *L, const navycraft::Vec3d &value)
{
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, value.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, value.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, value.z);
    lua_setfield(L, -2, "z");
}

navycraft::ConstructId readId(lua_State *L, int index)
{
    if (lua_isstring(L, index)) {
        const std::string value = lua_tostring(L, index);
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("invalid construct id");
        std::size_t consumed = 0;
        const auto id = std::stoull(value, &consumed, 10);
        if (consumed != value.size() || id == 0)
            throw std::invalid_argument("invalid construct id");
        return static_cast<navycraft::ConstructId>(id);
    }
    if (lua_isnumber(L, index)) {
        const lua_Number raw = lua_tonumber(L, index);
        if (!std::isfinite(raw) || raw < 1.0 || raw > 9007199254740991.0 || std::floor(raw) != raw)
            throw std::invalid_argument("construct id number is outside exact Lua range");
        return static_cast<navycraft::ConstructId>(raw);
    }
    throw std::invalid_argument("construct id must be a string or exact integer");
}

void pushId(lua_State *L, navycraft::ConstructId id)
{
    const std::string value = std::to_string(id);
    lua_pushlstring(L, value.data(), value.size());
}

void pushConstruct(lua_State *L, const navycraft::DynamicConstruct &construct, bool include_nodes)
{
    lua_createtable(L, 0, include_nodes ? 8 : 7);
    pushId(L, construct.id());
    lua_setfield(L, -2, "id");
    lua_pushlstring(L, construct.owner().data(), construct.owner().size());
    lua_setfield(L, -2, "owner");

    pushVec3(L, construct.transform().position);
    lua_setfield(L, -2, "position");
    lua_pushnumber(L, construct.transform().yaw_radians);
    lua_setfield(L, -2, "yaw");
    pushVec3(L, construct.linearVelocity());
    lua_setfield(L, -2, "velocity");
    lua_pushnumber(L, construct.yawVelocity());
    lua_setfield(L, -2, "yaw_velocity");
    lua_pushinteger(L, static_cast<lua_Integer>(construct.nodeCount()));
    lua_setfield(L, -2, "node_count");

    if (!include_nodes)
        return;
    const auto nodes = construct.nodes();
    lua_createtable(L, static_cast<int>(nodes.size()), 0);
    int array_index = 1;
    for (const auto &entry : nodes) {
        lua_createtable(L, 0, 5);
        pushVec3(L, {static_cast<double>(entry.position.x),
            static_cast<double>(entry.position.y), static_cast<double>(entry.position.z)});
        lua_setfield(L, -2, "pos");
        lua_pushlstring(L, entry.node.node_name.data(), entry.node.node_name.size());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, entry.node.param1);
        lua_setfield(L, -2, "param1");
        lua_pushinteger(L, entry.node.param2);
        lua_setfield(L, -2, "param2");
        lua_pushlstring(L, entry.node.metadata_blob.data(), entry.node.metadata_blob.size());
        lua_setfield(L, -2, "metadata");
        lua_rawseti(L, -2, array_index++);
    }
    lua_setfield(L, -2, "nodes");
}


navycraft::LocalNodePos readLocalPos(lua_State *L, int index)
{
    const navycraft::Vec3d value = readVec3(L, index);
    return {
        static_cast<std::int32_t>(std::llround(value.x)),
        static_cast<std::int32_t>(std::llround(value.y)),
        static_cast<std::int32_t>(std::llround(value.z)),
    };
}

void pushLocalPos(lua_State *L, const navycraft::LocalNodePos &position)
{
    pushVec3(L, {static_cast<double>(position.x),
        static_cast<double>(position.y), static_cast<double>(position.z)});
}

const char *actionName(navycraft::ConstructInteractionAction action)
{
    using Action = navycraft::ConstructInteractionAction;
    switch (action) {
    case Action::StartDig: return "start_dig";
    case Action::StopDig: return "stop_dig";
    case Action::DigComplete: return "dig";
    case Action::Place: return "place";
    case Action::Use: return "use";
    case Action::Activate: return "activate";
    case Action::ReceiveFields: return "receive_fields";
    case Action::Timer: return "timer";
    }
    return "unknown";
}

void pushNodeState(lua_State *L, const navycraft::ConstructNodeState *state)
{
    lua_createtable(L, 0, 4);
    lua_createtable(L, 0, 0);
    if (state) {
        for (const auto &[key, value] : state->fields) {
            lua_pushlstring(L, value.data(), value.size());
            lua_setfield(L, -2, key.c_str());
        }
    }
    lua_setfield(L, -2, "fields");
    lua_createtable(L, 0, 0);
    if (state) {
        for (const auto &[name, list] : state->inventories) {
            lua_createtable(L, 0, 2);
            lua_pushinteger(L, list.width);
            lua_setfield(L, -2, "width");
            lua_createtable(L, static_cast<int>(list.stacks.size()), 0);
            int index = 1;
            for (const auto &stack : list.stacks) {
                lua_pushlstring(L, stack.data(), stack.size());
                lua_rawseti(L, -2, index++);
            }
            lua_setfield(L, -2, "stacks");
            lua_setfield(L, -2, name.c_str());
        }
    }
    lua_setfield(L, -2, "inventories");
    lua_createtable(L, 0, 3);
    if (state) {
        lua_pushboolean(L, state->timer.active);
        lua_setfield(L, -2, "active");
        lua_pushnumber(L, state->timer.timeout);
        lua_setfield(L, -2, "timeout");
        lua_pushnumber(L, state->timer.elapsed);
        lua_setfield(L, -2, "elapsed");
    }
    lua_setfield(L, -2, "timer");
    lua_pushnumber(L, state ? static_cast<lua_Number>(state->revision) : 0.0);
    lua_setfield(L, -2, "revision");
}

bool readBoolField(lua_State *L, int table_index, const char *name, bool fallback)
{
    getField(L, table_index, name);
    const bool value = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

int readIntField(lua_State *L, int table_index, const char *name, int fallback,
    int minimum, int maximum)
{
    const double value = readNumberField(L, table_index, name, fallback);
    if (!std::isfinite(value))
        return fallback;
    const int rounded = static_cast<int>(std::lround(value));
    return std::max(minimum, std::min(maximum, rounded));
}

void pushString(lua_State *L, const char *value)
{
    lua_pushlstring(L, value, std::char_traits<char>::length(value));
}

const char *anchorName(navycraft::ChatAnchor anchor)
{
    return anchor == navycraft::ChatAnchor::BottomLeft ? "bottom_left" : "top_left";
}

const char *fontModeName(navycraft::ChatFontMode mode)
{
    switch (mode) {
    case navycraft::ChatFontMode::Standard:
        return "standard";
    case navycraft::ChatFontMode::Mono:
        return "mono";
    case navycraft::ChatFontMode::Default:
        break;
    }
    return "default";
}

navycraft::ChatAnchor readAnchorField(lua_State *L, int table_index, const char *name,
    navycraft::ChatAnchor fallback)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return fallback;
    }
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        throw std::invalid_argument("chat anchor must be a string");
    }
    const std::string value = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (value == "top_left" || value == "top")
        return navycraft::ChatAnchor::TopLeft;
    if (value == "bottom_left" || value == "bottom")
        return navycraft::ChatAnchor::BottomLeft;
    throw std::invalid_argument("chat anchor must be top_left or bottom_left");
}

navycraft::ChatFontMode readFontModeField(lua_State *L, int table_index, const char *name,
    navycraft::ChatFontMode fallback)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return fallback;
    }
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        throw std::invalid_argument("chat font mode must be a string");
    }
    const std::string value = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (value == "default" || value == "unspecified")
        return navycraft::ChatFontMode::Default;
    if (value == "standard" || value == "regular")
        return navycraft::ChatFontMode::Standard;
    if (value == "mono" || value == "monospace")
        return navycraft::ChatFontMode::Mono;
    throw std::invalid_argument("chat font mode must be default, standard, or mono");
}

navycraft::ChatColor readColorField(lua_State *L, int table_index, const char *name,
    navycraft::ChatColor fallback)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return fallback;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        throw std::invalid_argument("chat color must be a table");
    }
    navycraft::ChatColor color = fallback;
    color.enabled = readBoolField(L, -1, "enabled", true);
    color.alpha = readIntField(L, -1, "a", readIntField(L, -1, "alpha",
        color.alpha, 0, 255), 0, 255);
    color.red = readIntField(L, -1, "r", readIntField(L, -1, "red",
        color.red, 0, 255), 0, 255);
    color.green = readIntField(L, -1, "g", readIntField(L, -1, "green",
        color.green, 0, 255), 0, 255);
    color.blue = readIntField(L, -1, "b", readIntField(L, -1, "blue",
        color.blue, 0, 255), 0, 255);
    lua_pop(L, 1);
    return color;
}

void readRecentChatStyle(lua_State *L, int table_index, navycraft::ChatStyle &style)
{
    style.recent_anchor = readAnchorField(L, table_index, "anchor", style.recent_anchor);
    style.recent_margin_left = readIntField(L, table_index, "margin_left",
        style.recent_margin_left, 0, 1000);
    style.recent_margin_right = readIntField(L, table_index, "margin_right",
        style.recent_margin_right, 0, 1000);
    style.recent_margin_top = readIntField(L, table_index, "margin_top",
        style.recent_margin_top, 0, 1000);
    style.recent_margin_bottom = readIntField(L, table_index, "margin_bottom",
        style.recent_margin_bottom, 0, 1000);
    style.recent_font_mode = readFontModeField(L, table_index, "font",
        readFontModeField(L, table_index, "font_mode", style.recent_font_mode));
    style.recent_font_size = readIntField(L, table_index, "font_size",
        style.recent_font_size, 0, 72);
    style.recent_line_spacing = readIntField(L, table_index, "line_spacing",
        style.recent_line_spacing, 0, 32);
    style.recent_text_color = readColorField(L, table_index, "text_color",
        style.recent_text_color);
}

void readConsoleChatStyle(lua_State *L, int table_index, navycraft::ChatStyle &style)
{
    style.console_anchor = readAnchorField(L, table_index, "anchor", style.console_anchor);
    style.console_margin_left = readIntField(L, table_index, "margin_left",
        style.console_margin_left, 0, 1000);
    style.console_margin_right = readIntField(L, table_index, "margin_right",
        style.console_margin_right, 0, 1000);
    style.console_margin_top = readIntField(L, table_index, "margin_top",
        style.console_margin_top, 0, 1000);
    style.console_margin_bottom = readIntField(L, table_index, "margin_bottom",
        style.console_margin_bottom, 0, 1000);
    style.console_font_mode = readFontModeField(L, table_index, "font",
        readFontModeField(L, table_index, "font_mode", style.console_font_mode));
    style.console_font_size = readIntField(L, table_index, "font_size",
        style.console_font_size, 0, 72);
    style.console_line_spacing = readIntField(L, table_index, "line_spacing",
        style.console_line_spacing, 0, 32);
    style.console_height_speed = static_cast<float>(readNumberField(L, table_index,
        "height_speed", style.console_height_speed));
    if (!std::isfinite(style.console_height_speed) || style.console_height_speed <= 0.0f)
        style.console_height_speed = 5.0f;
    style.console_text_color = readColorField(L, table_index, "text_color",
        style.console_text_color);
    style.prompt_text_color = readColorField(L, table_index, "prompt_color",
        style.prompt_text_color);
    style.console_background_color = readColorField(L, table_index, "background_color",
        style.console_background_color);
}

void pushColor(lua_State *L, const navycraft::ChatColor &color)
{
    lua_createtable(L, 0, 5);
    lua_pushboolean(L, color.enabled);
    lua_setfield(L, -2, "enabled");
    lua_pushinteger(L, color.alpha);
    lua_setfield(L, -2, "a");
    lua_pushinteger(L, color.red);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, color.green);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, color.blue);
    lua_setfield(L, -2, "b");
}

void pushRecentChatStyle(lua_State *L, const navycraft::ChatStyle &style)
{
    lua_createtable(L, 0, 9);
    pushString(L, anchorName(style.recent_anchor));
    lua_setfield(L, -2, "anchor");
    lua_pushinteger(L, style.recent_margin_left);
    lua_setfield(L, -2, "margin_left");
    lua_pushinteger(L, style.recent_margin_right);
    lua_setfield(L, -2, "margin_right");
    lua_pushinteger(L, style.recent_margin_top);
    lua_setfield(L, -2, "margin_top");
    lua_pushinteger(L, style.recent_margin_bottom);
    lua_setfield(L, -2, "margin_bottom");
    pushString(L, fontModeName(style.recent_font_mode));
    lua_setfield(L, -2, "font");
    lua_pushinteger(L, style.recent_font_size);
    lua_setfield(L, -2, "font_size");
    lua_pushinteger(L, style.recent_line_spacing);
    lua_setfield(L, -2, "line_spacing");
    pushColor(L, style.recent_text_color);
    lua_setfield(L, -2, "text_color");
}

void pushConsoleChatStyle(lua_State *L, const navycraft::ChatStyle &style)
{
    lua_createtable(L, 0, 12);
    pushString(L, anchorName(style.console_anchor));
    lua_setfield(L, -2, "anchor");
    lua_pushinteger(L, style.console_margin_left);
    lua_setfield(L, -2, "margin_left");
    lua_pushinteger(L, style.console_margin_right);
    lua_setfield(L, -2, "margin_right");
    lua_pushinteger(L, style.console_margin_top);
    lua_setfield(L, -2, "margin_top");
    lua_pushinteger(L, style.console_margin_bottom);
    lua_setfield(L, -2, "margin_bottom");
    pushString(L, fontModeName(style.console_font_mode));
    lua_setfield(L, -2, "font");
    lua_pushinteger(L, style.console_font_size);
    lua_setfield(L, -2, "font_size");
    lua_pushinteger(L, style.console_line_spacing);
    lua_setfield(L, -2, "line_spacing");
    lua_pushnumber(L, style.console_height_speed);
    lua_setfield(L, -2, "height_speed");
    pushColor(L, style.console_text_color);
    lua_setfield(L, -2, "text_color");
    pushColor(L, style.prompt_text_color);
    lua_setfield(L, -2, "prompt_color");
    pushColor(L, style.console_background_color);
    lua_setfield(L, -2, "background_color");
}

std::optional<navycraft::ConstructNode> readOptionalNodeField(
    lua_State *L, int table_index, const char *name)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        throw std::invalid_argument("construct mutation node must be a table or nil");
    }
    navycraft::ConstructNode node;
    node.node_name = readStringField(L, -1, "name");
    if (node.node_name.empty()) {
        lua_pop(L, 1);
        throw std::invalid_argument("construct mutation node name cannot be empty");
    }
    const double param1 = readNumberField(L, -1, "param1", 0.0);
    const double param2 = readNumberField(L, -1, "param2", 0.0);
    if (param1 < 0.0 || param1 > 255.0 || param2 < 0.0 || param2 > 255.0) {
        lua_pop(L, 1);
        throw std::invalid_argument("construct mutation node parameters are outside byte range");
    }
    node.param1 = static_cast<std::uint8_t>(param1);
    node.param2 = static_cast<std::uint8_t>(param2);
    node.metadata_blob = readStringField(L, -1, "metadata");
    lua_pop(L, 1);
    return node;
}

std::vector<std::string> readStringArrayField(
    lua_State *L, int table_index, const char *name)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return {};
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        throw std::invalid_argument(std::string(name) + " must be an array");
    }
    const std::size_t count = static_cast<std::size_t>(lua_objlen(L, -1));
    if (count > 65535) {
        lua_pop(L, 1);
        throw std::length_error(std::string(name) + " has too many entries");
    }
    std::vector<std::string> result;
    result.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        lua_rawgeti(L, -1, static_cast<int>(index));
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 2);
            throw std::invalid_argument(std::string(name) + " entries must be strings");
        }
        result.emplace_back(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return result;
}

std::optional<navycraft::ConstructId> readOptionalIdField(
    lua_State *L, int table_index, const char *name)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    navycraft::ConstructId value = 0;
    try {
        value = readId(L, -1);
    } catch (...) {
        lua_pop(L, 1);
        throw;
    }
    lua_pop(L, 1);
    return value;
}

std::vector<navycraft::LocalNodePos> readLocalNodeArrayField(
    lua_State *L, int table_index, const char *name)
{
    getField(L, table_index, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return {};
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        throw std::invalid_argument(std::string(name) + " must be an array");
    }
    const std::size_t count = static_cast<std::size_t>(lua_objlen(L, -1));
    if (count > navycraft::ConstructArticulationPacketCodec::MAX_NODES) {
        lua_pop(L, 1);
        throw std::length_error(std::string(name) + " has too many entries");
    }
    std::vector<navycraft::LocalNodePos> result;
    result.reserve(count);
    for (std::size_t array_index = 1; array_index <= count; ++array_index) {
        lua_rawgeti(L, -1, static_cast<int>(array_index));
        const auto value = readVec3(L, -1);
        const auto convert = [](double component) -> std::int32_t {
            if (!std::isfinite(component) || std::floor(component) != component ||
                    component < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
                    component > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
                throw std::invalid_argument("articulation node coordinate must be an integer");
            return static_cast<std::int32_t>(component);
        };
        result.push_back({convert(value.x), convert(value.y), convert(value.z)});
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return result;
}

navycraft::ConstructJointKind articulationKind(const std::string &name)
{
    if (name.empty() || name == "revolute" || name == "rotate" || name == "hinge")
        return navycraft::ConstructJointKind::Revolute;
    if (name == "prismatic" || name == "slide" || name == "linear")
        return navycraft::ConstructJointKind::Prismatic;
    throw std::invalid_argument("unknown articulation joint kind");
}

navycraft::ConstructJointControlMode articulationControlMode(const std::string &name)
{
    if (name.empty() || name == "position" || name == "target")
        return navycraft::ConstructJointControlMode::Position;
    if (name == "velocity" || name == "speed")
        return navycraft::ConstructJointControlMode::Velocity;
    if (name == "oscillate" || name == "scan")
        return navycraft::ConstructJointControlMode::Oscillate;
    throw std::invalid_argument("unknown articulation control mode");
}

void pushArticulationState(lua_State *L,
    const navycraft::ConstructArticulationState &state)
{
    lua_createtable(L, 0, 18);
    pushId(L, state.definition.id);
    lua_setfield(L, -2, "id");
    pushId(L, state.definition.construct_id);
    lua_setfield(L, -2, "construct_id");
    if (state.definition.parent_id != 0) {
        pushId(L, state.definition.parent_id);
        lua_setfield(L, -2, "parent_id");
    }
    lua_pushlstring(L, state.definition.name.data(), state.definition.name.size());
    lua_setfield(L, -2, "name");
    const char *kind = navycraft::constructJointKindName(state.definition.kind);
    lua_pushlstring(L, kind, std::char_traits<char>::length(kind));
    lua_setfield(L, -2, "kind");
    const char *mode = navycraft::constructJointControlModeName(state.definition.control_mode);
    lua_pushlstring(L, mode, std::char_traits<char>::length(mode));
    lua_setfield(L, -2, "control_mode");
    pushVec3(L, state.definition.pivot_local);
    lua_setfield(L, -2, "pivot");
    pushVec3(L, state.definition.axis_local);
    lua_setfield(L, -2, "axis");
    lua_pushnumber(L, state.definition.minimum_position);
    lua_setfield(L, -2, "minimum");
    lua_pushnumber(L, state.definition.maximum_position);
    lua_setfield(L, -2, "maximum");
    lua_pushnumber(L, state.definition.maximum_speed);
    lua_setfield(L, -2, "maximum_speed");
    lua_pushnumber(L, state.definition.maximum_acceleration);
    lua_setfield(L, -2, "maximum_acceleration");
    lua_pushnumber(L, state.position);
    lua_setfield(L, -2, "position");
    lua_pushnumber(L, state.target_position);
    lua_setfield(L, -2, "target_position");
    lua_pushnumber(L, state.velocity);
    lua_setfield(L, -2, "velocity");
    lua_pushboolean(L, state.definition.enabled);
    lua_setfield(L, -2, "enabled");
    lua_pushboolean(L, state.definition.render_enabled);
    lua_setfield(L, -2, "render_enabled");
    lua_pushboolean(L, state.definition.collision_enabled);
    lua_setfield(L, -2, "collision_enabled");
    lua_pushnumber(L, static_cast<lua_Number>(state.revision));
    lua_setfield(L, -2, "revision");
}

void pushArticulationObstruction(lua_State *L,
    const navycraft::ConstructArticulationObstruction &obstruction)
{
    lua_createtable(L, 0, 7);
    pushId(L, obstruction.construct_id);
    lua_setfield(L, -2, "construct_id");
    pushId(L, obstruction.articulation_id);
    lua_setfield(L, -2, "articulation_id");
    pushLocalPos(L, obstruction.node_position);
    lua_setfield(L, -2, "node_pos");
    pushVec3(L, obstruction.local_point);
    lua_setfield(L, -2, "local_point");
    pushVec3(L, obstruction.world_point);
    lua_setfield(L, -2, "world_point");
    lua_pushnumber(L, obstruction.distance);
    lua_setfield(L, -2, "distance");
}

void pushTurretStatus(lua_State *L, const navycraft::ConstructTurretStatus &status)
{
    lua_createtable(L, 0, 18);
    pushId(L, status.definition.id);
    lua_setfield(L, -2, "id");
    pushId(L, status.definition.construct_id);
    lua_setfield(L, -2, "construct_id");
    pushId(L, status.definition.yaw_joint_id);
    lua_setfield(L, -2, "yaw_joint_id");
    if (status.definition.pitch_joint_id != 0) {
        pushId(L, status.definition.pitch_joint_id);
        lua_setfield(L, -2, "pitch_joint_id");
    }
    lua_pushlstring(L, status.definition.name.data(), status.definition.name.size());
    lua_setfield(L, -2, "name");
    pushVec3(L, status.target_world);
    lua_setfield(L, -2, "target");
    pushVec3(L, status.muzzle_world);
    lua_setfield(L, -2, "muzzle");
    pushVec3(L, status.forward_world);
    lua_setfield(L, -2, "forward");
    lua_pushnumber(L, status.desired_yaw);
    lua_setfield(L, -2, "desired_yaw");
    lua_pushnumber(L, status.desired_pitch);
    lua_setfield(L, -2, "desired_pitch");
    lua_pushnumber(L, status.current_yaw);
    lua_setfield(L, -2, "current_yaw");
    lua_pushnumber(L, status.current_pitch);
    lua_setfield(L, -2, "current_pitch");
    lua_pushnumber(L, status.angle_error);
    lua_setfield(L, -2, "angle_error");
    lua_pushboolean(L, status.has_target);
    lua_setfield(L, -2, "has_target");
    lua_pushboolean(L, status.aligned);
    lua_setfield(L, -2, "aligned");
    lua_pushboolean(L, status.obstructed);
    lua_setfield(L, -2, "obstructed");
    if (status.obstruction) {
        pushArticulationObstruction(L, *status.obstruction);
        lua_setfield(L, -2, "obstruction");
    }
}

void pushMutationAction(lua_State *L, const navycraft::ConstructDatabaseAction &action)
{
    lua_createtable(L, 0, 12);
    lua_pushnumber(L, static_cast<lua_Number>(action.action_id));
    lua_setfield(L, -2, "action_id");
    pushId(L, action.mutation.construct_id);
    lua_setfield(L, -2, "construct_id");
    lua_pushlstring(L, action.mutation.actor.data(), action.mutation.actor.size());
    lua_setfield(L, -2, "actor");
    const char *name = actionName(action.mutation.action);
    lua_pushlstring(L, name, std::char_traits<char>::length(name));
    lua_setfield(L, -2, "action");
    pushLocalPos(L, action.mutation.position);
    lua_setfield(L, -2, "node_pos");
    lua_pushboolean(L, action.mutation.accepted);
    lua_setfield(L, -2, "accepted");
    lua_pushboolean(L, action.mutation.protected_violation);
    lua_setfield(L, -2, "protected_violation");
    lua_pushlstring(L, action.mutation.wielded_item_before.data(),
        action.mutation.wielded_item_before.size());
    lua_setfield(L, -2, "item_before");
    lua_pushlstring(L, action.mutation.wielded_item_after.data(),
        action.mutation.wielded_item_after.size());
    lua_setfield(L, -2, "item_after");
    lua_pushlstring(L, action.mutation.reason.data(), action.mutation.reason.size());
    lua_setfield(L, -2, "reason");
    lua_pushnumber(L, static_cast<lua_Number>(action.created_at));
    lua_setfield(L, -2, "created_at");
    lua_createtable(L, static_cast<int>(action.mutation.drops.size()), 0);
    int drop_index = 1;
    for (const auto &drop : action.mutation.drops) {
        lua_pushlstring(L, drop.data(), drop.size());
        lua_rawseti(L, -2, drop_index++);
    }
    lua_setfield(L, -2, "drops");
}



navycraft::ConstructStructuralConfig readStructuralConfig(lua_State *L, int index)
{
    navycraft::ConstructStructuralConfig config;
    if (!lua_istable(L, index))
        return config;
    config.water_level = readNumberField(L, index, "water_level", config.water_level);
    config.gravity = readNumberField(L, index, "gravity", config.gravity);
    config.fluid_density = readNumberField(L, index, "fluid_density", config.fluid_density);
    config.flooding_rate = readNumberField(L, index, "flooding_rate", config.flooding_rate);
    config.linear_water_drag = readNumberField(L, index, "linear_water_drag",
        config.linear_water_drag);
    config.angular_water_drag = readNumberField(L, index, "angular_water_drag",
        config.angular_water_drag);
    config.maximum_vertical_acceleration = readNumberField(L, index,
        "maximum_vertical_acceleration", config.maximum_vertical_acceleration);
    config.sink_depth = readNumberField(L, index, "sink_depth", config.sink_depth);
    const double minimum_nodes = readNumberField(L, index, "minimum_fragment_nodes",
        static_cast<double>(config.minimum_fragment_nodes));
    const double maximum_cells = readNumberField(L, index, "maximum_enclosure_cells",
        static_cast<double>(config.maximum_enclosure_cells));
    if (minimum_nodes < 1.0 || maximum_cells < 1.0)
        throw std::invalid_argument("structural size limits must be positive");
    config.minimum_fragment_nodes = static_cast<std::size_t>(minimum_nodes);
    config.maximum_enclosure_cells = static_cast<std::size_t>(maximum_cells);
    config.split_enabled = readBoolField(L, index, "split_enabled", config.split_enabled);
    config.apply_fragment_physics = readBoolField(L, index, "apply_fragment_physics",
        config.apply_fragment_physics);
    config.apply_primary_physics = readBoolField(L, index, "apply_primary_physics",
        config.apply_primary_physics);
    config.recenter_fragments = readBoolField(L, index, "recenter_fragments",
        config.recenter_fragments);
    return config;
}

void pushStructuralState(lua_State *L, const navycraft::ConstructStructuralState &state)
{
    lua_createtable(L, 0, 23);
    pushId(L, state.construct_id);
    lua_setfield(L, -2, "construct_id");
    pushId(L, state.source_construct_id);
    lua_setfield(L, -2, "source_id");
    const char *role = navycraft::constructStructuralRoleName(state.role);
    lua_pushlstring(L, role, std::char_traits<char>::length(role));
    lua_setfield(L, -2, "role");
    lua_pushnumber(L, static_cast<lua_Number>(state.node_revision));
    lua_setfield(L, -2, "node_revision");
    lua_pushinteger(L, static_cast<lua_Integer>(state.component_count));
    lua_setfield(L, -2, "component_count");
    lua_pushinteger(L, static_cast<lua_Integer>(state.node_count));
    lua_setfield(L, -2, "node_count");
    lua_pushinteger(L, static_cast<lua_Integer>(state.control_nodes));
    lua_setfield(L, -2, "control_nodes");
    lua_pushinteger(L, static_cast<lua_Integer>(state.breach_count));
    lua_setfield(L, -2, "breach_count");
    lua_pushnumber(L, state.total_mass);
    lua_setfield(L, -2, "mass");
    lua_pushnumber(L, state.solid_displaced_volume);
    lua_setfield(L, -2, "solid_volume");
    lua_pushnumber(L, state.enclosed_volume);
    lua_setfield(L, -2, "enclosed_volume");
    lua_pushnumber(L, state.submerged_volume);
    lua_setfield(L, -2, "submerged_volume");
    lua_pushnumber(L, state.effective_displaced_volume);
    lua_setfield(L, -2, "effective_displacement");
    lua_pushnumber(L, state.flooded_fraction);
    lua_setfield(L, -2, "flooded_fraction");
    lua_pushnumber(L, state.integrity);
    lua_setfield(L, -2, "integrity");
    lua_pushnumber(L, state.buoyancy_force);
    lua_setfield(L, -2, "buoyancy_force");
    lua_pushnumber(L, state.weight_force);
    lua_setfield(L, -2, "weight_force");
    lua_pushnumber(L, state.vertical_acceleration);
    lua_setfield(L, -2, "vertical_acceleration");
    lua_pushboolean(L, state.afloat);
    lua_setfield(L, -2, "afloat");
    lua_pushboolean(L, state.sinking);
    lua_setfield(L, -2, "sinking");
    lua_pushboolean(L, state.sunk);
    lua_setfield(L, -2, "sunk");
}

void pushStructuralEvent(lua_State *L, const navycraft::ConstructStructuralEvent &event)
{
    lua_createtable(L, 0, 7);
    const char *kind = navycraft::constructStructuralEventName(event.kind);
    lua_pushlstring(L, kind, std::char_traits<char>::length(kind));
    lua_setfield(L, -2, "event");
    pushId(L, event.construct_id);
    lua_setfield(L, -2, "construct_id");
    pushId(L, event.source_construct_id);
    lua_setfield(L, -2, "source_id");
    lua_pushinteger(L, static_cast<lua_Integer>(event.moved_nodes));
    lua_setfield(L, -2, "moved_nodes");
    lua_createtable(L, static_cast<int>(event.fragment_ids.size()), 0);
    int index = 1;
    for (const auto id : event.fragment_ids) {
        pushId(L, id);
        lua_rawseti(L, -2, index++);
    }
    lua_setfield(L, -2, "fragment_ids");
    pushStructuralState(L, event.state);
    lua_setfield(L, -2, "state");
}

navycraft::ConstructProjectileKind projectileKind(const std::string &name)
{
    using Kind = navycraft::ConstructProjectileKind;
    if (name == "shell" || name == "cannon") return Kind::Shell;
    if (name == "fireball") return Kind::Fireball;
    if (name == "torpedo") return Kind::Torpedo;
    if (name == "depth_charge") return Kind::DepthCharge;
    if (name == "bomb") return Kind::Bomb;
    if (name == "aa" || name == "anti_aircraft") return Kind::AntiAircraft;
    throw std::invalid_argument("unknown construct projectile kind");
}

const char *projectileKindName(navycraft::ConstructProjectileKind kind)
{
    using Kind = navycraft::ConstructProjectileKind;
    switch (kind) {
    case Kind::Shell: return "shell";
    case Kind::Fireball: return "fireball";
    case Kind::Torpedo: return "torpedo";
    case Kind::DepthCharge: return "depth_charge";
    case Kind::Bomb: return "bomb";
    case Kind::AntiAircraft: return "aa";
    }
    return "shell";
}

const char *impactKindName(navycraft::ConstructImpactKind kind)
{
    using Kind = navycraft::ConstructImpactKind;
    switch (kind) {
    case Kind::None: return "none";
    case Kind::Construct: return "construct";
    case Kind::Terrain: return "terrain";
    case Kind::Water: return "water";
    case Kind::Expired: return "expired";
    }
    return "none";
}

void pushProjectileState(lua_State *L, const navycraft::ConstructProjectileState &state)
{
    lua_createtable(L, 0, 17);
    pushId(L, state.id);
    lua_setfield(L, -2, "id");
    lua_pushnumber(L, static_cast<lua_Number>(state.sequence));
    lua_setfield(L, -2, "sequence");
    pushId(L, state.source_construct_id);
    lua_setfield(L, -2, "source_id");
    if (state.target_construct_id != 0) {
        pushId(L, state.target_construct_id);
        lua_setfield(L, -2, "target_id");
    }
    lua_pushlstring(L, state.owner.data(), state.owner.size());
    lua_setfield(L, -2, "owner");
    const char *kind = projectileKindName(state.spec.kind);
    lua_pushlstring(L, kind, std::char_traits<char>::length(kind));
    lua_setfield(L, -2, "kind");
    pushVec3(L, state.position);
    lua_setfield(L, -2, "position");
    pushVec3(L, state.previous_position);
    lua_setfield(L, -2, "previous_position");
    pushVec3(L, state.velocity);
    lua_setfield(L, -2, "velocity");
    lua_pushnumber(L, state.age);
    lua_setfield(L, -2, "age");
    lua_pushnumber(L, state.distance_travelled);
    lua_setfield(L, -2, "distance");
    lua_pushboolean(L, state.armed);
    lua_setfield(L, -2, "armed");
    lua_pushnumber(L, state.spec.blast_radius);
    lua_setfield(L, -2, "blast_radius");
    lua_pushnumber(L, state.spec.blast_power);
    lua_setfield(L, -2, "blast_power");
    lua_pushnumber(L, state.spec.penetration);
    lua_setfield(L, -2, "penetration");
    lua_pushboolean(L, state.spec.guided);
    lua_setfield(L, -2, "guided");
    lua_pushboolean(L, state.spec.requires_water);
    lua_setfield(L, -2, "requires_water");
}

void pushExplosion(lua_State *L, const navycraft::ConstructExplosionResult &result)
{
    lua_createtable(L, 0, 6);
    pushVec3(L, result.world_position);
    lua_setfield(L, -2, "position");
    lua_pushinteger(L, static_cast<lua_Integer>(result.destroyed_nodes));
    lua_setfield(L, -2, "destroyed_nodes");
    lua_pushinteger(L, static_cast<lua_Integer>(result.breaches));
    lua_setfield(L, -2, "breaches");
    lua_createtable(L, static_cast<int>(result.affected_constructs.size()), 0);
    int affected_index = 1;
    for (const auto id : result.affected_constructs) {
        pushId(L, id);
        lua_rawseti(L, -2, affected_index++);
    }
    lua_setfield(L, -2, "affected_constructs");
    lua_createtable(L, static_cast<int>(result.node_damage.size()), 0);
    int damage_index = 1;
    for (const auto &damage : result.node_damage) {
        lua_createtable(L, 0, 9);
        pushId(L, damage.construct_id);
        lua_setfield(L, -2, "construct_id");
        pushLocalPos(L, damage.node_position);
        lua_setfield(L, -2, "node_pos");
        lua_pushlstring(L, damage.node_name.data(), damage.node_name.size());
        lua_setfield(L, -2, "node_name");
        lua_pushnumber(L, damage.distance);
        lua_setfield(L, -2, "distance");
        lua_pushnumber(L, damage.effective_power);
        lua_setfield(L, -2, "power");
        lua_pushnumber(L, damage.armour);
        lua_setfield(L, -2, "armour");
        lua_pushboolean(L, damage.destroyed);
        lua_setfield(L, -2, "destroyed");
        lua_pushboolean(L, damage.breached);
        lua_setfield(L, -2, "breached");
        lua_rawseti(L, -2, damage_index++);
    }
    lua_setfield(L, -2, "node_damage");
}

void pushProjectileEvent(lua_State *L, const navycraft::ConstructProjectileEvent &event)
{
    lua_createtable(L, 0, 4);
    const char *event_name = "update";
    switch (event.kind) {
    case navycraft::ConstructProjectileEventKind::Spawn: event_name = "spawn"; break;
    case navycraft::ConstructProjectileEventKind::Update: event_name = "update"; break;
    case navycraft::ConstructProjectileEventKind::Impact: event_name = "impact"; break;
    case navycraft::ConstructProjectileEventKind::Remove: event_name = "remove"; break;
    }
    lua_pushlstring(L, event_name, std::char_traits<char>::length(event_name));
    lua_setfield(L, -2, "event");
    pushProjectileState(L, event.projectile);
    lua_setfield(L, -2, "projectile");
    if (event.impact) {
        lua_createtable(L, 0, 8);
        const char *impact_name = impactKindName(event.impact->kind);
        lua_pushlstring(L, impact_name, std::char_traits<char>::length(impact_name));
        lua_setfield(L, -2, "kind");
        pushVec3(L, event.impact->world_position);
        lua_setfield(L, -2, "position");
        pushVec3(L, event.impact->world_normal);
        lua_setfield(L, -2, "normal");
        if (event.impact->construct_id != 0) {
            pushId(L, event.impact->construct_id);
            lua_setfield(L, -2, "construct_id");
            pushLocalPos(L, event.impact->node_position);
            lua_setfield(L, -2, "node_pos");
        }
        pushExplosion(L, event.impact->explosion);
        lua_setfield(L, -2, "explosion");
        lua_setfield(L, -2, "impact");
    }
}

navycraft::FireControlTargetClass fireControlTargetClass(const std::string &name)
{
    using Class = navycraft::FireControlTargetClass;
    if (name.empty() || name == "unknown") return Class::Unknown;
    if (name == "surface" || name == "ship" || name == "boat") return Class::Surface;
    if (name == "submarine" || name == "sub") return Class::Submarine;
    if (name == "air" || name == "aircraft" || name == "airship") return Class::Air;
    if (name == "ground" || name == "tank") return Class::Ground;
    throw std::invalid_argument("unknown fire-control target class");
}

const char *fireControlTargetClassName(navycraft::FireControlTargetClass value)
{
    using Class = navycraft::FireControlTargetClass;
    switch (value) {
    case Class::Unknown: return "unknown";
    case Class::Surface: return "surface";
    case Class::Submarine: return "submarine";
    case Class::Air: return "air";
    case Class::Ground: return "ground";
    }
    return "unknown";
}

navycraft::FireControlSensorKind fireControlSensorKind(const std::string &name)
{
    using Sensor = navycraft::FireControlSensorKind;
    if (name.empty() || name == "visual") return Sensor::Visual;
    if (name == "radar") return Sensor::Radar;
    if (name == "passive_sonar" || name == "passive") return Sensor::PassiveSonar;
    if (name == "active_sonar" || name == "active") return Sensor::ActiveSonar;
    if (name == "data_link" || name == "datalink") return Sensor::DataLink;
    throw std::invalid_argument("unknown fire-control sensor kind");
}

const char *fireControlSensorName(navycraft::FireControlSensorKind value)
{
    using Sensor = navycraft::FireControlSensorKind;
    switch (value) {
    case Sensor::Visual: return "visual";
    case Sensor::Radar: return "radar";
    case Sensor::PassiveSonar: return "passive_sonar";
    case Sensor::ActiveSonar: return "active_sonar";
    case Sensor::DataLink: return "data_link";
    }
    return "visual";
}

navycraft::FireControlBatteryMode fireControlBatteryMode(const std::string &name)
{
    using Mode = navycraft::FireControlBatteryMode;
    if (name.empty() || name == "manual") return Mode::Manual;
    if (name == "automatic" || name == "auto") return Mode::Automatic;
    if (name == "defensive" || name == "defence") return Mode::Defensive;
    throw std::invalid_argument("unknown fire-control battery mode");
}

const char *fireControlBatteryModeName(navycraft::FireControlBatteryMode value)
{
    using Mode = navycraft::FireControlBatteryMode;
    switch (value) {
    case Mode::Manual: return "manual";
    case Mode::Automatic: return "automatic";
    case Mode::Defensive: return "defensive";
    }
    return "manual";
}

navycraft::FireControlWeaponSpec readFireControlWeaponSpec(lua_State *L, int table_index)
{
    navycraft::FireControlWeaponSpec weapon;
    const std::string kind = readStringField(L, table_index, "kind");
    if (!kind.empty())
        weapon.projectile_kind = projectileKind(kind);
    weapon.muzzle_speed = readNumberField(L, table_index, "muzzle_speed",
        readNumberField(L, table_index, "speed", weapon.muzzle_speed));
    weapon.gravity = readNumberField(L, table_index, "gravity", weapon.gravity);
    weapon.drag = readNumberField(L, table_index, "drag", weapon.drag);
    weapon.minimum_range = readNumberField(L, table_index, "minimum_range",
        weapon.minimum_range);
    weapon.maximum_range = readNumberField(L, table_index, "maximum_range",
        readNumberField(L, table_index, "range", weapon.maximum_range));
    weapon.maximum_lead_time = readNumberField(L, table_index, "maximum_lead_time",
        weapon.maximum_lead_time);
    weapon.minimum_pitch_radians = readNumberField(L, table_index, "minimum_pitch",
        weapon.minimum_pitch_radians);
    weapon.maximum_pitch_radians = readNumberField(L, table_index, "maximum_pitch",
        weapon.maximum_pitch_radians);
    weapon.preferred_depth = readNumberField(L, table_index, "preferred_depth",
        weapon.preferred_depth);
    weapon.guidance_turn_rate = readNumberField(L, table_index, "guidance_turn_rate",
        weapon.guidance_turn_rate);
    weapon.guided = readBoolField(L, table_index, "guided", weapon.guided);
    const std::string arc = readStringField(L, table_index, "arc");
    if (arc == "high")
        weapon.arc_preference = navycraft::FireControlArcPreference::High;
    else if (!arc.empty() && arc != "low")
        throw std::invalid_argument("fire-control arc must be low or high");
    return weapon;
}

void pushFireControlTrack(lua_State *L, const navycraft::FireControlTrack &track)
{
    lua_createtable(L, 0, 11);
    pushId(L, track.observer_construct_id);
    lua_setfield(L, -2, "observer_id");
    pushId(L, track.target_construct_id);
    lua_setfield(L, -2, "target_id");
    lua_pushnumber(L, track.sample_time);
    lua_setfield(L, -2, "sample_time");
    pushVec3(L, track.position);
    lua_setfield(L, -2, "position");
    pushVec3(L, track.velocity);
    lua_setfield(L, -2, "velocity");
    pushVec3(L, track.acceleration);
    lua_setfield(L, -2, "acceleration");
    const char *classification = fireControlTargetClassName(track.classification);
    lua_pushlstring(L, classification, std::char_traits<char>::length(classification));
    lua_setfield(L, -2, "classification");
    const char *sensor = fireControlSensorName(track.sensor);
    lua_pushlstring(L, sensor, std::char_traits<char>::length(sensor));
    lua_setfield(L, -2, "sensor");
    lua_pushnumber(L, track.confidence);
    lua_setfield(L, -2, "confidence");
    lua_pushinteger(L, static_cast<lua_Integer>(track.sample_count));
    lua_setfield(L, -2, "sample_count");
}

void pushFireControlSolution(lua_State *L, const navycraft::FireControlSolution &solution)
{
    lua_createtable(L, 0, 16);
    lua_pushboolean(L, solution.valid);
    lua_setfield(L, -2, "valid");
    pushId(L, solution.shooter_construct_id);
    lua_setfield(L, -2, "shooter_id");
    pushId(L, solution.target_construct_id);
    lua_setfield(L, -2, "target_id");
    pushVec3(L, solution.muzzle_world_position);
    lua_setfield(L, -2, "muzzle_position");
    pushVec3(L, solution.aim_point);
    lua_setfield(L, -2, "aim_point");
    pushVec3(L, solution.launch_velocity);
    lua_setfield(L, -2, "launch_velocity");
    lua_pushnumber(L, solution.yaw_radians);
    lua_setfield(L, -2, "yaw");
    lua_pushnumber(L, solution.pitch_radians);
    lua_setfield(L, -2, "pitch");
    lua_pushnumber(L, solution.intercept_time);
    lua_setfield(L, -2, "intercept_time");
    lua_pushnumber(L, solution.range);
    lua_setfield(L, -2, "range");
    lua_pushnumber(L, solution.closure_rate);
    lua_setfield(L, -2, "closure_rate");
    lua_pushnumber(L, solution.estimated_miss_distance);
    lua_setfield(L, -2, "miss_distance");
    lua_pushnumber(L, solution.track_confidence);
    lua_setfield(L, -2, "track_confidence");
    lua_pushlstring(L, solution.reason.data(), solution.reason.size());
    lua_setfield(L, -2, "reason");
}


navycraft::ConstructNavigationMode navigationMode(const std::string &name)
{
    using Mode = navycraft::ConstructNavigationMode;
    if (name.empty() || name == "manual") return Mode::Manual;
    if (name == "hold" || name == "station") return Mode::Hold;
    if (name == "route" || name == "waypoint" || name == "autotravel") return Mode::Route;
    if (name == "formation" || name == "follow") return Mode::Formation;
    throw std::invalid_argument("unknown navigation mode");
}

const char *navigationModeName(navycraft::ConstructNavigationMode value)
{
    using Mode = navycraft::ConstructNavigationMode;
    switch (value) {
    case Mode::Manual: return "manual";
    case Mode::Hold: return "hold";
    case Mode::Route: return "route";
    case Mode::Formation: return "formation";
    }
    return "manual";
}

navycraft::ConstructNavigationDomain navigationDomain(const std::string &name)
{
    using Domain = navycraft::ConstructNavigationDomain;
    if (name.empty() || name == "surface" || name == "ship") return Domain::Surface;
    if (name == "submersible" || name == "submarine" || name == "sub")
        return Domain::Submersible;
    if (name == "air" || name == "aircraft" || name == "airship") return Domain::Air;
    if (name == "ground" || name == "tank") return Domain::Ground;
    throw std::invalid_argument("unknown navigation domain");
}

const char *navigationDomainName(navycraft::ConstructNavigationDomain value)
{
    using Domain = navycraft::ConstructNavigationDomain;
    switch (value) {
    case Domain::Surface: return "surface";
    case Domain::Submersible: return "submersible";
    case Domain::Air: return "air";
    case Domain::Ground: return "ground";
    }
    return "surface";
}

void pushNavigationCommand(lua_State *L,
    const navycraft::ConstructNavigationCommand &command)
{
    lua_createtable(L, 0, 18);
    pushId(L, command.construct_id);
    lua_setfield(L, -2, "construct_id");
    lua_pushnumber(L, static_cast<lua_Number>(command.sequence));
    lua_setfield(L, -2, "sequence");
    const char *mode = navigationModeName(command.mode);
    lua_pushlstring(L, mode, std::char_traits<char>::length(mode));
    lua_setfield(L, -2, "mode");
    pushVec3(L, command.target_position);
    lua_setfield(L, -2, "target_position");
    pushVec3(L, command.desired_velocity);
    lua_setfield(L, -2, "desired_velocity");
    lua_pushnumber(L, command.desired_yaw_radians);
    lua_setfield(L, -2, "desired_yaw");
    lua_pushnumber(L, command.yaw_rate);
    lua_setfield(L, -2, "yaw_rate");
    lua_pushnumber(L, command.forward_speed);
    lua_setfield(L, -2, "forward_speed");
    lua_pushnumber(L, command.vertical_speed);
    lua_setfield(L, -2, "vertical_speed");
    lua_pushnumber(L, command.distance_to_target);
    lua_setfield(L, -2, "distance");
    lua_pushnumber(L, command.cross_track_error);
    lua_setfield(L, -2, "avoidance");
    lua_pushinteger(L, static_cast<lua_Integer>(command.waypoint_index + 1));
    lua_setfield(L, -2, "waypoint_index");
    if (command.avoidance_obstacle_id != 0) {
        lua_pushnumber(L, static_cast<lua_Number>(command.avoidance_obstacle_id));
        lua_setfield(L, -2, "obstacle_id");
    }
    lua_pushboolean(L, command.avoiding);
    lua_setfield(L, -2, "avoiding");
    lua_pushboolean(L, command.arrived);
    lua_setfield(L, -2, "arrived");
    lua_pushboolean(L, command.completed);
    lua_setfield(L, -2, "completed");
    lua_pushboolean(L, command.recovering);
    lua_setfield(L, -2, "recovering");
    lua_pushlstring(L, command.reason.data(), command.reason.size());
    lua_setfield(L, -2, "reason");
}

void pushNavigationStatus(lua_State *L,
    const navycraft::ConstructNavigationStatus &status)
{
    lua_createtable(L, 0, 16);
    pushId(L, status.config.construct_id);
    lua_setfield(L, -2, "construct_id");
    const char *mode = navigationModeName(status.config.mode);
    lua_pushlstring(L, mode, std::char_traits<char>::length(mode));
    lua_setfield(L, -2, "mode");
    const char *domain = navigationDomainName(status.config.domain);
    lua_pushlstring(L, domain, std::char_traits<char>::length(domain));
    lua_setfield(L, -2, "domain");
    lua_pushboolean(L, status.enabled);
    lua_setfield(L, -2, "enabled");
    lua_pushboolean(L, status.completed);
    lua_setfield(L, -2, "completed");
    lua_pushinteger(L, static_cast<lua_Integer>(status.waypoint_index + 1));
    lua_setfield(L, -2, "waypoint_index");
    lua_pushinteger(L, static_cast<lua_Integer>(status.route.size()));
    lua_setfield(L, -2, "waypoint_count");
    lua_pushnumber(L, status.stuck_time);
    lua_setfield(L, -2, "stuck_time");
    lua_pushnumber(L, status.recovery_remaining);
    lua_setfield(L, -2, "recovery_remaining");
    pushVec3(L, status.hold_position);
    lua_setfield(L, -2, "hold_position");
    if (status.config.formation_leader_id != 0) {
        pushId(L, status.config.formation_leader_id);
        lua_setfield(L, -2, "leader_id");
    }
    pushVec3(L, status.config.formation_local_offset);
    lua_setfield(L, -2, "formation_offset");
    lua_pushnumber(L, status.config.maximum_speed);
    lua_setfield(L, -2, "maximum_speed");
    lua_pushboolean(L, status.config.route_loop);
    lua_setfield(L, -2, "loop");
    pushNavigationCommand(L, status.last_command);
    lua_setfield(L, -2, "command");
}

void pushFireControlBattery(lua_State *L, const navycraft::FireControlBattery &battery)
{
    lua_createtable(L, 0, 11);
    pushId(L, battery.id);
    lua_setfield(L, -2, "id");
    pushId(L, battery.shooter_construct_id);
    lua_setfield(L, -2, "shooter_id");
    pushVec3(L, battery.muzzle_local_position);
    lua_setfield(L, -2, "muzzle_local");
    const char *mode = fireControlBatteryModeName(battery.mode);
    lua_pushlstring(L, mode, std::char_traits<char>::length(mode));
    lua_setfield(L, -2, "mode");
    lua_pushnumber(L, static_cast<lua_Number>(battery.allowed_class_mask));
    lua_setfield(L, -2, "allowed_class_mask");
    if (battery.designated_target_id != 0) {
        pushId(L, battery.designated_target_id);
        lua_setfield(L, -2, "target_id");
    }
    lua_pushnumber(L, battery.minimum_track_confidence);
    lua_setfield(L, -2, "minimum_confidence");
    lua_pushnumber(L, battery.reload_seconds);
    lua_setfield(L, -2, "reload_seconds");
    lua_pushnumber(L, battery.next_ready_time);
    lua_setfield(L, -2, "next_ready_time");
    lua_pushboolean(L, battery.enabled);
    lua_setfield(L, -2, "enabled");
}

int fail(lua_State *L, const std::string &message)
{
    lua_pushnil(L);
    lua_pushlstring(L, message.data(), message.size());
    return 2;
}
}

int ModApiNavyCraft::l_create_dynamic_construct(lua_State *L)
{
    std::shared_ptr<navycraft::DynamicConstruct> construct;
    try {
        if (!lua_istable(L, 1))
            throw std::invalid_argument("construct definition must be a table");
        getField(L, 1, "origin");
        const navycraft::Vec3d origin = readVec3(L, -1);
        lua_pop(L, 1);

        getField(L, 1, "nodes");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("construct nodes must be an array");
        }
        const std::size_t count = static_cast<std::size_t>(lua_objlen(L, -1));
        if (count == 0 || count > MAX_LUA_CONSTRUCT_NODES) {
            lua_pop(L, 1);
            throw std::invalid_argument("construct node count is outside allowed range");
        }

        construct = navycraft::runtimeConstructRegistry().create();
        construct->setOwner(readStringField(L, 1, "owner"));
        construct->setTransform({origin, readNumberField(L, 1, "yaw", 0.0)});

        for (std::size_t index = 1; index <= count; ++index) {
            lua_rawgeti(L, -1, static_cast<int>(index));
            if (!lua_istable(L, -1))
                throw std::invalid_argument("construct node entry must be a table");
            getField(L, -1, "pos");
            const navycraft::Vec3d world = readVec3(L, -1);
            lua_pop(L, 1);
            navycraft::LocalNodePos local{
                static_cast<std::int32_t>(std::llround(world.x - origin.x)),
                static_cast<std::int32_t>(std::llround(world.y - origin.y)),
                static_cast<std::int32_t>(std::llround(world.z - origin.z)),
            };
            navycraft::ConstructNode node;
            node.node_name = readStringField(L, -1, "name");
            if (node.node_name.empty())
                throw std::invalid_argument("construct node name cannot be empty");
            node.param1 = static_cast<std::uint8_t>(readNumberField(L, -1, "param1", 0.0));
            node.param2 = static_cast<std::uint8_t>(readNumberField(L, -1, "param2", 0.0));
            node.metadata_blob = readStringField(L, -1, "metadata");
            construct->setNode(local, std::move(node));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        broadcastFullConstruct(getServer(L), *construct);
        pushId(L, construct->id());
        return 1;
    } catch (const std::exception &error) {
        if (construct) {
            navycraft::runtimeConstructRegistry().remove(construct->id());
            navycraft::clearRuntimeConstructState(construct->id());
        }
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const bool include_nodes = lua_toboolean(L, 2) != 0;
        pushConstruct(L, *construct, include_nodes);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_transform(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        if (!lua_istable(L, 2))
            throw std::invalid_argument("transform must be a table");
        auto transform = construct->transform();
        getField(L, 2, "position");
        if (lua_istable(L, -1))
            transform.position = readVec3(L, -1);
        lua_pop(L, 1);
        transform.yaw_radians = readNumberField(L, 2, "yaw", transform.yaw_radians);
        construct->setTransform(transform);
        broadcastTransform(getServer(L), *construct);
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_velocity(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        if (!lua_istable(L, 2))
            throw std::invalid_argument("velocity must be a vector table");
        construct->setLinearVelocity(readVec3(L, 2));
        const double yaw_velocity = lua_gettop(L) >= 3 && lua_isnumber(L, 3) ?
            lua_tonumber(L, 3) : 0.0;
        construct->setYawVelocity(yaw_velocity);
        broadcastTransform(getServer(L), *construct);
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_remove_dynamic_construct(lua_State *L)
{
    try {
        const auto id = readId(L, 1);
        const bool removed = navycraft::runtimeConstructRegistry().remove(id);
        if (removed) {
            navycraft::clearRuntimeConstructState(id);
            broadcastMessage(getServer(L), {navycraft::ConstructWireKind::Remove, id,
                navycraft::ConstructPacketCodec::encodeRemove(id), true});
        }
        lua_pushboolean(L, removed);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_list_dynamic_constructs(lua_State *L)
{
    const auto constructs = navycraft::runtimeConstructRegistry().snapshot();
    lua_createtable(L, static_cast<int>(constructs.size()), 0);
    int index = 1;
    for (const auto &construct : constructs) {
        pushConstruct(L, *construct, false);
        lua_rawseti(L, -2, index++);
    }
    return 1;
}


int ModApiNavyCraft::l_raycast_dynamic_constructs(lua_State *L)
{
    try {
        const navycraft::Vec3d start = readVec3(L, 1);
        const navycraft::Vec3d end = readVec3(L, 2);
        std::optional<navycraft::ConstructRaycastHit> closest;
        for (const auto &construct : navycraft::runtimeConstructRegistry().snapshot()) {
            const auto hit = navycraft::ConstructGeometry::raycast(*construct, start, end);
            if (hit && (!closest || hit->distance < closest->distance))
                closest = hit;
        }
        if (!closest) {
            lua_pushnil(L);
            return 1;
        }
        lua_createtable(L, 0, 6);
        pushId(L, closest->construct_id);
        lua_setfield(L, -2, "id");
        pushVec3(L, {static_cast<double>(closest->node_position.x),
            static_cast<double>(closest->node_position.y),
            static_cast<double>(closest->node_position.z)});
        lua_setfield(L, -2, "node_pos");
        pushVec3(L, closest->local_point);
        lua_setfield(L, -2, "local_point");
        pushVec3(L, closest->world_point);
        lua_setfield(L, -2, "world_point");
        pushVec3(L, closest->world_normal);
        lua_setfield(L, -2, "normal");
        lua_pushnumber(L, closest->distance);
        lua_setfield(L, -2, "distance");
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_surface_velocity(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const navycraft::Vec3d point = readVec3(L, 2);
        const navycraft::Vec3d radial = point - construct->transform().position;
        const navycraft::Vec3d velocity = construct->linearVelocity() + navycraft::Vec3d{
            -construct->yawVelocity() * radial.z,
            0.0,
            construct->yawVelocity() * radial.x,
        };
        pushVec3(L, velocity);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_node(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const navycraft::Vec3d position_value = readVec3(L, 2);
        const navycraft::LocalNodePos position{
            static_cast<std::int32_t>(std::llround(position_value.x)),
            static_cast<std::int32_t>(std::llround(position_value.y)),
            static_cast<std::int32_t>(std::llround(position_value.z)),
        };
        if (lua_isnil(L, 3)) {
            const bool removed = construct->removeNode(position);
            if (removed)
                broadcastSection(getServer(L), *construct,
                    navycraft::ConstructSectionIndex::sectionFor(position));
            lua_pushboolean(L, removed);
            return 1;
        }
        if (!lua_istable(L, 3))
            throw std::invalid_argument("node must be a table or nil");
        navycraft::ConstructNode node;
        node.node_name = readStringField(L, 3, "name");
        if (node.node_name.empty())
            throw std::invalid_argument("construct node name cannot be empty");
        node.param1 = static_cast<std::uint8_t>(readNumberField(L, 3, "param1", 0.0));
        node.param2 = static_cast<std::uint8_t>(readNumberField(L, 3, "param2", 0.0));
        node.metadata_blob = readStringField(L, 3, "metadata");
        construct->setNode(position, std::move(node));
        broadcastSection(getServer(L), *construct,
            navycraft::ConstructSectionIndex::sectionFor(position));
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_sections(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        navycraft::ConstructSectionIndex sections;
        sections.rebuild(*construct);
        const auto positions = sections.positions();
        lua_createtable(L, static_cast<int>(positions.size()), 0);
        int index = 1;
        for (const auto &position : positions) {
            const auto *section = sections.find(position);
            lua_createtable(L, 0, 3);
            pushVec3(L, {static_cast<double>(position.x), static_cast<double>(position.y),
                static_cast<double>(position.z)});
            lua_setfield(L, -2, "pos");
            lua_pushinteger(L, static_cast<lua_Integer>(section ? section->nodes.size() : 0));
            lua_setfield(L, -2, "node_count");
            lua_pushnumber(L, static_cast<lua_Number>(section ? section->revision : 0));
            lua_setfield(L, -2, "revision");
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}


int ModApiNavyCraft::l_get_dynamic_construct_node_state(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const auto position = readLocalPos(L, 2);
        if (!construct->getNode(position))
            return fail(L, "construct node not found");
        pushNodeState(L, construct->getNodeState(position));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_metadata(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const auto position = readLocalPos(L, 2);
        if (!lua_isstring(L, 3))
            throw std::invalid_argument("metadata key must be a string");
        const std::string key = lua_tostring(L, 3);
        const bool changed = lua_isnil(L, 4) ?
            construct->eraseNodeMetadataField(position, key) :
            construct->setNodeMetadataField(position, key, lua_isstring(L, 4) ? lua_tostring(L, 4) : "");
        lua_pushboolean(L, changed);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_inventory(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const auto position = readLocalPos(L, 2);
        if (!lua_isstring(L, 3))
            throw std::invalid_argument("inventory list name must be a string");
        const std::string list_name = lua_tostring(L, 3);
        if (lua_isnil(L, 4)) {
            lua_pushboolean(L, construct->eraseNodeInventory(position, list_name));
            return 1;
        }
        if (!lua_istable(L, 4))
            throw std::invalid_argument("inventory definition must be a table or nil");
        navycraft::ConstructInventoryList list;
        list.width = static_cast<std::uint16_t>(readNumberField(L, 4, "width", 0.0));
        getField(L, 4, "stacks");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("inventory stacks must be an array");
        }
        const std::size_t count = static_cast<std::size_t>(lua_objlen(L, -1));
        list.stacks.reserve(count);
        for (std::size_t index = 1; index <= count; ++index) {
            lua_rawgeti(L, -1, static_cast<int>(index));
            list.stacks.emplace_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        lua_pushboolean(L, construct->setNodeInventory(position, list_name, std::move(list)));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_start_dynamic_construct_timer(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const auto position = readLocalPos(L, 2);
        const double timeout = luaL_checknumber(L, 3);
        const double elapsed = lua_gettop(L) >= 4 ? luaL_checknumber(L, 4) : 0.0;
        lua_pushboolean(L, construct->startNodeTimer(position, timeout, elapsed));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_stop_dynamic_construct_timer(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        lua_pushboolean(L, construct->stopNodeTimer(readLocalPos(L, 2)));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_poll_dynamic_construct_events(lua_State *L)
{
    const auto events = navycraft::runtimeConstructInteractionEngine().drainEvents();
    lua_createtable(L, static_cast<int>(events.size()), 0);
    int index = 1;
    for (const auto &event : events) {
        lua_createtable(L, 0, 14);
        pushId(L, event.event_id);
        lua_setfield(L, -2, "event_id");
        pushId(L, event.request.construct_id);
        lua_setfield(L, -2, "construct_id");
        const char *action = actionName(event.request.action);
        lua_pushlstring(L, action, std::char_traits<char>::length(action));
        lua_setfield(L, -2, "action");
        pushLocalPos(L, event.request.node_position);
        lua_setfield(L, -2, "node_pos");
        pushLocalPos(L, event.request.adjacent_position);
        lua_setfield(L, -2, "adjacent_pos");
        pushVec3(L, event.request.world_point);
        lua_setfield(L, -2, "world_point");
        pushVec3(L, event.request.world_normal);
        lua_setfield(L, -2, "normal");
        lua_pushlstring(L, event.request.actor.data(), event.request.actor.size());
        lua_setfield(L, -2, "actor");
        lua_pushlstring(L, event.request.wielded_item.data(), event.request.wielded_item.size());
        lua_setfield(L, -2, "wielded_item");
        lua_pushlstring(L, event.node_name.data(), event.node_name.size());
        lua_setfield(L, -2, "node_name");
        lua_pushnumber(L, event.timer_elapsed);
        lua_setfield(L, -2, "elapsed");
        lua_pushnumber(L, event.timer_timeout);
        lua_setfield(L, -2, "timeout");
        lua_createtable(L, 0, 0);
        for (const auto &[key, value] : event.request.fields) {
            lua_pushlstring(L, value.data(), value.size());
            lua_setfield(L, -2, key.c_str());
        }
        lua_setfield(L, -2, "fields");
        lua_rawseti(L, -2, index++);
    }
    return 1;
}

int ModApiNavyCraft::l_resolve_dynamic_construct_timer(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        const auto event_id = readId(L, 2);
        const bool restart = lua_toboolean(L, 3) != 0;
        const double timeout = lua_gettop(L) >= 4 && lua_isnumber(L, 4) ?
            lua_tonumber(L, 4) : 0.0;
        lua_pushboolean(L, navycraft::runtimeConstructInteractionEngine().resolveTimerEvent(
            *construct, event_id, restart, timeout));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_resolve_dynamic_construct_mutation(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        if (!lua_istable(L, 3))
            throw std::invalid_argument("construct mutation resolution must be a table");

        const auto before_payload = navycraft::ConstructSerialization::encode(*construct);
        navycraft::ConstructMutationResolution resolution;
        resolution.event_id = readId(L, 2);
        resolution.accepted = readBoolField(L, 3, "accepted", false);
        resolution.protected_violation = readBoolField(
            L, 3, "protected_violation", false);
        resolution.replace_existing = readBoolField(L, 3, "replace_existing", false);
        resolution.placed_node = readOptionalNodeField(L, 3, "node");
        resolution.wielded_item_after = readStringField(L, 3, "wielded_item_after");
        resolution.reason = readStringField(L, 3, "reason");
        resolution.drops = readStringArrayField(L, 3, "drops");

        const auto result = navycraft::runtimeConstructInteractionEngine().resolveMutationEvent(
            *construct, resolution);
        const auto after_payload = navycraft::ConstructSerialization::encode(*construct);
        std::int64_t action_id = 0;
        for (const auto &record :
                navycraft::runtimeConstructInteractionEngine().drainMutationRecords()) {
            action_id = navycraft::runtimeConstructPersistence().recordMutation(
                record, before_payload, after_payload);
        }
        if (result.node_changed)
            broadcastSection(getServer(L), *construct,
                navycraft::ConstructSectionIndex::sectionFor(result.changed_position));

        lua_createtable(L, 0, 4);
        lua_pushboolean(L, result.accepted);
        lua_setfield(L, -2, "resolved");
        lua_pushboolean(L, result.node_changed);
        lua_setfield(L, -2, "node_changed");
        lua_pushlstring(L, result.message.data(), result.message.size());
        lua_setfield(L, -2, "message");
        lua_pushnumber(L, static_cast<lua_Number>(action_id));
        lua_setfield(L, -2, "action_id");
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_initialise_dynamic_construct_persistence(lua_State *L)
{
    try {
        if (!lua_isstring(L, 1))
            throw std::invalid_argument("construct persistence path must be a string");
        const std::string path = lua_tostring(L, 1);
        navycraft::runtimeConstructPersistence().initialise(
            path, navycraft::runtimeConstructRegistry());
        lua_pushinteger(L, static_cast<lua_Integer>(
            navycraft::runtimeConstructRegistry().size()));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_sync_dynamic_construct_persistence(lua_State *L)
{
    try {
        (void)L;
        navycraft::runtimeConstructPersistence().syncAll(
            navycraft::runtimeConstructRegistry());
        navycraft::runtimeConstructPersistence().flush();
        lua_pushinteger(L, static_cast<lua_Integer>(
            navycraft::runtimeConstructRegistry().size()));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_actions(lua_State *L)
{
    try {
        const auto id = readId(L, 1);
        std::size_t limit = 100;
        if (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
            const double raw = lua_tonumber(L, 2);
            if (!std::isfinite(raw) || raw < 0.0)
                throw std::invalid_argument("construct action limit must be non-negative");
            limit = static_cast<std::size_t>(std::min(raw, 10000.0));
        }
        const auto actions = navycraft::runtimeConstructPersistence().recentActions(id, limit);
        lua_createtable(L, static_cast<int>(actions.size()), 0);
        int index = 1;
        for (const auto &action : actions) {
            pushMutationAction(L, action);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_rollback_dynamic_construct_action(lua_State *L)
{
    try {
        const auto raw_id = readId(L, 1);
        if (raw_id > static_cast<navycraft::ConstructId>(
                std::numeric_limits<std::int64_t>::max()))
            throw std::invalid_argument("construct action id is too large");
        auto construct = navycraft::runtimeConstructPersistence().rollbackAction(
            static_cast<std::int64_t>(raw_id), navycraft::runtimeConstructRegistry());
        if (!construct)
            return fail(L, "construct rollback action not found");
        navycraft::clearRuntimeConstructState(construct->id());
        broadcastMessage(getServer(L), {navycraft::ConstructWireKind::Remove,
            construct->id(), navycraft::ConstructPacketCodec::encodeRemove(construct->id()), true});
        broadcastFullConstruct(getServer(L), *construct);
        pushConstruct(L, *construct, false);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_dynamic_construct_local_to_world(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        pushVec3(L, construct->transform().localToWorld(readVec3(L, 2)));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}


int ModApiNavyCraft::l_emit_dynamic_construct_effect(lua_State *L)
{
    try {
        const auto construct = navycraft::runtimeConstructRegistry().find(readId(L, 1));
        if (!construct)
            return fail(L, "construct not found");
        if (!lua_istable(L, 2))
            throw std::invalid_argument("effect definition must be a table");

        navycraft::ConstructEffectEvent event;
        event.sequence = navycraft::nextRuntimeEffectSequence(construct->id());
        event.construct_id = construct->id();
        const double raw_effect_id = readNumberField(L, 2, "effect_id", 0.0);
        if (raw_effect_id < 0.0 || raw_effect_id > 9007199254740991.0 ||
                std::floor(raw_effect_id) != raw_effect_id)
            throw std::invalid_argument("effect id is outside exact Lua integer range");
        event.effect_id = static_cast<std::uint64_t>(raw_effect_id);

        const std::string kind = readStringField(L, 2, "kind");
        if (kind == "sound") event.kind = navycraft::ConstructEffectKind::SoundOneShot;
        else if (kind == "sound_loop_start") event.kind = navycraft::ConstructEffectKind::SoundLoopStart;
        else if (kind == "sound_loop_stop") event.kind = navycraft::ConstructEffectKind::SoundLoopStop;
        else if (kind == "particles") event.kind = navycraft::ConstructEffectKind::ParticleBurst;
        else if (kind == "emitter_start") event.kind = navycraft::ConstructEffectKind::ParticleEmitterStart;
        else if (kind == "emitter_stop") event.kind = navycraft::ConstructEffectKind::ParticleEmitterStop;
        else if (kind == "light_flash") event.kind = navycraft::ConstructEffectKind::LightFlash;
        else throw std::invalid_argument("unknown construct effect kind");

        const std::string preset = readStringField(L, 2, "preset");
        static const std::unordered_map<std::string, navycraft::ConstructEffectPreset> presets = {
            {"generic", navycraft::ConstructEffectPreset::Generic},
            {"engine", navycraft::ConstructEffectPreset::Engine},
            {"wake", navycraft::ConstructEffectPreset::Wake},
            {"exhaust", navycraft::ConstructEffectPreset::Exhaust},
            {"smoke", navycraft::ConstructEffectPreset::Smoke},
            {"fire", navycraft::ConstructEffectPreset::Fire},
            {"flood", navycraft::ConstructEffectPreset::Flood},
            {"muzzle", navycraft::ConstructEffectPreset::Muzzle},
            {"explosion", navycraft::ConstructEffectPreset::Explosion},
            {"torpedo", navycraft::ConstructEffectPreset::Torpedo},
            {"depth_charge", navycraft::ConstructEffectPreset::DepthCharge},
            {"splash", navycraft::ConstructEffectPreset::Splash},
            {"damage_sparks", navycraft::ConstructEffectPreset::DamageSparks},
        };
        const auto preset_it = presets.find(preset.empty() ? "generic" : preset);
        if (preset_it == presets.end())
            throw std::invalid_argument("unknown construct effect preset");
        event.preset = preset_it->second;

        getField(L, 2, "local_pos");
        if (lua_istable(L, -1)) event.local_position = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "direction");
        if (lua_istable(L, -1)) event.local_direction = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "velocity");
        if (lua_istable(L, -1)) event.velocity = readVec3(L, -1);
        lua_pop(L, 1);

        event.sound_name = readStringField(L, 2, "sound");
        event.texture_name = readStringField(L, 2, "texture");
        event.gain = readNumberField(L, 2, "gain", 1.0);
        event.pitch = readNumberField(L, 2, "pitch", 1.0);
        event.max_distance = readNumberField(L, 2, "max_distance", 64.0);
        event.duration = readNumberField(L, 2, "duration", 0.5);
        const double raw_amount = readNumberField(L, 2, "amount", 1.0);
        if (raw_amount < 0.0 || raw_amount > 4096.0)
            throw std::invalid_argument("effect amount is outside allowed range");
        event.amount = static_cast<std::uint16_t>(raw_amount);
        event.size_min = readNumberField(L, 2, "size_min", 0.25);
        event.size_max = readNumberField(L, 2, "size_max", 1.0);
        event.lifetime_min = readNumberField(L, 2, "lifetime_min", 0.2);
        event.lifetime_max = readNumberField(L, 2, "lifetime_max", 1.0);
        const double raw_glow = readNumberField(L, 2, "glow", 0.0);
        if (raw_glow < 0.0 || raw_glow > 255.0)
            throw std::invalid_argument("effect glow is outside byte range");
        event.glow = static_cast<std::uint8_t>(raw_glow);
        event.collision = readBoolField(L, 2, "collision", false);

        broadcastMessage(getServer(L), {navycraft::ConstructWireKind::Effect,
            construct->id(), navycraft::ConstructEffectPacketCodec::encode(event), true});
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}


int ModApiNavyCraft::l_spawn_dynamic_construct_projectile(lua_State *L)
{
    try {
        const auto source_id = readId(L, 1);
        if (!navycraft::runtimeConstructRegistry().find(source_id))
            return fail(L, "source construct not found");
        if (!lua_istable(L, 2))
            throw std::invalid_argument("projectile definition must be a table");

        navycraft::ConstructProjectileState projectile;
        projectile.source_construct_id = source_id;
        projectile.owner = readStringField(L, 2, "owner");
        projectile.spec.kind = projectileKind(readStringField(L, 2, "kind"));
        getField(L, 2, "position");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("projectile position must be a vector");
        }
        projectile.position = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "velocity");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("projectile velocity must be a vector");
        }
        projectile.velocity = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "target_id");
        if (!lua_isnil(L, -1))
            projectile.target_construct_id = readId(L, -1);
        lua_pop(L, 1);

        auto &spec = projectile.spec;
        spec.radius = readNumberField(L, 2, "radius", spec.radius);
        spec.gravity = readNumberField(L, 2, "gravity", spec.gravity);
        spec.drag = readNumberField(L, 2, "drag", spec.drag);
        spec.arming_time = readNumberField(L, 2, "arming_time", spec.arming_time);
        spec.maximum_age = readNumberField(L, 2, "maximum_age", spec.maximum_age);
        spec.maximum_range = readNumberField(L, 2, "maximum_range", spec.maximum_range);
        spec.blast_radius = readNumberField(L, 2, "blast_radius", spec.blast_radius);
        spec.blast_power = readNumberField(L, 2, "blast_power", spec.blast_power);
        spec.penetration = readNumberField(L, 2, "penetration", spec.penetration);
        spec.guidance_turn_rate = readNumberField(L, 2, "guidance_turn_rate",
            spec.guidance_turn_rate);
        spec.preferred_depth = readNumberField(L, 2, "preferred_depth",
            projectile.position.y);
        spec.guided = readBoolField(L, 2, "guided", spec.guided);
        spec.detonate_on_expiry = readBoolField(L, 2, "detonate_on_expiry",
            spec.detonate_on_expiry);
        spec.requires_water = readBoolField(L, 2, "requires_water", spec.requires_water);

        const auto id = navycraft::runtimeConstructProjectileEngine().spawn(projectile);
        const auto stored = navycraft::runtimeConstructProjectileEngine().find(id);
        if (!stored)
            throw std::runtime_error("projectile disappeared after creation");
        navycraft::ConstructProjectileEvent event{
            navycraft::ConstructProjectileEventKind::Spawn, *stored, std::nullopt};
        broadcastMessage(getServer(L), {navycraft::ConstructWireKind::Projectile, source_id,
            navycraft::ConstructProjectilePacketCodec::encode(event), true});
        pushId(L, id);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_step_dynamic_construct_projectiles(lua_State *L)
{
    try {
        if (!lua_isnumber(L, 1))
            throw std::invalid_argument("projectile delta time must be a number");
        const double delta_seconds = lua_tonumber(L, 1);
        const auto events = navycraft::runtimeConstructProjectileEngine().step(delta_seconds);
        Server *server = getServer(L);
        bool changed_constructs = false;
        lua_createtable(L, static_cast<int>(events.size()), 0);
        int index = 1;
        for (const auto &event : events) {
            const bool reliable = event.kind != navycraft::ConstructProjectileEventKind::Update;
            broadcastMessage(server, {navycraft::ConstructWireKind::Projectile,
                event.projectile.source_construct_id,
                navycraft::ConstructProjectilePacketCodec::encode(event), reliable});
            if (event.impact) {
                std::unordered_map<navycraft::ConstructId, std::size_t> breaches;
                for (const auto &damage : event.impact->explosion.node_damage) {
                    if (damage.breached)
                        ++breaches[damage.construct_id];
                }
                for (const auto &[affected_id, count] : breaches)
                    navycraft::runtimeConstructStructureEngine().recordBreaches(affected_id, count);
                for (const auto affected_id : event.impact->explosion.affected_constructs) {
                    const auto construct = navycraft::runtimeConstructRegistry().find(affected_id);
                    if (construct) {
                        broadcastFullConstruct(server, *construct);
                        changed_constructs = true;
                    }
                }
            }
            pushProjectileEvent(L, event);
            lua_rawseti(L, -2, index++);
        }
        if (changed_constructs && navycraft::runtimeConstructPersistence().initialised())
            navycraft::runtimeConstructPersistence().syncAll(
                navycraft::runtimeConstructRegistry());
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_projectiles(lua_State *L)
{
    const auto projectiles = navycraft::runtimeConstructProjectileEngine().snapshot();
    lua_createtable(L, static_cast<int>(projectiles.size()), 0);
    int index = 1;
    for (const auto &projectile : projectiles) {
        pushProjectileState(L, projectile);
        lua_rawseti(L, -2, index++);
    }
    return 1;
}

int ModApiNavyCraft::l_impact_dynamic_construct_projectile(lua_State *L)
{
    try {
        const auto projectile_id = readId(L, 1);
        const auto projectile = navycraft::runtimeConstructProjectileEngine().find(projectile_id);
        if (!projectile)
            return fail(L, "projectile not found");
        const navycraft::Vec3d position = readVec3(L, 2);
        const std::string name = lua_gettop(L) >= 3 && lua_isstring(L, 3) ?
            lua_tostring(L, 3) : "terrain";
        navycraft::ConstructImpactKind kind = navycraft::ConstructImpactKind::Terrain;
        if (name == "water") kind = navycraft::ConstructImpactKind::Water;
        else if (name == "expired") kind = navycraft::ConstructImpactKind::Expired;
        else if (name != "terrain")
            throw std::invalid_argument("manual projectile impact kind must be terrain, water or expired");

        navycraft::ConstructProjectileImpact impact;
        impact.projectile_id = projectile_id;
        impact.kind = kind;
        impact.world_position = position;
        if (kind != navycraft::ConstructImpactKind::Expired ||
                projectile->spec.detonate_on_expiry) {
            impact.explosion = navycraft::runtimeConstructProjectileEngine().explode(
                position, *projectile, 0);
        }
        navycraft::runtimeConstructProjectileEngine().remove(projectile_id);
        navycraft::ConstructProjectileState final_state = *projectile;
        final_state.position = position;
        ++final_state.sequence;
        navycraft::ConstructProjectileEvent event{
            navycraft::ConstructProjectileEventKind::Impact, final_state, impact};
        Server *server = getServer(L);
        broadcastMessage(server, {navycraft::ConstructWireKind::Projectile,
            final_state.source_construct_id,
            navycraft::ConstructProjectilePacketCodec::encode(event), true});
        for (const auto affected_id : impact.explosion.affected_constructs) {
            const auto construct = navycraft::runtimeConstructRegistry().find(affected_id);
            if (construct)
                broadcastFullConstruct(server, *construct);
        }
        if (!impact.explosion.affected_constructs.empty() &&
                navycraft::runtimeConstructPersistence().initialised()) {
            navycraft::runtimeConstructPersistence().syncAll(
                navycraft::runtimeConstructRegistry());
        }
        pushProjectileEvent(L, event);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_observe_dynamic_construct_target(lua_State *L)
{
    try {
        const auto observer_id = readId(L, 1);
        if (!navycraft::runtimeConstructRegistry().find(observer_id))
            return fail(L, "observer construct not found");
        if (!lua_istable(L, 2))
            throw std::invalid_argument("target observation must be a table");
        navycraft::FireControlObservation observation;
        observation.observer_construct_id = observer_id;
        getField(L, 2, "target_id");
        observation.target_construct_id = readId(L, -1);
        lua_pop(L, 1);
        observation.sample_time = readNumberField(L, 2, "sample_time", 0.0);
        getField(L, 2, "position");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("target observation position must be a vector");
        }
        observation.position = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "velocity");
        if (lua_istable(L, -1))
            observation.velocity = readVec3(L, -1);
        lua_pop(L, 1);
        observation.classification = fireControlTargetClass(
            readStringField(L, 2, "classification"));
        observation.sensor = fireControlSensorKind(
            readStringField(L, 2, "sensor"));
        observation.confidence = readNumberField(L, 2, "confidence", 1.0);
        if (!navycraft::runtimeConstructFireControlEngine().observe(observation))
            return fail(L, "target observation was rejected");
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_solve_dynamic_construct_fire_control(lua_State *L)
{
    try {
        navycraft::FireControlRequest request;
        request.shooter_construct_id = readId(L, 1);
        request.target_construct_id = readId(L, 2);
        if (!lua_istable(L, 3))
            throw std::invalid_argument("fire-control weapon definition must be a table");
        request.weapon = readFireControlWeaponSpec(L, 3);
        request.solution_time = readNumberField(L, 3, "solution_time", 0.0);
        getField(L, 3, "muzzle_local");
        if (lua_istable(L, -1))
            request.muzzle_local_position = readVec3(L, -1);
        lua_pop(L, 1);
        const auto solution = navycraft::runtimeConstructFireControlEngine().solve(request);
        pushFireControlSolution(L, solution);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_configure_dynamic_construct_battery(lua_State *L)
{
    try {
        navycraft::FireControlBattery battery;
        battery.shooter_construct_id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("fire-control battery definition must be a table");
        getField(L, 2, "id");
        if (!lua_isnil(L, -1))
            battery.id = readId(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "muzzle_local");
        if (lua_istable(L, -1))
            battery.muzzle_local_position = readVec3(L, -1);
        lua_pop(L, 1);
        battery.weapon = readFireControlWeaponSpec(L, 2);
        battery.mode = fireControlBatteryMode(readStringField(L, 2, "mode"));
        const double raw_mask = readNumberField(L, 2, "allowed_class_mask", -1.0);
        if (raw_mask >= 0.0) {
            if (!std::isfinite(raw_mask) || raw_mask > 4294967295.0 ||
                    std::floor(raw_mask) != raw_mask) {
                throw std::invalid_argument("allowed class mask is invalid");
            }
            battery.allowed_class_mask = static_cast<std::uint32_t>(raw_mask);
        } else {
            const std::string allowed_class = readStringField(L, 2, "allowed_class");
            if (!allowed_class.empty()) {
                battery.allowed_class_mask = navycraft::fireControlClassMask(
                    fireControlTargetClass(allowed_class));
            } else if (battery.mode == navycraft::FireControlBatteryMode::Defensive) {
                battery.allowed_class_mask = navycraft::fireControlClassMask(
                    navycraft::FireControlTargetClass::Air);
            }
        }
        getField(L, 2, "target_id");
        if (!lua_isnil(L, -1))
            battery.designated_target_id = readId(L, -1);
        lua_pop(L, 1);
        battery.minimum_track_confidence = readNumberField(L, 2,
            "minimum_confidence", battery.minimum_track_confidence);
        battery.reload_seconds = readNumberField(L, 2, "reload_seconds",
            battery.reload_seconds);
        battery.next_ready_time = readNumberField(L, 2, "next_ready_time",
            battery.next_ready_time);
        battery.traverse_centre_radians = readNumberField(L, 2, "traverse_centre",
            battery.traverse_centre_radians);
        battery.traverse_half_width_radians = readNumberField(L, 2,
            "traverse_half_width", battery.traverse_half_width_radians);
        battery.enabled = readBoolField(L, 2, "enabled", battery.enabled);
        const auto id = navycraft::runtimeConstructFireControlEngine().configureBattery(
            battery);
        pushId(L, id);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_remove_dynamic_construct_battery(lua_State *L)
{
    const auto id = readId(L, 1);
    lua_pushboolean(L,
        navycraft::runtimeConstructFireControlEngine().removeBattery(id));
    return 1;
}

int ModApiNavyCraft::l_step_dynamic_construct_fire_control(lua_State *L)
{
    try {
        if (!lua_isnumber(L, 1))
            throw std::invalid_argument("fire-control current time must be a number");
        const double current_time = lua_tonumber(L, 1);
        const double maximum_age = lua_gettop(L) >= 2 && lua_isnumber(L, 2) ?
            lua_tonumber(L, 2) : 10.0;
        const auto orders = navycraft::runtimeConstructFireControlEngine().stepAutomatic(
            current_time, maximum_age);
        lua_createtable(L, static_cast<int>(orders.size()), 0);
        int index = 1;
        for (const auto &order : orders) {
            lua_createtable(L, 0, 5);
            pushId(L, order.battery_id);
            lua_setfield(L, -2, "battery_id");
            pushId(L, order.shooter_construct_id);
            lua_setfield(L, -2, "shooter_id");
            pushId(L, order.target_construct_id);
            lua_setfield(L, -2, "target_id");
            pushFireControlSolution(L, order.solution);
            lua_setfield(L, -2, "solution");
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_fire_control_tracks(lua_State *L)
{
    try {
        navycraft::ConstructId observer_id = 0;
        if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
            observer_id = readId(L, 1);
        const auto tracks = navycraft::runtimeConstructFireControlEngine().tracks(observer_id);
        lua_createtable(L, static_cast<int>(tracks.size()), 0);
        int index = 1;
        for (const auto &track : tracks) {
            pushFireControlTrack(L, track);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_fire_control_batteries(lua_State *L)
{
    try {
        navycraft::ConstructId shooter_id = 0;
        if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
            shooter_id = readId(L, 1);
        const auto batteries = navycraft::runtimeConstructFireControlEngine().batteries(
            shooter_id);
        lua_createtable(L, static_cast<int>(batteries.size()), 0);
        int index = 1;
        for (const auto &battery : batteries) {
            pushFireControlBattery(L, battery);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}


int ModApiNavyCraft::l_configure_dynamic_construct_navigation(lua_State *L)
{
    try {
        navycraft::ConstructNavigationConfig config;
        config.construct_id = readId(L, 1);
        if (!navycraft::runtimeConstructRegistry().find(config.construct_id))
            return fail(L, "construct not found");
        if (!lua_istable(L, 2))
            throw std::invalid_argument("navigation definition must be a table");
        config.mode = navigationMode(readStringField(L, 2, "mode"));
        config.domain = navigationDomain(readStringField(L, 2, "domain"));
        config.maximum_speed = readNumberField(L, 2, "maximum_speed",
            readNumberField(L, 2, "speed", config.maximum_speed));
        config.maximum_reverse_speed = readNumberField(L, 2, "maximum_reverse_speed",
            config.maximum_reverse_speed);
        config.maximum_acceleration = readNumberField(L, 2, "maximum_acceleration",
            config.maximum_acceleration);
        config.maximum_deceleration = readNumberField(L, 2, "maximum_deceleration",
            config.maximum_deceleration);
        config.maximum_yaw_rate = readNumberField(L, 2, "maximum_yaw_rate",
            config.maximum_yaw_rate);
        config.maximum_yaw_acceleration = readNumberField(L, 2,
            "maximum_yaw_acceleration", config.maximum_yaw_acceleration);
        config.maximum_vertical_speed = readNumberField(L, 2,
            "maximum_vertical_speed", config.maximum_vertical_speed);
        config.arrival_radius = readNumberField(L, 2, "arrival_radius",
            config.arrival_radius);
        config.lookahead_distance = readNumberField(L, 2, "lookahead",
            config.lookahead_distance);
        config.obstacle_margin = readNumberField(L, 2, "obstacle_margin",
            config.obstacle_margin);
        config.separation_distance = readNumberField(L, 2, "separation_distance",
            config.separation_distance);
        config.avoidance_strength = readNumberField(L, 2, "avoidance_strength",
            config.avoidance_strength);
        config.heading_gain = readNumberField(L, 2, "heading_gain", config.heading_gain);
        config.vertical_gain = readNumberField(L, 2, "vertical_gain", config.vertical_gain);
        config.stuck_timeout = readNumberField(L, 2, "stuck_timeout",
            config.stuck_timeout);
        config.recovery_seconds = readNumberField(L, 2, "recovery_seconds",
            config.recovery_seconds);
        config.route_loop = readBoolField(L, 2, "loop", config.route_loop);
        config.allow_reverse = readBoolField(L, 2, "allow_reverse", config.allow_reverse);
        config.apply_to_construct = readBoolField(L, 2, "apply_to_construct",
            config.apply_to_construct);
        getField(L, 2, "leader_id");
        if (!lua_isnil(L, -1))
            config.formation_leader_id = readId(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "formation_offset");
        if (lua_istable(L, -1))
            config.formation_local_offset = readVec3(L, -1);
        lua_pop(L, 1);
        if (!navycraft::runtimeConstructNavigationEngine().configure(config))
            return fail(L, "navigation configuration was rejected");
        getField(L, 2, "hold_position");
        if (lua_istable(L, -1))
            navycraft::runtimeConstructNavigationEngine().setHoldPosition(
                config.construct_id, readVec3(L, -1));
        lua_pop(L, 1);
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_route(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("route must be an array");
        const std::size_t count = static_cast<std::size_t>(lua_objlen(L, 2));
        if (count > 65535)
            throw std::length_error("route has too many waypoints");
        std::vector<navycraft::ConstructNavigationWaypoint> route;
        route.reserve(count);
        for (std::size_t index = 1; index <= count; ++index) {
            lua_rawgeti(L, 2, static_cast<int>(index));
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                throw std::invalid_argument("route waypoints must be tables");
            }
            navycraft::ConstructNavigationWaypoint waypoint;
            getField(L, -1, "position");
            if (lua_istable(L, -1)) {
                waypoint.position = readVec3(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                waypoint.position = readVec3(L, -1);
            }
            waypoint.arrival_radius = readNumberField(L, -1, "arrival_radius",
                waypoint.arrival_radius);
            waypoint.target_speed = readNumberField(L, -1, "target_speed",
                waypoint.target_speed);
            waypoint.stop = readBoolField(L, -1, "stop", waypoint.stop);
            route.push_back(waypoint);
            lua_pop(L, 1);
        }
        const bool loop = lua_gettop(L) >= 3 ? lua_toboolean(L, 3) != 0 : false;
        if (!navycraft::runtimeConstructNavigationEngine().setRoute(
                construct_id, std::move(route), loop))
            return fail(L, "route was rejected");
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_observe_dynamic_construct_obstacle(lua_State *L)
{
    try {
        navycraft::ConstructNavigationObstacle obstacle;
        obstacle.observer_construct_id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("navigation obstacle must be a table");
        const double raw_obstacle_id = readNumberField(L, 2, "id", 0.0);
        if (!std::isfinite(raw_obstacle_id) || raw_obstacle_id < 0.0 ||
                raw_obstacle_id > 9007199254740991.0 ||
                std::floor(raw_obstacle_id) != raw_obstacle_id)
            throw std::invalid_argument("navigation obstacle id is invalid");
        obstacle.id = static_cast<std::uint64_t>(raw_obstacle_id);
        getField(L, 2, "construct_id");
        if (!lua_isnil(L, -1))
            obstacle.construct_id = readId(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "position");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("navigation obstacle position must be a vector");
        }
        obstacle.position = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "velocity");
        if (lua_istable(L, -1))
            obstacle.velocity = readVec3(L, -1);
        lua_pop(L, 1);
        obstacle.radius = readNumberField(L, 2, "radius", obstacle.radius);
        obstacle.sample_time = readNumberField(L, 2, "sample_time", 0.0);
        obstacle.expires_at = readNumberField(L, 2, "expires_at",
            obstacle.sample_time + readNumberField(L, 2, "lifetime", 1.0));
        obstacle.hard = readBoolField(L, 2, "hard", obstacle.hard);
        if (!navycraft::runtimeConstructNavigationEngine().observeObstacle(obstacle))
            return fail(L, "navigation obstacle was rejected");
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_step_dynamic_construct_navigation(lua_State *L)
{
    try {
        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
            throw std::invalid_argument("navigation step requires delta time and current time");
        const auto commands = navycraft::runtimeConstructNavigationEngine().step(
            lua_tonumber(L, 1), lua_tonumber(L, 2));
        lua_createtable(L, static_cast<int>(commands.size()), 0);
        int index = 1;
        for (const auto &command : commands) {
            pushNavigationCommand(L, command);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_navigation(lua_State *L)
{
    try {
        if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
            const auto current = navycraft::runtimeConstructNavigationEngine().status(readId(L, 1));
            if (!current)
                return fail(L, "navigation state not found");
            pushNavigationStatus(L, *current);
            return 1;
        }
        const auto states = navycraft::runtimeConstructNavigationEngine().statuses();
        lua_createtable(L, static_cast<int>(states.size()), 0);
        int index = 1;
        for (const auto &state : states) {
            pushNavigationStatus(L, state);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_navigation_enabled(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        const bool enabled = lua_toboolean(L, 2) != 0;
        if (!navycraft::runtimeConstructNavigationEngine().setEnabled(construct_id, enabled))
            return fail(L, "navigation state not found");
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_clear_dynamic_construct_navigation(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        lua_pushboolean(L,
            navycraft::runtimeConstructNavigationEngine().remove(construct_id));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}


int ModApiNavyCraft::l_step_dynamic_construct_structure(lua_State *L)
{
    try {
        if (!lua_isnumber(L, 1))
            throw std::invalid_argument("structural delta time must be a number");
        const double delta_seconds = lua_tonumber(L, 1);
        const auto config = lua_gettop(L) >= 2 ? readStructuralConfig(L, 2) :
            navycraft::ConstructStructuralConfig{};
        const auto events = navycraft::runtimeConstructStructureEngine().step(
            delta_seconds, config);
        Server *server = getServer(L);
        bool changed = false;
        for (const auto &event : events) {
            if (event.kind == navycraft::ConstructStructuralEventKind::Split) {
                broadcastMessage(server, {navycraft::ConstructWireKind::Remove,
                    event.construct_id,
                    navycraft::ConstructPacketCodec::encodeRemove(event.construct_id), true});
                if (const auto parent = navycraft::runtimeConstructRegistry().find(event.construct_id))
                    broadcastFullConstruct(server, *parent);
                for (const auto fragment_id : event.fragment_ids) {
                    if (const auto fragment = navycraft::runtimeConstructRegistry().find(fragment_id))
                        broadcastFullConstruct(server, *fragment);
                }
                changed = true;
            } else if (event.state.role == navycraft::ConstructStructuralRole::Fragment ||
                event.state.role == navycraft::ConstructStructuralRole::Wreck) {
                if (const auto construct = navycraft::runtimeConstructRegistry().find(
                        event.construct_id))
                    broadcastTransform(server, *construct);
            }
        }
        if (changed && navycraft::runtimeConstructPersistence().initialised())
            navycraft::runtimeConstructPersistence().syncAll(
                navycraft::runtimeConstructRegistry());
        lua_createtable(L, static_cast<int>(events.size()), 0);
        int index = 1;
        for (const auto &event : events) {
            pushStructuralEvent(L, event);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_structure(lua_State *L)
{
    try {
        if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
            const auto state = navycraft::runtimeConstructStructureEngine().state(readId(L, 1));
            if (!state)
                return fail(L, "structural state not found");
            pushStructuralState(L, *state);
            return 1;
        }
        const auto states = navycraft::runtimeConstructStructureEngine().states();
        lua_createtable(L, static_cast<int>(states.size()), 0);
        int index = 1;
        for (const auto &state : states) {
            pushStructuralState(L, state);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_force_dynamic_construct_split(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        const auto config = lua_gettop(L) >= 2 ? readStructuralConfig(L, 2) :
            navycraft::ConstructStructuralConfig{};
        const auto events = navycraft::runtimeConstructStructureEngine().forceSplit(
            construct_id, config);
        Server *server = getServer(L);
        for (const auto &event : events) {
            if (event.kind != navycraft::ConstructStructuralEventKind::Split)
                continue;
            broadcastMessage(server, {navycraft::ConstructWireKind::Remove,
                event.construct_id,
                navycraft::ConstructPacketCodec::encodeRemove(event.construct_id), true});
            if (const auto parent = navycraft::runtimeConstructRegistry().find(event.construct_id))
                broadcastFullConstruct(server, *parent);
            for (const auto fragment_id : event.fragment_ids) {
                if (const auto fragment = navycraft::runtimeConstructRegistry().find(fragment_id))
                    broadcastFullConstruct(server, *fragment);
            }
        }
        lua_createtable(L, static_cast<int>(events.size()), 0);
        int index = 1;
        for (const auto &event : events) {
            pushStructuralEvent(L, event);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_flooding(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        if (!lua_isnumber(L, 2))
            throw std::invalid_argument("flooded fraction must be a number");
        if (!navycraft::runtimeConstructStructureEngine().setFloodedFraction(
                construct_id, lua_tonumber(L, 2)))
            return fail(L, "construct not found");
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_configure_dynamic_construct_joint(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("articulation definition must be a table");
        navycraft::ConstructArticulationDefinition definition;
        definition.construct_id = construct_id;
        if (const auto id = readOptionalIdField(L, 2, "id"))
            definition.id = *id;
        if (const auto parent = readOptionalIdField(L, 2, "parent_id"))
            definition.parent_id = *parent;
        definition.name = readStringField(L, 2, "name");
        definition.kind = articulationKind(readStringField(L, 2, "kind"));
        definition.control_mode = articulationControlMode(
            readStringField(L, 2, "control_mode"));
        getField(L, 2, "pivot");
        if (lua_istable(L, -1))
            definition.pivot_local = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "axis");
        if (lua_istable(L, -1))
            definition.axis_local = readVec3(L, -1);
        lua_pop(L, 1);
        definition.nodes = readLocalNodeArrayField(L, 2, "nodes");
        definition.minimum_position = readNumberField(L, 2, "minimum",
            readNumberField(L, 2, "min", definition.minimum_position));
        definition.maximum_position = readNumberField(L, 2, "maximum",
            readNumberField(L, 2, "max", definition.maximum_position));
        definition.maximum_speed = readNumberField(L, 2, "maximum_speed",
            readNumberField(L, 2, "speed", definition.maximum_speed));
        definition.maximum_acceleration = readNumberField(L, 2, "maximum_acceleration",
            readNumberField(L, 2, "acceleration", definition.maximum_acceleration));
        definition.enabled = readBoolField(L, 2, "enabled", definition.enabled);
        definition.render_enabled = readBoolField(L, 2, "render_enabled",
            definition.render_enabled);
        definition.collision_enabled = readBoolField(L, 2, "collision_enabled",
            definition.collision_enabled);
        auto &engine = navycraft::runtimeConstructArticulationEngine();
        const auto id = engine.configureJoint(definition);
        const auto state = engine.joint(id);
        if (!state)
            return fail(L, "articulation was not retained");
        Server *server = getServer(L);
        broadcastArticulationDefinition(server, state->definition);
        broadcastArticulationState(server, *state, server ? server->getUptime() : 0.0);
        pushId(L, id);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_joint(lua_State *L)
{
    try {
        const auto id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("articulation controls must be a table");
        auto &engine = navycraft::runtimeConstructArticulationEngine();
        bool changed = false;
        getField(L, 2, "position");
        if (lua_isnumber(L, -1))
            changed = engine.setJointPosition(id, lua_tonumber(L, -1)) || changed;
        lua_pop(L, 1);
        getField(L, 2, "target_position");
        if (lua_isnumber(L, -1))
            changed = engine.setJointTarget(id, lua_tonumber(L, -1)) || changed;
        lua_pop(L, 1);
        getField(L, 2, "target");
        if (lua_isnumber(L, -1))
            changed = engine.setJointTarget(id, lua_tonumber(L, -1)) || changed;
        lua_pop(L, 1);
        getField(L, 2, "velocity");
        if (lua_isnumber(L, -1))
            changed = engine.setJointVelocity(id, lua_tonumber(L, -1)) || changed;
        lua_pop(L, 1);
        getField(L, 2, "enabled");
        if (!lua_isnil(L, -1))
            changed = engine.setJointEnabled(id, lua_toboolean(L, -1) != 0) || changed;
        lua_pop(L, 1);
        const auto state = engine.joint(id);
        if (!state)
            return fail(L, "articulation not found");
        if (changed) {
            Server *server = getServer(L);
            broadcastArticulationDefinition(server, state->definition);
            broadcastArticulationState(server, *state, server ? server->getUptime() : 0.0);
        }
        pushArticulationState(L, *state);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_remove_dynamic_construct_joint(lua_State *L)
{
    try {
        const auto id = readId(L, 1);
        auto &engine = navycraft::runtimeConstructArticulationEngine();
        const auto state = engine.joint(id);
        if (!state) {
            lua_pushboolean(L, false);
            return 1;
        }
        const auto before = engine.joints(state->definition.construct_id);
        const bool removed = engine.removeJoint(id);
        if (removed) {
            for (const auto &candidate : before) {
                if (!engine.joint(candidate.definition.id)) {
                    broadcastMessage(getServer(L), {navycraft::ConstructWireKind::Articulation,
                        candidate.definition.construct_id,
                        navycraft::ConstructArticulationPacketCodec::encodeRemove(
                            candidate.definition.construct_id, candidate.definition.id), true});
                }
            }
        }
        lua_pushboolean(L, removed);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_configure_dynamic_construct_turret(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("turret definition must be a table");
        navycraft::ConstructTurretDefinition definition;
        definition.construct_id = construct_id;
        if (const auto id = readOptionalIdField(L, 2, "id"))
            definition.id = *id;
        const auto yaw = readOptionalIdField(L, 2, "yaw_joint_id");
        if (!yaw)
            throw std::invalid_argument("turret yaw_joint_id is required");
        definition.yaw_joint_id = *yaw;
        if (const auto pitch = readOptionalIdField(L, 2, "pitch_joint_id"))
            definition.pitch_joint_id = *pitch;
        definition.name = readStringField(L, 2, "name");
        getField(L, 2, "muzzle_local");
        if (lua_istable(L, -1))
            definition.muzzle_local = readVec3(L, -1);
        lua_pop(L, 1);
        getField(L, 2, "forward_local");
        if (lua_istable(L, -1))
            definition.forward_local = readVec3(L, -1);
        lua_pop(L, 1);
        definition.alignment_tolerance_radians = readNumberField(L, 2,
            "alignment_tolerance", definition.alignment_tolerance_radians);
        definition.projectile_radius = readNumberField(L, 2, "projectile_radius",
            definition.projectile_radius);
        definition.maximum_range = readNumberField(L, 2, "maximum_range",
            definition.maximum_range);
        definition.stabilised = readBoolField(L, 2, "stabilised", definition.stabilised);
        definition.enabled = readBoolField(L, 2, "enabled", definition.enabled);
        const auto id = navycraft::runtimeConstructArticulationEngine().configureTurret(
            definition);
        const auto status = navycraft::runtimeConstructArticulationEngine().turret(id);
        if (!status)
            return fail(L, "turret was not retained");
        pushTurretStatus(L, *status);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_aim_dynamic_construct_turret(lua_State *L)
{
    try {
        const auto id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("turret target must be a vector or solution table");
        navycraft::Vec3d target;
        getField(L, 2, "aim_point");
        if (lua_istable(L, -1))
            target = readVec3(L, -1);
        else
            target = readVec3(L, 2);
        lua_pop(L, 1);
        auto &engine = navycraft::runtimeConstructArticulationEngine();
        if (!engine.setTurretTarget(id, target))
            return fail(L, "turret not found");
        const auto status = engine.turret(id);
        if (!status)
            return fail(L, "turret not found");
        pushTurretStatus(L, *status);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_clear_dynamic_construct_turret_target(lua_State *L)
{
    const auto id = readId(L, 1);
    lua_pushboolean(L,
        navycraft::runtimeConstructArticulationEngine().clearTurretTarget(id));
    return 1;
}

int ModApiNavyCraft::l_get_dynamic_construct_articulations(lua_State *L)
{
    try {
        navycraft::ConstructId construct_id = 0;
        if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
            construct_id = readId(L, 1);
        auto &engine = navycraft::runtimeConstructArticulationEngine();
        const auto joints = engine.joints(construct_id);
        const auto turrets = engine.turrets(construct_id);
        lua_createtable(L, 0, 2);
        lua_createtable(L, static_cast<int>(joints.size()), 0);
        int index = 1;
        for (const auto &joint : joints) {
            pushArticulationState(L, joint);
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "joints");
        lua_createtable(L, static_cast<int>(turrets.size()), 0);
        index = 1;
        for (const auto &turret : turrets) {
            pushTurretStatus(L, turret);
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "turrets");
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_step_dynamic_construct_articulations(lua_State *L)
{
    try {
        if (!lua_isnumber(L, 1))
            throw std::invalid_argument("articulation delta must be a number");
        const double delta = lua_tonumber(L, 1);
        const double current_time = lua_gettop(L) >= 2 && lua_isnumber(L, 2) ?
            lua_tonumber(L, 2) : (getServer(L) ? getServer(L)->getUptime() : 0.0);
        auto &engine = navycraft::runtimeConstructArticulationEngine();
        const auto result = engine.step(delta, current_time);
        Server *server = getServer(L);
        for (const auto &snapshot : result.changed_joints) {
            broadcastMessage(server, {navycraft::ConstructWireKind::Articulation,
                snapshot.construct_id,
                navycraft::ConstructArticulationPacketCodec::encodeState(snapshot), false});
        }
        lua_createtable(L, 0, 2);
        lua_createtable(L, static_cast<int>(result.changed_joints.size()), 0);
        int index = 1;
        for (const auto &snapshot : result.changed_joints) {
            lua_createtable(L, 0, 8);
            pushId(L, snapshot.construct_id);
            lua_setfield(L, -2, "construct_id");
            pushId(L, snapshot.articulation_id);
            lua_setfield(L, -2, "id");
            lua_pushnumber(L, snapshot.position);
            lua_setfield(L, -2, "position");
            lua_pushnumber(L, snapshot.target_position);
            lua_setfield(L, -2, "target_position");
            lua_pushnumber(L, snapshot.velocity);
            lua_setfield(L, -2, "velocity");
            lua_pushboolean(L, snapshot.enabled);
            lua_setfield(L, -2, "enabled");
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "joints");
        lua_createtable(L, static_cast<int>(result.turrets.size()), 0);
        index = 1;
        for (const auto &turret : result.turrets) {
            pushTurretStatus(L, turret);
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "turrets");
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_dynamic_construct_line_of_fire(lua_State *L)
{
    try {
        const auto turret_id = readId(L, 1);
        const auto target = readVec3(L, 2);
        std::optional<navycraft::ConstructArticulationObstruction> obstruction;
        const bool clear = navycraft::runtimeConstructArticulationEngine().lineOfFireClear(
            turret_id, target, &obstruction);
        lua_pushboolean(L, clear);
        if (obstruction)
            pushArticulationObstruction(L, *obstruction);
        else
            lua_pushnil(L);
        const auto status = navycraft::runtimeConstructArticulationEngine().turret(turret_id);
        if (status)
            pushTurretStatus(L, *status);
        else
            lua_pushnil(L);
        return 3;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_configure_dynamic_construct_liquid_compartment(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        if (!lua_istable(L, 2))
            throw std::invalid_argument("liquid compartment definition must be a table");
        navycraft::ConstructLiquidCompartmentDefinition definition;
        definition.construct_id = construct_id;
        if (const auto id = readOptionalIdField(L, 2, "id"))
            definition.id = *id;
        definition.name = readStringField(L, 2, "name");
        definition.cells = readLocalNodeArrayField(L, 2, "cells");
        definition.sealed = readBoolField(L, 2, "sealed", definition.sealed);
        definition.allow_mixing = readBoolField(L, 2, "allow_mixing",
            definition.allow_mixing);
        definition.horizontal_flow_rate = readNumberField(L, 2,
            "horizontal_flow_rate", definition.horizontal_flow_rate);
        definition.vertical_flow_rate = readNumberField(L, 2,
            "vertical_flow_rate", definition.vertical_flow_rate);
        const auto id = navycraft::runtimeConstructLiquidEngine().configureCompartment(
            std::move(definition));
        pushId(L, id);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_configure_dynamic_construct_liquid_port(lua_State *L)
{
    try {
        if (!lua_istable(L, 1))
            throw std::invalid_argument("liquid port definition must be a table");
        navycraft::ConstructLiquidPortDefinition definition;
        if (const auto id = readOptionalIdField(L, 1, "id"))
            definition.id = *id;
        const auto compartment = readOptionalIdField(L, 1, "compartment_id");
        if (!compartment)
            throw std::invalid_argument("liquid port compartment_id is required");
        definition.compartment_id = *compartment;
        getField(L, 1, "position");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            throw std::invalid_argument("liquid port position is required");
        }
        definition.position = readLocalPos(L, -1);
        lua_pop(L, 1);
        const std::string kind = readStringField(L, 1, "kind");
        if (kind.empty() || kind == "source" || kind == "pump_in")
            definition.kind = navycraft::ConstructLiquidPortKind::Source;
        else if (kind == "drain" || kind == "pump_out")
            definition.kind = navycraft::ConstructLiquidPortKind::Drain;
        else if (kind == "breach" || kind == "leak")
            definition.kind = navycraft::ConstructLiquidPortKind::Breach;
        else
            throw std::invalid_argument("unknown liquid port kind");
        const std::string liquid_name = readStringField(L, 1, "liquid");
        if (!liquid_name.empty())
            definition.liquid_name = liquid_name;
        definition.units_per_second = readNumberField(L, 1, "rate", 0.0);
        definition.enabled = readBoolField(L, 1, "enabled", definition.enabled);
        const auto id = navycraft::runtimeConstructLiquidEngine().configurePort(
            std::move(definition));
        pushId(L, id);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_set_dynamic_construct_liquid(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        const auto position = readLocalPos(L, 2);
        const std::string liquid = lua_isstring(L, 3) ? lua_tostring(L, 3) : "water";
        const auto raw_level = static_cast<lua_Integer>(luaL_checknumber(L, 4));
        if (raw_level < 0 || raw_level > 8)
            throw std::invalid_argument("liquid level must be between zero and eight");
        const auto flags = lua_gettop(L) >= 5 ?
            static_cast<lua_Integer>(luaL_checknumber(L, 5)) : 0;
        if (flags < 0 || flags > 255)
            throw std::invalid_argument("liquid flags must fit in one byte");
        lua_pushboolean(L, navycraft::runtimeConstructLiquidEngine().setCell(
            construct_id, position, liquid, static_cast<std::uint8_t>(raw_level),
            static_cast<std::uint8_t>(flags)));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_step_dynamic_construct_liquids(lua_State *L)
{
    try {
        const double delta = luaL_checknumber(L, 1);
        const auto result = navycraft::runtimeConstructLiquidEngine().step(delta);
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, result.escaped_units);
        lua_setfield(L, -2, "escaped_units");
        lua_createtable(L, static_cast<int>(result.changed_cells.size()), 0);
        int index = 1;
        for (const auto &position : result.changed_cells) {
            pushVec3(L, {static_cast<double>(position.x),
                static_cast<double>(position.y), static_cast<double>(position.z)});
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "changed_cells");
        lua_createtable(L, static_cast<int>(result.compartments.size()), 0);
        index = 1;
        for (const auto &status : result.compartments) {
            lua_createtable(L, 0, 9);
            pushId(L, status.definition.id);
            lua_setfield(L, -2, "id");
            pushId(L, status.definition.construct_id);
            lua_setfield(L, -2, "construct_id");
            lua_pushlstring(L, status.definition.name.data(), status.definition.name.size());
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, static_cast<lua_Integer>(status.total_units));
            lua_setfield(L, -2, "total_units");
            lua_pushinteger(L, static_cast<lua_Integer>(status.capacity_units));
            lua_setfield(L, -2, "capacity_units");
            lua_pushnumber(L, status.fill_fraction);
            lua_setfield(L, -2, "fill_fraction");
            lua_pushnumber(L, status.escaped_units);
            lua_setfield(L, -2, "escaped_units");
            pushVec3(L, status.centre_of_mass_local);
            lua_setfield(L, -2, "centre_of_mass_local");
            lua_pushboolean(L, status.mixed);
            lua_setfield(L, -2, "mixed");
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "compartments");
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_liquids(lua_State *L)
{
    try {
        const auto construct_id = readId(L, 1);
        const auto construct = navycraft::runtimeConstructRegistry().find(construct_id);
        if (!construct)
            return fail(L, "construct not found");
        lua_createtable(L, 0, 2);
        const auto cells = construct->liquids();
        lua_createtable(L, static_cast<int>(cells.size()), 0);
        int index = 1;
        for (const auto &entry : cells) {
            lua_createtable(L, 0, 5);
            pushVec3(L, {static_cast<double>(entry.position.x),
                static_cast<double>(entry.position.y),
                static_cast<double>(entry.position.z)});
            lua_setfield(L, -2, "position");
            lua_pushlstring(L, entry.liquid.liquid_name.data(),
                entry.liquid.liquid_name.size());
            lua_setfield(L, -2, "liquid");
            lua_pushinteger(L, entry.liquid.level);
            lua_setfield(L, -2, "level");
            lua_pushinteger(L, entry.liquid.flags);
            lua_setfield(L, -2, "flags");
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "cells");
        const auto compartments = navycraft::runtimeConstructLiquidEngine().compartments(
            construct_id);
        lua_createtable(L, static_cast<int>(compartments.size()), 0);
        index = 1;
        for (const auto &status : compartments) {
            lua_createtable(L, 0, 6);
            pushId(L, status.definition.id);
            lua_setfield(L, -2, "id");
            lua_pushlstring(L, status.definition.name.data(), status.definition.name.size());
            lua_setfield(L, -2, "name");
            lua_pushnumber(L, status.fill_fraction);
            lua_setfield(L, -2, "fill_fraction");
            lua_pushinteger(L, static_cast<lua_Integer>(status.total_units));
            lua_setfield(L, -2, "total_units");
            lua_pushinteger(L, static_cast<lua_Integer>(status.capacity_units));
            lua_setfield(L, -2, "capacity_units");
            pushVec3(L, status.centre_of_mass_local);
            lua_setfield(L, -2, "centre_of_mass_local");
            lua_rawseti(L, -2, index++);
        }
        lua_setfield(L, -2, "compartments");
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_register_dynamic_construct_special_node(lua_State *L)
{
    try {
        if (!lua_istable(L, 1))
            throw std::invalid_argument("special node definition must be a table");
        navycraft::ConstructSpecialNodeDefinition definition;
        definition.node_name = readStringField(L, 1, "name");
        definition.climbable = readBoolField(L, 1, "climbable", false);
        definition.breathable = readBoolField(L, 1, "breathable", true);
        definition.liquid_permeable = readBoolField(L, 1, "liquid_permeable", false);
        definition.attachable_platform = readBoolField(L, 1,
            "attachable_platform", true);
        definition.damage_per_second = readNumberField(L, 1,
            "damage_per_second", 0.0);
        definition.conveyor_acceleration = readNumberField(L, 1,
            "conveyor_acceleration", definition.conveyor_acceleration);
        getField(L, 1, "conveyor_velocity");
        if (lua_istable(L, -1))
            definition.conveyor_velocity_local = readVec3(L, -1);
        lua_pop(L, 1);
        lua_pushboolean(L, navycraft::runtimeConstructSpecialNodeEngine().registerDefinition(
            std::move(definition)));
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_dynamic_construct_protocol_version(lua_State *L)
{
    lua_pushinteger(L, 16);
    return 1;
}

int ModApiNavyCraft::l_set_chat_style(lua_State *L)
{
    try {
        if (!lua_istable(L, 1))
            throw std::invalid_argument("chat style must be a table");
        navycraft::ChatStyle style = navycraft::currentChatStyle();
        getField(L, 1, "recent");
        if (lua_istable(L, -1))
            readRecentChatStyle(L, -1, style);
        lua_pop(L, 1);
        getField(L, 1, "console");
        if (lua_istable(L, -1))
            readConsoleChatStyle(L, -1, style);
        lua_pop(L, 1);
        navycraft::setChatStyle(std::move(style));
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception &error) {
        return fail(L, error.what());
    }
}

int ModApiNavyCraft::l_get_chat_style(lua_State *L)
{
    const navycraft::ChatStyle style = navycraft::currentChatStyle();
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, static_cast<lua_Number>(style.revision));
    lua_setfield(L, -2, "revision");
    pushRecentChatStyle(L, style);
    lua_setfield(L, -2, "recent");
    pushConsoleChatStyle(L, style);
    lua_setfield(L, -2, "console");
    return 1;
}

void ModApiNavyCraft::Initialize(lua_State *L, int top)
{
    registerFunction(L, "set_chat_style", l_set_chat_style, top);
    registerFunction(L, "get_chat_style", l_get_chat_style, top);
    registerFunction(L, "create_dynamic_construct", l_create_dynamic_construct, top);
    registerFunction(L, "get_dynamic_construct", l_get_dynamic_construct, top);
    registerFunction(L, "set_dynamic_construct_transform", l_set_dynamic_construct_transform, top);
    registerFunction(L, "set_dynamic_construct_velocity", l_set_dynamic_construct_velocity, top);
    registerFunction(L, "remove_dynamic_construct", l_remove_dynamic_construct, top);
    registerFunction(L, "list_dynamic_constructs", l_list_dynamic_constructs, top);
    registerFunction(L, "raycast_dynamic_constructs", l_raycast_dynamic_constructs, top);
    registerFunction(L, "get_dynamic_construct_surface_velocity",
        l_get_dynamic_construct_surface_velocity, top);
    registerFunction(L, "set_dynamic_construct_node", l_set_dynamic_construct_node, top);
    registerFunction(L, "get_dynamic_construct_sections", l_get_dynamic_construct_sections, top);
    registerFunction(L, "get_dynamic_construct_protocol_version",
        l_get_dynamic_construct_protocol_version, top);
    registerFunction(L, "get_dynamic_construct_node_state",
        l_get_dynamic_construct_node_state, top);
    registerFunction(L, "set_dynamic_construct_metadata",
        l_set_dynamic_construct_metadata, top);
    registerFunction(L, "set_dynamic_construct_inventory",
        l_set_dynamic_construct_inventory, top);
    registerFunction(L, "start_dynamic_construct_timer",
        l_start_dynamic_construct_timer, top);
    registerFunction(L, "stop_dynamic_construct_timer",
        l_stop_dynamic_construct_timer, top);
    registerFunction(L, "poll_dynamic_construct_events",
        l_poll_dynamic_construct_events, top);
    registerFunction(L, "resolve_dynamic_construct_timer",
        l_resolve_dynamic_construct_timer, top);
    registerFunction(L, "resolve_dynamic_construct_mutation",
        l_resolve_dynamic_construct_mutation, top);
    registerFunction(L, "initialise_dynamic_construct_persistence",
        l_initialise_dynamic_construct_persistence, top);
    registerFunction(L, "sync_dynamic_construct_persistence",
        l_sync_dynamic_construct_persistence, top);
    registerFunction(L, "get_dynamic_construct_actions",
        l_get_dynamic_construct_actions, top);
    registerFunction(L, "rollback_dynamic_construct_action",
        l_rollback_dynamic_construct_action, top);
    registerFunction(L, "dynamic_construct_local_to_world",
        l_dynamic_construct_local_to_world, top);
    registerFunction(L, "emit_dynamic_construct_effect",
        l_emit_dynamic_construct_effect, top);
    registerFunction(L, "spawn_dynamic_construct_projectile",
        l_spawn_dynamic_construct_projectile, top);
    registerFunction(L, "step_dynamic_construct_projectiles",
        l_step_dynamic_construct_projectiles, top);
    registerFunction(L, "get_dynamic_construct_projectiles",
        l_get_dynamic_construct_projectiles, top);
    registerFunction(L, "impact_dynamic_construct_projectile",
        l_impact_dynamic_construct_projectile, top);
    registerFunction(L, "observe_dynamic_construct_target",
        l_observe_dynamic_construct_target, top);
    registerFunction(L, "solve_dynamic_construct_fire_control",
        l_solve_dynamic_construct_fire_control, top);
    registerFunction(L, "configure_dynamic_construct_battery",
        l_configure_dynamic_construct_battery, top);
    registerFunction(L, "remove_dynamic_construct_battery",
        l_remove_dynamic_construct_battery, top);
    registerFunction(L, "step_dynamic_construct_fire_control",
        l_step_dynamic_construct_fire_control, top);
    registerFunction(L, "get_dynamic_construct_fire_control_tracks",
        l_get_dynamic_construct_fire_control_tracks, top);
    registerFunction(L, "get_dynamic_construct_fire_control_batteries",
        l_get_dynamic_construct_fire_control_batteries, top);
    registerFunction(L, "configure_dynamic_construct_navigation",
        l_configure_dynamic_construct_navigation, top);
    registerFunction(L, "set_dynamic_construct_route",
        l_set_dynamic_construct_route, top);
    registerFunction(L, "observe_dynamic_construct_obstacle",
        l_observe_dynamic_construct_obstacle, top);
    registerFunction(L, "step_dynamic_construct_navigation",
        l_step_dynamic_construct_navigation, top);
    registerFunction(L, "get_dynamic_construct_navigation",
        l_get_dynamic_construct_navigation, top);
    registerFunction(L, "set_dynamic_construct_navigation_enabled",
        l_set_dynamic_construct_navigation_enabled, top);
    registerFunction(L, "clear_dynamic_construct_navigation",
        l_clear_dynamic_construct_navigation, top);
    registerFunction(L, "step_dynamic_construct_structure",
        l_step_dynamic_construct_structure, top);
    registerFunction(L, "get_dynamic_construct_structure",
        l_get_dynamic_construct_structure, top);
    registerFunction(L, "force_dynamic_construct_split",
        l_force_dynamic_construct_split, top);
    registerFunction(L, "set_dynamic_construct_flooding",
        l_set_dynamic_construct_flooding, top);
    registerFunction(L, "configure_dynamic_construct_joint",
        l_configure_dynamic_construct_joint, top);
    registerFunction(L, "set_dynamic_construct_joint",
        l_set_dynamic_construct_joint, top);
    registerFunction(L, "remove_dynamic_construct_joint",
        l_remove_dynamic_construct_joint, top);
    registerFunction(L, "configure_dynamic_construct_turret",
        l_configure_dynamic_construct_turret, top);
    registerFunction(L, "aim_dynamic_construct_turret",
        l_aim_dynamic_construct_turret, top);
    registerFunction(L, "clear_dynamic_construct_turret_target",
        l_clear_dynamic_construct_turret_target, top);
    registerFunction(L, "get_dynamic_construct_articulations",
        l_get_dynamic_construct_articulations, top);
    registerFunction(L, "step_dynamic_construct_articulations",
        l_step_dynamic_construct_articulations, top);
    registerFunction(L, "dynamic_construct_line_of_fire",
        l_dynamic_construct_line_of_fire, top);
    registerFunction(L, "configure_dynamic_construct_liquid_compartment",
        l_configure_dynamic_construct_liquid_compartment, top);
    registerFunction(L, "configure_dynamic_construct_liquid_port",
        l_configure_dynamic_construct_liquid_port, top);
    registerFunction(L, "set_dynamic_construct_liquid",
        l_set_dynamic_construct_liquid, top);
    registerFunction(L, "step_dynamic_construct_liquids",
        l_step_dynamic_construct_liquids, top);
    registerFunction(L, "get_dynamic_construct_liquids",
        l_get_dynamic_construct_liquids, top);
    registerFunction(L, "register_dynamic_construct_special_node",
        l_register_dynamic_construct_special_node, top);
}

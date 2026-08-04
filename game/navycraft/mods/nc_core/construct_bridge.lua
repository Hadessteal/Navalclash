-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Native-only construct bridge. The temporary Lua entity renderer is not
-- permitted in production or native-engine testing builds.
local M = {}
local construct_state
local REQUIRED_PROTOCOL = 16

function M.set_state(value) construct_state = value end

function M.protocol_version()
    if type(core.get_dynamic_construct_protocol_version) ~= "function" then
        return 0
    end
    return tonumber(core.get_dynamic_construct_protocol_version()) or 0
end

function M.engine_available()
    return type(core.create_dynamic_construct) == "function" and
        type(core.remove_dynamic_construct) == "function" and
        M.protocol_version() >= REQUIRED_PROTOCOL
end

function M.native_renderer_available()
    return M.engine_available()
end

function M.require_native_engine()
    local version = M.protocol_version()
    if M.engine_available() then return true end
    return false, string.format(
        "NavyCraft requires the patched Luanti native construct engine " ..
        "(protocol %d; detected %d). The temporary Lua renderer has been removed.",
        REQUIRED_PROTOCOL, version)
end

function M.launch(player, scan_result)
    local available, error_message = M.require_native_engine()
    if not available then return nil, error_message end

    local native_id, native_error = core.create_dynamic_construct({
        origin = scan_result.origin,
        owner = player:get_player_name(),
        nodes = scan_result.nodes,
    })
    if not native_id then
        return nil, native_error or "native engine rejected construct"
    end

    -- The Lua object is persistent gameplay metadata only. Rendering, collision
    -- and rider motion remain native engine responsibilities.
    local state_id, state_error = construct_state.launch(player, scan_result, native_id)
    if not state_id then
        core.remove_dynamic_construct(native_id)
        return nil, state_error
    end

    return state_id, string.format(
        "native Luanti construct engine (protocol %d)", M.protocol_version())
end

M.REQUIRED_PROTOCOL = REQUIRED_PROTOCOL
return M

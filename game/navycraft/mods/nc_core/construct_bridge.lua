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

local function compute_pivot(scan_result)
    local function round(value) return math.floor(value + 0.5) end
    return {
        x = round((scan_result.minp.x + scan_result.maxp.x) / 2),
        y = round((scan_result.minp.y + scan_result.maxp.y) / 2),
        z = round((scan_result.minp.z + scan_result.maxp.z) / 2),
    }
end

local function normalise_yaw(yaw)
    local full = math.pi * 2
    yaw = (tonumber(yaw) or 0) % full
    if yaw < 0 then yaw = yaw + full end
    return yaw
end

local function sign(value)
    if value < 0 then return -1 elseif value > 0 then return 1 end
    return 0
end

local function rotate(local_pos, yaw)
    local cosine, sine = math.cos(yaw), math.sin(yaw)
    return {
        x = cosine * local_pos.x - sine * local_pos.z,
        y = local_pos.y,
        z = sine * local_pos.x + cosine * local_pos.z,
    }
end

local function cardinal_direction(dir, fallback)
    dir = dir or fallback or {x = 0, y = 0, z = 1}
    local x = tonumber(dir.x) or 0
    local z = tonumber(dir.z) or 0
    if math.abs(x) >= math.abs(z) and math.abs(x) >= 0.5 then
        return {x = sign(x), y = 0, z = 0}
    elseif math.abs(z) >= 0.5 then
        return {x = 0, y = 0, z = sign(z)}
    end
    return {x = fallback and fallback.x or 0, y = 0, z = fallback and fallback.z or 1}
end

local function facedir_vector(param2)
    if type(core.facedir_to_dir) == "function" then
        local dir = core.facedir_to_dir((tonumber(param2) or 0) % 32)
        if dir then return cardinal_direction(dir) end
    end
    return {x = 0, y = 0, z = 1}
end

local function local_direction_from_world(dir, yaw)
    if not dir then return nil end
    return cardinal_direction(rotate({x = dir.x or 0, y = 0, z = dir.z or 0}, -(yaw or 0)))
end

local function local_param2_from_world(node, yaw)
    local param2 = tonumber(node.param2) or 0
    local definition = core.registered_nodes[node.name] or {}
    if definition.paramtype2 ~= "facedir" or type(core.dir_to_facedir) ~= "function" then
        return param2
    end
    local local_dir = local_direction_from_world(facedir_vector(param2), yaw)
    local ok, local_param2 = pcall(core.dir_to_facedir, local_dir, false)
    return ok and local_param2 or param2
end

local function initial_yaw_from_helm(scan_result)
    for _, node in ipairs(scan_result.nodes or {}) do
        local def = core.registered_nodes[node.name] or {}
        if def._navycraft_component == "helm" or def._navycraft_helm then
            if type(core.facedir_to_dir) == "function" and type(core.dir_to_yaw) == "function" then
                local dir = core.facedir_to_dir((tonumber(node.param2) or 0) % 32)
                if dir then
                    return normalise_yaw(core.dir_to_yaw({x = dir.x or 0, y = 0, z = dir.z or 0}))
                end
            end
            return 0
        end
    end
    return 0
end

function M.launch(player, scan_result)
    local available, error_message = M.require_native_engine()
    if not available then return nil, error_message end

    local initial_yaw = initial_yaw_from_helm(scan_result)
    scan_result.navycraft_initial_yaw = initial_yaw
    local origin = compute_pivot(scan_result)
    local native_nodes = {}
    for index, node in ipairs(scan_result.nodes or {}) do
        local local_pos = rotate(vector.subtract(node.pos, origin), -initial_yaw)
        native_nodes[index] = {
            pos = vector.add(origin, local_pos),
            name = node.name,
            param1 = node.param1 or 0,
            param2 = local_param2_from_world(node, initial_yaw),
            metadata = node.metadata or "",
        }
    end
    local native_id, native_error = core.create_dynamic_construct({
        origin = origin,
        owner = player:get_player_name(),
        yaw = initial_yaw,
        nodes = native_nodes,
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

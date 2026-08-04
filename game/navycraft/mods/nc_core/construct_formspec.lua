-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Formspec and detached-inventory bridge for moving construct-local nodes.

local M = {sessions = {}, serial = 0}

local function sanitize(value)
    return tostring(value or ""):gsub("[^%w_]", "_")
end

local function copy(values)
    local result = {}
    for index, value in ipairs(values or {}) do result[index] = value end
    return result
end

local function protected(context)
    if context.actor_name == "" or type(core.is_protected) ~= "function" then return false end
    local position = context.world_for_local(context.node_pos)
    if not position then return false end
    position = vector.round(position)
    if core.is_protected(position, context.actor_name) then
        if type(core.record_protection_violation) == "function" then
            core.record_protection_violation(position, context.actor_name)
        end
        return true
    end
    return false
end

local function sync_list(session, list_name)
    local detached = core.get_inventory({type = "detached", name = session.detached_name})
    if not detached then return false end
    local values = detached:get_list(list_name) or {}
    local stacks = {}
    for index, stack in ipairs(values) do stacks[index] = stack:to_string() end
    return session.context.inventory:set_list(
        list_name, stacks, detached:get_width(list_name))
end

local function sync_all(session)
    for _, list_name in ipairs(session.list_names) do sync_list(session, list_name) end
end

local function allowed(session, action, list_name, index, stack, player)
    if not player or player:get_player_name() ~= session.player_name or protected(session.context) then
        return 0
    end
    local definition = core.registered_nodes[session.context.node_name]
    local callback = definition and definition._navycraft_allow_inventory_action
    if callback then
        local ok, result = pcall(callback, session.context, action, list_name, index, stack, player)
        if not ok then
            core.log("error", "[NavyCraft] construct inventory permission callback failed: " .. tostring(result))
            return 0
        end
        if type(result) == "number" then return math.max(0, math.floor(result)) end
        if result == false then return 0 end
    end
    return stack and stack:get_count() or 0
end

local function remove_session(formname)
    local session = M.sessions[formname]
    if not session then return end
    M.sessions[formname] = nil
    if type(core.remove_detached_inventory) == "function" then
        core.remove_detached_inventory(session.detached_name)
    end
end

function M.show(context, specification)
    specification = specification or {}
    local player = context.player
    if not player then return nil, "player is unavailable" end
    M.serial = M.serial + 1
    local player_name = player:get_player_name()
    local formname = string.format("navycraft:construct:%s:%d", sanitize(context.construct_id), M.serial)
    local detached_name = string.format("navycraft_construct_%s_%s_%d",
        sanitize(player_name), sanitize(context.construct_id), M.serial)
    local list_names = copy(specification.lists or {"main"})
    local session = {
        context = context,
        player_name = player_name,
        formname = formname,
        detached_name = detached_name,
        list_names = list_names,
        on_receive_fields = specification.on_receive_fields,
    }

    local callbacks = {
        allow_move = function(_, from_list, from_index, to_list, _, count, mover)
            local count_stack = {get_count = function() return count end}
            local permitted = allowed(session, "move", from_list, from_index,
                count_stack, mover)
            if permitted <= 0 or allowed(session, "move", to_list, 1,
                    count_stack, mover) <= 0 then return 0 end
            return math.min(count, permitted)
        end,
        allow_put = function(_, list_name, index, stack, putter)
            return allowed(session, "put", list_name, index, stack, putter)
        end,
        allow_take = function(_, list_name, index, stack, taker)
            return allowed(session, "take", list_name, index, stack, taker)
        end,
        on_move = function() sync_all(session) end,
        on_put = function(_, list_name) sync_list(session, list_name) end,
        on_take = function(_, list_name) sync_list(session, list_name) end,
    }
    local detached = core.create_detached_inventory(detached_name, callbacks, player_name)
    for _, list_name in ipairs(list_names) do
        local stacks = context.inventory:get_list(list_name) or {}
        detached:set_size(list_name, #stacks)
        detached:set_width(list_name, context.inventory:get_width(list_name))
        detached:set_list(list_name, stacks)
    end

    local location = "detached:" .. detached_name
    local formspec = specification.formspec
    if type(formspec) == "function" then formspec = formspec(location, context) end
    if type(formspec) ~= "string" then
        local first = list_names[1] or "main"
        formspec = "formspec_version[4]size[10,8]label[0.4,0.4;NavyCraft Construct Inventory]" ..
            "list[" .. location .. ";" .. first .. ";0.4,1.0;9,3;]" ..
            "list[current_player;main;0.4,4.2;8,4;]listring[]"
    end
    M.sessions[formname] = session
    core.show_formspec(player_name, formname, formspec)
    return formname, location
end

core.register_on_player_receive_fields(function(player, formname, fields)
    local session = M.sessions[formname]
    if not session or player:get_player_name() ~= session.player_name then return false end
    if fields.quit then
        sync_all(session)
        remove_session(formname)
        return true
    end
    sync_all(session)
    local definition = core.registered_nodes[session.context.node_name]
    local callback = session.on_receive_fields or
        (definition and definition._navycraft_on_receive_fields)
    if callback then
        local ok, error_message = xpcall(function()
            callback(session.context, fields)
        end, debug.traceback)
        if not ok then
            core.log("error", "[NavyCraft] construct formspec callback failed: " .. tostring(error_message))
        end
    end
    return true
end)

core.register_on_leaveplayer(function(player)
    local player_name = player:get_player_name()
    local remove = {}
    for formname, session in pairs(M.sessions) do
        if session.player_name == player_name then remove[#remove + 1] = formname end
    end
    for _, formname in ipairs(remove) do remove_session(formname) end
end)

return M

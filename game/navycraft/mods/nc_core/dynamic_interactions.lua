-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Construct-local callbacks, transactions, protection, inventories and persistence.

local modpath = core.get_modpath(core.get_current_modname())
local formspecs = dofile(modpath .. "/construct_formspec.lua")
local M = {handlers = {}, formspecs = formspecs}

local function construct_state()
    if not navycraft then return nil end
    return navycraft.construct_state or navycraft.preview
end

local function copy_array(values)
    local result = {}
    for index, value in ipairs(values or {}) do result[index] = value end
    return result
end

local function state_for(context)
    if type(core.get_dynamic_construct_node_state) ~= "function" then
        return {fields = {}, inventories = {}, timer = {}, revision = 0}
    end
    local state = core.get_dynamic_construct_node_state(context.construct_id, context.node_pos)
    if type(state) ~= "table" then
        return {fields = {}, inventories = {}, timer = {}, revision = 0}
    end
    state.fields = state.fields or {}
    state.inventories = state.inventories or {}
    state.timer = state.timer or {}
    return state
end

local function make_meta(context)
    return {
        get_string = function(_, key)
            local value = state_for(context).fields[key]
            return value == nil and "" or tostring(value)
        end,
        set_string = function(_, key, value)
            return core.set_dynamic_construct_metadata(
                context.construct_id, context.node_pos, key, tostring(value or ""))
        end,
        get_int = function(self, key) return math.floor(tonumber(self:get_string(key)) or 0) end,
        set_int = function(self, key, value) return self:set_string(key, math.floor(tonumber(value) or 0)) end,
        get_float = function(self, key) return tonumber(self:get_string(key)) or 0 end,
        set_float = function(self, key, value) return self:set_string(key, tonumber(value) or 0) end,
        contains = function(_, key) return state_for(context).fields[key] ~= nil end,
        to_table = function() return state_for(context) end,
    }
end

local function make_inventory(context)
    local inventory = {}
    function inventory:get_list(name)
        local list = state_for(context).inventories[name]
        return list and copy_array(list.stacks) or nil
    end
    function inventory:get_width(name)
        local list = state_for(context).inventories[name]
        return list and tonumber(list.width) or 0
    end
    function inventory:set_list(name, stacks, width)
        return core.set_dynamic_construct_inventory(context.construct_id, context.node_pos, name, {
            width = math.max(0, math.floor(tonumber(width) or 0)),
            stacks = copy_array(stacks),
        })
    end
    function inventory:get_stack(name, index)
        local list = self:get_list(name)
        return list and list[index] or ""
    end
    function inventory:set_stack(name, index, stack)
        index = math.max(1, math.floor(tonumber(index) or 1))
        local stacks = self:get_list(name) or {}
        while #stacks < index do stacks[#stacks + 1] = "" end
        stacks[index] = type(stack) == "userdata" and stack:to_string() or tostring(stack or "")
        return self:set_list(name, stacks, self:get_width(name))
    end
    function inventory:clear(name)
        return core.set_dynamic_construct_inventory(context.construct_id, context.node_pos, name, nil)
    end
    return inventory
end

local function make_timer(context)
    return {
        start = function(_, timeout, elapsed)
            return core.start_dynamic_construct_timer(
                context.construct_id, context.node_pos, timeout, elapsed or 0)
        end,
        stop = function() return core.stop_dynamic_construct_timer(context.construct_id, context.node_pos) end,
        is_started = function() return state_for(context).timer.active == true end,
        get_timeout = function() return tonumber(state_for(context).timer.timeout) or 0 end,
        get_elapsed = function() return tonumber(state_for(context).timer.elapsed) or 0 end,
    }
end

local callback_names = {
    start_dig = "_navycraft_on_punch",
    stop_dig = "_navycraft_on_stop_dig",
    use = "_navycraft_on_use",
    activate = "_navycraft_on_rightclick",
    receive_fields = "_navycraft_on_receive_fields",
    timer = "_navycraft_on_timer",
}

local function world_for_local(context, position)
    if type(core.dynamic_construct_local_to_world) == "function" then
        local world = core.dynamic_construct_local_to_world(context.construct_id, {
            x = position.x + 0.5, y = position.y + 0.5, z = position.z + 0.5,
        })
        if type(world) == "table" then return vector.new(world) end
    end
    return vector.new(context.world_point or {})
end

local function make_context(event)
    local context = {
        event_id = event.event_id,
        construct_id = event.construct_id,
        action = event.action,
        node_pos = vector.new(event.node_pos or {}),
        adjacent_pos = vector.new(event.adjacent_pos or {}),
        world_point = vector.new(event.world_point or {}),
        normal = vector.new(event.normal or {}),
        actor_name = event.actor or "",
        wielded_item = event.wielded_item or "",
        fields = event.fields or {},
        elapsed = tonumber(event.elapsed) or 0,
        timeout = tonumber(event.timeout) or 0,
        node_name = event.node_name or "",
    }
    context.player = context.actor_name ~= "" and core.get_player_by_name(context.actor_name) or nil
    context.meta = make_meta(context)
    context.inventory = make_inventory(context)
    context.timer = make_timer(context)
    context.state = function() return state_for(context) end
    context.world_for_local = function(_, position) return world_for_local(context, position) end
    context.show_formspec = function(_, specification) return formspecs.show(context, specification) end
    context.get_game_construct = function()
        local state = construct_state()
        if not state or not state.get_by_id then return nil end
        return state.get_by_id("native-" .. tostring(context.construct_id))
    end
    return context
end

local function context_at(context, position, node_name)
    local event = {
        event_id = context.event_id, construct_id = context.construct_id,
        action = context.action, node_pos = position, adjacent_pos = position,
        world_point = world_for_local(context, position), normal = context.normal,
        actor = context.actor_name, wielded_item = context.wielded_item,
        fields = context.fields, node_name = node_name,
    }
    return make_context(event)
end

local function log_error(message)
    core.log("error", "[NavyCraft] dynamic construct callback failed: " .. tostring(message))
end

local function invoke(callback, context, event)
    local ok, result = xpcall(function() return callback(context, event) end, debug.traceback)
    if not ok then log_error(result); return nil, false end
    return result, true
end

local function itemstack(value)
    if type(ItemStack) == "function" then return ItemStack(value or "") end
    return nil
end

local function creative(player_name)
    return type(core.is_creative_enabled) == "function" and core.is_creative_enabled(player_name)
end

local function protected(context, position)
    if context.actor_name == "" or type(core.is_protected) ~= "function" then return false end
    local world = vector.round(world_for_local(context, position))
    if not core.is_protected(world, context.actor_name) then return false end
    if type(core.record_protection_violation) == "function" then
        core.record_protection_violation(world, context.actor_name)
    end
    return true
end

local function resolve(context, definition)
    if type(core.resolve_dynamic_construct_mutation) ~= "function" then return nil end
    local result, error_message = core.resolve_dynamic_construct_mutation(
        context.construct_id, context.event_id, definition)
    if not result then log_error(error_message or "mutation resolution failed") end
    return result
end

local function get_local_node(construct_id, position)
    if type(core.get_dynamic_construct) ~= "function" then return nil end
    local construct = core.get_dynamic_construct(construct_id, true)
    for _, entry in ipairs(construct and construct.nodes or {}) do
        local pos = entry.pos
        if pos and pos.x == position.x and pos.y == position.y and pos.z == position.z then
            return entry
        end
    end
    return nil
end

local function give_drops(context, drops)
    if #drops == 0 then return end
    local world = world_for_local(context, context.node_pos)
    if type(core.handle_node_drops) == "function" then
        core.handle_node_drops(vector.round(world), drops, context.player)
        return
    end
    local inventory = context.player and context.player:get_inventory()
    for _, drop in ipairs(drops) do
        local leftover = inventory and inventory:add_item("main", drop) or drop
        if leftover and tostring(leftover) ~= "" and type(core.add_item) == "function" then
            core.add_item(world, leftover)
        end
    end
end

local function default_dig(context, event, definition)
    if protected(context, context.node_pos) then
        return resolve(context, {accepted = false, protected_violation = true,
            wielded_item_after = event.wielded_item, reason = "protected"})
    end
    if definition and definition._navycraft_can_dig then
        local allowed, ok = invoke(definition._navycraft_can_dig, context, event)
        if not ok or allowed == false then
            return resolve(context, {accepted = false, wielded_item_after = event.wielded_item,
                reason = "node refused digging"})
        end
    end

    local tool = context.player and context.player:get_wielded_item() or itemstack(event.wielded_item)
    local tool_name = tool and tool:get_name() or ""
    local drops = type(core.get_node_drops) == "function" and
        core.get_node_drops(context.node_name, tool_name) or {context.node_name}
    local wear = 0
    if definition and type(core.get_dig_params) == "function" and tool and
            type(tool.get_tool_capabilities) == "function" then
        local params = core.get_dig_params(definition.groups or {},
            tool:get_tool_capabilities(), tool:get_wear())
        if not params or params.diggable == false then
            return resolve(context, {accepted = false, wielded_item_after = tool:to_string(),
                reason = "tool cannot dig node"})
        end
        wear = math.max(0, math.floor(tonumber(params.wear) or 0))
    end

    local custom = definition and definition._navycraft_on_dig
    if custom then
        local result, ok = invoke(custom, context, event)
        if not ok or result == false then
            return resolve(context, {accepted = false,
                wielded_item_after = tool and tool:to_string() or event.wielded_item,
                reason = "dig callback rejected"})
        elseif type(result) == "table" then
            if result.drops then drops = copy_array(result.drops) end
            if result.wear ~= nil then wear = math.max(0, math.floor(tonumber(result.wear) or 0)) end
        end
    end

    if tool and context.player and not creative(context.actor_name) and wear > 0 then
        tool:add_wear(wear)
    end
    local tool_after = tool and tool:to_string() or event.wielded_item
    local committed = resolve(context, {accepted = true, wielded_item_after = tool_after,
        drops = drops, reason = "dig"})
    if committed and committed.node_changed then
        if context.player and tool then context.player:set_wielded_item(tool) end
        give_drops(context, drops)
        local state = construct_state()
        if state and state.sync_native_dig then
            state.sync_native_dig(event.construct_id, context.node_pos)
        end
        if definition and definition._navycraft_on_destruct then
            invoke(definition._navycraft_on_destruct, context, event)
        end
        if definition and definition._navycraft_after_destruct then
            invoke(definition._navycraft_after_destruct, context, event)
        end
    end
    return committed
end

local function placement_param2(definition, player, normal, stack)
    local paramtype2 = definition.paramtype2
    local param2 = 0
    if (paramtype2 == "facedir" or paramtype2 == "colorfacedir") and
            player and type(core.dir_to_facedir) == "function" then
        param2 = core.dir_to_facedir(player:get_look_dir(), false)
    elseif (paramtype2 == "wallmounted" or paramtype2 == "colorwallmounted") and
            type(core.dir_to_wallmounted) == "function" then
        param2 = core.dir_to_wallmounted(normal)
    end
    if paramtype2 and paramtype2:sub(1, 5) == "color" and stack and
            type(stack.get_meta) == "function" then
        local palette = stack:get_meta():get_int("palette_index")
        if paramtype2 == "colorfacedir" then param2 = (palette - palette % 32) + param2
        elseif paramtype2 == "colorwallmounted" then param2 = (palette - palette % 8) + param2
        else param2 = palette end
    end
    return math.max(0, math.min(255, param2))
end

local function default_place(context, event, clicked_definition)
    if clicked_definition and clicked_definition._navycraft_on_rightclick then
        local handled, ok = invoke(clicked_definition._navycraft_on_rightclick, context, event)
        if ok and handled ~= false then
            return resolve(context, {accepted = false, wielded_item_after = event.wielded_item,
                reason = "right-click callback handled interaction"})
        end
    end
    if protected(context, context.adjacent_pos) then
        return resolve(context, {accepted = false, protected_violation = true,
            wielded_item_after = event.wielded_item, reason = "protected"})
    end

    local stack = context.player and context.player:get_wielded_item() or itemstack(event.wielded_item)
    local name = stack and stack:get_name() or tostring(event.wielded_item):match("^%s*([^%s]+)")
    local definition = name and core.registered_nodes[name]
    if not definition then
        return resolve(context, {accepted = false, wielded_item_after = event.wielded_item,
            reason = "wielded item is not a node"})
    end
    local occupied = get_local_node(context.construct_id, context.adjacent_pos)
    local occupied_definition = occupied and core.registered_nodes[occupied.name]
    local replace_existing = occupied ~= nil and occupied_definition and occupied_definition.buildable_to == true
    if occupied and not replace_existing then
        return resolve(context, {accepted = false,
            wielded_item_after = stack and stack:to_string() or event.wielded_item,
            reason = "placement position is occupied"})
    end

    local node = {name = name, param1 = 0,
        param2 = placement_param2(definition, context.player, context.normal, stack), metadata = ""}
    local callback = definition._navycraft_on_place
    if callback then
        local result, ok = invoke(callback, context, event)
        if not ok or result == false then
            return resolve(context, {accepted = false,
                wielded_item_after = stack and stack:to_string() or event.wielded_item,
                reason = "placement callback rejected"})
        elseif type(result) == "table" then
            if result.node then node = result.node end
            if result.replace_existing ~= nil then replace_existing = result.replace_existing == true end
        end
    end

    local stack_after = stack and stack:to_string() or event.wielded_item
    if stack and context.player and not creative(context.actor_name) then
        stack:take_item(1)
        stack_after = stack:to_string()
    end
    local committed = resolve(context, {accepted = true, replace_existing = replace_existing,
        node = node, wielded_item_after = stack_after, reason = "place"})
    if committed and committed.node_changed then
        if context.player and stack then context.player:set_wielded_item(stack) end
        local placed_context = context_at(context, context.adjacent_pos, node.name)
        if definition._navycraft_on_construct then
            invoke(definition._navycraft_on_construct, placed_context, event)
        end
        if definition._navycraft_after_place_node then
            invoke(definition._navycraft_after_place_node, placed_context, event)
        end
        local state = construct_state()
        if state and state.sync_native_place then
            state.sync_native_place(context.construct_id, context.adjacent_pos,
                node.name, node.param1 or 0, node.param2 or 0, node.metadata or "")
        end
    end
    return committed
end

function M.register_handler(callback)
    assert(type(callback) == "function", "dynamic interaction handler must be a function")
    M.handlers[#M.handlers + 1] = callback
end

function M.poll_once()
    if type(core.poll_dynamic_construct_events) ~= "function" then return 0 end
    local events = core.poll_dynamic_construct_events() or {}
    for _, event in ipairs(events) do
        local context = make_context(event)
        local definition = core.registered_nodes[event.node_name]
        local handled = false
        for _, handler in ipairs(M.handlers) do
            local result, ok = invoke(handler, context, event)
            if ok and result == true then handled = true; break end
        end

        if event.action == "dig" then
            if handled then
                resolve(context, {accepted = false, wielded_item_after = event.wielded_item,
                    reason = "global handler consumed dig"})
            else
                default_dig(context, event, definition)
            end
        elseif event.action == "place" then
            if handled then
                resolve(context, {accepted = false, wielded_item_after = event.wielded_item,
                    reason = "global handler consumed placement"})
            else
                default_place(context, event, definition)
            end
        elseif event.action == "timer" then
            local callback = definition and definition._navycraft_on_timer
            local restart, timeout = false, 0
            if not handled and callback then
                local result, ok = invoke(callback, context, event)
                if ok then
                    restart = result == true or (type(result) == "number" and result > 0)
                    if type(result) == "number" then timeout = result end
                end
            end
            if type(core.resolve_dynamic_construct_timer) == "function" then
                core.resolve_dynamic_construct_timer(event.construct_id, event.event_id, restart, timeout)
            end
        elseif not handled then
            local callback_name = callback_names[event.action]
            local callback = definition and callback_name and definition[callback_name]
            if callback then invoke(callback, context, event) end
        end
    end
    return #events
end

local persistence_elapsed = 0
local persistence_ready = false
if type(core.initialise_dynamic_construct_persistence) == "function" and
        type(core.get_worldpath) == "function" then
    local count, error_message = core.initialise_dynamic_construct_persistence(
        core.get_worldpath() .. "/navycraft_constructs.sqlite")
    if count then
        persistence_ready = true
        core.log("action", "[NavyCraft] loaded " .. tostring(count) .. " native constructs")
    else
        core.log("error", "[NavyCraft] construct persistence disabled: " .. tostring(error_message))
    end
end

core.register_globalstep(function(dtime)
    M.poll_once()
    if persistence_ready then
        persistence_elapsed = persistence_elapsed + (tonumber(dtime) or 0)
        if persistence_elapsed >= 2 then
            persistence_elapsed = 0
            local _, error_message = core.sync_dynamic_construct_persistence()
            if error_message then core.log("error", "[NavyCraft] persistence sync failed: " .. error_message) end
        end
    end
end)

core.register_on_shutdown(function()
    if persistence_ready then pcall(core.sync_dynamic_construct_persistence) end
end)

core.register_chatcommand("nc_history", {
    params = "<construct id> [limit]",
    description = "Show native NavyCraft construct mutation history",
    privs = {server = true},
    func = function(_, parameter)
        if type(core.get_dynamic_construct_actions) ~= "function" then return false, "Native history unavailable" end
        local id, limit = parameter:match("^(%d+)%s*(%d*)$")
        if not id then return false, "Usage: /nc_history <construct id> [limit]" end
        local actions, error_message = core.get_dynamic_construct_actions(id, tonumber(limit) or 20)
        if not actions then return false, error_message end
        local lines = {}
        for _, action in ipairs(actions) do
            lines[#lines + 1] = string.format("#%s %s %s %s%s", tostring(action.action_id),
                action.actor or "", action.action or "", core.pos_to_string(action.node_pos or {}),
                action.protected_violation and " [protected]" or "")
        end
        return true, #lines > 0 and table.concat(lines, "\n") or "No recorded actions"
    end,
})

core.register_chatcommand("nc_rollback", {
    params = "<action id>",
    description = "Restore a native NavyCraft construct to before an action",
    privs = {server = true},
    func = function(_, parameter)
        if type(core.rollback_dynamic_construct_action) ~= "function" then return false, "Native rollback unavailable" end
        local id = parameter:match("^%s*(%d+)%s*$")
        if not id then return false, "Usage: /nc_rollback <action id>" end
        local construct, error_message = core.rollback_dynamic_construct_action(id)
        return construct ~= nil, construct and ("Rolled back construct " .. tostring(construct.id)) or error_message
    end,
})

return M

-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
local modpath = core.get_modpath(core.get_current_modname())
local scan = dofile(modpath .. "/scan.lua")
local construct_state = dofile(modpath .. "/native_construct_state.lua")
local bridge = dofile(modpath .. "/construct_bridge.lua")
bridge.set_state(construct_state)

navycraft = rawget(_G, "navycraft") or {}
navycraft.scan = scan
navycraft.construct = bridge
navycraft.construct_state = construct_state
-- Compatibility alias for source-derived gameplay modules. This is native
-- construct state, not a renderer or movement fallback.
navycraft.preview = construct_state
-- Rider contact, collision and platform displacement are native-engine
-- responsibilities. The former Lua rider-safety teleporter is deliberately
-- not loaded in native builds.
navycraft.rider_safety = nil
navycraft.item_tooltips = dofile(modpath .. "/item_tooltips.lua")
navycraft.minecraft_basics = dofile(modpath .. "/minecraft_basics.lua")
navycraft.dynamic_interactions = dofile(modpath .. "/dynamic_interactions.lua")

core.register_on_mods_loaded(function()
    local ok, message = bridge.require_native_engine()
    if ok then
        core.log("action", string.format(
            "[NavyCraft] native construct engine protocol %d active",
            bridge.protocol_version()))
    else
        core.log("error", "[NavyCraft] " .. message)
    end
end)

local function scan_from_origin(player, origin)
    local result, error_message = scan.connected_nodes(origin, {
        max_nodes = 5000,
        is_allowed = scan.default_allowed,
    })
    if not result then
        core.chat_send_player(player:get_player_name(), "NavyCraft scan failed: " .. error_message)
        return nil
    end
    core.chat_send_player(player:get_player_name(),
        string.format("NavyCraft craft detected: %d nodes; bounds %s to %s",
            #result.nodes, core.pos_to_string(result.minp), core.pos_to_string(result.maxp)))
    return result
end

local function launch_from_origin(player, origin)
    local name = player:get_player_name()
    if construct_state.get_for_owner(name) then
        core.chat_send_player(name, "Launch blocked: dock or remove your active vessel first")
        return false
    end
    if construct_state.find_at_world_node then
        local active = construct_state.find_at_world_node(origin)
        if active then
            core.chat_send_player(name, "Launch blocked: target block is already part of a moving vessel")
            return false
        end
    end
    if construct_state.untracked_runtime_count and construct_state.untracked_runtime_count() > 0 then
        core.chat_send_player(name, "Launch blocked: stale native ship data found. Run /nc_purge_constructs first.")
        return false
    end
    local result = scan_from_origin(player, origin)
    if not result then return false end
    if navycraft.systems and navycraft.systems.analyse then
        local selected_type = "ship"
        for _, entry in ipairs(result.nodes) do
            if entry.name == "nc_navycraft:helm" then
                local decoded = core.deserialize(entry.metadata or "")
                local fields = type(decoded) == "table" and decoded.fields or nil
                if fields and fields.craft_type and fields.craft_type ~= "" then selected_type = fields.craft_type end
                break
            end
        end
        local profile, profile_error = navycraft.systems.analyse(result, selected_type)
        if not profile then
            core.chat_send_player(player:get_player_name(), "NavyCraft validation failed: " .. profile_error)
            return false
        end
        result.navycraft_profile = profile
    end
    local construct_id, mode_or_error = bridge.launch(player, result)
    if not construct_id then
        core.chat_send_player(player:get_player_name(), "Launch failed: " .. mode_or_error)
        return false
    end
    core.chat_send_player(player:get_player_name(),
        "Launched " .. construct_id .. " using " .. mode_or_error)
    return true
end


core.register_node("nc_core:water_source", {
    description = "Ocean Water",
    drawtype = "liquid",
    tiles = {"nc_frame.png^[colorize:#176fa8:205"},
    special_tiles = {{name="nc_frame.png^[colorize:#176fa8:205", backface_culling=false}},
    paramtype = "light",
    walkable = false,
    pointable = false,
    diggable = false,
    buildable_to = true,
    is_ground_content = false,
    drop = "",
    drowning = 1,
    liquidtype = "source",
    liquid_alternative_flowing = "nc_core:water_flowing",
    liquid_alternative_source = "nc_core:water_source",
    liquid_viscosity = 1,
    liquid_range = 8,
    post_effect_color = {a=103,r=24,g=92,b=140},
    groups = {water=3, liquid=3},
})
core.register_node("nc_core:water_flowing", {
    description = "Flowing Ocean Water",
    drawtype = "flowingliquid",
    tiles = {"nc_frame.png^[colorize:#176fa8:205"},
    special_tiles = {
        {name="nc_frame.png^[colorize:#176fa8:205", backface_culling=false},
        {name="nc_frame.png^[colorize:#176fa8:205", backface_culling=true},
    },
    paramtype = "light",
    paramtype2 = "flowingliquid",
    walkable = false,
    pointable = false,
    diggable = false,
    buildable_to = true,
    is_ground_content = false,
    drop = "",
    drowning = 1,
    liquidtype = "flowing",
    liquid_alternative_flowing = "nc_core:water_flowing",
    liquid_alternative_source = "nc_core:water_source",
    liquid_viscosity = 1,
    liquid_range = 8,
    post_effect_color = {a=103,r=24,g=92,b=140},
    groups = {water=3, liquid=3, not_in_creative_inventory=1},
})

core.register_node("nc_core:ground", {
    description = "NavyCraft Development Ground",
    tiles = {"nc_frame.png^[colorize:#554433:120"},
    groups = {cracky = 3},
})

for _, alias in ipairs({
    "mapgen_stone", "mapgen_dirt", "mapgen_dirt_with_grass", "mapgen_sand",
}) do
    core.register_alias(alias, "nc_core:ground")
end
core.register_alias("mapgen_water_source", "nc_core:water_source")
core.register_alias("mapgen_river_water_source", "nc_core:water_source")
for _, alias in ipairs({"mapgen_lava_source", "mapgen_tree", "mapgen_leaves", "mapgen_apple"}) do
    core.register_alias(alias, "air")
end

core.register_node("nc_core:frame", {
    description = "NavyCraft Test Frame",
    tiles = {"nc_frame.png"},
    groups = {cracky = 2, navycraft_hull = 1, navycraft_weight = 25},
})

core.register_node("nc_core:helm", {
    description = "NavyCraft Development Helm",
    tiles = {
        "nc_helm_top.png", "nc_helm_top.png",
        "nc_helm_side.png", "nc_helm_side.png",
        "nc_helm_side.png", "nc_helm_front.png",
    },
    paramtype2 = "facedir",
    groups = {cracky = 2, navycraft_hull = 1, navycraft_component = 1, navycraft_helm = 1, navycraft_weight = 50},
    _navycraft_component = "helm",
    on_rightclick = function(pos, node, clicker)
        if not clicker or not clicker:is_player() then return end
        launch_from_origin(clicker, pos)
    end,
})

core.register_craft({
    output = "nc_core:helm",
    recipe = {
        {"nc_core:frame", "nc_core:frame", "nc_core:frame"},
        {"", "nc_core:frame", ""},
    },
})

local function player_origin(name)
    local player = core.get_player_by_name(name)
    if not player then return nil, "player unavailable" end
    local origin = vector.round(player:get_pos())
    origin.y = origin.y - 1
    return player, origin
end

core.register_chatcommand("nc_scan", {
    description = "Scan the connected craft from the node under your feet",
    privs = {interact = true},
    func = function(name)
        local player, origin = player_origin(name)
        if not player then return false, origin end
        return scan_from_origin(player, origin) ~= nil
    end,
})

core.register_chatcommand("nc_launch", {
    description = "Launch the connected craft from the node under your feet",
    privs = {interact = true},
    func = function(name)
        local player, origin = player_origin(name)
        if not player then return false, origin end
        return launch_from_origin(player, origin)
    end,
})

local function numeric_command(description, setter)
    return {
        params = "<number>",
        description = description,
        privs = {interact = true},
        func = function(name, param)
            local value = tonumber(param)
            if not value then return false, "a number is required" end
            return setter(name, value)
        end,
    }
end

core.register_chatcommand("nc_speed", numeric_command(
    "Set forward speed in blocks per second",
    function(name, value) return construct_state.set_motion(name, value, nil, nil) end))

core.register_chatcommand("nc_turn", numeric_command(
    "Turn by this many degrees once; positive values turn right",
    function(name, value) return construct_state.turn(name, value) end))

core.register_chatcommand("nc_spin", numeric_command(
    "Set continuous yaw rate in degrees per second. Use /nc_turn for one turn.",
    function(name, value) return construct_state.set_motion(name, nil, math.rad(value), nil) end))

core.register_chatcommand("nc_rise", numeric_command(
    "Set vertical speed in blocks per second",
    function(name, value) return construct_state.set_motion(name, nil, nil, value) end))

core.register_chatcommand("nc_stop", {
    description = "Stop the active NavyCraft construct",
    privs = {interact = true},
    func = function(name) return construct_state.stop(name) end,
})

core.register_chatcommand("nc_status", {
    description = "Show active NavyCraft construct status",
    privs = {interact = true},
    func = function(name)
        local status = construct_state.status(name)
        return status ~= nil, status or "no active construct"
    end,
})

core.register_chatcommand("nc_dock", {
    description = "Dock the active construct on the world grid",
    privs = {interact = true},
    func = function(name) return construct_state.dock(name) end,
})

core.register_chatcommand("nc_resetship", {
    description = "Delete your active native construct immediately",
    privs = {interact = true},
    func = function(name)
        local construct = construct_state.get_for_owner(name)
        if not construct then return false, "No active construct" end
        construct_state.clear_passenger(name)
        local ok, message = construct_state.remove(name, false, "manual_test_reset")
        return ok, ok and "Active native construct deleted" or message
    end,
})

core.register_chatcommand("nc_purge_constructs", {
    description = "Delete all active native NavyCraft constructs and clear saved active-ship state",
    privs = {interact = true},
    func = function(name)
        local ok, message = construct_state.purge_all("manual_test_purge")
        return ok, message
    end,
})

core.register_chatcommand("nc_kit", {
    description = "Give yourself development hull blocks and a helm",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "player unavailable" end
        local inventory = player:get_inventory()
        inventory:add_item("main", "nc_core:frame 99")
        inventory:add_item("main", "nc_navycraft:hull_wood 99")
        inventory:add_item("main", "nc_navycraft:hull_steel 99")
        inventory:add_item("main", "nc_navycraft:hull_armoured 50")
        inventory:add_item("main", "nc_navycraft:hull_glass 50")
        inventory:add_item("main", "nc_navycraft:lift_cell 50")
        inventory:add_item("main", "nc_navycraft:helm")
        inventory:add_item("main", "nc_navycraft:nav")
        inventory:add_item("main", "nc_navycraft:engine_boiler_1 2")
        inventory:add_item("main", "nc_navycraft:engine_motor_1")
        inventory:add_item("main", "nc_navycraft:subdrive")
        inventory:add_item("main", "nc_navycraft:ballast 4")
        inventory:add_item("main", "nc_navycraft:buoyancy")
        inventory:add_item("main", "nc_navycraft:radar")
        inventory:add_item("main", "nc_navycraft:sonar")
        inventory:add_item("main", "nc_navycraft:periscope")
        inventory:add_item("main", "nc_navycraft:launcher")
        inventory:add_item("main", "nc_navycraft:telegraph")
        inventory:add_item("main", "nc_navycraft:rudder")
        inventory:add_item("main", "nc_navycraft:planes")
        inventory:add_item("main", "nc_navycraft:firecontrol")
        inventory:add_item("main", "nc_navycraft:tdc")
        inventory:add_item("main", "nc_navycraft:tube")
        inventory:add_item("main", "nc_navycraft:target")
        inventory:add_item("main", "nc_navycraft:active_sonar")
        inventory:add_item("main", "nc_navycraft:passive_sonar")
        inventory:add_item("main", "nc_navycraft:hf_sonar")
        inventory:add_item("main", "nc_navycraft:aa_gun")
        inventory:add_item("main", "nc_navycraft:radio")
        inventory:add_item("main", "nc_navycraft:pump 4")
        inventory:add_item("main", "nc_navycraft:hyperdrive")
        inventory:add_item("main", "nc_navycraft:weapon_single_cannon 2")
        inventory:add_item("main", "nc_navycraft:weapon_torpedo_mk1 2")
        inventory:add_item("main", "nc_navycraft:weapon_depth_charge_dropper")
        inventory:add_item("main", "nc_navycraft:cannon_shell 50")
        inventory:add_item("main", "nc_navycraft:torpedo_mk1 20")
        inventory:add_item("main", "nc_navycraft:torpedo_mk2 20")
        inventory:add_item("main", "nc_navycraft:torpedo_mk3 20")
        inventory:add_item("main", "nc_navycraft:depth_charge 20")
        inventory:add_item("main", "nc_navycraft:bomb 20")
        inventory:add_item("main", "nc_navycraft:aa_round 99")
        inventory:add_item("main", "nc_navycraft:select_control")
        inventory:add_item("main", "nc_navycraft:claim_control")
        inventory:add_item("main", "nc_navycraft:recall_control")
        inventory:add_item("main", "nc_navycraft:spawn_control")
        inventory:add_item("main", "nc_navycraft:universal_remote")
        inventory:add_item("main", "nc_shipyard:plot_wand")
        inventory:add_item("main", "nc_shipyard:plot_controller")
        inventory:add_item("main", "nc_shipyard:plot_boundary 40")
        return true, "Complete NavyCraft/Shipyard development kit added"
    end,
})

core.log("action", "[NavyCraft] nc_core Milestone 3 foundation loaded")

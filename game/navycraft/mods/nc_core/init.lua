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
navycraft.construct_visuals = dofile(modpath .. "/construct_visuals.lua")
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
    if construct_state.find_at_world_node then
        local active, index = construct_state.find_at_world_node(origin)
        if active then
            if player:get_player_control().sneak and construct_state.request_convert_to_blocks then
                if active.owner ~= name and not core.check_player_privs(name, {server = true}) then
                    core.chat_send_player(name, "Only the owner may convert this construct")
                    return false
                end
                local ok, message = construct_state.request_convert_to_blocks(active, name)
                core.chat_send_player(name, message or (ok and "Construct conversion armed" or "Construct conversion failed"))
                return ok
            end
            local ok, message
            if navycraft.controls and navycraft.controls.take_helm then
                ok, message = navycraft.controls.take_helm(active, player, index)
            elseif navycraft.systems and navycraft.systems.take_helm then
                ok, message = navycraft.systems.take_helm(active, name)
            end
            if ok then
                return true, message or "Helm engaged"
            end
            core.chat_send_player(name, message or "Launch blocked: target block is already part of a moving vessel")
            return false, message
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
    if navycraft.controls and navycraft.controls.take_helm then
        local active = construct_state.get_by_id(construct_id)
        if active then navycraft.controls.take_helm(active, player) end
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

local function test_ship_forward(player)
    local look = player:get_look_dir()
    if math.abs(look.x) > math.abs(look.z) then
        return {x = look.x >= 0 and 1 or -1, y = 0, z = 0}
    end
    return {x = 0, y = 0, z = look.z >= 0 and 1 or -1}
end

local function test_ship_right(forward)
    return {x = forward.z, y = 0, z = -forward.x}
end

local function test_ship_world_pos(origin, right, forward, local_pos)
    return {
        x = origin.x + right.x * local_pos.x + forward.x * local_pos.z,
        y = origin.y + local_pos.y,
        z = origin.z + right.z * local_pos.x + forward.z * local_pos.z,
    }
end

local function test_ship_layout()
    local nodes = {}
    local by_key = {}

    local function set_node(x, y, z, name)
        local key = x .. ":" .. y .. ":" .. z
        local spec = by_key[key]
        if not spec then
            spec = {local_pos = {x = x, y = y, z = z}, name = name}
            by_key[key] = spec
            nodes[#nodes + 1] = spec
        else
            spec.name = name
        end
    end

    for z = -7, 7 do
        for x = -3, 3 do
            local edge = math.abs(x) == 3 or math.abs(z) == 7
            set_node(x, 0, z, edge and "nc_navycraft:hull_wood" or "nc_navycraft:lift_cell")
        end
    end

    for z = -6, 6 do
        set_node(-4, 0, z, "nc_navycraft:hull_wood")
        set_node(4, 0, z, "nc_navycraft:hull_wood")
    end

    for z = -5, 5 do
        set_node(-4, -1, z, "nc_navycraft:lift_cell")
        set_node(4, -1, z, "nc_navycraft:lift_cell")
        set_node(-2, -1, z, "nc_navycraft:hull_wood")
        set_node(2, -1, z, "nc_navycraft:hull_wood")
    end

    for z = -5, 5 do
        set_node(-4, 1, z, "nc_navycraft:hull_wood")
        set_node(4, 1, z, "nc_navycraft:hull_wood")
    end

    set_node(0, 1, 5, "nc_navycraft:helm")
    set_node(-1, 1, 4, "nc_navycraft:telegraph")
    set_node(1, 1, 4, "nc_navycraft:rudder")
    set_node(-1, 1, 1, "nc_navycraft:engine_boiler_1")
    set_node(1, 1, 1, "nc_navycraft:engine_boiler_1")
    set_node(0, 1, -1, "nc_navycraft:nav")
    set_node(0, 1, -2, "nc_navycraft:buoyancy")
    set_node(-2, 1, -3, "nc_navycraft:pump")
    set_node(2, 1, -3, "nc_navycraft:pump")
    set_node(-2, 1, 2, "nc_navycraft:ballast")
    set_node(2, 1, 2, "nc_navycraft:ballast")
    set_node(0, 1, -5, "nc_navycraft:weapon_single_cannon")
    set_node(0, 1, -6, "nc_navycraft:firecontrol")

    return nodes
end

local function test_ship_can_replace(pos)
    local node = core.get_node_or_nil(pos)
    if not node then return false, "unloaded map" end
    if node.name == "air" then return true end
    local def = core.registered_nodes[node.name]
    if def and def.buildable_to then return true end
    if def and def.liquidtype and def.liquidtype ~= "none" then return true end
    return false, node.name
end

local function test_ship_facedir(name, forward)
    local def = core.registered_nodes[name]
    if def and def.paramtype2 == "facedir" and core.dir_to_facedir then
        return core.dir_to_facedir(forward, false)
    end
    return 0
end

local function configure_test_ship_node(pos, name)
    local meta = core.get_meta(pos)
    if name == "nc_navycraft:helm" then
        meta:set_string("craft_type", "ship")
        meta:set_string("infotext", "NavyCraft Helm [ship]")
    elseif name == "nc_navycraft:engine_boiler_1" then
        meta:set_string("enabled", "true")
        meta:set_string("infotext", "Boiler 1 [ON]")
    elseif name == "nc_navycraft:telegraph" then
        meta:set_string("infotext", "Engine Telegraph")
    elseif name == "nc_navycraft:rudder" then
        meta:set_string("infotext", "Rudder Control")
    elseif name == "nc_navycraft:nav" then
        meta:set_string("infotext", "Navigation Control")
    elseif name == "nc_navycraft:buoyancy" then
        meta:set_string("infotext", "Buoyancy Indicator")
    elseif name == "nc_navycraft:pump" then
        meta:set_string("infotext", "Pump")
    elseif name == "nc_navycraft:ballast" then
        meta:set_string("infotext", "Ballast Tanks")
    elseif name == "nc_navycraft:weapon_single_cannon" then
        meta:set_int("weapon_type", 0)
        meta:set_string("infotext", "Single Cannon")
    elseif name == "nc_navycraft:firecontrol" then
        meta:set_string("infotext", "Fire Control")
    end
end

core.register_chatcommand("nc_testship", {
    params = "[force]",
    description = "Spawn a connected NavyCraft test ship in front of you. Use force to overwrite solid blockers.",
    privs = {interact = true},
    func = function(name, param)
        local player = core.get_player_by_name(name)
        if not player then return false, "player unavailable" end

        local force = tostring(param or ""):lower():find("force", 1, true) ~= nil
        local forward = test_ship_forward(player)
        local right = test_ship_right(forward)
        local origin = vector.round(vector.add(player:get_pos(), vector.multiply(forward, 8)))
        origin.y = math.floor(player:get_pos().y)
        local layout = test_ship_layout()

        for _, spec in ipairs(layout) do
            if not core.registered_nodes[spec.name] then
                return false, "Test ship node is not registered: " .. spec.name
            end
        end

        local blocked = {}
        for _, spec in ipairs(layout) do
            local pos = test_ship_world_pos(origin, right, forward, spec.local_pos)
            if core.is_protected(pos, name) then
                blocked[#blocked + 1] = core.pos_to_string(pos) .. " protected"
            else
                local ok, reason = test_ship_can_replace(pos)
                if not ok and not force then
                    blocked[#blocked + 1] = core.pos_to_string(pos) .. " " .. reason
                end
            end
            if #blocked >= 6 then break end
        end

        if #blocked > 0 then
            return false, "Test ship spawn blocked. Move to open water/air or use /nc_testship force. First blockers: " .. table.concat(blocked, ", ")
        end

        for _, spec in ipairs(layout) do
            local pos = test_ship_world_pos(origin, right, forward, spec.local_pos)
            core.set_node(pos, {name = spec.name, param2 = test_ship_facedir(spec.name, forward)})
            configure_test_ship_node(pos, spec.name)
        end

        return true, "Spawned " .. tostring(#layout) .. "-block NavyCraft test ship. Click the helm to launch and take control."
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

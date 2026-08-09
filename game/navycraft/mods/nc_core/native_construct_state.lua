-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Permanent Lua gameplay/state adapter for native engine constructs.
-- Lua owns gameplay state and guarded transform fallback when native movement
-- lacks map-block collision for the primary construct body.
local M = {}
local storage = core.get_mod_storage()
local HELM_DRIVER_SEAT_ENTITY = "nc_core:helm_driver_seat"
local constructs = {}
local owner_active = {}
local pending_conversions = {}
local settled_wrecks = {}
local wreck_counter = 0
local rider_support = {}
local jump_released_riders = {}
local helm_seats = {}
local helm_physics = {}
local launch_hooks, remove_hooks, step_hooks, dock_hooks = {}, {}, {}, {}
local boarding_filters, interaction_hooks, damage_hooks = {}, {}, {}
local MAX_FORWARD_SPEED = 8
local MAX_VERTICAL_SPEED = 4
local MAX_YAW_RATE = math.rad(45)
local MAX_QUEUED_TURN = math.rad(720)
local SETTLED_WRECK_TTL = 30 * 60
local WRECK_CLEANUP_INTERVAL = 5
local PASSENGER_VOLUME_MARGIN = 1
local ATTACH_OFFSET_SCALE = 10
local PI, HALF_PI, TWO_PI = math.pi, math.pi / 2, math.pi * 2
local MOTION_EPSILON = 0.0001
local step_accumulator = 0
local wreck_cleanup_accumulator = 0
local send_drive_velocity, motion_changed, mark_motion_sent
local within_construct, clear_construct_player_state
local sync_visuals, save_all
local COLLISION_DIRS = {
    {x = 1, y = 0, z = 0}, {x = -1, y = 0, z = 0},
    {x = 0, y = 1, z = 0}, {x = 0, y = -1, z = 0},
    {x = 0, y = 0, z = 1}, {x = 0, y = 0, z = -1},
}

if type(core.register_entity) == "function" then
    core.register_entity(HELM_DRIVER_SEAT_ENTITY, {
        initial_properties = {
            physical = false,
            collide_with_objects = false,
            pointable = false,
            visual = "cube",
            visual_size = {x = 1, y = 1, z = 1},
            textures = {
                "nc_frame.png^[opacity:0", "nc_frame.png^[opacity:0", "nc_frame.png^[opacity:0",
                "nc_frame.png^[opacity:0", "nc_frame.png^[opacity:0", "nc_frame.png^[opacity:0",
            },
            static_save = false,
        },
    })
end

local function copy(value)
    if type(value) ~= "table" then return value end
    local result = {}
    for key, child in pairs(value) do result[copy(key)] = copy(child) end
    return result
end

local function save_wrecks()
    storage:set_string("settled_wrecks_v1",core.serialize({counter=wreck_counter,wrecks=settled_wrecks}))
end

local function load_wrecks()
    local decoded=core.deserialize(storage:get_string("settled_wrecks_v1"))
    if type(decoded)=="table" then
        wreck_counter=tonumber(decoded.counter) or 0
        settled_wrecks=type(decoded.wrecks)=="table" and decoded.wrecks or {}
    end
end

local function normalise_yaw(yaw)
    yaw = (tonumber(yaw) or 0) % TWO_PI
    if yaw < 0 then yaw = yaw + TWO_PI end
    return yaw
end

local function clamp(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, tonumber(value) or 0))
end

local function configured_conversion_delay()
    local nc = rawget(_G, "navycraft")
    local source = nc and nc.definitions and nc.definitions.source or nil
    local value = source and source.construct_conversion_delay or nil
    local settings = rawget(_G, "core") and core.settings or nil
    if value == nil and settings and type(settings.get) == "function" then
        local keys = {
            "navalclash_construct_conversion_timer",
            "navalclash_conversion_timer",
            "navycraft_construct_conversion_timer",
        }
        for _, key in ipairs(keys) do
            value = tonumber(settings:get(key))
            if value then break end
        end
    end
    return clamp(value or 1, 0, 600)
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

local facedir_vector, cardinal_local_direction

local function yaw_forward(yaw)
    if type(core.yaw_to_dir) == "function" then
        local dir = core.yaw_to_dir(yaw or 0)
        if dir then return {x = dir.x or 0, y = 0, z = dir.z or 0} end
    end
    return rotate({x = 0, y = 0, z = 1}, yaw or 0)
end

local function construct_forward(construct, yaw)
    local systems = construct and construct.systems or nil
    local local_forward = cardinal_local_direction(systems and systems.active_helm_local_facing, {x = 0, y = 0, z = 1})
    return rotate(local_forward, yaw or (construct and construct.yaw) or 0)
end

local function yaw_from_bow_direction(dir)
    if type(core.dir_to_yaw) == "function" and dir then
        return normalise_yaw(core.dir_to_yaw({x = dir.x or 0, y = 0, z = dir.z or 0}))
    end
    return 0
end

cardinal_local_direction = function(dir, fallback)
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

local function local_direction_from_world(dir, yaw)
    if not dir then return nil end
    local local_dir = rotate({x = dir.x or 0, y = 0, z = dir.z or 0}, -(yaw or 0))
    return cardinal_local_direction(local_dir)
end

local function initial_yaw_from_scan(scan_result)
    if scan_result and scan_result.navycraft_initial_yaw ~= nil then
        return normalise_yaw(scan_result.navycraft_initial_yaw)
    end
    for _, node in ipairs(scan_result and scan_result.nodes or {}) do
        local def = core.registered_nodes[node.name] or {}
        if def._navycraft_component == "helm" or def._navycraft_helm then
            return yaw_from_bow_direction(facedir_vector(node.param2 or 0))
        end
    end
    return 0
end

local function local_key(pos)
    return string.format("%d:%d:%d", math.floor(pos.x + 0.5), math.floor(pos.y + 0.5), math.floor(pos.z + 0.5))
end

local function rotate_quarter(local_pos, quarter_turn)
    local x, z = local_pos.x, local_pos.z
    if quarter_turn == 0 then return {x=x, y=local_pos.y, z=z} end
    if quarter_turn == 1 then return {x=-z, y=local_pos.y, z=x} end
    if quarter_turn == 2 then return {x=-x, y=local_pos.y, z=-z} end
    return {x=z, y=local_pos.y, z=-x}
end

local function restored_param2(entry, quarter_turn)
    local param2 = entry.param2 or 0
    local definition = core.registered_nodes[entry.name] or {}
    if quarter_turn == 0 or definition.paramtype2 ~= "facedir" or
            type(core.dir_to_facedir) ~= "function" then
        return param2
    end
    local facing = cardinal_local_direction(entry.local_facing or facedir_vector(param2), {x = 0, y = 0, z = 1})
    local rotated = rotate_quarter(facing, quarter_turn % 4)
    local ok, rotated_param2 = pcall(core.dir_to_facedir, {x = rotated.x or 0, y = 0, z = rotated.z or 0})
    return ok and rotated_param2 or param2
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

local function world_position(construct, local_pos)
    return vector.add(construct.position, rotate(local_pos, construct.yaw))
end

local function world_position_at(position, yaw, local_pos)
    return vector.add(position, rotate(local_pos, yaw))
end

local function world_to_local(construct, world_pos)
    local relative = vector.subtract(world_pos, construct.position)
    local cosine, sine = math.cos(construct.yaw), math.sin(construct.yaw)
    return {
        x = cosine * relative.x + sine * relative.z,
        y = relative.y,
        z = -sine * relative.x + cosine * relative.z,
    }
end

local function construct_dimensions(construct)
    local bounds = construct and construct.bounds
    local minp = bounds and bounds.minp or {x = 0, y = 0, z = 0}
    local maxp = bounds and bounds.maxp or {x = 0, y = 0, z = 0}
    return {
        width = math.max(1, (maxp.x or 0) - (minp.x or 0) + 1),
        height = math.max(1, (maxp.y or 0) - (minp.y or 0) + 1),
        depth = math.max(1, (maxp.z or 0) - (minp.z or 0) + 1),
    }
end

local function passenger_bounds(construct)
    local bounds = construct and construct.bounds
    if not bounds then return nil end
    local minp = bounds.minp or {x = 0, y = 0, z = 0}
    local maxp = bounds.maxp or {x = 0, y = 0, z = 0}
    return {
        minp = {
            x = (minp.x or 0) - PASSENGER_VOLUME_MARGIN,
            y = (minp.y or 0) - PASSENGER_VOLUME_MARGIN,
            z = (minp.z or 0) - PASSENGER_VOLUME_MARGIN,
        },
        maxp = {
            x = (maxp.x or 0) + PASSENGER_VOLUME_MARGIN,
            y = (maxp.y or 0) + PASSENGER_VOLUME_MARGIN + 1,
            z = (maxp.z or 0) + PASSENGER_VOLUME_MARGIN,
        },
    }
end

local function contains_local(bounds, local_pos)
    if not bounds or not local_pos then return false end
    return local_pos.x >= bounds.minp.x and local_pos.x <= bounds.maxp.x
        and local_pos.y >= bounds.minp.y and local_pos.y <= bounds.maxp.y
        and local_pos.z >= bounds.minp.z and local_pos.z <= bounds.maxp.z
end

facedir_vector = function(param2)
    if type(core.facedir_to_dir) == "function" then
        local dir = core.facedir_to_dir((tonumber(param2) or 0) % 32)
        if dir then
            return {
                x = math.abs(dir.x or 0) >= 0.5 and sign(dir.x or 0) or 0,
                y = 0,
                z = math.abs(dir.z or 0) >= 0.5 and sign(dir.z or 0) or 0,
            }
        end
    end
    return {x = 0, y = 0, z = 1}
end

local function compute_pivot(scan_result)
    local function round(value) return math.floor(value + 0.5) end
    return {
        x=round((scan_result.minp.x + scan_result.maxp.x)/2),
        y=round((scan_result.minp.y + scan_result.maxp.y)/2),
        z=round((scan_result.minp.z + scan_result.maxp.z)/2),
    }
end

local function compute_bounds(nodes)
    local bounds={minp={x=math.huge,y=math.huge,z=math.huge},maxp={x=-math.huge,y=-math.huge,z=-math.huge}}
    for _,entry in ipairs(nodes or {}) do
        if not entry.destroyed then
            local p=entry.local_pos
            bounds.minp.x=math.min(bounds.minp.x,p.x); bounds.minp.y=math.min(bounds.minp.y,p.y); bounds.minp.z=math.min(bounds.minp.z,p.z)
            bounds.maxp.x=math.max(bounds.maxp.x,p.x); bounds.maxp.y=math.max(bounds.maxp.y,p.y); bounds.maxp.z=math.max(bounds.maxp.z,p.z)
        end
    end
    if bounds.minp.x==math.huge then bounds.minp={x=0,y=0,z=0};bounds.maxp={x=0,y=0,z=0} end
    return bounds
end

local function refresh_bounds(construct)
    construct.bounds=compute_bounds(construct.nodes)
    construct.dimensions=construct_dimensions(construct)
end

local function node_groups(entry)
    local definition=entry and core.registered_nodes[entry.name] or nil
    return definition and definition.groups or {}
end

local function node_max_hitpoints(entry)
    local groups=node_groups(entry)
    local armour=math.max(0.25,tonumber(groups.navycraft_armour) or tonumber(groups.cracky) or 1)
    local encoded_weight=tonumber(groups.navycraft_weight) or 0
    local weight=encoded_weight>0 and encoded_weight/100 or ((groups.navycraft_component or 0)>0 and 0.75 or 0.25)
    local component_bonus=(groups.navycraft_component or 0)>0 and 18 or 0
    local lift_penalty=(groups.navycraft_lift or 0)>0 and 0.65 or 1
    return math.max(4,math.floor((armour*12 + weight*35 + component_bonus)*lift_penalty + 0.5))
end

local function ensure_node_hitpoints(entry)
    if not entry then return 0 end
    local maximum=node_max_hitpoints(entry)
    entry.max_hp=math.max(1,tonumber(entry.max_hp) or maximum)
    entry.hp=math.max(0,math.min(entry.max_hp,tonumber(entry.hp) or entry.max_hp))
    return entry.hp,entry.max_hp
end

local function ensure_construct_damage_state(construct)
    for _,entry in ipairs(construct and construct.nodes or {}) do
        if not entry.destroyed then ensure_node_hitpoints(entry) end
    end
end

local function active_node_count(construct)
    local count=0
    for _,entry in ipairs(construct.nodes or {}) do if not entry.destroyed then count=count+1 end end
    return count
end

local function is_hull_entry(entry)
    local groups=node_groups(entry)
    return (groups.navycraft_hull or 0)>0 or (groups.navycraft_component or 0)>0
end

local function water_level()
    if type(core.get_mapgen_setting)=="function" then
        local ok,value=pcall(core.get_mapgen_setting,"water_level")
        if ok and tonumber(value) then return tonumber(value) end
    end
    return 1
end

local function local_pos_breached(construct,entry,impact_position,explicit_breach)
    if explicit_breach then return true end
    if not is_hull_entry(entry) then return false end
    local world=world_position(construct,entry.local_pos)
    if world.y<=water_level()+0.5 then return true end
    if impact_position and impact_position.y and impact_position.y<=water_level()+0.5 then return true end
    return false
end

local function count_breaches(construct)
    local breaches=0
    for _,entry in ipairs(construct and construct.nodes or {}) do
        if entry.breached then breaches=breaches+1 end
    end
    return breaches
end

local function update_damage_summary(construct)
    local alive,total_hp,max_hp=0,0,0
    for _,entry in ipairs(construct and construct.nodes or {}) do
        if not entry.destroyed then
            local hp,maximum=ensure_node_hitpoints(entry)
            alive=alive+1
            total_hp=total_hp+hp
            max_hp=max_hp+maximum
        end
    end
    if construct.profile then construct.profile.block_count_alive=alive end
    if construct.systems then
        local start=math.max(1,construct.profile and construct.profile.block_count or #(construct.nodes or {}))
        local block_ratio=alive/start
        local hp_ratio=max_hp>0 and total_hp/max_hp or block_ratio
        construct.systems.hull_integrity=clamp(math.min(block_ratio,hp_ratio),0,1)
        construct.systems.block_damage=clamp(1-hp_ratio,0,1)
        construct.systems.breach_count=count_breaches(construct)
        if alive<=0 or construct.systems.hull_integrity<=0 then
            construct.systems.sinking=true
            construct.systems.sunk=true
        end
    end
    return alive
end

local function mark_destroyed_component(construct,index,entry)
    local systems=construct and construct.systems
    if not systems then return end
    local definition=core.registered_nodes[entry.name] or {}
    local component=definition._navycraft_component
    if component=="helm" or definition._navycraft_helm then
        systems.helm_destroyed=true
        systems.driver=nil
        systems.rudder=0
        systems.turn_progress=0
    elseif component=="pump" then
        systems.pump_damage_count=(systems.pump_damage_count or 0)+1
    end
    local engine_state=systems.engines and systems.engines[tostring(index)]
    if engine_state then
        engine_state.set_on=false
        engine_state.is_on=false
        systems.engines_on=false
    end
end

local function destroy_dynamic_node(construct,index,entry,allow_missing_native)
    if entry.destroyed then return false end
    local ok=true
    if construct.native_id and type(core.set_dynamic_construct_node)=="function" then
        ok=core.set_dynamic_construct_node(construct.native_id,entry.local_pos,nil)
    end
    if ok or allow_missing_native then
        entry.destroyed=true
        entry.hp=0
        mark_destroyed_component(construct,index,entry)
        return true
    end
    return false
end

local function apply_damage_items(construct,items,impact_position,attacker,damage_type,options)
    options=options or {}
    ensure_construct_damage_state(construct)
    local changed,removed,breached,total_damage=0,0,0,0
    for _,item in ipairs(items or {}) do
        local local_pos=item.local_pos or item.node_pos
        local index=local_pos and M.find_node_index(construct,local_pos) or nil
        local entry=index and construct.nodes[index] or nil
        if entry and not entry.destroyed then
            local hp,maximum=ensure_node_hitpoints(entry)
            local amount=tonumber(item.amount or item.effective_power or item.power or 0) or 0
            amount=math.max(0,amount)
            if item.force_destroy then amount=math.max(amount,hp) end
            if amount>0 then
                entry.hp=clamp(hp-amount,0,maximum)
                entry.last_damage_power=amount
                entry.last_damage_type=damage_type or "damage"
                entry.last_attacker=attacker
                total_damage=total_damage+amount
                changed=changed+1
            end
            if item.force_destroy or entry.hp<=0 then
                if local_pos_breached(construct,entry,impact_position,item.breached) then
                    if not entry.breached then breached=breached+1 end
                    entry.breached=true
                end
                if destroy_dynamic_node(construct,index,entry,options.allow_missing_native) then
                    removed=removed+1
                end
            end
        end
    end
    if changed==0 and removed==0 and breached==0 then return 0,{removed=0,damaged=0,breaches=0,total_damage=0,alive=active_node_count(construct)} end
    construct._collision_samples=nil
    refresh_bounds(construct)
    local alive=update_damage_summary(construct)
    if construct.systems then
        local s=construct.systems
        s.last_attacker=attacker
        s.last_damage_type=damage_type or "damage"
        if breached>0 then
            local flood_mult=((s.equipment_modifiers or {}).flooding_mult or 1)
            s.flooding=(s.flooding or 0)+breached*2.5*flood_mult
            s.flooded_volume=math.max(tonumber(s.flooded_volume) or 0,s.flooding or 0)
        end
        local displacement=math.max(1,tonumber(s.displacement) or tonumber(construct.profile and construct.profile.displacement) or 1)
        if (s.breach_count or 0)>0 and (s.flooding or 0)>=displacement*0.75 then s.sinking=true end
        if s.helm_destroyed or (s.hull_integrity or 1)<0.28 then s.sinking=true end
    end
    local details={removed=removed,damaged=changed,breaches=breached,total_damage=total_damage,alive=alive}
    for _,callback in ipairs(damage_hooks) do callback(construct,removed,impact_position or construct.position,attacker,damage_type,details) end
    save_all()
    sync_visuals(construct)
    return removed,details
end

local function construct_visuals()
    local nc = rawget(_G, "navycraft")
    return nc and nc.construct_visuals or nil
end

local function spawn_visuals(construct)
    local visuals = construct_visuals()
    if visuals and type(visuals.spawn) == "function" then
        return visuals.spawn(construct)
    end
    return true
end

sync_visuals=function(construct)
    local visuals = construct_visuals()
    if visuals and type(visuals.sync) == "function" then
        return visuals.sync(construct)
    end
    return true
end

local function remove_visuals(construct)
    local visuals = construct_visuals()
    if visuals and type(visuals.remove) == "function" then
        pcall(visuals.remove, construct)
    end
end

local function stop_native_velocity(construct)
    if construct and construct.native_id and type(core.set_dynamic_construct_velocity) == "function" then
        pcall(core.set_dynamic_construct_velocity, construct.native_id, {x = 0, y = 0, z = 0}, 0)
    end
end

local function use_lua_motion_driver(construct)
    construct._lua_motion_fallback = true
    construct._lua_stall_ticks = 0
    stop_native_velocity(construct)
end

local function use_native_motion_driver(construct)
    construct._lua_motion_fallback = type(core.set_dynamic_construct_velocity) ~= "function"
    construct._lua_stall_ticks = 0
    stop_native_velocity(construct)
end

local function is_sunk(construct)
    local systems = construct and construct.systems
    if not systems then return false end
    if systems.sunk or systems.sinking then return true end
    return (tonumber(systems.hull_integrity) or 1) <= 0
end

local function freeze_construct(construct)
    local systems = construct and construct.systems
    if not systems or is_sunk(construct) then return false end
    systems.frozen = true
    systems.abandoned = false
    systems.captain_abandoned = false
    systems.taking_over = nil
    systems.takeover_started = 0
    systems.release_at = 0
    systems.remote_control = false
    systems.autotravel = false
    systems.throttle = 0
    systems.set_speed = 0
    systems.gear = 0
    systems.rudder = 0
    systems.turn_progress = 0
    systems.turn_elapsed = 0
    systems.vertical_planes = 0
    systems.engines_on = false
    systems.driver = nil
    construct.forward_speed = 0
    construct.vertical_speed = 0
    construct.yaw_rate = 0
    construct.turn_remaining = 0
    for _,state in pairs(systems.engines or {}) do
        state.set_on = false
    end
    send_drive_velocity(construct, true)
    return true
end

local function clean_construct(construct)
    return {
        id=construct.id,native_id=construct.native_id,owner=construct.owner,
        position=vector.new(construct.position),yaw=construct.yaw,
        forward_speed=construct.forward_speed or 0,vertical_speed=construct.vertical_speed or 0,
        yaw_rate=construct.yaw_rate or 0,turn_remaining=construct.turn_remaining or 0,
        nodes=copy(construct.nodes or {}),profile=copy(construct.profile or {}),
        systems=copy(construct.systems or {}),
        dimensions=copy(construct.dimensions or construct_dimensions(construct)),
    }
end

local function world_node_can_be_replaced(pos)
    local node = core.get_node_or_nil(pos)
    if not node then return false end
    if node.name == "air" then return true end
    local definition = core.registered_nodes[node.name]
    if not definition then return false end
    if definition.buildable_to then return true end
    if definition.liquidtype and definition.liquidtype ~= "none" then return true end
    local groups = definition.groups or {}
    return (groups.water or 0) > 0 or (groups.liquid or 0) > 0
end

local function collision_samples(construct)
    if construct._collision_samples then return construct._collision_samples end
    local occupied, samples = {}, {}
    for _, entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            occupied[local_key(entry.local_pos)] = true
        end
    end
    for _, entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            local surface = false
            for _, dir in ipairs(COLLISION_DIRS) do
                if not occupied[local_key(vector.add(entry.local_pos, dir))] then
                    surface = true
                    break
                end
            end
            if surface then samples[#samples + 1] = vector.new(entry.local_pos) end
        end
    end
    if #samples == 0 then
        for _, entry in ipairs(construct.nodes or {}) do
            if not entry.destroyed and entry.local_pos then
                samples[#samples + 1] = vector.new(entry.local_pos)
            end
        end
    end
    construct._collision_samples = samples
    return samples
end

local function bottom_collision_samples(construct)
    local occupied, samples = {}, {}
    for _, entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            occupied[local_key(entry.local_pos)] = true
        end
    end
    for _, entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos and
                not occupied[local_key(vector.add(entry.local_pos, {x = 0, y = -1, z = 0}))] then
            samples[#samples + 1] = vector.new(entry.local_pos)
        end
    end
    return samples
end

local function construct_collides_world(construct, position, yaw)
    for _, local_pos in ipairs(collision_samples(construct)) do
        local target = vector.round(world_position_at(position, yaw, local_pos))
        if not world_node_can_be_replaced(target) then
            return true, target
        end
    end
    return false
end

local function construct_bottom_collides_world(construct, position, yaw)
    local probe_position = vector.add(position, {x = 0, y = -0.75, z = 0})
    for _, local_pos in ipairs(bottom_collision_samples(construct)) do
        local target = vector.round(world_position_at(probe_position, yaw, local_pos))
        if not world_node_can_be_replaced(target) then
            return true, target
        end
    end
    return false
end

local function depenetrate_construct(construct, max_raise)
    if not construct or not construct.position then return false end
    local start = vector.new(construct.position)
    local yaw = normalise_yaw(construct.yaw or 0)
    if not construct_collides_world(construct, start, yaw) then return false end
    for lift = 1, max_raise or 12 do
        local candidate = vector.add(start, {x = 0, y = lift, z = 0})
        if not construct_collides_world(construct, candidate, yaw) then
            construct.position = vector.new(candidate)
            construct.yaw = yaw
            stop_native_velocity(construct)
            if type(core.set_dynamic_construct_transform) == "function" then
                pcall(core.set_dynamic_construct_transform, construct.native_id, {
                    position = construct.position,
                    yaw = construct.yaw,
                })
            end
            return true
        end
    end
    return false
end

local function place_settled_wreck(construct)
    local placements, seen = {}, {}
    local now = os.time()
    local expires_at = now + SETTLED_WRECK_TTL
    for _,entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            local target=vector.round(world_position(construct,entry.local_pos))
            local key=target.x..":"..target.y..":"..target.z
            if not seen[key] and world_node_can_be_replaced(target) then
                seen[key]=true
                placements[#placements+1]={
                    pos=target,name=entry.name,param1=entry.param1 or 0,
                    param2=restored_param2(entry,math.floor((construct.yaw or 0)/HALF_PI+0.5)%4),
                    metadata=entry.metadata or "",
                }
            end
        end
    end
    if #placements==0 then return nil,"no clear seabed space for wreck" end
    wreck_counter=wreck_counter+1
    local wreck_id="wreck-"..tostring(wreck_counter)
    for _,placement in ipairs(placements) do
        core.set_node(placement.pos,{
            name=placement.name,param1=placement.param1 or 0,param2=placement.param2 or 0,
        })
        local meta_ref = core.get_meta(placement.pos)
        if placement.metadata and placement.metadata~="" then
            local metadata=core.deserialize(placement.metadata)
            if type(metadata)=="table" then meta_ref:from_table(metadata) end
        end
        meta_ref:set_string("navycraft_wreck","1")
        meta_ref:set_string("navycraft_wreck_id",wreck_id)
        meta_ref:set_int("navycraft_wreck_expires_at",expires_at)
    end
    settled_wrecks[wreck_id]={
        id=wreck_id,source_id=construct.id,owner=construct.owner,
        created=now,expires_at=expires_at,
        positions=placements,
    }
    save_wrecks()
    return wreck_id
end

local function cleanup_settled_wrecks(force)
    local now=os.time()
    local changed=false
    for wreck_id,wreck in pairs(settled_wrecks) do
        if force or now>=(tonumber(wreck.expires_at) or now) then
            local unloaded=false
            for _,placement in ipairs(wreck.positions or {}) do
                local current=core.get_node_or_nil(placement.pos)
                if not current then
                    unloaded=true
                elseif current.name==placement.name and
                        core.get_meta(placement.pos):get_string("navycraft_wreck_id")==wreck_id then
                    core.remove_node(placement.pos)
                end
            end
            if not unloaded then
                settled_wrecks[wreck_id]=nil
                changed=true
            end
        end
    end
    if changed then save_wrecks() end
end

save_all=function()
    local serializable={}
    for id,construct in pairs(constructs) do serializable[id]=clean_construct(construct) end
    storage:set_string("active_native_constructs_v1",core.serialize(serializable))
end

local function construct_should_settle(construct)
    if not construct or construct._settling_wreck or not construct.position then return false end
    local systems=construct.systems
    if not systems then return false end
    local integrity=tonumber(systems.hull_integrity) or 1
    local sinking=systems.sinking or systems.sunk or integrity<=0
    if not sinking then return false end
    return construct_bottom_collides_world(construct,construct.position,normalise_yaw(construct.yaw or 0))
end

local function settle_sunk_construct(construct)
    if not construct or not constructs[construct.id] then return false end
    construct._settling_wreck=true
    local wreck_id,error_message=place_settled_wreck(construct)
    if not wreck_id then
        construct._settling_wreck=nil
        return false,error_message
    end
    local snapshot=clean_construct(construct)
    for _,callback in ipairs(remove_hooks) do
        local ok,err=pcall(callback,construct,"sunk",snapshot)
        if not ok then core.log("error","[NavyCraft] sunk remove hook failed: "..tostring(err)) end
    end
    clear_construct_player_state(construct)
    remove_visuals(construct)
    core.remove_dynamic_construct(construct.native_id)
    constructs[construct.id]=nil
    if owner_active[construct.owner]==construct.id then owner_active[construct.owner]=nil end
    save_all()
    if type(core.sync_dynamic_construct_persistence)=="function" then
        local called,synced,sync_error=pcall(core.sync_dynamic_construct_persistence)
        if not called then
            core.log("error","[NavyCraft] wreck persistence sync failed: "..tostring(synced))
        elseif synced==false and sync_error then
            core.log("error","[NavyCraft] wreck persistence sync failed: "..tostring(sync_error))
        end
    end
    core.log("action",string.format("[NavyCraft] settled %s as %s for 30 minutes",
        tostring(snapshot.id),tostring(wreck_id)))
    return true,wreck_id
end

local function runtime_constructs()
    if type(core.list_dynamic_constructs) ~= "function" then return {} end
    local listed = core.list_dynamic_constructs()
    if type(listed) ~= "table" then return {} end
    return listed
end

local function runtime_id(entry)
    return tonumber(type(entry) == "table" and entry.id or entry)
end

local function tracked_native_ids()
    local tracked = {}
    for _,construct in pairs(constructs) do
        if construct.native_id then tracked[tonumber(construct.native_id)] = true end
    end
    return tracked
end

local function untracked_runtime_ids(exclude_id)
    local tracked = tracked_native_ids()
    exclude_id = tonumber(exclude_id)
    local ids = {}
    for _,entry in ipairs(runtime_constructs()) do
        local native_id = runtime_id(entry)
        if native_id and native_id ~= exclude_id and not tracked[native_id] then ids[#ids + 1] = native_id end
    end
    table.sort(ids)
    return ids
end

local function clear_source_nodes(scan_result)
    local failures = {}
    for _,entry in ipairs(scan_result.nodes or {}) do
        local current = core.get_node_or_nil(entry.pos)
        if not current then
            failures[#failures + 1] = core.pos_to_string(entry.pos) .. " is not loaded"
        elseif current.name ~= entry.name then
            failures[#failures + 1] = core.pos_to_string(entry.pos) .. " changed to " .. tostring(current.name)
        end
    end
    if #failures > 0 then
        return false, "source craft changed before launch: " .. table.concat(failures, "; ")
    end

    for _,entry in ipairs(scan_result.nodes or {}) do
        core.remove_node(entry.pos)
    end
    for _,entry in ipairs(scan_result.nodes or {}) do
        local current = core.get_node_or_nil(entry.pos)
        if not current then
            failures[#failures + 1] = core.pos_to_string(entry.pos) .. " is not loaded after clear"
        elseif current.name ~= "air" and not (core.registered_nodes[current.name] and core.registered_nodes[current.name].buildable_to) then
            failures[#failures + 1] = core.pos_to_string(entry.pos) .. " still contains " .. tostring(current.name)
        end
    end
    if #failures > 0 then
        return false, "source blocks were not cleared: " .. table.concat(failures, "; ")
    end
    return true
end

local function refresh_native(construct)
    local state,error_message=core.get_dynamic_construct(construct.native_id,false)
    if not state then return false,error_message or "native construct not found" end
    if not construct._lua_motion_fallback then
        construct.position=vector.new(state.position)
        construct.yaw=normalise_yaw(state.yaw)
    end
    construct.native_velocity=vector.new(state.velocity or {x=0,y=0,z=0})
    construct.native_yaw_velocity=state.yaw_velocity or 0
    refresh_bounds(construct)
    sync_visuals(construct)
    return true
end

local function apply_drive_velocity(construct)
    local forward = construct_forward(construct, construct.yaw)
    local speed = construct.forward_speed or 0
    local velocity={
        x=forward.x*speed,
        y=construct.vertical_speed or 0,
        z=forward.z*speed,
    }
    return core.set_dynamic_construct_velocity(
        construct.native_id,velocity,construct.yaw_rate or 0)
end

local function prediction_velocity(construct)
    local forward = construct_forward(construct, construct.yaw)
    local speed = construct.forward_speed or 0
    return {
        x = forward.x * speed,
        y = construct.vertical_speed or 0,
        z = forward.z * speed,
    }, construct.yaw_rate or 0
end

local function prediction_motion_changed(construct, velocity, yaw_velocity)
    local last = construct._last_sent_prediction_motion
    if not last then return true end
    return math.abs((velocity.x or 0) - (last.velocity.x or 0)) > MOTION_EPSILON
        or math.abs((velocity.y or 0) - (last.velocity.y or 0)) > MOTION_EPSILON
        or math.abs((velocity.z or 0) - (last.velocity.z or 0)) > MOTION_EPSILON
        or math.abs((yaw_velocity or 0) - (last.yaw_velocity or 0)) > MOTION_EPSILON
end

local function sync_lua_prediction_velocity(construct, force)
    if not (construct and construct.native_id and type(core.set_dynamic_construct_velocity) == "function") then
        return true
    end
    local velocity, yaw_velocity = prediction_velocity(construct)
    if not force and not prediction_motion_changed(construct, velocity, yaw_velocity) then
        return true
    end
    local ok, error_message = core.set_dynamic_construct_velocity(construct.native_id, velocity, yaw_velocity)
    if ok then
        construct.native_velocity = vector.new(velocity)
        construct.native_yaw_velocity = yaw_velocity
        construct._last_sent_prediction_motion = {
            velocity = vector.new(velocity),
            yaw_velocity = yaw_velocity,
        }
    end
    return ok, error_message
end

local function yaw_difference(a,b)
    return math.abs(((normalise_yaw(a)-normalise_yaw(b)+PI)%TWO_PI)-PI)
end

motion_changed = function(construct)
    local last=construct._last_sent_motion
    if not last then return true end
    local forward=construct.forward_speed or 0
    return math.abs(forward-(last.forward_speed or 0))>MOTION_EPSILON
        or math.abs((construct.vertical_speed or 0)-(last.vertical_speed or 0))>MOTION_EPSILON
        or math.abs((construct.yaw_rate or 0)-(last.yaw_rate or 0))>MOTION_EPSILON
        or (math.abs(forward)>MOTION_EPSILON and yaw_difference(construct.yaw,last.yaw or 0)>MOTION_EPSILON)
end

mark_motion_sent = function(construct)
    construct._last_sent_motion={
        forward_speed=construct.forward_speed or 0,
        vertical_speed=construct.vertical_speed or 0,
        yaw_rate=construct.yaw_rate or 0,
        yaw=construct.yaw or 0,
    }
end

send_drive_velocity = function(construct,force)
    if construct and construct._lua_motion_fallback then
        if force then mark_motion_sent(construct) end
        return true
    end
    if not force and not motion_changed(construct) then return true end
    local ok,error_message=apply_drive_velocity(construct)
    if ok then mark_motion_sent(construct) end
    return ok,error_message
end

local function motion_active(construct)
    return math.abs(construct.forward_speed or 0)>MOTION_EPSILON
        or math.abs(construct.vertical_speed or 0)>MOTION_EPSILON
        or math.abs(construct.yaw_rate or 0)>MOTION_EPSILON
end

local function integrate_lua_motion(construct,dt)
    if not motion_active(construct) then return true end
    local position = vector.new(construct.position or {x=0,y=0,z=0})
    local yaw = normalise_yaw(construct.yaw or 0)
    local forward = construct.forward_speed or 0
    local vertical = construct.vertical_speed or 0
    local yaw_rate = construct.yaw_rate or 0
    local direction = construct_forward(construct, yaw)
    local next_position = vector.add(position,{
        x=direction.x*forward*dt,
        y=vertical*dt,
        z=direction.z*forward*dt,
    })
    local next_yaw = normalise_yaw(yaw + yaw_rate*dt)
    local horizontal_position={x=next_position.x,y=position.y,z=next_position.z}
    local horizontal_blocked, horizontal_block_pos = construct_collides_world(construct,horizontal_position,next_yaw)
    if horizontal_blocked then
        construct._last_motion_block = "horizontal " .. core.pos_to_string(horizontal_block_pos or horizontal_position)
        local now = os.clock()
        if not construct._last_motion_block_log or now - construct._last_motion_block_log > 1 then
            construct._last_motion_block_log = now
            core.log("action", "[NavyCraft] motion blocked for " .. tostring(construct.id) .. ": " .. construct._last_motion_block)
        end
        horizontal_position={x=position.x,y=position.y,z=position.z}
        construct.forward_speed=0
    else
        construct._last_motion_block = nil
    end
    next_position={x=horizontal_position.x,y=next_position.y,z=horizontal_position.z}
    if construct_collides_world(construct,next_position,next_yaw) then
        next_position=horizontal_position
        construct.vertical_speed=0
    end
    if construct_collides_world(construct,next_position,next_yaw) then
        next_yaw=yaw
        construct.yaw_rate=0
    end
    construct.position=vector.new(next_position)
    construct.yaw=next_yaw
    construct._lua_motion_fallback=true
    if type(core.set_dynamic_construct_transform) == "function" then
        pcall(core.set_dynamic_construct_transform, construct.native_id, {
            position=next_position,
            yaw=next_yaw,
        })
    end
    sync_lua_prediction_velocity(construct,false)
    return true
end

local function restore_world_nodes(construct,quarter_turn)
    local placements={}
    local reference
    for _,entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed then
            local definition=core.registered_nodes[entry.name] or {}
            if definition._navycraft_component=="helm" or definition._navycraft_helm then
                reference=entry
                break
            end
            reference=reference or entry
        end
    end
    if not reference then return false,"construct has no remaining blocks" end
    local reference_offset=rotate_quarter(reference.local_pos,quarter_turn)
    local reference_target=vector.round(vector.add(construct.position,reference_offset))
    local restore_origin=vector.subtract(reference_target,reference_offset)
    for index,entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed then
            local target=vector.round(vector.add(restore_origin,rotate_quarter(entry.local_pos,quarter_turn)))
            local key=target.x..":"..target.y..":"..target.z
            if placements[key] then return false,"two vessel blocks would occupy "..core.pos_to_string(target) end
            local existing=core.get_node_or_nil(target)
            if not existing then return false,"target mapblock is not loaded" end
            local definition=core.registered_nodes[existing.name]
            if existing.name~="air" and not(definition and definition.buildable_to) then
                return false,"docking space blocked at "..core.pos_to_string(target)
            end
            placements[key]={index=index,pos=target}
        end
    end
    for _,placement in pairs(placements) do
        local entry=construct.nodes[placement.index]
        core.set_node(placement.pos,{name=entry.name,param1=entry.param1 or 0,param2=restored_param2(entry,quarter_turn)})
        if entry.metadata and entry.metadata~="" then
            local metadata=core.deserialize(entry.metadata)
            if type(metadata)=="table" then core.get_meta(placement.pos):from_table(metadata) end
        end
    end
    return true
end

local function create_native(owner,nodes,position,yaw)
    local world_nodes={}
    for _,entry in ipairs(nodes or {}) do
        if not entry.destroyed then
            world_nodes[#world_nodes+1]={
                pos=vector.add(position,entry.local_pos),name=entry.name,
                param1=entry.param1 or 0,param2=entry.param2 or 0,
                metadata=entry.metadata or "",
            }
        end
    end
    return core.create_dynamic_construct({origin=position,owner=owner,yaw=yaw or 0,nodes=world_nodes})
end

function M.launch(player,scan_result,native_id)
    if not native_id then return nil,"native construct id required" end
    local owner=player:get_player_name()
    local previous_id = owner_active[owner]
    local previous = previous_id and constructs[previous_id] or nil
    local ghost_ids=untracked_runtime_ids(native_id)
    if #ghost_ids>0 then
        core.remove_dynamic_construct(native_id)
        return nil,"native construct runtime has untracked ships; run /nc_purge_constructs before launching again"
    end
    local pivot=compute_pivot(scan_result)
    local initial_yaw=initial_yaw_from_scan(scan_result)
    local construct={
        id="native-"..tostring(native_id),native_id=native_id,owner=owner,
        position=vector.new(pivot),yaw=initial_yaw,forward_speed=0,vertical_speed=0,
        yaw_rate=0,turn_remaining=0,nodes={},profile=scan_result.navycraft_profile,
        systems=scan_result.navycraft_systems or {},
    }
    construct._lua_last_position=vector.new(construct.position)
    use_lua_motion_driver(construct)
    for _,node in ipairs(scan_result.nodes) do
        local world_offset=vector.subtract(node.pos,pivot)
        local local_pos=rotate(world_offset,-initial_yaw)
        local facing=facedir_vector(node.param2 or 0)
        construct.nodes[#construct.nodes+1]={
            local_pos=local_pos,name=node.name,param1=node.param1,
            param2=local_param2_from_world(node,initial_yaw),
            metadata=node.metadata,local_facing=local_direction_from_world(facing,initial_yaw),
        }
    end
    refresh_bounds(construct)
    for _,callback in ipairs(launch_hooks) do
        local ok,error_message=callback(construct,scan_result,player)
        if ok==false then core.remove_dynamic_construct(native_id);return nil,error_message or "launch hook rejected construct" end
    end
    ensure_construct_damage_state(construct)
    update_damage_summary(construct)
    local spawned,spawn_error=spawn_visuals(construct)
    if spawned==false then
        core.remove_dynamic_construct(native_id)
        return nil,spawn_error or "failed to create visual shell"
    end
    local cleared,clear_error=clear_source_nodes(scan_result)
    if not cleared then remove_visuals(construct);core.remove_dynamic_construct(native_id);return nil,clear_error end
    depenetrate_construct(construct,12)
    construct._lua_last_position=vector.new(construct.position)
    if previous and previous.id ~= construct.id and not is_sunk(previous) then
        freeze_construct(previous)
    end
    constructs[construct.id]=construct;owner_active[owner]=construct.id;save_all()
    sync_visuals(construct)
    return construct.id
end

function M.get_for_owner(owner) local id=owner_active[owner];return id and constructs[id] or nil end
function M.get_all() return constructs end
function M.get_by_id(id) return constructs[id] end
function M.untracked_runtime_count() return #untracked_runtime_ids() end

local function same_local_pos(a,b) return a and b and a.x==b.x and a.y==b.y and a.z==b.z end
function M.find_node_index(construct,local_pos)
    if type(construct)=="string" then construct=constructs[construct] end
    if not construct then return nil end
    for index,entry in ipairs(construct.nodes or {}) do if not entry.destroyed and same_local_pos(entry.local_pos,local_pos) then return index end end
end

function M.find_at_world_node(world_pos)
    local rounded=vector.round(world_pos)
    for _,construct in pairs(constructs) do
        refresh_native(construct)
        if within_construct(construct, rounded) then
            local local_pos=vector.round(world_to_local(construct,rounded))
            local index=M.find_node_index(construct,local_pos)
            if index then
                local exact=world_position(construct,construct.nodes[index].local_pos)
                if vector.distance(exact,rounded)<1.25 then
                    return construct,index
                end
            else
                local best_index,best_distance
                for i,entry in ipairs(construct.nodes or {}) do
                    if not entry.destroyed and entry.local_pos then
                        local exact=world_position(construct,entry.local_pos)
                        local distance=vector.distance(exact,rounded)
                        if not best_distance or distance<best_distance then
                            best_index,best_distance=i,distance
                        end
                    end
                end
                if best_index and best_distance and best_distance<1.25 then
                    return construct,best_index
                end
            end
        end
    end
end

function M.sync_native_dig(native_id,local_pos)
    local construct=constructs["native-"..tostring(native_id)]
    local index=M.find_node_index(construct,local_pos)
    if not index then return false end
    construct.nodes[index].destroyed=true;construct._collision_samples=nil;refresh_bounds(construct);save_all();sync_visuals(construct);return true
end

function M.sync_native_place(native_id,local_pos,node_name,param1,param2,metadata)
    local construct=constructs["native-"..tostring(native_id)];if not construct then return false end
    local index=M.find_node_index(construct,local_pos)
    local entry=index and construct.nodes[index]
    if entry then
        entry.name=node_name;entry.param1=param1 or 0;entry.param2=param2 or 0;entry.metadata=metadata or "";entry.destroyed=false
    else
        construct.nodes[#construct.nodes+1]={local_pos=vector.new(local_pos),name=node_name,param1=param1 or 0,param2=param2 or 0,metadata=metadata or "",destroyed=false}
    end
    construct._collision_samples=nil;refresh_bounds(construct);save_all();sync_visuals(construct);return true
end

function M.world_position(construct,local_pos) return world_position(construct,local_pos) end
function M.world_to_local(construct,world_pos) return world_to_local(construct,world_pos) end

local function object_method(object, method_name)
    if not object then return nil end
    local ok, method = pcall(function() return object[method_name] end)
    return ok and type(method) == "function" and method or nil
end

local function player_object(player_or_name)
    if type(player_or_name) == "string" then
        return core.get_player_by_name(player_or_name), player_or_name
    end
    if object_method(player_or_name, "get_player_name") then
        local ok, name = pcall(function() return player_or_name:get_player_name() end)
        if ok and name and name ~= "" then return player_or_name, name end
    end
    return nil, nil
end

local function clear_downward_velocity(player)
    if not (player and player.get_velocity and player.add_velocity) then return end
    local velocity = player:get_velocity()
    if velocity and (tonumber(velocity.y) or 0) < 0 then
        player:add_velocity({x = 0, y = -velocity.y, z = 0})
    end
end

local function construct_from_value(value)
    if type(value) == "table" then return value end
    if type(value) == "string" then return constructs[value] or M.get_for_owner(value) end
    return nil
end

local function onboard_volume_for_position(construct, position)
    if not construct or not position or construct._settling_wreck then return nil end
    local bounds = passenger_bounds(construct)
    local local_pos = world_to_local(construct, position)
    if not contains_local(bounds, local_pos) then return nil end
    return {construct = construct, local_pos = local_pos, bounds = bounds}
end

local function support_floor_for_local(construct, local_pos)
    if not construct or not local_pos then return nil end
    local best
    for _, entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            local node = entry.local_pos
            if math.abs(local_pos.x - node.x) <= 0.72
                    and math.abs(local_pos.z - node.z) <= 0.72
                    and local_pos.y >= node.y - 0.25
                    and local_pos.y <= node.y + 2.25 then
                local support_y = node.y + 1.0
                if not best or support_y > best then best = support_y end
            end
        end
    end
    return best
end

local function best_onboard_volume(position, preferred_id)
    if preferred_id and constructs[preferred_id] then
        local preferred = onboard_volume_for_position(constructs[preferred_id], position)
        if preferred then return preferred end
    end
    local best, best_distance
    for _,construct in pairs(constructs) do
        local onboard = onboard_volume_for_position(construct, position)
        if onboard then
            local distance = vector.distance(position, construct.position)
            if not best_distance or distance < best_distance then
                best, best_distance = onboard, distance
            end
        end
    end
    return best
end

local function save_player_physics(player, name)
    if not (player and player.set_physics_override and name and name ~= "") then return end
    if not helm_physics[name] then
        local physics
        if player.get_physics_override then
            physics = copy(player:get_physics_override() or {})
        end
        physics = physics or {}
        helm_physics[name] = {
            speed = physics.speed or 1,
            jump = physics.jump or 1,
            gravity = physics.gravity or 1,
        }
    end
    player:set_physics_override({speed = 0, jump = 0, gravity = 0})
end

local function restore_player_physics(player_or_name)
    local player, name = player_object(player_or_name)
    if not name or name == "" then return end
    local physics = helm_physics[name]
    helm_physics[name] = nil
    if player and player.set_detach then pcall(function() player:set_detach() end) end
    if player and player.set_physics_override then
        player:set_physics_override(physics or {speed = 1, jump = 1, gravity = 1})
    end
end

local function helm_entry(construct, node_index)
    local entry = node_index and construct.nodes and construct.nodes[node_index]
    if entry and not entry.destroyed then return entry, node_index end
    for index, candidate in ipairs(construct.nodes or {}) do
        local definition = core.registered_nodes[candidate.name] or {}
        if not candidate.destroyed and (definition._navycraft_component == "helm" or definition._navycraft_helm) then
            return candidate, index
        end
    end
    return nil, nil
end

local function helm_seat_local_pos(construct, node_index)
    local entry, index = helm_entry(construct, node_index)
    if not entry or not entry.local_pos then return nil, nil end
    local facing = entry.local_facing or facedir_vector(entry.param2 or 0)
    local behind = {x = -facing.x, y = 0, z = -facing.z}
    if behind.x == 0 and behind.z == 0 then behind.z = -1 end
    local candidate = vector.add(entry.local_pos, behind)
    return {x = candidate.x, y = entry.local_pos.y, z = candidate.z}, index
end

local function helm_seat_world_pos(construct, seat_local_pos)
    local world = world_position(construct, seat_local_pos)
    return {x = world.x, y = world.y + 1.0, z = world.z}
end

local function valid_object(object)
    if not object_method(object, "get_pos") then return false end
    local ok, position = pcall(function() return object:get_pos() end)
    return ok and position ~= nil
end

local function construct_anchor(construct)
    if not construct then return nil end
    local visuals = construct_visuals()
    if visuals and type(visuals.anchor) == "function" then
        local anchor = visuals.anchor(construct)
        if valid_object(anchor) then return anchor end
    end
    sync_visuals(construct)
    local shell = construct._visual_shell
    local anchor = shell and shell.anchor or nil
    return valid_object(anchor) and anchor or nil
end

local function helm_attach_offset(seat_local_pos)
    return {
        x = seat_local_pos.x * ATTACH_OFFSET_SCALE,
        y = (seat_local_pos.y + 1.0) * ATTACH_OFFSET_SCALE,
        z = seat_local_pos.z * ATTACH_OFFSET_SCALE,
    }
end

local function create_driver_seat(position)
    if type(core.add_entity) ~= "function" then return nil end
    local object = core.add_entity(position or {x = 0, y = 0, z = 0}, HELM_DRIVER_SEAT_ENTITY)
    return valid_object(object) and object or nil
end

local function attach_seat_to_ship(seat_object, anchor, seat_local_pos)
    if not (valid_object(seat_object) and valid_object(anchor) and seat_object.set_attach) then return false end
    local ok = pcall(function()
        seat_object:set_attach(anchor, "", helm_attach_offset(seat_local_pos), {x = 0, y = 0, z = 0}, true)
    end)
    if not ok then return false end
    if seat_object.get_attach then return seat_object:get_attach() == anchor end
    return true
end

local function attach_player_to_seat(player, seat_object)
    if not (player and player.set_attach and valid_object(seat_object)) then return false end
    local ok = pcall(function()
        player:set_attach(seat_object, "", {x = 0, y = 0, z = 0}, {x = 0, y = 0, z = 0}, true)
    end)
    if not ok then return false end
    if player.get_attach then return player:get_attach() == seat_object end
    return true
end

local function release_helm_seat(name, options)
    local seat = helm_seats[name]
    if not seat then restore_player_physics(name); return false end
    local construct = constructs[seat.construct_id]
    if construct and construct.systems and construct.systems.driver == name then
        construct.systems.driver = nil
    end
    helm_seats[name] = nil
    restore_player_physics(name)
    if seat.seat_object and seat.seat_object.remove then seat.seat_object:remove() end
    if not (options and options.keep_rider) then rider_support[name] = nil end
    save_all()
    return true
end

local function update_helm_seat(player, name, seat)
    local construct = constructs[seat.construct_id]
    if not construct or not player or not player.set_pos then
        release_helm_seat(name)
        return false
    end
    local seat_local_pos, helm_index = helm_seat_local_pos(construct, seat.helm_index)
    if not seat_local_pos then
        release_helm_seat(name)
        return false
    end
    seat.local_pos = seat_local_pos
    seat.helm_index = helm_index
    local anchor = construct_anchor(construct)
    if not anchor then
        core.chat_send_player(name, "Helm released: ship seat anchor unavailable")
        release_helm_seat(name)
        return false
    end
    local seat_position = helm_seat_world_pos(construct, seat.local_pos)
    if not valid_object(seat.seat_object) then
        seat.seat_object = create_driver_seat(seat_position)
        seat.attached = false
    end
    if not valid_object(seat.seat_object) then
        core.chat_send_player(name, "Helm released: driver seat unavailable")
        release_helm_seat(name)
        return false
    end
    if seat.anchor ~= anchor or (seat.seat_object.get_attach and seat.seat_object:get_attach() ~= anchor) then
        seat.anchor = anchor
        if not attach_seat_to_ship(seat.seat_object, anchor, seat.local_pos) then
            core.chat_send_player(name, "Helm released: driver seat attach failed")
            release_helm_seat(name)
            return false
        end
    end
    if player.get_attach and player:get_attach() ~= seat.seat_object then seat.attached = false end
    if not seat.attached then seat.attached = attach_player_to_seat(player, seat.seat_object) end
    if not seat.attached then
        core.chat_send_player(name, "Helm released: player attach failed")
        release_helm_seat(name)
        return false
    end
    construct._onboard_players = construct._onboard_players or {}
    construct._onboard_players[name] = true
    return true
end

local function update_rider_volume(player, name, controls)
    local position = player:get_pos()
    if not position then rider_support[name] = nil; return end
    local previous = rider_support[name]
    if controls.jump and previous then
        jump_released_riders[name] = previous.construct_id
        rider_support[name] = nil
        return
    end
    local onboard = best_onboard_volume(position, previous and previous.construct_id or nil)
    if not onboard then
        rider_support[name] = nil
        jump_released_riders[name] = nil
        return
    end
    if jump_released_riders[name] == onboard.construct.id then return end
    if previous and previous.construct_id == onboard.construct.id and previous.local_pos and previous.world_pos and player.set_pos then
        local carried_world = world_position(onboard.construct, previous.local_pos)
        local delta = vector.subtract(carried_world, previous.world_pos)
        local distance_sq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z
        if distance_sq > 0.000001 and distance_sq < 25 then
            position = vector.add(position, delta)
            player:set_pos(position)
        end
    end
    local local_pos = world_to_local(onboard.construct, position)
    local support_y = support_floor_for_local(onboard.construct, local_pos)
    if support_y then
        local support_world = world_position(onboard.construct, {x = local_pos.x, y = support_y, z = local_pos.z})
        if position.y < support_world.y and player.set_pos then
            position = {x = position.x, y = support_world.y, z = position.z}
            player:set_pos(position)
            local_pos = world_to_local(onboard.construct, position)
        end
    end
    local world_pos = world_position(onboard.construct, local_pos)
    rider_support[name] = {
        construct_id = onboard.construct.id,
        local_pos = local_pos,
        world_pos = world_pos,
    }
    clear_downward_velocity(player)
    onboard.construct._onboard_players = onboard.construct._onboard_players or {}
    onboard.construct._onboard_players[name] = true
end

local function update_player_support()
    if type(core.get_connected_players) ~= "function" then return end
    for _,construct in pairs(constructs) do construct._onboard_players = {} end
    for _,player in ipairs(core.get_connected_players()) do
        if object_method(player, "get_player_name") and object_method(player, "get_pos") then
            local name = player:get_player_name()
            local controls = player.get_player_control and player:get_player_control() or {}
            local seat = helm_seats[name]
            if seat then
                update_helm_seat(player, name, seat)
            else
                update_rider_volume(player, name, controls)
            end
        end
    end
    for _,construct in pairs(constructs) do
        local count = 0
        for _ in pairs(construct._onboard_players or {}) do count = count + 1 end
        construct._onboard_count = count
    end
end

within_construct = function(construct, position)
    if not construct or not position then return false end
    local local_pos = world_to_local(construct, position)
    local node_pos = vector.round(local_pos)
    return M.find_node_index(construct, node_pos) ~= nil
end

function M.player_support(player_or_name)
    local player, name = player_object(player_or_name)
    if name and helm_seats[name] then return constructs[helm_seats[name].construct_id] end
    if not player or not player.get_pos then return nil end
    local position = player:get_pos()
    if not position then return nil end
    local onboard = best_onboard_volume(position, rider_support[name] and rider_support[name].construct_id or nil)
    return onboard and onboard.construct or nil
end

function M.player_helm_construct(player_or_name)
    local _, name = player_object(player_or_name)
    local seat = name and helm_seats[name] or nil
    return seat and constructs[seat.construct_id] or nil
end

function M.take_helm(player_or_name, construct_or_id, node_index)
    local player, name = player_object(player_or_name)
    local construct = construct_from_value(construct_or_id)
    if not player or not name or name == "" then return false, "player unavailable" end
    if not construct then return false, "no active vessel" end
    local existing = helm_seats[name]
    if existing and existing.construct_id == construct.id then
        release_helm_seat(name)
        return true, "Helm released"
    elseif existing then
        release_helm_seat(name)
    end
    local seat_local_pos, helm_index = helm_seat_local_pos(construct, node_index)
    if not seat_local_pos then return false, "helm block not found" end
    local helm_node = construct.nodes and construct.nodes[helm_index] or nil
    local helm_facing = cardinal_local_direction(
        helm_node and (helm_node.local_facing or facedir_vector(helm_node.param2 or 0)),
        {x = 0, y = 0, z = 1})
    local anchor = construct_anchor(construct)
    if not anchor then return false, "ship seat anchor unavailable" end
    save_player_physics(player, name)
    local seat_position = helm_seat_world_pos(construct, seat_local_pos)
    local seat_object = create_driver_seat(seat_position)
    if not seat_object then
        restore_player_physics(name)
        return false, "driver seat unavailable"
    end
    if not attach_seat_to_ship(seat_object, anchor, seat_local_pos) then
        if seat_object.remove then seat_object:remove() end
        restore_player_physics(name)
        return false, "driver seat attach failed"
    end
    player:set_pos(seat_position)
    local attached = attach_player_to_seat(player, seat_object)
    if not attached then
        if seat_object.remove then seat_object:remove() end
        restore_player_physics(name)
        return false, "helm seat attach failed"
    end
    helm_seats[name] = {
        construct_id = construct.id,
        local_pos = seat_local_pos,
        helm_index = helm_index,
        anchor = anchor,
        seat_object = seat_object,
        attached = true,
    }
    if not update_helm_seat(player, name, helm_seats[name]) then
        return false, "helm seat validation failed"
    end
    if construct.systems then
        construct.systems.active_helm_index = helm_index
        construct.systems.active_helm_local_facing = helm_facing
    end
    rider_support[name] = nil
    jump_released_riders[name] = nil
    construct._onboard_count = math.max(1, tonumber(construct._onboard_count) or 0)
    core.log("action", string.format("[NavyCraft] %s engaged helm on %s at local (%.2f,%.2f,%.2f)",
        name, tostring(construct.id), seat_local_pos.x or 0, seat_local_pos.y or 0, seat_local_pos.z or 0))
    save_all()
    return true, "Helm engaged"
end

function M.release_helm(player_or_name)
    local _, name = player_object(player_or_name)
    if not name or name == "" then return false, "player unavailable" end
    return release_helm_seat(name)
end

function M.clear_passenger(player_or_name)
    local player, name = player_object(player_or_name)
    if not name or name == "" then return false end
    local changed = false
    if helm_seats[name] then
        release_helm_seat(name)
        changed = true
    else
        restore_player_physics(player or name)
    end
    rider_support[name] = nil
    jump_released_riders[name] = nil
    for _,construct in pairs(constructs) do
        local systems = construct.systems
        if systems and systems.driver == name then
            systems.driver = nil
            systems.remote_control = false
            systems.abandoned = false
            changed = true
        end
    end
    if changed then save_all() end
    return changed
end

clear_construct_player_state = function(construct)
    if not construct then return end
    local id = construct.id
    for name, seat in pairs(copy(helm_seats)) do
        if seat.construct_id == id then release_helm_seat(name) end
    end
    for name, state in pairs(copy(rider_support)) do
        if state.construct_id == id then rider_support[name] = nil end
    end
    for name, construct_id in pairs(copy(jump_released_riders)) do
        if construct_id == id then jump_released_riders[name] = nil end
    end
end

function M.snapshot(construct) if type(construct)=="string" then construct=constructs[construct] end;return construct and clean_construct(construct) or nil end
function M.save() save_all() end
function M.register_launch_hook(callback) launch_hooks[#launch_hooks+1]=callback end
function M.register_step_hook(callback) step_hooks[#step_hooks+1]=callback end
function M.register_dock_hook(callback) dock_hooks[#dock_hooks+1]=callback end
function M.register_boarding_filter(callback) boarding_filters[#boarding_filters+1]=callback end
function M.register_interaction_hook(callback) interaction_hooks[#interaction_hooks+1]=callback end
function M.register_damage_hook(callback) damage_hooks[#damage_hooks+1]=callback end
function M.register_remove_hook(callback) remove_hooks[#remove_hooks+1]=callback end

function M.set_motion(owner,forward_speed,yaw_rate,vertical_speed)
    local construct=M.get_for_owner(owner);if not construct then return false,"no active construct" end
    if construct.systems then return false,"use helm controls or /ship throttle|gear|rudder for NavyCraft vessels" end
    if forward_speed~=nil then construct.forward_speed=clamp(forward_speed,-MAX_FORWARD_SPEED,MAX_FORWARD_SPEED) end
    if yaw_rate~=nil then construct.yaw_rate=clamp(yaw_rate,-MAX_YAW_RATE,MAX_YAW_RATE);construct.turn_remaining=0 end
    if vertical_speed~=nil then construct.vertical_speed=clamp(vertical_speed,-MAX_VERTICAL_SPEED,MAX_VERTICAL_SPEED) end
    local ok,error_message=send_drive_velocity(construct,true);save_all();return ok~=nil and ok or false,error_message
end

function M.turn(owner,degrees)
    local construct=M.get_for_owner(owner);if not construct then return false,"no active construct" end
    if construct.systems then
        local nc=rawget(_G,"navycraft")
        if nc and nc.systems and nc.systems.rudder_order then
            return nc.systems.rudder_order(construct,(tonumber(degrees) or 0)<0 and -1 or 1,true)
        end
        return false,"use helm controls or /ship turn for NavyCraft vessels"
    end
    construct.turn_remaining=clamp((construct.turn_remaining or 0)+math.rad(degrees),-MAX_QUEUED_TURN,MAX_QUEUED_TURN)
    construct.yaw_rate=sign(construct.turn_remaining)*MAX_YAW_RATE
    send_drive_velocity(construct,true);save_all()
    return true,string.format("queued %.1f° turn",math.deg(construct.turn_remaining))
end

function M.stop(owner)
    local construct=M.get_for_owner(owner);if not construct then return false,"no active construct" end
    if construct.systems then
        construct.systems.set_speed=0;construct.systems.throttle=0;construct.systems.rudder=0;construct.systems.turn_progress=0;construct.systems.turn_elapsed=0;construct.systems.target_forward_speed=0
    end
    construct.forward_speed=0;construct.vertical_speed=0;construct.yaw_rate=0;construct.turn_remaining=0
    local ok,error_message=send_drive_velocity(construct,true);save_all();return ok~=nil and ok or false,error_message
end

function M.status(owner)
    local construct=M.get_for_owner(owner);if not construct then return nil end
    refresh_native(construct)
    if true then
    local systems = construct.systems or {}
    local driver = systems.driver or "-"
    local sinking = systems.sinking and "yes" or "no"
    return string.format("%s: %d nodes, position %s, yaw %.1f deg, speed %.2f target %.2f, queued turn %.1f deg, spin %.1f deg/s, rise %.2f, set=%s gear=%s throttle=%d%% sinking=%s driver=%s onboard=%d blocked=%s",
        construct.id,active_node_count(construct),core.pos_to_string(construct.position),math.deg(construct.yaw),construct.forward_speed or 0,
        tonumber(systems.target_forward_speed) or 0,math.deg(construct.turn_remaining or 0),math.deg(construct.yaw_rate or 0),construct.vertical_speed or 0,
        tostring(systems.set_speed or "-"),tostring(systems.gear or "-"),math.floor((tonumber(systems.throttle) or 0)*100+0.5),
        sinking,tostring(driver),tonumber(construct._onboard_count) or 0,tostring(construct._last_motion_block or "-"))
    end
    return string.format("%s: %d nodes, position %s, yaw %.1f°, speed %.2f, queued turn %.1f°, spin %.1f°/s, rise %.2f",
        construct.id,active_node_count(construct),core.pos_to_string(construct.position),math.deg(construct.yaw),construct.forward_speed or 0,
        math.deg(construct.turn_remaining or 0),math.deg(construct.yaw_rate or 0),construct.vertical_speed or 0)
end

local function stop_for_block_conversion(construct, previous_frozen)
    local systems = construct and construct.systems or nil
    if systems then
        systems.conversion_pending = true
        systems.frozen = true
        systems.abandoned = false
        systems.captain_abandoned = false
        systems.remote_control = false
        systems.autotravel = false
        systems.throttle = 0
        systems.set_speed = 0
        systems.gear = 0
        systems.rudder = 0
        systems.turn_progress = 0
        systems.turn_elapsed = 0
        systems.vertical_planes = 0
        systems.target_forward_speed = 0
        systems.engines_on = false
        systems._conversion_previous_frozen = previous_frozen and true or false
        for _, state in pairs(systems.engines or {}) do
            state.set_on = false
        end
    end
    construct.forward_speed = 0
    construct.vertical_speed = 0
    construct.yaw_rate = 0
    construct.turn_remaining = 0
    stop_native_velocity(construct)
    send_drive_velocity(construct, true)
end

local function finish_restore_to_blocks(construct, quarter, reason)
    for _,callback in ipairs(dock_hooks) do
        local ok,err=callback(construct)
        if ok==false then return false,err or "docking hook rejected construct" end
    end
    local ok,err=restore_world_nodes(construct,quarter)
    if not ok then return false,err end
    reason=reason or "converted_to_blocks"
    clear_construct_player_state(construct)
    remove_visuals(construct)
    core.remove_dynamic_construct(construct.native_id)
    constructs[construct.id]=nil
    pending_conversions[construct.id]=nil
    if owner_active[construct.owner]==construct.id then owner_active[construct.owner]=nil end
    save_all()
    return true,reason
end

function M.convert_to_blocks(id_or_owner, reason)
    local construct=construct_from_value(id_or_owner)
    if not construct then return false,"construct not found" end
    local refreshed,error_message=refresh_native(construct)
    if not refreshed then return false,error_message end
    local quarter=math.floor(construct.yaw/HALF_PI+0.5)%4
    construct.yaw=quarter*HALF_PI
    stop_for_block_conversion(construct, construct.systems and construct.systems.frozen)
    return finish_restore_to_blocks(construct,quarter,reason)
end

function M.request_convert_to_blocks(id_or_owner, requester)
    local construct=construct_from_value(id_or_owner)
    if not construct then return false,"construct not found" end
    if pending_conversions[construct.id] then return false,"vessel conversion already armed" end
    local delay=configured_conversion_delay()
    local requester_name=type(requester)=="string" and requester or nil
    if type(requester)=="table" and requester.get_player_name then requester_name=requester:get_player_name() end
    local was_frozen=construct.systems and construct.systems.frozen
    pending_conversions[construct.id]={requester=requester_name,was_frozen=was_frozen}
    stop_for_block_conversion(construct, was_frozen)
    save_all()
    local id=construct.id
    local function finish()
        local pending=pending_conversions[id]
        pending_conversions[id]=nil
        local current=constructs[id]
        if not current then return end
        local ok,message=M.convert_to_blocks(current,"helm_conversion")
        if not ok and current.systems then
            current.systems.frozen=pending and pending.was_frozen or false
            current.systems.conversion_pending=false
            current.systems._conversion_previous_frozen=nil
            save_all()
        end
        if pending and pending.requester and pending.requester~="" then
            core.chat_send_player(pending.requester,ok and "Vessel converted to editable blocks" or ("Vessel conversion failed: "..tostring(message)))
        end
    end
    if delay<=0 or type(core.after)~="function" then finish() else core.after(delay,finish) end
    return true,string.format("Converting vessel to editable blocks in %.1f second%s",delay,delay==1 and "" or "s")
end

function M.dock(owner)
    local construct=M.get_for_owner(owner);if not construct then return false,"no active construct" end
    local refreshed,error_message=refresh_native(construct);if not refreshed then return false,error_message end
    local quarter=math.floor(construct.yaw/HALF_PI+0.5)%4;local snapped=quarter*HALF_PI
    local difference=math.abs(((construct.yaw-snapped+PI)%TWO_PI)-PI)
    if difference>math.rad(5) then return false,"align the craft within 5 degrees of a cardinal direction" end
    construct.yaw=snapped
    local ok,err=finish_restore_to_blocks(construct,quarter,"docked_to_world")
    return ok, ok and "Construct docked as editable blocks" or err
end

function M.find_nearest(position,maximum_range,predicate)
    local best,best_distance;maximum_range=maximum_range or math.huge
    for _,construct in pairs(constructs) do
        refresh_native(construct)
        if not predicate or predicate(construct) then local d=vector.distance(position,construct.position);if d<=maximum_range and(not best_distance or d<best_distance) then best,best_distance=construct,d end end
    end
    return best,best_distance
end

function M.teleport(id_or_owner,position,yaw)
    local construct=constructs[id_or_owner] or M.get_for_owner(id_or_owner);if not construct then return false,"construct not found" end
    local transform={position=vector.new(position),yaw=yaw~=nil and normalise_yaw(yaw) or construct.yaw}
    local ok,error_message=core.set_dynamic_construct_transform(construct.native_id,transform);if not ok then return false,error_message end
    construct.position=transform.position;construct.yaw=transform.yaw;construct.forward_speed=0;construct.vertical_speed=0;construct.yaw_rate=0;construct.turn_remaining=0
    construct._lua_last_position=vector.new(construct.position)
    use_lua_motion_driver(construct)
    send_drive_velocity(construct,true);sync_visuals(construct);save_all();return true
end

function M.transfer_owner(id_or_construct,new_owner,options)
    options=options or {};local construct=type(id_or_construct)=="table" and id_or_construct or constructs[id_or_construct] or M.get_for_owner(id_or_construct)
    if not construct then return false,"construct not found" end;new_owner=tostring(new_owner or "");if new_owner=="" then return false,"new owner required" end
    local old_owner=construct.owner;if old_owner==new_owner then return true,construct end
    if owner_active[new_owner] and owner_active[new_owner]~=construct.id and not options.allow_multiple then return false,"new owner already has an active vessel" end
    if owner_active[old_owner]==construct.id then owner_active[old_owner]=nil end;if not owner_active[new_owner] then owner_active[new_owner]=construct.id end
    construct.owner=options.runtime_owner or new_owner;construct.systems=construct.systems or {};construct.systems.owner=new_owner;construct.systems.captain=options.captain or new_owner;construct.systems.driver=options.driver or new_owner
    if options.reset_crew~=false then construct.systems.crew={[new_owner]="owner"};construct.systems.crew_history=construct.systems.crew_history or {};construct.systems.crew_history[new_owner]=true end
    construct.systems.abandoned=false;construct.systems.captain_abandoned=false;construct.systems.taking_over=nil;save_all();return true,construct
end

function M.remove(id_or_owner,restore,reason)
    local construct=constructs[id_or_owner] or M.get_for_owner(id_or_owner);if not construct then return false,"construct not found" end
    refresh_native(construct)
    if restore then local quarter=math.floor(construct.yaw/HALF_PI+0.5)%4;local ok,err=restore_world_nodes(construct,quarter);if not ok then return false,err end end
    reason=reason or(restore and "restored_to_world" or "removed")
    for _,callback in ipairs(remove_hooks) do local ok,err=pcall(callback,construct,reason,clean_construct(construct));if not ok then core.log("error","[NavyCraft] remove hook failed: "..tostring(err)) end end
    clear_construct_player_state(construct)
    remove_visuals(construct)
    core.remove_dynamic_construct(construct.native_id);constructs[construct.id]=nil;if owner_active[construct.owner]==construct.id then owner_active[construct.owner]=nil end;save_all();return true
end

function M.purge_all(reason)
    reason=reason or "manual_native_purge"
    for _,construct in pairs(constructs) do
        for _,callback in ipairs(remove_hooks) do
            local ok,err=pcall(callback,construct,reason,clean_construct(construct))
            if not ok then core.log("error","[NavyCraft] purge hook failed: "..tostring(err)) end
        end
        clear_construct_player_state(construct)
        remove_visuals(construct)
    end
    local removed=0
    local seen={}
    for _,entry in ipairs(runtime_constructs()) do
        local native_id=runtime_id(entry)
        if native_id and not seen[native_id] then
            seen[native_id]=true
            if core.remove_dynamic_construct(native_id) then removed=removed+1 end
        end
    end
    constructs={};owner_active={}
    storage:set_string("active_constructs_v1","")
    save_all()
    if type(core.sync_dynamic_construct_persistence) == "function" then
        local _,error_message=core.sync_dynamic_construct_persistence()
        if error_message then core.log("error","[NavyCraft] purge persistence sync failed: "..tostring(error_message)) end
    end
    return true,string.format("Purged %d native construct(s)",removed)
end

local function remove_native_nodes(construct,positions)
    local items={}
    for _,local_pos in ipairs(positions) do
        items[#items+1]={local_pos=local_pos,force_destroy=true,breached=true}
    end
    local removed=apply_damage_items(construct,items,construct.position,nil,"structural",{})
    return removed
end

function M.damage_radius(position,radius,force,attacker,damage_type)
    radius=math.max(0.1,radius or 1);force=math.max(0.1,force or 1);local results={}
    for _,construct in pairs(constructs) do
        refresh_native(construct);local items={}
        for _,entry in ipairs(construct.nodes or {}) do if not entry.destroyed then
            local d=vector.distance(world_position(construct,entry.local_pos),position)
            if d<=radius then
                local groups=node_groups(entry)
                local armour=math.max(0.25,tonumber(groups.navycraft_armour) or tonumber(groups.cracky) or 1)
                local effective=force*(1-d/(radius+0.001))
                if effective>=armour*0.12 then
                    items[#items+1]={
                        local_pos=entry.local_pos,
                        amount=effective*3.25,
                        breached=local_pos_breached(construct,entry,position,damage_type=="torpedo" or damage_type=="depth_charge"),
                    }
                end
            end
        end end
        if #items>0 then
            local removed,details=apply_damage_items(construct,items,position,attacker,damage_type or "explosion",{})
            if details.damaged>0 or removed>0 then
                results[#results+1]={id=construct.id,removed=removed,damaged=details.damaged,breaches=details.breaches,alive=details.alive}
            end
        end
    end
    save_all();return results
end

function M.apply_native_damage(native_id,node_damage,position,attacker,damage_type)
    local construct=constructs["native-"..tostring(native_id)];if not construct then return nil,"native construct not found" end
    local items={}
    for _,damage in ipairs(node_damage or {}) do
        if damage.node_pos then
            local effective=tonumber(damage.effective_power or damage.power or 0) or 0
            items[#items+1]={
                local_pos=damage.node_pos,
                amount=math.max(0,effective*3.25),
                force_destroy=damage.destroyed==true,
                breached=damage.breached==true,
            }
        end
    end
    local removed,details=apply_damage_items(construct,items,position or construct.position,attacker,damage_type or "native_projectile",{allow_missing_native=true})
    return construct,removed,details
end

function M.spawn_snapshot(owner,snapshot,position,yaw,options)
    options=options or {}
    local previous_id = owner_active[owner]
    local previous = previous_id and constructs[previous_id] or nil
    if not snapshot or type(snapshot.nodes)~="table" then return nil,"invalid snapshot" end
    local native_id,error_message=create_native(owner,snapshot.nodes,vector.new(position),normalise_yaw(yaw or 0));if not native_id then return nil,error_message end
    local construct={id="native-"..tostring(native_id),native_id=native_id,owner=owner,position=vector.new(position),yaw=normalise_yaw(yaw or 0),forward_speed=0,vertical_speed=0,yaw_rate=0,turn_remaining=0,nodes=copy(snapshot.nodes),profile=copy(snapshot.profile or {}),systems=copy(snapshot.systems or {})}
    refresh_bounds(construct)
    ensure_construct_damage_state(construct)
    update_damage_summary(construct)
    construct.systems=construct.systems or {}
    construct.systems.sinking=false
    construct.systems.launch_settle_until=os.time()+3
    construct._lua_last_position=vector.new(construct.position)
    use_lua_motion_driver(construct)
    depenetrate_construct(construct,12)
    construct._lua_last_position=vector.new(construct.position)
    if previous and previous.id ~= construct.id and not options.allow_multiple and not is_sunk(previous) then
        freeze_construct(previous)
    end
    local spawned,spawn_error=spawn_visuals(construct)
    if spawned==false then
        core.remove_dynamic_construct(native_id)
        return nil,spawn_error or "failed to create visual shell"
    end
    constructs[construct.id]=construct
    if not options.allow_multiple then owner_active[owner]=construct.id end
    save_all();sync_visuals(construct);return construct.id
end

local function restore_saved_constructs()
    local raw=storage:get_string("active_native_constructs_v1")
    if raw=="" then
        -- Deliberately discard legacy lua-* state instead of resurrecting the
        -- removed entity renderer.
        storage:set_string("active_constructs_v1","")
        local ghost_ids=untracked_runtime_ids()
        if #ghost_ids>0 then
            core.log("warning","[NavyCraft] native runtime has "..#ghost_ids.." untracked construct(s); use /nc_purge_constructs before launching")
        end
        return
    end
    local loaded=core.deserialize(raw);if type(loaded)~="table" then core.log("error","[NavyCraft] native construct storage could not be decoded");return end
    for _,saved in pairs(loaded) do
        if saved.owner and type(saved.nodes)=="table" and saved.position then
            local native_id=tonumber(saved.native_id)
            if native_id and native_id~=0 then
                pcall(core.remove_dynamic_construct,native_id)
            end
            local state
            local error_message
            native_id,error_message=create_native(saved.owner,saved.nodes,vector.new(saved.position),normalise_yaw(saved.yaw or 0))
            if not native_id then
                core.log("error","[NavyCraft] native construct restore failed: "..tostring(error_message))
            else
                state=core.get_dynamic_construct(native_id,false)
            end
            if native_id then
                saved.native_id=native_id;saved.id="native-"..tostring(native_id);saved.position=vector.new(state and state.position or saved.position);saved.yaw=normalise_yaw(state and state.yaw or saved.yaw or 0);refresh_bounds(saved);saved.forward_speed=saved.forward_speed or 0;saved.vertical_speed=saved.vertical_speed or 0;saved.yaw_rate=saved.yaw_rate or 0;saved.turn_remaining=saved.turn_remaining or 0
                saved.systems=saved.systems or {}
                ensure_construct_damage_state(saved)
                update_damage_summary(saved)
                saved.systems.sinking=saved.systems.sinking and true or false
                saved.systems.launch_settle_until=os.time()+3
                saved._lua_last_position=vector.new(saved.position)
                use_lua_motion_driver(saved)
                depenetrate_construct(saved,12)
                saved._lua_last_position=vector.new(saved.position)
                constructs[saved.id]=saved;owner_active[saved.owner]=saved.id;spawn_visuals(saved);send_drive_velocity(saved,true);sync_visuals(saved)
            end
        end
    end
    save_all()
end

core.register_globalstep(function(dtime)
    step_accumulator=step_accumulator+dtime;if step_accumulator<0.05 then return end
    local dt=step_accumulator;step_accumulator=0
    local settle_ids={}
    for id,construct in pairs(constructs) do
        local previous_position=construct._lua_last_position and vector.new(construct._lua_last_position) or nil
        local previous_yaw=construct._lua_last_yaw
        local ok=refresh_native(construct)
        if not ok then constructs[id]=nil;if owner_active[construct.owner]==id then owner_active[construct.owner]=nil end
        else
            for _,callback in ipairs(step_hooks) do callback(construct,dt) end
            if not construct.systems and math.abs(construct.turn_remaining or 0)>0.0001 then
                local amount=math.min(math.abs(construct.turn_remaining),MAX_YAW_RATE*dt)
                construct.turn_remaining=construct.turn_remaining-sign(construct.turn_remaining)*amount
                construct.yaw_rate=sign(construct.turn_remaining)*MAX_YAW_RATE
                if math.abs(construct.turn_remaining or 0)<0.001 then construct.turn_remaining=0;construct.yaw_rate=0 end
            end
            if motion_active(construct) then
                if construct._lua_motion_fallback then
                    local moved_ok,move_error=integrate_lua_motion(construct,dt)
                    if not moved_ok and move_error then
                        core.log("warning","[NavyCraft] Lua motion fallback failed for "..tostring(construct.id)..": "..tostring(move_error))
                    end
                else
                    local drive_ok,drive_error=send_drive_velocity(construct,false)
                    if not drive_ok and drive_error then
                        core.log("warning","[NavyCraft] native motion drive failed for "..tostring(construct.id)..": "..tostring(drive_error))
                    end
                    local moved=previous_position and (
                        vector.distance(construct.position,previous_position)>0.001 or
                        (previous_yaw and yaw_difference(construct.yaw,previous_yaw)>0.001))
                    if moved then
                        construct._lua_stall_ticks=0
                    else
                        construct._lua_stall_ticks=(construct._lua_stall_ticks or 0)+1
                    end
                    if construct._lua_stall_ticks>=4 then
                        construct._lua_motion_fallback=true
                        construct._lua_stall_ticks=0
                        pcall(core.set_dynamic_construct_velocity,construct.native_id,{x=0,y=0,z=0},0)
                        core.log("warning","[NavyCraft] Lua motion fallback engaged for "..tostring(construct.id))
                        local moved_ok,move_error=integrate_lua_motion(construct,dt)
                        if not moved_ok and move_error then
                            core.log("warning","[NavyCraft] Lua motion fallback failed for "..tostring(construct.id)..": "..tostring(move_error))
                        end
                    end
                end
            else
                construct._lua_stall_ticks=0
                if not construct._lua_motion_fallback then
                    send_drive_velocity(construct,false)
                else
                    sync_lua_prediction_velocity(construct,false)
                end
            end
            sync_visuals(construct)
            construct._lua_last_position=vector.new(construct.position)
            construct._lua_last_yaw=construct.yaw
            if construct_should_settle(construct) then settle_ids[#settle_ids+1]=id end
        end
    end
    for _,id in ipairs(settle_ids) do
        local construct=constructs[id]
        if construct then
            local ok,error_message=settle_sunk_construct(construct)
            if not ok and error_message then
                core.log("warning","[NavyCraft] sunk wreck settlement failed for "..tostring(id)..": "..tostring(error_message))
            end
        end
    end
    update_player_support()
    wreck_cleanup_accumulator=wreck_cleanup_accumulator+dt
    if wreck_cleanup_accumulator>=WRECK_CLEANUP_INTERVAL then
        wreck_cleanup_accumulator=0
        cleanup_settled_wrecks(false)
    end
end)

load_wrecks()
core.register_on_mods_loaded(function()
    cleanup_settled_wrecks(false)
    core.after(0,restore_saved_constructs)
end)
core.register_on_leaveplayer(function(player)
    if player and player.get_player_name then
        local name=player:get_player_name()
        release_helm_seat(name)
        rider_support[name]=nil
        jump_released_riders[name]=nil
        helm_physics[name]=nil
    end
end)
core.register_on_shutdown(function()
    save_all()
    save_wrecks()
end)
return M

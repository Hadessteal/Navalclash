-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Permanent Lua gameplay/state adapter for native engine constructs.
-- This module never renders hull blocks, moves constructs, or carries riders.
local M = {}
local storage = core.get_mod_storage()
local constructs = {}
local owner_active = {}
local launch_hooks, remove_hooks, step_hooks, dock_hooks = {}, {}, {}, {}
local boarding_filters, interaction_hooks, damage_hooks = {}, {}, {}
local MAX_FORWARD_SPEED = 8
local MAX_VERTICAL_SPEED = 4
local MAX_YAW_RATE = math.rad(45)
local MAX_QUEUED_TURN = math.rad(720)
local PI, HALF_PI, TWO_PI = math.pi, math.pi / 2, math.pi * 2
local MOTION_EPSILON = 0.0001
local step_accumulator = 0

local function copy(value)
    if type(value) ~= "table" then return value end
    local result = {}
    for key, child in pairs(value) do result[copy(key)] = copy(child) end
    return result
end

local function normalise_yaw(yaw)
    yaw = (tonumber(yaw) or 0) % TWO_PI
    if yaw < 0 then yaw = yaw + TWO_PI end
    return yaw
end

local function clamp(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, tonumber(value) or 0))
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

local function rotate_quarter(local_pos, quarter_turn)
    local x, z = local_pos.x, local_pos.z
    if quarter_turn == 0 then return {x=x, y=local_pos.y, z=z} end
    if quarter_turn == 1 then return {x=-z, y=local_pos.y, z=x} end
    if quarter_turn == 2 then return {x=-x, y=local_pos.y, z=-z} end
    return {x=z, y=local_pos.y, z=-x}
end

local function world_position(construct, local_pos)
    return vector.add(construct.position, rotate(local_pos, construct.yaw))
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

local function compute_pivot(scan_result)
    return {
        x=(scan_result.minp.x + scan_result.maxp.x)/2,
        y=(scan_result.minp.y + scan_result.maxp.y)/2,
        z=(scan_result.minp.z + scan_result.maxp.z)/2,
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

local function active_node_count(construct)
    local count=0
    for _,entry in ipairs(construct.nodes or {}) do if not entry.destroyed then count=count+1 end end
    return count
end

local function clean_construct(construct)
    return {
        id=construct.id,native_id=construct.native_id,owner=construct.owner,
        position=vector.new(construct.position),yaw=construct.yaw,
        forward_speed=construct.forward_speed or 0,vertical_speed=construct.vertical_speed or 0,
        yaw_rate=construct.yaw_rate or 0,turn_remaining=construct.turn_remaining or 0,
        nodes=copy(construct.nodes or {}),profile=copy(construct.profile or {}),
        systems=copy(construct.systems or {}),
    }
end

local function save_all()
    local serializable={}
    for id,construct in pairs(constructs) do serializable[id]=clean_construct(construct) end
    storage:set_string("active_native_constructs_v1",core.serialize(serializable))
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
    construct.position=vector.new(state.position)
    construct.yaw=normalise_yaw(state.yaw)
    construct.native_velocity=vector.new(state.velocity or {x=0,y=0,z=0})
    construct.native_yaw_velocity=state.yaw_velocity or 0
    construct.bounds=compute_bounds(construct.nodes)
    return true
end

local function apply_drive_velocity(construct)
    local velocity={
        x=math.sin(construct.yaw)*(construct.forward_speed or 0),
        y=construct.vertical_speed or 0,
        z=math.cos(construct.yaw)*(construct.forward_speed or 0),
    }
    return core.set_dynamic_construct_velocity(
        construct.native_id,velocity,construct.yaw_rate or 0)
end

local function yaw_difference(a,b)
    return math.abs(((normalise_yaw(a)-normalise_yaw(b)+PI)%TWO_PI)-PI)
end

local function motion_changed(construct)
    local last=construct._last_sent_motion
    if not last then return true end
    local forward=construct.forward_speed or 0
    return math.abs(forward-(last.forward_speed or 0))>MOTION_EPSILON
        or math.abs((construct.vertical_speed or 0)-(last.vertical_speed or 0))>MOTION_EPSILON
        or math.abs((construct.yaw_rate or 0)-(last.yaw_rate or 0))>MOTION_EPSILON
        or (math.abs(forward)>MOTION_EPSILON and yaw_difference(construct.yaw,last.yaw or 0)>MOTION_EPSILON)
end

local function mark_motion_sent(construct)
    construct._last_sent_motion={
        forward_speed=construct.forward_speed or 0,
        vertical_speed=construct.vertical_speed or 0,
        yaw_rate=construct.yaw_rate or 0,
        yaw=construct.yaw or 0,
    }
end

local function send_drive_velocity(construct,force)
    if not force and not motion_changed(construct) then return true end
    local ok,error_message=apply_drive_velocity(construct)
    if ok then mark_motion_sent(construct) end
    return ok,error_message
end

local function restore_world_nodes(construct,quarter_turn)
    local placements={}
    for index,entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed then
            local target=vector.round(vector.add(construct.position,rotate_quarter(entry.local_pos,quarter_turn)))
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
        core.set_node(placement.pos,{name=entry.name,param1=entry.param1 or 0,param2=entry.param2 or 0})
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
            -- create_dynamic_construct converts these world positions back into
            -- construct-local coordinates before applying the supplied yaw.
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
    if owner_active[owner] then return nil,"you already have an active construct" end
    local ghost_ids=untracked_runtime_ids(native_id)
    if #ghost_ids>0 then
        core.remove_dynamic_construct(native_id)
        return nil,"native construct runtime has untracked ships; run /nc_purge_constructs before launching again"
    end
    local pivot=compute_pivot(scan_result)
    local construct={
        id="native-"..tostring(native_id),native_id=native_id,owner=owner,
        position=vector.new(pivot),yaw=0,forward_speed=0,vertical_speed=0,
        yaw_rate=0,turn_remaining=0,nodes={},profile=scan_result.navycraft_profile,
        systems=scan_result.navycraft_systems or {},
    }
    for _,node in ipairs(scan_result.nodes) do
        construct.nodes[#construct.nodes+1]={local_pos=vector.subtract(node.pos,pivot),name=node.name,param1=node.param1,param2=node.param2,metadata=node.metadata}
    end
    construct.bounds=compute_bounds(construct.nodes)
    for _,callback in ipairs(launch_hooks) do
        local ok,error_message=callback(construct,scan_result,player)
        if ok==false then core.remove_dynamic_construct(native_id);return nil,error_message or "launch hook rejected construct" end
    end
    local cleared,clear_error=clear_source_nodes(scan_result)
    if not cleared then core.remove_dynamic_construct(native_id);return nil,clear_error end
    constructs[construct.id]=construct;owner_active[owner]=construct.id;save_all()
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
        local local_pos=vector.round(world_to_local(construct,rounded))
        local index=M.find_node_index(construct,local_pos)
        if index then
            local exact=world_position(construct,construct.nodes[index].local_pos)
            if vector.distance(exact,rounded)<0.65 then
                return construct,index
            end
        end
    end
end

function M.sync_native_dig(native_id,local_pos)
    local construct=constructs["native-"..tostring(native_id)]
    local index=M.find_node_index(construct,local_pos)
    if not index then return false end
    construct.nodes[index].destroyed=true;construct.bounds=compute_bounds(construct.nodes);save_all();return true
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
    construct.bounds=compute_bounds(construct.nodes);save_all();return true
end

function M.world_position(construct,local_pos) return world_position(construct,local_pos) end
function M.world_to_local(construct,world_pos) return world_to_local(construct,world_pos) end
function M.player_support(_) return nil end
function M.clear_passenger(_) end
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
        construct.systems.set_speed=0;construct.systems.throttle=0;construct.systems.rudder=0;construct.systems.turn_progress=0;construct.systems.turn_elapsed=0
    end
    construct.forward_speed=0;construct.vertical_speed=0;construct.yaw_rate=0;construct.turn_remaining=0
    local ok,error_message=send_drive_velocity(construct,true);save_all();return ok~=nil and ok or false,error_message
end

function M.status(owner)
    local construct=M.get_for_owner(owner);if not construct then return nil end
    refresh_native(construct)
    return string.format("%s: %d nodes, position %s, yaw %.1f°, speed %.2f, queued turn %.1f°, spin %.1f°/s, rise %.2f",
        construct.id,active_node_count(construct),core.pos_to_string(construct.position),math.deg(construct.yaw),construct.forward_speed or 0,
        math.deg(construct.turn_remaining or 0),math.deg(construct.yaw_rate or 0),construct.vertical_speed or 0)
end

function M.dock(owner)
    local construct=M.get_for_owner(owner);if not construct then return false,"no active construct" end
    local refreshed,error_message=refresh_native(construct);if not refreshed then return false,error_message end
    local quarter=math.floor(construct.yaw/HALF_PI+0.5)%4;local snapped=quarter*HALF_PI
    local difference=math.abs(((construct.yaw-snapped+PI)%TWO_PI)-PI)
    if difference>math.rad(5) then return false,"align the craft within 5 degrees of a cardinal direction" end
    construct.yaw=snapped
    for _,callback in ipairs(dock_hooks) do local ok,err=callback(construct);if ok==false then return false,err or "docking hook rejected construct" end end
    local ok,err=restore_world_nodes(construct,quarter);if not ok then return false,err end
    core.remove_dynamic_construct(construct.native_id);constructs[construct.id]=nil;owner_active[owner]=nil;save_all();return true
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
    send_drive_velocity(construct,true);save_all();return true
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
    core.remove_dynamic_construct(construct.native_id);constructs[construct.id]=nil;if owner_active[construct.owner]==construct.id then owner_active[construct.owner]=nil end;save_all();return true
end

function M.purge_all(reason)
    reason=reason or "manual_native_purge"
    for _,construct in pairs(constructs) do
        for _,callback in ipairs(remove_hooks) do
            local ok,err=pcall(callback,construct,reason,clean_construct(construct))
            if not ok then core.log("error","[NavyCraft] purge hook failed: "..tostring(err)) end
        end
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
    local removed=0
    for _,local_pos in ipairs(positions) do
        local index=M.find_node_index(construct,local_pos)
        if index and core.set_dynamic_construct_node(construct.native_id,local_pos,nil) then construct.nodes[index].destroyed=true;removed=removed+1 end
    end
    if removed>0 then construct.bounds=compute_bounds(construct.nodes);save_all() end
    return removed
end

function M.damage_radius(position,radius,force,attacker,damage_type)
    radius=math.max(0.1,radius or 1);force=math.max(0.1,force or 1);local results={}
    for _,construct in pairs(constructs) do
        refresh_native(construct);local positions={}
        for _,entry in ipairs(construct.nodes or {}) do if not entry.destroyed then
            local d=vector.distance(world_position(construct,entry.local_pos),position)
            if d<=radius then local def=core.registered_nodes[entry.name];local groups=def and def.groups or {};local armour=math.max(0.25,groups.navycraft_armour or groups.cracky or 1);local effective=force*(1-d/(radius+0.001));if effective>=armour*0.65 then positions[#positions+1]=entry.local_pos end end
        end end
        local removed=remove_native_nodes(construct,positions)
        if removed>0 then
            local alive=active_node_count(construct);if construct.profile then construct.profile.block_count_alive=alive end
            if construct.systems then local start=math.max(1,construct.profile and construct.profile.block_count or #construct.nodes);construct.systems.hull_integrity=alive/start;construct.systems.flooding=(construct.systems.flooding or 0)+removed*0.75;construct.systems.last_attacker=attacker;construct.systems.last_damage_type=damage_type or "explosion" end
            for _,callback in ipairs(damage_hooks) do callback(construct,removed,position,attacker,damage_type) end
            results[#results+1]={id=construct.id,removed=removed,alive=alive}
        end
    end
    save_all();return results
end

function M.apply_native_damage(native_id,node_damage,position,attacker,damage_type)
    local construct=constructs["native-"..tostring(native_id)];if not construct then return nil,"native construct not found" end
    local positions={};for _,damage in ipairs(node_damage or {}) do if damage.destroyed and damage.node_pos then positions[#positions+1]=damage.node_pos end end
    local removed=0
    for _,local_pos in ipairs(positions) do local index=M.find_node_index(construct,local_pos);if index then construct.nodes[index].destroyed=true;removed=removed+1 end end
    if removed==0 then return construct,0 end
    construct.bounds=compute_bounds(construct.nodes);local alive=active_node_count(construct);if construct.profile then construct.profile.block_count_alive=alive end
    if construct.systems then local start=math.max(1,construct.profile and construct.profile.block_count or #construct.nodes);construct.systems.hull_integrity=alive/start;construct.systems.last_attacker=attacker;construct.systems.last_damage_type=damage_type or "native_projectile" end
    for _,callback in ipairs(damage_hooks) do callback(construct,removed,position or construct.position,attacker,damage_type) end
    save_all();return construct,removed
end

function M.spawn_snapshot(owner,snapshot,position,yaw)
    if owner_active[owner] then return nil,"owner already has an active construct" end
    if not snapshot or type(snapshot.nodes)~="table" then return nil,"invalid snapshot" end
    local native_id,error_message=create_native(owner,snapshot.nodes,vector.new(position),normalise_yaw(yaw or 0));if not native_id then return nil,error_message end
    local construct={id="native-"..tostring(native_id),native_id=native_id,owner=owner,position=vector.new(position),yaw=normalise_yaw(yaw or 0),forward_speed=0,vertical_speed=0,yaw_rate=0,turn_remaining=0,nodes=copy(snapshot.nodes),profile=copy(snapshot.profile or {}),systems=copy(snapshot.systems or {})}
    construct.bounds=compute_bounds(construct.nodes);constructs[construct.id]=construct;owner_active[owner]=construct.id;save_all();return construct.id
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
            local state
            if native_id and native_id~=0 then
                state=core.get_dynamic_construct(native_id,false)
            end
            if not state then
                local error_message
                native_id,error_message=create_native(saved.owner,saved.nodes,vector.new(saved.position),normalise_yaw(saved.yaw or 0))
                if not native_id then
                    core.log("error","[NavyCraft] native construct restore failed: "..tostring(error_message))
                else
                    state=core.get_dynamic_construct(native_id,false)
                end
            end
            if native_id then
                saved.native_id=native_id;saved.id="native-"..tostring(native_id);saved.position=vector.new(state and state.position or saved.position);saved.yaw=normalise_yaw(state and state.yaw or saved.yaw or 0);saved.bounds=compute_bounds(saved.nodes);saved.forward_speed=saved.forward_speed or 0;saved.vertical_speed=saved.vertical_speed or 0;saved.yaw_rate=saved.yaw_rate or 0;saved.turn_remaining=saved.turn_remaining or 0
                constructs[saved.id]=saved;owner_active[saved.owner]=saved.id;send_drive_velocity(saved,true)
            end
        end
    end
    save_all()
end

core.register_globalstep(function(dtime)
    step_accumulator=step_accumulator+dtime;if step_accumulator<0.05 then return end
    local dt=step_accumulator;step_accumulator=0
    for id,construct in pairs(constructs) do
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
            send_drive_velocity(construct,false)
        end
    end
end)

core.register_on_mods_loaded(function() core.after(0,restore_saved_constructs) end)
core.register_on_shutdown(save_all)
return M

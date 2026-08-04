-- Native articulated machinery and turret bridge for NavyCraft M4M.
-- Generic joints drive turrets, scanning radars, doors and other moving parts.
local M = {constructs = {}, accumulator = 0}

local function protocol_version()
    if type(core.get_dynamic_construct_protocol_version) ~= "function" then return 0 end
    local ok, value = pcall(core.get_dynamic_construct_protocol_version)
    return ok and tonumber(value) or 0
end

function M.native_available()
    return protocol_version() >= 10 and
        type(core.configure_dynamic_construct_joint) == "function" and
        type(core.configure_dynamic_construct_turret) == "function" and
        type(core.aim_dynamic_construct_turret) == "function" and
        type(core.step_dynamic_construct_articulations) == "function" and
        type(core.dynamic_construct_line_of_fire) == "function"
end

local function add(a,b)
    return {x=(a.x or 0)+(b.x or 0),y=(a.y or 0)+(b.y or 0),z=(a.z or 0)+(b.z or 0)}
end

local function state_key(construct)
    return tostring(construct.native_id or construct.id)
end

local function weapon_definition(weapon_id)
    return navycraft.definitions.weapons[tonumber(weapon_id)]
end

local function component_of(node)
    local def = core.registered_nodes and core.registered_nodes[node.name]
    return def and def._navycraft_component
end

local function configure_joint(construct, definition)
    local id, error_message = core.configure_dynamic_construct_joint(
        construct.native_id, definition)
    if not id then
        core.log("warning", "[NavyCraft] articulation rejected: " .. tostring(error_message))
    end
    return id
end

local function configure_turret(construct, definition)
    local status, error_message = core.configure_dynamic_construct_turret(
        construct.native_id, definition)
    if not status then
        core.log("warning", "[NavyCraft] turret rejected: " .. tostring(error_message))
        return nil
    end
    return status.id
end

function M.configure(construct)
    if not M.native_available() or not construct or not construct.native_id then return nil end
    local key = state_key(construct)
    local previous = M.constructs[key]
    local signature = tostring(construct.native_id) .. ":" .. tostring(construct.profile and construct.profile.block_count_alive or #construct.nodes)
    if previous and previous.signature == signature then return previous end
    if previous then
        for _,joint_id in ipairs(previous.joints or {}) do
            pcall(core.remove_dynamic_construct_joint,joint_id)
        end
    end

    local state = {signature=signature, construct_id=construct.id, native_id=construct.native_id,
        turrets={}, joints={}, scanners={}}
    for _, mount in ipairs(construct.profile and construct.profile.weapons or {}) do
        local node = construct.nodes and construct.nodes[mount.node_index]
        if node and not node.destroyed then
            local weapon = weapon_definition(mount.type)
            local pivot = node.local_pos
            local yaw_id = configure_joint(construct, {
                name = "weapon_yaw_" .. tostring(mount.node_index),
                kind = "revolute", control_mode = "position",
                pivot = pivot, axis = {x=0,y=1,z=0}, nodes = {pivot},
                minimum = -math.pi, maximum = math.pi,
                maximum_speed = weapon and weapon.kind == "aa" and 3.5 or 1.6,
                maximum_acceleration = weapon and weapon.kind == "aa" and 9 or 4,
                enabled = true, render_enabled = true, collision_enabled = true,
            })
            if yaw_id then
                local turret_id = configure_turret(construct, {
                    name = "weapon_" .. tostring(mount.node_index),
                    yaw_joint_id = yaw_id,
                    muzzle_local = add(pivot,{x=0,y=.20,z=.75}),
                    forward_local = {x=0,y=0,z=1},
                    alignment_tolerance = math.rad(2.0),
                    projectile_radius = weapon and math.max(.03, tonumber(weapon.radius) or .05) or .05,
                    maximum_range = weapon and tonumber(weapon.range) or 500,
                    stabilised = true, enabled = true,
                })
                local entry={turret_id=turret_id,yaw_joint_id=yaw_id,node_index=mount.node_index,
                    weapon_id=mount.type,muzzle_local=add(pivot,{x=0,y=.20,z=.75})}
                state.turrets[#state.turrets+1]=entry
                state.joints[#state.joints+1]=yaw_id
            end
        end
    end

    -- Scanning radar dishes use the same generic revolute joint system.
    for index,node in ipairs(construct.nodes or {}) do
        if not node.destroyed and component_of(node)=="radar" then
            local joint_id=configure_joint(construct,{
                name="radar_scan_"..tostring(index),kind="revolute",control_mode="oscillate",
                pivot=node.local_pos,axis={x=0,y=1,z=0},nodes={node.local_pos},
                minimum=-math.pi,maximum=math.pi,maximum_speed=1.2,maximum_acceleration=4,
                enabled=construct.systems and construct.systems.radar_on or false,
                render_enabled=true,collision_enabled=false,
            })
            if joint_id then state.scanners[#state.scanners+1]={id=joint_id,enabled=construct.systems and construct.systems.radar_on or false};state.joints[#state.joints+1]=joint_id end
        end
    end
    M.constructs[key]=state
    construct.systems=construct.systems or {}
    construct.systems.articulation_count=#state.joints
    construct.systems.turret_count=#state.turrets
    return state
end

local function matching_turret(construct,weapon_id)
    local state=M.configure(construct)
    if not state then return nil end
    weapon_id=tonumber(weapon_id)
    for _,entry in ipairs(state.turrets) do
        if tonumber(entry.weapon_id)==weapon_id then return entry end
    end
    return state.turrets[1]
end

function M.prepare_fire(construct,weapon_id,options)
    if not M.native_available() then return true end
    local turret=matching_turret(construct,weapon_id)
    if not turret or not turret.turret_id then return true end
    options=options or {}
    local aim_point=options.aim_point
    if not aim_point and options.target_id then
        local target=navycraft.preview.get_by_id(options.target_id)
        aim_point=target and target.position
    end
    if not aim_point and options.launch_velocity then
        aim_point=add(construct.position,options.launch_velocity)
    end
    if not aim_point then return false,"no turret aim point" end
    local status,error_message=core.aim_dynamic_construct_turret(turret.turret_id,aim_point)
    if not status then return false,"turret aim failed: "..tostring(error_message) end
    local clear,obstruction,updated=core.dynamic_construct_line_of_fire(turret.turret_id,aim_point)
    status=updated or status
    if not clear or status.obstructed then
        local node=obstruction and obstruction.node_pos
        local where=node and string.format(" (%d,%d,%d)",node.x,node.y,node.z) or ""
        return false,"line of fire obstructed"..where
    end
    if not status.aligned then
        return false,string.format("turret slewing (error %.1f degrees)",math.deg(status.angle_error or 0))
    end
    return true,status
end

function M.park(construct)
    local state=M.configure(construct)
    if not state then return false,"native machinery unavailable" end
    for _,entry in ipairs(state.turrets) do
        pcall(core.clear_dynamic_construct_turret_target,entry.turret_id)
        pcall(core.set_dynamic_construct_joint,entry.yaw_joint_id,{target_position=0})
    end
    return true,"turrets parked"
end

function M.status(construct)
    if not construct then return "no vessel" end
    local state=M.configure(construct)
    if not state then return "machinery=fallback" end
    local all=core.get_dynamic_construct_articulations(construct.native_id) or {joints={},turrets={}}
    local aligned,blocked=0,0
    for _,turret in ipairs(all.turrets or {}) do
        if turret.aligned then aligned=aligned+1 end
        if turret.obstructed then blocked=blocked+1 end
    end
    return string.format("machinery=native joints=%d turrets=%d aligned=%d blocked=%d",
        #(all.joints or {}),#(all.turrets or {}),aligned,blocked)
end

function M.step(delta_seconds)
    if not M.native_available() then return end
    M.accumulator=M.accumulator+math.max(0,tonumber(delta_seconds) or 0)
    if M.accumulator<0.05 then return end
    local delta=M.accumulator;M.accumulator=0
    local active={}
    for _,construct in pairs(navycraft.preview.get_all()) do
        if construct.native_id then
            local key=state_key(construct);active[key]=true
            local state=M.configure(construct)
            if state and construct.systems then
                for _,scanner in ipairs(state.scanners) do
                    local enabled=construct.systems.radar_on==true
                    if scanner.enabled~=enabled then
                        pcall(core.set_dynamic_construct_joint,scanner.id,{enabled=enabled})
                        scanner.enabled=enabled
                    end
                end
            end
        end
    end
    for key in pairs(M.constructs) do if not active[key] then M.constructs[key]=nil end end
    local result,error_message=core.step_dynamic_construct_articulations(delta,
        type(core.get_us_time)=="function" and core.get_us_time()/1000000 or os.clock())
    if not result and error_message then
        core.log("error","[NavyCraft] articulation step failed: "..tostring(error_message))
    end
end

if M.native_available() then
    core.register_globalstep(M.step)
    core.log("action","[NavyCraft] native articulated machinery enabled")
end
return M

local D=navycraft.definitions
local S={}
local ballast_names={"closed","flood","blow","auto"}
local sonar_names={"off","passive","active","hf"}
local role_rank={passenger=1,crew=2,officer=3,captain=4,owner=5}

local function clamp(v,a,b) return math.max(a,math.min(b,v)) end
local function bool_text(v) return v and "ON" or "OFF" end
local function copy(v) return core.deserialize(core.serialize(v)) end
local function atan2(y,x) return math.atan2 and math.atan2(y,x) or math.atan(y,x) end
local function normalise_angle(a)
    while a>math.pi do a=a-math.pi*2 end
    while a<-math.pi do a=a+math.pi*2 end
    return a
end

local function water_level()
    if type(core.get_mapgen_setting)=="function" then
        local ok,value=pcall(core.get_mapgen_setting,"water_level")
        if ok and tonumber(value) then return tonumber(value) end
    end
    return 1
end

local function meta_fields(serialized)
    if not serialized or serialized=="" then return {} end
    local decoded=core.deserialize(serialized)
    return type(decoded)=="table" and (decoded.fields or {}) or {}
end

local function node_weight(name)
    local def=core.registered_nodes[name]
    local groups=def and def.groups or {}
    local encoded=groups.navycraft_weight or 0
    if encoded>0 then return encoded/100 end
    if (groups.navycraft_engine or 0)>0 then return 1 end
    if (groups.navycraft_component or 0)>0 then return .5 end
    return .25
end

local function component_key(name)
    local def=core.registered_nodes[name]
    return def and def._navycraft_component or nil
end

local function engine_key(name)
    local def=core.registered_nodes[name]
    return def and def._navycraft_engine_key or nil
end

local function weapon_type(name)
    local def=core.registered_nodes[name]
    return def and def._navycraft_weapon_type or nil
end

local function enclosed_air_count(scan_result)
    if not scan_result.minp or not scan_result.maxp then return 0 end
    local minp,maxp=scan_result.minp,scan_result.maxp
    local sx,sy,sz=maxp.x-minp.x+1,maxp.y-minp.y+1,maxp.z-minp.z+1
    local volume=sx*sy*sz
    if volume<=0 or volume>250000 then return 0 end
    local occupied={}
    for _,node in ipairs(scan_result.nodes or {}) do occupied[node.pos.x..":"..node.pos.y..":"..node.pos.z]=true end
    local outside,queue={},{}
    local function add(x,y,z)
        if x<minp.x or x>maxp.x or y<minp.y or y>maxp.y or z<minp.z or z>maxp.z then return end
        local key=x..":"..y..":"..z
        if occupied[key] or outside[key] then return end
        outside[key]=true;queue[#queue+1]={x=x,y=y,z=z}
    end
    for x=minp.x,maxp.x do for y=minp.y,maxp.y do add(x,y,minp.z);add(x,y,maxp.z) end end
    for x=minp.x,maxp.x do for z=minp.z,maxp.z do add(x,minp.y,z);add(x,maxp.y,z) end end
    for y=minp.y,maxp.y do for z=minp.z,maxp.z do add(minp.x,y,z);add(maxp.x,y,z) end end
    local cursor=1;local offsets={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}
    while cursor<=#queue do local p=queue[cursor];cursor=cursor+1;for _,o in ipairs(offsets) do add(p.x+o[1],p.y+o[2],p.z+o[3]) end end
    local enclosed=0
    for x=minp.x,maxp.x do for y=minp.y,maxp.y do for z=minp.z,maxp.z do local key=x..":"..y..":"..z;if not occupied[key] and not outside[key] then enclosed=enclosed+1 end end end end
    return enclosed
end

function S.analyse(scan_result,craft_type_name)
    local craft_type=D.craft_types[craft_type_name]
    if not craft_type then return nil,"unknown craft type "..tostring(craft_type_name) end
    local profile={craft_type=craft_type_name,block_count=#scan_result.nodes,block_count_alive=#scan_result.nodes,
        weight=0,components={},engines={},weapons={},ballast_count=0,pump_count=0,
        source="NavyCraft 1.1.1-iizDevBuild"}
    for index,node in ipairs(scan_result.nodes) do
        profile.weight=profile.weight+node_weight(node.name)
        local component=component_key(node.name)
        if component then
            profile.components[component]=(profile.components[component] or 0)+1
            if component=="ballast" then profile.ballast_count=profile.ballast_count+1 end
            if component=="pump" then profile.pump_count=profile.pump_count+1 end
        end
        local key=engine_key(node.name)
        if key and D.engines[key] then
            local fields=meta_fields(node.metadata)
            profile.engines[#profile.engines+1]={node_index=index,key=key,enabled=fields.enabled=="true"}
        end
        local wt=weapon_type(node.name)
        if wt~=nil and D.weapons[wt] then profile.weapons[#profile.weapons+1]={node_index=index,type=wt} end
    end
    profile.weight=math.max(profile.weight,1)
    profile.block_disp_value=D.source.block_disp_value
    profile.air_disp_value=D.source.air_disp_value
    profile.min_disp_value=D.source.minimum_disp_value
    profile.weight_multiplier=D.source.weight_multiplier
    profile.block_displacement=profile.block_count*profile.block_disp_value
    profile.enclosed_air_blocks=enclosed_air_count(scan_result)
    profile.air_displacement=profile.enclosed_air_blocks*profile.air_disp_value
    profile.full_ballast_displacement=profile.ballast_count*profile.air_disp_value
    profile.displacement=profile.block_displacement+profile.full_ballast_displacement
    if profile.block_count<craft_type.min_blocks then
        return nil,string.format("%s requires at least %d blocks; detected %d",craft_type_name,craft_type.min_blocks,profile.block_count)
    end
    if profile.block_count>craft_type.max_blocks then
        return nil,string.format("%s allows at most %d blocks; detected %d",craft_type_name,craft_type.max_blocks,profile.block_count)
    end
    if (profile.components.helm or 0)<1 then return nil,"craft requires a helm" end
    return profile
end

local function default_ammo()
    return {cannon_shell=0,fireball_shell=0,torpedo_mk1=0,torpedo_mk2=0,torpedo_mk3=0,depth_charge=0,bomb=0,aa_round=0}
end

local function default_systems(profile,owner)
    local engines={}
    for _,engine in ipairs(profile.engines or {}) do
        engines[tostring(engine.node_index)]={key=engine.key,set_on=engine.enabled,is_on=false}
    end
    local tubes={}
    for _,mount in ipairs(profile.weapons or {}) do
        local weapon=D.weapons[mount.type]
        if weapon and weapon.kind=="torpedo" then
            tubes[tostring(mount.node_index)]={weapon_type=mount.type,armed=false,auto=false,heading=0,depth=0,rudder=0,display=true}
        end
    end
    return {
        owner=owner,captain=owner,driver=owner,crew={[owner]="owner"},crew_history={[owner]=true},
        boarding="open",abandoned=false,captain_abandoned=false,taking_over=nil,takeover_started=0,
        release_at=0,remote_control=false,last_teleport=0,
        throttle=0,set_speed=0,gear=1,rudder=0,turn_progress=0,turn_elapsed=0,vertical_planes=0,docking_mode=true,
        submerged_mode=false,ballast_mode=0,ballast_air_percent=100,
        buoyancy=0,displacement=profile.displacement,last_displacement=profile.displacement,
        block_displacement=profile.block_displacement,air_displacement=profile.air_displacement,
        engines=engines,engines_on=false,top_speed=0,target_forward_speed=0,
        periscope_raised=false,periscope_destroyed=false,radar_on=false,sonar_mode="off",do_ping=false,
        sonar_target_id=nil,sonar_target_index=-1,launcher_on=false,radio_on=true,
        radio_channels={0,0,0,0},radio_selector=1,hyperdrive=false,in_hyperspace=false,
        flooding=0,flooded_volume=0,breach_count=0,block_damage=0,
        hull_integrity=1,helm_destroyed=false,sinking=false,sunk=false,scuttle_at=0,
        pump_damage_count=0,
        frozen=false,
        launch_settle_until=0,
        pump_on=true,pump_charge=(profile.pump_count or 0)*D.source.pump_charge_limit,
        ammo=default_ammo(),selected_weapon=0,weapon_range=10,weapon_depth=0,
        fire_control_mode="manual",fire_control_arc="low",fire_control_battery_id=nil,
        tdc_mode="straight",
        tubes=tubes,tube_heading=0,tube_rudder=0,tube_auto=false,tube_display=true,tube_fire_mode=0,
        target_id=nil,last_weapon_fire=0,last_damage_type=nil,last_attacker=nil,damagers={},
        waypoints={},current_waypoint=1,autotravel=false,route_id="",route_stage=0,route_loop=true,is_auto_craft=false,
        navigation_mode="manual",navigation_hold_position=nil,formation_leader_id=nil,
        formation_offset={x=0,y=0,z=-10},navigation_status="idle",navigation_avoiding=false,
        navigation_recovering=false,navigation_distance=0,
        stop_requested=false,custom_name=nil,sink_value=0,free_spawn=false,discount_spawn=false,
        repair_snapshot=nil,stored_name=nil,
    }
end

local function compatible(craft_type,engine)
    if craft_type.terrestrial then return engine.class=="tank" end
    if craft_type.can_fly then return engine.class=="aircraft" end
    if craft_type.can_dive then return engine.key=="nuclear" or engine.class=="ship" or engine.class=="sub" end
    if craft_type.can_navigate then return engine.class=="ship" or engine.class=="sub" end
    return false
end

local function engine_operates(state,engine,systems)
    if not state.set_on then return false end
    if not systems.submerged_mode then return true end
    if engine.key=="nuclear" or engine.key=="motor_1" then return true end
    if engine.key=="diesel_1" or engine.key=="diesel_2" or engine.key=="diesel_3" then return true end
    return false
end

local function calculate_engine_speed(construct)
    local p,s=construct.profile,construct.systems
    local craft_type=D.craft_types[p.craft_type]
    local total,any=0,false
    for _,state in pairs(s.engines or {}) do
        local engine=D.engines[state.key]
        if engine and compatible(craft_type,engine) then
            state.is_on=engine_operates(state,engine,s)
            if state.is_on then
                any=true
                local effective_weight=math.max(s.operating_mass or p.weight,engine.power)
                local contribution=engine.max_speed*engine.power*engine.power/(effective_weight*effective_weight)
                if s.submerged_mode and engine.key~="nuclear" and engine.key~="motor_1" then contribution=contribution/2 end
                total=total+contribution
            end
        else state.is_on=false end
    end
    s.engines_on=any
    local cap=s.submerged_mode and craft_type.max_submerged_speed or (craft_type.max_surface_speed or craft_type.max_speed)
    local equipment=s.equipment_modifiers or{}
    local maintenance=s.maintenance_speed_mult or 1
    local overload=s.overloaded and .75 or 1
    s.top_speed=math.min(total,craft_type.max_speed,cap)*(equipment.speed_mult or 1)*maintenance*overload
    return s.top_speed
end

local function movement_interval(craft_type,gear)
    local absolute=math.abs(tonumber(gear) or 0)
    if craft_type.can_fly and absolute>=3 then return 2 end
    if absolute>=3 then return 2.5 end
    if absolute==2 then return 5 end
    return 8
end

local function approach(current,target,amount)
    if current<target then return math.min(target,current+amount) end
    if current>target then return math.max(target,current-amount) end
    return target
end

local function vessel_mass(construct)
    local p,s=construct.profile or{},construct.systems or{}
    return math.max(1,(tonumber(s.operating_mass)or tonumber(p.weight)or 1)*
        (tonumber(p.weight_multiplier)or 1))
end

local function momentum_rate(construct,craft_type,current,target)
    local mass=vessel_mass(construct)
    local top=math.max(.5,math.abs(tonumber(construct.systems.top_speed)or 0),
        tonumber(craft_type.max_speed)or 1)
    local mass_root=math.sqrt(mass)
    local accel=top/math.max(7,7+mass_root*1.6)
    local decel=top/math.max(4.5,4.5+mass_root)
    if craft_type.can_fly then
        accel=accel*1.8;decel=decel*1.5
    elseif craft_type.terrestrial then
        accel=accel*1.25;decel=decel*1.35
    elseif craft_type.can_dive then
        accel=accel*.85;decel=decel*.9
    elseif craft_type.can_navigate then
        accel=accel*.75;decel=decel*.85
    end
    if current~=0 and target~=0 and current*target<0 then return decel*1.15 end
    if math.abs(target)<math.abs(current) then return decel end
    return accel
end

local function apply_forward_momentum(construct,craft_type,target_speed,dt)
    local current=tonumber(construct.forward_speed)or 0
    local target=tonumber(target_speed)or 0
    local rate=momentum_rate(construct,craft_type,current,target)
    local step=math.max(0,tonumber(dt)or 0)*rate
    if current~=0 and target~=0 and current*target<0 then
        target=0
    end
    local next_speed=approach(current,target,step)
    if math.abs(next_speed)<0.01 and math.abs(target)<0.01 then next_speed=0 end
    construct.forward_speed=next_speed
    construct.systems.target_forward_speed=target_speed
    return next_speed
end

local function rudder_yaw_sign(construct)
    local facing=((construct.systems or{}).active_helm_local_facing)or{x=0,z=1}
    local fx,fz=tonumber(facing.x)or 0,tonumber(facing.z)or 1
    if math.abs(fx)<.5 and math.abs(fz)<.5 then fx,fz=0,1 end
    local positive_yaw_delta={x=-fz,z=fx}
    local helm_right={x=fz,z=-fx}
    local dot=positive_yaw_delta.x*helm_right.x+positive_yaw_delta.z*helm_right.z
    return dot>=0 and 1 or -1
end

local function telegraph_text(craft_type,set_speed)
    if craft_type.can_fly then return "Throttle-"..tostring(set_speed*10).."%" end
    if craft_type.terrestrial then return "Throttle-"..tostring(set_speed*25).."%" end
    local names={[0]="All Stop",[1]="Engines Slow",[2]="Engines 1/3",[3]="Engines 2/3",
        [4]="Engines Standard",[5]="Engines Full",[6]="Engines Flank!"}
    return names[set_speed] or ("Engines "..tostring(set_speed))
end

local function update_damage_and_pumps(construct,dt)
    local p,s=construct.profile,construct.systems
    local breach_count=math.max(0,tonumber(s.breach_count) or 0)
    local displacement=math.max(1,tonumber(p.displacement) or tonumber(s.displacement) or 1)
    local hull_integrity=clamp(tonumber(s.hull_integrity) or 1,0,1)
    local flood_mult=((s.equipment_modifiers or {}).flooding_mult or 1)
    if breach_count>0 then
        local breach_pressure=math.max(0.35,1-hull_integrity)
        local inflow=breach_count*(0.18+breach_pressure*0.55)*flood_mult*dt
        if s.sinking then inflow=inflow*1.75 end
        s.flooding=math.min(displacement*1.35,(tonumber(s.flooding) or 0)+inflow)
        s.flooded_volume=math.max(tonumber(s.flooded_volume) or 0,s.flooding or 0)
    end
    local pump_count=math.max(0,(p.pump_count or 0)-(s.pump_damage_count or 0))
    if s.pump_on and pump_count>0 and (s.flooding or 0)>0 and (s.pump_charge or 0)>0 and not s.sunk then
        local equipment=s.equipment_modifiers or{}
        local removed=math.min(s.flooding,pump_count*.45*(equipment.pump_mult or 1)*(s.maintenance_system_mult or 1)*dt,s.pump_charge)
        s.flooding=s.flooding-removed
        s.pump_charge=s.pump_charge-removed
    end
    if s.scuttle_at>0 and os.time()>=s.scuttle_at then s.helm_destroyed=true;s.scuttle_at=0 end
    local flood_ratio=(s.flooding or 0)/displacement
    local critical=s.helm_destroyed or hull_integrity<.28 or flood_ratio>=0.75 or (breach_count>0 and hull_integrity<.45)
    if critical then s.sinking=true end
    if flood_ratio>=1.05 or hull_integrity<=0 then s.sunk=true;s.sinking=true end
    if not critical and s.sinking and not s.sunk and breach_count==0 and flood_ratio<0.25 and hull_integrity>0.5 then
        s.sinking=false
    end
    if s.sinking then
        s.throttle=0;s.set_speed=0;s.gear=0;s.hyperdrive=false
        s.rudder=0;s.turn_progress=0;s.turn_elapsed=0
        for _,state in pairs(s.engines or {}) do state.set_on=false;state.is_on=false end
        s.engines_on=false
    end
end

local function update_ballast(construct,dt)
    local p,s=construct.profile,construct.systems
    local rate=4*dt
    if s.ballast_mode==1 then s.ballast_air_percent=clamp(s.ballast_air_percent-rate,0,100)
    elseif s.ballast_mode==2 then s.ballast_air_percent=clamp(s.ballast_air_percent+rate,0,100)
    elseif s.ballast_mode==3 then
        local neutral=(s.operating_mass or p.weight)*p.weight_multiplier
        if s.displacement>neutral*1.05 then s.ballast_air_percent=clamp(s.ballast_air_percent-rate,0,100)
        elseif s.displacement<neutral*.95 then s.ballast_air_percent=clamp(s.ballast_air_percent+rate,0,100) end
    end
    s.last_displacement=s.displacement
    s.block_displacement=(p.block_count_alive or p.block_count)*p.block_disp_value
    s.displacement=s.block_displacement+p.air_displacement+
        p.full_ballast_displacement*(s.ballast_air_percent/100)-s.flooding
    local weight=(s.operating_mass or p.weight)*p.weight_multiplier
    if s.displacement<weight then
        if D.craft_types[p.craft_type].can_dive and s.displacement>=weight*.9 then s.buoyancy=0 else s.buoyancy=-1 end
    else
        if D.craft_types[p.craft_type].can_dive and s.displacement<=weight*1.1 then s.buoyancy=0 else s.buoyancy=1 end
    end
    if s.buoyancy==1 and s.displacement<s.last_displacement*.995 then s.buoyancy=0 end
end

local function update_autotravel(construct)
    if navycraft.navigation and navycraft.navigation.native_available and
            navycraft.navigation.native_available() and construct.native_id then return end
    local s=construct.systems
    if not s.autotravel or #s.waypoints==0 then return end
    local target=s.waypoints[s.current_waypoint]
    if not target then s.current_waypoint=1;target=s.waypoints[1] end
    local dx,dz=target.x-construct.position.x,target.z-construct.position.z
    local distance=math.sqrt(dx*dx+dz*dz)
    if distance<3 and math.abs(target.y-construct.position.y)<3 then
        s.current_waypoint=s.current_waypoint%#s.waypoints+1
        s.route_stage=s.current_waypoint
        return
    end
    local desired=atan2(dx,dz)
    local delta=normalise_angle(desired-construct.yaw)
    s.rudder=clamp(delta/.6,-1,1)
    s.throttle=distance<8 and .25 or .75
    if D.craft_types[construct.profile.craft_type].can_fly or D.craft_types[construct.profile.craft_type].can_dive then
        s.vertical_planes=clamp((target.y-construct.position.y)/8,-1,1)
    end
end

local function surface_float_vertical(construct)
    local p,s=construct.profile,construct.systems
    if (s.buoyancy or 0)<0 then return -0.5 end
    local bounds=construct.bounds
    if not bounds or not bounds.minp or not bounds.maxp then return 0 end
    local height=math.max(1,(bounds.maxp.y or 0)-(bounds.minp.y or 0)+1)
    local displacement=math.max(1,tonumber(s.displacement)or tonumber(p.displacement)or 1)
    local weight=math.max(0.1,(tonumber(s.operating_mass)or tonumber(p.weight)or 1)*(tonumber(p.weight_multiplier)or 1))
    local ratio=clamp(weight/displacement,0.04,0.95)
    local reserve=clamp(displacement/math.max(weight,0.1),1,8)
    local draft=clamp(height*(ratio^1.65),0.08,math.max(0.08,height-0.25))
    local surface=water_level()+0.5
    local visual_lift=reserve>=3 and 0.18 or 0
    local desired_y=surface-draft-(bounds.minp.y or 0)+visual_lift
    local error=desired_y-(construct.position.y or 0)
    if math.abs(error)<0.05 then return 0 end
    return clamp(error*0.9,-1.25,1.25)
end

function S.step(construct,dt)
    if not construct.profile or not construct.systems then return end
    local p,s=construct.profile,construct.systems
    local craft_type=D.craft_types[p.craft_type]
    if not craft_type then return end
    local settling=(tonumber(s.launch_settle_until)or 0)>os.time()
    update_damage_and_pumps(construct,dt)
    update_ballast(construct,dt)
    if (tonumber(construct._onboard_count) or 0)<=0 and not s.sinking then
        s.unmanned_slow_timer=(s.unmanned_slow_timer or 0)+dt
        if s.unmanned_slow_timer>=3 then
            s.unmanned_slow_timer=0
            local current=math.floor(tonumber(s.set_speed) or ((s.throttle or 0)*(craft_type.max_engine_speed or 1))+.5)
            if current>0 then
                current=current-1
                s.set_speed=current
                s.throttle=current/math.max(1,craft_type.max_engine_speed or 1)
                if current==0 then
                    s.rudder=0
                    s.turn_progress=0
                    s.turn_elapsed=0
                    for _,state in pairs(s.engines or {}) do state.set_on=false end
                end
            end
        end
    else
        s.unmanned_slow_timer=0
    end
    if s.frozen then
        s.autotravel=false
        s.throttle=0
        s.set_speed=0
        s.gear=0
        s.rudder=0
        s.turn_progress=0
        s.turn_elapsed=0
        s.vertical_planes=0
        s.engines_on=false
        construct.forward_speed=0
        s.target_forward_speed=0
        construct.vertical_speed=s.sinking and -math.max(.5,1+s.flooding/math.max(1,p.block_count)) or 0
        construct.yaw_rate=0
        if navycraft.effects then navycraft.effects.update(construct,dt) end
        return
    end
    update_autotravel(construct)
    if s.release_at>0 and os.time()>=s.release_at then
        s.driver=nil;s.captain_abandoned=true;s.abandoned=true;s.release_at=0
    end
    local top_speed=calculate_engine_speed(construct)
    local max_engine_speed=math.max(1,tonumber(craft_type.max_engine_speed) or 1)
    local set_speed=clamp(math.floor(tonumber(s.set_speed) or ((s.throttle or 0)*max_engine_speed)+0.5),0,max_engine_speed)
    s.set_speed=set_speed
    s.throttle=set_speed/max_engine_speed
    if set_speed==0 then
        s.rudder=0
        s.turn_progress=0
        s.turn_elapsed=0
    end
    local throttle=s.throttle
    local gear=clamp(math.floor(s.gear or 1),craft_type.max_reverse_gear,craft_type.max_forward_gear)
    local direction=gear<0 and -1 or (gear==0 and 0 or 1)
    local denominator=gear<0 and math.max(1,math.abs(craft_type.max_reverse_gear)) or math.max(1,craft_type.max_forward_gear)
    local gear_ratio=gear==0 and 0 or math.min(math.abs(gear),denominator)/denominator
    local target_speed=top_speed*throttle*gear_ratio*direction
    if s.hyperdrive or s.in_hyperspace then target_speed=target_speed*D.source.hyperspace_move_multiplier end
    if s.sinking and not settling then target_speed=0 end
    local speed=apply_forward_momentum(construct,craft_type,target_speed,dt)
    local can_turn=set_speed>0 and gear>0 and math.abs(speed)>0.001
    if not can_turn then
        construct.yaw_rate=0
    else
        local interval=movement_interval(craft_type,gear)
        local turn_radius=math.max(1,tonumber(craft_type.turn_radius) or 4)
        local degrees_per_second=90/(interval*turn_radius)
        construct.yaw_rate=math.rad(clamp((s.rudder or 0)*degrees_per_second*rudder_yaw_sign(construct),-45,45))
        if (s.turn_progress or 0)>0 then
            s.turn_elapsed=(s.turn_elapsed or 0)+dt
            while s.turn_elapsed>=interval and (s.turn_progress or 0)>0 do
                s.turn_elapsed=s.turn_elapsed-interval
                s.turn_progress=s.turn_progress-1
            end
            if (s.turn_progress or 0)<=0 then
                s.turn_progress=0
                s.turn_elapsed=0
                s.rudder=0
                construct.yaw_rate=0
            end
        else
            s.turn_elapsed=0
        end
    end
    local vertical=0
    if s.sinking and not settling then vertical=-math.max(.5,1+s.flooding/math.max(1,p.block_count))
    elseif craft_type.can_fly then
        if s.engines_on then vertical=(s.vertical_planes or 0)*math.max(1,math.abs(speed)*.25)
        elseif craft_type.obeys_gravity then vertical=-1 end
    elseif craft_type.can_dive then
        vertical=s.buoyancy+(s.vertical_planes or 0)*.5
        vertical=clamp(vertical,-2,2)
    elseif craft_type.can_navigate then vertical=surface_float_vertical(construct)
    elseif craft_type.terrestrial then vertical=0 end
    construct.vertical_speed=vertical
    if navycraft.effects then navycraft.effects.update(construct,dt) end
end

function S.launch_hook(construct,scan_result,player)
    if not construct.profile then return false,"missing NavyCraft profile" end
    construct.systems=default_systems(construct.profile,player:get_player_name())
    construct.systems.launch_settle_until=os.time()+3
    construct.systems.repair_snapshot={nodes=copy(construct.nodes),profile=copy(construct.profile)}
    return true
end

function S.damage_hook(construct,removed,position,attacker,damage_type,details)
    local s=construct.systems
    if not s then return end
    details=details or {}
    local score=math.max(tonumber(removed) or 0,(tonumber(details.total_damage) or 0)/20)
    s.damagers=s.damagers or {}
    if attacker and attacker~="" then s.damagers[attacker]=(s.damagers[attacker] or 0)+score end
    if attacker and attacker~="" and navycraft.campaign and navycraft.campaign.missions then
        navycraft.campaign.missions.record_damage(attacker,construct,score)
    end
    if (details.breaches or 0)>0 then
        local flood_mult=((s.equipment_modifiers or{}).flooding_mult or 1)
        s.flooding=(s.flooding or 0)+(details.breaches or 0)*flood_mult
        s.flooded_volume=math.max(tonumber(s.flooded_volume) or 0,s.flooding or 0)
    end
    if s.hull_integrity<.5 and not s.sinking then core.chat_send_player(construct.owner,"NavyCraft hull integrity below 50%") end
    if s.helm_destroyed then core.chat_send_player(construct.owner,"NavyCraft helm destroyed; vessel is sinking") end
    if navycraft.effects then navycraft.effects.damage(construct,position,math.max(removed or 0,details.damaged or 0)) end
end

function S.role(construct,name)
    local s=construct.systems
    if name==s.owner then return "owner" end
    if name==s.captain then return "captain" end
    return s.crew and s.crew[name] or nil
end

function S.authorized(construct,player,minimum)
    local s=construct.systems;if not s then return false end
    local name=type(player)=="string" and player or player:get_player_name()
    local role=S.role(construct,name)
    if minimum=="passenger" and s.boarding=="open" then return true end
    return role and role_rank[role]>=role_rank[minimum or "crew"] or false
end

function S.boarding_filter(construct,player)
    local s=construct.systems;if not s then return true end
    local name=player:get_player_name()
    if s.boarding=="open" then return true end
    if name==s.owner or name==s.captain then return true end
    if s.boarding=="crew" then return s.crew and s.crew[name]~=nil end
    return false
end

local function set_all_engines(c,on)
    for _,state in pairs(c.systems.engines or {}) do state.set_on=on and true or false end
end

local function moving(c)
    return math.abs(tonumber(c.forward_speed) or 0)>0.05 or
        math.abs(tonumber(c.vertical_speed) or 0)>0.05
end

local function locked(c)
    return c and c.systems and c.systems.frozen
end

local function locked_message()
    return false,"vessel is locked while another ship is active"
end

function S.freeze(c,reason)
    if not c or not c.systems then return false,"no active vessel" end
    local s=c.systems
    s.frozen=true
    s.abandoned=false
    s.captain_abandoned=false
    s.taking_over=nil
    s.takeover_started=0
    s.release_at=0
    s.remote_control=false
    s.autotravel=false
    s.throttle=0
    s.set_speed=0
    s.gear=0
    s.rudder=0
    s.turn_progress=0
    s.turn_elapsed=0
    s.vertical_planes=0
    s.engines_on=false
    s.target_forward_speed=0
    s.driver=nil
    c.forward_speed=0
    c.vertical_speed=0
    c.yaw_rate=0
    c.turn_remaining=0
    for _,state in pairs(s.engines or {}) do
        state.set_on=false
    end
    if navycraft and navycraft.preview and navycraft.preview.save then navycraft.preview.save() end
    return true,reason or "vessel frozen"
end

function S.take_helm(c,name)
    if not c or not c.systems then return false,"no active vessel" end
    if locked(c) then return locked_message() end
    if not S.authorized(c,name,"crew") then return false,"you are not on this vessel's crew" end
    c.systems.driver=name
    c.systems.abandoned=false
    c.systems.captain_abandoned=false
    return true,"You are driving "..c.id
end

function S.speed_change(c,increase)
    local s,t=c.systems,D.craft_types[c.profile.craft_type]
    if locked(c) then return locked_message() end
    if s.helm_destroyed then return false,"Helm Control or Engines Destroyed!" end
    if increase and (tonumber(s.gear) or 0)==0 then s.gear=1 end
    local total=0;for _ in pairs(s.engines or {}) do total=total+1 end
    if total==0 then
        s.set_speed=0;s.throttle=0;s.engines_on=false
        return false,"Error: No engines detected! Check engine signs."
    end
    local set_speed=math.floor(tonumber(s.set_speed) or ((s.throttle or 0)*(t.max_engine_speed or 1))+.5)
    if increase then
        set_speed=math.min(set_speed+1,t.max_engine_speed)
        if set_speed>=1 then set_all_engines(c,true) end
    else
        set_speed=set_speed-1
        if t.can_fly and set_speed==0 and ((s.gear or 1)>1 or math.abs(c.vertical_speed or 0)>0.05) then
            set_speed=1
            s.set_speed=set_speed;s.throttle=set_speed/math.max(1,t.max_engine_speed)
            return false,"Can't reduce speed to zero in this gear"
        end
        if set_speed<=0 then
            set_speed=0
            s.rudder=0;s.turn_progress=0;s.turn_elapsed=0
            set_all_engines(c,false)
        end
    end
    s.set_speed=set_speed
    s.throttle=set_speed/math.max(1,t.max_engine_speed)
    return true,set_speed==0 and "Stopping Engines..." or telegraph_text(t,set_speed)
end

function S.gear_change(c,increase)
    local s,t=c.systems,D.craft_types[c.profile.craft_type]
    if locked(c) then return locked_message() end
    if s.helm_destroyed then return false,"Helm Control or Engines Destroyed!" end
    local next_gear=math.floor(s.gear or 1)+(increase and 1 or -1)
    if next_gear==0 then next_gear=next_gear+(increase and 1 or -1) end
    next_gear=clamp(next_gear,t.max_reverse_gear,t.max_forward_gear)
    if next_gear>0 and (s.gear or 1)<0 and moving(c) then return false,"Stop moving before changing to forward gears." end
    if next_gear<0 and (s.gear or 1)>0 and moving(c) then return false,"Stop moving before changing to reverse gears." end
    if t.can_fly and next_gear==1 and ((math.abs(c.vertical_speed or 0)>0.05) or (s.set_speed or 0)~=1) then
        return false,"Must be on ground and engine at idle to shift into 1..."
    end
    s.gear=next_gear
    return true,"Set engines to Gear-("..tostring(s.gear)..")"
end

function S.set_throttle(c,v)
    local t=D.craft_types[c.profile.craft_type]
    if locked(c) then return locked_message() end
    local set_speed=clamp(math.floor(clamp(v,0,1)*(t.max_engine_speed or 1)+0.5),0,t.max_engine_speed)
    c.systems.set_speed=set_speed
    c.systems.throttle=set_speed/math.max(1,t.max_engine_speed)
    if set_speed>0 then
        if (tonumber(c.systems.gear) or 0)==0 then c.systems.gear=1 end
        set_all_engines(c,true)
    else
        set_all_engines(c,false);c.systems.rudder=0;c.systems.turn_progress=0;c.systems.turn_elapsed=0
    end
end
function S.set_gear(c,v) if locked(c) then return locked_message() end local t=D.craft_types[c.profile.craft_type];c.systems.gear=clamp(math.floor(v),t.max_reverse_gear,t.max_forward_gear) end
function S.rudder_order(c,order,turn)
    local s,t=c.systems,D.craft_types[c.profile.craft_type]
    if locked(c) then return locked_message() end
    order=order<0 and -1 or 1
    if s.helm_destroyed then return false,"Helm Control or Engines Destroyed!" end
    if (s.set_speed or 0)==0 or (s.gear or 1)<=0 then return false,"You have to be moving forward to turn." end
    if t.can_fly and (s.gear or 1)>1 and math.abs(c.vertical_speed or 0)<0.05 then return false,"You can't turn while taking off." end
    if s.rudder==0 or (s.rudder==order and turn and (s.turn_progress or 0)==0) then
        s.rudder=order
        if turn then
            s.turn_progress=t.turn_radius or 4
            s.turn_elapsed=0
            return true,order>0 and "Rudder Turning Right" or "Rudder Turning Left"
        end
        return true,order>0 and "Rudder Right" or "Rudder Left"
    elseif s.rudder==-order then
        if (s.turn_progress or 0)==0 or (s.turn_progress or 0)>(t.turn_radius or 4)/2 then
            s.rudder=0;s.turn_progress=0;s.turn_elapsed=0
            return true,"Rudder Centered"
        end
        return false,"Too late to cancel turn, please wait."
    end
    return false,"Rudder already set. Look other way to cancel."
end
function S.set_rudder(c,v)
    if locked(c) then return locked_message() end
    local value=clamp(v,-1,1)
    c.systems.rudder=value
    if value==0 then c.systems.turn_progress=0;c.systems.turn_elapsed=0 end
end
function S.set_planes(c,v) if locked(c) then return locked_message() end c.systems.vertical_planes=clamp(v,-1,1) end
function S.cycle_ballast(c) if locked(c) then return locked_message() end c.systems.ballast_mode=(c.systems.ballast_mode+1)%4;return ballast_names[c.systems.ballast_mode+1] end
function S.toggle_subdrive(c) if locked(c) then return locked_message() end c.systems.submerged_mode=not c.systems.submerged_mode;c.systems.vertical_planes=0;return c.systems.submerged_mode and "submerged/electric" or "surface/diesel" end
function S.toggle_engine(c,node_index) if locked(c) then return locked_message() end local state=c.systems.engines[tostring(node_index)];if not state then return nil end;state.set_on=not state.set_on;return state.set_on end
function S.cycle_sonar(c) if locked(c) then return locked_message() end local i=1;for n,v in ipairs(sonar_names) do if v==c.systems.sonar_mode then i=n end end;i=i%#sonar_names+1;c.systems.sonar_mode=sonar_names[i];return c.systems.sonar_mode end
function S.set_pump(c,on) if locked(c) then return locked_message() end c.systems.pump_on=on end

function S.add_crew(c,name,role)
    role=role or "crew";if not role_rank[role] or role=="owner" then return false,"invalid role" end
    c.systems.crew[name]=role;c.systems.crew_history[name]=true;return true
end
function S.remove_crew(c,name) c.systems.crew[name]=nil;if c.systems.captain==name then c.systems.captain=c.systems.owner end;return true end
function S.release(c,name)
    if name~=c.systems.captain and name~=c.systems.owner then return false,"only captain or owner may release" end
    if locked(c) then return locked_message() end
    c.systems.release_at=os.time()+D.source.craft_release_delay;c.systems.abandoned=true;return true
end
function S.takeover(c,name)
    if locked(c) then return locked_message() end
    if not c.systems.abandoned and not c.systems.captain_abandoned then return false,"vessel is not abandoned" end
    if c.systems.taking_over and c.systems.taking_over~=name then return false,"another takeover is in progress" end
    if not c.systems.taking_over then c.systems.taking_over=name;c.systems.takeover_started=os.time();return true,"takeover started" end
    if os.time()-c.systems.takeover_started<15 then return false,"takeover timer has not completed" end
    c.systems.captain=name;c.systems.driver=name;c.systems.crew[name]="captain";c.systems.taking_over=nil;c.systems.abandoned=false;c.systems.captain_abandoned=false
    return true,"takeover complete"
end
function S.scuttle(c,name)
    if name~=c.systems.captain and name~=c.systems.owner then return false,"you do not command this vessel" end
    if locked(c) then return locked_message() end
    if c.systems.sinking then return false,"vessel is already sinking" end
    c.systems.scuttle_at=os.time()+D.source.scuttle_delay;return true,"scuttle armed for 3 minutes"
end

local function bearing(from,to)
    local dx,dz=to.x-from.x,to.z-from.z
    local degrees=math.deg(atan2(dx,dz));if degrees<0 then degrees=degrees+360 end
    return degrees
end
function S.sensor_contacts(c,mode)
    local contacts={}
    for _,other in pairs(navycraft.preview.get_all()) do
        if other~=c and other.profile then
            local range=vector.distance(c.position,other.position)
            local sensor_mult=(((c.systems or{}).equipment_modifiers or{}).sensor_mult or 1)*((c.systems or{}).maintenance_system_mult or 1)
            local visible=false;local strength=0
            if mode=="radar" then visible=range<=500*sensor_mult and not (other.systems and other.systems.submerged_mode);strength=1-range/(500*sensor_mult)
            elseif mode=="detector" then visible=range<=150*sensor_mult;strength=1-range/(150*sensor_mult)
            elseif mode=="passive" then visible=range<=350*sensor_mult and other.systems and other.systems.engines_on;strength=1-range/(350*sensor_mult)
            elseif mode=="active" then visible=range<=500*sensor_mult;strength=1-range/(500*sensor_mult)
            elseif mode=="hf" then visible=range<=150*sensor_mult;strength=1-range/(150*sensor_mult) end
            if visible then contacts[#contacts+1]={id=other.id,type=other.profile.craft_type,range=range,bearing=bearing(c.position,other.position),strength=clamp(strength,0,1)} end
        end
    end
    table.sort(contacts,function(a,b) return a.range<b.range end)
    return contacts
end
function S.format_contacts(contacts)
    local out={};for _,v in ipairs(contacts) do out[#out+1]=string.format("%s %s %.0fm brg%03d str%.0f%%",v.id,v.type,v.range,v.bearing,v.strength*100) end
    return #out>0 and table.concat(out," | ") or "no contacts"
end
function S.select_target(c,index)
    local contacts=S.sensor_contacts(c,c.systems.sonar_mode~="off" and c.systems.sonar_mode or "radar")
    local target=contacts[index];if not target then return false,"target index not found" end
    c.systems.target_id=target.id;c.systems.sonar_target_id=target.id;c.systems.sonar_target_index=index;return true,target.id
end

function S.summary(c)
    if not c or not c.profile then return "no active NavyCraft vessel" end
    local p,s=c.profile,c.systems
    local running,total=0,0;for _,state in pairs(s.engines or {}) do total=total+1;if state.is_on then running=running+1 end end
    local condition=s.maintenance and math.floor((s.maintenance.condition or 1)*100+.5)or 100
    return string.format("%s/%s%s | blocks=%d/%d health=%d%% mass=%.1f cargo=%.1f/%.1f disp=%.1f buoy=%d flood=%.1f sinking=%s maint=%d%% | engines=%d/%d top=%.1f throttle=%d%% gear=%d rudder=%.1f | ballast=%s %.0f%% pumps=%s %.1f | sub=%s radar=%s sonar=%s launcher=%s route=%s:%d",
        p.craft_type,s.vessel_class or "unclassified",s.custom_name and ("/"..s.custom_name) or "",p.block_count_alive or p.block_count,p.block_count,math.floor((s.hull_integrity or 1)*100),
        s.operating_mass or p.weight,s.cargo_used or 0,s.cargo_capacity or 0,s.displacement,s.buoyancy,s.flooding,bool_text(s.sinking),condition,running,total,s.top_speed or 0,math.floor(s.throttle*100+.5),s.gear,s.rudder,
        ballast_names[s.ballast_mode+1],s.ballast_air_percent,bool_text(s.pump_on),s.pump_charge or 0,
        s.submerged_mode and "SUBMERGED" or "SURFACE",bool_text(s.radar_on),s.sonar_mode,bool_text(s.launcher_on),s.route_id or "",s.route_stage or 0)
end

return S

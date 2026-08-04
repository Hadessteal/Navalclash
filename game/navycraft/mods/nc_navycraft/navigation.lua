-- Native navigation bridge for Milestone 4K.
-- Source-derived concepts: Craft.WayPoints/currentWayPoint, routeID/routeStage,
-- isAutoCraft, stuckAutoTimer and the autocraft forward obstacle scan.
-- Continuous steering, braking, formation slots and velocity-obstacle avoidance
-- are Luanti engine adaptations for smoothly moving constructs.
local N={commands={},signatures={},last_step_frame=nil,avoidance_enabled={},last_cleanup=0}
local D=navycraft.definitions

local function protocol_version()
    if type(core.get_dynamic_construct_protocol_version)~="function" then return 0 end
    local ok,value=pcall(core.get_dynamic_construct_protocol_version)
    return ok and tonumber(value)or 0
end
function N.native_available()
    return protocol_version()>=8 and
        type(core.configure_dynamic_construct_navigation)=="function" and
        type(core.set_dynamic_construct_route)=="function" and
        type(core.step_dynamic_construct_navigation)=="function"
end

local function now_seconds()
    if type(core.get_us_time)=="function" then return core.get_us_time()/1000000 end
    return os.clock()
end
local function clamp(v,a,b)return math.max(a,math.min(b,v))end
local function bool(v)return v and "ON" or "OFF"end
local function domain(construct)
    local kind=construct.profile and construct.profile.craft_type or "ship"
    if kind=="aircraft"or kind=="airship"then return"air"end
    if kind=="submarine"then return"submersible"end
    if kind=="tank"then return"ground"end
    return"surface"
end
local function local_radius(construct)
    local minx,maxx,miny,maxy,minz,maxz
    for _,node in ipairs(construct.nodes or{})do if not node.destroyed then
        local p=node.local_pos
        minx=not minx and p.x or math.min(minx,p.x);maxx=not maxx and p.x or math.max(maxx,p.x)
        miny=not miny and p.y or math.min(miny,p.y);maxy=not maxy and p.y or math.max(maxy,p.y)
        minz=not minz and p.z or math.min(minz,p.z);maxz=not maxz and p.z or math.max(maxz,p.z)
    end end
    if not minx then return 2,1 end
    return math.max(1,(maxx-minx+1)/2,(maxz-minz+1)/2),math.max(1,(maxy-miny+1)/2)
end
local function game_construct(id)return navycraft.preview.get_by_id(id)end
local function native_id(id)local c=game_construct(id);return c and c.native_id or nil end
local function mode_of(construct)
    local s=construct.systems or{}
    if s.navigation_mode=="formation"then return"formation"end
    if s.navigation_mode=="hold"then return"hold"end
    if s.autotravel and #(s.waypoints or{})>0 then return"route"end
    return"manual"
end
local function route_signature(construct)
    local s=construct.systems or{};local parts={mode_of(construct),tostring(s.route_loop~=false)}
    for _,p in ipairs(s.waypoints or{})do
        parts[#parts+1]=string.format("%.3f,%.3f,%.3f",p.x or 0,p.y or 0,p.z or 0)
    end
    if s.navigation_mode=="formation"then
        local o=s.formation_offset or{x=0,y=0,z=0}
        parts[#parts+1]=tostring(s.formation_leader_id or"")
        parts[#parts+1]=string.format("%.3f,%.3f,%.3f",o.x or 0,o.y or 0,o.z or 0)
    elseif s.navigation_mode=="hold"then
        local p=s.navigation_hold_position or construct.position
        parts[#parts+1]=string.format("%.3f,%.3f,%.3f",p.x or 0,p.y or 0,p.z or 0)
    end
    parts[#parts+1]=string.format("%.3f",tonumber(s.top_speed)or 0)
    return table.concat(parts,"|")
end

local function configure(construct)
    if not N.native_available()or not construct.native_id or not construct.systems then return false end
    local s=construct.systems;local mode=mode_of(construct);local signature=route_signature(construct)
    if N.signatures[construct.id]==signature then return true end
    local craft_type=D.craft_types[construct.profile.craft_type]
    local top=math.max(.25,tonumber(s.top_speed)or 0,tonumber(craft_type.max_speed)or 1)
    local radius=local_radius(construct)
    local definition={
        mode=mode,domain=domain(construct),maximum_speed=top,
        maximum_reverse_speed=math.max(.5,math.min(top*.35,2)),
        maximum_acceleration=math.max(.5,top*.35),maximum_deceleration=math.max(.75,top*.5),
        maximum_yaw_rate=math.rad(math.min(45,90/math.max(1,craft_type.turn_radius))),
        maximum_yaw_acceleration=math.rad(45),maximum_vertical_speed=math.max(1,top*.25),
        arrival_radius=math.max(2,radius*.5),lookahead=math.max(12,radius*3),
        obstacle_margin=math.max(2,radius*.4),separation_distance=math.max(6,radius*2),
        avoidance_strength=N.avoidance_enabled[construct.id]==false and 0 or 1.8,
        stuck_timeout=8,recovery_seconds=3,allow_reverse=true,
        loop=s.route_loop~=false,apply_to_construct=false,
    }
    if mode=="formation"then
        definition.leader_id=native_id(s.formation_leader_id)
        definition.formation_offset=s.formation_offset or{x=0,y=0,z=-10}
        if not definition.leader_id then return false,"formation leader unavailable"end
    elseif mode=="hold"then definition.hold_position=s.navigation_hold_position or construct.position end
    local ok,err=core.configure_dynamic_construct_navigation(construct.native_id,definition)
    if not ok then return false,err end
    if mode=="route"then
        local route={}
        for i,p in ipairs(s.waypoints or{})do route[i]={position=p,arrival_radius=math.max(2,radius*.5),target_speed=top*(i==#s.waypoints and s.route_loop==false and .35 or .75),stop=i==#s.waypoints and s.route_loop==false}end
        local route_ok,route_err=core.set_dynamic_construct_route(construct.native_id,route,s.route_loop~=false)
        if not route_ok then return false,route_err end
    end
    N.signatures[construct.id]=signature
    return true
end

local function first_raycast_hit(start_pos,end_pos)
    if type(core.raycast)~="function"then return nil end
    local ok,ray=pcall(core.raycast,start_pos,end_pos,false,false);if not ok or not ray then return nil end
    if type(ray)=="function"then
        local ok2,pointed=pcall(ray);return ok2 and pointed or nil
    end
    if type(ray.next)=="function"then
        local ok2,pointed=pcall(ray.next,ray);return ok2 and pointed or nil
    end
    return nil
end
local function observe_terrain(construct,current_time)
    if N.avoidance_enabled[construct.id]==false or type(core.observe_dynamic_construct_obstacle)~="function"then return end
    local radius,height=local_radius(construct);local lookahead=math.max(16,radius*3)
    local forward={x=math.sin(construct.yaw),y=0,z=math.cos(construct.yaw)}
    local right={x=math.cos(construct.yaw),y=0,z=-math.sin(construct.yaw)}
    local offsets={-radius,0,radius};local verticals={0,math.min(height,3)}
    local ray_index=0
    for _,lateral in ipairs(offsets)do for _,vertical in ipairs(verticals)do
        ray_index=ray_index+1
        local start=vector.add(construct.position,vector.add(vector.multiply(right,lateral),{x=0,y=vertical,z=0}))
        local finish=vector.add(start,vector.multiply(forward,lookahead))
        local hit=first_raycast_hit(start,finish)
        if hit and hit.type=="node"then
            local position=hit.intersection_point or hit.under or hit.node_undersurface
            if position then pcall(core.observe_dynamic_construct_obstacle,construct.native_id,{
                id=ray_index,position=position,velocity={x=0,y=0,z=0},radius=math.max(1.5,radius*.25),
                sample_time=current_time,expires_at=current_time+.35,hard=true,
            })end
        end
    end end
end

local function cleanup_runtime_tables(current_time)
    if current_time-N.last_cleanup<10 then return end
    N.last_cleanup=current_time
    local active=navycraft.preview.get_all and navycraft.preview.get_all() or{}
    for id in pairs(N.signatures)do
        if not active[id]then N.signatures[id]=nil;N.avoidance_enabled[id]=nil end
    end
end

local function refresh_commands(dt,current_time)
    cleanup_runtime_tables(current_time)
    local frame=math.floor(current_time*20)
    if frame==N.last_step_frame then return end
    N.last_step_frame=frame;N.commands={}
    local commands,err=core.step_dynamic_construct_navigation(dt,current_time)
    if not commands then core.log("warning","[NavyCraft] native navigation step failed: "..tostring(err));return end
    for _,command in ipairs(commands)do N.commands[tostring(command.construct_id)]=command end
end

function N.step(construct,dt)
    if not N.native_available()or not construct.native_id or not construct.systems then return end
    local ok,err=configure(construct);if not ok and err then core.log("warning","[NavyCraft] navigation configure failed: "..tostring(err))end
    local current_time=now_seconds();observe_terrain(construct,current_time);refresh_commands(dt,current_time)
    local command=N.commands[tostring(construct.native_id)];if not command then return end
    local s=construct.systems
    s.navigation_status=command.reason;s.navigation_avoiding=command.avoiding;s.navigation_recovering=command.recovering
    s.navigation_distance=command.distance or 0;s.navigation_obstacle_id=command.obstacle_id
    if command.waypoint_index then s.current_waypoint=command.waypoint_index;s.route_stage=command.waypoint_index end
    construct.forward_speed=tonumber(command.forward_speed)or 0
    construct.yaw_rate=tonumber(command.yaw_rate)or 0
    construct.vertical_speed=tonumber(command.vertical_speed)or 0
    if command.completed then
        s.autotravel=false;s.navigation_mode="manual";s.throttle=0;construct.forward_speed=0;construct.vertical_speed=0
        N.signatures[construct.id]=nil
    end
end

function N.set_mode(construct,mode,options)
    options=options or{};mode=(mode or"manual"):lower();local s=construct.systems
    if mode=="off"then mode="manual"end
    if mode~="manual"and mode~="hold"and mode~="route"and mode~="formation"then return false,"navigation mode must be manual, hold, route or formation"end
    s.navigation_mode=mode
    if mode=="hold"then s.navigation_hold_position=options.position or vector.new(construct.position);s.autotravel=false
    elseif mode=="formation"then
        if not options.leader_id or not game_construct(options.leader_id)then return false,"formation leader not found"end
        s.formation_leader_id=options.leader_id;s.formation_offset=options.offset or{x=0,y=0,z=-10};s.autotravel=false
    elseif mode=="route"then if #(s.waypoints or{})==0 then return false,"route has no waypoints"end;s.autotravel=true
    else s.autotravel=false end
    N.signatures[construct.id]=nil;navycraft.preview.save();return true,"Navigation "..mode
end
function N.set_avoidance(construct,enabled)N.avoidance_enabled[construct.id]=enabled;N.signatures[construct.id]=nil;return true,"Obstacle avoidance "..bool(enabled)end
function N.status(construct)
    local s=construct.systems or{};local command=N.commands[tostring(construct.native_id)]or{}
    return string.format("mode=%s route=%s:%d/%d loop=%s speed=%.2f yaw_rate=%.1fdeg/s distance=%.1f avoiding=%s recovery=%s status=%s",
        mode_of(construct),s.route_id or"",s.current_waypoint or 1,#(s.waypoints or{}),bool(s.route_loop~=false),
        command.forward_speed or construct.forward_speed or 0,math.deg(command.yaw_rate or construct.yaw_rate or 0),
        command.distance or s.navigation_distance or 0,bool(command.avoiding or s.navigation_avoiding),
        bool(command.recovering or s.navigation_recovering),command.reason or s.navigation_status or"idle")
end
function N.clear(construct)
    if construct.native_id and type(core.clear_dynamic_construct_navigation)=="function"then pcall(core.clear_dynamic_construct_navigation,construct.native_id)end
    N.signatures[construct.id]=nil;N.commands[tostring(construct.native_id)]=nil
end
return N

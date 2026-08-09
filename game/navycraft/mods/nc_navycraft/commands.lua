local D=navycraft.definitions
local S=navycraft.systems
local W=navycraft.weapons
local F=navycraft.fire_control
local N=navycraft.navigation
local T=navycraft.structure
local M=navycraft.machinery
local L=navycraft.fluids
local C={}
local command_storage=core.get_mod_storage()
local sailor_kits={}
local function words(param)local t={};for v in (param or ""):gmatch("%S+") do t[#t+1]=v end;return t end
local function join(t,start)local out={};for i=start or 1,#t do out[#out+1]=t[i] end;return table.concat(out," ") end
local function clamp(v,a,b)return math.max(a,math.min(b,v))end
local function command_craft(name,minimum)
    local craft=navycraft.preview.get_for_owner(name)
    if craft and S.authorized(craft,name,minimum or "crew") then return craft end
    local player=core.get_player_by_name(name)
    if player then
        local nearest=navycraft.preview.find_nearest(player:get_pos(),24,function(c)return S.authorized(c,name,minimum or "crew")end)
        if nearest then return nearest end
    end
    return nil,"no NavyCraft vessel under your control"
end
local function commander(name)local c,e=command_craft(name,"captain");return c,e end
local function save()navycraft.preview.save()end
local function bool(v)return v and "ON" or "OFF"end
local function parse_on(value)return value and (value:lower()=="on" or value:lower()=="true" or value=="1")end


local function solved_fire(c,name,weapon_id)
    weapon_id=tonumber(weapon_id or c.systems.selected_weapon or 0)
    local solution,error_message=F.solve(c,weapon_id,c.systems.target_id)
    if not solution or not solution.valid then
        return false,error_message or (solution and solution.reason) or "no firing solution"
    end
    local ok,message=W.fire(c,name,weapon_id,{
        target_id=c.systems.target_id,
        launch_velocity=solution.launch_velocity,
        aim_point=solution.aim_point,
        yaw=solution.yaw,
        pitch=solution.pitch,
        fire_control=true,
    })
    if not ok then return false,message end
    return true,message.." | "..F.format_solution(solution)
end

local function set_engine(c,target,state)
    local enabled=parse_on(state)
    if not state or (state:lower()~="on" and state:lower()~="off") then return false,"use on or off" end
    if not target or target:lower()=="all" then for _,v in pairs(c.systems.engines)do v.set_on=enabled end
    else local v=c.systems.engines[tostring(target)];if not v then return false,"engine node index not found"end;v.set_on=enabled end
    save();return true,S.summary(c)
end

local function ship_help()
    return "info|drive|sail|pilot|dive|throttle|gear|rudder|planes|turn|park|dock|name|remote|release|takeover|crew|add|remove|store|select|recall|spawn|repair|addwaypoint|autotravel|route|navigation|structure|machinery|turret|fluids|engine|ballast|subdrive|sensors|target|firecontrol|tdc|solution|weapons|fire|reload|launcher|radio|hyperdrive|scuttle|destroy"
end

local function route_command(name,c,args)
    local sub=(args[2] or ""):lower()
    if sub=="create" then return navycraft.routes.create(name,args[3])
    elseif sub=="delete" then return navycraft.routes.delete(name,args[3])
    elseif sub=="list" then local out={};for _,r in ipairs(navycraft.routes.list(name))do out[#out+1]=navycraft.routes.format(r)end;return true,#out>0 and table.concat(out," | ")or"no routes"
    elseif sub=="add" then local player=core.get_player_by_name(name);return navycraft.routes.add_waypoint(name,args[3],player:get_pos())
    elseif sub=="remove" then return navycraft.routes.remove_waypoint(name,args[3],args[4])
    elseif sub=="bind" then if not c then return false,"no active vessel"end;return navycraft.routes.bind(c,name,args[3])
    elseif sub=="clear" then if not c then return false,"no active vessel"end;return navycraft.routes.unbind(c)
    elseif sub=="spawn" then
        local route=navycraft.routes.get(name,args[4]);if not route or not route.waypoints[1]then return false,"route not found or empty"end
        return navycraft.routes.spawn_auto(name,args[3],args[4],route.waypoints[1],0)
    end
    return false,"route create|delete|list|add|remove|bind|clear|spawn <stored> <route>"
end

local function ship_dispatch(name,param,forced_type)
    local args=words(param);local cmd=(args[1]or""):lower()
    local c=command_craft(name,"crew")
    if cmd==""or cmd=="help"then return true,ship_help()end
    if cmd=="info"or cmd=="update"then return c~=nil,c and S.summary(c)or"no active vessel"
    elseif cmd=="types"then local out={};for _,n in ipairs(D.craft_order)do local t=D.craft_types[n];out[#out+1]=string.format("%s %d-%d",n,t.min_blocks,t.max_blocks)end;return true,table.concat(out," | ")
    elseif cmd=="drive"or cmd=="sail"or cmd=="pilot"or cmd=="dive"then
        if not c then return false,"no active vessel"end
        local player=core.get_player_by_name(name)
        local ok,msg
        if player and navycraft.controls and navycraft.controls.take_helm then ok,msg=navycraft.controls.take_helm(c,player)
        else ok,msg=S.take_helm(c,name) end
        save();return ok,msg
    elseif cmd=="throttle"or cmd=="setspeed"then if not c then return false,"no active vessel"end;local v=tonumber(args[2]);if not v then return false,"number required"end;S.set_throttle(c,v>1 and v/100 or v);save();return true,S.summary(c)
    elseif cmd=="gear"then if not c then return false,"no active vessel"end;S.set_gear(c,tonumber(args[2])or 0);save();return true,S.summary(c)
    elseif cmd=="rudder"then
        if not c then return false,"no active vessel"end
        local v=tonumber(args[2])or 0
        if v==0 then S.set_rudder(c,0);save();return true,S.summary(c) end
        local ok,msg=S.rudder_order(c,v<0 and -1 or 1,false);save();return ok,msg
    elseif cmd=="planes"or cmd=="lift"then if not c then return false,"no active vessel"end;S.set_planes(c,tonumber(args[2])or 0);save();return true,S.summary(c)
    elseif cmd=="turn"then
        if not c then return false,"no active vessel"end;local value=(args[2]or""):lower();local degrees=value=="right"and 90 or value=="left"and-90 or value=="around"and 180 or tonumber(value)
        if not degrees then return false,"turn right|left|around"end
        local ok,msg=S.rudder_order(c,degrees<0 and -1 or 1,true)
        if ok and math.abs(degrees)>=180 then c.systems.turn_progress=(D.craft_types[c.profile.craft_type].turn_radius or 4)*2 end
        save();return ok,msg
    elseif cmd=="park"or cmd=="neutral"then if not c then return false,"no active vessel"end;c.systems.throttle=0;c.systems.set_speed=0;c.systems.gear=0;c.systems.rudder=0;c.systems.turn_progress=0;c.systems.turn_elapsed=0;c.systems.vertical_planes=0;navycraft.preview.stop(c.owner);return true,"Vessel parked"
    elseif cmd=="dock"then local cc,e=commander(name);if not cc then return false,e end;return navycraft.preview.dock(cc.owner)
    elseif cmd=="name"or cmd=="setname"then local cc,e=commander(name);if not cc then return false,e end;cc.systems.custom_name=join(args,2):sub(1,32);save();return true,"Vessel named "..cc.systems.custom_name
    elseif cmd=="remote"then if not c then return false,"no active vessel"end;c.systems.remote_control=not c.systems.remote_control;save();return true,"Remote control "..bool(c.systems.remote_control)
    elseif cmd=="release"or cmd=="leave"then
        if navycraft.preview and navycraft.preview.release_helm then
            local ok,msg=navycraft.preview.release_helm(name)
            if ok then save();return true,msg or"Helm released" end
        end
        local cc,e=commander(name);if not cc then return false,e end;local ok,msg=S.release(cc,name);save();return ok,msg
    elseif cmd=="takeover"or cmd=="claim"then if not c then local p=core.get_player_by_name(name);c=navycraft.preview.find_nearest(p:get_pos(),12)end;if not c then return false,"no vessel nearby"end;local ok,msg=S.takeover(c,name);if ok and msg=="takeover complete"then c.owner=name;c.systems.owner=name end;save();return ok,msg
    elseif cmd=="crew"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"list"):lower()
        if sub=="list"then local out={};for member,role in pairs(c.systems.crew or{})do out[#out+1]=member..":"..role end;table.sort(out);return true,table.concat(out,", ")
        elseif sub=="add"then local cc,e=commander(name);if not cc then return false,e end;local ok,msg=S.add_crew(cc,args[3]or"",args[4]or"crew");save();return ok,msg or"crew added"
        elseif sub=="remove"then local cc,e=commander(name);if not cc then return false,e end;local ok,msg=S.remove_crew(cc,args[3]or"");save();return ok,msg or"crew removed"
        elseif sub=="chat"then return navycraft.radio.crew(c,name,join(args,3))end
        return false,"crew list|add <player> [role]|remove <player>|chat <message>"
    elseif cmd=="add"then local cc,e=commander(name);if not cc then return false,e end;local ok,msg=S.add_crew(cc,args[2]or"",args[3]or"crew");save();return ok,msg or"crew added"
    elseif cmd=="remove"then local cc,e=commander(name);if not cc then return false,e end;local ok,msg=S.remove_crew(cc,args[2]or"");save();return ok,msg or"crew removed"
    elseif cmd=="boarding"then local cc,e=commander(name);if not cc then return false,e end;local mode=(args[2]or""):lower();if mode~="open"and mode~="crew"and mode~="closed"then return false,"open|crew|closed"end;cc.systems.boarding=mode;save();return true,"Boarding "..mode
    elseif cmd=="store"then return navycraft.storage.store(name,join(args,2))
    elseif cmd=="select"then return navycraft.storage.select(name,join(args,2))
    elseif cmd=="recall"then return navycraft.storage.recall(core.get_player_by_name(name),join(args,2))
    elseif cmd=="spawn"then return navycraft.storage.spawn_for_player(core.get_player_by_name(name),join(args,2))
    elseif cmd=="repair"then if c and (args[2] or "")=="" then return navycraft.storage.repair_active(name) end;return navycraft.storage.repair(name,join(args,2))
    elseif cmd=="stored"then local out={};for _,r in ipairs(navycraft.storage.list(name))do out[#out+1]=r.name.."("..(r.snapshot.profile.craft_type or"?")..")"end;return true,#out>0 and table.concat(out," | ")or"no stored vehicles"
    elseif cmd=="addwaypoint"then
        if not c then return false,"no active vessel"end;local pos
        if args[2]and args[3]and args[4]then pos={x=tonumber(args[2]),y=tonumber(args[3]),z=tonumber(args[4])};if not pos.x or not pos.y or not pos.z then return false,"coordinates required"end
        else pos=core.get_player_by_name(name):get_pos()end
        c.systems.waypoints[#c.systems.waypoints+1]=vector.round(pos);save();return true,"Added waypoint "..#c.systems.waypoints
    elseif cmd=="autotravel"then
        if not c then return false,"no active vessel"end
        local enabled=not c.systems.autotravel
        if enabled and#c.systems.waypoints==0 then return false,"add a waypoint first"end
        c.systems.autotravel=enabled;c.systems.navigation_mode=enabled and"route"or"manual";save();return true,"Autotravel "..bool(enabled)
    elseif cmd=="route"then return route_command(name,c,args)
    elseif cmd=="navigation"or cmd=="nav"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"status"):lower()
        if sub=="status"then return true,N.status(c)
        elseif sub=="off"or sub=="manual"then return N.set_mode(c,"manual")
        elseif sub=="hold"then return N.set_mode(c,"hold",{position=vector.new(c.position)})
        elseif sub=="route"or sub=="resume"then return N.set_mode(c,"route")
        elseif sub=="loop"then local enabled=parse_on(args[3]);if not args[3]or((args[3]or""):lower()~="on"and(args[3]or""):lower()~="off")then return false,"navigation loop on|off"end;c.systems.route_loop=enabled;N.signatures[c.id]=nil;save();return true,"Route loop "..bool(enabled)
        elseif sub=="avoidance"then local enabled=parse_on(args[3]);if not args[3]or((args[3]or""):lower()~="on"and(args[3]or""):lower()~="off")then return false,"navigation avoidance on|off"end;return N.set_avoidance(c,enabled)
        elseif sub=="formation"then
            local leader=args[3];if not leader then return false,"navigation formation <craft-id> [x y z]"end
            local offset={x=tonumber(args[4])or 0,y=tonumber(args[5])or 0,z=tonumber(args[6])or-10}
            return N.set_mode(c,"formation",{leader_id=leader,offset=offset})
        end
        return false,"navigation status|hold|route|off|loop on|off|avoidance on|off|formation <craft-id> [x y z]"
    elseif cmd=="machinery"or cmd=="turret"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"status"):lower()
        if sub=="status"then return true,M.status(c)
        elseif sub=="park"then return M.park(c)
        end
        return false,"machinery status|park"
    elseif cmd=="fluids"or cmd=="liquids"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"status"):lower()
        if sub=="status"then return true,L.status(c)
        elseif sub=="pump"then local value=(args[3]or""):lower();if value~="on"and value~="off"then return false,"fluids pump on|off"end;c.systems.pump_on=value=="on";L.configure(c);save();return true,"Pumps "..(c.systems.pump_on and"ON"or"OFF")
        end
        return false,"fluids status|pump on|off"
    elseif cmd=="structure"or cmd=="structural"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"status"):lower()
        if sub=="status"then return true,T.status(c)
        elseif sub=="split"or sub=="check"then local cc,e=commander(name);if not cc then return false,e end;return T.force_split(cc)
        end
        return false,"structure status|split"
    elseif cmd=="engine"then if not c then return false,"no active vessel"end;return set_engine(c,args[2]or"all",args[3])
    elseif cmd=="ballast"then if not c then return false,"no active vessel"end;local map={closed=0,flood=1,blow=2,auto=3};local value=map[(args[2]or""):lower()];if value==nil then return false,"closed|flood|blow|auto"end;c.systems.ballast_mode=value;save();return true,S.summary(c)
    elseif cmd=="subdrive"then if not c then return false,"no active vessel"end;local value=(args[2]or""):lower();if value=="surface"then c.systems.submerged_mode=false elseif value=="submerged"then c.systems.submerged_mode=true else return false,"surface|submerged"end;save();return true,S.summary(c)
    elseif cmd=="buoy"or cmd=="buoyancy"then
        if not c then return false,"no active vessel"end
        local field=(args[2] or ""):lower();local value=tonumber(args[3])
        if field=="block" and value and value>=.01 and value<=100 then c.profile.block_disp_value=value
        elseif field=="air" and value and value>=.01 and value<=100 then c.profile.air_disp_value=value;c.profile.air_displacement=(c.profile.enclosed_air_blocks or 0)*value
        elseif field=="min" and value and value>=.01 and value<=100 then c.profile.min_disp_value=value
        elseif field=="weight" and value and value>=.01 and value<=100 then c.profile.weight_multiplier=value
        elseif tonumber(args[2]) then c.systems.ballast_air_percent=clamp(c.systems.ballast_air_percent+tonumber(args[2]),0,100)
        else return true,string.format("Block Displacement=%.2f Air Displacement=%.2f Minimum=%.2f Weight Multiplier=%.2f",c.profile.block_disp_value,c.profile.air_disp_value,c.profile.min_disp_value,c.profile.weight_multiplier) end
        save();return true,S.summary(c)
    elseif cmd=="pump"then if not c then return false,"no active vessel"end;c.systems.pump_on=parse_on(args[2]);save();return true,"Pumps "..bool(c.systems.pump_on)
    elseif cmd=="sensors"then
        if not c then return false,"no active vessel"end;local mode=(args[2]or(c.systems.sonar_mode~="off"and c.systems.sonar_mode or"radar")):lower();return true,S.format_contacts(S.sensor_contacts(c,mode))
    elseif cmd=="target"then if not c then return false,"no active vessel"end;local ok,msg=S.select_target(c,tonumber(args[2])or 1);save();return ok,msg
    elseif cmd=="firecontrol"or cmd=="fc"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"status"):lower()
        if sub=="status"then return true,F.status(c)
        elseif sub=="manual"or sub=="auto"or sub=="automatic"or sub=="defensive"then return F.set_mode(c,sub)
        elseif sub=="arc"then local arc=(args[3]or""):lower();if arc~="low"and arc~="high"then return false,"arc low|high"end;c.systems.fire_control_arc=arc;save();return true,"Fire-control arc "..arc:upper()
        elseif sub=="solution"then local solution,msg=F.solve(c,args[3]or c.systems.selected_weapon,c.systems.target_id);return solution~=nil,solution and F.format_solution(solution)or msg
        elseif sub=="fire"then return solved_fire(c,name,args[3])
        elseif sub=="tracks"then local tracks=type(core.get_dynamic_construct_fire_control_tracks)=="function"and core.get_dynamic_construct_fire_control_tracks(c.native_id)or{};return true,"tracks="..tostring(#(tracks or{}))
        end
        return false,"firecontrol status|manual|auto|defensive|arc <low|high>|solution [weapon]|fire [weapon]|tracks"
    elseif cmd=="tdc"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or"status"):lower()
        if sub=="status"then return true,string.format("tdc=%s heading=%.1f depth=%.1f armed=%s",c.systems.tdc_mode or"straight",c.systems.tube_heading or 0,c.systems.weapon_depth or 0,bool(c.systems.launcher_on))
        elseif sub=="mode"then return F.set_tdc_mode(c,args[3])
        elseif sub=="straight"or sub=="periscope"or sub=="auto"then return F.set_tdc_mode(c,sub)
        elseif sub=="heading"then c.systems.tube_heading=tonumber(args[3])or 0;save();return true,"TDC heading "..c.systems.tube_heading
        elseif sub=="depth"then local depth=tonumber(args[3]);if not depth or depth<0 or depth>60 then return false,"TDC depth must be 0-60"end;c.systems.weapon_depth=depth;save();return true,"TDC depth "..depth
        elseif sub=="solution"then local solution,msg=F.solve(c,args[3]or c.systems.selected_weapon,c.systems.target_id);return solution~=nil,solution and F.format_solution(solution)or msg
        elseif sub=="fire"then c.systems.launcher_on=true;return solved_fire(c,name,args[3]) end
        return false,"tdc status|straight|periscope|auto|heading <degrees>|depth <0-60>|solution [weapon]|fire [weapon]"
    elseif cmd=="solution"then if not c then return false,"no active vessel"end;local solution,msg=F.solve(c,args[2]or c.systems.selected_weapon,c.systems.target_id);return solution~=nil,solution and F.format_solution(solution)or msg
    elseif cmd=="range"then if not c then return false,"no active vessel"end;local value=tonumber(args[2]);if not value or value<10 or value>200 then return false,"range must be 10-200"end;c.systems.weapon_range=value;save();return true,"Cannon range "..value
    elseif cmd=="tube"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or""):lower()
        if sub=="arm"then c.systems.launcher_on=true
        elseif sub=="safe"then c.systems.launcher_on=false
        elseif sub=="auto"then c.systems.tube_auto=not c.systems.tube_auto
        elseif sub=="heading"then c.systems.tube_heading=tonumber(args[3])or 0
        elseif sub=="depth"then c.systems.weapon_depth=tonumber(args[3])or 0
        elseif sub=="rudder"then c.systems.tube_rudder=clamp(tonumber(args[3])or 0,-1,1)
        elseif sub=="display"then c.systems.tube_display=parse_on(args[3])
        elseif sub=="fire"then c.systems.launcher_on=true;return W.fire(c,name,args[3]or c.systems.selected_weapon,{relative_heading=c.systems.tube_heading,depth_y=c.systems.weapon_depth})
        else return true,string.format("tube armed=%s auto=%s heading=%.1f depth=%.1f rudder=%.1f display=%s",bool(c.systems.launcher_on),bool(c.systems.tube_auto),c.systems.tube_heading or 0,c.systems.weapon_depth or 0,c.systems.tube_rudder or 0,bool(c.systems.tube_display)) end
        save();return true,"Tube controls updated"
    elseif cmd=="periscope"then if not c then return false,"no active vessel"end;c.systems.periscope_raised=not c.systems.periscope_raised;save();return true,"Periscope "..(c.systems.periscope_raised and"UP"or"DOWN")
    elseif cmd=="weapons"or cmd=="cannons"then if not c then return false,"no active vessel"end;return true,W.list(c)
    elseif cmd=="weapon"then if not c then return false,"no active vessel"end;local ok,msg=W.set_weapon(c,args[2]);save();return ok,msg
    elseif cmd=="fire"then
        if not c then return false,"no active vessel"end
        if (args[2]or""):lower()=="solution"or(args[2]or""):lower()=="auto"then return solved_fire(c,name,args[3]) end
        return W.fire(c,name,args[2]or c.systems.selected_weapon,{relative_heading=tonumber(args[3])or 0})
    elseif cmd=="reload"then if not c then return false,"no active vessel"end;return W.reload_from_player(c,core.get_player_by_name(name),args[2])
    elseif cmd=="launcher"then if not c then return false,"no active vessel"end;c.systems.launcher_on=parse_on(args[2]);save();return true,"Launcher "..(c.systems.launcher_on and"ARMED"or"SAFE")
    elseif cmd=="radio"then
        if not c then return false,"no active vessel"end;local sub=(args[2]or""):lower()
        if sub=="on"or sub=="off"then c.systems.radio_on=sub=="on";save();return true,"Radio "..sub
        elseif sub=="tune"then return navycraft.radio.set_channel(c,args[3],args[4])
        elseif sub=="send"then return navycraft.radio.send(c,name,join(args,3))
        elseif sub=="cycle"then local slot,ch=navycraft.radio.cycle(c);save();return true,string.format("Radio %d channel %04d",slot,ch or 0)end
        return false,"radio on|off|tune <1-4> <channel>|cycle|send <message>"
    elseif cmd=="hyperdrive"or cmd=="hyperspace"or cmd=="warpdrive"then if not c then return false,"no active vessel"end;c.systems.hyperdrive=not c.systems.hyperdrive;save();return true,"Hyperdrive "..bool(c.systems.hyperdrive)
    elseif cmd=="summon"or cmd=="tp"then
        local cc,e=commander(name);if not cc then return false,e end;local now=os.time();if now-(cc.systems.last_teleport or 0)<D.source.ship_teleport_cooldown and not core.check_player_privs(name,{server=true})then return false,"teleport cooldown active"end
        cc.systems.last_teleport=now;return navycraft.preview.teleport(cc.id,vector.add(core.get_player_by_name(name):get_pos(),{x=0,y=3,z=0}),core.get_player_by_name(name):get_look_horizontal())
    elseif cmd=="scuttle"or cmd=="sink"then local cc,e=commander(name);if not cc then return false,e end;local ok,msg=S.scuttle(cc,name);save();return ok,msg
    elseif cmd=="disable"then
        local cc,e=commander(name);if not cc then return false,e end
        local plot=navycraft.shipyard and navycraft.shipyard.find_plot_at(cc.position)
        if plot then return navycraft.preview.dock(cc.owner) end
        local ok,msg=S.scuttle(cc,name);save();return ok,ok and "Vehicle disable timer armed: "..msg or msg
    elseif cmd=="destroy"then
        local cc,e=commander(name);if not cc then return false,e end
        local plot=navycraft.shipyard and navycraft.shipyard.find_plot_at(cc.position)
        if not plot and not core.check_player_privs(name,{server=true})then return false,"destroy is only permitted in a Shipyard/safe dock"end
        return navycraft.preview.remove(cc.id,false)
    elseif cmd=="move"then
        local cc,e=commander(name);if not cc then return false,e end;local dx,dy,dz=tonumber(args[2]),tonumber(args[3]),tonumber(args[4]);if not dx or not dy or not dz then return false,"move <dx> <dy> <dz>"end
        return navycraft.preview.teleport(cc.id,vector.add(cc.position,{x=dx,y=dy,z=dz}),cc.yaw)
    end
    return false,ship_help()
end

local function navycraft_dispatch(name,param)
    local args=words(param);local cmd=(args[1]or""):lower()
    if cmd==""or cmd=="help"then return true,"types|list|reload|debug|config|spawntimer|cleanup|listships|weapons|cannons|firecontrol|machinery|structure|destroyships|removeships|destroyauto|destroystuck|tpship|loadships"end
    if cmd=="types"then return ship_dispatch(name,"types")
    elseif cmd=="list"or cmd=="listships"or cmd=="loadships"then
        local out={};for id,c in pairs(navycraft.preview.get_all())do out[#out+1]=string.format("%s %s owner=%s route=%s:%s",id,c.profile and c.profile.craft_type or"?",c.systems and c.systems.owner or c.owner,c.systems and c.systems.route_id or"",c.systems and c.systems.route_stage or 0)end;table.sort(out);return true,#out>0 and table.concat(out," | ")or"no loaded vessels"
    elseif cmd=="reload"then save();return true,"NavyCraft state saved; Luanti mods require a server restart to reload code"
    elseif cmd=="debug"or cmd=="loglevel"then return true,"Debug command acknowledged; use Luanti debug.txt for runtime output"
    elseif cmd=="config"then return true,string.format("releaseDelay=%ds conversionTimer=%.1fs teleportCooldown=%ds scuttle=%ds pumpCharge=%d hyper=%dx",D.source.craft_release_delay,D.source.construct_conversion_delay or 1,D.source.ship_teleport_cooldown,D.source.scuttle_delay,D.source.pump_charge_limit,D.source.hyperspace_move_multiplier)
    elseif cmd=="spawntimer"then return true,"Stored vehicle spawn is immediate in this Luanti adaptation"
    elseif cmd=="weapons"or cmd=="cannons"then local out={};for _,id in ipairs(D.weapon_order)do out[#out+1]=id..":"..D.weapons[id].display end;return true,table.concat(out," | ")
    elseif cmd=="projectiles"then local native=navycraft.projectiles and navycraft.projectiles.native_available();local count=navycraft.projectiles and navycraft.projectiles.count()or 0;return true,string.format("projectile_engine=%s active=%d",native and "native" or "fallback",count)
    elseif cmd=="firecontrol"then local tracks=type(core.get_dynamic_construct_fire_control_tracks)=="function"and core.get_dynamic_construct_fire_control_tracks(nil)or{};local batteries=type(core.get_dynamic_construct_fire_control_batteries)=="function"and core.get_dynamic_construct_fire_control_batteries(nil)or{};return true,string.format("fire_control=%s tracks=%d batteries=%d",F.native_available()and"native"or"unavailable",#(tracks or{}),#(batteries or{}))
    elseif cmd=="navigation"then local states=type(core.get_dynamic_construct_navigation)=="function"and core.get_dynamic_construct_navigation(nil)or{};return true,string.format("navigation=%s vessels=%d",N.native_available()and"native"or"fallback",#(states or{}))
    elseif cmd=="machinery"then local states=type(core.get_dynamic_construct_articulations)=="function"and core.get_dynamic_construct_articulations(nil)or{joints={},turrets={}};return true,string.format("machinery=%s joints=%d turrets=%d",M.native_available()and"native"or"unavailable",#(states.joints or{}),#(states.turrets or{}))
    elseif cmd=="structure"then local states=type(core.get_dynamic_construct_structure)=="function"and core.get_dynamic_construct_structure(nil)or{};local fragments=0;for _,state in ipairs(states or{})do if state.role=="fragment"or state.role=="wreck"then fragments=fragments+1 end end;return true,string.format("structure=%s constructs=%d fragments=%d",T.native_available()and"native"or"unavailable",#(states or{}),fragments)
    elseif cmd=="cleanup"or cmd=="destroystuck"then local removed=0;for id,c in pairs(navycraft.preview.get_all())do if c.systems and c.systems.sinking and(c.position.y<-1000 or c.systems.hull_integrity<=0)then navycraft.preview.remove(id,false);removed=removed+1 end end;return true,"Removed "..removed.." unrecoverable vessel(s)"
    elseif cmd=="destroyauto"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local ids={};for id,c in pairs(navycraft.preview.get_all())do if c.systems and c.systems.is_auto_craft then ids[#ids+1]=id end end;for _,id in ipairs(ids)do navycraft.preview.remove(id,false)end;return true,"Destroyed "..#ids.." automatic craft(s)"
    elseif cmd=="destroyships"or cmd=="removeships"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local ids={};for id in pairs(navycraft.preview.get_all())do ids[#ids+1]=id end;for _,id in ipairs(ids)do navycraft.preview.remove(id,false)end;return true,"Removed "..#ids.." vessel(s)"
    elseif cmd=="tpship"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local c=navycraft.preview.get_by_id(args[2]);if not c then return false,"ship not found"end;core.get_player_by_name(name):set_pos(vector.add(c.position,{x=0,y=3,z=0}));return true,"Teleported"
    end
    return false,"unknown NavyCraft command"
end

core.register_chatcommand("ship",{params=ship_help(),description="NavyCraft vessel command",privs={interact=true},func=ship_dispatch})
core.register_chatcommand("aircraft",{params=ship_help(),description="NavyCraft aircraft command",privs={interact=true},func=function(n,p)return ship_dispatch(n,p,"aircraft")end})
core.register_chatcommand("submarine",{params=ship_help(),description="NavyCraft submarine command",privs={interact=true},func=function(n,p)return ship_dispatch(n,p,"submarine")end})
core.register_chatcommand("navycraft",{params="<admin command>",description="NavyCraft administration",func=navycraft_dispatch})
core.register_chatcommand("nc",{params=ship_help(),description="NavyCraft vessel shortcut",privs={interact=true},func=ship_dispatch})

-- Source shortcut commands.
core.register_chatcommand("park",{description="Park controlled vessel",privs={interact=true},func=function(n)return ship_dispatch(n,"park")end})
core.register_chatcommand("remote",{description="Toggle remote vessel control",privs={interact=true},func=function(n)return ship_dispatch(n,"remote")end})
core.register_chatcommand("crew",{params="<message>",description="Vessel crew chat",privs={interact=true},func=function(n,p)return ship_dispatch(n,"crew chat "..p)end})
core.register_chatcommand("radio",{params="<radio command>",description="Vessel radio",privs={interact=true},func=function(n,p)return ship_dispatch(n,"radio "..p)end})
core.register_chatcommand("ra",{params="<message>",description="Send on selected vessel radio",privs={interact=true},func=function(n,p)return ship_dispatch(n,"radio send "..p)end})
core.register_chatcommand("engine",{params="<0-100>",description="Set NavyCraft engine sound volume",privs={interact=true},func=function(n,p)local v=tonumber(p);if not v or v<0 or v>100 then return false,"volume must be 0-100"end;command_storage:set_int("engine_volume_"..n,math.floor(v));return true,"Engine volume set to "..math.floor(v).."%"end})
core.register_chatcommand("rank",{description="Show Shipyard rank",func=function(n)return true,navycraft.shipyard and navycraft.shipyard.player_status(n)or"Shipyard not loaded"end})
core.register_chatcommand("nc_projectiles",{description="Show native NavyCraft projectile status",func=function()local native=navycraft.projectiles and navycraft.projectiles.native_available();local count=navycraft.projectiles and navycraft.projectiles.count()or 0;return true,string.format("projectile_engine=%s active=%d",native and "native" or "fallback",count)end})
core.register_chatcommand("nc_firecontrol",{description="Show native NavyCraft fire-control status",func=function()local tracks=type(core.get_dynamic_construct_fire_control_tracks)=="function"and core.get_dynamic_construct_fire_control_tracks(nil)or{};local batteries=type(core.get_dynamic_construct_fire_control_batteries)=="function"and core.get_dynamic_construct_fire_control_batteries(nil)or{};return true,string.format("fire_control=%s tracks=%d batteries=%d",F.native_available()and"native"or"unavailable",#(tracks or{}),#(batteries or{}))end})
core.register_chatcommand("nc_navigation",{description="Show native NavyCraft navigation status",func=function()local states=type(core.get_dynamic_construct_navigation)=="function"and core.get_dynamic_construct_navigation(nil)or{};return true,string.format("navigation=%s vessels=%d",N.native_available()and"native"or"fallback",#(states or{}))end})
core.register_chatcommand("nc_structure",{description="Show native NavyCraft structural status",func=function()local states=type(core.get_dynamic_construct_structure)=="function"and core.get_dynamic_construct_structure(nil)or{};local fragments=0;for _,state in ipairs(states or{})do if state.role=="fragment"or state.role=="wreck"then fragments=fragments+1 end end;return true,string.format("structure=%s constructs=%d fragments=%d",T.native_available()and"native"or"unavailable",#(states or{}),fragments)end})
core.register_chatcommand("nc_machinery",{description="Show native NavyCraft articulated machinery status",func=function()local states=type(core.get_dynamic_construct_articulations)=="function"and core.get_dynamic_construct_articulations(nil)or{joints={},turrets={}};return true,string.format("machinery=%s joints=%d turrets=%d",M.native_available()and"native"or"unavailable",#(states.joints or{}),#(states.turrets or{}))end})
core.register_chatcommand("nc_fluids",{description="Show native NavyCraft construct-liquid status",func=function()local count=0;for _,c in pairs(navycraft.preview.get_all())do if c.native_id then count=count+1 end end;return true,string.format("liquids=%s constructs=%d",L.native_available()and"native"or"unavailable",count)end})

local function give_sailor_kit(name)
    if sailor_kits[name] then return false,"You only get one sailor kit per life" end
    local player=core.get_player_by_name(name);if not player then return false,"player unavailable" end
    sailor_kits[name]=true;local inv=player:get_inventory()
    inv:add_item("main","nc_navycraft:hull_wood 20");inv:add_item("main","nc_navycraft:hull_steel 20")
    inv:add_item("main","nc_navycraft:hull_glass 10");inv:add_item("main","nc_navycraft:universal_remote")
    inv:add_item("main","nc_navycraft:cannon_shell 16");inv:add_item("main","nc_navycraft:aa_round 32")
    return true,"Anchors Aweigh! Sailor kit issued"
end
core.register_chatcommand("sailor",{description="Receive the source-style one-per-life sailor kit",privs={interact=true},func=give_sailor_kit})
core.register_chatcommand("explode",{params="<1-100>",description="Admin NavyCraft explosion test",privs={server=true},func=function(n,p)return W.admin_explosion(core.get_player_by_name(n):get_pos(),tonumber(p),false,n)end})
core.register_chatcommand("explodesigns",{params="<1-100>",description="Admin explosion propagation debug markers",privs={server=true},func=function(n,p)return W.admin_explosion(core.get_player_by_name(n):get_pos(),tonumber(p),true,n)end})
core.register_chatcommand("sign",{params="undo",description="Undo the last paid NavyCraft control purchase",privs={interact=true},func=function(n,p)if p:lower()~="undo"then return false,"use /sign undo"end;return false,"No paid control-sign transaction is pending"end})
core.register_chatcommand("yard",{params="<shipyard command>",description="Shipyard shortcut",privs={interact=true},func=function(n,p)local def=core.registered_chatcommands.shipyard;return def and def.func(n,p)or false,"Shipyard not loaded"end})
core.register_chatcommand("movecraft",{params="<command>",description="NavyCraft administration alias",func=navycraft_dispatch})
core.register_chatcommand("periscope",{description="Toggle vessel periscope",privs={interact=true},func=function(n)return ship_dispatch(n,"periscope")end})
core.register_chatcommand("warpdrive",{description="Toggle vessel hyperdrive",privs={interact=true},func=function(n)return ship_dispatch(n,"warpdrive")end})
if core.register_on_respawnplayer then core.register_on_respawnplayer(function(player)sailor_kits[player:get_player_name()]=nil end) end

-- Compatibility nc_* commands from earlier milestones.
local aliases={nc_types="types",nc_systems="info",nc_throttle="throttle",nc_gear="gear",nc_rudder="rudder",nc_planes="planes",nc_ballast="ballast",nc_subdrive="subdrive",nc_engine="engine",nc_sensors="sensors",nc_boarding="boarding",nc_crew="crew",nc_fire="fire",nc_reload="reload",nc_weapons="weapons",nc_store="store",nc_recall="recall",nc_spawn="spawn"}
for command,subcommand in pairs(aliases)do core.register_chatcommand(command,{params="<args>",description="NavyCraft compatibility command",privs={interact=true},func=function(n,p)return ship_dispatch(n,subcommand..(p~=""and" "..p or""))end})end
return C

local D=navycraft.definitions
local S=navycraft.systems
local W=navycraft.weapons
local F=navycraft.fire_control
local N={}

local function tell(player,message)
    if player and player:is_player() then core.chat_send_player(player:get_player_name(),message) end
end
local function take_helm(construct,player,node_index)
    if not construct or not player then return false,"no active vessel" end
    if navycraft.controls and navycraft.controls.take_helm then
        return navycraft.controls.take_helm(construct,player,node_index)
    end
    return S.take_helm(construct,player:get_player_name())
end
local function convert_to_blocks(construct,player)
    if not construct or not player then return false,"no active vessel" end
    if not S.authorized(construct,player,"captain") then return false,"only the captain or owner may convert the vessel" end
    if not (navycraft.preview and navycraft.preview.request_convert_to_blocks) then return false,"vessel conversion unavailable" end
    return navycraft.preview.request_convert_to_blocks(construct,player:get_player_name())
end
local function nearest_active(player,range)
    local construct,distance=navycraft.preview.find_nearest(player:get_pos(),range or 16)
    return construct,distance
end
local function component_groups(weight,armour)
    return {cracky=2,navycraft_component=1,navycraft_hull=1,navycraft_weight=weight or 50,navycraft_armour=armour or 1}
end
local function register_component(key,description,color,extra)
    extra=extra or {}
    local definition={
        description=description,tiles={"nc_frame.png^[colorize:"..color..":160"},paramtype2="facedir",
        groups=component_groups(extra.weight,extra.armour),_navycraft_component=key,
        on_construct=function(pos) local meta=core.get_meta(pos);meta:set_string("component",key);meta:set_string("infotext",description) end,
        on_rightclick=extra.on_rightclick,
    }
    if extra.groups then for k,v in pairs(extra.groups) do definition.groups[k]=v end end
    core.register_node("nc_navycraft:"..key,definition)
end

register_component("nav","NavyCraft Navigation Control","#336699")
register_component("telegraph","NavyCraft Engine Telegraph","#3366aa")
register_component("rudder","NavyCraft Rudder Control","#335588")
register_component("planes","NavyCraft Dive Plane Control","#224477")
register_component("periscope","NavyCraft Periscope","#669999",{armour=2})
register_component("subdrive","NavyCraft Submarine Drive Selector","#224466")
register_component("ballast","NavyCraft Ballast Tanks","#335577",{weight=100})
register_component("buoyancy","NavyCraft Buoyancy Indicator","#4488aa")
register_component("firecontrol","NavyCraft Fire Control","#993333")
register_component("tdc","NavyCraft Torpedo Data Computer","#773344")
register_component("tube","NavyCraft Torpedo Tube Control","#663344")
register_component("target","NavyCraft Target Selector","#993366")
register_component("radar","NavyCraft Radar","#339966")
register_component("detector","NavyCraft Detector","#66aa66")
register_component("sonar","NavyCraft Sonar","#226688")
register_component("hydrophone","NavyCraft Hydrophone","#225577")
register_component("passive_sonar","NavyCraft Passive Sonar","#114477")
register_component("active_sonar","NavyCraft Active Sonar","#117799")
register_component("hf_sonar","NavyCraft High-Frequency Sonar","#22aacc")
register_component("aa_gun","NavyCraft AA Gun Control","#777777",{weight=100,armour=2})
register_component("launcher","NavyCraft Launcher Control","#884444")
register_component("radio","NavyCraft Radio","#aa8844")
register_component("pump","NavyCraft Pump","#5588aa",{weight=100})
register_component("hyperdrive","NavyCraft Hyperdrive Control","#8844aa",{weight=150})
register_component("searchlight","NavyCraft Searchlight","#dddd88")

core.register_node("nc_navycraft:helm",{
    description="NavyCraft Helm",tiles={"nc_helm_top.png","nc_helm_top.png","nc_helm_side.png","nc_helm_side.png","nc_helm_side.png","nc_helm_front.png"},
    paramtype2="facedir",groups={cracky=2,navycraft_component=1,navycraft_hull=1,navycraft_helm=1,navycraft_weight=50,navycraft_armour=2},_navycraft_component="helm",
    on_construct=function(pos) local meta=core.get_meta(pos);meta:set_string("craft_type","ship");meta:set_string("infotext","NavyCraft Helm [ship]") end,
    on_rightclick=function(pos,node,player)
        if not player or not player:is_player() then return end
        local meta=core.get_meta(pos)
        local active,index=navycraft.preview and navycraft.preview.find_at_world_node and navycraft.preview.find_at_world_node(pos)
        if active then
            if player:get_player_control().sneak then
                local ok,msg=convert_to_blocks(active,player)
                tell(player,msg or (ok and "Vessel conversion armed" or "Vessel conversion failed"))
                return
            end
            local ok,msg=take_helm(active,player,index)
            tell(player,msg or (ok and "Helm engaged" or "Helm control denied"))
            return
        end
        if player:get_player_control().sneak then
            local current=meta:get_string("craft_type");local found=1
            for i,v in ipairs(D.craft_order) do if v==current then found=i end end
            local next_type=D.craft_order[found%#D.craft_order+1]
            meta:set_string("craft_type",next_type);meta:set_string("infotext","NavyCraft Helm ["..next_type.."]");tell(player,"Helm craft type set to "..next_type);return
        end
        local result,error_message=navycraft.scan.connected_nodes(pos,{max_nodes=18000})
        if not result then tell(player,"NavyCraft scan failed: "..error_message);return end
        local craft_type=meta:get_string("craft_type");if craft_type=="" then craft_type="ship" end
        local profile,profile_error=S.analyse(result,craft_type)
        if not profile then tell(player,"NavyCraft validation failed: "..profile_error);return end
        result.navycraft_profile=profile
        local id,mode=navycraft.construct.launch(player,result)
        if not id then tell(player,"NavyCraft launch failed: "..mode);return end
        local construct=navycraft.preview and navycraft.preview.get_by_id and navycraft.preview.get_by_id(id)
        local helm_index
        if navycraft.preview and navycraft.preview.find_at_world_node then
            local active,index=navycraft.preview.find_at_world_node(pos)
            if active and active.id==id then construct=active;helm_index=index end
        end
        if construct then take_helm(construct,player,helm_index) end
        tell(player,"Launched "..craft_type.." "..id.." using "..mode)
    end,
})

local hull_materials={
    {"hull_wood","Wooden Hull","#8b5a2b",20,1},
    {"hull_steel","Steel Hull","#6f7b84",60,3},
    {"hull_armoured","Armoured Hull","#3f474d",110,6},
    {"hull_glass","Hull Glass","#77ccdd",35,1},
    {"lift_cell","Airship Lift Cell","#ddddbb",10,1},
}
for _,h in ipairs(hull_materials) do
    core.register_node("nc_navycraft:"..h[1],{
        description="NavyCraft "..h[2],tiles={"nc_frame.png^[colorize:"..h[3]..":155"},
        drawtype=h[1]=="hull_glass" and "glasslike" or "normal",paramtype="light",sunlight_propagates=h[1]=="hull_glass",
        groups={cracky=2,navycraft_hull=1,navycraft_weight=h[4],navycraft_armour=h[5],navycraft_lift=h[1]=="lift_cell" and 1 or 0},
    })
end

for _,key in ipairs(D.engine_order) do
    local engine=D.engines[key]
    core.register_node("nc_navycraft:engine_"..key,{
        description="NavyCraft Engine - "..engine.display,tiles={"nc_frame.png^[colorize:#664422:130"},paramtype2="facedir",
        groups={cracky=2,navycraft_component=1,navycraft_hull=1,navycraft_engine=1,navycraft_weight=100,navycraft_armour=2},
        _navycraft_component="engine",_navycraft_engine_key=key,
        on_construct=function(pos) local meta=core.get_meta(pos);meta:set_string("enabled","false");meta:set_string("infotext",engine.display.." [OFF]") end,
        on_rightclick=function(pos,node,player)
            local meta=core.get_meta(pos);local enabled=meta:get_string("enabled")=="true";enabled=not enabled
            meta:set_string("enabled",enabled and "true" or "false");meta:set_string("infotext",engine.display..(enabled and " [ON]" or " [OFF]"));tell(player,engine.display..(enabled and " enabled" or " disabled"))
        end,
    })
end

-- Source sign equivalents for vehicle selection/claim/recall/spawn.
local function storage_control(key,description,color,handler)
    core.register_node("nc_navycraft:"..key,{
        description=description,tiles={"nc_frame.png^[colorize:"..color..":170"},groups={cracky=2},
        on_construct=function(pos) core.get_meta(pos):set_string("infotext",description) end,
        on_rightclick=function(pos,node,player) local ok,msg=handler(player,pos);tell(player,msg or (ok and "Done" or "Failed")) end,
    })
end
storage_control("select_control","NavyCraft *Select* Control","#55aa55",function(player)
    local name=player:get_player_name();local list=navycraft.storage.list(name);if #list==0 then return false,"No stored vehicles" end
    local current=navycraft.storage.selected(name);local index=0;for i,r in ipairs(list) do if r.name==current then index=i end end
    local record=list[index%#list+1];return navycraft.storage.select(name,record.name)
end)
storage_control("claim_control","NavyCraft *Claim* Control","#aaaa55",function(player) return navycraft.storage.claim_active(player:get_player_name()) end)
storage_control("recall_control","NavyCraft *Recall* Control","#55aaaa",function(player) return navycraft.storage.recall(player) end)
storage_control("spawn_control","NavyCraft *Spawn* Control","#aa55aa",function(player,pos) return navycraft.storage.spawn_for_player(player,nil,vector.add(pos,{x=0,y=2,z=4}),0) end)

local function cycle_value(value,values)
    local index=1;for i,v in ipairs(values) do if v==value then index=i end end
    return values[index%#values+1]
end
local function set_telegraph(construct,value)
    S.set_throttle(construct,value)
    return "Engine telegraph "..math.floor((construct.systems.throttle or 0)*100).."%"
end
local function set_rudder_order(construct,value)
    if value==0 then S.set_rudder(construct,0);return true,"Rudder Centered" end
    return S.rudder_order(construct,value<0 and -1 or 1,false)
end

core.register_craftitem("nc_navycraft:universal_remote",{
    description="NavyCraft Universal Remote",inventory_image="nc_frame.png^[colorize:#ddaa33:190",
    on_use=function(itemstack,user)
        local c=navycraft.preview.get_for_owner(user:get_player_name())
        if not c then c=nearest_active(user,32) end
        if not c then tell(user,"No vessel in remote range");return itemstack end
        if not S.authorized(c,user,"crew") then tell(user,"Remote access denied");return itemstack end
        c.systems.remote_control=not c.systems.remote_control;navycraft.preview.save();tell(user,"Remote control "..(c.systems.remote_control and "ON" or "OFF"));return itemstack
    end,
})
core.register_node("nc_navycraft:explosion_debug",{
    description="NavyCraft Explosion Debug Marker",tiles={"nc_frame.png^[colorize:#ff44ff:190"},
    groups={dig_immediate=3,not_in_creative_inventory=1},drop="",
})

function N.handle_moving_interaction(construct,node_index,player,action)
    if not construct.profile or not construct.systems then return false end
    local entry=construct.nodes and construct.nodes[node_index];if not entry or entry.destroyed then return false end
    local def=core.registered_nodes[entry.name];if not def then return false end
    local component=def._navycraft_component
    if component=="helm" and action=="rightclick" and player and player:get_player_control().sneak then
        local ok,msg=convert_to_blocks(construct,player)
        tell(player,msg or (ok and "Vessel conversion armed" or "Vessel conversion failed"))
        return true
    end
    if action=="rightclick" and navycraft.preview and navycraft.preview.player_helm_construct
            and navycraft.preview.player_helm_construct(player)==construct
            and navycraft.controls and navycraft.controls.take_helm then
        navycraft.controls.take_helm(construct,player,node_index)
        return true
    end
    if not S.authorized(construct,player,"crew") then tell(player,"You are not on this vessel's crew.");return true end
    if def._navycraft_engine_key and action=="rightclick" then
        local enabled=S.toggle_engine(construct,node_index);tell(player,D.engines[def._navycraft_engine_key].display..(enabled and " set ON" or " set OFF"));navycraft.preview.save();return true
    end
    if def._navycraft_weapon_type~=nil and action=="rightclick" then local ok,msg=W.fire(construct,player,def._navycraft_weapon_type);tell(player,msg);return true end
    local s=construct.systems
    if component=="nav" then
        if action=="rightclick" then tell(player,set_telegraph(construct,cycle_value(s.throttle,{0,.25,.5,.75,1})))
        else local ok,msg=set_rudder_order(construct,cycle_value(s.rudder,{-1,0,1}));tell(player,msg) end
    elseif component=="telegraph" and action=="rightclick" then tell(player,set_telegraph(construct,cycle_value(s.throttle,{0,.25,.5,.75,1})))
    elseif component=="rudder" and action=="rightclick" then local ok,msg=set_rudder_order(construct,cycle_value(s.rudder,{-1,0,1}));tell(player,msg)
    elseif component=="planes" and action=="rightclick" then s.vertical_planes=cycle_value(s.vertical_planes,{-1,0,1});tell(player,"Planes "..s.vertical_planes)
    elseif component=="subdrive" and action=="rightclick" then tell(player,"Subdrive: "..S.toggle_subdrive(construct))
    elseif component=="ballast" and action=="rightclick" then tell(player,"Ballast mode: "..S.cycle_ballast(construct))
    elseif component=="periscope" and action=="rightclick" then s.periscope_raised=not s.periscope_raised;tell(player,"Periscope "..(s.periscope_raised and "UP" or "DOWN"))
    elseif component=="radar" and action=="rightclick" then s.radar_on=not s.radar_on;tell(player,"Radar "..(s.radar_on and "ON: "..S.format_contacts(S.sensor_contacts(construct,"radar")) or "OFF"))
    elseif component=="detector" and action=="rightclick" then tell(player,"Detector: "..S.format_contacts(S.sensor_contacts(construct,"detector")))
    elseif component=="hydrophone" and action=="rightclick" then s.sonar_mode="passive";tell(player,"Hydrophone: "..S.format_contacts(S.sensor_contacts(construct,"passive")))
    elseif component=="sonar" and action=="rightclick" then local mode=S.cycle_sonar(construct);tell(player,"Sonar "..mode..(mode~="off" and ": "..S.format_contacts(S.sensor_contacts(construct,mode)) or ""))
    elseif component=="passive_sonar" and action=="rightclick" then s.sonar_mode="passive";tell(player,"Passive sonar: "..S.format_contacts(S.sensor_contacts(construct,"passive")))
    elseif component=="active_sonar" and action=="rightclick" then s.sonar_mode="active";tell(player,"Active sonar: "..S.format_contacts(S.sensor_contacts(construct,"active")))
    elseif component=="hf_sonar" and action=="rightclick" then s.sonar_mode="hf";tell(player,"HF sonar: "..S.format_contacts(S.sensor_contacts(construct,"hf")))
    elseif component=="target" and action=="rightclick" then
        local contacts=S.sensor_contacts(construct,s.sonar_mode~="off" and s.sonar_mode or "radar");if #contacts==0 then tell(player,"No contacts") else local next_index=(s.sonar_target_index or 0)%#contacts+1;local ok,target=S.select_target(construct,next_index);tell(player,ok and "Target "..target or target) end
    elseif component=="firecontrol" then
        if action=="rightclick" then
            local next_id=(s.selected_weapon+1)%10;W.set_weapon(construct,next_id);tell(player,"Selected "..D.weapons[next_id].display)
        else
            local modes={manual="auto",auto="defensive",defensive="manual"};local ok,msg=F.set_mode(construct,modes[s.fire_control_mode or "manual"] or "manual");tell(player,msg)
        end
    elseif component=="tdc" then
        if action=="rightclick" then
            if (s.tdc_mode or "straight")=="auto" then
                local solution,msg=F.solve(construct,s.selected_weapon,s.target_id);tell(player,solution and F.format_solution(solution) or msg)
            else
                s.tube_heading=((s.tube_heading or 0)+15)%360;tell(player,"TDC relative heading "..s.tube_heading.." degrees")
            end
        else
            local modes={straight="periscope",periscope="auto",auto="straight"};local ok,msg=F.set_tdc_mode(construct,modes[s.tdc_mode or "straight"] or "straight");tell(player,msg)
        end
    elseif component=="tube" and action=="rightclick" then
        s.launcher_on=true;local ok,msg=W.fire(construct,player,s.selected_weapon,{relative_heading=s.tube_heading or 0,depth_y=s.weapon_depth});tell(player,msg)
    elseif component=="launcher" and action=="rightclick" then s.launcher_on=not s.launcher_on;tell(player,"Launcher "..(s.launcher_on and "ARMED" or "SAFE"))
    elseif component=="aa_gun" then
        if action=="rightclick" then local ok,msg=W.fire_aa(construct,player);tell(player,msg)
        else local ok,msg=F.set_mode(construct,(s.fire_control_mode=="defensive") and "manual" or "defensive");tell(player,msg) end
    elseif component=="radio" and action=="rightclick" then local slot,channel=navycraft.radio.cycle(construct);tell(player,string.format("Radio selector %d channel %04d",slot,channel or 0))
    elseif component=="pump" and action=="rightclick" then s.pump_on=not s.pump_on;tell(player,"Pumps "..(s.pump_on and "ON" or "OFF"))
    elseif component=="hyperdrive" and action=="rightclick" then s.hyperdrive=not s.hyperdrive;tell(player,"Hyperdrive "..(s.hyperdrive and "ENGAGED" or "DISENGAGED"))
    elseif component=="buoyancy" and action=="rightclick" then tell(player,S.summary(construct))
    elseif component=="helm" and action=="rightclick" then
        if navycraft.controls and navycraft.controls.take_helm then
            navycraft.controls.take_helm(construct,player,node_index)
        else
            S.take_helm(construct,player:get_player_name())
        end
        tell(player,S.summary(construct))
    else return false end
    navycraft.preview.save();return true
end
return N

local I={}
local sessions={}
local function esc(v)return core.formspec_escape(tostring(v or""))end
local function command(name,param)local def=core.registered_chatcommands.ship;if not def then return false,"ship command unavailable"end;return def.func(name,param)end
local function vessel(name)return navycraft.campaign.hud.vessel_for(name)end
local function button(x,y,w,label,field)return string.format("button[%g,%g;%g,.75;%s;%s]",x,y,w,field,esc(label))end
local function style(name)local s=navycraft.campaign.presentation.get(name);local p=navycraft.campaign.presentation.palette(name);local out="bgcolor["..p.background..";true]style_type[label;textcolor="..p.text.."]style_type[button;border=false]";if s.high_contrast then out=out.."style_type[button;bgcolor=#FFFFFFFF;textcolor=#000000FF]"end;return out end
local tabs={"Command","Helm","Engineering","Weapons","Sensors","Missions","Settings"}
local function current_tab(fields,session)local selected=tonumber((fields.tabs or""):match("(%d+)"));return selected or session.tab or 1 end
local function header(name,tab,title)return"formspec_version[6]size[14,9.5]"..style(name).."tabheader[.2,.1;13.6,.7;tabs;"..table.concat(tabs,",")..";"..tab..";false;false]label[.4,.95;"..esc(title).."]"end
local function command_page(name,c)
 local status=c and navycraft.systems.summary(c)or"No active vessel. Launch a vessel or join a crew first.";return"textarea[.4,1.4;13.2,3.2;status;;"..esc(status).."]textarea[.4,4.75;13.2,1.4;mission;;"..esc(navycraft.campaign.missions.status(name)).."]"..button(.4,6.4,2.4,"Refresh","refresh")..button(3,6.4,2.4,"Crew roster","crew")..button(5.6,6.4,2.4,"Tutorial hint","hint")..button(8.2,6.4,2.4,"Park vessel","park")
end
local function helm_page(c)
 local s=c and c.systems or{};return string.format("label[.4,1.4;Throttle: %d%%   Gear: %d   Rudder: %.1f]",math.floor((s.throttle or 0)*100),s.gear or 0,s.rudder or 0)..button(.4,2,2,"Stop","throttle_0")..button(2.6,2,2,"25%","throttle_25")..button(4.8,2,2,"50%","throttle_50")..button(7,2,2,"75%","throttle_75")..button(9.2,2,2,"Full","throttle_100")..button(.4,3,2.4,"Reverse","gear_reverse")..button(3,3,2.4,"Neutral","gear_neutral")..button(5.6,3,2.4,"Ahead","gear_forward")..button(.4,4,2.4,"Rudder left","rudder_left")..button(3,4,2.4,"Rudder centre","rudder_center")..button(5.6,4,2.4,"Rudder right","rudder_right")..button(.4,5,2.4,"Hold position","nav_hold")..button(3,5,2.4,"Resume route","nav_route")
end
local function engineering_page(c)
 local s=c and c.systems or{};return string.format("label[.4,1.4;Engines: %s   Pumps: %s   Charge: %.1f   Flood: %.1f   Ballast air: %.0f%%]",s.engines_on and"RUNNING"or"STOPPED",s.pump_on and"ON"or"OFF",s.pump_charge or 0,s.flooding or 0,s.ballast_air_percent or 100)..button(.4,2.2,2.6,"Toggle engines","engine_toggle")..button(3.2,2.2,2.6,"Toggle pumps","pump_toggle")..button(.4,3.2,2.6,"Fill ballast","ballast_fill")..button(3.2,3.2,2.6,"Blow ballast","ballast_blow")..button(6,3.2,2.6,"Auto ballast","ballast_auto")..button(.4,4.2,2.6,"Maintenance","maintenance")..button(3.2,4.2,2.6,"Fluid status","fluid_status")
end
local function weapons_page(c)
 local s=c and c.systems or{};return"label[.4,1.4;Fire control: "..esc(s.fire_control_mode or"manual").."   Target: "..esc(s.target_id or"none").."]"..button(.4,2.2,2.4,"Solution","solution")..button(3,2.2,2.4,"Fire","fire")..button(5.6,2.2,2.4,"Reload","reload")..button(.4,3.2,2.4,"Manual","fc_manual")..button(3,3.2,2.4,"Automatic","fc_auto")..button(5.6,3.2,2.4,"Defensive","fc_defensive")..button(.4,4.2,2.4,"Low arc","arc_low")..button(3,4.2,2.4,"High arc","arc_high")
end
local function sensors_page(c)
 local s=c and c.systems or{};return"label[.4,1.4;Radar: "..(s.radar_on and"ON"or"OFF").."   Sonar: "..esc(s.sonar_mode or"off").."   Target: "..esc(s.target_id or"none").."]"..button(.4,2.2,2.6,"Toggle radar","radar")..button(3.2,2.2,2.6,"Sonar passive","sonar_passive")..button(6,2.2,2.6,"Active ping","sonar_ping")..button(.4,3.2,2.6,"Sensor status","sensor_status")..button(3.2,3.2,2.6,"Clear target","clear_target")
end
local function missions_page(name)
 return"textarea[.4,1.4;13.2,2.1;career;;"..esc(navycraft.campaign.career.status(name)).."]textarea[.4,3.6;13.2,2.1;mission;;"..esc(navycraft.campaign.missions.status(name)).."]textarea[.4,5.8;13.2,1.3;tutorial;;"..esc(navycraft.campaign.tutorial.status(name)).."]"..button(.4,7.4,2.4,"Mission list","mission_list")..button(3,7.4,2.4,"Faction status","faction_status")..button(5.6,7.4,2.4,"Operation status","operation_status")
end
local function settings_page(name)
 local s=navycraft.campaign.presentation.get(name);return"label[.4,1.4;"..esc(navycraft.campaign.presentation.status(name)).."]"..button(.4,2.2,2.7,"HUD: "..s.hud_mode,"hud_cycle")..button(3.3,2.2,2.7,"High contrast","contrast")..button(6.2,2.2,2.7,"Large text","large_text")..button(.4,3.2,2.7,"Subtitles","subtitles")..button(3.3,3.2,2.7,"Reduced motion","motion")..button(6.2,3.2,2.7,"Colour mode","colour")..button(.4,4.2,2.7,"Alert level","alerts")..button(3.3,4.2,2.7,"Tutorial toggle","tutorial_toggle")..button(.4,5.5,2.7,"Reset tutorial","tutorial_reset")
end
local function form(name,tab)
 local c=vessel(name);local title=(c and((c.systems.custom_name or(c.profile and c.profile.craft_type)or"Vessel").." — ")or"")..tabs[tab]
 local body=tab==1 and command_page(name,c)or tab==2 and helm_page(c)or tab==3 and engineering_page(c)or tab==4 and weapons_page(c)or tab==5 and sensors_page(c)or tab==6 and missions_page(name)or settings_page(name)
 return header(name,tab,title)..body.."button_exit[11.2,8.55;2.4,.75;close;Close]"
end
function I.open(name,tab)
 local c=vessel(name);sessions[name]={tab=tonumber(tab)or 1,construct_id=c and c.id};navycraft.campaign.tutorial.mark(name,"station");if c then navycraft.campaign.tutorial.mark(name,"vessel")end;core.show_formspec(name,"nc_campaign:station",form(name,sessions[name].tab));return true
end
local actions={
 nav_hold="navigation hold",nav_route="navigation route",ballast_fill="ballast fill",ballast_blow="ballast blow",ballast_auto="ballast auto",fluid_status="fluids status",solution="solution",fire="fire",reload="reload",fc_manual="firecontrol manual",fc_auto="firecontrol auto",fc_defensive="firecontrol defensive",arc_low="firecontrol arc low",arc_high="firecontrol arc high",sensor_status="sensors",park="park",
}
local function cycle(value,list)local index=1;for i,v in ipairs(list)do if v==value then index=i%#list+1 end end;return list[index]end
core.register_on_player_receive_fields(function(player,formname,fields)
 if formname~="nc_campaign:station"then return end;local name=player:get_player_name();local session=sessions[name]or{tab=1};session.tab=current_tab(fields,session);sessions[name]=session
 local acted=false;local message
 local c=vessel(name)
 local helm_values={throttle_0={"throttle",0},throttle_25={"throttle",.25},throttle_50={"throttle",.5},throttle_75={"throttle",.75},throttle_100={"throttle",1},gear_reverse={"gear",-1},gear_neutral={"gear",0},gear_forward={"gear",1},rudder_left={"rudder",-1},rudder_center={"rudder",0},rudder_right={"rudder",1}}
 for field,pair in pairs(helm_values)do if fields[field]and c then
  if not navycraft.campaign.crew.can_operate(c,name,"helm")then message="Your assigned station cannot operate the helm";navycraft.campaign.notifications.push(name,"warning",message,{key="station-denied"})
  else c.systems[pair[1]]=pair[2];message=pair[1].." set to "..tostring(pair[2]);navycraft.campaign.tutorial.mark(name,"control")end
  acted=true;break
 end end
 if not acted and fields.engine_toggle and c then local enabled=not c.systems.engines_on;for _,engine in pairs(c.systems.engines or{})do engine.set_on=enabled end;message="Engines "..(enabled and"started"or"stopped");acted=true;navycraft.campaign.tutorial.mark(name,"control")
 elseif fields.pump_toggle and c then c.systems.pump_on=not c.systems.pump_on;message="Pumps "..(c.systems.pump_on and"ON"or"OFF");acted=true;navycraft.campaign.tutorial.mark(name,"control")
 elseif fields.radar and c then c.systems.radar_on=not c.systems.radar_on;message="Radar "..(c.systems.radar_on and"ON"or"OFF");acted=true;navycraft.campaign.tutorial.mark(name,"control")
 elseif fields.sonar_passive and c then c.systems.sonar_mode="passive";message="Passive sonar enabled";acted=true;navycraft.campaign.tutorial.mark(name,"control")
 elseif fields.sonar_ping and c then c.systems.sonar_mode="active";c.systems.do_ping=true;message="Active sonar ping";acted=true;navycraft.campaign.tutorial.mark(name,"control")
 elseif fields.clear_target and c then c.systems.target_id=nil;message="Target cleared";acted=true;navycraft.campaign.tutorial.mark(name,"control") end
 for field,param in pairs(actions)do if fields[field]then local ok,msg=command(name,param);acted=true;message=msg;if ok then navycraft.campaign.tutorial.mark(name,"control")else navycraft.campaign.notifications.push(name,"warning",msg or"Control rejected",{key="station-action"})end;break end end
 if fields.crew then message=navycraft.campaign.crew.roster(vessel(name));acted=true elseif fields.hint then message=navycraft.campaign.tutorial.hint(name);acted=true elseif fields.maintenance then message=navycraft.campaign.maintenance.status(vessel(name));acted=true elseif fields.mission_list then local p=navycraft.campaign.ports.for_player(name);message=p and navycraft.campaign.missions.list_text(name,p.id)or"Stand inside a registered port";acted=true elseif fields.faction_status then message=navycraft.campaign.factions.status(name);acted=true elseif fields.operation_status then message=navycraft.campaign.operations.status(name);acted=true
 elseif fields.hud_cycle then local s=navycraft.campaign.presentation.get(name);navycraft.campaign.presentation.set(name,"hud_mode",cycle(s.hud_mode,{"full","minimal","off"}));navycraft.campaign.hud.rebuild(name);acted=true
 elseif fields.contrast then navycraft.campaign.presentation.toggle(name,"high_contrast");navycraft.campaign.hud.rebuild(name);acted=true
 elseif fields.large_text then navycraft.campaign.presentation.toggle(name,"large_text");navycraft.campaign.hud.rebuild(name);acted=true
 elseif fields.subtitles then navycraft.campaign.presentation.toggle(name,"subtitles");acted=true
 elseif fields.motion then navycraft.campaign.presentation.toggle(name,"reduced_motion");acted=true
 elseif fields.colour then local s=navycraft.campaign.presentation.get(name);navycraft.campaign.presentation.set(name,"colorblind",cycle(s.colorblind,{"none","deuteranopia","protanopia","tritanopia"}));navycraft.campaign.hud.rebuild(name);acted=true
 elseif fields.alerts then local s=navycraft.campaign.presentation.get(name);navycraft.campaign.presentation.set(name,"alerts",cycle(s.alerts,{"all","normal","critical"}));acted=true
 elseif fields.tutorial_toggle then navycraft.campaign.presentation.toggle(name,"tutorial");acted=true
 elseif fields.tutorial_reset then local _,msg=navycraft.campaign.tutorial.reset(name);message=msg;acted=true end
 if message then navycraft.campaign.notifications.push(name,"info",type(message)=="table"and tostring(message[1])or tostring(message),{key="station-message",cooldown=0})end
 if not fields.quit then I.open(name,session.tab)end
end)
return I

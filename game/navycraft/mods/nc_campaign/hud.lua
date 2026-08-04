local H={}
local states={};local accum=0
local function vessel_for(name)
 local c=navycraft.preview.get_for_owner(name);if c then return c end
 for _,x in pairs(navycraft.preview.get_all())do if x.systems and(name==x.systems.captain or(x.systems.crew or{})[name])then return x end end
end
H.vessel_for=vessel_for
local function mission_progress(name)local m=navycraft.campaign.missions.active(name);if not m then return"No active contract"end;local progress=m.kind=="patrol"and("stage "..tostring(m.stage or 1))or string.format("%.0f/%.0f",m.progress or 0,m.target or 0);return m.title.." — "..progress end
local function heading(yaw)local degrees=(math.deg(yaw or 0)%360+360)%360;return math.floor(degrees+.5)end
local function station_text(name,c)
 if not c then return"ON FOOT"end;local s=c.systems or{};local role=navycraft.campaign.crew.role(c,name)or"passenger"
 if role=="engineer"then return string.format("ENGINEERING  engines:%s  pumps:%s %.0f  ballast:%.0f%%",s.engines_on and"RUN"or"STOP",s.pump_on and"ON"or"OFF",s.pump_charge or 0,s.ballast_air_percent or 100)
 elseif role=="gunner"then return string.format("GUNNERY  weapon:%s  target:%s  FC:%s",tostring(s.selected_weapon or 0),tostring(s.target_id or"none"),tostring(s.fire_control_mode or"manual"))
 elseif role=="sensor"then return string.format("SENSORS  radar:%s  sonar:%s  target:%s",s.radar_on and"ON"or"OFF",tostring(s.sonar_mode or"off"),tostring(s.target_id or"none"))
 elseif role=="helm"then return string.format("HELM  Hdg:%03d  throttle:%d%%  gear:%d  rudder:%+.1f",heading(c.yaw),math.floor((s.throttle or 0)*100+.5),s.gear or 0,s.rudder or 0)
 elseif role=="captain"or role=="executive"then return string.format("COMMAND  crew:%d  nav:%s  target:%s",(function()local n=0;for _ in pairs(s.crew or{})do n=n+1 end;return n end)(),tostring(s.navigation_status or"manual"),tostring(s.target_id or"none"))end
 return role:upper()
end
local function texts(name)
 local career=navycraft.campaign.career.get(name);local rank=navycraft.campaign.career.rank(name);local faction=navycraft.campaign.factions.membership(name);local c=vessel_for(name)
 local top=string.format("%s  %s  %dc  %s",rank.display or rank.name,name,navycraft.campaign.career.balance(name),faction and tostring(faction.faction):upper()or"UNALIGNED")
 local vessel="No active vessel"
 if c then local s=c.systems or{};vessel=string.format("%s  %.1f m/s  hull:%d%%  flood:%.1f  cond:%d%%",s.custom_name or(c.profile and c.profile.craft_type)or"Vessel",math.abs(c.forward_speed or 0),math.floor((s.hull_integrity or 1)*100),s.flooding or 0,math.floor((s.maintenance_condition or 1)*100))end
 return top,vessel,station_text(name,c),mission_progress(name),c
end
local function add(player,def)if not player.hud_add then return nil end;return player:hud_add(def)end
local function change(player,id,field,value)if id and player.hud_change then player:hud_change(id,field,value)end end
local function remove(player,id)if id and player.hud_remove then player:hud_remove(id)end end
function H.rebuild(name)
 local player=core.get_player_by_name(name);if not player then return end;local old=states[name];if old then for _,id in pairs(old.ids)do remove(player,id)end end
 local pref=navycraft.campaign.presentation.get(name);if pref.hud_mode=="off"then states[name]={ids={}};return end
 -- Luanti HUD text size is a multiplier of the player's configured font
 -- size, not a pixel value. Values such as 16 or 22 make the HUD enormous.
 local font_scale=pref.large_text and 1.25 or 1.0
 local alert_scale=pref.large_text and 1.45 or 1.2
 local palette=navycraft.campaign.presentation.palette(name);local ids={}
 ids.background=add(player,{type="image",position={x=.5,y=0},offset={x=0,y=18},scale={x=10,y=1.6},text="nc_hud_panel.png",alignment={x=0,y=1},z_index=-100})
 ids.top=add(player,{type="text",position={x=.02,y=.02},offset={x=0,y=0},text="",number=tonumber(palette.text:sub(2,7),16),alignment={x=1,y=1},style=1,size={x=font_scale,y=font_scale}})
 ids.vessel=add(player,{type="text",position={x=.02,y=.055},text="",number=tonumber(palette.accent:sub(2,7),16),alignment={x=1,y=1},style=1,size={x=font_scale,y=font_scale}})
 if pref.hud_mode=="full"then
  ids.station=add(player,{type="text",position={x=.02,y=.09},text="",number=tonumber(palette.good:sub(2,7),16),alignment={x=1,y=1},style=1,size={x=font_scale,y=font_scale}})
  ids.mission=add(player,{type="text",position={x=.02,y=.125},text="",number=tonumber(palette.warning:sub(2,7),16),alignment={x=1,y=1},style=1,size={x=font_scale,y=font_scale}})
 end
 ids.alert=add(player,{type="text",position={x=.5,y=.2},offset={x=0,y=0},text="",number=tonumber(palette.critical:sub(2,7),16),alignment={x=0,y=0},style=1,size={x=alert_scale,y=alert_scale}})
 states[name]={ids=ids};H.refresh(name)
end
function H.refresh(name)
 local player=core.get_player_by_name(name);if not player then return end;local state=states[name];if not state then H.rebuild(name);state=states[name]end
 local top,vessel,station,mission,c=texts(name);change(player,state.ids.top,"text",top);change(player,state.ids.vessel,"text",vessel);change(player,state.ids.station,"text",station);change(player,state.ids.mission,"text",mission)
 local alert=navycraft.campaign.notifications.latest(name);change(player,state.ids.alert,"text",alert and alert.text or"")
 if c then
  local s=c.systems or{};if(s.hull_integrity or 1)<.5 then navycraft.campaign.notifications.push(name,"critical","Hull integrity below 50 percent",{key="hull-low",cooldown=15})end
  if(s.flooding or 0)>5 then navycraft.campaign.notifications.push(name,"warning","Flooding rising: "..string.format("%.1f",s.flooding),{key="flooding",cooldown=15})end
 end
end
function H.step(dt)accum=accum+(dt or 0);if accum<.25 then return end;accum=0;for _,player in ipairs(core.get_connected_players())do H.refresh(player:get_player_name())end end
function H.remove(name)local player=core.get_player_by_name(name);local state=states[name];if player and state then for _,id in pairs(state.ids)do remove(player,id)end end;states[name]=nil end
return H

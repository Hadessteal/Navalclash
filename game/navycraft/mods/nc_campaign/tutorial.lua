local T={}
local storage=core.get_mod_storage();local progress={}
local stages={
 {id="enlist",title="Enlist",instruction="Start your naval career with /career start.",test=function(name)return navycraft.campaign.career.get(name).started end},
 {id="faction",title="Choose a service",instruction="Join a faction with /faction join navy, merchant, or salvage.",test=function(name)return navycraft.campaign.factions.membership(name)~=nil end},
 {id="station",title="Open your station",instruction="Open the station console with /station.",test=function(name,state)return state.markers.station end},
 {id="mission",title="Take a contract",instruction="At a port, inspect /mission list and accept a contract.",test=function(name)return navycraft.campaign.missions.active(name)~=nil end},
 {id="vessel",title="Board a vessel",instruction="Launch or join a vessel, then open its station interface.",test=function(name,state)return state.markers.vessel end},
 {id="controls",title="Operate your station",instruction="Use one station control—helm, engineering, sensors, or weapons.",test=function(name,state)return state.markers.control end},
 {id="complete",title="Ready for duty",instruction="Tutorial complete. Continue your active mission.",test=function()return false end},
}
local function copy(v)
 if type(v)~="table"then return v end
 local result={}
 for key,value in pairs(v)do
  if type(value)~="function"then result[key]=copy(value)end
 end
 return result
end
local function save()storage:set_string("tutorial_v1",core.serialize(progress))end
local function load()local v=core.deserialize(storage:get_string("tutorial_v1"));if type(v)=="table"then progress=v end end;load()
local function state(name)progress[name]=progress[name]or{index=1,markers={},completed=false,dismissed=false};return progress[name]end
local function advance(name,silent)
 local s=state(name);if s.completed or s.dismissed or not navycraft.campaign.presentation.get(name).tutorial then return false end
 local changed=false
 while s.index<#stages and stages[s.index].test(name,s)do s.index=s.index+1;changed=true;if not silent then navycraft.campaign.notifications.push(name,"success","Tutorial step complete: "..stages[s.index-1].title,{key="tutorial:"..stages[s.index-1].id})end end
 if s.index==#stages and not s.completed then s.completed=true;changed=true;if not silent then navycraft.campaign.notifications.push(name,"success","Basic training complete. You are cleared for independent operations.",{duration=10,key="tutorial:complete"})end end
 if changed then save()end;return changed
end
function T.get(name)advance(name,true);local s=state(name);return copy(s),copy(stages[s.index])end
function T.status(name)local s,stage=T.get(name);if s.dismissed then return"tutorial dismissed"end;if s.completed then return"basic training complete"end;return string.format("Step %d/%d — %s: %s",s.index,#stages-1,stage.title,stage.instruction)end
function T.mark(name,key)local s=state(name);s.markers[key]=true;save();advance(name,false);return true end
function T.reset(name)progress[name]={index=1,markers={},completed=false,dismissed=false};save();advance(name,true);return true,"tutorial reset"end
function T.dismiss(name)local s=state(name);s.dismissed=true;save();return true,"tutorial dismissed; use /tutorial reset to restart"end
function T.resume(name)local s=state(name);s.dismissed=false;s.completed=false;save();advance(name,true);return true,T.status(name)end
function T.hint(name)local _,stage=T.get(name);return stage and stage.instruction or"Basic training complete."end
function T.step()for _,player in ipairs(core.get_connected_players())do advance(player:get_player_name(),false)end end
function T.save()save()end
return T

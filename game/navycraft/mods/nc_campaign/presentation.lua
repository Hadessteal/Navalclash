local P={}
local storage=core.get_mod_storage()
local settings={}
local defaults={hud_mode="full",high_contrast=false,large_text=false,colorblind="none",subtitles=true,reduced_motion=false,alerts="normal",tutorial=true}
local allowed={
 hud_mode={full=true,minimal=true,off=true},colorblind={none=true,deuteranopia=true,protanopia=true,tritanopia=true},alerts={all=true,normal=true,critical=true},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("presentation_v1",core.serialize(settings))end
local function load()local value=core.deserialize(storage:get_string("presentation_v1"));if type(value)=="table"then settings=value end end
load()
function P.get(name)
 local current=settings[name]or{};for k,v in pairs(defaults)do if current[k]==nil then current[k]=v end end;settings[name]=current;return copy(current)
end
function P.set(name,key,value)
 local current=settings[name]or P.get(name);key=tostring(key or""):lower()
 if defaults[key]==nil then return false,"unknown presentation setting"end
 if type(defaults[key])=="boolean"then
  if type(value)=="string"then value=value:lower();value=value=="on"or value=="true"or value=="yes"or value=="1" end
  value=not not value
 elseif allowed[key]then value=tostring(value or""):lower();if not allowed[key][value]then return false,"invalid value for "..key end end
 current[key]=value;settings[name]=current;persist();return true,key.." set to "..tostring(value)
end
function P.toggle(name,key)local current=P.get(name);if type(current[key])~="boolean"then return false,"setting is not a toggle"end;return P.set(name,key,not current[key])end
function P.status(name)local s=P.get(name);return string.format("HUD=%s | contrast=%s | large text=%s | colour mode=%s | subtitles=%s | reduced motion=%s | alerts=%s | tutorial=%s",s.hud_mode,s.high_contrast and"on"or"off",s.large_text and"on"or"off",s.colorblind,s.subtitles and"on"or"off",s.reduced_motion and"on"or"off",s.alerts,s.tutorial and"on"or"off")end
function P.palette(name)
 local s=P.get(name)
 if s.high_contrast then return{background="#000000EE",text="#FFFFFFFF",good="#00FF88FF",warning="#FFD400FF",critical="#FF4466FF",accent="#7FD9FFFF"}end
 if s.colorblind=="deuteranopia"or s.colorblind=="protanopia"then return{background="#10202BDD",text="#F4F4F4FF",good="#56B4E9FF",warning="#F0E442FF",critical="#D55E00FF",accent="#CC79A7FF"}end
 if s.colorblind=="tritanopia"then return{background="#21182BDD",text="#FFFFFFFF",good="#E69F00FF",warning="#F0E442FF",critical="#CC79A7FF",accent="#56B4E9FF"}end
 return{background="#07121BDD",text="#E8F3FFFF",good="#62D48AFF",warning="#F0C45AFF",critical="#F06060FF",accent="#55BCEBFF"}
end
function P.save()persist()end
return P

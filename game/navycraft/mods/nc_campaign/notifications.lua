local N={}
local active={};local history={};local dedup={};local listeners={}
local severity_rank={info=1,success=1,warning=2,critical=3}
local function now()return os.time()end
local function settings(name)return navycraft.campaign.presentation.get(name)end
local function allowed(name,severity)
 local mode=settings(name).alerts;if mode=="all"then return true end
 local rank=severity_rank[severity]or 1;if mode=="critical"then return rank>=3 end;return rank>=2 or severity=="success"or severity=="info"
end
function N.register_listener(fn)listeners[#listeners+1]=fn end
function N.push(name,severity,text,options)
 if not name or name==""or not allowed(name,severity)then return false end
 options=options or{};text=tostring(text or"");local key=name.."\0"..(options.key or text);local stamp=now()
 if dedup[key]and stamp-dedup[key]<(tonumber(options.cooldown)or 3)then return false end;dedup[key]=stamp
 local entry={severity=severity or"info",text=text,created=stamp,expires=stamp+(tonumber(options.duration)or 6),key=options.key,sound=options.sound}
 active[name]=entry;history[name]=history[name]or{};history[name][#history[name]+1]=entry;while#history[name]>40 do table.remove(history[name],1)end
 local s=settings(name);if s.subtitles or s.hud_mode=="off"then core.chat_send_player(name,"[NavyCraft "..entry.severity:upper().."] "..text)end
 for _,fn in ipairs(listeners)do pcall(fn,name,entry)end;return true
end
function N.broadcast(names,severity,text,options)for name in pairs(names or{})do N.push(name,severity,text,options)end end
function N.latest(name)local entry=active[name];if entry and entry.expires>=now()then return entry end;active[name]=nil;return nil end
function N.history(name)return history[name]or{}end
function N.clear(name)active[name]=nil end
function N.step()for name,entry in pairs(active)do if entry.expires<now()then active[name]=nil end end end
return N

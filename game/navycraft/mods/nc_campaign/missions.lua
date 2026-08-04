local M={}
local storage=core.get_mod_storage();local active={};local history={};local save_accum=0
local function copy(v)return core.deserialize(core.serialize(v))end
local function safe(v)return(v or""):gsub("[^%w_:%-]",""):sub(1,96)end
local function persist()storage:set_string("missions_v2",core.serialize({active=active,history=history}))end
local function load()local raw=storage:get_string("missions_v2");if raw==""then raw=storage:get_string("missions_v1")end;local v=core.deserialize(raw);if type(v)=="table"then active=v.active or{};history=v.history or{}end end;load()
local function player_construct(name)local c=navycraft.preview.get_for_owner(name);if c then return c end;for _,x in pairs(navycraft.preview.get_all())do if x.systems and(name==x.systems.captain or(x.systems.crew or{})[name])then return x end end end
local function item_count(inv,name)if inv.contains_item then local stack=inv:contains_item("main",name);if type(stack)=="boolean"then return stack and 1 or 0 elseif stack and stack.get_count then return stack:get_count()end end;local count=0;for _,s in ipairs(inv:get_list("main")or{})do local st=ItemStack(s);if st:get_name()==name then count=count+st:get_count()end end;return count end
local function remove_item(inv,name,count)if inv.remove_item then return inv:remove_item("main",name.." "..count)end;local list=inv:get_list("main")or{};for i,s in ipairs(list)do local st=ItemStack(s);if st:get_name()==name then local take=math.min(count,st:get_count());st:take_item(take);list[i]=st;count=count-take;if count<=0 then break end end end;inv:set_list("main",list)end
local function all_ports()local t={};for _,p in pairs(navycraft.campaign.ports.all())do t[#t+1]=p end;table.sort(t,function(a,b)return a.id<b.id end);return t end
local function reward_for(kind,distance,difficulty)difficulty=difficulty or 1;distance=distance or 0;if kind=="sea_trial"then return{credits=150,xp=100,reputation={navy=10}}end;if kind=="courier"then return{credits=math.floor(180+distance*.7)*difficulty,xp=80+20*difficulty,reputation={merchant=12*difficulty}}end;if kind=="patrol"then return{credits=math.floor(220+distance*.5)*difficulty,xp=120+30*difficulty,reputation={navy=15*difficulty}}end;return{credits=300*difficulty,xp=160*difficulty,reputation={navy=20*difficulty}}end
local function offer(id,kind,title,description,origin,destination,target,reward,required_rank)return{id=id,kind=kind,title=title,description=description,origin=origin,destination=destination,target=target,reward=reward,required_rank=required_rank or"recruit"}end
function M.offers_for(port_id,name)
    local port=navycraft.campaign.ports.get(port_id);if not port then return{}end;local offers={};local account=navycraft.campaign.career.get(name)
    if not account.completed.sea_trial then offers[#offers+1]=offer("sea_trial","sea_trial","Commissioning Sea Trial","Travel 250 metres in an active vessel and return to any port",port.id,nil,250,reward_for("sea_trial"))end
    local ports=all_ports();local cycle=math.floor(os.time()/1800)
    for _,dest in ipairs(ports)do if dest.id~=port.id then local distance=vector.distance(port.pos,dest.pos);local difficulty=math.max(1,math.min(4,math.floor(distance/500)+1));offers[#offers+1]=offer(string.format("courier:%s:%s:%d",port.id,dest.id,cycle),"courier","Sealed Naval Dispatch","Carry sealed orders to "..dest.name,port.id,dest.id,distance,reward_for("courier",distance,difficulty),difficulty>=3 and"sailor"or"recruit");offers[#offers+1]=offer(string.format("patrol:%s:%s:%d",port.id,dest.id,cycle),"patrol","Maritime Patrol","Sail to "..dest.name.." and return to "..port.name,port.id,dest.id,distance*2,reward_for("patrol",distance*2,difficulty),difficulty>=3 and"sailor"or"recruit");if #offers>=5 then break end end end
    offers[#offers+1]=offer(string.format("combat:%s:%d",port.id,cycle),"combat","Combat Readiness Patrol","Inflict 25 blocks of authorised vessel damage",port.id,nil,25,reward_for("combat",0,1),"sailor")
    if navycraft.campaign.operations then for _,o in ipairs(navycraft.campaign.operations.offers_for(port_id,name))do offers[#offers+1]=o end end
    return offers
end
local function resolve_offer(name,id)local player=core.get_player_by_name(name);if not player then return nil,"player unavailable"end;local port=navycraft.campaign.ports.at_position(player:get_pos());if not port then return nil,"stand inside a registered port"end;for i,o in ipairs(M.offers_for(port.id,name))do if o.id==id or tostring(i)==id then return o end end;return nil,"offer not found or expired"end
function M.active(name)return active[name]and copy(active[name])or nil end
function M.accept_offer(name,o)
    if active[name]then return false,"finish or abandon the active mission first"end;local career=navycraft.campaign.career.ensure(name);if not career.started then return false,"start your career with /career start"end;if not navycraft.campaign.career.has_rank(name,o.required_rank)then return false,"mission requires rank "..o.required_rank end
    local mission=copy(o);mission.accepted=os.time();mission.progress=0;mission.stage=1;mission.last_position=nil;mission.construct_id=nil
    if mission.kind=="courier"then local player=core.get_player_by_name(name);player:get_inventory():add_item("main","nc_campaign:sealed_dispatch");mission.cargo="nc_campaign:sealed_dispatch"end
    if navycraft.campaign.operations then local ok,err=navycraft.campaign.operations.on_accept(name,mission);if ok==false then return false,err end end
    active[name]=mission;persist();return true,"Accepted: "..mission.title.." — "..mission.description
end
function M.accept(name,id)local o,err=resolve_offer(name,safe(id));if not o then return false,err end;return M.accept_offer(name,o)end
local function finish(name,construct)
    local mission=active[name];if not mission then return false,"no active mission"end;local reason="mission: "..mission.title;navycraft.campaign.career.award(name,mission.reward,reason);navycraft.campaign.career.mark_completed(name,mission.kind);if mission.kind=="courier"then navycraft.campaign.career.record_stat(name,"cargo_delivered",1)end;if construct then navycraft.campaign.crew.reward_crew(construct,name,mission.reward.credits or 0,mission.reward.xp or 0,reason)end
    history[name]=history[name]or{};history[name][#history[name]+1]={id=mission.id,kind=mission.kind,title=mission.title,completed=os.time(),reward=copy(mission.reward)};while#history[name]>30 do table.remove(history[name],1)end
    active[name]=nil;if navycraft.campaign.operations then navycraft.campaign.operations.on_complete(name,mission)end;persist();core.chat_send_player(name,string.format("Mission complete: %s — %d credits, %d XP",mission.title,mission.reward.credits or 0,mission.reward.xp or 0));return true
end
M.complete=finish
function M.abandon(name)local mission=active[name];if not mission then return false,"no active mission"end;if mission.cargo then local p=core.get_player_by_name(name);if p then remove_item(p:get_inventory(),mission.cargo,1)end end;if navycraft.campaign.operations then navycraft.campaign.operations.on_abandon(name,mission)end;active[name]=nil;persist();return true,"Mission abandoned"end
local function at_port(construct,port_id)local port=navycraft.campaign.ports.get(port_id);return port and vector.distance(construct.position,port.pos)<=port.radius,port end
local function participants(construct)local names={[construct.owner]=true};if construct.systems then if construct.systems.captain then names[construct.systems.captain]=true end;for n in pairs(construct.systems.crew or{})do names[n]=true end end;return names end
local function update_mission(name,construct,distance,dt)
    local m=active[name];if not m then return end;if m.construct_id and m.construct_id~=construct.id then m.last_position=nil end;m.construct_id=construct.id
    if m.operation and navycraft.campaign.operations.update_mission(name,m,construct,dt,finish)then return end
    if m.kind=="sea_trial"then m.progress=(m.progress or 0)+distance;navycraft.campaign.career.record_stat(name,"distance",distance);if m.progress>=m.target then local port=navycraft.campaign.ports.at_position(construct.position);if port then finish(name,construct)end end
    elseif m.kind=="courier"then local arrived=at_port(construct,m.destination);if arrived then local p=core.get_player_by_name(name);if p and item_count(p:get_inventory(),m.cargo)>=1 then remove_item(p:get_inventory(),m.cargo,1);finish(name,construct)end end
    elseif m.kind=="patrol"then if m.stage==1 and at_port(construct,m.destination)then m.stage=2;core.chat_send_player(name,"Patrol checkpoint reached; return to "..tostring(m.origin))elseif m.stage==2 and at_port(construct,m.origin)then finish(name,construct)end
    elseif m.kind=="combat"and(m.progress or 0)>=m.target then finish(name,construct)end
end
function M.step_construct(construct,dt)construct._campaign_mission_accum=(construct._campaign_mission_accum or 0)+(dt or 0);save_accum=save_accum+(dt or 0);if save_accum>=10 then save_accum=0;persist()end;if construct._campaign_mission_accum<.25 then return end;local elapsed=construct._campaign_mission_accum;construct._campaign_mission_accum=0;local last=construct._campaign_mission_last_pos;local distance=last and vector.distance(last,construct.position)or 0;if distance>50 then distance=0 end;construct._campaign_mission_last_pos=vector.new(construct.position);for name in pairs(participants(construct))do update_mission(name,construct,distance,elapsed)end end
function M.on_dock(construct)for name in pairs(participants(construct))do update_mission(name,construct,0,0)end end
function M.record_damage(attacker,target,amount)if not attacker or attacker==""then return end;amount=math.max(0,tonumber(amount)or 0);navycraft.campaign.career.record_stat(attacker,"combat_damage",amount);local m=active[attacker];if m and m.kind=="combat"then m.progress=(m.progress or 0)+amount;local c=player_construct(attacker);if m.progress>=m.target then finish(attacker,c)else persist()end end end
function M.status(name)local m=active[name];if not m then return"no active mission"end;local progress;if m.operation and navycraft.campaign.operations then progress=navycraft.campaign.operations.mission_status(m)elseif m.kind=="patrol"then progress="stage "..m.stage.."/2"else progress=string.format("%.0f/%.0f",m.progress or 0,m.target or 0)end;return string.format("%s [%s] %s | progress %s | reward %dc/%dxp",m.title,m.kind,m.description,progress,m.reward.credits or 0,m.reward.xp or 0)end
function M.list_text(name,port_id)local offers=M.offers_for(port_id,name);local out={};for i,o in ipairs(offers)do out[#out+1]=string.format("%d) %s [%s] %dc/%dxp",i,o.title,o.required_rank,o.reward.credits or 0,o.reward.xp or 0)end;return table.concat(out," | "),offers end
function M.save()persist()end
return M

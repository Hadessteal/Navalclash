local H={}
local storage=core.get_mod_storage()
local shipments={}
local next_id=1
local accumulator=0
local scheduler=0
local MAX_ACTIVE=4
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("shipping_v1",core.serialize({shipments=shipments,next_id=next_id}))end
local decoded=core.deserialize(storage:get_string("shipping_v1"));if type(decoded)=="table"then shipments=decoded.shipments or{};next_id=decoded.next_id or 1 end
local function safe_id(v)return tostring(v or""):lower():gsub("[^%w_%-]",""):sub(1,48)end
local function active_count()local n=0;for _,s in pairs(shipments)do if s.status=="scheduled"or s.status=="enroute"then n=n+1 end end;return n end
local function manifest_value(port_id,manifest)
    local total=0;for key,count in pairs(manifest or{})do local p=navycraft.campaign.markets.price(port_id,key,"buy")or 1;total=total+p*(tonumber(count)or 0)end;return total
end
local function validate_manifest(origin_id,manifest)
    local clean={};local total=0
    for key,count in pairs(manifest or{})do
        count=math.max(0,math.floor(tonumber(count)or 0))
        if count>0 then
            if not navycraft.campaign.markets.commodities()[key]then return nil,"unknown commodity "..tostring(key)end
            if navycraft.campaign.markets.stock(origin_id,key)<count then return nil,string.format("%s has only %d %s",origin_id,navycraft.campaign.markets.stock(origin_id,key),key)end
            clean[key]=count;total=total+count
        end
    end
    if total<=0 then return nil,"cargo manifest is empty"end
    return clean,total
end
local function automatic_manifest(origin_id,destination_id)
    local choices={};local commodities=navycraft.campaign.markets.commodities()
    for key,def in pairs(commodities)do
        local origin=navycraft.campaign.markets.stock(origin_id,key);local destination=navycraft.campaign.markets.stock(destination_id,key)
        local surplus=origin-def.target*1.05;local shortage=def.target*.85-destination
        if surplus>4 and shortage>2 then choices[#choices+1]={key=key,score=surplus+shortage,count=math.max(2,math.min(30,math.floor(math.min(surplus,shortage))))}end
    end
    table.sort(choices,function(a,b)return a.score>b.score end)
    local manifest={};for i=1,math.min(4,#choices)do manifest[choices[i].key]=choices[i].count end
    if next(manifest)==nil then
        local fallback={fuel_drum=8,provisions=12,hull_plate=5,machinery_parts=4}
        for key,count in pairs(fallback)do manifest[key]=math.min(count,navycraft.campaign.markets.stock(origin_id,key))end
    end
    return manifest
end
local function route_risk(origin,destination)
    local distance=vector.distance(origin.pos,destination.pos);local risk=.12+math.min(.5,distance/1600)
    local territory=navycraft.campaign.territories.get(destination.id)
    if territory and(territory.owner=="corsair"or territory.challenger=="corsair")then risk=risk+.35 end
    return math.max(.05,math.min(.9,risk))
end
function H.get(id)local s=shipments[safe_id(id)];return s and copy(s)or nil end
function H.all()return copy(shipments)end
function H.dispatch(origin_id,destination_id,manifest,options)
    options=options or{};origin_id=safe_id(origin_id);destination_id=safe_id(destination_id)
    local origin=navycraft.campaign.ports.get(origin_id);local destination=navycraft.campaign.ports.get(destination_id)
    if not origin or not destination then return nil,"origin or destination port not found"end
    if origin.id==destination.id then return nil,"convoy route needs two different ports"end
    if active_count()>=MAX_ACTIVE and not options.ignore_limit then return nil,"too many active logistics convoys"end
    manifest=manifest or automatic_manifest(origin.id,destination.id)
    local clean,total=validate_manifest(origin.id,manifest);if not clean then return nil,total end
    for key,count in pairs(clean)do navycraft.campaign.markets.adjust(origin.id,key,-count)end
    local id=safe_id(options.id or("shipment-"..next_id));next_id=next_id+1
    local distance=vector.distance(origin.pos,destination.pos);local value=manifest_value(origin.id,clean)
    local shipment={id=id,origin_id=origin.id,destination_id=destination.id,faction=options.faction or(origin.faction=="corsair"and"corsair"or"merchant"),manifest=clean,cargo_units=total,value=value,distance=distance,risk=tonumber(options.risk)or route_risk(origin,destination),status="scheduled",created=os.time(),escort_reward=math.max(80,math.floor(distance*.35+value*.08)),escort_bonus=math.max(35,math.floor(value*.04)),escort_seconds=0,travel_seconds=0,pirates_defeated=0,auto_raid=options.auto_raid~=false}
    shipments[id]=shipment;persist()
    if options.start~=false then local ok,err=H.start(id);if not ok then for key,count in pairs(clean)do navycraft.campaign.markets.adjust(origin.id,key,count)end;shipments[id]=nil;persist();return nil,err end end
    return copy(shipments[id])
end
function H.start(id)
    local s=shipments[safe_id(id)];if not s then return false,"shipment not found"end;if s.status~="scheduled"then return false,"shipment is not scheduled"end
    local origin=navycraft.campaign.ports.get(s.origin_id);local destination=navycraft.campaign.ports.get(s.destination_id);if not origin or not destination then return false,"route port no longer exists"end
    local units={{class="freighter",role="freighter"},{class="escort",role="escort"}}
    if s.value>1200 then units[#units+1]={class="escort",role="escort"}end
    local encounter,err=navycraft.campaign.fleets.spawn(s.faction,origin.pos,math.max(1,math.ceil(s.value/900)),{id="convoy-"..s.id,kind="convoy",units=units,destination=destination.pos,shipment_id=s.id,owner=s.escort_name})
    if not encounter then return false,err end
    s.encounter_id=encounter.id;s.status="enroute";s.departed=os.time();persist();return true,H.status(s.id)
end
function H.accept(name,id)
    local s=shipments[safe_id(id)];if not s then return false,"shipment not found"end;if s.status~="scheduled"and s.status~="enroute"then return false,"convoy is no longer accepting escorts"end
    if s.escort_name and s.escort_name~=name then return false,"another captain already holds this escort contract"end
    local account=navycraft.campaign.career.ensure(name);if not account.started then return false,"start your career first"end
    local port=navycraft.campaign.ports.for_player(name);if s.status=="scheduled"and(not port or port.id~=s.origin_id)then return false,"accept this contract at the origin port"end
    local faction=navycraft.campaign.factions.player_faction(name);if navycraft.campaign.factions.hostile(faction,s.faction)then return false,"your faction is hostile to this convoy"end
    s.escort_name=name;s.escort_accepted=os.time();persist();return true,string.format("Escort contract accepted: %s -> %s, reward %dc",s.origin_id,s.destination_id,s.escort_reward)
end
function H.raid(id,difficulty)
    local s=shipments[safe_id(id)];if not s then return false,"shipment not found"end;if s.status~="enroute"then return false,"convoy is not underway"end;if s.pirate_encounter_id then return false,"a pirate attack is already active"end
    local convoy=navycraft.campaign.fleets.get(s.encounter_id);if not convoy then return false,"convoy encounter missing"end
    local centre=convoy.centre or navycraft.campaign.ports.get(s.origin_id).pos
    local count=math.max(1,math.min(4,math.floor(tonumber(difficulty)or math.ceil(1+s.risk*2))))
    local units={};for _=1,count do units[#units+1]={class="raider",role="raider"}end
    local pirates,err=navycraft.campaign.fleets.spawn("corsair",vector.add(centre,{x=35,y=0,z=25}),count,{id="raid-"..s.id,kind="piracy",units=units,target_encounter_id=s.encounter_id,shipment_id=s.id})
    if not pirates then return false,err end
    s.pirate_encounter_id=pirates.id;s.raid_started=os.time();persist();return true,"Corsair raiders are attacking convoy "..s.id
end
local function freighter_state(s)
    local encounter=s.encounter_id and navycraft.campaign.fleets.get(s.encounter_id);if not encounter then return nil,nil end
    for _,unit in ipairs(encounter.units or{})do if unit.role=="freighter"then return encounter,unit end end
    return encounter,nil
end
local function reward_escort(s)
    if not s.escort_name or s.escort_paid then return end
    local ratio=s.travel_seconds>0 and s.escort_seconds/s.travel_seconds or 0
    if ratio<.25 then s.escort_result="insufficient coverage";return end
    local reward=s.escort_reward+(s.pirates_defeated>0 and s.escort_bonus or 0)
    navycraft.shipyard.credit(s.escort_name,reward,"merchant convoy escort: "..s.id)
    navycraft.campaign.career.award(s.escort_name,{credits=0,xp=math.max(12,math.floor(reward/8)),reputation={merchant=8,navy=3}},"convoy escort")
    navycraft.campaign.career.record_stat(s.escort_name,"convoys_escorted",1);s.escort_paid=true;s.escort_result=string.format("paid %dc at %.0f%% coverage",reward,ratio*100)
end
function H.deliver(id)
    local s=shipments[safe_id(id)];if not s then return false,"shipment not found"end;if s.status=="delivered"then return false,"shipment already delivered"end;if s.status=="lost"then return false,"shipment was lost"end
    for key,count in pairs(s.manifest or{})do navycraft.campaign.markets.adjust(s.destination_id,key,count)end
    navycraft.campaign.supply.apply_manifest(s.destination_id,s.manifest,"convoy delivery "..s.id)
    local destination=navycraft.campaign.ports.get(s.destination_id);if destination then navycraft.campaign.territories.contest(destination.id,destination.faction,math.max(2,navycraft.campaign.supply.defence_multiplier(destination.id)*3))end
    s.status="delivered";s.delivered=os.time();reward_escort(s);persist();return true,H.status(s.id)
end
function H.lose(id,reason)
    local s=shipments[safe_id(id)];if not s then return false,"shipment not found"end;if s.status=="lost"or s.status=="delivered"then return false,"shipment already resolved"end
    s.status="lost";s.lost=os.time();s.loss_reason=reason or"freighter destroyed"
    navycraft.campaign.supply.adjust(s.destination_id,"provisions",-4,"convoy loss")
    navycraft.campaign.supply.adjust(s.destination_id,"fuel",-3,"convoy loss")
    if s.escort_name then navycraft.campaign.career.add_reputation(s.escort_name,"merchant",-3)end
    persist();return true,H.status(s.id)
end
local function update_escort(s,dt,unit)
    if not s.escort_name or not unit or unit.status~="active"then return end
    local freighter=unit.construct_id and navycraft.preview.get_by_id(unit.construct_id);local escort=navycraft.preview.get_for_owner(s.escort_name)
    if not escort then for _,c in pairs(navycraft.preview.get_all())do if c.systems and(c.systems.captain==s.escort_name or(c.systems.crew or{})[s.escort_name])then escort=c;break end end end
    if freighter and escort and vector.distance(freighter.position,escort.position)<=120 then s.escort_seconds=(s.escort_seconds or 0)+dt end
end
local function count_pirates(s)
    if not s.pirate_encounter_id then return end
    local p=navycraft.campaign.fleets.get(s.pirate_encounter_id);if not p then return end
    local dead=0;for _,unit in ipairs(p.units or{})do if unit.status=="destroyed"then dead=dead+1 end end;s.pirates_defeated=dead
end
function H.step(dt)
    dt=math.max(0,tonumber(dt)or 0);accumulator=accumulator+dt;scheduler=scheduler+dt
    if accumulator<.5 then return 0 end;local elapsed=accumulator;accumulator=0;local changed=0
    for _,s in pairs(shipments)do if s.status=="enroute"then
        s.travel_seconds=(s.travel_seconds or 0)+elapsed
        local encounter,freighter=freighter_state(s);update_escort(s,elapsed,freighter);count_pirates(s)
        if not encounter then H.lose(s.id,"convoy state missing");changed=changed+1
        elseif encounter.status=="arrived"then H.deliver(s.id);changed=changed+1
        elseif not freighter or freighter.status=="destroyed"or freighter.status=="lost"then H.lose(s.id,"freighter destroyed");changed=changed+1
        elseif s.auto_raid and not s.pirate_encounter_id and s.travel_seconds>=5 and s.risk>=.2 then H.raid(s.id);changed=changed+1 end
    end end
    if scheduler>=180 then scheduler=0;H.schedule_one()end
    if changed>0 then persist()end;return changed
end
function H.schedule_one()
    if active_count()>=MAX_ACTIVE then return nil,"active convoy limit reached"end
    local ports=navycraft.campaign.ports.all();local best
    for _,origin in pairs(ports)do for _,destination in pairs(ports)do if origin.id~=destination.id and not navycraft.campaign.factions.hostile(origin.faction,destination.faction)then
        local manifest=automatic_manifest(origin.id,destination.id);local clean,total=validate_manifest(origin.id,manifest)
        if clean then local score=0;for key,count in pairs(clean)do local def=navycraft.campaign.markets.commodities()[key];score=score+count*((def and def.base)or 1)end;if not best or score>best.score then best={origin=origin.id,destination=destination.id,manifest=clean,score=score,total=total}end end
    end end end
    if not best then return nil,"no viable supply route"end
    return H.dispatch(best.origin,best.destination,best.manifest,{auto_raid=true})
end
function H.status(id)
    local s=shipments[safe_id(id)];if not s then return"shipment not found"end
    local cargo={};for key,count in pairs(s.manifest or{})do cargo[#cargo+1]=key.."="..count end;table.sort(cargo)
    local escort=s.escort_name and(" escort="..s.escort_name.." "..(s.escort_result or string.format("coverage=%.0fs",s.escort_seconds or 0)))or" escort=open"
    return string.format("%s %s->%s status=%s cargo=[%s] value=%dc risk=%d%%%s pirates=%d",s.id,s.origin_id,s.destination_id,s.status,table.concat(cargo,","),s.value or 0,math.floor((s.risk or 0)*100+.5),escort,s.pirates_defeated or 0)
end
function H.list_text()local out={};for id in pairs(shipments)do out[#out+1]=H.status(id)end;table.sort(out);return#out>0 and table.concat(out," | ")or"no logistics shipments"end
function H.save()persist()end
return H

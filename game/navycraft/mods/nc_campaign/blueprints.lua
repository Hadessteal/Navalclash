local B={}
local storage=core.get_mod_storage();local records={}
local starter={motor_launch=true,patrol_boat=true,patrol_cutter=true}
local research={
    corvette={credits=400,rank="sailor",items={hull_plate=4,machinery_parts=2}},
    frigate={credits=900,rank="petty_officer",items={hull_plate=8,machinery_parts=4,electronics=2}},
    destroyer={credits=1800,rank="officer",items={hull_plate=16,machinery_parts=8,electronics=4}},
    cruiser={credits=3500,rank="captain",items={hull_plate=28,machinery_parts=14,electronics=8}},
    carrier={credits=8000,rank="admiral",items={hull_plate=60,machinery_parts=30,electronics=20}},
    coastal_submarine={credits=650,rank="sailor",items={hull_plate=6,machinery_parts=3,electronics=2}},
    fleet_submarine={credits=1800,rank="officer",items={hull_plate=16,machinery_parts=8,electronics=8}},
    attack_submarine={credits=4200,rank="captain",items={hull_plate=32,machinery_parts=18,electronics=16}},
    scout_aircraft={credits=900,rank="officer",items={hull_plate=4,machinery_parts=4,electronics=5}},
    strike_aircraft={credits=2200,rank="captain",items={hull_plate=8,machinery_parts=8,electronics=10}},
    patrol_bomber={credits=6000,rank="admiral",items={hull_plate=20,machinery_parts=18,electronics=18}},
    light_airship={credits=500,rank="sailor",items={hull_plate=3,machinery_parts=2,electronics=2}},
    fleet_airship={credits=1900,rank="officer",items={hull_plate=12,machinery_parts=8,electronics=8}},
    light_armor={credits=700,rank="petty_officer",items={hull_plate=6,machinery_parts=4}},
    heavy_armor={credits=2100,rank="officer",items={hull_plate=18,machinery_parts=10,electronics=4}},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("blueprints_v1",core.serialize(records))end
local function load()local d=core.deserialize(storage:get_string("blueprints_v1"));if type(d)=="table"then records=d end end;load()
local function ensure(name)
    records[name]=records[name]or{unlocked={},researched={},certified={}}
    local r=records[name];r.unlocked=r.unlocked or{};r.researched=r.researched or{};r.certified=r.certified or{}
    for key in pairs(starter)do r.unlocked[key]=true end
    return r
end
local function port_for(name)local p=navycraft.campaign.ports.for_player(name);if not p then return nil,"stand inside a registered port"end;if p.services and p.services.shipyard==false then return nil,"this port has no shipyard service"end;return p end
function B.save()persist()end
function B.get(name)return copy(ensure(name))end
function B.has(name,key)return ensure(name).unlocked[key]==true end
function B.unlock(name,key,reason)
    if not navycraft.campaign.classes.catalog()[key]then return false,"unknown blueprint"end
    local r=ensure(name);r.unlocked[key]=true;r.researched[key]={at=os.time(),reason=reason or"grant"};persist();return true,key.." blueprint unlocked"
end
function B.research(name,key)
    local _,err=port_for(name);if err then return false,err end;key=(key or""):lower();local d=research[key]
    if not d then return false,starter[key]and"starter blueprint is already issued"or"blueprint is not researchable"end
    if B.has(name,key)then return false,"blueprint already unlocked"end
    if d.rank and not navycraft.campaign.career.has_rank(name,d.rank)then return false,"requires rank "..d.rank end
    local inv=core.get_player_by_name(name):get_inventory();local commodities=navycraft.campaign.markets.commodities()
    for item,count in pairs(d.items or{})do local def=commodities[item];if not def or not inv:contains_item("main",def.item.." "..count)then return false,"missing "..item.." x"..count end end
    local ok,result=navycraft.campaign.career.spend(name,d.credits,"blueprint research: "..key);if not ok then return false,result end
    for item,count in pairs(d.items or{})do inv:remove_item("main",commodities[item].item.." "..count)end
    B.unlock(name,key,"research");navycraft.campaign.career.record_stat(name,"blueprints_researched",1)
    return true,string.format("%s blueprint researched for %d credits",navycraft.campaign.classes.get(key).name,d.credits)
end
function B.certify(name,key)
    local port,err=port_for(name);if not port then return false,err end;key=(key or""):lower()
    if not B.has(name,key)then return false,"blueprint is locked"end
    local c=navycraft.preview.get_for_owner(name);if not c then return false,"launch or command a vessel first"end
    if vector.distance(c.position,port.pos)>port.radius then return false,"bring the vessel inside the same port"end
    local ok,detail=navycraft.campaign.classes.validate(c,key);if not ok then return false,detail end
    local cost=math.max(10,math.ceil((detail.service or 10)*2));local paid,msg=navycraft.campaign.career.spend(name,cost,"vessel class certification: "..key);if not paid then return false,msg end
    c.systems.vessel_class=key;c.systems.class_provisional=false;c.systems.blueprint_owner=name;c.systems.certified_at=os.time();c.systems.certified_port=port.id
    ensure(name).certified[key]=(ensure(name).certified[key]or 0)+1;navycraft.campaign.classes.apply(c);navycraft.preview.save();persist()
    return true,string.format("%s certified as %s for %d credits",c.systems.custom_name or c.id,detail.name,cost)
end
function B.list_text(name)
    local r=ensure(name);local out={};for key,d in pairs(navycraft.campaign.classes.catalog())do local req=research[key];out[#out+1]=string.format("%s=%s%s",key,r.unlocked[key]and"UNLOCKED"or"LOCKED",req and(" "..req.credits.."c/"..req.rank)or"")end;table.sort(out);return table.concat(out," | ")
end
function B.status(name)
    local r=ensure(name);local list={};for key in pairs(r.unlocked)do list[#list+1]=key end;table.sort(list);return"blueprints="..table.concat(list,",")
end
return B

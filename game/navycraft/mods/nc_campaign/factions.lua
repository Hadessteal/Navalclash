local F={}
local storage=core.get_mod_storage()
local memberships={}
local definitions={
    navy={name="Commonwealth Navy",short="NAVY",description="Commissioned defence, patrol and anti-piracy service",colour="#315a8c",playable=true,relations={navy=100,merchant=65,salvage=20,corsair=-100}},
    merchant={name="Merchant Marine",short="MERCHANT",description="Trade convoys, logistics and port development",colour="#bd8f34",playable=true,relations={navy=65,merchant=100,salvage=35,corsair=-100}},
    salvage={name="Independent Salvage Guild",short="SALVAGE",description="Recovery, towing and wreck reclamation",colour="#6c8b78",playable=true,relations={navy=20,merchant=35,salvage=100,corsair=-55}},
    corsair={name="Corsair Coalition",short="CORSAIR",description="Hostile raiders and privateer flotillas",colour="#8c3131",playable=false,relations={navy=-100,merchant=-100,salvage=-55,corsair=100}},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function safe(v)return tostring(v or""):lower():gsub("[^%w_%-]",""):sub(1,32)end
local function persist()storage:set_string("factions_v1",core.serialize(memberships))end
local decoded=core.deserialize(storage:get_string("factions_v1"));if type(decoded)=="table"then memberships=decoded end
function F.definitions()return copy(definitions)end
function F.get(id)local d=definitions[safe(id)];return d and copy(d)or nil end
function F.membership(name)return memberships[name]and copy(memberships[name])or nil end
function F.player_faction(name)
    local m=memberships[name]
    if m and definitions[m.faction]then return m.faction end
    local account=navycraft.campaign.career.ensure(name)
    return account.started and"navy"or"neutral"
end
function F.construct_faction(construct)
    if not construct then return"neutral"end
    local systems=construct.systems or{}
    if definitions[systems.faction]then return systems.faction end
    local owner=tostring(systems.owner or construct.owner or"")
    local parsed=owner:match("^NPC:([^:]+):")
    if parsed and definitions[parsed]then return parsed end
    return F.player_faction(owner)
end
function F.relation(a,b)
    a=safe(a);b=safe(b);if a==b and definitions[a]then return 100 end
    local d=definitions[a];return d and(d.relations[b]or 0)or 0
end
function F.hostile(a,b)return F.relation(a,b)<=-50 or F.relation(b,a)<=-50 end
function F.join(name,id)
    id=safe(id);local d=definitions[id]
    if not d then return false,"unknown faction"end
    if not d.playable then return false,"that faction is not open to players"end
    local account=navycraft.campaign.career.ensure(name);if not account.started then return false,"start your career first"end
    local current=memberships[name]
    if current and current.faction==id then return false,"already enlisted with "..d.name end
    local standing=(account.reputation or{})[id]or 0
    if standing<-100 then return false,"standing is too low to enlist"end
    memberships[name]={faction=id,joined=os.time(),previous=current and current.faction or nil}
    navycraft.campaign.career.add_reputation(name,id,10);persist();return true,"Joined "..d.name
end
function F.set(name,id,reason)
    id=safe(id);if id~="neutral"and not definitions[id]then return false,"unknown faction"end
    memberships[name]=id=="neutral"and nil or{faction=id,joined=os.time(),reason=reason};persist();return true
end
function F.status(name)
    local id=F.player_faction(name);local d=definitions[id];local account=navycraft.campaign.career.get(name)
    if not d then return"Independent / no faction allegiance"end
    local standing=(account.reputation or{})[id]or 0
    return string.format("%s [%s] standing=%d | allies and enemies derive from faction diplomacy",d.name,id,standing)
end
function F.list_text()
    local out={};for id,d in pairs(definitions)do out[#out+1]=string.format("%s=%s%s",id,d.name,d.playable and""or" [NPC]")end;table.sort(out);return table.concat(out," | ")
end
function F.save()persist()end
return F

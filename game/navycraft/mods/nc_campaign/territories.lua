local T={}
local storage=core.get_mod_storage();local territories={};local accumulator=0
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("territories_v1",core.serialize(territories))end
local decoded=core.deserialize(storage:get_string("territories_v1"));if type(decoded)=="table"then territories=decoded end
local function territory_radius(port)return math.max(80,(port.radius or 20)*5)end
function T.ensure(port_id)
    local port=navycraft.campaign.ports.get(port_id);if not port then return nil end
    local t=territories[port.id]
    if not t then t={id=port.id,name=port.name,port_id=port.id,pos=vector.new(port.pos),radius=territory_radius(port),owner=port.faction or"navy",challenger=nil,progress=0,changed=os.time()};territories[port.id]=t;persist()end
    t.name=port.name;t.pos=vector.new(port.pos);t.radius=territory_radius(port);return t
end
function T.get(id)local t=T.ensure(id);return t and copy(t)or nil end
function T.all()for id in pairs(navycraft.campaign.ports.all())do T.ensure(id)end;return copy(territories)end
function T.at_position(pos)
    local best,dist;for _,t in pairs(T.all())do local d=vector.distance(pos,t.pos);if d<=t.radius and(not dist or d<dist)then best,dist=t,d end end;return best,dist
end
function T.set_owner(id,faction,reason)
    local t=T.ensure(id);if not t then return false,"territory not found"end
    if faction~="neutral"and not navycraft.campaign.factions.get(faction)then return false,"unknown faction"end
    local previous=t.owner;t.owner=faction;t.challenger=nil;t.progress=0;t.changed=os.time();t.reason=reason
    territories[id]=t;navycraft.campaign.ports.set_faction(id,faction);persist()
    return true,string.format("%s changed control from %s to %s",t.name,previous,faction)
end
function T.contest(id,faction,amount)
    local t=T.ensure(id);if not t then return false,"territory not found"end
    faction=tostring(faction or"neutral");amount=math.max(0,tonumber(amount)or 0)
    if faction==t.owner then t.progress=math.max(0,(t.progress or 0)-amount);if t.progress==0 then t.challenger=nil end
    else
        if t.challenger~=faction then t.challenger=faction;t.progress=0 end
        t.progress=math.min(100,(t.progress or 0)+amount)
        if t.progress>=100 then return T.set_owner(id,faction,"territorial capture")end
    end
    territories[id]=t;persist();return true,T.status(id)
end
local function strength(c)
    local p=c.profile or{};local alive=p.block_count_alive or p.block_count or 1
    return math.max(.5,math.min(15,1+alive/100+#(p.weapons or{})*1.5))
end
function T.step(dt)
    accumulator=accumulator+math.max(0,tonumber(dt)or 0);if accumulator<1 then return end;local elapsed=accumulator;accumulator=0
    local presence={}
    for _,c in pairs(navycraft.preview.get_all())do
        if c.position and c.systems and not c.systems.sinking then
            local t=T.at_position(c.position);if t then local faction=navycraft.campaign.factions.construct_faction(c);presence[t.id]=presence[t.id]or{};presence[t.id][faction]=(presence[t.id][faction]or 0)+strength(c)end
        end
    end
    for id,t in pairs(territories)do
        local p=presence[id]or{};local owner_strength=p[t.owner]or 0;if navycraft.campaign.supply then owner_strength=owner_strength*navycraft.campaign.supply.defence_multiplier(id)end;local challenger,challenger_strength
        for faction,value in pairs(p)do if faction~=t.owner and faction~="neutral"and value>(challenger_strength or 0)then challenger,challenger_strength=faction,value end end
        if challenger and challenger_strength>owner_strength then T.contest(id,challenger,(challenger_strength-owner_strength)*elapsed*.45)
        elseif owner_strength>0 and(t.progress or 0)>0 then T.contest(id,t.owner,owner_strength*elapsed*.35)end
    end
end
function T.record_victory(position,winner,loser,weight)
    local t=T.at_position(position);if not t then return end
    local amount=math.max(5,tonumber(weight)or 10)
    if winner and winner~="neutral"then T.contest(t.id,winner,amount)end
end
function T.status(id)
    local t=T.ensure(id);if not t then return"territory not found"end
    return string.format("%s owner=%s%s progress=%.1f/100 radius=%d",t.name,t.owner,t.challenger and(" challenged-by="..t.challenger)or"",t.progress or 0,t.radius)
end
function T.list_text()local out={};for id in pairs(T.all())do out[#out+1]=T.status(id)end;table.sort(out);return#out>0 and table.concat(out," | ")or"no territories"end
function T.save()persist()end
return T

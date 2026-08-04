local S={}
local storage=core.get_mod_storage()
local ports={}
local accumulator=0
local categories={"fuel","provisions","munitions","repair","industry"}
local commodity_supply={
    fuel_drum={fuel=3.0},crude_oil={fuel=.7},coal={fuel=.25},
    provisions={provisions=2.5},
    munitions_crate={munitions=4.0},steel_ingot={munitions=.3},
    hull_plate={repair=2.0},machinery_parts={repair=1.2,industry=1.6},
    electronics={industry=2.2},copper_ingot={industry=.4},salvage={repair=.5,industry=.4},
}
local service_category={ammo="munitions",repair="repair",industry="industry",trade="provisions",shipyard="industry"}
local commodity_category={
    fuel_drum="fuel",crude_oil="fuel",coal="fuel",provisions="provisions",
    munitions_crate="munitions",hull_plate="repair",machinery_parts="industry",electronics="industry",
}
local faction_initial={
    navy={fuel=72,provisions=70,munitions=82,repair=76,industry=68},
    merchant={fuel=84,provisions=88,munitions=45,repair=66,industry=78},
    salvage={fuel=58,provisions=56,munitions=48,repair=82,industry=72},
    corsair={fuel=62,provisions=48,munitions=76,repair=50,industry=42},
    neutral={fuel=55,provisions=55,munitions=55,repair=55,industry=55},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function clamp(v,a,b)return math.max(a,math.min(b,v))end
local function persist()storage:set_string("strategic_supply_v1",core.serialize(ports))end
local decoded=core.deserialize(storage:get_string("strategic_supply_v1"));if type(decoded)=="table"then ports=decoded end
local function make_state(port)
    local base=faction_initial[port.faction]or faction_initial.neutral
    local state={port_id=port.id,levels={},delivered=0,consumed=0,revision=1,last_tick=os.time()}
    for _,key in ipairs(categories)do state.levels[key]=base[key]or 55 end
    return state
end
function S.ensure(port_id)
    local port=navycraft.campaign.ports.get(port_id);if not port then return nil,"port not found"end
    local state=ports[port.id]
    if not state then state=make_state(port);ports[port.id]=state;persist()end
    state.levels=state.levels or{};for _,key in ipairs(categories)do state.levels[key]=clamp(tonumber(state.levels[key])or 50,0,100)end
    return state
end
function S.get(port_id)local s=S.ensure(port_id);return s and copy(s)or nil end
function S.all()for id in pairs(navycraft.campaign.ports.all())do S.ensure(id)end;return copy(ports)end
function S.level(port_id,category)local s=S.ensure(port_id);return s and(s.levels[category]or 0)or 0 end
function S.set(port_id,category,value)
    local s,err=S.ensure(port_id);if not s then return false,err end;if not s.levels[category]then return false,"unknown supply category"end
    s.levels[category]=clamp(tonumber(value)or 0,0,100);s.revision=(s.revision or 0)+1;persist();return true,s.levels[category]
end
function S.adjust(port_id,category,amount,reason)
    local s,err=S.ensure(port_id);if not s then return false,err end;if s.levels[category]==nil then return false,"unknown supply category"end
    local delta=tonumber(amount)or 0;s.levels[category]=clamp(s.levels[category]+delta,0,100);s.revision=(s.revision or 0)+1;s.last_reason=reason;s.last_change=os.time()
    if delta>=0 then s.delivered=(s.delivered or 0)+delta else s.consumed=(s.consumed or 0)-delta end;persist();return true,s.levels[category]
end
function S.apply_manifest(port_id,manifest,reason)
    local changed={}
    for commodity,count in pairs(manifest or{})do
        local mapping=commodity_supply[commodity]
        if mapping and(tonumber(count)or 0)>0 then for category,per_unit in pairs(mapping)do
            local delta=per_unit*(tonumber(count)or 0);S.adjust(port_id,category,delta,reason or("delivery: "..commodity));changed[category]=(changed[category]or 0)+delta
        end end
    end
    return changed
end
function S.readiness(port_id)
    local s=S.ensure(port_id);if not s then return 0 end
    return(s.levels.fuel*.22+s.levels.provisions*.18+s.levels.munitions*.22+s.levels.repair*.20+s.levels.industry*.18)/100
end
function S.category_for_commodity(key)return commodity_category[key]end
function S.price_multiplier(port_id,key,side)
    local category=commodity_category[key];if not category then return 1 end
    local level=S.level(port_id,category);local demand=1+(55-level)/95
    demand=clamp(demand,.72,1.65)
    if side=="sell"then return clamp(.88+demand*.20,.82,1.24)end
    return demand
end
function S.service_status(port_id,service,units)
    local category=service_category[service];if not category then return true,1,"ready"end
    local level=S.level(port_id,category);local need=math.max(.5,tonumber(units)or 1)
    if level<math.min(8,need*.25)then return false,nil,string.format("port %s stores are critically depleted (%d%%)",category,math.floor(level+.5))end
    local surcharge=clamp(1+(55-level)/85,.82,1.85)
    return true,surcharge,string.format("%s supply %d%%",category,math.floor(level+.5))
end
function S.consume_service(port_id,service,units,reason)
    local category=service_category[service];if not category then return true end
    local ok,_,message=S.service_status(port_id,service,units);if not ok then return false,message end
    local cost=math.max(.2,tonumber(units)or 1);return S.adjust(port_id,category,-cost,reason or("service: "..service))
end
function S.industry_multiplier(port_id)
    local industry=S.level(port_id,"industry");local fuel=S.level(port_id,"fuel")
    return clamp(.35+industry*.006+fuel*.0025,.4,1.2)
end
function S.defence_multiplier(port_id)
    local m=S.level(port_id,"munitions");local r=S.level(port_id,"repair");local f=S.level(port_id,"fuel")
    return clamp(.35+(m*.004+r*.003+f*.002),.35,1.25)
end
function S.step(dt)
    accumulator=accumulator+math.max(0,tonumber(dt)or 0);if accumulator<60 then return 0 end
    local ticks=math.floor(accumulator/60);accumulator=accumulator-ticks*60;local changed=0
    for id,state in pairs(ports)do
        local territory=navycraft.campaign.territories and navycraft.campaign.territories.get(id)
        local contested=territory and territory.challenger and 1.7 or 1
        local drains={fuel=.22,provisions=.30,munitions=.10,repair=.12,industry=.18}
        for category,rate in pairs(drains)do state.levels[category]=clamp((state.levels[category]or 50)-rate*ticks*contested,0,100);changed=changed+1 end
        state.last_tick=os.time();state.revision=(state.revision or 0)+1
    end
    if changed>0 then persist()end;return changed
end
function S.status(port_id)
    local port=navycraft.campaign.ports.get(port_id);local s=S.ensure(port_id);if not port or not s then return"port not found"end
    return string.format("%s readiness=%d%% fuel=%d provisions=%d munitions=%d repair=%d industry=%d defence=%.2fx industry-rate=%.2fx",
        port.name,math.floor(S.readiness(port_id)*100+.5),math.floor(s.levels.fuel+.5),math.floor(s.levels.provisions+.5),math.floor(s.levels.munitions+.5),math.floor(s.levels.repair+.5),math.floor(s.levels.industry+.5),S.defence_multiplier(port_id),S.industry_multiplier(port_id))
end
function S.list_text()local out={};for id in pairs(S.all())do out[#out+1]=S.status(id)end;table.sort(out);return#out>0 and table.concat(out," | ")or"no port supply states"end
function S.save()persist()end
return S

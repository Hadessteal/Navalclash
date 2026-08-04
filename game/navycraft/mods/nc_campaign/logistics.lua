local L={}
local cargo_data={
    iron_ore={units=1,mass=1.2},coal={units=1,mass=.8},copper_ore={units=1,mass=1.1},crude_oil={units=1,mass=.9},salvage={units=1.5,mass=1.0},
    steel_ingot={units=1.5,mass=1.5},copper_ingot={units=1.5,mass=1.4},fuel_drum={units=2,mass=1.8},provisions={units=1.2,mass=.7},munitions_crate={units=2.5,mass=2.0},hull_plate={units=3,mass=2.5},machinery_parts={units=3,mass=2.2},electronics={units=2,mass=.8},
}
local function active(name)local c=navycraft.preview.get_for_owner(name);if not c then return nil,"launch or command a vessel first"end;return c end
local function port_for(name,c)
    local p=navycraft.campaign.ports.for_player(name);if not p then return nil,"stand inside a registered port"end
    if vector.distance(c.position,p.pos)>p.radius then return nil,"bring the vessel inside the same port"end;return p
end
local function ensure(c)c.systems.cargo_manifest=c.systems.cargo_manifest or{};return c.systems.cargo_manifest end
local function totals(c)
    local used,mass=0,0;for key,count in pairs(ensure(c))do local d=cargo_data[key];if d and count>0 then used=used+d.units*count;mass=mass+d.mass*count end end;return used,mass
end
function L.data()return cargo_data end
function L.apply(c)
    if not c or not c.systems then return nil end
    local class=navycraft.campaign.classes.apply(c);local eq=c.systems.equipment_modifiers or{}
    local capacity=(class and class.cargo or 8)*(eq.cargo_mult or 1);local used,mass=totals(c)
    c.systems.cargo_capacity=capacity;c.systems.cargo_used=used;c.systems.cargo_mass=mass
    c.systems.operating_mass=(c.profile.weight or 1)*(eq.mass_mult or 1)+(eq.mass_add or 0)+mass
    c.systems.overloaded=used>capacity+.001
    return{capacity=capacity,used=used,mass=mass,overloaded=c.systems.overloaded}
end
function L.load(name,key,amount)
    local c,err=active(name);if not c then return false,err end;local _,perr=port_for(name,c);if perr then return false,perr end
    key=(key or""):lower();local d=cargo_data[key];local commodity=navycraft.campaign.markets.commodities()[key];if not d or not commodity then return false,"unknown cargo commodity"end
    amount=math.max(1,math.min(500,math.floor(tonumber(amount)or 1)));local state=L.apply(c);local room=math.floor((state.capacity-state.used)/d.units+1e-6);if room<amount then return false,string.format("cargo hold has room for only %d unit(s)",math.max(0,room))end
    local inv=core.get_player_by_name(name):get_inventory();if not inv:contains_item("main",commodity.item.." "..amount)then return false,"not enough "..key end
    inv:remove_item("main",commodity.item.." "..amount);local manifest=ensure(c);manifest[key]=(manifest[key]or 0)+amount;L.apply(c);navycraft.preview.save();navycraft.campaign.career.record_stat(name,"cargo_loaded",amount)
    return true,string.format("Loaded %d %s; cargo %.1f/%.1f units",amount,key,c.systems.cargo_used,c.systems.cargo_capacity)
end
function L.unload(name,key,amount)
    local c,err=active(name);if not c then return false,err end;local _,perr=port_for(name,c);if perr then return false,perr end
    key=(key or""):lower();local commodity=navycraft.campaign.markets.commodities()[key];if not cargo_data[key]or not commodity then return false,"unknown cargo commodity"end
    local manifest=ensure(c);amount=math.max(1,math.min(math.floor(tonumber(amount)or 1),manifest[key]or 0));if amount<=0 then return false,"that commodity is not aboard"end
    local leftover=core.get_player_by_name(name):get_inventory():add_item("main",commodity.item.." "..amount);local remaining=leftover and leftover.get_count and leftover:get_count()or 0;local moved=amount-remaining;if moved<=0 then return false,"inventory is full"end
    manifest[key]=(manifest[key]or 0)-moved;if manifest[key]<=0 then manifest[key]=nil end;L.apply(c);navycraft.preview.save();return true,string.format("Unloaded %d %s",moved,key)
end
function L.status(c)
    if not c then return"no active vessel"end;local s=L.apply(c);local out={};for key,count in pairs(ensure(c))do if count>0 then out[#out+1]=key.."="..count end end;table.sort(out)
    return string.format("cargo %.1f/%.1f units mass=%.1f operating_mass=%.1f %s | %s",s.used,s.capacity,s.mass,c.systems.operating_mass,s.overloaded and"OVERLOADED"or"ready",#out>0 and table.concat(out,",")or"empty")
end
return L

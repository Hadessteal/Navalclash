local Q={}
-- Upgrades are deliberately side-grades as well as upgrades: higher performance
-- adds mass and maintenance burden, while class certification caps usable tier.
local catalog={
    engine_t1={item="nc_campaign:engine_kit_t1",slot="engine",tier=1,name="Improved Propulsion I",speed_mult=1.08,mass_mult=1.02,maintenance_mult=1.08},
    engine_t2={item="nc_campaign:engine_kit_t2",slot="engine",tier=2,name="Improved Propulsion II",speed_mult=1.15,mass_mult=1.05,maintenance_mult=1.20,rank="officer"},
    pump_t1={item="nc_campaign:pump_kit_t1",slot="pump",tier=1,name="Damage-Control Pumps I",pump_mult=1.30,mass_add=2,maintenance_mult=1.04},
    pump_t2={item="nc_campaign:pump_kit_t2",slot="pump",tier=2,name="Damage-Control Pumps II",pump_mult=1.62,mass_add=5,maintenance_mult=1.12,rank="officer"},
    sensor_t1={item="nc_campaign:sensor_kit_t1",slot="sensor",tier=1,name="Sensor Suite I",sensor_mult=1.22,mass_add=1.5,maintenance_mult=1.05,rank="sailor"},
    sensor_t2={item="nc_campaign:sensor_kit_t2",slot="sensor",tier=2,name="Sensor Suite II",sensor_mult=1.50,mass_add=4,maintenance_mult=1.14,rank="officer"},
    fire_control_t1={item="nc_campaign:fire_control_kit_t1",slot="fire_control",tier=1,name="Fire Control I",reload_mult=.90,mass_add=1.5,maintenance_mult=1.06,rank="petty_officer"},
    fire_control_t2={item="nc_campaign:fire_control_kit_t2",slot="fire_control",tier=2,name="Fire Control II",reload_mult=.76,mass_add=4,maintenance_mult=1.16,rank="captain"},
    cargo_t1={item="nc_campaign:cargo_kit_t1",slot="cargo",tier=1,name="Cargo Handling I",cargo_mult=1.35,mass_add=2,speed_mult=.99},
    cargo_t2={item="nc_campaign:cargo_kit_t2",slot="cargo",tier=2,name="Cargo Handling II",cargo_mult=1.75,mass_add=6,speed_mult=.96,maintenance_mult=1.06,rank="officer"},
    armor_t1={item="nc_campaign:armor_kit_t1",slot="armor",tier=1,name="Compartment Armour I",flooding_mult=.84,mass_mult=1.08,speed_mult=.97},
    armor_t2={item="nc_campaign:armor_kit_t2",slot="armor",tier=2,name="Compartment Armour II",flooding_mult=.66,mass_mult=1.18,speed_mult=.92,maintenance_mult=1.10,rank="captain"},
}
local function port_for(name)local p=navycraft.campaign.ports.for_player(name);if not p then return nil,"stand inside a registered port"end;if p.services and p.services.shipyard==false then return nil,"this port has no shipyard service"end;return p end
local function modifiers(c)
    local out={speed_mult=1,pump_mult=1,sensor_mult=1,reload_mult=1,flooding_mult=1,cargo_mult=1,mass_mult=1,mass_add=0,maintenance_mult=1}
    for _,key in pairs((c.systems and c.systems.equipment)or{})do local d=catalog[key];if d then for field,value in pairs(d)do
        if field=="mass_add"then out.mass_add=out.mass_add+value elseif field:find("_mult$")then out[field]=(out[field]or 1)*value end
    end end end
    return out
end
function Q.catalog()return catalog end
function Q.apply(c)if not c or not c.systems then return nil end;c.systems.equipment=c.systems.equipment or{};c.systems.equipment_modifiers=modifiers(c);return c.systems.equipment_modifiers end
function Q.install(name,key)
    local port,err=port_for(name);if err then return false,err end;key=(key or""):lower();local d=catalog[key];if not d then return false,"unknown equipment package"end
    if d.rank and not navycraft.campaign.career.has_rank(name,d.rank)then return false,"requires rank "..d.rank end
    local c=navycraft.preview.get_for_owner(name);if not c then return false,"launch or command a vessel first"end
    if vector.distance(c.position,port.pos)>port.radius then return false,"bring the vessel inside the same port"end
    local allowed=navycraft.campaign.classes.max_tier(c,d.slot);if allowed<d.tier then return false,string.format("%s class supports %s only through tier %d",c.systems.vessel_class or"unclassified",d.slot,allowed)end
    local inv=core.get_player_by_name(name):get_inventory();if not inv:contains_item("main",d.item.." 1")then return false,"required refit kit is not in your inventory"end
    c.systems.equipment=c.systems.equipment or{};local existing=c.systems.equipment[d.slot];if existing and(catalog[existing]and catalog[existing].tier or 0)>=d.tier then return false,"an equal or better "..d.slot.." package is already installed"end
    inv:remove_item("main",d.item.." 1");c.systems.equipment[d.slot]=key;Q.apply(c);navycraft.campaign.logistics.apply(c);navycraft.preview.save();navycraft.campaign.career.record_stat(name,"equipment_installed",1)
    return true,d.name.." installed on "..(c.systems.custom_name or c.id)
end
function Q.status(c)
    if not c then return"no active vessel"end;Q.apply(c);local out={};for slot,key in pairs(c.systems.equipment or{})do out[#out+1]=slot.."="..key end;table.sort(out);local m=c.systems.equipment_modifiers
    return string.format("equipment %s | speed=%.2fx pump=%.2fx sensor=%.2fx reload=%.2fx cargo=%.2fx flood=%.2fx mass=%.2fx+%.1f maint=%.2fx",#out>0 and table.concat(out,",")or"stock",m.speed_mult,m.pump_mult,m.sensor_mult,m.reload_mult,m.cargo_mult,m.flooding_mult,m.mass_mult,m.mass_add,m.maintenance_mult)
end
function Q.list_text(name)local out={};for key,d in pairs(catalog)do if not d.rank or navycraft.campaign.career.has_rank(name,d.rank)then out[#out+1]=key.."="..d.name end end;table.sort(out);return table.concat(out," | ")end
return Q

local V={}

-- Gameplay classifications layered over the source-derived NavyCraft craft types.
-- The source plugin supplied broad craft categories and Shipyard lot labels; these
-- finer classes are original campaign balance data for the Luanti game.
local catalog={
    motor_launch={name="Motor Launch",craft={boat=true},min_blocks=9,max_blocks=79,max_weight=60,cargo=12,max_engines=2,max_weapons=1,crew=1,rank="recruit",lot="SHIP1",service=5,
        tiers={engine=1,pump=1,cargo=1}},
    patrol_boat={name="Patrol Boat",craft={boat=true},min_blocks=80,max_blocks=500,max_weight=220,cargo=30,max_engines=3,max_weapons=3,crew=2,rank="recruit",lot="SHIP1",service=9,
        tiers={engine=1,pump=1,sensor=1,fire_control=1,cargo=1,armor=1}},
    patrol_cutter={name="Patrol Cutter",craft={ship=true,freeship=true,halfship=true},min_blocks=9,max_blocks=179,max_weight=110,cargo=28,max_engines=2,max_weapons=2,crew=2,rank="recruit",lot="SHIP1",service=10,
        tiers={engine=1,pump=1,sensor=1,fire_control=1,cargo=1,armor=1}},
    corvette={name="Corvette",craft={ship=true,freeship=true,halfship=true},min_blocks=180,max_blocks=449,max_weight=270,cargo=55,max_engines=3,max_weapons=4,crew=3,rank="sailor",lot="SHIP1",service=18,
        tiers={engine=1,pump=2,sensor=1,fire_control=1,cargo=1,armor=1}},
    frigate={name="Frigate",craft={ship=true,freeship=true,halfship=true},min_blocks=450,max_blocks=999,max_weight=600,cargo=95,max_engines=5,max_weapons=7,crew=5,rank="petty_officer",lot="SHIP1",service=32,
        tiers={engine=2,pump=2,sensor=2,fire_control=1,cargo=2,armor=1}},
    destroyer={name="Destroyer",craft={ship=true,freeship=true,halfship=true},min_blocks=1000,max_blocks=2199,max_weight=1350,cargo=150,max_engines=8,max_weapons=12,crew=8,rank="officer",lot="SHIP1",service=55,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=2,armor=2}},
    cruiser={name="Cruiser",craft={ship=true,freeship=true,halfship=true},min_blocks=2200,max_blocks=4499,max_weight=2900,cargo=260,max_engines=12,max_weapons=18,crew=12,rank="captain",lot="SHIP4",service=95,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=2,armor=2}},
    carrier={name="Fleet Carrier",craft={ship=true,freeship=true,halfship=true},min_blocks=4500,max_blocks=18000,max_weight=12000,cargo=520,max_engines=24,max_weapons=28,crew=20,rank="admiral",lot="SHIP5",service=180,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=2,armor=2}},
    coastal_submarine={name="Coastal Submarine",craft={submarine=true},min_blocks=20,max_blocks=249,max_weight=160,cargo=20,max_engines=3,max_weapons=4,crew=3,rank="sailor",lot="SHIP2",service=22,
        tiers={engine=1,pump=2,sensor=1,fire_control=1,cargo=1,armor=1}},
    fleet_submarine={name="Fleet Submarine",craft={submarine=true},min_blocks=250,max_blocks=899,max_weight=600,cargo=45,max_engines=5,max_weapons=8,crew=6,rank="officer",lot="SHIP3",service=48,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=1,armor=2}},
    attack_submarine={name="Attack Submarine",craft={submarine=true},min_blocks=900,max_blocks=18000,max_weight=9000,cargo=90,max_engines=12,max_weapons=16,crew=10,rank="captain",lot="SHIP3",service=110,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=2,armor=2}},
    scout_aircraft={name="Scout Aircraft",craft={aircraft=true},min_blocks=20,max_blocks=199,max_weight=100,cargo=8,max_engines=2,max_weapons=2,crew=1,rank="officer",lot="HANGAR1",service=20,
        tiers={engine=1,sensor=1,fire_control=1,cargo=1,armor=1}},
    strike_aircraft={name="Strike Aircraft",craft={aircraft=true},min_blocks=200,max_blocks=699,max_weight=420,cargo=18,max_engines=4,max_weapons=6,crew=2,rank="captain",lot="HANGAR1",service=45,
        tiers={engine=2,sensor=2,fire_control=2,cargo=1,armor=1}},
    patrol_bomber={name="Patrol Bomber",craft={aircraft=true},min_blocks=700,max_blocks=18000,max_weight=9000,cargo=55,max_engines=12,max_weapons=16,crew=6,rank="admiral",lot="HANGAR2",service=115,
        tiers={engine=2,pump=1,sensor=2,fire_control=2,cargo=2,armor=2}},
    light_airship={name="Light Airship",craft={airship=true},min_blocks=9,max_blocks=199,max_weight=100,cargo=20,max_engines=2,max_weapons=2,crew=2,rank="sailor",lot="HANGAR1",service=16,
        tiers={engine=1,pump=1,sensor=1,fire_control=1,cargo=1,armor=1}},
    fleet_airship={name="Fleet Airship",craft={airship=true},min_blocks=200,max_blocks=1000,max_weight=700,cargo=80,max_engines=8,max_weapons=10,crew=7,rank="officer",lot="HANGAR2",service=62,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=2,armor=2}},
    light_armor={name="Light Armoured Vehicle",craft={tank=true},min_blocks=10,max_blocks=199,max_weight=130,cargo=10,max_engines=2,max_weapons=2,crew=2,rank="petty_officer",lot="TANK1",service=18,
        tiers={engine=1,pump=1,sensor=1,fire_control=1,cargo=1,armor=1}},
    heavy_armor={name="Heavy Armoured Vehicle",craft={tank=true},min_blocks=200,max_blocks=2000,max_weight=1400,cargo=25,max_engines=6,max_weapons=8,crew=5,rank="officer",lot="TANK2",service=55,
        tiers={engine=2,pump=2,sensor=2,fire_control=2,cargo=1,armor=2}},
}

local order={"motor_launch","patrol_boat","patrol_cutter","corvette","frigate","destroyer","cruiser","carrier","coastal_submarine","fleet_submarine","attack_submarine","scout_aircraft","strike_aircraft","patrol_bomber","light_airship","fleet_airship","light_armor","heavy_armor"}
local function copy(v)return core.deserialize(core.serialize(v))end
local function counts(c)
    local p=c.profile or{}
    return tonumber(p.block_count)or tonumber(p.block_count_alive)or 0,tonumber(p.weight)or 0,#(p.engines or{}),#(p.weapons or{})
end
function V.catalog()return catalog end
function V.get(key)return catalog[key]and copy(catalog[key])or nil end
function V.validate(c,key)
    local d=catalog[key];if not d then return false,"unknown vessel class"end
    if not c or not c.profile then return false,"no active vessel"end
    local blocks,weight,engines,weapons=counts(c);local craft=c.profile.craft_type
    if not d.craft[craft]then return false,d.name.." is not compatible with craft type "..tostring(craft)end
    if blocks<d.min_blocks or blocks>d.max_blocks then return false,string.format("%s requires %d-%d live blocks; vessel has %d",d.name,d.min_blocks,d.max_blocks,blocks)end
    if weight>d.max_weight then return false,string.format("%s maximum structural weight is %.0f; vessel has %.1f",d.name,d.max_weight,weight)end
    if engines>d.max_engines then return false,string.format("%s supports at most %d engines; vessel has %d",d.name,d.max_engines,engines)end
    if weapons>d.max_weapons then return false,string.format("%s supports at most %d weapon mounts; vessel has %d",d.name,d.max_weapons,weapons)end
    return true,d
end
function V.suggest(c)
    for _,key in ipairs(order)do local ok=V.validate(c,key);if ok then return key end end
    return nil
end
function V.apply(c)
    if not c or not c.systems or not c.profile then return nil end
    local key=c.systems.vessel_class
    if not key or not V.validate(c,key)then key=V.suggest(c);c.systems.vessel_class=key;c.systems.class_provisional=true end
    local d=key and catalog[key]or nil
    c.systems.class_limits=d and{cargo=d.cargo,crew=d.crew,service=d.service,max_engines=d.max_engines,max_weapons=d.max_weapons,tiers=copy(d.tiers),lot=d.lot}or{cargo=8,crew=1,service=8,tiers={}}
    return d
end
function V.max_tier(c,slot)local d=V.apply(c);return d and(d.tiers[slot]or 0)or 0 end
function V.status(c)
    if not c then return"no active vessel"end;local d=V.apply(c);if not d then return"unclassified vessel: no campaign class accepts the current hull"end
    local blocks,weight,engines,weapons=counts(c)
    return string.format("%s (%s%s) | blocks=%d/%d-%d weight=%.1f/%.0f engines=%d/%d weapons=%d/%d cargo=%d crew=%d lot=%s",
        d.name,c.systems.vessel_class,c.systems.class_provisional and" provisional"or" certified",blocks,d.min_blocks,d.max_blocks,weight,d.max_weight,engines,d.max_engines,weapons,d.max_weapons,d.cargo,d.crew,d.lot)
end
function V.list_text(name)
    local out={};for _,key in ipairs(order)do local d=catalog[key];if not d.rank or navycraft.campaign.career.has_rank(name,d.rank)then out[#out+1]=string.format("%s=%s [%s %d-%d blocks]",key,d.name,d.rank or"recruit",d.min_blocks,d.max_blocks)end end
    return table.concat(out," | ")
end
return V

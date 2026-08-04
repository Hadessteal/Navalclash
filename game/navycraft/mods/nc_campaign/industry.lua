local I={}
local storage=core.get_mod_storage();local jobs={};local claims={};local next_id=0
local recipes={
    steel_ingot={name="Steel Smelting",time=8,inputs={iron_ore=3,coal=1},output="steel_ingot",count=2},
    copper_ingot={name="Copper Refining",time=7,inputs={copper_ore=2,coal=1},output="copper_ingot",count=2},
    fuel_drum={name="Fuel Refining",time=10,inputs={crude_oil=3,coal=1},output="fuel_drum",count=2},
    munitions_crate={name="Naval Munitions Packing",time=14,inputs={steel_ingot=1,coal=2,machinery_parts=1},output="munitions_crate",count=2},
    hull_plate={name="Hull Plate Rolling",time=12,inputs={steel_ingot=2},output="hull_plate",count=2},
    machinery_parts={name="Machinery Fabrication",time=15,inputs={steel_ingot=2,copper_ingot=1},output="machinery_parts",count=1},
    electronics={name="Marine Electronics",time=18,inputs={copper_ingot=2,salvage=1},output="electronics",count=1},
    engine_kit_t1={name="Tier I Engine Refit",time=30,rank="sailor",inputs={machinery_parts=4,electronics=2,fuel_drum=2},item="nc_campaign:engine_kit_t1",count=1},
    engine_kit_t2={name="Tier II Engine Refit",time=50,rank="officer",inputs={machinery_parts=7,electronics=4,fuel_drum=3},item="nc_campaign:engine_kit_t2",count=1},
    pump_kit_t1={name="Tier I Pump Refit",time=24,inputs={machinery_parts=3,electronics=1},item="nc_campaign:pump_kit_t1",count=1},
    pump_kit_t2={name="Tier II Pump Refit",time=42,rank="officer",inputs={machinery_parts=6,electronics=3},item="nc_campaign:pump_kit_t2",count=1},
    sensor_kit_t1={name="Tier I Sensor Refit",time=28,rank="sailor",inputs={electronics=4,copper_ingot=2},item="nc_campaign:sensor_kit_t1",count=1},
    sensor_kit_t2={name="Tier II Sensor Refit",time=48,rank="officer",inputs={electronics=8,copper_ingot=4},item="nc_campaign:sensor_kit_t2",count=1},
    fire_control_kit_t1={name="Tier I Fire-Control Refit",time=32,rank="petty_officer",inputs={electronics=5,machinery_parts=2},item="nc_campaign:fire_control_kit_t1",count=1},
    fire_control_kit_t2={name="Tier II Fire-Control Refit",time=58,rank="captain",inputs={electronics=10,machinery_parts=4},item="nc_campaign:fire_control_kit_t2",count=1},
    cargo_kit_t1={name="Tier I Cargo Handling Refit",time=24,inputs={hull_plate=2,machinery_parts=2},item="nc_campaign:cargo_kit_t1",count=1},
    cargo_kit_t2={name="Tier II Cargo Handling Refit",time=44,rank="officer",inputs={hull_plate=5,machinery_parts=5,electronics=2},item="nc_campaign:cargo_kit_t2",count=1},
    armor_kit_t1={name="Tier I Compartment Armour",time=34,inputs={hull_plate=6,machinery_parts=1},item="nc_campaign:armor_kit_t1",count=1},
    armor_kit_t2={name="Tier II Compartment Armour",time=62,rank="captain",inputs={hull_plate=14,machinery_parts=5,electronics=2},item="nc_campaign:armor_kit_t2",count=1},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("industry_v1",core.serialize({jobs=jobs,claims=claims,next_id=next_id}))end
local function load()local d=core.deserialize(storage:get_string("industry_v1"));if type(d)=="table"then jobs=d.jobs or{};claims=d.claims or{};next_id=d.next_id or 0 end end;load()
local function port_for(name)local p=navycraft.campaign.ports.for_player(name);if not p then return nil,"stand inside a registered port"end;if p.services and p.services.industry==false then return nil,"this port has no industrial service"end;return p end
local function commodity_item(key)local d=navycraft.campaign.markets.commodities()[key];return d and d.item end
function I.save()persist()end
function I.recipes()return recipes end
function I.queue(name,key,batches)
    local port,err=port_for(name);if not port then return false,err end;key=(key or""):lower();local r=recipes[key];if not r then return false,"unknown recipe"end
    if r.rank and not navycraft.campaign.career.has_rank(name,r.rank)then return false,"requires rank "..r.rank end
    if navycraft.campaign.supply then local available,_,message=navycraft.campaign.supply.service_status(port.id,"industry",batches or 1);if not available then return false,message end end
    batches=math.max(1,math.min(20,math.floor(tonumber(batches)or 1)));local inv=core.get_player_by_name(name):get_inventory()
    for input,count in pairs(r.inputs)do local item=commodity_item(input);if not item or not inv:contains_item("main",item.." "..count*batches)then return false,"missing "..input.." x"..count*batches end end
    for input,count in pairs(r.inputs)do inv:remove_item("main",commodity_item(input).." "..count*batches)end
    next_id=next_id+1;local id=tostring(next_id);jobs[id]={id=id,owner=name,port_id=port.id,recipe=key,batches=batches,remaining=r.time*batches,created=os.time(),status="queued"};if navycraft.campaign.supply then navycraft.campaign.supply.consume_service(port.id,"industry",math.max(.5,batches*.3),"factory job "..key)end;persist()
    return true,string.format("Queued %s x%d as job %s",r.name,batches,id),id
end
function I.step(dt)
    local completed=0;for _,job in pairs(jobs)do if job.status=="queued"then local rate=navycraft.campaign.supply and navycraft.campaign.supply.industry_multiplier(job.port_id)or 1;job.remaining=math.max(0,(job.remaining or 0)-(tonumber(dt)or 0)*rate);if job.remaining<=0 then
        local r=recipes[job.recipe];local item=r.item or commodity_item(r.output);local count=(r.count or 1)*job.batches
        claims[job.port_id]=claims[job.port_id]or{};claims[job.port_id][job.owner]=claims[job.port_id][job.owner]or{};local locker=claims[job.port_id][job.owner];locker[item]=(locker[item]or 0)+count
        job.status="complete";job.completed=os.time();completed=completed+1;navycraft.campaign.career.record_stat(job.owner,"manufactured",count)
    end end end;if completed>0 then persist()end;return completed
end
function I.collect(name)
    local port,err=port_for(name);if not port then return false,err end;local locker=claims[port.id]and claims[port.id][name];if not locker then return false,"no completed goods at this port"end
    local inv=core.get_player_by_name(name):get_inventory();local collected=0
    for item,count in pairs(copy(locker))do if count>0 then local leftover=inv:add_item("main",item.." "..count);local remaining=leftover and leftover.get_count and leftover:get_count()or 0;local moved=count-remaining;if moved>0 then collected=collected+moved;locker[item]=remaining end end end
    if collected==0 then return false,"inventory is full"end;persist();return true,"Collected "..collected.." manufactured item(s)"
end
function I.status(name)
    local out={};for _,job in pairs(jobs)do if job.owner==name then out[#out+1]=string.format("#%s %s x%d %s %.0fs",job.id,job.recipe,job.batches,job.status,job.remaining or 0)end end;table.sort(out);return#out>0 and table.concat(out," | ")or"no industry jobs"
end
function I.list_text(name)
    local out={};for key,r in pairs(recipes)do if not r.rank or navycraft.campaign.career.has_rank(name,r.rank)then local inputs={};for k,v in pairs(r.inputs)do inputs[#inputs+1]=k.."x"..v end;table.sort(inputs);out[#out+1]=string.format("%s [%s] -> x%d %.0fs",key,table.concat(inputs,"+"),r.count or 1,r.time)end end;table.sort(out);return table.concat(out," | ")
end
return I

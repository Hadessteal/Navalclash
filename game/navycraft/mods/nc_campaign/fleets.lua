local F={}
local storage=core.get_mod_storage()
local encounters={}
local next_id=1
local accumulator=0
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("fleets_v1",core.serialize({encounters=encounters,next_id=next_id}))end
local decoded=core.deserialize(storage:get_string("fleets_v1"));if type(decoded)=="table"then encounters=decoded.encounters or{};next_id=decoded.next_id or 1 end
local function atan2(y,x)return math.atan2 and math.atan2(y,x)or math.atan(y,x)end
local function normalise(a)while a>math.pi do a=a-math.pi*2 end;while a<-math.pi do a=a+math.pi*2 end;return a end
local function clamp(v,a,b)return math.max(a,math.min(b,v))end
local function node(nodes,name,x,y,z,extra)
    nodes[#nodes+1]={name=name,local_pos={x=x,y=y,z=z},param1=0,param2=0}
    if extra then for k,v in pairs(extra)do nodes[#nodes][k]=v end end
    return#nodes
end
local function generated_snapshot(faction,difficulty,class)
    difficulty=math.max(1,math.min(5,math.floor(tonumber(difficulty)or 1)))
    class=class or(difficulty>=3 and"gunboat"or"patrol")
    local freighter=class=="freighter"
    local raider=class=="raider"
    local escort=class=="escort"or class=="gunboat"
    local length=freighter and(7+difficulty)or(4+difficulty)
    local width=freighter and 2 or(difficulty>=4 and 2 or 1)
    local nodes={};local hull=(difficulty>=4 and not freighter)and"hull_armoured"or"hull_steel"
    for x=-width,width do for z=-length,length do node(nodes,"nc_navycraft:"..hull,x,0,z)end end
    for z=-math.max(1,length-2),math.max(1,length-2)do node(nodes,"nc_navycraft:hull_steel",0,1,z)end
    if freighter then for z=-length+2,length-2,2 do node(nodes,"nc_core:frame",-1,1,z);node(nodes,"nc_core:frame",1,1,z)end end
    local helm=node(nodes,"nc_navycraft:helm",0,2,-1)
    local engine=node(nodes,freighter and"nc_navycraft:engine_diesel_2"or"nc_navycraft:engine_diesel_2",0,1,-length+1)
    local cannon=nil
    if not freighter then cannon=node(nodes,"nc_navycraft:cannon",0,2,length-1,{param2=0})end
    local radar=node(nodes,"nc_navycraft:radar",0,3,0)
    local pump=node(nodes,"nc_navycraft:pump",width,1,-1)
    local count=#nodes;local weight=count*.72;local displacement=count+math.max(10,count*.25)
    local weapons={};if cannon then weapons[1]={node_index=cannon,type=difficulty>=4 and 1 or 0}end
    local profile={craft_type="ship",block_count=count,block_count_alive=count,weight=weight,weight_multiplier=1,block_disp_value=1,block_displacement=count,air_displacement=math.max(10,count*.25),full_ballast_displacement=0,displacement=displacement,pump_count=1,engines={{node_index=engine,key="diesel_2",enabled=true}},weapons=weapons,components={helm=1,radar=1,pump=1},bounds={}}
    local speed=freighter and 4.2 or(raider and 7.2 or(escort and 6.3 or 5.5))
    local systems={owner="",captain="",driver="",crew={},crew_history={},faction=faction,npc=true,npc_class=class,boarding="closed",throttle=.65,gear=1,rudder=0,vertical_planes=0,docking_mode=false,submerged_mode=false,ballast_mode=0,ballast_air_percent=100,buoyancy=1,displacement=displacement,last_displacement=displacement,block_displacement=count,air_displacement=profile.air_displacement,engines={[tostring(engine)]={key="diesel_2",set_on=true,is_on=true}},engines_on=true,top_speed=speed,radar_on=true,sonar_mode="off",launcher_on=false,radio_on=true,flooding=0,hull_integrity=1,helm_destroyed=false,sinking=false,pump_on=true,pump_charge=20,ammo={cannon_shell=freighter and 0 or 999,aa_round=999,torpedo_mk1=0,torpedo_mk2=0,torpedo_mk3=0,depth_charge=0,bomb=0,fireball_shell=0},selected_weapon=difficulty>=4 and 1 or 0,weapon_range=10,weapon_depth=0,fire_control_mode=freighter and"manual"or"auto",tdc_mode="straight",target_id=nil,last_weapon_fire=0,damagers={},waypoints={},current_waypoint=1,autotravel=false,navigation_mode="manual",stop_requested=false,is_auto_craft=true,cargo_manifest={}}
    return{nodes=nodes,profile=profile,systems=systems,indices={helm=helm,engine=engine,cannon=cannon,radar=radar,pump=pump}}
end
local function unit_by_construct(id)
    for _,e in pairs(encounters)do for _,u in ipairs(e.units or{})do if u.construct_id==id then return e,u end end end
end
local function encounter_live_position(encounter)
    if not encounter then return nil end
    for _,unit in ipairs(encounter.units or{})do if unit.status=="active"and unit.construct_id then local c=navycraft.preview.get_by_id(unit.construct_id);if c then return c.position end end end
    return encounter.centre
end
local function nearest_hostile(c,ignore_encounter)
    local own=navycraft.campaign.factions.construct_faction(c);local best,dist
    for _,other in pairs(navycraft.preview.get_all())do if other.id~=c.id and other.position and not(other.systems and other.systems.sinking)then
        local oe=unit_by_construct(other.id)
        if not ignore_encounter or not oe or oe.id~=ignore_encounter.id then
            local f=navycraft.campaign.factions.construct_faction(other)
            if navycraft.campaign.factions.hostile(own,f)then local d=vector.distance(c.position,other.position);if d<=350 and(not dist or d<dist)then best,dist=other,d end end
        end
    end end
    return best,dist
end
local function steer(c,target,speed)
    local delta=vector.subtract(target,c.position);local desired=atan2(delta.x,delta.z);local turn=normalise(desired-(c.yaw or 0))
    c.systems.rudder=clamp(turn/.6,-1,1);c.systems.throttle=clamp(speed or .5,0,1)
end
local function fire_at(c,target,distance)
    c.systems.target_id=target.id;c.systems.fire_control_mode="auto"
    if #(c.profile.weapons or{})>0 and distance<180 and os.time()-(c.systems.last_weapon_fire or 0)>=navycraft.definitions.source.weapon_timeout then
        navycraft.weapons.fire(c,c.systems.owner,c.systems.selected_weapon,{allow_virtual=true,free_ammo=true,target_id=target.id})
    end
end
function F.spawn(faction,centre,difficulty,options)
    options=options or{};if not navycraft.campaign.factions.get(faction)then return nil,"unknown faction"end
    local id=options.id or("enc-"..next_id);next_id=next_id+1;if encounters[id]and encounters[id].status=="active"then return nil,"encounter already active"end
    local unit_specs=options.units
    if not unit_specs then
        local count=math.max(1,math.min(4,math.floor(options.count or math.max(1,math.ceil((difficulty or 1)/2)))))
        unit_specs={};for _=1,count do unit_specs[#unit_specs+1]={class=options.class,role=options.role or"combat"}end
    end
    local encounter={id=id,faction=faction,difficulty=difficulty or 1,kind=options.kind or"patrol",centre=vector.new(centre),destination=options.destination and vector.new(options.destination)or nil,target_encounter_id=options.target_encounter_id,shipment_id=options.shipment_id,status="active",units={},created=os.time(),owner=options.owner,territory_id=options.territory_id}
    local count=#unit_specs
    for i,spec in ipairs(unit_specs)do
        local snapshot=generated_snapshot(faction,difficulty,spec.class);local runtime_owner=string.format("NPC:%s:%s:%d",faction,id,i)
        snapshot.systems.owner=runtime_owner;snapshot.systems.captain=runtime_owner;snapshot.systems.driver=runtime_owner;snapshot.systems.crew={[runtime_owner]="owner"};snapshot.systems.encounter_id=id;snapshot.systems.encounter_unit=i;snapshot.systems.encounter_role=spec.role or"combat";snapshot.systems.cargo_manifest=copy(spec.cargo or{})
        local angle=(i-1)*math.pi*2/math.max(1,count);local pos=vector.add(centre,{x=math.cos(angle)*12,y=0,z=math.sin(angle)*12})
        local construct_id,error_message=navycraft.preview.spawn_snapshot(runtime_owner,snapshot,pos,angle+math.pi)
        if construct_id then encounter.units[#encounter.units+1]={construct_id=construct_id,owner=runtime_owner,status="active",role=spec.role or"combat",class=spec.class or"patrol",spawned=os.time(),cargo=copy(spec.cargo or{})}else encounter.units[#encounter.units+1]={owner=runtime_owner,status="failed",role=spec.role or"combat",class=spec.class,error=error_message}end
    end
    encounters[id]=encounter;persist();return copy(encounter)
end
function F.spawn_for_player(name,faction,position,difficulty,options)options=options or{};options.owner=name;return F.spawn(faction,position,difficulty,options)end
function F.get(id)return encounters[id]and copy(encounters[id])or nil end
function F.all()return copy(encounters)end
local function defeat(encounter,unit,construct,attacker)
    if unit.status~="active"then return end;unit.status="destroyed";unit.destroyed=os.time();unit.attacker=attacker
    local winner=attacker and navycraft.campaign.factions.player_faction(attacker)or"navy";navycraft.campaign.territories.record_victory(construct.position,winner,encounter.faction,10+encounter.difficulty*5)
    navycraft.preview.remove(construct.id,false)
end
function F.on_damage(construct,removed,position,attacker)
    local encounter,unit=unit_by_construct(construct and construct.id);if not encounter or not unit then return end
    unit.damage=(unit.damage or 0)+(tonumber(removed)or 0);unit.last_attacker=attacker
    if construct.systems and((construct.systems.hull_integrity or 1)<=.28 or construct.systems.helm_destroyed or construct.systems.sinking)then defeat(encounter,unit,construct,attacker)end
end
local function convoy_step(encounter,unit,c)
    local target,distance=nearest_hostile(c,encounter)
    if unit.role=="freighter"then
        if encounter.destination then steer(c,encounter.destination,target and .9 or .7)end
        if target and distance<70 then local away=vector.add(c.position,vector.multiply(vector.direction(target.position,c.position),80));steer(c,away,1)end
        if encounter.destination and vector.distance(c.position,encounter.destination)<=12 then encounter.status="arrived";encounter.arrived=os.time()end
    else
        if target then steer(c,target.position,distance<35 and .35 or .8);fire_at(c,target,distance)
        else
            local lead
            for _,other in ipairs(encounter.units or{})do if other.role=="freighter"and other.status=="active"then lead=other.construct_id and navycraft.preview.get_by_id(other.construct_id);break end end
            if lead then steer(c,vector.add(lead.position,{x=unit.role=="escort"and 8 or 0,y=0,z=-8}),.7)elseif encounter.destination then steer(c,encounter.destination,.7)end
        end
    end
end
local function piracy_step(encounter,c)
    local target,distance=nearest_hostile(c,encounter)
    if target then steer(c,target.position,distance<30 and .3 or .95);fire_at(c,target,distance);return end
    local target_encounter=encounters[encounter.target_encounter_id];local target_pos=encounter_live_position(target_encounter)
    if target_pos then encounter.centre=vector.new(target_pos);steer(c,target_pos,.95)else steer(c,encounter.centre,.4)end
end
local function patrol_step(encounter,c)
    local target,distance=nearest_hostile(c,encounter)
    if target then steer(c,target.position,distance<35 and .25 or .75);fire_at(c,target,distance)
    else local delta=vector.subtract(encounter.centre,c.position);if vector.length(delta)>50 then steer(c,encounter.centre,.45)else c.systems.rudder=.2;c.systems.throttle=.2 end end
end
function F.step(dt)
    accumulator=accumulator+math.max(0,tonumber(dt)or 0);if accumulator<.5 then return end;accumulator=0
    for _,encounter in pairs(encounters)do if encounter.status=="active"then
        local active=0
        for _,unit in ipairs(encounter.units or{})do if unit.status=="active"then
            local c=navycraft.preview.get_by_id(unit.construct_id)
            if not c then unit.status="lost"else active=active+1
                if c.systems and((c.systems.hull_integrity or 1)<=.28 or c.systems.sinking)then defeat(encounter,unit,c,unit.last_attacker)
                elseif encounter.kind=="convoy"then convoy_step(encounter,unit,c)
                elseif encounter.kind=="piracy"then piracy_step(encounter,c)
                else patrol_step(encounter,c)end
            end
        end end
        if encounter.status=="arrived"then
            for _,unit in ipairs(encounter.units or{})do if unit.status=="active"then unit.status="arrived";if unit.construct_id then navycraft.preview.remove(unit.construct_id,false)end end end
        elseif active==0 then encounter.status="complete";encounter.completed=os.time();if navycraft.campaign.operations then navycraft.campaign.operations.on_encounter_complete(encounter)end end
    end end
    persist()
end
function F.status(id)
    local e=encounters[id];if not e then return"encounter not found"end;local active,dead=0,0
    for _,u in ipairs(e.units or{})do if u.status=="active"then active=active+1 elseif u.status=="destroyed"then dead=dead+1 end end
    return string.format("%s faction=%s kind=%s difficulty=%d status=%s active=%d defeated=%d",e.id,e.faction,e.kind,e.difficulty,e.status,active,dead)
end
function F.list_text()local out={};for id in pairs(encounters)do out[#out+1]=F.status(id)end;table.sort(out);return#out>0 and table.concat(out," | ")or"no encounters"end
function F.clear(id)local e=encounters[id];if not e then return false,"encounter not found"end;for _,u in ipairs(e.units or{})do if u.construct_id then navycraft.preview.remove(u.construct_id,false)end end;e.status="cleared";persist();return true,"encounter cleared"end
function F.save()persist()end
return F

local O={}
local storage=core.get_mod_storage();local progress={}
local chain={id="frontier_watch",title="Frontier Watch",stages={
    [1]={key="recon",kind="operation_recon",title="Chart the Frontier",rank="sailor",credits=320,xp=180,rep=20},
    [2]={key="intercept",kind="operation_battle",title="Break the Raider Screen",rank="sailor",credits=520,xp=300,rep=35},
    [3]={key="secure",kind="operation_control",title="Secure the Anchorage",rank="petty_officer",credits=750,xp=450,rep=50},
}}
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("operations_v1",core.serialize(progress))end
local decoded=core.deserialize(storage:get_string("operations_v1"));if type(decoded)=="table"then progress=decoded end
local function state(name)
    progress[name]=progress[name]or{chain=chain.id,stage=1,completed=false,history={}}
    return progress[name]
end
local function destination_for(origin)
    local best,dist;local p=navycraft.campaign.ports.get(origin);if not p then return nil end
    for id,other in pairs(navycraft.campaign.ports.all())do if id~=origin then local d=vector.distance(p.pos,other.pos);if not dist or d>dist then best,dist=id,d end end end
    return best
end
local function stage_offer(name,port_id)
    local s=state(name);if s.completed then return nil end;local def=chain.stages[s.stage];if not def then return nil end
    local destination=destination_for(port_id);if not destination then return nil end
    local target=navycraft.campaign.territories.ensure(destination)
    if def.kind=="operation_recon"then return{id=string.format("operation:%s:%d:%s",chain.id,s.stage,port_id),kind=def.kind,title=def.title,description="Enter the waters around "..target.name..", identify local control, then return to "..navycraft.campaign.ports.get(port_id).name,origin=port_id,destination=destination,target=2,reward={credits=def.credits,xp=def.xp,reputation={navy=def.rep}},required_rank=def.rank,operation=chain.id,operation_stage=s.stage}
    elseif def.kind=="operation_battle"then return{id=string.format("operation:%s:%d:%s",chain.id,s.stage,port_id),kind=def.kind,title=def.title,description="Intercept and defeat the corsair patrol screening "..target.name,origin=port_id,destination=destination,target=1,reward={credits=def.credits,xp=def.xp,reputation={navy=def.rep}},required_rank=def.rank,operation=chain.id,operation_stage=s.stage,difficulty=2}
    else return{id=string.format("operation:%s:%d:%s",chain.id,s.stage,port_id),kind=def.kind,title=def.title,description="Establish Commonwealth control over "..target.name.." and hold the waters for 20 seconds",origin=port_id,destination=destination,target=20,reward={credits=def.credits,xp=def.xp,reputation={navy=def.rep}},required_rank=def.rank,operation=chain.id,operation_stage=s.stage}end
end
function O.offers_for(port_id,name)local o=stage_offer(name,port_id);return o and{o}or{}end
function O.on_accept(name,mission)
    if not mission.operation then return true end
    local s=state(name);if mission.operation~=chain.id or mission.operation_stage~=s.stage then return false,"operation stage is no longer current"end
    mission.operation_key=chain.stages[s.stage].key
    if mission.kind=="operation_battle"then
        local territory=navycraft.campaign.territories.get(mission.destination);local encounter,error_message=navycraft.campaign.fleets.spawn_for_player(name,"corsair",vector.add(territory.pos,{x=90,y=0,z=40}),mission.difficulty or 2,{kind="intercept",territory_id=territory.id,count=2})
        if not encounter then return false,error_message end;mission.encounter_id=encounter.id;mission.description=mission.description.." [encounter "..encounter.id.."]"
    elseif mission.kind=="operation_control"then mission.hold_time=0 end
    persist();return true
end
local function at_territory(construct,id)local t=navycraft.campaign.territories.get(id);return t and vector.distance(construct.position,t.pos)<=t.radius,t end
function O.update_mission(name,mission,construct,dt,finish)
    if not mission.operation then return false end
    if mission.kind=="operation_recon"then
        local inside,t=at_territory(construct,mission.destination)
        if mission.stage==1 and inside then mission.stage=2;mission.progress=1;mission.observed_owner=t.owner;core.chat_send_player(name,"Frontier contact recorded: "..t.name.." is controlled by "..t.owner..". Return to base.")
        elseif mission.stage==2 then local port=navycraft.campaign.ports.get(mission.origin);if port and vector.distance(construct.position,port.pos)<=port.radius then mission.progress=2;finish(name,construct)end end
    elseif mission.kind=="operation_battle"then
        local e=navycraft.campaign.fleets.get(mission.encounter_id);if e and e.status=="complete"then mission.progress=1;finish(name,construct)end
    elseif mission.kind=="operation_control"then
        local inside,t=at_territory(construct,mission.destination)
        local faction=navycraft.campaign.factions.player_faction(name)
        if inside then
            navycraft.campaign.territories.contest(mission.destination,faction,math.max(.5,(dt or 0)*2.5))
            t=navycraft.campaign.territories.get(mission.destination)
            if t.owner==faction then mission.hold_time=(mission.hold_time or 0)+(dt or 0);mission.progress=mission.hold_time else mission.hold_time=0;mission.progress=0 end
            if mission.hold_time>=mission.target then finish(name,construct)end
        else mission.hold_time=math.max(0,(mission.hold_time or 0)-(dt or 0));mission.progress=mission.hold_time end
    end
    return true
end
function O.on_complete(name,mission)
    if not mission.operation then return end
    local s=state(name);s.history[#s.history+1]={stage=s.stage,key=mission.operation_key,completed=os.time()};s.stage=s.stage+1
    if not chain.stages[s.stage]then s.completed=true;s.completed_at=os.time();navycraft.campaign.career.award(name,{credits=500,xp=250,reputation={navy=25}},"operation completion bonus");core.chat_send_player(name,"Operation Frontier Watch complete. Campaign bonus awarded.")else core.chat_send_player(name,"Operation Frontier Watch advanced to stage "..s.stage..": "..chain.stages[s.stage].title)end
    persist()
end
function O.on_abandon(name,mission)if mission and mission.encounter_id then navycraft.campaign.fleets.clear(mission.encounter_id)end end
function O.on_encounter_complete(encounter)
    if not encounter.owner then return end
    local mission=navycraft.campaign.missions and navycraft.campaign.missions.active(encounter.owner)
    if mission and mission.encounter_id==encounter.id then core.chat_send_player(encounter.owner,"Enemy patrol destroyed. Mission objective complete; remain with your vessel for debrief.")end
end
function O.status(name)
    local s=state(name);if s.completed then return"Frontier Watch COMPLETE"end;local def=chain.stages[s.stage];return string.format("%s stage %d/3: %s",chain.title,s.stage,def.title)
end
function O.mission_status(mission)
    if mission.kind=="operation_recon"then return"stage "..tostring(mission.stage).."/2"
    elseif mission.kind=="operation_battle"then local e=navycraft.campaign.fleets.get(mission.encounter_id);return e and navycraft.campaign.fleets.status(e.id)or"encounter missing"
    elseif mission.kind=="operation_control"then return string.format("hold %.1f/%.1fs",mission.hold_time or 0,mission.target or 0)end
end
function O.reset(name)progress[name]={chain=chain.id,stage=1,completed=false,history={}};persist();return true,"operation progress reset"end
function O.save()persist()end
return O

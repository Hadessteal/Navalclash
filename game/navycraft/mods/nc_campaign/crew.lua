local R={}
local roles={
    captain={display="Captain",weight=1.5,stations={all=true}},
    executive={display="Executive Officer",weight=1.3,stations={all=true}},
    helm={display="Helm",weight=1.15,stations={helm=true,navigation=true}},
    engineer={display="Engineer",weight=1.15,stations={engine=true,fluids=true,repair=true}},
    gunner={display="Gunner",weight=1.15,stations={weapons=true,fire_control=true}},
    sensor={display="Sensor Operator",weight=1.05,stations={sensors=true,fire_control=true}},
    quartermaster={display="Quartermaster",weight=1.05,stations={economy=true,inventory=true}},
    deck={display="Deck Crew",weight=1.0,stations={deck=true}},
    passenger={display="Passenger",weight=.25,stations={}},
}
local function name_of(player)return type(player)=="string"and player or(player and player:get_player_name())end
local function commander(c,name)return c and c.systems and(name==c.systems.owner or name==c.systems.captain)end
function R.roles()return roles end
function R.assign(construct,actor,target,role)
    actor=name_of(actor);target=name_of(target);role=(role or""):lower()
    if not commander(construct,actor)then return false,"only the owner or captain may assign stations"end
    if not roles[role]then return false,"unknown station role"end
    construct.systems.station_roles=construct.systems.station_roles or{}
    construct.systems.station_roles[target]=role
    if role~="passenger"and target~=construct.systems.owner then navycraft.systems.add_crew(construct,target,"crew")end
    navycraft.preview.save();return true,target.." assigned as "..roles[role].display
end
function R.role(construct,name)
    name=name_of(name);if not construct or not construct.systems then return nil end
    if name==construct.systems.owner or name==construct.systems.captain then return"captain"end
    return(construct.systems.station_roles or{})[name]or((construct.systems.crew or{})[name]and"deck")or nil
end
function R.can_operate(construct,name,station)
    local role=R.role(construct,name);if not role then return false end
    local def=roles[role];return def and(def.stations.all or def.stations[station])or false
end
function R.record(construct,name,points,station)
    name=name_of(name);if not name or not R.role(construct,name)then return end
    construct.systems.campaign_contributions=construct.systems.campaign_contributions or{}
    local e=construct.systems.campaign_contributions[name]or{points=0,stations={}}
    e.points=e.points+(tonumber(points)or 0);e.stations[station or"general"]=(e.stations[station or"general"]or 0)+(tonumber(points)or 0)
    construct.systems.campaign_contributions[name]=e
end
function R.step(construct,dt)
    if not construct.systems then return end
    local last=construct._campaign_crew_last_pos
    if last then
        local distance=vector.distance(last,construct.position)
        if distance>0 and distance<50 then
            local driver=construct.systems.driver or construct.systems.captain or construct.owner
            R.record(construct,driver,distance,"helm")
        end
    end
    construct._campaign_crew_last_pos=vector.new(construct.position)
end
function R.roster(construct)
    if not construct then return"no active vessel"end
    local names={};local seen={}
    for name in pairs(construct.systems.crew or{})do seen[name]=true end;seen[construct.systems.owner]=true;if construct.systems.captain then seen[construct.systems.captain]=true end
    for name in pairs(seen)do local role=R.role(construct,name)or"passenger";local points=((construct.systems.campaign_contributions or{})[name]or{}).points or 0;names[#names+1]=string.format("%s:%s(%.0f)",name,role,points)end
    table.sort(names);return table.concat(names," | ")
end
function R.reward_crew(construct,mission_owner,credits,xp,reason)
    if not construct or not construct.systems then return{}end
    local candidates={};local total=0
    for name,entry in pairs(construct.systems.campaign_contributions or{})do
        if name~=mission_owner and R.role(construct,name)then
            local role=R.role(construct,name);local weight=(roles[role]and roles[role].weight or 1)*math.max(1,math.sqrt(math.max(0,entry.points or 0)))
            candidates[#candidates+1]={name=name,weight=weight};total=total+weight
        end
    end
    local paid={};local pool=math.floor((credits or 0)*.25);local xp_pool=math.floor((xp or 0)*.25)
    if total>0 then for _,e in ipairs(candidates)do
        local c=math.max(1,math.floor(pool*e.weight/total));local x=math.max(1,math.floor(xp_pool*e.weight/total))
        navycraft.campaign.career.award(e.name,{credits=c,xp=x},reason.." crew share");paid[#paid+1]={name=e.name,credits=c,xp=x}
    end end
    construct.systems.campaign_contributions={};return paid
end
return R

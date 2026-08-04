local C={states={}}
local function disabled(c)
    local s=c and c.systems or{};return c and math.abs(c.forward_speed or 0)<=.5 and math.abs(c.vertical_speed or 0)<=.5 and((s.hull_integrity or 1)<=.5 or s.helm_destroyed or s.abandoned or s.captain_abandoned)
end
local function nearest_target(name)
    local p=core.get_player_by_name(name);if not p then return nil end;local best,dist
    for _,c in pairs(navycraft.preview.get_all())do local d=vector.distance(p:get_pos(),c.position);if d<=12 and(not dist or d<dist)then best,dist=c,d end end;return best
end
local function defenders_near(c,attacker)
    for _,p in pairs(core.get_connected_players and core.get_connected_players()or{})do local n=p:get_player_name();if n~=attacker and c.systems and(n==c.systems.owner or n==c.systems.captain or(c.systems.crew or{})[n])and vector.distance(p:get_pos(),c.position)<=12 then return true end end
    return false
end
function C.start(name)
    local c=nearest_target(name);if not c then return false,"no vessel within boarding range"end;local owner=c.systems and c.systems.owner or c.owner;if owner==name then return false,"you already own this vessel"end
    if not disabled(c)then return false,"vessel must be disabled, abandoned, or below 50% hull"end
    local af=navycraft.campaign.factions.player_faction(name);local df=navycraft.campaign.factions.construct_faction(c)
    if owner and owner~=""and not c.systems.abandoned and not navycraft.campaign.factions.hostile(af,df)then return false,"capture is allowed only against hostile or abandoned vessels"end
    C.states[c.id]={construct_id=c.id,attacker=name,old_owner=owner,progress=0,required=20,started=os.time(),status="boarding"};c.systems.capture_state=C.states[c.id]
    return true,"Boarding action started on "..c.id
end
function C.abort(name)
    for id,state in pairs(C.states)do if state.attacker==name then local c=navycraft.preview.get_by_id(id);if c and c.systems then c.systems.capture_state=nil end;C.states[id]=nil;return true,"Capture aborted"end end;return false,"no capture in progress"
end
function C.status(name)
    for _,s in pairs(C.states)do if s.attacker==name then return string.format("capture %s progress=%.1f/%ds status=%s",s.construct_id,s.progress,s.required,s.status)end end;return"no capture in progress"
end
function C.step(dt)
    for id,state in pairs(C.states)do
        local c=navycraft.preview.get_by_id(id);local player=core.get_player_by_name(state.attacker)
        if not c or not player then C.states[id]=nil
        elseif vector.distance(player:get_pos(),c.position)>12 or not disabled(c)then state.progress=math.max(0,state.progress-dt*2);state.status="interrupted"
        elseif defenders_near(c,state.attacker)then state.progress=math.max(0,state.progress-dt);state.status="contested"
        else
            state.status="boarding";state.progress=state.progress+dt
            if state.progress>=state.required then
                local old_owner=state.old_owner;local snapshot=navycraft.preview.snapshot(c)
                navycraft.campaign.ownership.record_loss(c,"capture",old_owner,snapshot)
                local ok,result=navycraft.campaign.ownership.transfer_live(c,state.attacker,"combat capture")
                if ok then
                    c.systems.faction=navycraft.campaign.factions.player_faction(state.attacker);c.systems.captured_from=old_owner;c.systems.captured_at=os.time();c.systems.capture_state=nil;c.systems.loss_recorded=nil
                    navycraft.campaign.career.record_stat(state.attacker,"vessels_captured",1);navycraft.campaign.career.add_reputation(state.attacker,"salvage",5)
                    core.chat_send_player(state.attacker,"Capture complete: title transferred from "..tostring(old_owner))
                end
                C.states[id]=nil
            end
        end
        if c and c.systems and C.states[id]then c.systems.capture_state=C.states[id]end
    end
end
return C

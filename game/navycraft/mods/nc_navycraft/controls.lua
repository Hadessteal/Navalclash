local S=navycraft.systems
local C={}
local last_controls={}
local last_helm_click={}

local function save() if navycraft.preview then navycraft.preview.save() end end

local function controlled_craft(player)
    local name=player:get_player_name()
    local direct=navycraft.preview.get_for_owner(name)
    local seated=navycraft.preview.player_helm_construct and navycraft.preview.player_helm_construct(player)
    if seated and seated.systems and seated.systems.driver==name and S.authorized(seated,name,"crew") then
        return seated
    end
    if direct and direct.systems and direct.systems.driver==name and not seated then
        if navycraft.preview.clear_passenger then navycraft.preview.clear_passenger(name) end
        last_controls[name]=nil
    end
    return nil
end

local function edge(controls,last,key)
    return controls[key] and not (last and last[key])
end

local function place_edge(controls,last)
    return edge(controls,last,"place") or edge(controls,last,"RMB")
end

local function release_click_ready(name, now)
    local previous = last_helm_click[name]
    if not previous or previous.state ~= "engaged" then return true end
    return now - previous.time >= 0.45
end

local function send(player,message)
    if message and message~="" then core.chat_send_player(player:get_player_name(),message) end
end

local function run_order(player,craft,fn)
    local ok,message=fn(craft)
    send(player,message)
    if ok then save() end
end

function C.take_helm(construct,player,node_index)
    local name=player:get_player_name()
    local now=os.clock()
    local previous=last_helm_click[name]
    local seated=navycraft.preview.player_helm_construct and navycraft.preview.player_helm_construct(player)==construct
    if previous and previous.construct_id==construct.id and now-previous.time<0.25
            and ((seated and previous.state=="engaged") or (not seated and previous.state=="released")) then
        return true,""
    end
    if seated then
        local ok,message=navycraft.preview.release_helm(player)
        if ok and construct.systems and construct.systems.driver==name then construct.systems.driver=nil end
        last_helm_click[name]={construct_id=construct.id,time=now,state="released"}
        save()
        send(player,message or "Helm released")
        return ok,message
    end
    local ok,message=S.take_helm(construct,name)
    if ok and navycraft.preview.take_helm then
        local seated,seat_message=navycraft.preview.take_helm(player,construct,node_index)
        if not seated then
            construct.systems.driver=nil
            ok=false
            message=seat_message or "helm seat unavailable"
        else
            message=seat_message or message
        end
    end
    if ok then
        last_helm_click[name]={construct_id=construct.id,time=now,state="engaged"}
        last_controls[name]=last_controls[name] or {}
        last_controls[name].place=true
        last_controls[name].RMB=true
    end
    send(player,message)
    if ok then save() end
    return ok,message
end

local function set_forward(c)
    local s=c.systems
    if (s.gear or 1)<0 then
        if (s.set_speed or 0)>0 then return S.speed_change(c,false) end
        S.set_gear(c,1)
    elseif (s.gear or 1)==0 then
        S.set_gear(c,1)
    end
    return S.speed_change(c,true)
end

local function set_reverse(c)
    local s=c.systems
    if (s.gear or 1)>0 then
        if (s.set_speed or 0)>0 then return S.speed_change(c,false) end
        S.set_gear(c,-1)
    elseif (s.gear or 1)==0 then
        S.set_gear(c,-1)
    end
    return S.speed_change(c,true)
end

core.register_globalstep(function()
    for _,player in ipairs(core.get_connected_players()) do
        local name=player:get_player_name()
        local controls=player:get_player_control() or {}
        local last=last_controls[name] or {}
        local craft=controlled_craft(player)
        if craft then
            local now=os.clock()
            if place_edge(controls,last) and release_click_ready(name, now) then
                local ok,message=navycraft.preview.release_helm(player)
                if ok and craft.systems and craft.systems.driver==name then craft.systems.driver=nil end
                last_helm_click[name]={construct_id=craft.id,time=now,state="released"}
                send(player,message or "Helm released")
                if ok then save() end
            elseif edge(controls,last,"up") then
                run_order(player,craft,set_forward)
            elseif edge(controls,last,"down") then
                run_order(player,craft,set_reverse)
            elseif edge(controls,last,"right") then
                run_order(player,craft,function(c) return S.rudder_order(c,1,true) end)
            elseif edge(controls,last,"left") then
                run_order(player,craft,function(c) return S.rudder_order(c,-1,true) end)
            end
        end
        last_controls[name]={up=controls.up,down=controls.down,left=controls.left,
            right=controls.right,sneak=controls.sneak,place=controls.place,RMB=controls.RMB}
    end
end)

core.register_on_leaveplayer(function(player)
    local name=player:get_player_name()
    last_controls[name]=nil
    last_helm_click[name]=nil
end)

return C

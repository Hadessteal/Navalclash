local S=navycraft.systems
local C={}
local last_controls={}

local function save() if navycraft.preview then navycraft.preview.save() end end

local function controlled_craft(player)
    local name=player:get_player_name()
    local direct=navycraft.preview.get_for_owner(name)
    if direct and direct.systems and direct.systems.driver==name and S.authorized(direct,name,"crew") then return direct end
    return navycraft.preview.find_nearest(player:get_pos(),48,function(c)
        return c.systems and c.systems.driver==name and S.authorized(c,name,"crew")
    end)
end

local function edge(controls,last,key)
    return controls[key] and not (last and last[key])
end

local function send(player,message)
    if message and message~="" then core.chat_send_player(player:get_player_name(),message) end
end

local function run_order(player,craft,fn)
    local ok,message=fn(craft)
    send(player,message)
    if ok then save() end
end

function C.take_helm(construct,player)
    local ok,message=S.take_helm(construct,player:get_player_name())
    send(player,message)
    if ok then save() end
    return ok,message
end

core.register_globalstep(function()
    for _,player in ipairs(core.get_connected_players()) do
        local name=player:get_player_name()
        local controls=player:get_player_control() or {}
        local last=last_controls[name] or {}
        local craft=controlled_craft(player)
        if craft then
            if edge(controls,last,"up") then
                if controls.sneak then
                    run_order(player,craft,function(c) return S.gear_change(c,true) end)
                else
                    run_order(player,craft,function(c) return S.speed_change(c,true) end)
                end
            elseif edge(controls,last,"down") then
                if controls.sneak then
                    run_order(player,craft,function(c) return S.gear_change(c,false) end)
                else
                    run_order(player,craft,function(c) return S.speed_change(c,false) end)
                end
            elseif edge(controls,last,"right") then
                run_order(player,craft,function(c) return S.rudder_order(c,1,true) end)
            elseif edge(controls,last,"left") then
                run_order(player,craft,function(c) return S.rudder_order(c,-1,true) end)
            end
        end
        last_controls[name]={up=controls.up,down=controls.down,left=controls.left,
            right=controls.right,sneak=controls.sneak}
    end
end)

core.register_on_leaveplayer(function(player)
    last_controls[player:get_player_name()]=nil
end)

return C

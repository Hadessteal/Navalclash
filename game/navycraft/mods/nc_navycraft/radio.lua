-- Four-channel vessel radio and crew chat based on Craft.radioChannels,
-- radioChannelSelector and radioSetOn.
local R={}
local function aboard_or_crew(construct,name)
    if construct.systems and (construct.systems.owner==name or construct.systems.captain==name or construct.systems.crew[name]) then return true end
    local player=core.get_player_by_name(name);if not player then return false end
    return vector.distance(player:get_pos(),construct.position)<=math.max(12,(construct.bounds and construct.bounds.radius or 8)+4)
end
local function listeners(construct)
    local names={};for _,player in ipairs(core.get_connected_players()) do if aboard_or_crew(construct,player:get_player_name()) then names[player:get_player_name()]=true end end
    return names
end
function R.set_channel(construct,slot,channel)
    slot=math.max(1,math.min(4,math.floor(tonumber(slot) or 1)))
    channel=math.max(0,math.min(9999,math.floor(tonumber(channel) or 0)))
    construct.systems.radio_channels[slot]=channel;construct.systems.radio_selector=slot
    navycraft.preview.save();return true,string.format("Radio %d tuned to %04d",slot,channel)
end
function R.cycle(construct)
    construct.systems.radio_selector=(construct.systems.radio_selector or 1)%4+1
    return construct.systems.radio_selector,construct.systems.radio_channels[construct.systems.radio_selector]
end
function R.send(construct,sender,message)
    if not construct.systems.radio_on then return false,"radio is off" end
    local slot=construct.systems.radio_selector or 1;local channel=construct.systems.radio_channels[slot] or 0
    if channel==0 then return false,"selected radio channel is unset" end
    local delivered={}
    for _,other in pairs(navycraft.preview.get_all()) do
        if other.systems and other.systems.radio_on then
            local matched=false
            for _,value in ipairs(other.systems.radio_channels or {}) do if value==channel then matched=true;break end end
            if matched then for name,_ in pairs(listeners(other)) do delivered[name]=true end end
        end
    end
    local line=string.format("[Radio %04d][%s] %s",channel,sender,message)
    for name,_ in pairs(delivered) do core.chat_send_player(name,line) end
    return true,string.format("Radio message delivered to %d player(s)",(function()local n=0;for _ in pairs(delivered) do n=n+1 end;return n end)())
end
function R.crew(construct,sender,message)
    local delivered={}
    for name,_ in pairs(construct.systems.crew or {}) do if core.get_player_by_name(name) then delivered[name]=true end end
    if construct.systems.owner then delivered[construct.systems.owner]=true end
    if construct.systems.captain then delivered[construct.systems.captain]=true end
    for name,_ in pairs(delivered) do core.chat_send_player(name,string.format("[Crew:%s][%s] %s",construct.systems.custom_name or construct.id,sender,message)) end
    return true
end
return R

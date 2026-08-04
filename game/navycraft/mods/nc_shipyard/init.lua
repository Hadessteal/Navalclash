-- Native Luanti implementation of the supplied NavyCraft-Shipyard 3.1.5 class.
-- Original commands/lot labels are preserved. WorldGuard regions and PermissionsEx
-- permission nodes are replaced with persistent Luanti plot boxes and rank records.
local D=navycraft.definitions
local Y={}
navycraft.shipyard=Y
local storage=core.get_mod_storage()
local plots={}
local rewards={}
local players={}
local plot_positions={}
local MAX_CLEAR_VOLUME=300000

local function copy(v) return core.deserialize(core.serialize(v)) end
local function safe_id(v) return (v or ""):gsub("[^%w_%-]",""):sub(1,32) end
local function safe_name(v) return (v or ""):gsub("[^%w _%-]",""):gsub("^%s+",""):gsub("%s+$",""):sub(1,40) end
local function pos_min(a,b) return {x=math.min(a.x,b.x),y=math.min(a.y,b.y),z=math.min(a.z,b.z)} end
local function pos_max(a,b) return {x=math.max(a.x,b.x),y=math.max(a.y,b.y),z=math.max(a.z,b.z)} end
local function inside(pos,plot) return pos.x>=plot.minp.x and pos.x<=plot.maxp.x and pos.y>=plot.minp.y and pos.y<=plot.maxp.y and pos.z>=plot.minp.z and pos.z<=plot.maxp.z end
local function volume(plot) return (plot.maxp.x-plot.minp.x+1)*(plot.maxp.y-plot.minp.y+1)*(plot.maxp.z-plot.minp.z+1) end
local function centre(plot) return {x=(plot.minp.x+plot.maxp.x)/2,y=plot.minp.y+1,z=(plot.minp.z+plot.maxp.z)/2} end
local function persist()
    storage:set_string("shipyard_plots_v1",core.serialize(plots))
    storage:set_string("shipyard_rewards_v1",core.serialize(rewards))
    storage:set_string("shipyard_players_v1",core.serialize(players))
end
local function load()
    local value=core.deserialize(storage:get_string("shipyard_plots_v1"));if type(value)=="table" then plots=value end
    value=core.deserialize(storage:get_string("shipyard_rewards_v1"));if type(value)=="table" then rewards=value end
    value=core.deserialize(storage:get_string("shipyard_players_v1"));if type(value)=="table" then players=value end
end
load()

local function canonical_type(lot_type)
    lot_type=(lot_type or ""):upper();lot_type=D.lot_aliases[lot_type] or lot_type
    return D.lots[lot_type] and lot_type or nil
end
local function ensure_player(name)
    players[name]=players[name] or {experience=0,credits=0,last_payday=0,joined=os.time(),ledger={}}
    players[name].ledger=players[name].ledger or {}
    rewards[name]=rewards[name] or {}
    return players[name]
end
local function owned_plots(name)
    local out={};for _,plot in pairs(plots) do if plot.owner==name then out[#out+1]=plot end end
    table.sort(out,function(a,b)return a.id<b.id end);return out
end
local function resolve_plot(name,token)
    token=token or ""
    if plots[token] then return plots[token] end
    local owned=owned_plots(name);local index=tonumber(token)
    if index and owned[index] then return owned[index] end
    for _,plot in ipairs(owned) do if plot.name and plot.name:lower()==token:lower() then return plot end end
    return nil
end
local function may_manage(plot,name)
    return plot and (plot.owner==name or core.check_player_privs(name,{server=true}))
end
local function rank_for(exp)
    local selected=D.rank_defaults[1]
    for _,rank in ipairs(D.rank_defaults) do if exp>=rank.exp then selected=rank end end
    return selected
end

function Y.find_plot_at(pos)
    for _,plot in pairs(plots) do if inside(pos,plot) then return plot end end
    return nil
end
function Y.get_plot(id) return plots[id] end
function Y.can_build(name,pos)
    local plot=Y.find_plot_at(pos);if not plot then return true end
    return plot.owner==name or plot.public or (plot.members and plot.members[name]) or core.check_player_privs(name,{server=true})
end
function Y.create_plot(id,lot_type,minp,maxp,creator)
    id=safe_id(id);lot_type=canonical_type(lot_type)
    if id=="" then return false,"plot id required" end
    if not lot_type then return false,"unknown lot type" end
    if plots[id] then return false,"plot id already exists" end
    minp=vector.round(minp);maxp=vector.round(maxp)
    local plot={id=id,type=lot_type,name=id,owner=nil,members={},public=false,minp=pos_min(minp,maxp),maxp=pos_max(minp,maxp),created_by=creator,created=os.time()}
    if volume(plot)>MAX_CLEAR_VOLUME then return false,"plot volume exceeds safety limit" end
    plot.spawn_pos=centre(plot);plots[id]=plot;persist();return true,plot
end
function Y.delete_plot(id) if not plots[id] then return false,"plot not found" end;plots[id]=nil;persist();return true,"plot deleted" end
function Y.reward(player_name,lot_type,count)
    lot_type=canonical_type(lot_type);if not lot_type then return false,"unknown lot type" end
    count=math.floor(tonumber(count) or 1);rewards[player_name]=rewards[player_name] or {}
    rewards[player_name][lot_type]=math.max(0,(rewards[player_name][lot_type] or 0)+count);persist()
    return true,string.format("%s has %d %s reward(s)",player_name,rewards[player_name][lot_type],lot_type)
end
function Y.claim(name,id)
    local plot=plots[id];if not plot then return false,"plot not found" end
    if plot.owner then return false,"plot is already claimed by "..plot.owner end
    rewards[name]=rewards[name] or {};local available=rewards[name][plot.type] or 0
    if available<1 and not core.check_player_privs(name,{server=true}) then return false,"no "..plot.type.." plot reward available" end
    if available>0 then rewards[name][plot.type]=available-1 end
    plot.owner=name;plot.members={[name]=true};plot.public=false;plot.claimed=os.time();ensure_player(name);persist()
    return true,"Claimed "..plot.id.." ("..plot.type..")"
end
function Y.release(name,id)
    local plot=plots[id];if not may_manage(plot,name) then return false,"you do not manage that plot" end
    plot.owner=nil;plot.members={};plot.public=false;plot.name=plot.id;persist();return true,"Plot released"
end
function Y.add_member(name,id,member)
    local plot=plots[id];if not may_manage(plot,name) then return false,"you do not manage that plot" end
    plot.members=plot.members or {};plot.members[member]=true;persist();return true,member.." added to "..plot.id
end
function Y.remove_member(name,id,member)
    local plot=plots[id];if not may_manage(plot,name) then return false,"you do not manage that plot" end
    if member==plot.owner then return false,"owner cannot be removed" end
    plot.members[member]=nil;persist();return true,member.." removed from "..plot.id
end
function Y.clear(name,id)
    local plot=plots[id];if not may_manage(plot,name) then return false,"you do not manage that plot" end
    if volume(plot)>MAX_CLEAR_VOLUME then return false,"plot volume exceeds safety limit" end
    local keep={ ["nc_shipyard:plot_controller"]=true,["nc_shipyard:plot_boundary"]=true }
    local removed=0
    for x=plot.minp.x,plot.maxp.x do for y=plot.minp.y,plot.maxp.y do for z=plot.minp.z,plot.maxp.z do
        local pos={x=x,y=y,z=z};local node=core.get_node_or_nil(pos)
        if node and node.name~="air" and not keep[node.name] and not (core.is_protected and core.is_protected(pos,name)) then core.remove_node(pos);removed=removed+1 end
    end end end
    return true,string.format("Cleared %d nodes from %s",removed,plot.id)
end
function Y.rename(name,id,new_name)
    local plot=plots[id];if not may_manage(plot,name) then return false,"you do not manage that plot" end
    new_name=safe_name(new_name);if new_name=="" then return false,"name required" end
    plot.name=new_name;persist();return true,"Plot renamed to "..new_name
end
function Y.set_public(name,id,value)
    local plot=plots[id];if not may_manage(plot,name) then return false,"you do not manage that plot" end
    plot.public=value and true or false;persist();return true,"Plot set "..(plot.public and "public" or "private")
end
function Y.open_plot(lot_type)
    lot_type=canonical_type(lot_type);if not lot_type then return nil,"unknown lot type" end
    local ids={};for id,plot in pairs(plots) do if plot.type==lot_type and not plot.owner then ids[#ids+1]=id end end
    table.sort(ids);return ids[1] and plots[ids[1]] or nil,"no open "..lot_type.." plots"
end
function Y.teleport_player(name,plot)
    local player=core.get_player_by_name(name);if not player then return false,"player is offline" end
    if not plot then return false,"plot not found" end
    if plot.owner~=name and not plot.public and not (plot.members and plot.members[name]) and not core.check_player_privs(name,{server=true}) then return false,"plot access denied" end
    player:set_pos(vector.add(plot.spawn_pos or centre(plot),{x=0,y=1,z=0}));return true,"Teleported to "..plot.id
end
function Y.reward_experience(name,amount,reason)
    local state=ensure_player(name);local before=rank_for(state.experience).name
    state.experience=math.max(0,state.experience+math.floor(tonumber(amount) or 0));local after=rank_for(state.experience).name;persist()
    if before~=after then core.chat_send_player(name,"Shipyard rank advanced to "..after) end
    return state.experience,after,reason
end
function Y.reward_craft(construct,amount)
    if not construct or not construct.systems then return end
    local owner=construct.systems.owner or construct.owner;Y.reward_experience(owner,amount or math.max(1,math.floor((construct.profile.block_count or 1)/100)),"craft")
end
function Y.payday(name)
    local state=ensure_player(name);local now=os.time()
    if now-(state.last_payday or 0)<86400 then return false,"next payday is not ready" end
    local rank=rank_for(state.experience);state.credits=state.credits+(rank.pay or 0);state.last_payday=now;persist()
    return true,string.format("Payday: %d credits (%s); balance %d",rank.pay or 0,rank.name,state.credits)
end
function Y.player_status(name)
    local state=ensure_player(name);local rank=rank_for(state.experience);local owned=owned_plots(name)
    local reward_text={};for _,t in ipairs(D.lot_order) do if rewards[name] and (rewards[name][t] or 0)>0 then reward_text[#reward_text+1]=t.."="..rewards[name][t] end end
    return string.format("%s rank=%s exp=%d credits=%d plots=%d rewards=[%s]",name,rank.name,state.experience,state.credits,#owned,table.concat(reward_text,","))
end

local function record_transaction(name,amount,reason)
    local state=ensure_player(name)
    state.ledger=state.ledger or {}
    state.ledger[#state.ledger+1]={time=os.time(),amount=math.floor(amount),reason=tostring(reason or "transaction"),balance=state.credits}
    while #state.ledger>40 do table.remove(state.ledger,1) end
end
function Y.get_player_state(name) return copy(ensure_player(name)) end
function Y.get_rank(name) local state=ensure_player(name);return copy(rank_for(state.experience)) end
function Y.balance(name) return ensure_player(name).credits end
function Y.credit(name,amount,reason)
    amount=math.floor(tonumber(amount) or 0);if amount<0 then return Y.debit(name,-amount,reason) end
    local state=ensure_player(name);state.credits=state.credits+amount;record_transaction(name,amount,reason);persist();return true,state.credits
end
function Y.debit(name,amount,reason)
    amount=math.max(0,math.floor(tonumber(amount) or 0));local state=ensure_player(name)
    if state.credits<amount then return false,string.format("insufficient credits: need %d, balance %d",amount,state.credits) end
    state.credits=state.credits-amount;record_transaction(name,-amount,reason);persist();return true,state.credits
end
function Y.transaction_history(name,limit)
    local ledger=ensure_player(name).ledger or {};local out={};limit=math.max(1,math.min(40,math.floor(tonumber(limit) or 10)))
    for i=math.max(1,#ledger-limit+1),#ledger do out[#out+1]=copy(ledger[i]) end
    return out
end
function Y.all_plots() return copy(plots) end

core.register_craftitem("nc_shipyard:plot_wand",{
    description="Shipyard Plot Wand (use=corner 1, place=corner 2)",inventory_image="nc_frame.png^[colorize:#44aaff:180",
    on_use=function(itemstack,user,pointed)
        if not core.check_player_privs(user,{server=true}) then return itemstack end
        local pos=pointed and (pointed.under or pointed.above) or vector.round(user:get_pos())
        plot_positions[user:get_player_name()]=plot_positions[user:get_player_name()] or {};plot_positions[user:get_player_name()].pos1=vector.round(pos)
        core.chat_send_player(user:get_player_name(),"Shipyard corner 1: "..core.pos_to_string(pos));return itemstack
    end,
    on_place=function(itemstack,user,pointed)
        if not core.check_player_privs(user,{server=true}) then return itemstack end
        local pos=pointed and (pointed.above or pointed.under) or vector.round(user:get_pos())
        plot_positions[user:get_player_name()]=plot_positions[user:get_player_name()] or {};plot_positions[user:get_player_name()].pos2=vector.round(pos)
        core.chat_send_player(user:get_player_name(),"Shipyard corner 2: "..core.pos_to_string(pos));return itemstack
    end,
})
core.register_node("nc_shipyard:plot_controller",{
    description="Shipyard Claim Controller",tiles={"nc_frame.png^[colorize:#2288cc:180"},groups={cracky=2},
    on_construct=function(pos) core.get_meta(pos):set_string("infotext","Shipyard Claim Controller") end,
    on_rightclick=function(pos,node,player)
        local name=player:get_player_name();local plot=Y.find_plot_at(pos)
        if not plot then core.chat_send_player(name,"Controller is not inside a registered Shipyard plot");return end
        if not plot.owner then local ok,msg=Y.claim(name,plot.id);core.chat_send_player(name,msg)
        else core.chat_send_player(name,string.format("%s [%s] owner=%s access=%s",plot.name,plot.type,plot.owner,plot.public and "public" or "private")) end
    end,
})
core.register_node("nc_shipyard:plot_boundary",{description="Shipyard Boundary Marker",tiles={"nc_frame.png^[colorize:#33bbff:120"},drawtype="glasslike",paramtype="light",sunlight_propagates=true,groups={cracky=2}})
core.register_craftitem("nc_shipyard:credit",{description="Shipyard Credit Token",inventory_image="nc_frame.png^[colorize:#ffcc33:200"})

local function usage() return "reward|list|tp|open|info|addmember|remmember|clear|rename|public|private|player|plist|playerlist|ptp (plus Luanti admin create/delete/release/payday)" end
core.register_chatcommand("shipyard",{
    params=usage(),description="NavyCraft Shipyard 3.1.5 command surface",privs={interact=true},
    func=function(name,param)
        local args={};for word in (param or ""):gmatch("%S+") do args[#args+1]=word end
        local cmd=(args[1] or ""):lower()
        if cmd=="" then return true,usage() end
        if cmd=="reward" then
            if not core.check_player_privs(name,{server=true}) then return false,"server privilege required" end
            return Y.reward(args[2] or "",args[3] or "",args[4])
        elseif cmd=="list" then
            local out={};for i,p in ipairs(owned_plots(name)) do out[#out+1]=string.format("%d:%s/%s(%s)",i,p.id,p.name,p.type) end
            return true,#out>0 and table.concat(out," | ") or "You own no Shipyard plots"
        elseif cmd=="tp" then return Y.teleport_player(name,resolve_plot(name,args[2] or "1"))
        elseif cmd=="open" then
            local plot,err=Y.open_plot(args[2] or "");if not plot then return false,err end
            return Y.teleport_player(name,plot)
        elseif cmd=="info" then
            local plot=resolve_plot(name,args[2] or "") or Y.find_plot_at(core.get_player_by_name(name):get_pos())
            if not plot then return false,"plot not found" end
            local members={};for member in pairs(plot.members or {}) do members[#members+1]=member end;table.sort(members)
            return true,string.format("%s/%s type=%s owner=%s %s members=%s bounds=%s..%s",plot.id,plot.name,plot.type,plot.owner or "OPEN",plot.public and "PUBLIC" or "PRIVATE",table.concat(members,","),core.pos_to_string(plot.minp),core.pos_to_string(plot.maxp))
        elseif cmd=="addmember" then return Y.add_member(name,args[2] or "",args[3] or "")
        elseif cmd=="remmember" then return Y.remove_member(name,args[2] or "",args[3] or "")
        elseif cmd=="clear" then return Y.clear(name,args[2] or "")
        elseif cmd=="rename" then return Y.rename(name,args[2] or "",table.concat(args," ",3))
        elseif cmd=="public" then return Y.set_public(name,args[2] or "",true)
        elseif cmd=="private" then return Y.set_public(name,args[2] or "",false)
        elseif cmd=="player" then return true,Y.player_status(args[2] or name)
        elseif cmd=="plist" or cmd=="playerlist" then
            local names={};for player_name in pairs(players) do names[#names+1]=player_name end;table.sort(names);return true,table.concat(names,", ")
        elseif cmd=="ptp" then
            if not core.check_player_privs(name,{server=true}) then return false,"server privilege required" end
            local target=args[2] or "";return Y.teleport_player(target,plots[args[3] or ""])
        elseif cmd=="claim" then return Y.claim(name,args[2] or "")
        elseif cmd=="release" then return Y.release(name,args[2] or "")
        elseif cmd=="payday" then return Y.payday(name)
        elseif cmd=="create" then
            if not core.check_player_privs(name,{server=true}) then return false,"server privilege required" end
            local selection=plot_positions[name];if not selection or not selection.pos1 or not selection.pos2 then return false,"set both corners with the plot wand" end
            local ok,result=Y.create_plot(args[2] or "",args[3] or "",selection.pos1,selection.pos2,name)
            return ok,ok and ("Created plot "..result.id) or result
        elseif cmd=="delete" then
            if not core.check_player_privs(name,{server=true}) then return false,"server privilege required" end
            return Y.delete_plot(args[2] or "")
        end
        return false,usage()
    end,
})

core.register_on_joinplayer(function(player) ensure_player(player:get_player_name());persist() end)
core.register_on_shutdown(persist)
core.register_on_placenode(function(pos,newnode,placer)
    if placer and not Y.can_build(placer:get_player_name(),pos) then core.remove_node(pos);core.chat_send_player(placer:get_player_name(),"This Shipyard plot is private") end
end)
core.register_on_dignode(function(pos,oldnode,digger)
    if digger and not Y.can_build(digger:get_player_name(),pos) then core.set_node(pos,oldnode);core.chat_send_player(digger:get_player_name(),"This Shipyard plot is private") end
end)
core.log("action","[NavyCraft Shipyard] source-derived Shipyard 3.1.5 systems loaded")
return Y

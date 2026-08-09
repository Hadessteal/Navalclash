-- Vehicle storage, selection, recall and spawn systems based on the supplied
-- NavyCraft *select*, *claim*, *recall*, *spawn* controls and /ship store flow.
local ST = {}
local storage = core.get_mod_storage()
local blueprints = {}
local selected = {}

local function deep_copy(value) return core.deserialize(core.serialize(value)) end
local function key(owner,name) return owner.."\n"..name:lower() end
local function sanitise_name(name)
    name=(name or ""):gsub("[^%w _%-]",""):gsub("^%s+",""):gsub("%s+$","")
    if #name>32 then name=name:sub(1,32) end
    return name
end

local function persist()
    storage:set_string("navycraft_blueprints_v1",core.serialize(blueprints))
    storage:set_string("navycraft_selected_v1",core.serialize(selected))
end

local function load()
    local decoded=core.deserialize(storage:get_string("navycraft_blueprints_v1"))
    if type(decoded)=="table" then blueprints=decoded end
    decoded=core.deserialize(storage:get_string("navycraft_selected_v1"))
    if type(decoded)=="table" then selected=decoded end
end
load()

local function commander(construct,name)
    return construct and construct.systems and (name==construct.systems.owner or name==construct.systems.captain)
end

local function storage_region_for(name,position)
    local shipyard=navycraft.shipyard
    if not shipyard or not shipyard.find_plot_at then return nil end
    local plot=shipyard.find_plot_at(position)
    if not plot then return nil end
    if plot.owner==name or plot.public or (plot.members and plot.members[name]) then return plot end
    return nil
end

function ST.can_store(name,construct)
    if not construct then return false,"no active vessel" end
    if not commander(construct,name) then return false,"captain or owner permission required" end
    if math.abs(construct.forward_speed or 0)>.1 or math.abs(construct.vertical_speed or 0)>.1 or math.abs(construct.yaw_rate or 0)>.01 then
        return false,"stop the vessel before storage"
    end
    local plot=storage_region_for(name,construct.position)
    if not plot and not core.check_player_privs(name,{server=true}) then
        return false,"vessel must be inside an owned, member or public Shipyard plot"
    end
    return true,plot
end

function ST.store(name,blueprint_name)
    local construct=navycraft.preview.get_for_owner(name)
    if not construct then
        -- Permit a captain to store a vessel owned by somebody else.
        for _,candidate in pairs(navycraft.preview.get_all()) do
            if commander(candidate,name) then construct=candidate;break end
        end
    end
    local ok,plot_or_error=ST.can_store(name,construct)
    if not ok then return false,plot_or_error end
    blueprint_name=sanitise_name(blueprint_name)
    if blueprint_name=="" then blueprint_name=sanitise_name(construct.systems.custom_name or construct.profile.craft_type.."-"..os.time()) end
    local snapshot=navycraft.preview.snapshot(construct)
    snapshot.id=nil;snapshot.native_id=nil;snapshot.position=nil;snapshot.yaw=0
    snapshot.forward_speed=0;snapshot.vertical_speed=0;snapshot.yaw_rate=0;snapshot.turn_remaining=0
    snapshot.owner=name
    snapshot.systems=deep_copy(snapshot.systems or {})
    snapshot.systems.owner=name;snapshot.systems.captain=name;snapshot.systems.driver=name
    snapshot.systems.crew={[name]="owner"};snapshot.systems.crew_history={[name]=true}
    snapshot.systems.autotravel=false;snapshot.systems.hyperdrive=false;snapshot.systems.in_hyperspace=false
    snapshot.systems.sinking=false;snapshot.systems.frozen=false;snapshot.systems.scuttle_at=0;snapshot.systems.last_weapon_fire=0
    snapshot.systems.stored_name=blueprint_name
    snapshot.stored_at=os.time();snapshot.plot_id=type(plot_or_error)=="table" and plot_or_error.id or nil
    blueprints[key(name,blueprint_name)]={owner=name,name=blueprint_name,snapshot=snapshot,created=os.time(),updated=os.time()}
    selected[name]=blueprint_name
    local removed,remove_error=navycraft.preview.remove(construct.id,false,"stored")
    if not removed then blueprints[key(name,blueprint_name)]=nil;return false,remove_error end
    persist()
    return true,"Stored vehicle as "..blueprint_name
end

function ST.list(name,include_shared)
    local result={}
    for _,record in pairs(blueprints) do
        if record.owner==name or include_shared then result[#result+1]=record end
    end
    table.sort(result,function(a,b) return a.name:lower()<b.name:lower() end)
    return result
end

function ST.get(owner,name)
    name=sanitise_name(name or selected[owner] or "")
    if name=="" then return nil end
    return blueprints[key(owner,name)]
end

function ST.select(owner,name)
    local record=ST.get(owner,name)
    if not record then return false,"stored vehicle not found" end
    selected[owner]=record.name;persist();return true,"Selected "..record.name
end

function ST.selected(owner) return selected[owner] end

function ST.delete(owner,name)
    local record=ST.get(owner,name)
    if not record then return false,"stored vehicle not found" end
    blueprints[key(owner,record.name)]=nil
    if selected[owner]==record.name then selected[owner]=nil end
    persist();return true,"Deleted stored vehicle "..record.name
end

function ST.import_snapshot(owner,name,snapshot,options)
    options=options or{}
    name=sanitise_name(name)
    if name==""then return false,"stored vehicle name required"end
    local base=name;local suffix=2
    while blueprints[key(owner,name)]do name=sanitise_name(base.." "..suffix);suffix=suffix+1 end
    snapshot=deep_copy(snapshot or{})
    snapshot.id=nil;snapshot.native_id=nil;snapshot.position=nil;snapshot.yaw=0
    snapshot.forward_speed=0;snapshot.vertical_speed=0;snapshot.yaw_rate=0;snapshot.turn_remaining=0
    snapshot.owner=owner;snapshot.systems=deep_copy(snapshot.systems or{})
    snapshot.systems.owner=owner;snapshot.systems.captain=owner;snapshot.systems.driver=owner
    snapshot.systems.crew={[owner]="owner"};snapshot.systems.crew_history=snapshot.systems.crew_history or{};snapshot.systems.crew_history[owner]=true
    snapshot.systems.autotravel=false;snapshot.systems.hyperdrive=false;snapshot.systems.in_hyperspace=false;snapshot.systems.frozen=false
    snapshot.systems.stored_name=name;snapshot.stored_at=os.time()
    blueprints[key(owner,name)]={owner=owner,name=name,snapshot=snapshot,created=options.created or os.time(),updated=os.time(),source=options.source,title_id=options.title_id}
    if options.select~=false then selected[owner]=name end
    persist();return true,name
end

function ST.transfer(from_owner,name,to_owner,new_name)
    local record=ST.get(from_owner,name)
    if not record then return false,"stored vehicle not found"end
    to_owner=tostring(to_owner or"");if to_owner==""then return false,"new owner required"end
    local ok,imported=ST.import_snapshot(to_owner,new_name or record.name,record.snapshot,{source="title_transfer",title_id=record.title_id})
    if not ok then return false,imported end
    blueprints[key(from_owner,record.name)]=nil
    if selected[from_owner]==record.name then selected[from_owner]=nil end
    persist();return true,imported
end

function ST.remove_record(owner,name)
    local record=ST.get(owner,name);if not record then return nil end
    blueprints[key(owner,record.name)]=nil;if selected[owner]==record.name then selected[owner]=nil end;persist();return deep_copy(record)
end

local function spawn_position_for_player(player,position)
    if position then return vector.new(position) end
    local base=player:get_pos()
    local direction=player:get_look_dir()
    return vector.round(vector.add(base,vector.multiply(direction,6)))
end

function ST.spawn(owner,name,position,yaw,options)
    options=options or {}
    local record=ST.get(owner,name)
    if not record then return false,"stored vehicle not found" end
    local snapshot=deep_copy(record.snapshot)
    snapshot.systems=snapshot.systems or {}
    snapshot.systems.owner=owner
    snapshot.systems.captain=options.captain or owner
    snapshot.systems.driver=options.driver or owner
    snapshot.systems.stored_name=record.name
    snapshot.systems.frozen=false
    snapshot.systems.abandoned=false
    snapshot.systems.captain_abandoned=false
    snapshot.systems.taking_over=nil
    snapshot.systems.takeover_started=0
    snapshot.systems.release_at=0
    snapshot.systems.remote_control=false
    if options.auto then
        snapshot.systems.is_auto_craft=true
        snapshot.systems.route_id=options.route_id or ""
        snapshot.systems.autotravel=true
    end
    local runtime_owner=options.runtime_owner or owner
    local id,error_message=navycraft.preview.spawn_snapshot(runtime_owner,snapshot,position,yaw or 0,options)
    if not id then return false,error_message end
    local construct=navycraft.preview.get_by_id(id)
    if construct then
        construct.systems=deep_copy(snapshot.systems)
        construct.profile=deep_copy(snapshot.profile)
    end
    persist();navycraft.preview.save()
    return true,id
end

function ST.recall(player,name)
    local owner=player:get_player_name()
    local active=navycraft.preview.get_for_owner(owner)
    if active then
        local plot=storage_region_for(owner,player:get_pos())
        if not plot and not core.check_player_privs(owner,{server=true}) then return false,"stand in a Shipyard recall region" end
        local now=os.time()
        if now-(active.systems.last_teleport or 0)<navycraft.definitions.source.ship_teleport_cooldown and not core.check_player_privs(owner,{server=true}) then
            return false,"ship recall is still on cooldown"
        end
        active.systems.last_teleport=now
        local position=plot and vector.add(plot.spawn_pos or plot.controller or player:get_pos(),{x=0,y=2,z=0}) or spawn_position_for_player(player)
        navycraft.preview.teleport(active.id,position,0)
        return true,"Active vehicle recalled"
    end
    local plot=storage_region_for(owner,player:get_pos())
    if not plot and not core.check_player_privs(owner,{server=true}) then return false,"stand in a Shipyard recall region" end
    local position=plot and vector.add(plot.spawn_pos or plot.controller or player:get_pos(),{x=0,y=2,z=0}) or spawn_position_for_player(player)
    return ST.spawn(owner,name or selected[owner],position,0)
end

function ST.spawn_for_player(player,name,position,yaw)
    local owner=player:get_player_name()
    return ST.spawn(owner,name or selected[owner],spawn_position_for_player(player,position),yaw or player:get_look_horizontal())
end

function ST.repair_active(name)
    local construct=navycraft.preview.get_for_owner(name)
    if not construct then
        for _,candidate in pairs(navycraft.preview.get_all()) do if commander(candidate,name) then construct=candidate;break end end
    end
    if not construct then return false,"no active vessel" end
    local plot=storage_region_for(name,construct.position)
    if not plot and not core.check_player_privs(name,{server=true}) then return false,"vessel must be in an accessible Shipyard repair plot" end
    local repair=construct.systems and construct.systems.repair_snapshot
    if not repair then return false,"no launch-time repair snapshot exists" end
    local runtime_owner=construct.owner;local position=vector.new(construct.position);local yaw=construct.yaw
    local systems=deep_copy(construct.systems);systems.hull_integrity=1;systems.flooding=0;systems.sinking=false;systems.helm_destroyed=false;systems.scuttle_at=0;systems.frozen=false
    local snapshot={nodes=deep_copy(repair.nodes),profile=deep_copy(repair.profile),systems=systems}
    local ok,error_message=navycraft.preview.remove(construct.id,false,"repair_replace");if not ok then return false,error_message end
    local id,spawn_error=navycraft.preview.spawn_snapshot(runtime_owner,snapshot,position,yaw)
    if not id then return false,"repair removed vessel but restore failed: "..tostring(spawn_error) end
    return true,"Active vessel repaired in Shipyard plot"
end

function ST.repair(owner,name)
    local record=ST.get(owner,name)
    if not record then return false,"stored vehicle not found" end
    local systems=record.snapshot.systems or {}
    local repair=systems.repair_snapshot
    if not repair then return false,"no launch-time repair snapshot exists" end
    record.snapshot.nodes=deep_copy(repair.nodes)
    record.snapshot.profile=deep_copy(repair.profile)
    record.snapshot.systems.hull_integrity=1
    record.snapshot.systems.flooding=0
    record.snapshot.systems.sinking=false
    record.snapshot.systems.helm_destroyed=false
    record.snapshot.systems.frozen=false
    record.updated=os.time();persist()
    return true,"Stored vehicle repaired to its last launch snapshot"
end

function ST.claim_active(name,custom_name)
    local nearest=navycraft.preview.find_nearest(core.get_player_by_name(name):get_pos(),12)
    if not nearest then return false,"no vessel nearby" end
    if not nearest.systems.abandoned and nearest.systems.owner~=name then return false,"vessel is not abandoned" end
    local ok,message=navycraft.systems.takeover(nearest,name)
    if not ok then return false,message end
    if message=="takeover complete" then
        nearest.owner=name;nearest.systems.owner=name
        if custom_name and custom_name~="" then nearest.systems.custom_name=sanitise_name(custom_name) end
    end
    navycraft.preview.save();return true,message
end

function ST.export_state() return deep_copy({blueprints=blueprints,selected=selected}) end

return ST

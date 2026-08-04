-- Waypoint and automatic-route support based on Craft.WayPoints, routeID,
-- routeStage, WayPointTravel and /ship addwaypoint/autotravel.
local R={}
local storage=core.get_mod_storage()
local routes={}

local function persist() storage:set_string("navycraft_routes_v1",core.serialize(routes)) end
local decoded=core.deserialize(storage:get_string("navycraft_routes_v1"));if type(decoded)=="table" then routes=decoded end
local function clean(name) return (name or ""):gsub("[^%w _%-]",""):sub(1,32) end
local function route_key(owner,name) return owner.."\n"..name:lower() end
local function copy(v) return core.deserialize(core.serialize(v)) end

function R.create(owner,name)
    name=clean(name);if name=="" then return false,"route name required" end
    local k=route_key(owner,name)
    if routes[k] then return false,"route already exists" end
    routes[k]={owner=owner,name=name,waypoints={},loop=true,created=os.time(),updated=os.time()};persist()
    return true,"Created route "..name
end
function R.delete(owner,name)
    local k=route_key(owner,clean(name));if not routes[k] then return false,"route not found" end
    routes[k]=nil;persist();return true,"Deleted route"
end
function R.get(owner,name) return routes[route_key(owner,clean(name))] end
function R.list(owner)
    local out={};for _,route in pairs(routes) do if route.owner==owner then out[#out+1]=route end end
    table.sort(out,function(a,b)return a.name:lower()<b.name:lower() end);return out
end
function R.add_waypoint(owner,name,pos,index)
    local route=R.get(owner,name);if not route then return false,"route not found" end
    local wp={x=math.floor(pos.x+.5),y=math.floor(pos.y+.5),z=math.floor(pos.z+.5)}
    if index then table.insert(route.waypoints,math.max(1,math.min(#route.waypoints+1,index)),wp) else route.waypoints[#route.waypoints+1]=wp end
    route.updated=os.time();persist();return true,string.format("Waypoint %d added",#route.waypoints)
end
function R.remove_waypoint(owner,name,index)
    local route=R.get(owner,name);if not route then return false,"route not found" end
    index=tonumber(index);if not index or not route.waypoints[index] then return false,"waypoint not found" end
    table.remove(route.waypoints,index);route.updated=os.time();persist();return true,"Waypoint removed"
end
function R.bind(construct,owner,name)
    local route=R.get(owner,name);if not route then return false,"route not found" end
    construct.systems.route_id=route.name;construct.systems.route_stage=1
    construct.systems.waypoints=copy(route.waypoints);construct.systems.current_waypoint=1
    construct.systems.route_loop=route.loop~=false;construct.systems.navigation_mode="route"
    construct.systems.autotravel=#route.waypoints>0
    navycraft.preview.save();return true,string.format("Bound route %s with %d waypoints",route.name,#route.waypoints)
end
function R.unbind(construct)
    construct.systems.autotravel=false;construct.systems.navigation_mode="manual";construct.systems.route_id="";construct.systems.route_stage=0
    construct.systems.waypoints={};construct.systems.current_waypoint=1;navycraft.preview.save();return true,"Route cleared"
end
function R.spawn_auto(owner,blueprint,route_name,position,yaw)
    local route=R.get(owner,route_name);if not route then return false,"route not found" end
    if #route.waypoints==0 then return false,"route has no waypoints" end
    local auto_owner="AUTO:"..owner..":"..route.name..":"..os.time()
    local ok,id=navycraft.storage.spawn(owner,blueprint,position or route.waypoints[1],yaw or 0,{allow_multiple=true,auto=true,route_id=route.name,runtime_owner=auto_owner})
    if not ok then return false,id end
    local construct=navycraft.preview.get_by_id(id)
    if construct then
        construct.owner=auto_owner;construct.systems.owner=owner;construct.systems.captain=owner
        construct.systems.is_auto_craft=true;construct.systems.waypoints=copy(route.waypoints)
        construct.systems.current_waypoint=1;construct.systems.route_stage=1;construct.systems.route_loop=route.loop~=false
        construct.systems.navigation_mode="route";construct.systems.autotravel=true
        construct.systems.throttle=.75;construct.systems.gear=1
        for _,state in pairs(construct.systems.engines or {}) do state.set_on=true end
    end
    navycraft.preview.save();return true,id
end
function R.format(route)
    local points={};for i,p in ipairs(route.waypoints or {}) do points[#points+1]=string.format("%d:%s",i,core.pos_to_string(p)) end
    return string.format("%s [%d] %s",route.name,#route.waypoints,table.concat(points," "))
end
return R

local P={}
local storage=core.get_mod_storage();local ports={}
local function copy(v)return core.deserialize(core.serialize(v))end
local function safe_id(v)return(v or""):lower():gsub("[^%w_%-]",""):sub(1,32)end
local function persist()storage:set_string("ports_v1",core.serialize(ports))end
local function load()local v=core.deserialize(storage:get_string("ports_v1"));if type(v)=="table"then ports=v end end
load()
function P.save()persist()end
function P.all()return copy(ports)end
function P.get(id)return ports[safe_id(id)]and copy(ports[safe_id(id)])or nil end
function P.create(id,name,pos,options)
    id=safe_id(id);if id==""then return false,"port id required"end;if ports[id]then return false,"port already exists"end
    options=options or{};ports[id]={id=id,name=(name and name~=""and name or id):sub(1,48),pos=vector.round(pos),radius=math.max(4,math.min(128,tonumber(options.radius)or 20)),faction=options.faction or"navy",services=options.services or{missions=true,repair=true,ammo=true,trade=true,industry=true,shipyard=true},created=os.time()};persist();return true,copy(ports[id])
end
function P.remove(id)id=safe_id(id);if not ports[id]then return false,"port not found"end;ports[id]=nil;persist();return true,"port removed"end
function P.set_faction(id,faction)id=safe_id(id);if not ports[id]then return false,"port not found"end;ports[id].faction=faction or"neutral";persist();return true,copy(ports[id])end
function P.nearest(pos,max_distance)
    local best,dist;for _,port in pairs(ports)do local d=vector.distance(pos,port.pos);if(not dist or d<dist)and(not max_distance or d<=max_distance)then best,dist=port,d end end
    return best and copy(best)or nil,dist
end
function P.at_position(pos)
    local p,d=P.nearest(pos);if p and d<=p.radius then return p,d end;return nil,d
end
function P.for_player(name)local player=core.get_player_by_name(name);return player and P.at_position(player:get_pos())or nil end
function P.list_text()
    local out={};for _,p in pairs(ports)do out[#out+1]=string.format("%s:%s %s r=%d",p.id,p.name,core.pos_to_string(p.pos),p.radius)end;table.sort(out);return#out>0 and table.concat(out," | ")or"no ports registered"
end
return P

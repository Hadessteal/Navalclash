local R={}
local storage=core.get_mod_storage();local sites={}
local kinds={
    iron={item="nc_campaign:iron_ore",name="Iron Deposit",yield=3,base_reserve=900},
    coal={item="nc_campaign:coal",name="Coal Seam",yield=3,base_reserve=800},
    copper={item="nc_campaign:copper_ore",name="Copper Deposit",yield=2,base_reserve=650},
    oil={item="nc_campaign:crude_oil",name="Offshore Oil Field",yield=2,base_reserve=1000},
    salvage={item="nc_campaign:salvage",name="Wreck Salvage Field",yield=2,base_reserve=450},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function safe(v)return(v or""):lower():gsub("[^%w_%-]",""):sub(1,32)end
local function persist()storage:set_string("resource_sites_v1",core.serialize(sites))end
local function load()local data=core.deserialize(storage:get_string("resource_sites_v1"));if type(data)=="table"then sites=data end end;load()
function R.save()persist()end
function R.kinds()return kinds end
function R.create(id,kind,pos,reserve,radius)
    id=safe(id);kind=safe(kind);local def=kinds[kind];if id==""then return false,"site id required"end;if not def then return false,"unknown resource kind"end;if sites[id]then return false,"resource site already exists"end
    sites[id]={id=id,kind=kind,name=def.name,pos=vector.round(pos),radius=math.max(4,math.min(64,tonumber(radius)or 14)),reserve=math.max(1,math.floor(tonumber(reserve)or def.base_reserve)),extracted=0,created=os.time()};persist();return true,copy(sites[id])
end
function R.get(id)return sites[safe(id)]and copy(sites[safe(id)])or nil end
function R.all()return copy(sites)end
function R.nearest(pos,max_distance)
    local best,distance;for _,site in pairs(sites)do local d=vector.distance(pos,site.pos);if(not distance or d<distance)and(not max_distance or d<=max_distance)then best,distance=site,d end end;return best and copy(best)or nil,distance
end
function R.extract(name,id,cycles)
    local player=core.get_player_by_name(name);if not player then return false,"player unavailable"end
    local site=id and sites[safe(id)]or nil;if not site then site=R.nearest(player:get_pos(),64);site=site and sites[site.id]or nil end
    if not site then return false,"resource site not found"end;if vector.distance(player:get_pos(),site.pos)>site.radius then return false,"move inside the resource site"end
    if site.reserve<=0 then return false,"resource site is depleted"end
    cycles=math.max(1,math.min(20,math.floor(tonumber(cycles)or 1)));local def=kinds[site.kind]
    local amount=math.min(site.reserve,cycles*def.yield);local leftover=player:get_inventory():add_item("main",def.item.." "..amount)
    if leftover and leftover.get_count and leftover:get_count()>0 then return false,"inventory is full"end
    site.reserve=site.reserve-amount;site.extracted=(site.extracted or 0)+amount;site.last_operator=name;site.last_extracted=os.time();persist()
    navycraft.campaign.career.record_stat(name,"resources_extracted",amount)
    return true,string.format("Extracted %d %s; reserve %d",amount,def.name,site.reserve)
end
function R.list_text()
    local out={};for _,s in pairs(sites)do out[#out+1]=string.format("%s:%s reserve=%d %s",s.id,s.kind,s.reserve,core.pos_to_string(s.pos))end;table.sort(out);return#out>0 and table.concat(out," | ")or"no resource sites registered"
end
return R

local M={}
local storage=core.get_mod_storage()
local warehouses={}
local tick_accumulator=0

local commodities={
    iron_ore={item="nc_campaign:iron_ore",name="Iron Ore",base=6,target=180,category="raw"},
    coal={item="nc_campaign:coal",name="Coal",base=5,target=160,category="raw"},
    copper_ore={item="nc_campaign:copper_ore",name="Copper Ore",base=9,target=120,category="raw"},
    crude_oil={item="nc_campaign:crude_oil",name="Crude Oil",base=8,target=150,category="raw"},
    salvage={item="nc_campaign:salvage",name="Recovered Salvage",base=11,target=90,category="raw"},
    steel_ingot={item="nc_campaign:steel_ingot",name="Steel Ingot",base=20,target=100,category="refined"},
    copper_ingot={item="nc_campaign:copper_ingot",name="Copper Ingot",base=24,target=80,category="refined"},
    fuel_drum={item="nc_campaign:fuel_drum",name="Fuel Drum",base=30,target=90,category="refined"},
    provisions={item="nc_campaign:provisions",name="Provisions",base=16,target=130,category="supply"},
    munitions_crate={item="nc_campaign:munitions_crate",name="Munitions Crate",base=52,target=65,category="supply"},
    hull_plate={item="nc_campaign:hull_plate",name="Hull Plate",base=46,target=70,category="component"},
    machinery_parts={item="nc_campaign:machinery_parts",name="Machinery Parts",base=58,target=55,category="component"},
    electronics={item="nc_campaign:electronics",name="Electronics",base=74,target=45,category="component"},
}

local faction_modifiers={
    navy={hull_plate=.90,machinery_parts=.94,electronics=1.04,fuel_drum=.96,munitions_crate=.88},
    merchant={iron_ore=.94,coal=.94,copper_ore=.95,crude_oil=.93,fuel_drum=.90,provisions=.82},
    salvage={salvage=.78,electronics=.92,machinery_parts=.95},
    corsair={fuel_drum=1.18,electronics=1.22,hull_plate=1.15},
    neutral={},
}

local function copy(v)return core.deserialize(core.serialize(v))end
local function clamp(v,a,b)return math.max(a,math.min(b,v))end
local function persist()storage:set_string("markets_v1",core.serialize({warehouses=warehouses}))end
local function load()
    local data=core.deserialize(storage:get_string("markets_v1"))
    if type(data)=="table"then warehouses=data.warehouses or data end
end
load()

local function port_or_error(id)
    local port=navycraft.campaign.ports.get(id)
    if not port then return nil,"port not found"end
    return port
end
local function initial_stock(port,key,def)
    local seed=0
    for i=1,#port.id do seed=seed+port.id:byte(i)*i end
    local spread=((seed+#key*17)%61)-30
    local stock=math.floor(def.target*(1+spread/100))
    if port.faction=="merchant"and def.category=="raw"then stock=math.floor(stock*1.35)end
    if port.faction=="navy"and def.category=="component"then stock=math.floor(stock*1.2)end
    if port.faction=="salvage"and key=="salvage"then stock=math.floor(stock*1.8)end
    return math.max(5,stock)
end
function M.ensure_port(id)
    local port,err=port_or_error(id);if not port then return nil,err end
    local w=warehouses[port.id]
    if not w then
        w={port_id=port.id,capacity=2500,stock={},revision=1,last_tick=os.time()}
        for key,def in pairs(commodities)do w.stock[key]=initial_stock(port,key,def)end
        warehouses[port.id]=w;persist()
    end
    return w
end
function M.save()persist()end
function M.commodities()return commodities end
function M.warehouse(id)local w=M.ensure_port(id);return w and copy(w)or nil end
function M.stock(id,key)local w=M.ensure_port(id);return w and(w.stock[key]or 0)or 0 end
function M.adjust(id,key,amount)
    local w,err=M.ensure_port(id);if not w then return false,err end
    if not commodities[key]then return false,"unknown commodity"end
    local next_value=math.max(0,math.floor((w.stock[key]or 0)+(tonumber(amount)or 0)))
    local total=0;for k,v in pairs(w.stock)do if k~=key then total=total+v end end
    if total+next_value>w.capacity then next_value=math.max(0,w.capacity-total)end
    w.stock[key]=next_value;w.revision=(w.revision or 0)+1;persist();return true,next_value
end
function M.price(id,key,side)
    local port=navycraft.campaign.ports.get(id);local def=commodities[key];if not port or not def then return nil end
    local stock=M.stock(id,key)
    local scarcity=((def.target+20)/(stock+20))^.68
    local faction=(faction_modifiers[port.faction]or{})[key]or 1
    local strategic=navycraft.campaign.supply and navycraft.campaign.supply.price_multiplier(id,key,side)or 1
    local price=def.base*clamp(scarcity,.48,2.8)*faction*strategic
    if side=="sell"then price=price*.78 end
    return math.max(1,math.floor(price+.5))
end
local function player_port(name)
    local port=navycraft.campaign.ports.for_player(name)
    if not port then return nil,"stand inside a registered port"end
    if port.services and port.services.trade==false then return nil,"this port does not offer trade"end
    M.ensure_port(port.id);return port
end
function M.buy(name,key,amount)
    local port,err=player_port(name);if not port then return false,err end
    key=(key or""):lower();local def=commodities[key];if not def then return false,"unknown commodity"end
    amount=math.max(1,math.min(200,math.floor(tonumber(amount)or 1)))
    local stock=M.stock(port.id,key);if stock<amount then return false,string.format("only %d units available",stock)end
    local unit=M.price(port.id,key,"buy");local cost=unit*amount
    local ok,result=navycraft.campaign.career.spend(name,cost,"commodity purchase: "..key);if not ok then return false,result end
    local player=core.get_player_by_name(name);local leftover=player:get_inventory():add_item("main",def.item.." "..amount)
    if leftover and leftover.get_count and leftover:get_count()>0 then navycraft.shipyard.credit(name,cost,"refund: inventory full");return false,"inventory is full; purchase refunded"end
    M.adjust(port.id,key,-amount);navycraft.campaign.career.record_stat(name,"commodities_bought",amount)
    return true,string.format("Bought %d %s at %dc each (%dc)",amount,def.name,unit,cost)
end
function M.sell(name,key,amount)
    local port,err=player_port(name);if not port then return false,err end
    key=(key or""):lower();local def=commodities[key];if not def then return false,"unknown commodity"end
    amount=math.max(1,math.min(200,math.floor(tonumber(amount)or 1)))
    local player=core.get_player_by_name(name);local inv=player:get_inventory()
    if not inv:contains_item("main",def.item.." "..amount)then return false,"not enough "..def.name end
    local unit=M.price(port.id,key,"sell");local payment=unit*amount
    inv:remove_item("main",def.item.." "..amount);M.adjust(port.id,key,amount)
    navycraft.shipyard.credit(name,payment,"commodity sale: "..key);navycraft.campaign.career.record_stat(name,"commodities_sold",amount)
    return true,string.format("Sold %d %s at %dc each (%dc)",amount,def.name,unit,payment)
end
function M.list_text(id)
    local port=navycraft.campaign.ports.get(id);if not port then return"port not found"end;M.ensure_port(id)
    local out={};for key,def in pairs(commodities)do out[#out+1]=string.format("%s stock=%d buy=%dc sell=%dc",key,M.stock(id,key),M.price(id,key,"buy"),M.price(id,key,"sell"))end
    table.sort(out);return table.concat(out," | ")
end
function M.route_text(name,key)
    key=(key or""):lower();if key~=""and not commodities[key]then return"unknown commodity"end
    local ports=navycraft.campaign.ports.all();local opportunities={}
    for _,from in pairs(ports)do for _,to in pairs(ports)do if from.id~=to.id then
        local keys={};if key~=""then keys={key}else for commodity in pairs(commodities)do keys[#keys+1]=commodity end end
        for _,commodity in ipairs(keys)do local margin=M.price(to.id,commodity,"sell")-M.price(from.id,commodity,"buy");if margin>0 then opportunities[#opportunities+1]={margin=margin,text=string.format("%s %s->%s +%dc/unit",commodity,from.id,to.id,margin)}end end
    end end end
    table.sort(opportunities,function(a,b)return a.margin>b.margin end);local out={};for i=1,math.min(5,#opportunities)do out[#out+1]=opportunities[i].text end
    return#out>0 and table.concat(out," | ")or"no profitable route at current prices"
end
function M.step(dt)
    tick_accumulator=tick_accumulator+(tonumber(dt)or 0);if tick_accumulator<30 then return 0 end
    local ticks=math.floor(tick_accumulator/30);tick_accumulator=tick_accumulator-ticks*30;local changed=0
    for id,w in pairs(warehouses)do local port=navycraft.campaign.ports.get(id);if port then
        for key,def in pairs(commodities)do
            local drift=0
            if def.category=="raw"then drift=(port.faction=="merchant"or port.faction=="salvage")and 2 or 1
            elseif def.category=="component"then drift=port.faction=="navy"and 1 or-1
            elseif key=="provisions"then drift=port.faction=="merchant"and 2 or 0
            elseif key=="munitions_crate"then drift=port.faction=="navy"and 1 or-1 else drift=0 end
            local current=w.stock[key]or 0;if current>def.target*1.35 then drift=drift-1 elseif current<def.target*.55 then drift=drift+1 end
            w.stock[key]=math.max(0,current+drift*ticks);changed=changed+1
        end
        w.revision=(w.revision or 0)+1;w.last_tick=os.time()
    end end
    if changed>0 then persist()end;return changed
end
return M

local C=navycraft.campaign
local sessions={}
local function esc(v)return core.formspec_escape and core.formspec_escape(tostring(v))or tostring(v):gsub("[\\%[%];,]","")end
core.register_craftitem("nc_campaign:sealed_dispatch",{description="Sealed Naval Dispatch",inventory_image="nc_frame.png^[colorize:#e6d58a:180",stack_max=1,groups={not_in_creative_inventory=1}})
core.register_craftitem("nc_campaign:repair_parts",{description="Naval Repair Parts",inventory_image="nc_frame.png^[colorize:#d8a060:180"})
core.register_craftitem("nc_campaign:salvage_crate",{description="Recovered Salvage Crate",inventory_image="nc_frame.png^[colorize:#7b9b8b:180"})
local commodity_items={
    iron_ore={"Iron Ore","#8c6a54"},coal={"Coal","#292929"},copper_ore={"Copper Ore","#b56f42"},crude_oil={"Crude Oil","#1b1b24"},salvage={"Recovered Salvage","#6d8277"},
    steel_ingot={"Steel Ingot","#aab1b8"},copper_ingot={"Copper Ingot","#cf7c45"},fuel_drum={"Refined Fuel Drum","#80783f"},provisions={"Packed Provisions","#9a7b46"},munitions_crate={"Naval Munitions Crate","#6d5245"},hull_plate={"Rolled Hull Plate","#65717b"},machinery_parts={"Marine Machinery Parts","#876b4e"},electronics={"Marine Electronics","#4f8791"},
}
for key,entry in pairs(commodity_items)do core.register_craftitem("nc_campaign:"..key,{description=entry[1],inventory_image="nc_frame.png^[colorize:"..entry[2]..":190",groups={navycraft_commodity=1}})end
local equipment_items={engine_kit_t1={"Tier I Engine Refit Kit","#bc6b3c"},engine_kit_t2={"Tier II Engine Refit Kit","#d68a4a"},pump_kit_t1={"Tier I Pump Refit Kit","#3b82a0"},pump_kit_t2={"Tier II Pump Refit Kit","#4d9dbb"},sensor_kit_t1={"Tier I Sensor Refit Kit","#3f9d71"},sensor_kit_t2={"Tier II Sensor Refit Kit","#54b889"},fire_control_kit_t1={"Tier I Fire-Control Refit Kit","#8b66a8"},fire_control_kit_t2={"Tier II Fire-Control Refit Kit","#a37bc0"},cargo_kit_t1={"Tier I Cargo Handling Refit","#9a7a45"},cargo_kit_t2={"Tier II Cargo Handling Refit","#b99655"},armor_kit_t1={"Tier I Compartment Armour","#69747d"},armor_kit_t2={"Tier II Compartment Armour","#89949d"}}
for key,entry in pairs(equipment_items)do core.register_craftitem("nc_campaign:"..key,{description=entry[1],inventory_image="nc_frame.png^[colorize:"..entry[2]..":200",stack_max=1,groups={navycraft_equipment=1}})end
core.register_craftitem("nc_campaign:portable_extractor",{description="Portable Resource Extractor",inventory_image="nc_frame.png^[colorize:#d8b45c:180",stack_max=1})

local function mission_formspec(name,port)
    local text,offers=C.missions.list_text(name,port.id);sessions[name]={port_id=port.id,offers=offers}
    local lines={};for i,o in ipairs(offers)do lines[#lines+1]=esc(string.format("%d. %s — %dc / %dxp",i,o.title,o.reward.credits or 0,o.reward.xp or 0))end
    return "formspec_version[6]size[12,8]label[.4,.4;"..esc(port.name.." Mission Board").."]textlist[.4,1;11.2,4.5;offers;"..table.concat(lines,",")..";1;false]button[.4,5.8;2.5,.8;accept;Accept]button[3.1,5.8;2.5,.8;status;Status]button[5.8,5.8;2.5,.8;abandon;Abandon]label[.4,7;"..esc(text).."]"
end
local function store_formspec(name,port)
    return "formspec_version[6]size[12,7]label[.4,.4;"..esc(port.name.." Quartermaster").."]textarea[.4,1;11.2,3;catalog;;"..esc(C.economy.list_text(name)).."]field[.6,4.5;4,.8;item;Item;]field[4.8,4.5;2,.8;units;Units;1]button[7,4.35;2,.8;buy;Buy]button[9.2,4.35;2,.8;repair;Repair Vessel]label[.4,6;Balance: "..C.career.balance(name).." credits]"
end
core.register_node("nc_campaign:port_beacon",{description="Naval Port Beacon",tiles={"nc_frame.png^[colorize:#2f83c7:170"},groups={cracky=2},light_source=5,
    on_construct=function(pos)local m=core.get_meta(pos);m:set_string("infotext","Unregistered Naval Port Beacon")end,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local p=C.ports.at_position(pos);if not p then core.chat_send_player(name,"Register this beacon using /port create <id> <name>")else core.chat_send_player(name,p.name.." port services: missions, repair, ammunition")end end})
core.register_node("nc_campaign:mission_board",{description="Naval Mission Board",tiles={"nc_frame.png^[colorize:#315a8c:190"},groups={cracky=2},
    on_rightclick=function(pos,node,player)local port=C.ports.at_position(pos);if not port then core.chat_send_player(player:get_player_name(),"Mission board is outside a registered port")return end;core.show_formspec(player:get_player_name(),"nc_campaign:missions",mission_formspec(player:get_player_name(),port))end})
core.register_node("nc_campaign:quartermaster",{description="Port Quartermaster Terminal",tiles={"nc_frame.png^[colorize:#bd8f34:180"},groups={cracky=2},
    on_rightclick=function(pos,node,player)local port=C.ports.at_position(pos);if not port then core.chat_send_player(player:get_player_name(),"Quartermaster terminal is outside a registered port")return end;core.show_formspec(player:get_player_name(),"nc_campaign:store",store_formspec(player:get_player_name(),port))end})
core.register_on_player_receive_fields(function(player,formname,fields)
    local name=player:get_player_name()
    if formname=="nc_campaign:missions"then local s=sessions[name];if not s then return end
        local selected=tonumber((fields.offers or""):match("CHG:(%d+)")or(fields.offers or""):match("DCL:(%d+)"))or 1;s.selected=selected
        if fields.accept then local o=s.offers[s.selected or 1];if o then local ok,msg=C.missions.accept(name,o.id);core.chat_send_player(name,msg)end
        elseif fields.status then core.chat_send_player(name,C.missions.status(name))elseif fields.abandon then local ok,msg=C.missions.abandon(name);core.chat_send_player(name,msg)end
    elseif formname=="nc_campaign:store"then
        if fields.buy then local ok,msg=C.economy.buy(name,fields.item,fields.units);core.chat_send_player(name,msg)
        elseif fields.repair then local ok,msg=C.economy.repair(name);core.chat_send_player(name,msg)end
    end
end)

core.register_node("nc_campaign:operations_table",{description="Fleet Operations Table",tiles={"nc_frame.png^[colorize:#28455f:200"},groups={cracky=2},light_source=3,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);if not port then core.chat_send_player(name,"Operations table is outside a registered port")return end;local text=C.operations.status(name).." | "..C.territories.status(port.id);core.chat_send_player(name,text);core.show_formspec(name,"nc_campaign:missions",mission_formspec(name,port))end})
core.register_node("nc_campaign:faction_ensign",{description="Faction Ensign",tiles={"nc_frame.png^[colorize:#d8d8d8:120"},groups={snappy=2},
    on_construct=function(pos)core.get_meta(pos):set_string("faction","navy")end,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();core.chat_send_player(name,C.factions.status(name))end})

core.register_node("nc_campaign:market_terminal",{description="Commodity Market Terminal",tiles={"nc_frame.png^[colorize:#3f7e5f:190"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);core.chat_send_player(name,port and C.markets.list_text(port.id)or"Market terminal is outside a registered port")end})
core.register_node("nc_campaign:factory_terminal",{description="Naval Factory Terminal",tiles={"nc_frame.png^[colorize:#76573f:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);core.chat_send_player(name,port and(C.industry.list_text(name).." | "..C.industry.status(name))or"Factory terminal is outside a registered port")end})
core.register_node("nc_campaign:resource_beacon",{description="Resource Survey Beacon",tiles={"nc_frame.png^[colorize:#9b8246:180"},groups={cracky=2},light_source=4,
    on_rightclick=function(pos,node,player)local site=C.resources.nearest(pos,16);core.chat_send_player(player:get_player_name(),site and(string.format("%s %s reserve=%d",site.id,site.kind,site.reserve))or"No registered resource site nearby")end})


core.register_node("nc_campaign:blueprint_desk",{description="Naval Architecture Desk",tiles={"nc_frame.png^[colorize:#5377a8:190"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);core.chat_send_player(name,port and(C.blueprints.status(name).." | "..C.classes.list_text(name))or"Architecture desk is outside a registered port")end})
core.register_node("nc_campaign:cargo_terminal",{description="Vessel Cargo Terminal",tiles={"nc_frame.png^[colorize:#8c6a43:190"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local c=navycraft.preview.get_for_owner(player:get_player_name());core.chat_send_player(player:get_player_name(),C.logistics.status(c))end})
core.register_node("nc_campaign:maintenance_terminal",{description="Vessel Maintenance Terminal",tiles={"nc_frame.png^[colorize:#57706a:190"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local c=navycraft.preview.get_for_owner(player:get_player_name());core.chat_send_player(player:get_player_name(),C.maintenance.status(c))end})

core.register_node("nc_campaign:logistics_table",{description="Harbour Logistics Table",tiles={"nc_frame.png^[colorize:#466b7a:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);core.chat_send_player(name,port and(C.supply.status(port.id).." | "..C.shipping.list_text())or"Logistics table is outside a registered port")end})
core.register_node("nc_campaign:convoy_board",{description="Merchant Convoy Board",tiles={"nc_frame.png^[colorize:#8e733e:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)core.chat_send_player(player:get_player_name(),C.shipping.list_text())end})

core.register_node("nc_campaign:title_registry",{description="Vessel Title Registry",tiles={"nc_frame.png^[colorize:#355d83:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);core.chat_send_player(name,port and C.ownership.status(name)or"Title registry is outside a registered port")end})
core.register_node("nc_campaign:insurance_office",{description="Marine Insurance Office",tiles={"nc_frame.png^[colorize:#4c7a62:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)local name=player:get_player_name();local port=C.ports.at_position(pos);core.chat_send_player(name,port and C.insurance.status(name)or"Insurance office is outside a registered port")end})
core.register_node("nc_campaign:ship_broker",{description="Vessel Brokerage Terminal",tiles={"nc_frame.png^[colorize:#9b743d:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)core.chat_send_player(player:get_player_name(),C.ownership.listings_text())end})
core.register_node("nc_campaign:salvage_office",{description="Salvage Rights Office",tiles={"nc_frame.png^[colorize:#667b72:200"},groups={cracky=2},light_source=2,
    on_rightclick=function(pos,node,player)core.chat_send_player(player:get_player_name(),C.salvage.list_text(player:get_player_name()))end})

core.register_node("nc_campaign:training_console",{description="Naval Training Console",tiles={"nc_frame.png^[colorize:#4f79a7:190"},groups={cracky=2},light_source=3,
 on_rightclick=function(pos,node,player)local name=player:get_player_name();navycraft.campaign.tutorial.mark(name,"station");navycraft.campaign.interfaces.open(name,6)end})
core.register_node("nc_campaign:bridge_console",{description="Integrated Bridge Console",tiles={"nc_frame.png^[colorize:#24566f:210"},groups={cracky=2},light_source=4,
 on_rightclick=function(pos,node,player)navycraft.campaign.interfaces.open(player:get_player_name(),1)end})
core.register_craftitem("nc_campaign:service_handbook",{description="NavyCraft Service Handbook",inventory_image="nc_frame.png^[colorize:#4f79a7:190",stack_max=1,
 on_use=function(itemstack,user)if user then navycraft.campaign.interfaces.open(user:get_player_name(),6)end;return itemstack end})

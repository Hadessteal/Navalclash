local E={}
local catalog={
    cannon_shell={item="nc_navycraft:cannon_shell",price=12,bundle=4},
    aa_round={item="nc_navycraft:aa_round",price=8,bundle=8},
    torpedo_mk1={item="nc_navycraft:torpedo_mk1",price=75,bundle=1},
    torpedo_mk2={item="nc_navycraft:torpedo_mk2",price=120,bundle=1},
    torpedo_mk3={item="nc_navycraft:torpedo_mk3",price=220,bundle=1,rank="officer"},
    depth_charge={item="nc_navycraft:depth_charge",price=65,bundle=2},
    bomb={item="nc_navycraft:bomb",price=80,bundle=1},
    repair_parts={item="nc_campaign:repair_parts",price=30,bundle=2},
}
local function require_port(name,service)
    local player=core.get_player_by_name(name);if not player then return nil,"player unavailable"end
    local port=navycraft.campaign.ports.at_position(player:get_pos());if not port then return nil,"stand inside a registered port"end
    if port.services and port.services[service]==false then return nil,"this port does not offer "..service end
    return port
end
function E.catalog()return catalog end
function E.list_text(name)
    local out={};for key,e in pairs(catalog)do if not e.rank or navycraft.campaign.career.has_rank(name,e.rank)then out[#out+1]=string.format("%s=%dc/%d",key,e.price,e.bundle)end end;table.sort(out);return table.concat(out," | ")
end
function E.buy(name,key,units)
    local port,err=require_port(name,"ammo");if err then return false,err end
    local entry=catalog[(key or""):lower()];if not entry then return false,"unknown store item"end
    if entry.rank and not navycraft.campaign.career.has_rank(name,entry.rank)then return false,"requires rank "..entry.rank end
    units=math.max(1,math.min(100,math.floor(tonumber(units)or 1)));local surcharge=1;if navycraft.campaign.supply then local available,mult,message=navycraft.campaign.supply.service_status(port.id,"ammo",units*.35);if not available then return false,message end;surcharge=mult or 1 end;local cost=math.ceil(entry.price*units*surcharge)
    local ok,result=navycraft.campaign.career.spend(name,cost,"port purchase: "..key);if not ok then return false,result end
    local player=core.get_player_by_name(name);local count=entry.bundle*units;local leftover=player:get_inventory():add_item("main",entry.item.." "..count)
    if leftover and leftover.get_count and leftover:get_count()>0 then
        navycraft.shipyard.credit(name,cost,"refund: inventory full");return false,"inventory is full; purchase refunded"
    end
    if navycraft.campaign.supply then navycraft.campaign.supply.consume_service(port.id,"ammo",math.max(.2,units*.35),"quartermaster sale: "..key)end
    return true,string.format("Purchased %d %s for %d credits; balance %d",count,key,cost,navycraft.campaign.career.balance(name))
end
function E.repair_quote(construct)
    if not construct or not construct.systems then return nil end
    local missing=math.max(0,(construct.profile.block_count or 0)-(construct.profile.block_count_alive or construct.profile.block_count or 0))
    local integrity_missing=math.max(0,1-(construct.systems.hull_integrity or 1))
    local flood=math.max(0,construct.systems.flooding or 0)
    return math.max(25,math.ceil(missing*6+integrity_missing*250+flood*2))
end
function E.repair(name)
    local port,err=require_port(name,"repair");if err then return false,err end
    local c=navycraft.preview.get_for_owner(name);if not c then return false,"launch or command a vessel first"end
    local cost=E.repair_quote(c);if navycraft.campaign.supply then local available,mult,message=navycraft.campaign.supply.service_status(port.id,"repair",cost/35);if not available then return false,message end;cost=math.ceil(cost*(mult or 1))end;local ok,result=navycraft.campaign.career.spend(name,cost,"port vessel repair");if not ok then return false,result end
    local repaired,message=navycraft.storage.repair_active(name)
    if not repaired then navycraft.shipyard.credit(name,cost,"refund: repair failed");return false,message.."; payment refunded"end
    if navycraft.campaign.supply then navycraft.campaign.supply.consume_service(port.id,"repair",math.max(.5,cost/35),"vessel repair")end;navycraft.campaign.career.record_stat(name,"repairs",1);return true,message..string.format("; cost %d credits",cost)
end
function E.recharge_pumps(name)
    local port,err=require_port(name,"repair");if err then return false,err end
    local c=navycraft.preview.get_for_owner(name);if not c then return false,"no active vessel"end
    local capacity=(c.profile.pump_count or 0)*navycraft.definitions.source.pump_charge_limit;local missing=math.max(0,capacity-(c.systems.pump_charge or 0));local cost=math.ceil(missing*2)
    if missing<=0 then return false,"pump charge is already full"end
    if navycraft.campaign.supply then local available,mult,message=navycraft.campaign.supply.service_status(port.id,"repair",math.max(.2,missing/10));if not available then return false,message end;cost=math.ceil(cost*(mult or 1))end
    local ok,result=navycraft.campaign.career.spend(name,cost,"pump recharge");if not ok then return false,result end
    if navycraft.campaign.supply then navycraft.campaign.supply.consume_service(port.id,"repair",math.max(.2,missing/10),"pump recharge")end;c.systems.pump_charge=capacity;navycraft.preview.save();return true,string.format("Pumps recharged for %d credits",cost)
end
return E

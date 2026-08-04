local O={}
local storage=core.get_mod_storage()
local titles,offers,listings={},{},{}
local counters={title=0,offer=0,listing=0}
local settings={loss_mode="standard"}
local ignored_remove={stored=true,repair_replace=true,sale_escrow=true,salvage_recovery=true,admin_cleanup=true,npc_cleanup=true,restored_to_world=true}
local function copy(v)return core.deserialize(core.serialize(v))end
local function safe_name(v)return tostring(v or""):gsub("[^%w _%-]",""):gsub("^%s+",""):gsub("%s+$",""):sub(1,32)end
local function persist()storage:set_string("ownership_m5f_v1",core.serialize({titles=titles,offers=offers,listings=listings,counters=counters,settings=settings}))end
local function load()
    local v=core.deserialize(storage:get_string("ownership_m5f_v1"));if type(v)~="table"then return end
    titles=v.titles or{};offers=v.offers or{};listings=v.listings or{};counters=v.counters or counters;settings=v.settings or settings
end
load()
local function next_id(kind,prefix)counters[kind]=(counters[kind]or 0)+1;return string.format("%s%06d",prefix,counters[kind])end
local function active(name)
    local c=navycraft.preview.get_for_owner(name)
    if c and c.systems and c.systems.owner==name then return c end
    for _,x in pairs(navycraft.preview.get_all())do if x.systems and x.systems.owner==name then return x end end
    return nil
end
local function port_and_vessel(name)
    local c=active(name);if not c then return nil,nil,"launch or command an owned vessel first"end
    local p=navycraft.campaign.ports.for_player(name);if not p then return nil,nil,"stand inside a registered port"end
    if vector.distance(c.position,p.pos)>p.radius then return nil,nil,"bring the vessel inside the same port"end
    if math.abs(c.forward_speed or 0)>.1 or math.abs(c.vertical_speed or 0)>.1 or math.abs(c.yaw_rate or 0)>.01 then return nil,nil,"stop the vessel before changing title"end
    return c,p
end
function O.save()persist()end
function O.loss_mode()return settings.loss_mode or"standard"end
function O.set_loss_mode(mode)
    mode=tostring(mode or""):lower();if mode~="casual"and mode~="standard"and mode~="strict"then return false,"loss mode must be casual, standard or strict"end
    settings.loss_mode=mode;persist();return true,"Permanent-loss mode set to "..mode
end
function O.ensure(c)
    if not c or not c.systems then return nil end
    local changed=false;local id=c.systems.title_id
    if not id or id==""then id=next_id("title","NC-");c.systems.title_id=id;changed=true end
    local t=titles[id]
    if not t then
        t={id=id,owner=c.systems.owner or c.owner,name=safe_name(c.systems.custom_name or(c.profile and c.profile.craft_type)or id),class=c.systems.vessel_class,status="active",created=os.time(),history={}}
        titles[id]=t;changed=true
    end
    local owner=c.systems.owner or t.owner;local name=safe_name(c.systems.custom_name or t.name);local class=c.systems.vessel_class or t.class
    if t.owner~=owner then t.owner=owner;changed=true end;if t.name~=name then t.name=name;changed=true end;if t.class~=class then t.class=class;changed=true end
    if t.status~="active"then t.status="active";changed=true end;if t.construct_id~=c.id then t.construct_id=c.id;changed=true end
    if changed then t.updated=os.time();persist()end;return t
end
function O.get(id)return titles[tostring(id or"")]and copy(titles[tostring(id)])or nil end
function O.value(c)
    if not c then return 0 end
    local blocks=tonumber(c.profile and(c.profile.block_count_alive or c.profile.block_count))or 0
    local class=navycraft.campaign.classes.apply(c);local base=blocks*3+(class and class.service or 10)*12
    for _,key in pairs((c.systems and c.systems.equipment)or{})do local d=navycraft.campaign.equipment.catalog()[key];base=base+(d and d.tier or 1)*120 end
    local condition=((c.systems or{}).maintenance or{}).condition or 1
    local hull=(c.systems and c.systems.hull_integrity)or 1
    return math.max(50,math.floor(base*(.35+.65*math.max(0,math.min(1,condition)))*(.25+.75*math.max(0,math.min(1,hull)))))
end
function O.status(name)
    local c=active(name);if not c then return"no owned active vessel"end;local t=O.ensure(c)
    return string.format("title=%s owner=%s name=%s class=%s status=%s value=%dc loss_mode=%s",t.id,t.owner,t.name,t.class or"unclassified",t.status,O.value(c),O.loss_mode())
end
function O.offer(name,buyer,price)
    local c,port,err=port_and_vessel(name);if not c then return false,err end
    buyer=tostring(buyer or"");if buyer==""or buyer==name then return false,"different buyer required"end
    local t=O.ensure(c);price=math.max(0,math.floor(tonumber(price)or 0));local id=next_id("offer","TO-")
    offers[id]={id=id,title_id=t.id,seller=name,buyer=buyer,price=price,port_id=port.id,construct_id=c.id,status="open",created=os.time(),expires=os.time()+900}
    persist();return true,string.format("Transfer offer %s created for %s at %dc",id,buyer,price)
end
local function transfer_live(c,new_owner,reason)
    local old_owner=c.systems.owner or c.owner;local t=O.ensure(c)
    local ok,result=navycraft.preview.transfer_owner(c,new_owner,{reset_crew=true})
    if not ok then return false,result end
    t.owner=new_owner;t.status="active";t.construct_id=c.id;t.updated=os.time();t.history=t.history or{};t.history[#t.history+1]={time=os.time(),from=old_owner,to=new_owner,reason=reason or"transfer"}
    if navycraft.campaign.insurance then navycraft.campaign.insurance.on_transfer(t.id,old_owner,new_owner)end
    persist();return true,t
end
function O.accept(name,id)
    local o=offers[tostring(id or"")];if not o or o.status~="open"then return false,"transfer offer not found"end
    if o.buyer~=name then return false,"offer is addressed to "..tostring(o.buyer)end
    if os.time()>(o.expires or 0)then o.status="expired";persist();return false,"transfer offer expired"end
    local c=navycraft.preview.get_by_id(o.construct_id);if not c then return false,"offered vessel is no longer active"end
    local p=navycraft.campaign.ports.for_player(name);if not p or p.id~=o.port_id then return false,"accept the transfer at the offering port"end
    if o.price>0 then local ok,msg=navycraft.campaign.career.spend(name,o.price,"purchase vessel title "..o.title_id);if not ok then return false,msg end end
    local ok,result=transfer_live(c,name,"accepted title offer")
    if not ok then if o.price>0 then navycraft.shipyard.credit(name,o.price,"failed title transfer refund")end;return false,result end
    if o.price>0 then navycraft.shipyard.credit(o.seller,o.price,"vessel title sale "..o.title_id)end
    o.status="accepted";o.accepted=os.time();persist();return true,"Title "..o.title_id.." transferred to "..name
end
function O.cancel_offer(name,id)
    local o=offers[tostring(id or"")];if not o or o.status~="open"then return false,"open transfer offer not found"end
    if o.seller~=name and not core.check_player_privs(name,{server=true})then return false,"only the seller may cancel this offer"end
    o.status="cancelled";persist();return true,"Transfer offer cancelled"
end
function O.list_for_sale(name,price)
    local c,port,err=port_and_vessel(name);if not c then return false,err end
    local t=O.ensure(c);price=math.max(1,math.floor(tonumber(price)or O.value(c)*.7));local snapshot=navycraft.preview.snapshot(c)
    snapshot.systems=snapshot.systems or{};snapshot.systems.title_id=t.id
    local id=next_id("listing","SL-");listings[id]={id=id,title_id=t.id,seller=name,price=price,port_id=port.id,name=t.name,class=t.class,snapshot=snapshot,status="open",created=os.time()}
    t.status="listed";t.construct_id=nil;t.updated=os.time()
    local ok,msg=navycraft.preview.remove(c.id,false,"sale_escrow");if not ok then listings[id]=nil;t.status="active";return false,msg end
    persist();return true,string.format("Listed %s as %s for %dc",t.name,id,price)
end
function O.buy_listing(name,id)
    local l=listings[tostring(id or"")];if not l or l.status~="open"then return false,"sale listing not found"end
    if l.seller==name then return false,"you already own this vessel"end
    local p=navycraft.campaign.ports.for_player(name);if not p or p.id~=l.port_id then return false,"purchase this vessel at its listing port"end
    local ok,msg=navycraft.campaign.career.spend(name,l.price,"purchase vessel listing "..l.id);if not ok then return false,msg end
    local snapshot=copy(l.snapshot);snapshot.systems=snapshot.systems or{};snapshot.systems.owner=name;snapshot.systems.title_id=l.title_id
    local imported,stored=navycraft.storage.import_snapshot(name,l.name,snapshot,{source="broker_purchase",title_id=l.title_id})
    if not imported then navycraft.shipyard.credit(name,l.price,"failed vessel purchase refund");return false,stored end
    navycraft.shipyard.credit(l.seller,l.price,"vessel broker sale "..l.id)
    local t=titles[l.title_id];if t then t.owner=name;t.status="stored";t.construct_id=nil;t.stored_name=stored;t.history=t.history or{};t.history[#t.history+1]={time=os.time(),from=l.seller,to=name,reason="broker sale"}end
    if navycraft.campaign.insurance then navycraft.campaign.insurance.on_transfer(l.title_id,l.seller,name)end
    l.status="sold";l.buyer=name;l.sold=os.time();persist();return true,"Purchased vessel; stored as "..stored
end
function O.cancel_listing(name,id)
    local l=listings[tostring(id or"")];if not l or l.status~="open"then return false,"open listing not found"end
    if l.seller~=name and not core.check_player_privs(name,{server=true})then return false,"only the seller may cancel this listing"end
    local ok,stored=navycraft.storage.import_snapshot(l.seller,l.name,l.snapshot,{source="broker_return",title_id=l.title_id})
    if not ok then return false,stored end
    l.status="cancelled";local t=titles[l.title_id];if t then t.status="stored";t.stored_name=stored end;persist();return true,"Listing cancelled; vessel stored as "..stored
end
function O.listings_text()
    local out={};for _,l in pairs(listings)do if l.status=="open"then out[#out+1]=string.format("%s %s/%s seller=%s price=%dc port=%s",l.id,l.name,l.class or"?",l.seller,l.price,l.port_id)end end;table.sort(out);return#out>0 and table.concat(out," | ")or"no vessels listed"
end
function O.record_loss(c,kind,owner,snapshot)
    if not c or not c.systems or c.systems.loss_recorded then return nil end
    local t=O.ensure(c);owner=owner or t.owner;c.systems.loss_recorded=true;t.status=kind=="capture"and"captured"or"wrecked";t.lost_at=os.time();t.loss_kind=kind;t.updated=os.time();snapshot=snapshot or navycraft.preview.snapshot(c)
    if kind~="capture"and navycraft.campaign.salvage then navycraft.campaign.salvage.record_loss(c,kind,owner,snapshot,t.id)end
    if navycraft.campaign.insurance then navycraft.campaign.insurance.record_loss(c,kind,owner,snapshot,t.id)end
    if O.loss_mode()=="casual"and kind~="capture"then
        local replacement=copy(snapshot);replacement.systems=replacement.systems or{};replacement.systems.title_id=nil;replacement.systems.hull_integrity=.5;replacement.systems.flooding=0;replacement.systems.sinking=false;replacement.systems.loss_recorded=nil;replacement.systems.ammo={}
        if replacement.systems.cargo then replacement.systems.cargo={}end
        local ok,name=navycraft.storage.import_snapshot(owner,"Emergency Recovery "..t.name,replacement,{source="casual_recovery"});if ok then t.casual_recovery=name end
    end
    persist();return t
end
function O.step()
    for _,c in pairs(navycraft.preview.get_all())do
        if c.systems and not tostring(c.systems.owner or""):match("^NPC:")then
            O.ensure(c)
            local lost=(tonumber(c.systems.hull_integrity)or 1)<=0 or c.systems.sunk or(c.systems.sinking and(c.position.y or 0)<-1000)
            if lost and not c.systems.loss_recorded then O.record_loss(c,"destroyed",c.systems.owner)end
        end
    end
end
function O.on_remove(c,reason,snapshot)
    if ignored_remove[reason]or not c or not c.systems or tostring(c.systems.owner or""):match("^NPC:")then return end
    local lost=reason=="destroyed"or reason=="sunk"or reason=="combat_loss"or(tonumber(c.systems.hull_integrity)or 1)<=0
    if lost then O.record_loss(c,reason,c.systems.owner,snapshot)end
end
function O.transfer_live(c,new_owner,reason)return transfer_live(c,new_owner,reason)end
function O.export()return copy({titles=titles,offers=offers,listings=listings,settings=settings})end
return O

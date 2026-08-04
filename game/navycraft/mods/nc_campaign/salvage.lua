local S={}
local storage=core.get_mod_storage();local wrecks={};local counter=0
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("salvage_m5f_v1",core.serialize({wrecks=wrecks,counter=counter}))end
local function load()local v=core.deserialize(storage:get_string("salvage_m5f_v1"));if type(v)=="table"then wrecks=v.wrecks or{};counter=v.counter or 0 end end;load()
local function find_construct(id)return id and navycraft.preview.get_by_id(id)or nil end
local function has_right(name,w)
    if w.insurer_claimed or w.claimant=="INSURER"then return false end
    if w.claimant==name or w.original_owner==name then return true end
    if w.claimant then return false end
    if os.time()>=(w.exclusive_until or 0)then return true end
    return navycraft.campaign.factions.player_faction(name)=="salvage"
end
function S.save()persist()end
function S.record_loss(c,kind,owner,snapshot,title_id)
    for _,w in pairs(wrecks)do if w.title_id==title_id and w.status=="available"then return copy(w)end end
    counter=counter+1;local id=string.format("WR-%06d",counter);local blocks=tonumber(snapshot.profile and(snapshot.profile.block_count_alive or snapshot.profile.block_count))or#(snapshot.nodes or{})
    wrecks[id]={id=id,title_id=title_id,kind=kind,original_owner=owner,construct_id=c and c.id or nil,position=copy((c and c.position)or snapshot.position or{x=0,y=0,z=0}),snapshot=copy(snapshot),blocks=blocks,status="available",created=os.time(),exclusive_until=os.time()+600}
    persist();return copy(wrecks[id])
end
function S.assign_to_insurer(title_id,claim_id)for _,w in pairs(wrecks)do if w.title_id==title_id and w.status=="available"then w.claimant="INSURER";w.insurer_claimed=true;w.claim_id=claim_id;persist()end end end
function S.claim(name,id)
    local w=wrecks[tostring(id or"")];if not w or w.status~="available"then return false,"wreck not found"end
    if not has_right(name,w)then return false,"exclusive salvage rights belong to "..tostring(w.original_owner)end
    w.claimant=name;w.claimed=os.time();persist();return true,"Salvage rights registered for "..w.id
end
local function award_materials(name,w)
    local player=core.get_player_by_name(name);if not player then return 0,0 end;local inv=player:get_inventory();local crates=math.max(1,math.floor((w.blocks or 1)/25));local plates=math.max(0,math.floor((w.blocks or 0)/40))
    inv:add_item("main","nc_campaign:salvage_crate "..crates);if plates>0 then inv:add_item("main","nc_campaign:hull_plate "..plates)end
    local credits=math.max(5,math.floor((w.blocks or 1)*.35));navycraft.shipyard.credit(name,credits,"salvage recovery "..w.id);navycraft.campaign.career.record_stat(name,"wrecks_salvaged",1)
    return crates,credits
end
function S.recover(name,id)
    local w=wrecks[tostring(id or"")];if not w or w.status~="available"then return false,"wreck not found"end;if not has_right(name,w)then return false,"you do not hold salvage rights"end
    local player=core.get_player_by_name(name);if not player or vector.distance(player:get_pos(),w.position)>20 then return false,"move within 20 nodes of the wreck"end
    local c=find_construct(w.construct_id);if c then navycraft.preview.remove(c.id,false,"salvage_recovery")end
    local crates,credits=award_materials(name,w);w.status="recovered";w.recovered_by=name;w.recovered=os.time();persist();return true,string.format("Recovered %s: %d salvage crate(s), %dc",w.id,crates,credits)
end
function S.restore(name,id)
    local w=wrecks[tostring(id or"")];if not w or w.status~="available"then return false,"wreck not found"end;if not has_right(name,w)then return false,"you do not hold salvage rights"end
    local mode=navycraft.campaign.ownership.loss_mode();if mode=="strict"then return false,"strict loss mode permits material salvage only"end
    local player=core.get_player_by_name(name);if not player or vector.distance(player:get_pos(),w.position)>20 then return false,"move within 20 nodes of the wreck"end
    local port=navycraft.campaign.ports.nearest(w.position,128);if not port then return false,"wreck must be within recovery range of a registered port"end
    local cost=math.max(20,math.floor((w.blocks or 1)*.8));local ok,msg=navycraft.campaign.career.spend(name,cost,"recover wreck "..w.id);if not ok then return false,msg end
    local snap=copy(w.snapshot);snap.systems=snap.systems or{};snap.systems.title_id=nil;snap.systems.owner=name;snap.systems.hull_integrity=.35;snap.systems.flooding=0;snap.systems.sinking=false;snap.systems.loss_recorded=nil;snap.systems.ammo={};snap.systems.cargo={};snap.systems.maintenance=snap.systems.maintenance or{};snap.systems.maintenance.condition=.3
    local imported,stored=navycraft.storage.import_snapshot(name,"Recovered Hulk "..w.id,snap,{source="wreck_restoration"});if not imported then navycraft.shipyard.credit(name,cost,"failed wreck recovery refund");return false,stored end
    local c=find_construct(w.construct_id);if c then navycraft.preview.remove(c.id,false,"salvage_recovery")end
    w.status="restored";w.restored_by=name;w.restored_name=stored;w.restored=os.time();persist();return true,"Wreck restored as stored hulk "..stored
end
function S.list_text(name)
    local out={};for _,w in pairs(wrecks)do if w.status=="available"then local right=has_right(name,w)and"RIGHTS"or"restricted";out[#out+1]=string.format("%s title=%s blocks=%d owner=%s %s at %s",w.id,w.title_id or"?",w.blocks or 0,w.original_owner or"?",right,core.pos_to_string(w.position))end end;table.sort(out);return#out>0 and table.concat(out," | ")or"no recoverable wrecks"
end
function S.get(id)return wrecks[tostring(id or"")]and copy(wrecks[tostring(id)])or nil end
function S.all()return copy(wrecks)end
return S

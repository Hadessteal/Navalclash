local I={}
local storage=core.get_mod_storage();local policies,claims={},{};local counter=0
local plans={
    hull={name="Hull Protection",premium=.08,deductible=.15,capture=false,equipment=false,cargo=false,waiting=60},
    comprehensive={name="Comprehensive Protection",premium=.14,deductible=.10,capture=true,equipment=true,cargo=true,waiting=120},
}
local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("insurance_m5f_v1",core.serialize({policies=policies,claims=claims,counter=counter}))end
local function load()local v=core.deserialize(storage:get_string("insurance_m5f_v1"));if type(v)=="table"then policies=v.policies or{};claims=v.claims or{};counter=v.counter or 0 end end;load()
local function active(name)local c=navycraft.preview.get_for_owner(name);if c and c.systems and c.systems.owner==name then return c end;return nil end
function I.save()persist()end
function I.plans()return copy(plans)end
function I.buy(name,plan_key)
    plan_key=tostring(plan_key or"hull"):lower();local plan=plans[plan_key];if not plan then return false,"insurance plan must be hull or comprehensive"end
    local c=active(name);if not c then return false,"launch an owned vessel first"end;local port=navycraft.campaign.ports.for_player(name);if not port or vector.distance(c.position,port.pos)>port.radius then return false,"insure the vessel while it is inside your port"end
    local title=navycraft.campaign.ownership.ensure(c);local value=navycraft.campaign.ownership.value(c);local premium=math.max(10,math.ceil(value*plan.premium));local deductible=math.max(10,math.ceil(value*plan.deductible))
    local ok,msg=navycraft.campaign.career.spend(name,premium,"vessel insurance premium "..title.id);if not ok then return false,msg end
    policies[title.id]={title_id=title.id,owner=name,plan=plan_key,value=value,premium=premium,deductible=deductible,issued=os.time(),matures=os.time()+plan.waiting,status="active",snapshot=navycraft.preview.snapshot(c),port_id=port.id}
    persist();return true,string.format("%s issued for %s: premium %dc, deductible %dc",plan.name,title.id,premium,deductible)
end
function I.force_mature(title_id)if policies[title_id]then policies[title_id].matures=0;persist();return true end;return false end
function I.on_transfer(title_id,old_owner,new_owner)
    local p=policies[title_id];if p and p.status=="active"then p.status="cancelled_transfer";p.cancelled=os.time();p.transferred_to=new_owner;persist()end
end
function I.record_loss(c,kind,owner,snapshot,title_id)
    local p=policies[title_id];if not p or p.status~="active"or p.owner~=owner then return nil end
    local plan=plans[p.plan];if os.time()<(p.matures or 0)then p.status="void_waiting_period";persist();return nil end
    if kind=="capture"and not plan.capture then p.status="excluded_capture";persist();return nil end
    counter=counter+1;local id=string.format("CL-%06d",counter)
    claims[id]={id=id,title_id=title_id,owner=owner,kind=kind,status="approved",deductible=p.deductible,value=p.value,plan=p.plan,snapshot=copy(snapshot or p.snapshot),created=os.time()}
    p.status="claimed";p.claim_id=id;p.claimed=os.time();persist();return copy(claims[id])
end
local function replacement_snapshot(claim)
    local s=copy(claim.snapshot);s.id=nil;s.native_id=nil;s.position=nil;s.systems=s.systems or{};s.systems.title_id=nil;s.systems.owner=claim.owner;s.systems.loss_recorded=nil;s.systems.hull_integrity=1;s.systems.flooding=0;s.systems.sinking=false;s.systems.helm_destroyed=false;s.systems.scuttle_at=0;s.systems.ammo={}
    if not plans[claim.plan].cargo then s.systems.cargo={}end
    if not plans[claim.plan].equipment then s.systems.equipment={}end
    s.systems.maintenance=s.systems.maintenance or{};s.systems.maintenance.condition=.8;s.systems.maintenance.accrued_cost=0
    return s
end
function I.file(name,claim_id)
    local claim=claims[tostring(claim_id or"")];if not claim or claim.owner~=name then return false,"insurance claim not found"end
    if claim.status~="approved"then return false,"claim is "..tostring(claim.status)end
    local port=navycraft.campaign.ports.for_player(name);if not port then return false,"file the claim inside a registered port"end
    local ok,msg=navycraft.campaign.career.spend(name,claim.deductible,"insurance deductible "..claim.id);if not ok then return false,msg end
    local imported,stored=navycraft.storage.import_snapshot(name,"Insurance Replacement "..claim.title_id,replacement_snapshot(claim),{source="insurance_claim"})
    if not imported then navycraft.shipyard.credit(name,claim.deductible,"failed insurance claim refund");return false,stored end
    claim.status="settled";claim.settled=os.time();claim.replacement_name=stored;claim.port_id=port.id
    if navycraft.campaign.salvage then navycraft.campaign.salvage.assign_to_insurer(claim.title_id,claim.id)end
    persist();return true,"Claim settled; replacement stored as "..stored
end
function I.status(name)
    local out={};for id,p in pairs(policies)do if p.owner==name then out[#out+1]=string.format("%s %s %s premium=%dc deductible=%dc",id,p.plan,p.status,p.premium,p.deductible)end end
    for id,c in pairs(claims)do if c.owner==name and c.status~="settled"then out[#out+1]=string.format("%s claim %s %s deductible=%dc",id,c.kind,c.status,c.deductible)end end
    table.sort(out);return#out>0 and table.concat(out," | ")or"no vessel insurance or claims"
end
function I.claims_for(name)local out={};for _,c in pairs(claims)do if c.owner==name then out[#out+1]=copy(c)end end;return out end
return I

local M={}
local function clamp(v,a,b)return math.max(a,math.min(b,v))end
local function active(name)local c=navycraft.preview.get_for_owner(name);if not c then return nil,"launch or command a vessel first"end;return c end
local function ensure(c)
    c.systems.maintenance=c.systems.maintenance or{condition=1,operating_hours=0,accrued_cost=0,last_service=os.time(),cycles=0}
    local m=c.systems.maintenance;m.condition=clamp(tonumber(m.condition)or 1,0,1);return m
end
function M.apply(c)
    if not c or not c.systems then return nil end;local m=ensure(c);local condition=m.condition
    c.systems.maintenance_speed_mult=.55+.45*condition;c.systems.maintenance_system_mult=.50+.50*condition;c.systems.maintenance_reliability=.35+.65*condition
    return m
end
function M.step(c,dt)
    if not c or not c.systems then return end;local m=M.apply(c);dt=math.max(0,tonumber(dt)or 0)
    local running=c.systems.engines_on or math.abs(c.forward_speed or 0)>.05;if not running then return end
    local class=navycraft.campaign.classes.apply(c);local eq=c.systems.equipment_modifiers or{};local rate=(class and class.service or 10)*(eq.maintenance_mult or 1)
    local hours=dt/3600;m.operating_hours=m.operating_hours+hours;m.accrued_cost=m.accrued_cost+hours*rate
    local strain=1+math.min(1,math.abs(c.forward_speed or 0)/math.max(.1,c.systems.top_speed or 1))*.5
    if c.systems.overloaded then strain=strain*1.75 end
    m.condition=clamp(m.condition-hours*.012*strain*(eq.maintenance_mult or 1),0,1);m.cycles=(m.cycles or 0)+1;M.apply(c)
end
function M.quote(c)
    local m=ensure(c);local class=navycraft.campaign.classes.apply(c);local base=class and class.service or 10
    return math.max(10,math.ceil((m.accrued_cost or 0)+(1-m.condition)*base*12)),math.max(0,math.ceil((1-m.condition)*4))
end
function M.service(name)
    local c,err=active(name);if not c then return false,err end;local p=navycraft.campaign.ports.for_player(name);if not p then return false,"stand inside a registered port"end;if vector.distance(c.position,p.pos)>p.radius then return false,"bring the vessel inside the same port"end
    local cost,parts=M.quote(c);if navycraft.campaign.supply then local available,mult,message=navycraft.campaign.supply.service_status(p.id,"repair",math.max(.5,parts+cost/50));if not available then return false,message end;cost=math.ceil(cost*(mult or 1))end;local inv=core.get_player_by_name(name):get_inventory();if parts>0 and not inv:contains_item("main","nc_campaign:repair_parts "..parts)then return false,"maintenance requires repair_parts x"..parts end
    local ok,msg=navycraft.campaign.career.spend(name,cost,"scheduled vessel maintenance");if not ok then return false,msg end
    if parts>0 then inv:remove_item("main","nc_campaign:repair_parts "..parts)end
    if navycraft.campaign.supply then navycraft.campaign.supply.consume_service(p.id,"repair",math.max(.5,parts+cost/50),"scheduled maintenance")end
    local m=ensure(c);m.condition=1;m.accrued_cost=0;m.last_service=os.time();m.services=(m.services or 0)+1;M.apply(c);navycraft.preview.save();navycraft.campaign.career.record_stat(name,"maintenance_services",1)
    return true,string.format("Maintenance completed for %d credits and %d repair part(s)",cost,parts)
end
function M.status(c)
    if not c then return"no active vessel"end;local m=M.apply(c);local cost,parts=M.quote(c)
    return string.format("maintenance condition=%d%% hours=%.2f accrued=%.1fc service_quote=%dc/%d parts speed=%.2fx systems=%.2fx",math.floor(m.condition*100+.5),m.operating_hours or 0,m.accrued_cost or 0,cost,parts,c.systems.maintenance_speed_mult,c.systems.maintenance_system_mult)
end
return M

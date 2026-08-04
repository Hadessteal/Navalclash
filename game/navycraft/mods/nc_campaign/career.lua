local C={}
local storage=core.get_mod_storage()
local accounts={}
local SCHEMA=1
local rank_unlocks={
    recruit={"boat","ship"},
    sailor={"submarine","airship"},
    petty_officer={"tank"},
    officer={"aircraft"},
    captain={"formation_command","high_value_contracts"},
    admiral={"fleet_command","capital_contracts"},
}
local rank_order={recruit=1,sailor=2,petty_officer=3,officer=4,captain=5,admiral=6}

local function copy(v)return core.deserialize(core.serialize(v))end
local function persist()storage:set_string("career_v1",core.serialize({schema=SCHEMA,accounts=accounts}))end
local function load()
    local decoded=core.deserialize(storage:get_string("career_v1"))
    if type(decoded)=="table" then accounts=decoded.accounts or decoded end
end
load()

function C.ensure(name)
    local account=accounts[name]
    if not account then
        account={started=false,reputation={navy=0,merchant=0,salvage=0},stats={distance=0,missions=0,combat_damage=0,cargo_delivered=0,credits_earned=0,credits_spent=0},completed={},created=os.time()}
        accounts[name]=account
    end
    account.reputation=account.reputation or {navy=0,merchant=0,salvage=0}
    account.stats=account.stats or {}
    account.completed=account.completed or {}
    return account
end
function C.save()persist()end
function C.get(name)return copy(C.ensure(name))end
function C.rank(name)return navycraft.shipyard.get_rank(name)end
function C.balance(name)return navycraft.shipyard.balance(name)end
function C.has_rank(name,required)
    local rank=C.rank(name);return (rank_order[rank.name]or 1)>=(rank_order[required]or 1)
end
function C.unlocks(name)
    local current=rank_order[C.rank(name).name]or 1;local out={}
    for rank,items in pairs(rank_unlocks)do if (rank_order[rank]or 99)<=current then for _,item in ipairs(items)do out[#out+1]=item end end end
    table.sort(out);return out
end
function C.start(name)
    local account=C.ensure(name)
    if account.started then return false,"career already started"end
    account.started=true;account.started_at=os.time();if navycraft.campaign and navycraft.campaign.blueprints then navycraft.campaign.blueprints.get(name) end
    navycraft.shipyard.credit(name,250,"career starter grant")
    navycraft.shipyard.reward_experience(name,25,"career enlistment")
    local player=core.get_player_by_name(name)
    if player then
        local inv=player:get_inventory()
        inv:add_item("main","nc_navycraft:cannon_shell 8")
        inv:add_item("main","nc_campaign:repair_parts 4")
    end
    persist();return true,"Career started: 250 credits, basic ammunition, and repair parts issued"
end
function C.add_reputation(name,faction,amount)
    local account=C.ensure(name);faction=faction or"navy";amount=math.floor(tonumber(amount)or 0)
    account.reputation[faction]=math.max(-1000,(account.reputation[faction]or 0)+amount);persist();return account.reputation[faction]
end
function C.award(name,reward,reason)
    reward=reward or{};local account=C.ensure(name)
    local credits=math.max(0,math.floor(tonumber(reward.credits)or 0));local xp=math.max(0,math.floor(tonumber(reward.xp)or 0))
    if credits>0 then navycraft.shipyard.credit(name,credits,reason or"mission reward");account.stats.credits_earned=(account.stats.credits_earned or 0)+credits end
    if xp>0 then navycraft.shipyard.reward_experience(name,xp,reason or"mission reward")end
    if reward.reputation then for faction,amount in pairs(reward.reputation)do C.add_reputation(name,faction,amount)end end
    persist();return credits,xp
end
function C.spend(name,amount,reason)
    local ok,result=navycraft.shipyard.debit(name,amount,reason)
    if ok then local account=C.ensure(name);account.stats.credits_spent=(account.stats.credits_spent or 0)+math.floor(amount);persist()end
    return ok,result
end
function C.record_stat(name,key,amount)
    local account=C.ensure(name);account.stats[key]=(account.stats[key]or 0)+(tonumber(amount)or 1);persist();return account.stats[key]
end
function C.mark_completed(name,key)
    local account=C.ensure(name);account.completed[key]=(account.completed[key]or 0)+1;account.stats.missions=(account.stats.missions or 0)+1;persist()
end
function C.status(name)
    local a=C.ensure(name);local rank=C.rank(name);local rep=a.reputation
    return string.format("%s | rank=%s xp=%d credits=%d | missions=%d distance=%.0f | reputation navy=%d merchant=%d salvage=%d | unlocks=%s",
        a.started and"ACTIVE"or"NOT STARTED",rank.name,navycraft.shipyard.get_player_state(name).experience,C.balance(name),a.stats.missions or 0,a.stats.distance or 0,
        rep.navy or 0,rep.merchant or 0,rep.salvage or 0,table.concat(C.unlocks(name),","))
end
return C

local C=navycraft.campaign
local function words(param)local t={};for w in(param or""):gmatch("%S+")do t[#t+1]=w end;return t end
core.register_chatcommand("career",{params="start|status|ledger|unlocks",description="NavyCraft career progression",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="start"then return C.career.start(name)elseif cmd=="status"then return true,C.career.status(name)
    elseif cmd=="unlocks"then return true,table.concat(C.career.unlocks(name),", ")
    elseif cmd=="ledger"then local out={};for _,e in ipairs(navycraft.shipyard.transaction_history(name,a[2]))do out[#out+1]=string.format("%+d %s => %d",e.amount,e.reason,e.balance)end;return true,#out>0 and table.concat(out," | ")or"no transactions"end
    return false,"/career start|status|ledger|unlocks"end})
core.register_chatcommand("mission",{params="list|accept <id>|status|abandon",description="NavyCraft mission contracts",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="status"then return true,C.missions.status(name)elseif cmd=="abandon"then return C.missions.abandon(name)
    elseif cmd=="list"then local p=C.ports.for_player(name);if not p then return false,"stand inside a registered port"end;local text=C.missions.list_text(name,p.id);return true,text
    elseif cmd=="accept"then return C.missions.accept(name,a[2]or"")end
    return false,"/mission list|accept <id>|status|abandon"end})
core.register_chatcommand("missions",{params="list|accept <id>|status|abandon",description="Mission command alias",privs={interact=true},func=function(name,param)return core.registered_chatcommands.mission.func(name,param)end})
core.register_chatcommand("port",{params="list|info|create <id> <name>|remove <id>|tp <id>",description="Naval port administration",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower()
    if cmd=="list"then return true,C.ports.list_text()elseif cmd=="info"then local p=C.ports.for_player(name);return p and true or false,p and string.format("%s/%s radius=%d faction=%s",p.id,p.name,p.radius,p.faction)or"not inside a port"
    elseif cmd=="create"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local player=core.get_player_by_name(name);local ok,p=C.ports.create(a[2]or"",table.concat(a," ",3),player:get_pos());return ok,ok and("Created port "..p.id)or p
    elseif cmd=="remove"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;return C.ports.remove(a[2])
    elseif cmd=="tp"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local p=C.ports.get(a[2]);if not p then return false,"port not found"end;core.get_player_by_name(name):set_pos(vector.add(p.pos,{x=0,y=2,z=0}));return true,"Teleported to "..p.name end
    return false,"/port list|info|create|remove|tp"end})
core.register_chatcommand("navystore",{params="list|buy <item> [units]|repair|pumps",description="Port quartermaster services",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower();if cmd=="list"then return true,C.economy.list_text(name)elseif cmd=="buy"then return C.economy.buy(name,a[2],a[3])elseif cmd=="repair"then return C.economy.repair(name)elseif cmd=="pumps"then return C.economy.recharge_pumps(name)end;return false,"/navystore list|buy|repair|pumps"end})
core.register_chatcommand("crewstation",{params="roster|assign <player> <role>",description="Assign vessel crew stations",privs={interact=true},func=function(name,param)
    local c=navycraft.preview.get_for_owner(name);if not c then for _,x in pairs(navycraft.preview.get_all())do if x.systems and(name==x.systems.captain or(x.systems.crew or{})[name])then c=x;break end end end;if not c then return false,"no commanded vessel"end
    local a=words(param);local cmd=(a[1]or"roster"):lower();if cmd=="roster"then return true,C.crew.roster(c)elseif cmd=="assign"then return C.crew.assign(c,name,a[2],a[3])end;return false,"/crewstation roster|assign <player> <captain|executive|helm|engineer|gunner|sensor|quartermaster|deck|passenger>"end})

core.register_chatcommand("faction",{params="list|status|join <navy|merchant|salvage>",description="Faction allegiance and diplomacy",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="list"then return true,C.factions.list_text()elseif cmd=="status"then return true,C.factions.status(name)elseif cmd=="join"then return C.factions.join(name,a[2])end
    return false,"/faction list|status|join <navy|merchant|salvage>"end})
core.register_chatcommand("territory",{params="list|status [port]|set <port> <faction>|contest <port> <faction> <amount>",description="Territorial control diagnostics",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower()
    if cmd=="list"then return true,C.territories.list_text()
    elseif cmd=="status"then local id=a[2];if not id then local player=core.get_player_by_name(name);local t=player and C.territories.at_position(player:get_pos());id=t and t.id end;return true,id and C.territories.status(id)or"not inside a territory"
    elseif cmd=="set"or cmd=="contest"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;if cmd=="set"then return C.territories.set_owner(a[2],a[3],"administrator")else return C.territories.contest(a[2],a[3],a[4])end end
    return false,"/territory list|status [port]|set <port> <faction>|contest <port> <faction> <amount>"end})
core.register_chatcommand("operation",{params="status|list|accept [offer-id]|reset",description="Connected NavyCraft operations",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="status"then return true,C.operations.status(name)
    elseif cmd=="list"then local p=C.ports.for_player(name);if not p then return false,"stand inside a registered port"end;local offers=C.operations.offers_for(p.id,name);if #offers==0 then return true,"no operation stage available"end;local out={};for i,o in ipairs(offers)do out[#out+1]=string.format("%d) %s [%s] %dc/%dxp",i,o.title,o.required_rank,o.reward.credits,o.reward.xp)end;return true,table.concat(out," | ")
    elseif cmd=="accept"then local p=C.ports.for_player(name);if not p then return false,"stand inside a registered port"end;local offers=C.operations.offers_for(p.id,name);local o=offers[tonumber(a[2])or 1];if a[2]and not tonumber(a[2])then for _,candidate in ipairs(offers)do if candidate.id==a[2]then o=candidate end end end;if not o then return false,"operation offer not found"end;return C.missions.accept_offer(name,o)
    elseif cmd=="reset"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;return C.operations.reset(name)end
    return false,"/operation status|list|accept [offer-id]|reset"end})
core.register_chatcommand("fleet",{params="list|status <id>|spawn <faction> [difficulty]|clear <id>",description="NPC fleet encounters",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower()
    if cmd=="list"then return true,C.fleets.list_text()elseif cmd=="status"then return true,C.fleets.status(a[2])
    elseif cmd=="spawn"or cmd=="clear"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;if cmd=="clear"then return C.fleets.clear(a[2])end;local player=core.get_player_by_name(name);local encounter,err=C.fleets.spawn(a[2]or"corsair",vector.add(player:get_pos(),{x=80,y=0,z=0}),tonumber(a[3])or 1,{kind="admin"});return encounter and true or false,encounter and C.fleets.status(encounter.id)or err end
    return false,"/fleet list|status <id>|spawn <faction> [difficulty]|clear <id>"end})

core.register_chatcommand("market",{params="list|buy <commodity> [amount]|sell <commodity> [amount]|routes [commodity]",description="Trade dynamic port commodities",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower();local port=C.ports.for_player(name)
    if cmd=="list"then return port and true or false,port and C.markets.list_text(port.id)or"stand inside a registered port"
    elseif cmd=="buy"then return C.markets.buy(name,a[2],a[3])elseif cmd=="sell"then return C.markets.sell(name,a[2],a[3])
    elseif cmd=="routes"then return true,C.markets.route_text(name,a[2])end;return false,"/market list|buy <commodity> [amount]|sell <commodity> [amount]|routes [commodity]"end})
core.register_chatcommand("warehouse",{params="status [port]",description="Inspect port commodity inventory",privs={interact=true},func=function(name,param)
    local a=words(param);local port=a[2]and C.ports.get(a[2])or C.ports.for_player(name);if not port then return false,"port not found"end;return true,C.markets.list_text(port.id)end})
core.register_chatcommand("resource",{params="list|create <id> <kind> [reserve]|extract [id] [cycles]",description="Survey and extract resource sites",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower();if cmd=="list"then return true,C.resources.list_text()
    elseif cmd=="extract"then return C.resources.extract(name,a[2],a[3])
    elseif cmd=="create"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local player=core.get_player_by_name(name);local ok,result=C.resources.create(a[2],a[3],player:get_pos(),a[4]);return ok,ok and(string.format("Created %s resource site",result.id))or result end
    return false,"/resource list|create <id> <iron|coal|copper|oil|salvage> [reserve]|extract [id] [cycles]"end})
core.register_chatcommand("industry",{params="list|queue <recipe> [batches]|status|collect",description="Refine resources and manufacture naval equipment",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower();if cmd=="list"then return true,C.industry.list_text(name)
    elseif cmd=="queue"then return C.industry.queue(name,a[2],a[3])elseif cmd=="status"then return true,C.industry.status(name)
    elseif cmd=="collect"then return C.industry.collect(name)end;return false,"/industry list|queue <recipe> [batches]|status|collect"end})
core.register_chatcommand("equipment",{params="list|status|install <package>",description="Install manufactured vessel equipment",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower();local c=navycraft.preview.get_for_owner(name)
    if cmd=="list"then return true,C.equipment.list_text(name)elseif cmd=="status"then return true,C.equipment.status(c)
    elseif cmd=="install"then return C.equipment.install(name,a[2])end;return false,"/equipment list|status|install <package>"end})


core.register_chatcommand("vesselclass",{params="list|status|certify <class>",description="Inspect and certify vessel classes",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower();local c=navycraft.preview.get_for_owner(name)
    if cmd=="list"then return true,C.classes.list_text(name)elseif cmd=="status"then return true,C.classes.status(c)elseif cmd=="certify"then return C.blueprints.certify(name,a[2])end
    return false,"/vesselclass list|status|certify <class>"end})
core.register_chatcommand("blueprint",{params="list|status|research <class>",description="Research vessel blueprints",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower();if cmd=="list"then return true,C.blueprints.list_text(name)elseif cmd=="status"then return true,C.blueprints.status(name)elseif cmd=="research"then return C.blueprints.research(name,a[2])end
    return false,"/blueprint list|status|research <class>"end})
core.register_chatcommand("cargo",{params="status|load <commodity> [amount]|unload <commodity> [amount]",description="Manage vessel cargo capacity and mass",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower();if cmd=="status"then return true,C.logistics.status(navycraft.preview.get_for_owner(name))elseif cmd=="load"then return C.logistics.load(name,a[2],a[3])elseif cmd=="unload"then return C.logistics.unload(name,a[2],a[3])end
    return false,"/cargo status|load <commodity> [amount]|unload <commodity> [amount]"end})
core.register_chatcommand("maintenance",{params="status|service",description="Inspect or service vessel condition",privs={interact=true},func=function(name,param)
    local cmd=((words(param)[1])or"status"):lower();local c=navycraft.preview.get_for_owner(name);if cmd=="status"then return true,C.maintenance.status(c)elseif cmd=="service"then return C.maintenance.service(name)end;return false,"/maintenance status|service"end})

core.register_chatcommand("supply",{params="list|status [port]|set <port> <category> <0-100>",description="Inspect strategic port supply and readiness",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="list"then return true,C.supply.list_text()
    elseif cmd=="status"then local port=a[2]and C.ports.get(a[2])or C.ports.for_player(name);return port and true or false,port and C.supply.status(port.id)or"port not found"
    elseif cmd=="set"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;local ok,result=C.supply.set(a[2],a[3],a[4]);return ok,ok and C.supply.status(a[2])or result end
    return false,"/supply list|status [port]|set <port> <fuel|provisions|munitions|repair|industry> <0-100>"end})
core.register_chatcommand("convoy",{params="list|status <id>|accept <id>|dispatch <origin> <destination> [commodity amount...]|raid <id> [difficulty]",description="Merchant convoy and escort operations",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower()
    if cmd=="list"then return true,C.shipping.list_text()
    elseif cmd=="status"then return true,C.shipping.status(a[2])
    elseif cmd=="accept"then return C.shipping.accept(name,a[2])
    elseif cmd=="raid"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;return C.shipping.raid(a[2],a[3])
    elseif cmd=="dispatch"then
        if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end
        local manifest=nil;if a[4]then manifest={};local i=4;while i<=#a do local key=(a[i]or""):lower();local amount=tonumber(a[i+1]);if key~=""and amount then manifest[key]=math.floor(amount)end;i=i+2 end end
        local shipment,err=C.shipping.dispatch(a[2],a[3],manifest,{auto_raid=true});return shipment and true or false,shipment and C.shipping.status(shipment.id)or err
    end
    return false,"/convoy list|status <id>|accept <id>|dispatch <origin> <destination> [commodity amount...]|raid <id> [difficulty]"end})
core.register_chatcommand("shipping",{params="list|status <id>|accept <id>",description="Convoy command alias",privs={interact=true},func=function(name,param)return core.registered_chatcommands.convoy.func(name,param)end})

core.register_chatcommand("title",{params="status|offer <player> [price]|accept <offer>|cancel <offer>",description="Vessel title and direct ownership transfers",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="status"then return true,C.ownership.status(name)elseif cmd=="offer"then return C.ownership.offer(name,a[2],a[3])elseif cmd=="accept"then return C.ownership.accept(name,a[2])elseif cmd=="cancel"then return C.ownership.cancel_offer(name,a[2])end
    return false,"/title status|offer <player> [price]|accept <offer>|cancel <offer>"end})
core.register_chatcommand("shipmarket",{params="list|sell <price>|buy <listing>|cancel <listing>",description="Brokered vessel resale with title escrow",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower()
    if cmd=="list"then return true,C.ownership.listings_text()elseif cmd=="sell"then return C.ownership.list_for_sale(name,a[2])elseif cmd=="buy"then return C.ownership.buy_listing(name,a[2])elseif cmd=="cancel"then return C.ownership.cancel_listing(name,a[2])end
    return false,"/shipmarket list|sell <price>|buy <listing>|cancel <listing>"end})
core.register_chatcommand("insurance",{params="plans|buy <hull|comprehensive>|status|claim <claim-id>",description="Vessel insurance policies and claims",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower()
    if cmd=="plans"then local out={};for key,p in pairs(C.insurance.plans())do out[#out+1]=string.format("%s=%s premium=%d%% deductible=%d%% capture=%s",key,p.name,p.premium*100,p.deductible*100,p.capture and"yes"or"no")end;table.sort(out);return true,table.concat(out," | ")
    elseif cmd=="buy"then return C.insurance.buy(name,a[2])elseif cmd=="status"then return true,C.insurance.status(name)elseif cmd=="claim"then return C.insurance.file(name,a[2])end
    return false,"/insurance plans|buy <hull|comprehensive>|status|claim <claim-id>"end})
core.register_chatcommand("salvage",{params="list|claim <wreck>|recover <wreck>|restore <wreck>",description="Wreck rights, material salvage and hulk recovery",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"list"):lower()
    if cmd=="list"then return true,C.salvage.list_text(name)elseif cmd=="claim"then return C.salvage.claim(name,a[2])elseif cmd=="recover"then return C.salvage.recover(name,a[2])elseif cmd=="restore"then return C.salvage.restore(name,a[2])end
    return false,"/salvage list|claim <wreck>|recover <wreck>|restore <wreck>"end})
core.register_chatcommand("capture",{params="start|status|abort",description="Board and capture disabled hostile vessels",privs={interact=true},func=function(name,param)
    local cmd=((words(param)[1])or"status"):lower();if cmd=="start"then return C.capture.start(name)elseif cmd=="status"then return true,C.capture.status(name)elseif cmd=="abort"then return C.capture.abort(name)end;return false,"/capture start|status|abort"end})
core.register_chatcommand("lossmode",{params="status|set <casual|standard|strict>",description="Configure vessel permanent-loss rules",privs={interact=true},func=function(name,param)
    local a=words(param);local cmd=(a[1]or"status"):lower();if cmd=="status"then return true,"Permanent-loss mode: "..C.ownership.loss_mode()elseif cmd=="set"then if not core.check_player_privs(name,{server=true})then return false,"server privilege required"end;return C.ownership.set_loss_mode(a[2])end;return false,"/lossmode status|set <casual|standard|strict>"end})

core.register_chatcommand("station",{params="[command|helm|engineering|weapons|sensors|missions|settings]",description="Open the NavyCraft station interface",privs={interact=true},func=function(name,param)
 local tabs={command=1,helm=2,engineering=3,engineer=3,weapons=4,gunnery=4,sensors=5,missions=6,settings=7};return C.interfaces.open(name,tabs[(param or""):lower()]or 1),"Station interface opened"end})
core.register_chatcommand("bridge",{params="",description="Station interface alias",privs={interact=true},func=function(name,param)return core.registered_chatcommands.station.func(name,param)end})
core.register_chatcommand("tutorial",{params="status|hint|reset|dismiss|resume",description="NavyCraft basic training",privs={interact=true},func=function(name,param)
 local cmd=((param or"status"):match("^%S+")or"status"):lower();if cmd=="status"then return true,C.tutorial.status(name)elseif cmd=="hint"then return true,C.tutorial.hint(name)elseif cmd=="reset"then return C.tutorial.reset(name)elseif cmd=="dismiss"then return C.tutorial.dismiss(name)elseif cmd=="resume"then return C.tutorial.resume(name)end;return false,"/tutorial status|hint|reset|dismiss|resume"end})
core.register_chatcommand("accessibility",{params="status|<setting> <value>",description="HUD and accessibility settings",privs={interact=true},func=function(name,param)
 local a=words(param);if not a[1]or a[1]=="status"then return true,C.presentation.status(name)end;local ok,msg;if a[2]then ok,msg=C.presentation.set(name,a[1],a[2])else ok,msg=C.presentation.toggle(name,a[1])end;if ok then C.hud.rebuild(name)end;return ok,msg end})
core.register_chatcommand("hud",{params="full|minimal|off",description="Set the NavyCraft HUD mode",privs={interact=true},func=function(name,param)local value=(param or""):lower();if value==""then return true,C.presentation.status(name)end;local ok,msg=C.presentation.set(name,"hud_mode",value);if ok then C.hud.rebuild(name)end;return ok,msg end})
core.register_chatcommand("notificationlog",{params="",description="Show recent NavyCraft alerts",privs={interact=true},func=function(name)
 local out={};for _,entry in ipairs(C.notifications.history(name))do out[#out+1]=entry.severity:upper()..": "..entry.text end;while#out>8 do table.remove(out,1)end;return true,#out>0 and table.concat(out," | ")or"no recent alerts"end})

local root=arg[1] or error("game root required")
local serial_values={};local serial_counter=0
local world={};local metas={};local current_mod="";local storages={};local players={};local objects={}
local function pkey(p)return math.floor(p.x+.5)..":"..math.floor(p.y+.5)..":"..math.floor(p.z+.5)end
local function deepcopy(v,seen)if type(v)~="table"then return v end;seen=seen or{};if seen[v]then return seen[v]end;local r={};seen[v]=r;for k,x in pairs(v)do r[deepcopy(k,seen)]=deepcopy(x,seen)end;return r end
vector={}
function vector.new(v)return{x=v.x or 0,y=v.y or 0,z=v.z or 0}end
local function bin(a,b,op)if type(b)=="number"then return{x=op(a.x,b),y=op(a.y,b),z=op(a.z,b)}end;return{x=op(a.x,b.x),y=op(a.y,b.y),z=op(a.z,b.z)}end
function vector.add(a,b)return bin(a,b,function(x,y)return x+y end)end
function vector.subtract(a,b)return bin(a,b,function(x,y)return x-y end)end
function vector.multiply(a,b)return bin(a,b,function(x,y)return x*y end)end
function vector.distance(a,b)local d=vector.subtract(a,b);return math.sqrt(d.x*d.x+d.y*d.y+d.z*d.z)end
function vector.length(a)return math.sqrt(a.x*a.x+a.y*a.y+a.z*a.z)end
function vector.normalize(a)local l=vector.length(a);return l==0 and{x=0,y=0,z=0}or vector.multiply(a,1/l)end
function vector.direction(a,b)return vector.normalize(vector.subtract(b,a))end
function vector.round(a)return{x=math.floor(a.x+.5),y=math.floor(a.y+.5),z=math.floor(a.z+.5)}end
function vector.floor(a)return{x=math.floor(a.x),y=math.floor(a.y),z=math.floor(a.z)}end
function vector.ceil(a)return{x=math.ceil(a.x),y=math.ceil(a.y),z=math.ceil(a.z)}end
core={registered_nodes={},registered_items={},registered_entities={},registered_chatcommands={},callbacks={}}
core.settings={get=function()return nil end,get_bool=function()return false end}
local stack_mt={}
stack_mt.__index=stack_mt
local function parse_stack(value)
 if type(value)=="table" and getmetatable(value)==stack_mt then return value end
 local text=tostring(value or "");local name,count,wear=text:match("^%s*([^%s]*)%s*(%d*)%s*(%d*)")
 return setmetatable({name=name or"",count=tonumber(count)or((name and name~="")and 1 or 0),wear=tonumber(wear)or 0,meta={}},stack_mt)
end
function ItemStack(value)return parse_stack(value)end
function stack_mt:get_name()return self.name end
function stack_mt:get_count()return self.count end
function stack_mt:get_wear()return self.wear end
function stack_mt:get_tool_capabilities()return{groupcaps={cracky={times={[1]=1},uses=20,maxlevel=1}}}end
function stack_mt:add_wear(value)self.wear=math.min(65535,self.wear+(tonumber(value)or 0))end
function stack_mt:take_item(count)self.count=math.max(0,self.count-(count or 1));if self.count==0 then self.name="" end;return self end
function stack_mt:to_string()if self.name==""or self.count<=0 then return""end;return self.name.." "..self.count..(self.wear>0 and(" "..self.wear)or"")end
function stack_mt:get_meta()return{get_int=function(_,key)return tonumber(self.meta[key])or 0 end,set_int=function(_,key,value)self.meta[key]=value end}end
stack_mt.__tostring=stack_mt.to_string
function core.get_current_modname()return current_mod end
function core.get_modpath(name)return root.."/mods/"..name end
function core.serialize(v)serial_counter=serial_counter+1;local k="SER"..serial_counter;serial_values[k]=deepcopy(v);return k end
function core.deserialize(k)if type(k)=="table"then return deepcopy(k)end;return deepcopy(serial_values[k])end
function core.get_mod_storage()storages[current_mod]=storages[current_mod]or{};local t=storages[current_mod];return{get_string=function(_,k)return t[k]or""end,set_string=function(_,k,v)t[k]=v end,get_int=function(_,k)return tonumber(t[k])or 0 end,set_int=function(_,k,v)t[k]=v end}end
function core.register_node(name,def)core.registered_nodes[name]=def;core.registered_items[name]=def end
function core.register_craftitem(name,def)core.registered_items[name]=def end
function core.override_item(name,def)core.registered_items[name]=core.registered_items[name]or{};for k,v in pairs(def)do core.registered_items[name][k]=v end end
function core.register_entity(name,def)core.registered_entities[name]=def end
function core.register_chatcommand(name,def)core.registered_chatcommands[name]=def end
function core.register_on_chatcommand(f)core.callbacks.on_chatcommand=core.callbacks.on_chatcommand or{};table.insert(core.callbacks.on_chatcommand,f)end
function core.register_alias()end;function core.register_craft()end
function core.register_globalstep(f)table.insert(core.callbacks,f)end
function core.after(_,f,...)return f(...)end
for _,name in ipairs{"on_mods_loaded","on_shutdown","on_joinplayer","on_leaveplayer","on_player_receive_fields","on_placenode","on_dignode"}do core["register_"..name]=function(f)core.callbacks[name]=core.callbacks[name]or{};table.insert(core.callbacks[name],f)end end
function core.log(_,message)io.stderr:write(message.."\n")end
local chat_messages={}
function core.chat_send_player(name,message)chat_messages[#chat_messages+1]={name=name,message=tostring(message)}end
function core.pos_to_string(p)return string.format("(%g,%g,%g)",p.x,p.y,p.z)end
function core.formspec_escape(v)return tostring(v):gsub("\\","\\\\"):gsub("%]","\\]"):gsub("%[","\\["):gsub(";","\\;"):gsub(",","\\,")end
function core.check_player_privs(_,privs)return true,{} end
function core.get_player_privs()return{interact=true}end
function core.set_player_privs()return true end
function core.is_protected()return false end
function core.record_protection_violation()end
function core.is_creative_enabled()return false end
function core.get_node_drops(name)return{name}end
function core.get_dig_params()return{diggable=true,time=0.1,wear=10}end
function core.handle_node_drops(_,drops,player)local inv=player and player:get_inventory();if inv then for _,drop in ipairs(drops)do inv:add_item("main",drop)end end end
function core.dir_to_facedir()return 0 end
function core.dir_to_wallmounted()return 1 end
function core.get_worldpath()return"/tmp/navycraft-smoke"end
function core.get_node_or_nil(pos)return deepcopy(world[pkey(pos)]or{name="air",param1=0,param2=0})end
function core.set_node(pos,node)world[pkey(pos)]=deepcopy(node)end
function core.remove_node(pos)world[pkey(pos)]={name="air",param1=0,param2=0}end
local function meta(pos)local k=pkey(pos);metas[k]=metas[k]or{fields={},inventory={}};local data=metas[k];return{set_string=function(_,n,v)data.fields[n]=v end,get_string=function(_,n)return data.fields[n]or""end,set_int=function(_,n,v)data.fields[n]=v end,get_int=function(_,n)return tonumber(data.fields[n])or 0 end,to_table=function()return deepcopy(data)end,from_table=function(_,v)metas[k]=deepcopy(v)end}end
function core.get_meta(pos)return meta(pos)end
local function object(pos)
 local o={pos=deepcopy(pos),vel={x=0,y=0,z=0},removed=false,props={}}
 function o:get_pos()return self.removed and nil or deepcopy(self.pos)end;function o:set_pos(p)self.pos=deepcopy(p)end;function o:remove()self.removed=true end
 function o:set_velocity(v)self.vel=deepcopy(v)end;function o:get_velocity()return deepcopy(self.vel)end;function o:set_yaw(y)self.yaw=y end;function o:set_properties(p)self.props=p end
 function o:set_armor_groups()end;function o:get_luaentity()return self end
 o.hp=20;function o:get_hp()return self.hp end;function o:set_hp(value)self.hp=value end
 function o:punch(_,_,caps)self.hp=math.max(0,self.hp-(((caps or{}).damage_groups or{}).fleshy or 0))end
 function o:is_player()return false end;objects[#objects+1]=o;return o
end
function core.add_entity(pos,name,staticdata)local o=object(pos);local def=core.registered_entities[name];if def then for k,v in pairs(def)do o[k]=v end;o.object=o;if o.on_activate then o:on_activate(staticdata or"")end end;return o end
function core.get_objects_inside_radius(pos,r)local out={};for _,o in ipairs(objects)do if o:get_pos()and vector.distance(pos,o:get_pos())<=r then out[#out+1]=o end end;for _,p in pairs(players)do if vector.distance(pos,p:get_pos())<=r then out[#out+1]=p end end;return out end
function core.add_particlespawner()end
local emitted_construct_effects={}
function core.emit_dynamic_construct_effect(id,definition)
 emitted_construct_effects[#emitted_construct_effects+1]={id=tostring(id),definition=deepcopy(definition)}
 return true
end
local function inventory()local inv={items={},lists={}}
 function inv:add_item(_,stack)table.insert(self.items,tostring(stack));return ItemStack("")end
 function inv:contains_item(_,wanted)local target=ItemStack(wanted);local count=0;for _,value in ipairs(self.items)do local st=ItemStack(value);if st:get_name()==target:get_name()then count=count+st:get_count()end end;return count>=math.max(1,target:get_count())end
 function inv:remove_item(_,wanted)local target=ItemStack(wanted);local remaining=math.max(1,target:get_count());for i=#self.items,1,-1 do local st=ItemStack(self.items[i]);if st:get_name()==target:get_name()then local take=math.min(remaining,st:get_count());st:take_item(take);remaining=remaining-take;if st:get_count()==0 then table.remove(self.items,i)else self.items[i]=st:to_string()end;if remaining<=0 then break end end end;return ItemStack(target:get_name().." "..(target:get_count()-remaining))end
 function inv:get_list(name)return self.lists[name]or{}end;function inv:set_list(name,list)self.lists[name]=list end
 function inv:set_size(name,size)local list=self.lists[name]or{};while #list<size do list[#list+1]=ItemStack("")end;self.lists[name]=list end
 function inv:set_width()end;function inv:get_width()return 1 end
 return inv end
local detached={}
function core.create_detached_inventory(name,callbacks)local inv=inventory();inv.callbacks=callbacks;detached[name]=inv;return inv end
function core.get_inventory(location)return location and location.type=="detached"and detached[location.name]or nil end
function core.remove_detached_inventory(name)detached[name]=nil end
function core.show_formspec(name,formname,formspec)core.last_formspec={name,formname,formspec};return true end
local function player(name,pos)local p={name=name,pos=pos or{x=0,y=2,z=0},inv=inventory(),wield=ItemStack(""),hp=20,huds={},next_hud=0}
 function p:is_player()return true end;function p:get_player_name()return self.name end;function p:get_pos()return deepcopy(self.pos)end;function p:set_pos(v)self.pos=deepcopy(v)end
 function p:get_inventory()return self.inv end;function p:get_player_control()return{sneak=false}end;function p:get_look_dir()return{x=0,y=0,z=1}end;function p:get_look_horizontal()return 0 end
 function p:get_wielded_item()return ItemStack(self.wield:to_string())end
 function p:set_wielded_item(stack)self.wield=ItemStack(stack:to_string())end;function p:get_hp()return self.hp end;function p:set_hp(value)self.hp=value end
 function p:hud_add(def)self.next_hud=self.next_hud+1;self.huds[self.next_hud]=deepcopy(def);return self.next_hud end
 function p:hud_change(id,field,value)if self.huds[id]then self.huds[id][field]=deepcopy(value)end end
 function p:hud_remove(id)self.huds[id]=nil end
 players[name]=p;return p end
function core.get_player_by_name(name)return players[name]end
function core.get_connected_players()local out={};for _,p in pairs(players)do out[#out+1]=p end;return out end
core.registered_nodes.air={buildable_to=true,liquidtype="none",groups={}};core.registered_items.air=core.registered_nodes.air

-- Minimal Milestone 4N native API mock. This is defined before mod loading so
-- the game selects the server-authoritative projectile and fire-control paths.
local native_constructs={};local next_native_id=900
local native_projectile_spawns={};local native_projectiles={};local next_projectile_id=7000
local native_projectile_events={}
local native_fire_observations={};local native_fire_batteries={};local native_fire_orders={};local next_battery_id=8000
function core.get_dynamic_construct_protocol_version()return 16 end
function core.create_dynamic_construct(definition)
 next_native_id=next_native_id+1;local id=tostring(next_native_id)
 native_constructs[id]={id=id,owner=definition.owner or"",position=deepcopy(definition.origin or{x=0,y=0,z=0}),yaw=definition.yaw or 0,velocity={x=0,y=0,z=0},yaw_velocity=0,node_count=#(definition.nodes or{}),nodes=deepcopy(definition.nodes or{})}
 return id
end
function core.remove_dynamic_construct(id)native_constructs[tostring(id)]=nil;return true end
function core.get_dynamic_construct(id)local value=native_constructs[tostring(id)];return value and deepcopy(value)or nil end
function core.set_dynamic_construct_transform(id,transform)
 local state=native_constructs[tostring(id)];if not state then return nil,"construct not found"end
 if transform.position then state.position=deepcopy(transform.position)end;if transform.yaw~=nil then state.yaw=transform.yaw end;return true
end
function core.set_dynamic_construct_velocity(id,velocity,yaw_velocity)
 local state=native_constructs[tostring(id)];if not state then return nil,"construct not found"end
 state.velocity=deepcopy(velocity);state.yaw_velocity=yaw_velocity or 0;return true
end
function core.set_dynamic_construct_node(id,pos,node)
 local state=native_constructs[tostring(id)];if not state then return nil,"construct not found"end;return true
end

local native_liquid_compartments={};local native_liquid_ports={};local native_liquid_cells={};local next_liquid_compartment=100;local next_liquid_port=200;local native_special_nodes={};local liquid_steps=0
function core.configure_dynamic_construct_liquid_compartment(construct_id,definition)
 next_liquid_compartment=next_liquid_compartment+1;local id=definition.id and tostring(definition.id)or tostring(next_liquid_compartment)
 native_liquid_compartments[id]={id=id,construct_id=tostring(construct_id),name=definition.name or"",cells=deepcopy(definition.cells or{}),capacity_units=#(definition.cells or{})*8,total_units=0,fill_fraction=0,centre_of_mass_local={x=0,y=0,z=0}};return id
end
function core.configure_dynamic_construct_liquid_port(definition)
 next_liquid_port=next_liquid_port+1;local id=definition.id and tostring(definition.id)or tostring(next_liquid_port);local copy=deepcopy(definition);copy.id=id;native_liquid_ports[id]=copy;return id
end
function core.set_dynamic_construct_liquid(construct_id,pos,liquid,level,flags)
 local k=tostring(construct_id);native_liquid_cells[k]=native_liquid_cells[k]or{};native_liquid_cells[k][pkey(pos)]={position=deepcopy(pos),liquid=liquid,level=level,flags=flags or 0};return true
end
function core.step_dynamic_construct_liquids(dt)
 liquid_steps=liquid_steps+1;local out={}
 for _,status in pairs(native_liquid_compartments)do
  local source,drain=0,0;for _,port in pairs(native_liquid_ports)do if tostring(port.compartment_id)==tostring(status.id)and port.enabled then if port.kind=="pump_in"then source=source+(port.rate or 0)elseif port.kind=="pump_out"then drain=drain+(port.rate or 0)end end end
  status.total_units=math.max(0,math.min(status.capacity_units,(status.total_units or 0)+(source-drain)*(dt or 0)));status.fill_fraction=status.capacity_units>0 and status.total_units/status.capacity_units or 0;out[#out+1]=deepcopy(status)
 end
 return{escaped_units=0,changed_cells={},compartments=out}
end
function core.get_dynamic_construct_liquids(construct_id)
 local cells={};for _,cell in pairs(native_liquid_cells[tostring(construct_id)]or{})do cells[#cells+1]=deepcopy(cell)end
 local compartments={};for _,status in pairs(native_liquid_compartments)do if tostring(status.construct_id)==tostring(construct_id)then compartments[#compartments+1]=deepcopy(status)end end
 return{cells=cells,compartments=compartments}
end
function core.register_dynamic_construct_special_node(definition)native_special_nodes[definition.name]=deepcopy(definition);return true end
function core.spawn_dynamic_construct_projectile(source_id,definition)
 next_projectile_id=next_projectile_id+1;local id=tostring(next_projectile_id)
 local state=deepcopy(definition);state.id=id;state.source_id=tostring(source_id);state.previous_position=deepcopy(state.position);state.age=0;state.distance=0;state.armed=false
 native_projectiles[id]=state;native_projectile_spawns[#native_projectile_spawns+1]=deepcopy(state);return id
end
function core.step_dynamic_construct_projectiles()local events=native_projectile_events;native_projectile_events={};return deepcopy(events)end
function core.get_dynamic_construct_projectiles()local out={};for _,state in pairs(native_projectiles)do out[#out+1]=deepcopy(state)end;return out end
function core.impact_dynamic_construct_projectile(id,position,kind)
 local state=native_projectiles[tostring(id)];if not state then return nil,"projectile not found"end
 native_projectiles[tostring(id)]=nil;state.position=deepcopy(position)
 return{event="impact",projectile=deepcopy(state),impact={kind=kind,position=deepcopy(position),explosion={node_damage={},affected_constructs={},destroyed_nodes=0,breaches=0}}}
end

function core.observe_dynamic_construct_target(observer_id,definition)
 native_fire_observations[#native_fire_observations+1]={observer_id=tostring(observer_id),definition=deepcopy(definition)};return true
end
function core.solve_dynamic_construct_fire_control(shooter_id,target_id,definition)
 local speed=tonumber(definition.muzzle_speed)or 20
 return{valid=true,shooter_id=tostring(shooter_id),target_id=tostring(target_id),
  muzzle_position={x=0,y=2,z=0},aim_point={x=20,y=2,z=0},launch_velocity={x=speed,y=1,z=0},
  yaw=math.pi/2,pitch=math.atan(1/speed),intercept_time=1,range=20,closure_rate=0,
  miss_distance=.05,track_confidence=.9,reason="mock solution"}
end
function core.configure_dynamic_construct_battery(shooter_id,definition)
 local id=definition.id and tostring(definition.id)or nil
 if not id then next_battery_id=next_battery_id+1;id=tostring(next_battery_id)end
 local copy=deepcopy(definition);copy.id=id;copy.shooter_id=tostring(shooter_id);native_fire_batteries[id]=copy;return id
end
function core.remove_dynamic_construct_battery(id)native_fire_batteries[tostring(id)]=nil;return true end
function core.step_dynamic_construct_fire_control()local orders=deepcopy(native_fire_orders);native_fire_orders={};return orders end
function core.get_dynamic_construct_fire_control_tracks(observer_id)
 local out={};for _,observation in ipairs(native_fire_observations)do if not observer_id or tostring(observer_id)==observation.observer_id then out[#out+1]=deepcopy(observation.definition)end end;return out
end
function core.get_dynamic_construct_fire_control_batteries(shooter_id)
 local out={};for _,battery in pairs(native_fire_batteries)do if not shooter_id or tostring(shooter_id)==battery.shooter_id then out[#out+1]=deepcopy(battery)end end;return out
end

local native_navigation={};local native_navigation_routes={};local native_navigation_obstacles={}
function core.configure_dynamic_construct_navigation(id,definition)
 local key=tostring(id);native_navigation[key]=native_navigation[key]or{};for k,v in pairs(deepcopy(definition))do native_navigation[key][k]=v end;native_navigation[key].construct_id=key;native_navigation[key].enabled=definition.mode~="manual";return true
end
function core.set_dynamic_construct_route(id,route,loop)
 local key=tostring(id);native_navigation_routes[key]=deepcopy(route);native_navigation[key]=native_navigation[key]or{construct_id=key};native_navigation[key].waypoint_count=#route;native_navigation[key].loop=loop;return true
end
function core.observe_dynamic_construct_obstacle(id,obstacle)native_navigation_obstacles[#native_navigation_obstacles+1]={id=tostring(id),obstacle=deepcopy(obstacle)};return true end
function core.step_dynamic_construct_navigation(dt,current_time)
 local out={};for key,state in pairs(native_navigation)do if state.enabled then
  local route=native_navigation_routes[key]or{};local target=route[1]and(route[1].position or route[1])or state.hold_position or{x=0,y=0,z=0}
  out[#out+1]={construct_id=key,sequence=1,mode=state.mode,target_position=deepcopy(target),desired_velocity={x=0,y=0,z=2},desired_yaw=0,yaw_rate=.1,forward_speed=2,vertical_speed=state.domain=="air"and.5 or 0,distance=10,avoidance=0,waypoint_index=1,avoiding=false,arrived=false,completed=false,recovering=false,reason="mock navigating"}
 end end;return out
end
function core.get_dynamic_construct_navigation(id)
 if id then local state=native_navigation[tostring(id)];if not state then return nil,"navigation state not found"end;return deepcopy(state)end
 local out={};for _,state in pairs(native_navigation)do out[#out+1]=deepcopy(state)end;return out
end
function core.set_dynamic_construct_navigation_enabled(id,enabled)local state=native_navigation[tostring(id)];if not state then return nil,"navigation state not found"end;state.enabled=enabled;return true end
function core.clear_dynamic_construct_navigation(id)native_navigation[tostring(id)]=nil;native_navigation_routes[tostring(id)]=nil;return true end

local native_structure_states={};local native_structure_events={};local native_flooding={}
function core.set_dynamic_construct_flooding(id,fraction)native_flooding[tostring(id)]=fraction;return true end
function core.step_dynamic_construct_structure()
 local events=deepcopy(native_structure_events);native_structure_events={}
 if #events==0 then for id,_ in pairs(native_constructs)do
  local state=native_structure_states[id]or{construct_id=id,source_id=id,role="vessel",component_count=1,node_count=50,control_nodes=1,breach_count=0,mass=35,solid_volume=50,enclosed_volume=10,submerged_volume=30,effective_displacement=40,flooded_fraction=native_flooding[id]or 0,integrity=.94,buoyancy_force=392.4,weight_force=343.35,vertical_acceleration=1.4,afloat=true,sinking=false,sunk=false}
  native_structure_states[id]=deepcopy(state);events[#events+1]={event="updated",construct_id=id,source_id=id,fragment_ids={},moved_nodes=0,state=deepcopy(state)}
 end end
 return events
end
function core.get_dynamic_construct_structure(id)
 if id then local state=native_structure_states[tostring(id)];if not state then return nil,"structural state not found"end;return deepcopy(state)end
 local out={};for _,state in pairs(native_structure_states)do out[#out+1]=deepcopy(state)end;return out
end
function core.force_dynamic_construct_split(id)
 local key=tostring(id);local fragment_id=key.."1";native_structure_states[fragment_id]={construct_id=fragment_id,source_id=key,role="fragment",component_count=1,node_count=2,control_nodes=0,breach_count=1,mass=3,solid_volume=2,enclosed_volume=0,submerged_volume=2,effective_displacement=2,flooded_fraction=.2,integrity=.8,buoyancy_force=19.62,weight_force=29.43,vertical_acceleration=-3.27,afloat=false,sinking=true,sunk=false}
 return{{event="split",construct_id=key,source_id=key,fragment_ids={fragment_id},moved_nodes=2,state=native_structure_states[key]or{construct_id=key,source_id=key,role="primary",component_count=1,node_count=48,integrity=.9,flooded_fraction=0,afloat=true,sinking=false,sunk=false}}}
end

local native_articulations={};local native_turrets={};local next_joint_id=7000;local next_turret_id=8000;local articulation_steps=0
function core.configure_dynamic_construct_joint(id,definition)
 next_joint_id=next_joint_id+1;local joint_id=definition.id and tostring(definition.id)or tostring(next_joint_id)
 local state=deepcopy(definition);state.id=joint_id;state.construct_id=tostring(id);state.position=state.position or 0;state.target_position=state.target_position or 0;state.velocity=0;state.revision=1;state.enabled=state.enabled~=false
 native_articulations[joint_id]=state;return joint_id
end
function core.set_dynamic_construct_joint(id,controls)
 local state=native_articulations[tostring(id)];if not state then return nil,"articulation not found"end
 for k,v in pairs(controls or{})do if k=="target"then state.target_position=v else state[k]=v end end
 state.position=state.target_position or state.position;state.velocity=0;state.revision=(state.revision or 0)+1;return deepcopy(state)
end
function core.remove_dynamic_construct_joint(id)native_articulations[tostring(id)]=nil;return true end
function core.configure_dynamic_construct_turret(id,definition)
 next_turret_id=next_turret_id+1;local turret_id=definition.id and tostring(definition.id)or tostring(next_turret_id)
 local status=deepcopy(definition);status.id=turret_id;status.construct_id=tostring(id);status.aligned=true;status.obstructed=false;status.angle_error=0;status.has_target=false;native_turrets[turret_id]=status;return deepcopy(status)
end
function core.aim_dynamic_construct_turret(id,target)
 local status=native_turrets[tostring(id)];if not status then return nil,"turret not found"end;status.target=deepcopy(target);status.has_target=true;status.aligned=true;status.obstructed=false;status.angle_error=0;return deepcopy(status)
end
function core.clear_dynamic_construct_turret_target(id)local status=native_turrets[tostring(id)];if not status then return false end;status.has_target=false;return true end
function core.dynamic_construct_line_of_fire(id,target)local status=native_turrets[tostring(id)];if not status then return nil,"turret not found"end;return true,nil,deepcopy(status)end
function core.step_dynamic_construct_articulations()articulation_steps=articulation_steps+1;return{joints={},turrets=deepcopy(native_turrets)}end
function core.get_dynamic_construct_articulations(id)
 local result={joints={},turrets={}};for _,state in pairs(native_articulations)do if not id or tostring(id)==state.construct_id then result.joints[#result.joints+1]=deepcopy(state)end end;for _,status in pairs(native_turrets)do if not id or tostring(id)==status.construct_id then result.turrets[#result.turrets+1]=deepcopy(status)end end;return result
end

local brett=player("Brett",{x=0,y=2,z=0})
for _,mod in ipairs{"nc_core","nc_navycraft","nc_shipyard","nc_campaign"}do current_mod=mod;dofile(root.."/mods/"..mod.."/init.lua")end
if core.callbacks.on_mods_loaded then for _,f in ipairs(core.callbacks.on_mods_loaded)do f()end end
assert(#navycraft.definitions.craft_order==8,"craft type count")
assert(#navycraft.definitions.engine_order==20,"engine count")
assert(#navycraft.definitions.weapon_order==10,"weapon count")
assert(core.registered_chatcommands.shipyard,"shipyard command missing")
assert(core.registered_chatcommands.ship,"ship command missing")
assert(core.registered_entities["nc_navycraft:weapon_projectile"],"projectile missing")
local nodes={}
for i=1,50 do nodes[i]={name="nc_core:frame",metadata=core.serialize({fields={}}),pos={x=i,y=0,z=0},param1=0,param2=0}end
nodes[1].name="nc_navycraft:helm";nodes[2].name="nc_navycraft:engine_boiler_1";nodes[3].name="nc_navycraft:nav";nodes[4].name="nc_navycraft:weapon_single_cannon"
local profile,err=navycraft.systems.analyse({nodes=nodes},"ship");assert(profile,err);assert(#profile.engines==1);assert(#profile.weapons==1)
local ok,plot=navycraft.shipyard.create_plot("DD01","DD",{x=-10,y=0,z=-10},{x=10,y=10,z=10},"Brett");assert(ok)
assert(navycraft.shipyard.reward("Brett","SHIP1",1));assert(navycraft.shipyard.claim("Brett","DD01"))
assert(navycraft.routes.create("Brett","Patrol"));assert(navycraft.routes.add_waypoint("Brett","Patrol",{x=20,y=2,z=20}))
-- Launch, arm, fire, store and respawn a source-derived vessel.
for i=0,49 do local name="nc_core:frame";if i==0 then name="nc_navycraft:helm"elseif i==1 then name="nc_navycraft:engine_boiler_1"elseif i==2 then name="nc_navycraft:nav"elseif i==3 then name="nc_navycraft:weapon_single_cannon"end;core.set_node({x=i,y=1,z=0},{name=name,param1=0,param2=0});local def=core.registered_nodes[name];if def and def.on_construct then def.on_construct({x=i,y=1,z=0})end end
local scan,scanerr=navycraft.scan.connected_nodes({x=0,y=1,z=0},{max_nodes=100});assert(scan,scanerr)
local launch_profile,lerr=navycraft.systems.analyse(scan,"ship");assert(launch_profile,lerr);scan.navycraft_profile=launch_profile
local id,mode=navycraft.construct.launch(brett,scan);assert(id,mode)
local craft=navycraft.preview.get_by_id(id);assert(craft and craft.systems,"launch systems missing")
craft.systems.ammo.cannon_shell=5
local fired,fmsg=navycraft.weapons.fire(craft,"Brett",0);assert(fired,fmsg)
assert(craft.native_id,"M4J launch did not use native construct API")
assert(#native_projectile_spawns==1,"M4I fire did not use native projectile API")
assert(native_projectile_spawns[1].kind=="shell","M4I projectile kind mismatch")
local damaged_entry=craft.nodes[10];assert(damaged_entry and not damaged_entry.destroyed,"M4I damage test node missing")
local target_player=player("Target",deepcopy(craft.position))
native_projectile_events={{event="impact",projectile={
    id=native_projectile_spawns[1].id,source_id=craft.native_id,owner="Brett",kind="shell",
    position=deepcopy(craft.position),previous_position=deepcopy(craft.position),
    blast_radius=3,blast_power=4,penetration=3,age=.5,requires_water=false,
},impact={kind="construct",position=deepcopy(craft.position),construct_id=craft.native_id,
    explosion={destroyed_nodes=1,breaches=1,affected_constructs={craft.native_id},node_damage={{
        construct_id=craft.native_id,node_pos=deepcopy(damaged_entry.local_pos),node_name=damaged_entry.name,
        distance=0,power=7,armour=1,destroyed=true,breached=true,
    }}}}}}
local impact_events=navycraft.projectiles.step(.05);assert(#impact_events==1,"M4I impact event was not processed")
assert(damaged_entry.destroyed,"M4I native construct damage was not mirrored")
assert(target_player:get_hp()<20,"M4I authoritative blast did not damage nearby player")
local partial_entry=craft.nodes[12];assert(partial_entry and not partial_entry.destroyed,"partial damage test node missing")
local hp_before=partial_entry.hp or partial_entry.max_hp or 0
local _,partial_removed,partial_details=navycraft.preview.apply_native_damage(craft.native_id,{{
    construct_id=craft.native_id,node_pos=deepcopy(partial_entry.local_pos),node_name=partial_entry.name,
    distance=0,effective_power=1,armour=10,destroyed=false,breached=false,
}},craft.position,"Brett","shell")
assert(partial_removed==0,"partial native damage should not remove armored block")
assert(partial_details and partial_details.damaged>0,"partial native damage was not recorded")
assert((partial_entry.hp or hp_before)<hp_before and not partial_entry.destroyed,"partial native damage did not reduce block HP")
craft.forward_speed=0;craft.vertical_speed=0;craft.yaw_rate=0
local stored,smsg=navycraft.storage.store("Brett","Smoke Ship");assert(stored,smsg)
local spawned,newid=navycraft.storage.spawn("Brett","Smoke Ship",{x=0,y=3,z=0},0);assert(spawned,newid)
assert(navycraft.preview.get_by_id(newid),"stored craft did not respawn")

-- Exercise M4J target lead, solved fire and automatic battery execution.
local shooter=navycraft.preview.get_by_id(newid);assert(shooter,"M4J shooter missing")
local spawned_target,target_id=navycraft.storage.spawn("Brett","Smoke Ship",{x=20,y=3,z=0},0,{allow_multiple=true,runtime_owner="TargetRuntime"});assert(spawned_target,target_id)
local target=navycraft.preview.get_by_id(target_id);assert(target,"M4J target missing")
local damage_snapshot=navycraft.preview.snapshot(target)
local damage_id,damage_error=navycraft.preview.spawn_snapshot("DamageRuntime",damage_snapshot,{x=120,y=1,z=120},0,{allow_multiple=true})
assert(damage_id,damage_error)
local damage_craft=navycraft.preview.get_by_id(damage_id);assert(damage_craft,"damage craft missing")
damage_craft.systems.pump_on=false
local damage_results=navycraft.preview.damage_radius(damage_craft.position,6,35,"Brett","torpedo")
local saw_damage=false
for _,result in ipairs(damage_results or {})do if result.id==damage_craft.id and (result.removed or 0)>0 then saw_damage=true end end
assert(saw_damage,"torpedo radius damage did not remove any ship blocks")
assert((damage_craft.systems.breach_count or 0)>0,"torpedo damage did not create hull breaches")
assert((damage_craft.systems.flooding or 0)>0,"breached ship did not start flooding")
for _=1,60 do navycraft.systems.step(damage_craft,.25)end
assert(damage_craft.systems.sinking,"breached and flooded ship did not enter sinking state")
-- Stored snapshots still use the stock-renderer fallback in this isolated smoke harness;
-- attach mock native IDs so the M4J server API path can be exercised directly.
shooter.native_id=core.create_dynamic_construct({origin=shooter.position,nodes={}})
target.native_id=core.create_dynamic_construct({origin=target.position,nodes={}})
shooter.systems.target_id=target.id;shooter.systems.selected_weapon=0;shooter.systems.ammo.cannon_shell=8;shooter.systems.last_weapon_fire=0
local solution,solution_error=navycraft.fire_control.solve(shooter,0,target.id);assert(solution and solution.valid,solution_error)
assert(solution.launch_velocity and solution.launch_velocity.x>0,"M4J launch vector missing")
assert(navycraft.fire_control.format_solution(solution):find("lead="),"M4J solution formatting missing")
assert(navycraft.machinery and navycraft.machinery.native_available(),"M4M machinery native path unavailable")
local machinery_state=navycraft.machinery.configure(shooter);assert(machinery_state and #machinery_state.turrets>0,"M4M turret was not configured")
local ready,turret_status=navycraft.machinery.prepare_fire(shooter,0,{target_id=target.id,aim_point=solution.aim_point,launch_velocity=solution.launch_velocity,fire_control=true});assert(ready,turret_status)
assert(turret_status and turret_status.aligned and not turret_status.obstructed,"M4M turret did not align cleanly")
navycraft.machinery.step(.1);assert(articulation_steps>0,"M4M articulation step did not run")
local before_solved=#native_projectile_spawns
local solved_ok,solved_message=core.registered_chatcommands.ship.func("Brett","fire solution 0");assert(solved_ok,solved_message)
assert(#native_projectile_spawns==before_solved+1,"M4J solved fire did not spawn a projectile")
assert(math.abs(native_projectile_spawns[#native_projectile_spawns].velocity.x-solution.launch_velocity.x)<.001,"M4J solved launch velocity was not applied")
shooter.systems.last_weapon_fire=0
assert(navycraft.fire_control.set_mode(shooter,"auto"))
navycraft.fire_control.step(.25)
local battery_id=shooter.systems.fire_control_battery_id;assert(battery_id,"M4J automatic battery was not configured")
native_fire_orders={{battery_id=tostring(battery_id),shooter_id=tostring(shooter.native_id),target_id=tostring(target.native_id),solution=deepcopy(solution)}}
local before_auto=#native_projectile_spawns;navycraft.fire_control.step(.25)
assert(#native_projectile_spawns==before_auto+1,"M4J automatic fire order was not executed")
assert(core.registered_chatcommands.nc_firecontrol,"M4J diagnostic command missing")

-- Exercise M4K native routes, command output and navigation override.
assert(navycraft.routes.bind(shooter,"Brett","Patrol"))
shooter.systems.top_speed=6;shooter.systems.route_loop=true
navycraft.navigation.step(shooter,.1)
assert(native_navigation[tostring(shooter.native_id)],"M4K native navigation was not configured")
assert(native_navigation_routes[tostring(shooter.native_id)]and#native_navigation_routes[tostring(shooter.native_id)]==1,"M4K route was not sent to native navigation")
assert(shooter.forward_speed==2 and math.abs(shooter.yaw_rate-.1)<.001,"M4K native navigation command was not applied")
local nav_ok,nav_message=core.registered_chatcommands.ship.func("Brett","navigation status");assert(nav_ok and nav_message:find("mode=route"),"M4K navigation status command failed")
assert(core.registered_chatcommands.nc_navigation,"M4K diagnostic command missing")

-- Exercise M4L structural state, flooding sync and split diagnostics.
native_structure_states[tostring(shooter.native_id)]={construct_id=tostring(shooter.native_id),source_id=tostring(shooter.native_id),role="primary",component_count=1,node_count=50,control_nodes=1,breach_count=1,mass=35,solid_volume=50,enclosed_volume=10,submerged_volume=30,effective_displacement=38,flooded_fraction=.1,integrity=.9,buoyancy_force=372.8,weight_force=343.35,vertical_acceleration=.84,afloat=true,sinking=false,sunk=false}
local structure_events=navycraft.structure.step(.1);assert(#structure_events>0,"M4L structural step did not run")
local structure_ok,structure_message=core.registered_chatcommands.ship.func("Brett","structure status");assert(structure_ok and structure_message:find("integrity=90.0%%"),"M4L structural status command failed")
assert(core.registered_chatcommands.nc_structure,"M4L diagnostic command missing")
local split_ok,split_message=core.registered_chatcommands.ship.func("Brett","structure split");assert(split_ok and split_message:find("fragment"),"M4L force split command failed")

-- Exercise M4N construct-local compartments, pump controls and specialised nodes.
assert(navycraft.fluids and navycraft.fluids.native_available(),"M4N liquids native path unavailable")
shooter.systems.ballast_mode=1;shooter.systems.pump_on=false
local fluid_state=navycraft.fluids.configure(shooter);assert(fluid_state and fluid_state.compartment_id,"M4N liquid compartment was not configured")
navycraft.fluids.step(.2);assert(liquid_steps>0,"M4N liquid simulation did not step")
local fluids_ok,fluids_message=core.registered_chatcommands.ship.func("Brett","fluids status");assert(fluids_ok and fluids_message:find("liquids=native"),"M4N fluid status command failed")
assert(core.registered_chatcommands.nc_fluids,"M4N liquid diagnostic command missing")
assert(native_special_nodes["nc_navycraft:ballast"],"M4N specialised-node registration missing")


-- Exercise M5A career, crew, missions, port services and reward loop.
assert(navycraft.campaign and navycraft.campaign.missions,"M5A campaign module missing")
local home_pos=vector.new(shooter.position);local outpost_pos=vector.add(home_pos,{x=400,y=0,z=0})
assert(navycraft.campaign.ports.create("home","Home Harbour",home_pos))
assert(navycraft.campaign.ports.create("outpost","Outer Anchorage",outpost_pos))
brett:set_pos(home_pos)
local career_ok,career_message=core.registered_chatcommands.career.func("Brett","start");assert(career_ok,career_message)
assert(navycraft.campaign.career.balance("Brett")>=250,"M5A starter credits missing")
local alex=player("Alex",home_pos);navycraft.campaign.career.ensure("Alex")
local role_ok,role_message=navycraft.campaign.crew.assign(shooter,"Brett","Alex","gunner");assert(role_ok,role_message)
navycraft.campaign.crew.record(shooter,"Alex",25,"weapons")
local mission_ok,mission_message=navycraft.campaign.missions.accept("Brett","sea_trial");assert(mission_ok,mission_message)
navycraft.campaign.missions.step_construct(shooter,.3)
for _=1,10 do shooter.position=vector.add(shooter.position,{x=25,y=0,z=0});navycraft.campaign.missions.step_construct(shooter,.3)end
shooter.position=vector.new(home_pos);navycraft.campaign.missions.on_dock(shooter)
assert(not navycraft.campaign.missions.active("Brett"),"M5A sea trial did not complete")
assert(navycraft.campaign.career.balance("Alex")>0,"M5A crew reward share missing")
local offers=navycraft.campaign.missions.offers_for("home","Brett");local courier
for _,offer in ipairs(offers)do if offer.kind=="courier"then courier=offer;break end end
assert(courier,"M5A courier offer missing");brett:set_pos(home_pos)
local courier_ok,courier_message=navycraft.campaign.missions.accept("Brett",courier.id);assert(courier_ok,courier_message)
assert(brett:get_inventory():contains_item("main","nc_campaign:sealed_dispatch"),"M5A cargo item missing")
shooter.position=vector.new(outpost_pos);navycraft.campaign.missions.step_construct(shooter,.3)
assert(not navycraft.campaign.missions.active("Brett"),"M5A courier mission did not complete")
brett:set_pos(outpost_pos);local buy_ok,buy_message=navycraft.campaign.economy.buy("Brett","cannon_shell",1);assert(buy_ok,buy_message)
assert(core.registered_chatcommands.mission and core.registered_chatcommands.navystore and core.registered_chatcommands.crewstation,"M5A command surface missing")

-- Exercise M5B factions, connected operations, territories and NPC fleets.
assert(navycraft.campaign.factions and navycraft.campaign.fleets and navycraft.campaign.operations,"M5B campaign systems missing")
local faction_ok,faction_message=navycraft.campaign.factions.join("Brett","navy");assert(faction_ok or faction_message:find("already enlisted"),faction_message)
assert(navycraft.campaign.factions.hostile("navy","corsair"),"M5B faction diplomacy missing")
assert(navycraft.campaign.territories.set_owner("outpost","corsair","smoke test"))
brett:set_pos(home_pos);shooter.position=vector.new(home_pos)
local operation_offers=navycraft.campaign.operations.offers_for("home","Brett");assert(operation_offers[1]and operation_offers[1].kind=="operation_recon","M5B recon operation offer missing")
local op_ok,op_message=navycraft.campaign.missions.accept_offer("Brett",operation_offers[1]);assert(op_ok,op_message)
shooter.position=vector.new(outpost_pos);navycraft.campaign.missions.step_construct(shooter,.3)
shooter.position=vector.new(home_pos);navycraft.campaign.missions.step_construct(shooter,.3)
assert(not navycraft.campaign.missions.active("Brett"),"M5B recon stage did not complete")
operation_offers=navycraft.campaign.operations.offers_for("home","Brett");assert(operation_offers[1]and operation_offers[1].kind=="operation_battle","M5B battle operation offer missing")
op_ok,op_message=navycraft.campaign.missions.accept_offer("Brett",operation_offers[1]);assert(op_ok,op_message)
local battle=navycraft.campaign.missions.active("Brett");assert(battle.encounter_id,"M5B NPC encounter was not spawned")
local encounter=navycraft.campaign.fleets.get(battle.encounter_id);assert(encounter and #encounter.units>=1,"M5B encounter units missing")
for _,unit in ipairs(encounter.units)do local npc=unit.construct_id and navycraft.preview.get_by_id(unit.construct_id);if npc then npc.systems.hull_integrity=.2;unit.last_attacker="Brett"end end
navycraft.campaign.fleets.step(.6);navycraft.campaign.fleets.step(.6)
encounter=navycraft.campaign.fleets.get(battle.encounter_id);assert(encounter.status=="complete","M5B encounter did not complete")
navycraft.campaign.missions.step_construct(shooter,.3);assert(not navycraft.campaign.missions.active("Brett"),"M5B battle stage did not complete")
operation_offers=navycraft.campaign.operations.offers_for("home","Brett");assert(operation_offers[1]and operation_offers[1].kind=="operation_control","M5B territory stage offer missing")
op_ok,op_message=navycraft.campaign.missions.accept_offer("Brett",operation_offers[1]);assert(op_ok,op_message)
navycraft.campaign.territories.contest("outpost","navy",100)
shooter.position=vector.new(outpost_pos)
for _=1,70 do navycraft.campaign.missions.step_construct(shooter,.3)end
assert(not navycraft.campaign.missions.active("Brett"),"M5B territory stage did not complete")
assert(navycraft.campaign.operations.status("Brett"):find("COMPLETE"),"M5B operation chain did not complete")
assert(navycraft.campaign.territories.get("outpost").owner=="navy","M5B territory capture missing")
assert(core.registered_chatcommands.faction and core.registered_chatcommands.territory and core.registered_chatcommands.operation and core.registered_chatcommands.fleet,"M5B command surface missing")

-- Exercise M5C extraction, industry, port inventories, markets and equipment tiers.
assert(navycraft.campaign.markets and navycraft.campaign.resources and navycraft.campaign.industry and navycraft.campaign.equipment,"M5C economy systems missing")
navycraft.shipyard.credit("Brett",2000,"M5C smoke capital")
navycraft.shipyard.reward_experience("Brett",150,"M5C manufacturing qualification")
brett:set_pos(home_pos);shooter.position=vector.new(home_pos)
assert(navycraft.campaign.resources.create("ironfield","iron",home_pos,200,20))
assert(navycraft.campaign.resources.create("coalfield","coal",home_pos,200,20))
assert(navycraft.campaign.resources.create("copperfield","copper",home_pos,200,20))
assert(navycraft.campaign.resources.create("oilfield","oil",home_pos,200,20))
assert(navycraft.campaign.resources.create("salvagefield","salvage",home_pos,200,20))
for _,entry in ipairs{{"ironfield",4},{"coalfield",3},{"copperfield",4},{"oilfield",2},{"salvagefield",1}}do local ok_extract,msg_extract=navycraft.campaign.resources.extract("Brett",entry[1],entry[2]);assert(ok_extract,msg_extract)end
assert(brett:get_inventory():contains_item("main","nc_campaign:iron_ore 12"),"M5C extracted iron missing")
assert(navycraft.campaign.industry.queue("Brett","steel_ingot",4))
assert(navycraft.campaign.industry.queue("Brett","copper_ingot",4))
assert(navycraft.campaign.industry.queue("Brett","fuel_drum",1))
assert(navycraft.campaign.industry.step(1000)==3,"M5C refining jobs did not complete")
local collect_ok,collect_msg=navycraft.campaign.industry.collect("Brett");assert(collect_ok,collect_msg)
assert(navycraft.campaign.industry.queue("Brett","machinery_parts",4))
assert(navycraft.campaign.industry.queue("Brett","electronics",2))
assert(navycraft.campaign.industry.step(1000)==2,"M5C component jobs did not complete")
collect_ok,collect_msg=navycraft.campaign.industry.collect("Brett");assert(collect_ok,collect_msg)
assert(navycraft.campaign.industry.queue("Brett","engine_kit_t1",1))
assert(navycraft.campaign.industry.step(1000)==1,"M5C equipment job did not complete")
collect_ok,collect_msg=navycraft.campaign.industry.collect("Brett");assert(collect_ok,collect_msg)
assert(brett:get_inventory():contains_item("main","nc_campaign:engine_kit_t1 1"),"M5C manufactured refit kit missing")
local active_for_refit=navycraft.preview.get_for_owner("Brett");assert(active_for_refit,"M5C active vessel missing")
active_for_refit.systems.equipment={};navycraft.campaign.equipment.apply(active_for_refit)
for _,engine_state in pairs(active_for_refit.systems.engines or{})do engine_state.set_on=true end
navycraft.systems.step(active_for_refit,.1);local stock_top_speed=active_for_refit.systems.top_speed;assert(stock_top_speed>0,"M5C stock propulsion baseline missing")
local install_ok,install_msg=navycraft.campaign.equipment.install("Brett","engine_t1");assert(install_ok,install_msg)
assert(active_for_refit.systems.equipment.engine=="engine_t1","M5C equipment slot missing")
assert(math.abs(active_for_refit.systems.equipment_modifiers.speed_mult-1.08)<.001,"M5C propulsion modifier missing")
navycraft.systems.step(active_for_refit,.1);assert(active_for_refit.systems.top_speed>stock_top_speed*1.07,"M5C propulsion upgrade did not reach vessel systems")
local home_before=navycraft.campaign.markets.price("home","iron_ore","buy")
navycraft.campaign.markets.adjust("home","iron_ore",-1000)
local home_after=navycraft.campaign.markets.price("home","iron_ore","buy");assert(home_after>home_before,"M5C scarcity did not raise price")
navycraft.campaign.markets.adjust("home","iron_ore",1000);navycraft.campaign.markets.adjust("outpost","iron_ore",-1000)
local stock_before=navycraft.campaign.markets.stock("home","iron_ore")
local market_buy_ok,market_buy_msg=navycraft.campaign.markets.buy("Brett","iron_ore",2);assert(market_buy_ok,market_buy_msg)
assert(navycraft.campaign.markets.stock("home","iron_ore")==stock_before-2,"M5C port warehouse buy debit missing")
brett:set_pos(outpost_pos);local credits_before_sale=navycraft.campaign.career.balance("Brett")
local market_sell_ok,market_sell_msg=navycraft.campaign.markets.sell("Brett","iron_ore",2);assert(market_sell_ok,market_sell_msg)
assert(navycraft.campaign.career.balance("Brett")>credits_before_sale,"M5C commodity sale payment missing")
assert(navycraft.campaign.markets.route_text("Brett","iron_ore"):find("iron_ore"),"M5C trade route analysis missing")
brett:set_pos(home_pos)
assert(core.registered_chatcommands.market and core.registered_chatcommands.warehouse and core.registered_chatcommands.resource and core.registered_chatcommands.industry and core.registered_chatcommands.equipment,"M5C command surface missing")
assert(core.registered_nodes["nc_campaign:market_terminal"]and core.registered_nodes["nc_campaign:factory_terminal"]and core.registered_nodes["nc_campaign:resource_beacon"],"M5C terminal nodes missing")

-- Exercise M5D vessel classes, blueprint progression, cargo mass and maintenance.
assert(navycraft.campaign.classes and navycraft.campaign.blueprints and navycraft.campaign.logistics and navycraft.campaign.maintenance,"M5D progression systems missing")
local m5d_vessel=navycraft.preview.get_for_owner("Brett");assert(m5d_vessel,"M5D active vessel missing")
m5d_vessel.position=vector.new(home_pos);brett:set_pos(home_pos)
local class_def=navycraft.campaign.classes.apply(m5d_vessel);assert(class_def and m5d_vessel.systems.vessel_class=="patrol_cutter","M5D automatic vessel classification failed")
assert(navycraft.campaign.blueprints.has("Brett","patrol_cutter"),"M5D starter blueprint missing")
local certify_ok,certify_msg=navycraft.campaign.blueprints.certify("Brett","patrol_cutter");assert(certify_ok,certify_msg)
assert(m5d_vessel.systems.class_provisional==false,"M5D vessel certification did not persist")
brett:get_inventory():add_item("main","nc_campaign:hull_plate 4");brett:get_inventory():add_item("main","nc_campaign:machinery_parts 2")
local research_ok,research_msg=navycraft.campaign.blueprints.research("Brett","corvette");assert(research_ok,research_msg)
assert(navycraft.campaign.blueprints.has("Brett","corvette"),"M5D researched blueprint missing")
brett:get_inventory():add_item("main","nc_campaign:iron_ore 8")
local unloaded=navycraft.campaign.logistics.apply(m5d_vessel);local unloaded_mass=m5d_vessel.systems.operating_mass
local load_ok,load_msg=navycraft.campaign.logistics.load("Brett","iron_ore",8);assert(load_ok,load_msg)
assert(m5d_vessel.systems.cargo_used==8 and m5d_vessel.systems.operating_mass>unloaded_mass,"M5D cargo did not add operating mass")
brett:get_inventory():add_item("main","nc_campaign:cargo_kit_t1 1")
local base_capacity=m5d_vessel.systems.cargo_capacity
local cargo_fit_ok,cargo_fit_msg=navycraft.campaign.equipment.install("Brett","cargo_t1");assert(cargo_fit_ok,cargo_fit_msg)
assert(m5d_vessel.systems.cargo_capacity>base_capacity,"M5D cargo refit did not expand class capacity")
for _,engine_state in pairs(m5d_vessel.systems.engines or{})do engine_state.set_on=true end
m5d_vessel.systems.engines_on=true
navycraft.campaign.maintenance.step(m5d_vessel,36000)
assert(m5d_vessel.systems.maintenance.condition<.9,"M5D operating wear did not reduce condition")
navycraft.campaign.maintenance.apply(m5d_vessel);assert(m5d_vessel.systems.maintenance_speed_mult<1,"M5D maintenance penalty missing")
brett:get_inventory():add_item("main","nc_campaign:repair_parts 10")
local service_ok,service_msg=navycraft.campaign.maintenance.service("Brett");assert(service_ok,service_msg)
assert(m5d_vessel.systems.maintenance.condition==1,"M5D maintenance service did not restore condition")
local unload_ok,unload_msg=navycraft.campaign.logistics.unload("Brett","iron_ore",3);assert(unload_ok,unload_msg)
assert(m5d_vessel.systems.cargo_manifest.iron_ore==5,"M5D cargo unload did not update manifest")
assert(core.registered_chatcommands.vesselclass and core.registered_chatcommands.blueprint and core.registered_chatcommands.cargo and core.registered_chatcommands.maintenance,"M5D command surface missing")
assert(core.registered_nodes["nc_campaign:blueprint_desk"]and core.registered_nodes["nc_campaign:cargo_terminal"]and core.registered_nodes["nc_campaign:maintenance_terminal"],"M5D terminal nodes missing")

-- Exercise M5E NPC logistics, convoy escorts, piracy and strategic port supply.
assert(navycraft.campaign.supply and navycraft.campaign.shipping,"M5E logistics systems missing")
navycraft.campaign.markets.adjust("home","fuel_drum",80)
navycraft.campaign.markets.adjust("home","provisions",100)
navycraft.campaign.markets.adjust("home","munitions_crate",60)
navycraft.campaign.markets.adjust("home","hull_plate",40)
navycraft.campaign.supply.set("outpost","fuel",100)
local supplied_price=navycraft.campaign.markets.price("outpost","fuel_drum","buy")
navycraft.campaign.supply.set("outpost","fuel",10)
local shortage_price=navycraft.campaign.markets.price("outpost","fuel_drum","buy")
assert(shortage_price>supplied_price,"M5E strategic shortage did not affect market price")
local outpost_fuel_before=navycraft.campaign.markets.stock("outpost","fuel_drum")
local supply_before=navycraft.campaign.supply.level("outpost","fuel")
local credits_before_escort=navycraft.campaign.career.balance("Brett")
brett:set_pos(home_pos);m5d_vessel.position=vector.new(home_pos)
local shipment,shipment_error=navycraft.campaign.shipping.dispatch("home","outpost",{fuel_drum=6,provisions=10,munitions_crate=3,hull_plate=4},{auto_raid=false,risk=.8});assert(shipment,shipment_error)
local escort_ok,escort_message=navycraft.campaign.shipping.accept("Brett",shipment.id);assert(escort_ok,escort_message)
local convoy=navycraft.campaign.fleets.get(shipment.encounter_id);assert(convoy and convoy.kind=="convoy","M5E convoy encounter missing")
local freighter_unit;local escort_units=0
for _,unit in ipairs(convoy.units or{})do if unit.role=="freighter"then freighter_unit=unit elseif unit.role=="escort"then escort_units=escort_units+1 end end
assert(freighter_unit and escort_units>=1,"M5E freighter and escort composition missing")
local raid_ok,raid_message=navycraft.campaign.shipping.raid(shipment.id,2);assert(raid_ok,raid_message)
local live_shipment=navycraft.campaign.shipping.get(shipment.id);local pirates=navycraft.campaign.fleets.get(live_shipment.pirate_encounter_id);assert(pirates and#pirates.units==2,"M5E pirate raid missing")
for _,unit in ipairs(pirates.units)do local pirate=navycraft.preview.get_by_id(unit.construct_id);assert(pirate,"M5E pirate vessel missing");pirate.systems.hull_integrity=.2;navycraft.campaign.fleets.on_damage(pirate,10,pirate.position,"Brett")end
navycraft.campaign.shipping.step(2)
convoy=navycraft.campaign.fleets.get(shipment.encounter_id)
for _,unit in ipairs(convoy.units or{})do local vessel=unit.construct_id and navycraft.preview.get_by_id(unit.construct_id);if vessel then vessel.position=vector.new(outpost_pos)end end
m5d_vessel.position=vector.new(outpost_pos)
navycraft.campaign.fleets.step(.5);navycraft.campaign.shipping.step(.5)
local delivered=navycraft.campaign.shipping.get(shipment.id);assert(delivered.status=="delivered","M5E convoy did not deliver")
assert(delivered.pirates_defeated==2,"M5E pirate defeat accounting missing")
assert(navycraft.campaign.markets.stock("outpost","fuel_drum")==outpost_fuel_before+6,"M5E destination warehouse delivery missing")
assert(navycraft.campaign.supply.level("outpost","fuel")>supply_before,"M5E strategic fuel delivery missing")
assert(navycraft.campaign.career.balance("Brett")>credits_before_escort,"M5E escort reward missing")
brett:set_pos(outpost_pos)
navycraft.campaign.supply.set("outpost","munitions",0)
local starved_ok=navycraft.campaign.economy.buy("Brett","cannon_shell",1);assert(not starved_ok,"M5E depleted port still sold ammunition")
navycraft.campaign.supply.set("outpost","munitions",80)
local supplied_ok,supplied_message=navycraft.campaign.economy.buy("Brett","cannon_shell",1);assert(supplied_ok,supplied_message)
assert(navycraft.campaign.supply.level("outpost","munitions")<80,"M5E ammunition sale did not consume strategic stores")
brett:set_pos(home_pos);m5d_vessel.position=vector.new(home_pos)
local lost_shipment,lost_error=navycraft.campaign.shipping.dispatch("home","outpost",{provisions=4},{auto_raid=false,ignore_limit=true});assert(lost_shipment,lost_error)
local lost_convoy=navycraft.campaign.fleets.get(lost_shipment.encounter_id);local lost_freighter
for _,unit in ipairs(lost_convoy.units or{})do if unit.role=="freighter"then lost_freighter=navycraft.preview.get_by_id(unit.construct_id)end end
assert(lost_freighter,"M5E loss-test freighter missing");lost_freighter.systems.hull_integrity=.2;navycraft.campaign.fleets.on_damage(lost_freighter,10,lost_freighter.position,"Corsair")
navycraft.campaign.shipping.step(.5);assert(navycraft.campaign.shipping.get(lost_shipment.id).status=="lost","M5E convoy loss was not resolved")
assert(core.registered_chatcommands.supply and core.registered_chatcommands.convoy and core.registered_chatcommands.shipping,"M5E command surface missing")
assert(core.registered_nodes["nc_campaign:logistics_table"]and core.registered_nodes["nc_campaign:convoy_board"],"M5E logistics terminal nodes missing")

-- Exercise M5F title ownership, insurance, capture, resale, salvage and loss modes.
function run_m5f_smoke()
assert(navycraft.campaign.ownership and navycraft.campaign.insurance and navycraft.campaign.capture and navycraft.campaign.salvage,"M5F ownership modules missing")
assert(core.registered_chatcommands.title and core.registered_chatcommands.shipmarket and core.registered_chatcommands.insurance and core.registered_chatcommands.salvage and core.registered_chatcommands.capture and core.registered_chatcommands.lossmode,"M5F command surface missing")
assert(core.registered_nodes["nc_campaign:title_registry"]and core.registered_nodes["nc_campaign:insurance_office"]and core.registered_nodes["nc_campaign:ship_broker"]and core.registered_nodes["nc_campaign:salvage_office"],"M5F service nodes missing")
navycraft.campaign.ownership.set_loss_mode("standard")
m5d_vessel.position=vector.new(home_pos);m5d_vessel.forward_speed=0;m5d_vessel.vertical_speed=0;m5d_vessel.yaw_rate=0
m5d_vessel.systems.hull_integrity=1;m5d_vessel.systems.helm_destroyed=false;m5d_vessel.systems.loss_recorded=nil
brett:set_pos(home_pos);navycraft.shipyard.credit("Brett",5000,"M5F smoke funding")
local original_title=navycraft.campaign.ownership.ensure(m5d_vessel);assert(original_title and original_title.owner=="Brett","M5F vessel title was not issued")
local insured,insured_message=navycraft.campaign.insurance.buy("Brett","comprehensive");assert(insured,insured_message)
assert(navycraft.campaign.insurance.force_mature(original_title.id),"M5F policy could not be matured for deterministic test")
alex:set_pos(home_pos);navycraft.campaign.factions.set("Alex","corsair","M5F capture test")
brett:set_pos(vector.add(home_pos,{x=100,y=0,z=0}))
m5d_vessel.systems.hull_integrity=.4;m5d_vessel.systems.helm_destroyed=true
local capture_ok,capture_message=navycraft.campaign.capture.start("Alex");assert(capture_ok,capture_message)
navycraft.campaign.capture.step(21)
assert(m5d_vessel.systems.owner=="Alex","M5F hostile boarding did not transfer the live vessel")
local claims=navycraft.campaign.insurance.claims_for("Brett");local approved_claim
for _,claim in ipairs(claims)do if claim.title_id==original_title.id and claim.status=="approved"then approved_claim=claim end end
assert(approved_claim,"M5F comprehensive capture claim was not approved")
brett:set_pos(home_pos);navycraft.shipyard.credit("Brett",approved_claim.deductible+100,"M5F deductible funding")
local claim_ok,claim_message=navycraft.campaign.insurance.file("Brett",approved_claim.id);assert(claim_ok,claim_message)
local settled_claim
for _,claim in ipairs(navycraft.campaign.insurance.claims_for("Brett"))do if claim.id==approved_claim.id then settled_claim=claim end end
assert(settled_claim and settled_claim.status=="settled"and navycraft.storage.get("Brett",settled_claim.replacement_name),"M5F insurance replacement was not stored")
-- Direct title transfer requires buyer acceptance and moves the owner-active mapping.
alex:set_pos(home_pos);target_player:set_pos(home_pos);navycraft.shipyard.credit("Target",1000,"M5F buyer funding")
local offer_ok,offer_message=navycraft.campaign.ownership.offer("Alex","Target",75);assert(offer_ok,offer_message)
local offer_id=offer_message:match("(TO%-%d+)");assert(offer_id,"M5F transfer offer id missing")
local accept_ok,accept_message=navycraft.campaign.ownership.accept("Target",offer_id);assert(accept_ok,accept_message)
assert(m5d_vessel.systems.owner=="Target"and navycraft.campaign.ownership.get(original_title.id).owner=="Target","M5F accepted title transfer did not update ownership")
-- Brokered resale escrows the vessel and delivers a stored title to the buyer.
local replacement_ok,replacement_id=navycraft.storage.spawn("Brett",settled_claim.replacement_name,vector.add(home_pos,{x=5,y=0,z=0}),0);assert(replacement_ok,replacement_id)
local replacement_vessel=navycraft.preview.get_by_id(replacement_id);replacement_vessel.position=vector.new(home_pos);replacement_vessel.forward_speed=0;replacement_vessel.vertical_speed=0;replacement_vessel.yaw_rate=0
local listing_ok,listing_message=navycraft.campaign.ownership.list_for_sale("Brett",120);assert(listing_ok,listing_message)
local listing_id=listing_message:match("(SL%-%d+)");assert(listing_id,"M5F sale listing id missing")
target_player:set_pos(home_pos);navycraft.shipyard.credit("Target",500,"M5F broker funding")
local buy_ok,buy_message=navycraft.campaign.ownership.buy_listing("Target",listing_id);assert(buy_ok,buy_message)
local ownership_state=navycraft.campaign.ownership.export();local sold_listing=ownership_state.listings[listing_id]
assert(sold_listing and sold_listing.status=="sold"and navycraft.storage.get("Target",sold_listing.name),"M5F broker purchase did not deliver the stored vessel")
-- A destroyed title becomes a rights-controlled wreck; strict mode forbids restoration.
target_player:set_pos(home_pos);m5d_vessel.position=vector.new(home_pos);m5d_vessel.systems.hull_integrity=0;m5d_vessel.systems.loss_recorded=nil
navycraft.campaign.ownership.step(.1)
local wreck_id
for id,wreck in pairs(navycraft.campaign.salvage.all())do if wreck.title_id==original_title.id and wreck.status=="available"then wreck_id=id end end
assert(wreck_id,"M5F destroyed vessel did not create a salvage record")
assert(navycraft.campaign.salvage.claim("Target",wreck_id))
navycraft.campaign.ownership.set_loss_mode("strict")
local strict_restore=navycraft.campaign.salvage.restore("Target",wreck_id);assert(not strict_restore,"M5F strict loss mode allowed wreck restoration")
navycraft.campaign.ownership.set_loss_mode("standard");navycraft.shipyard.credit("Target",1000,"M5F wreck recovery funding")
local restore_ok,restore_message=navycraft.campaign.salvage.restore("Target",wreck_id);assert(restore_ok,restore_message)
local restored_wreck=navycraft.campaign.salvage.get(wreck_id);assert(restored_wreck.status=="restored"and navycraft.storage.get("Target",restored_wreck.restored_name),"M5F standard wreck restoration failed")
end
run_m5f_smoke()

local function run_m5g_smoke()
 assert(core.registered_chatcommands.station and core.registered_chatcommands.tutorial and core.registered_chatcommands.accessibility,"M5G presentation commands missing")
 assert(core.registered_nodes["nc_campaign:training_console"]and core.registered_nodes["nc_campaign:bridge_console"],"M5G console nodes missing")
 local ok,msg=navycraft.campaign.tutorial.reset("Brett");assert(ok,msg)
 navycraft.campaign.tutorial.step()
 local state,stage=navycraft.campaign.tutorial.get("Brett");assert(stage.id=="station","M5G tutorial did not advance through enlistment and faction steps")
 local ui_vessel=navycraft.campaign.hud.vessel_for("Brett")or craft
 ui_vessel.systems.owner="Brett";ui_vessel.owner="Brett";ui_vessel.systems.captain="Brett";ui_vessel.systems.station_roles=ui_vessel.systems.station_roles or{};ui_vessel.systems.station_roles.Brett="helm"
 assert(navycraft.campaign.interfaces.open("Brett",2),"M5G station interface did not open")
 assert(core.last_formspec and core.last_formspec[2]=="nc_campaign:station"and core.last_formspec[3]:find("Throttle",1,true),"M5G helm formspec missing")
 local offers=navycraft.campaign.missions.offers_for("home","Brett")
 if not navycraft.campaign.missions.active("Brett")and offers[1]then assert(navycraft.campaign.missions.accept_offer("Brett",offers[1]))end
 navycraft.campaign.tutorial.step();navycraft.campaign.interfaces.open("Brett",2)
 for _,callback in ipairs(core.callbacks.on_player_receive_fields or{})do callback(brett,"nc_campaign:station",{throttle_50=true})end
 assert(math.abs((ui_vessel.systems.throttle or 0)-.5)<.001,"M5G helm control did not reach the vessel")
 local tutorial_state=navycraft.campaign.tutorial.get("Brett");assert(tutorial_state.completed,"M5G guided tutorial did not complete")
 navycraft.campaign.hud.rebuild("Brett");navycraft.campaign.hud.refresh("Brett")
 local hud_count=0;for _ in pairs(brett.huds)do hud_count=hud_count+1 end;assert(hud_count>=8,"M5G role-aware HUD elements were not created")
 local top_left_hud=false;local header=false;local vessel_readout=false;local damage_readout=false;local drive_readout=false
 for _,def in pairs(brett.huds)do
  if def.type=="text"and def.position and def.position.x<.1 and def.position.y<.15 and def.alignment and def.alignment.x==1 then top_left_hud=true end
  if def.text and def.text:find("NAVALCLASH",1,true)then header=true end
  if def.text and def.text:find("SPD",1,true)and def.text:find("HDG",1,true)then vessel_readout=true end
  if def.text and def.text:find("HULL [",1,true)and def.text:find("FLOOD",1,true)then damage_readout=true end
  if def.text and def.text:find("GEAR",1,true)and def.text:find("POWER",1,true)then drive_readout=true end
 end
 assert(top_left_hud,"M5G HUD text was not anchored at the top left")
 assert(header and vessel_readout and damage_readout and drive_readout,"M5G game HUD readouts were not populated")
 assert(navycraft.campaign.presentation.set("Brett","high_contrast",true));assert(navycraft.campaign.presentation.set("Brett","large_text",true));navycraft.campaign.hud.rebuild("Brett")
 local pref=navycraft.campaign.presentation.get("Brett");assert(pref.high_contrast and pref.large_text,"M5G accessibility settings were not persisted")
 assert(navycraft.campaign.notifications.push("Brett","critical","Test collision alarm",{cooldown=0,duration=10}),"M5G notification was rejected")
 navycraft.campaign.hud.refresh("Brett");local found=false;for _,def in pairs(brett.huds)do if def.text and def.text:find("Test collision alarm",1,true)then found=true end end;assert(found,"M5G alert was not surfaced on the HUD")
 assert(#navycraft.campaign.notifications.history("Brett")>=1,"M5G notification history is empty")
end
run_m5g_smoke()

local conversion_snapshot=navycraft.preview.snapshot(craft)
local conversion_id,conversion_error=navycraft.preview.spawn_snapshot("ConversionSmoke",conversion_snapshot,{x=1000,y=5,z=1000},0,{allow_multiple=true})
assert(conversion_id,conversion_error)
local conversion_construct=navycraft.preview.get_by_id(conversion_id);assert(conversion_construct,"helm conversion smoke construct missing")
local first_entry=conversion_construct.nodes[1];local restored_pos=vector.round(vector.add(conversion_construct.position,first_entry.local_pos))
local convert_ok,convert_message=navycraft.preview.request_convert_to_blocks(conversion_construct,"Brett");assert(convert_ok,convert_message)
assert(not navycraft.preview.get_by_id(conversion_id),"helm conversion did not remove active construct")
assert(core.get_node_or_nil(restored_pos).name==first_entry.name,"helm conversion did not restore editable blocks")

-- Exercise the M4H native construct-attached effect adapter.
craft.native_id="901"
navycraft.effects.weapon_fire(craft,{x=craft.position.x,y=craft.position.y+1,z=craft.position.z},{kind="cannon"})
assert(#emitted_construct_effects>=2,"M4H weapon effects were not emitted")
assert(emitted_construct_effects[#emitted_construct_effects-1].definition.kind=="sound","M4H sound event missing")
assert(emitted_construct_effects[#emitted_construct_effects].definition.kind=="light_flash","M4H muzzle event missing")

-- Exercise the M4F transactional callback, inventory and formspec layer.
local dynamic_state={fields={},inventories={main={width=1,stacks={"nc_core:frame 2"}}},timer={active=true,timeout=1,elapsed=1},revision=0}
local dynamic_nodes={{pos={x=0,y=0,z=0},name="nc_core:m4f_test"}}
local dynamic_events={{event_id=77,construct_id="900",action="timer",node_pos={x=0,y=0,z=0},
    adjacent_pos={x=1,y=0,z=0},world_point={x=0,y=0,z=0},normal={x=0,y=1,z=0},
    actor="Brett",wielded_item="",node_name="nc_core:m4f_test",elapsed=1,timeout=1,fields={}}}
local resolved_timer;local resolved_mutations={};local persistence_syncs=0
function core.get_dynamic_construct_node_state()return deepcopy(dynamic_state)end
function core.set_dynamic_construct_metadata(_,_,key,value)dynamic_state.fields[key]=value;return true end
function core.set_dynamic_construct_inventory(_,_,name,list)dynamic_state.inventories[name]=deepcopy(list);return true end
function core.start_dynamic_construct_timer()return true end
function core.stop_dynamic_construct_timer()return true end
function core.poll_dynamic_construct_events()local events=dynamic_events;dynamic_events={};return events end
function core.resolve_dynamic_construct_timer(_,event_id,restart,timeout)resolved_timer={event_id,restart,timeout};return true end
function core.resolve_dynamic_construct_mutation(_,event_id,resolution)resolved_mutations[#resolved_mutations+1]={event_id=event_id,resolution=deepcopy(resolution)};return{resolved=true,node_changed=resolution.accepted,action_id=#resolved_mutations}end
function core.get_dynamic_construct(id)if tostring(id)=="900"then return{nodes=deepcopy(dynamic_nodes)}end;local value=native_constructs[tostring(id)];return value and deepcopy(value)or nil end
function core.dynamic_construct_local_to_world(_,pos)return deepcopy(pos)end
function core.sync_dynamic_construct_persistence()persistence_syncs=persistence_syncs+1;return 1 end
core.register_node("nc_core:m4f_test",{groups={cracky=1},
    _navycraft_on_timer=function(context)context.meta:set_string("fired","yes");return 2 end,
    _navycraft_on_construct=function(context)context.meta:set_string("placed","yes")end})
assert(navycraft.dynamic_interactions.poll_once()==1,"M4F timer event adapter did not poll")
assert(dynamic_state.fields.fired=="yes","M4F metadata wrapper failed")
assert(resolved_timer and resolved_timer[1]==77 and resolved_timer[2]==true and resolved_timer[3]==2,"M4F timer callback resolution failed")

brett.wield=ItemStack("nc_core:m4f_test 2")
dynamic_events={{event_id=78,construct_id="900",action="place",node_pos={x=0,y=0,z=0},adjacent_pos={x=1,y=0,z=0},world_point={x=0,y=0,z=0},normal={x=1,y=0,z=0},actor="Brett",wielded_item="nc_core:m4f_test 2",node_name="nc_core:m4f_test",fields={}}}
assert(navycraft.dynamic_interactions.poll_once()==1,"M4F place event did not poll")
assert(resolved_mutations[1]and resolved_mutations[1].resolution.accepted==true,"M4F placement did not resolve")
assert(resolved_mutations[1].resolution.node.name=="nc_core:m4f_test","M4F placement node missing")

dynamic_events={{event_id=79,construct_id="900",action="dig",node_pos={x=0,y=0,z=0},adjacent_pos={x=1,y=0,z=0},world_point={x=0,y=0,z=0},normal={x=1,y=0,z=0},actor="Brett",wielded_item="nc_core:pick 1",node_name="nc_core:m4f_test",fields={}}}
brett.wield=ItemStack("nc_core:pick 1")
assert(navycraft.dynamic_interactions.poll_once()==1,"M4F dig event did not poll")
assert(resolved_mutations[2]and resolved_mutations[2].resolution.accepted==true,"M4F dig did not resolve")
assert(resolved_mutations[2].resolution.drops[1]=="nc_core:m4f_test","M4F dig drops missing")

local context={construct_id="900",node_pos={x=0,y=0,z=0},world_point={x=0,y=0,z=0},actor_name="Brett",player=brett,node_name="nc_core:m4f_test"}
context.inventory={get_list=function()return{"nc_core:frame 2"}end,get_width=function()return 1 end,set_list=function(_,_,stacks,width)dynamic_state.inventories.main={stacks=stacks,width=width};return true end}
context.world_for_local=function(_,pos)return pos end
local formname=navycraft.dynamic_interactions.formspecs.show(context,{lists={"main"}})
assert(formname and core.last_formspec,"M4F construct formspec did not open")

print("NavyCraft Lua game smoke tests passed")

-- NavyCraft weapon implementation derived from OneCannon.java, Torpedo.java and Weapon.java.
-- Weapon type ids, names, salvo counts, cooldown and explosive strengths follow the supplied source.
-- Projectile speeds/ranges are Luanti adaptations because Bukkit tick movement does not map 1:1.
local D = navycraft.definitions
local S = navycraft.systems
local W = {}
local PROJECTILE = "nc_navycraft:weapon_projectile"
local storage = core.get_mod_storage()

local function copy(value)
    return core.deserialize(core.serialize(value))
end

local function normalise_yaw(yaw)
    local full = math.pi * 2
    yaw = yaw % full
    if yaw < 0 then yaw = yaw + full end
    return yaw
end

local function heading_vector(yaw, vertical)
    local horizontal = math.cos(vertical or 0)
    local direction = core.yaw_to_dir and core.yaw_to_dir(yaw or 0) or {
        x = -math.sin(yaw or 0),
        y = 0,
        z = math.cos(yaw or 0),
    }
    return {
        x = (direction.x or 0) * horizontal,
        y = math.sin(vertical or 0),
        z = (direction.z or 0) * horizontal,
    }
end

local function player_name(player)
    if type(player) == "string" then return player end
    return player and player:is_player() and player:get_player_name() or ""
end

local function active_weapon_node(construct, weapon_id)
    for _, weapon in ipairs(construct.profile.weapons or {}) do
        if weapon.type == weapon_id then
            local entry = construct.nodes[weapon.node_index]
            if entry and not entry.destroyed then return weapon.node_index, entry end
        end
    end
    return nil
end

local function projectile_origin(construct, weapon_id)
    local index, entry = active_weapon_node(construct, weapon_id)
    if entry then
        return navycraft.preview.world_position(construct, entry.local_pos), index
    end
    return vector.add(construct.position, {x=0,y=1,z=0}), nil
end

local function use_ammo(construct, ammo_key, amount)
    local systems = construct.systems
    systems.ammo = systems.ammo or {}
    local available = systems.ammo[ammo_key] or 0
    if available < amount then return false, available end
    systems.ammo[ammo_key] = available - amount
    return true, systems.ammo[ammo_key]
end

local function add_particles(position, texture, count)
    if not core.add_particlespawner then return end
    core.add_particlespawner({
        amount=count or 20,time=.15,minpos=vector.subtract(position,1),maxpos=vector.add(position,1),
        minvel={x=-3,y=-3,z=-3},maxvel={x=3,y=3,z=3},minacc={x=0,y=-4,z=0},maxacc={x=0,y=-2,z=0},
        minexptime=.25,maxexptime=1.2,minsize=1,maxsize=4,texture=texture or "nc_frame.png",
    })
end

local function damage_world(position, weapon, owner)
    local radius = math.max(1, math.min(5, math.sqrt(weapon.blast or 1)))
    local minimum = vector.floor(vector.subtract(position, radius))
    local maximum = vector.ceil(vector.add(position, radius))
    for x=minimum.x,maximum.x do
        for y=minimum.y,maximum.y do
            for z=minimum.z,maximum.z do
                local pos={x=x,y=y,z=z}
                local distance=vector.distance(pos,position)
                if distance<=radius and not (core.is_protected and core.is_protected(pos,owner)) then
                    local node=core.get_node_or_nil(pos)
                    if node and node.name~="air" and node.name~="ignore" then
                        local def=core.registered_nodes[node.name]
                        local groups=def and def.groups or {}
                        local armour=groups.navycraft_armour or groups.cracky or 1
                        local effective=(weapon.blast or 1)*(1-distance/(radius+.001))
                        if effective>=armour*1.25 and (groups.unbreakable or 0)==0 then core.remove_node(pos) end
                    end
                end
            end
        end
    end
end

local function damage_objects(position, weapon, owner)
    if not core.get_objects_inside_radius then return end
    local radius=math.max(2,math.sqrt(weapon.blast or 1)*1.6)
    local attacker=owner~="" and core.get_player_by_name(owner) or nil
    for _,object in ipairs(core.get_objects_inside_radius(position,radius)) do
        local object_pos=object.get_pos and object:get_pos() or nil
        local is_owner=object.is_player and object:is_player() and
            object.get_player_name and object:get_player_name()==owner
        if object_pos and not is_owner then
            local distance=vector.distance(position,object_pos)
            local falloff=math.max(0,1-distance/(radius+.001))
            local damage=math.max(1,math.floor((weapon.blast or 1)*falloff))
            local reason={type="navycraft_weapon",from=owner}
            if object.is_player and object:is_player() and object.get_hp and object.set_hp then
                object:set_hp(math.max(0,object:get_hp()-damage),reason)
            elseif object.punch then
                local direction=vector.direction(position,object_pos)
                object:punch(attacker or object,1,{full_punch_interval=1,damage_groups={fleshy=damage}},direction)
            elseif object.get_hp and object.set_hp then
                object:set_hp(math.max(0,object:get_hp()-damage),reason)
            end
        end
    end
end

function W.apply_native_world_explosion(position, projectile)
    if not position or type(projectile)~="table" then return end
    local virtual={
        blast=math.max(0,tonumber(projectile.blast_power) or 0),
        kind=projectile.kind or "native_projectile",
    }
    if virtual.blast<=0 then return end
    local owner=projectile.owner or ""
    damage_world(position,virtual,owner)
    damage_objects(position,virtual,owner)
end

function W.explode(position, data)
    local weapon=D.weapons[tonumber(data.weapon_id)]
    if not weapon then return end
    navycraft.preview.damage_radius(position, math.max(1.5, math.sqrt(weapon.blast)*1.8), weapon.blast,
        data.owner or "", weapon.kind)
    damage_world(position,weapon,data.owner or "")
    damage_objects(position,weapon,data.owner or "")
    add_particles(position, weapon.kind=="fireball" and "nc_frame.png^[colorize:#ff5500:220" or "nc_frame.png^[colorize:#888888:180", 30)
end

local projectile_def = {
    static_save=true,
    initial_properties={
        physical=false, collide_with_objects=false, pointable=false, visual="sprite",
        textures={"nc_frame.png^[colorize:#ffcc55:220"}, visual_size={x=.25,y=.25},
        glow=8, collisionbox={-.1,-.1,-.1,.1,.1,.1},
    },
    data=nil,
    on_activate=function(self,staticdata)
        local decoded=staticdata~="" and core.deserialize(staticdata) or nil
        self.data=type(decoded)=="table" and decoded or self.data or {}
        self.data.age=self.data.age or 0
        self.data.travelled=self.data.travelled or 0
        self.last_pos=self.object:get_pos()
    end,
    get_staticdata=function(self) return core.serialize(self.data or {}) end,
    on_step=function(self,dtime)
        local data=self.data or {}
        local weapon=D.weapons[tonumber(data.weapon_id)]
        if not weapon then self.object:remove();return end
        data.age=(data.age or 0)+dtime
        local pos=self.object:get_pos()
        local previous=self.last_pos or pos
        self.last_pos=vector.new(pos)
        data.travelled=(data.travelled or 0)+vector.distance(previous,pos)

        if weapon.guided and data.target_id then
            local target=navycraft.preview.get_by_id(data.target_id)
            if target then
                local desired=vector.direction(pos,target.position)
                local current=self.object:get_velocity()
                local blend=math.min(1,dtime*1.5)
                local next_velocity=vector.multiply(vector.add(vector.multiply(vector.normalize(current),1-blend),vector.multiply(desired,blend)),weapon.speed)
                self.object:set_velocity(next_velocity)
            end
        end
        if weapon.kind=="torpedo" then
            local velocity=self.object:get_velocity(); velocity.y=0
            if data.depth_y then
                velocity.y=math.max(-1,math.min(1,(data.depth_y-pos.y)*.5))
            end
            self.object:set_velocity(velocity)
        elseif weapon.kind=="bomb" or weapon.kind=="depth_charge" then
            local velocity=self.object:get_velocity(); velocity.y=velocity.y-9.81*dtime; self.object:set_velocity(velocity)
        end

        local armed=data.age>=(data.arming_time or .25)
        if armed then
            local node=core.get_node_or_nil(vector.round(pos))
            if node and node.name~="air" and node.name~="ignore" then
                W.explode(pos,data);self.object:remove();return
            end
            local contact,distance=navycraft.preview.find_nearest(pos,1.25,function(c) return c.id~=data.source_id end)
            if contact and distance and distance<=1.25 then
                W.explode(pos,data);self.object:remove();return
            end
        end
        if data.travelled>(weapon.range or 200) or data.age>90 then
            if weapon.kind=="depth_charge" or weapon.kind=="bomb" then W.explode(pos,data) end
            self.object:remove()
        end
    end,
}
core.register_entity(PROJECTILE,projectile_def)

local function spawn_projectile(construct,owner,weapon,origin,yaw,pitch,target_id,depth_y,offset,launch_velocity)
    local direction
    if launch_velocity then
        direction=vector.normalize(launch_velocity)
    else
        direction=heading_vector(yaw,pitch)
    end
    local start=vector.add(origin,vector.add(vector.multiply(direction,1.1),offset or {x=0,y=0,z=0}))
    if navycraft.projectiles and navycraft.projectiles.native_available() and construct.native_id then
        local projectile_id,error_message=navycraft.projectiles.spawn(
            construct,owner,weapon,start,direction,target_id,depth_y,launch_velocity)
        if projectile_id then return true end
        core.log("error","[NavyCraft] native projectile spawn failed: "..tostring(error_message))
        return false
    end
    local object=core.add_entity(start,PROJECTILE,core.serialize({
        weapon_id=weapon.id,owner=owner,source_id=construct.id,target_id=target_id,
        depth_y=depth_y,arming_time=weapon.arming or .25,age=0,travelled=0,
    }))
    if not object then return false end
    local speed=weapon.speed or 1
    local velocity=launch_velocity or vector.multiply(direction,speed)
    if not launch_velocity and (weapon.kind=="bomb" or weapon.kind=="depth_charge") then velocity.y=math.min(velocity.y,-.1) end
    object:set_velocity(velocity)
    return true
end

function W.fire(construct,player,weapon_id,options)
    options=options or {}
    weapon_id=tonumber(weapon_id or construct.systems.selected_weapon or 0)
    local weapon=D.weapons[weapon_id]
    if not weapon then return false,"unknown weapon type" end
    local owner=player_name(player)
    if not S.authorized(construct,owner,"crew") then return false,"crew permission required" end
    if navycraft.machinery and options.fire_control then
        local ready,message=navycraft.machinery.prepare_fire(construct,weapon_id,options)
        if not ready then return false,message end
    end
    if (weapon.kind=="torpedo" or weapon.kind=="depth_charge" or weapon.kind=="bomb") and not construct.systems.launcher_on then
        return false,"launcher is SAFE"
    end
    local now=os.time()
    local reload_mult=((construct.systems.equipment_modifiers or{}).reload_mult or 1)/math.max(.35,construct.systems.maintenance_system_mult or 1)
    if now-(construct.systems.last_weapon_fire or 0)<D.source.weapon_timeout*reload_mult then
        return false,"weapon is reloading"
    end
    local _,entry=active_weapon_node(construct,weapon_id)
    if not entry and not options.allow_virtual then return false,"this vessel has no "..weapon.display end
    local consume=weapon.count or 1
    if not options.free_ammo then
        local ok,remaining=use_ammo(construct,weapon.ammo,consume)
        if not ok then return false,string.format("not enough %s ammunition (%d available)",weapon.ammo,remaining) end
    end
    local origin=projectile_origin(construct,weapon_id)
    local heading=options.yaw or normalise_yaw((construct.yaw or 0)+math.rad(options.relative_heading or construct.systems.tube_heading or 0))
    local pitch=options.pitch or math.rad(options.pitch_degrees or 0)
    local target_id=options.target_id or construct.systems.target_id
    local depth_y=options.depth_y or ((construct.systems.weapon_depth or 0)~=0 and construct.systems.weapon_depth or nil)
    local fired=0
    for i=1,(weapon.count or 1) do
        local lateral=(i-((weapon.count or 1)+1)/2)*.45
        local offset={x=math.cos(heading)*lateral,y=0,z=math.sin(heading)*lateral}
        if spawn_projectile(construct,owner,weapon,origin,heading,pitch,target_id,depth_y,offset,options.launch_velocity) then fired=fired+1 end
    end
    if fired==0 then
        construct.systems.ammo[weapon.ammo]=(construct.systems.ammo[weapon.ammo] or 0)+consume
        return false,"could not create projectile"
    end
    if navycraft.effects then navycraft.effects.weapon_fire(construct,origin,weapon) end
    construct.systems.last_weapon_fire=now
    construct.systems.selected_weapon=weapon_id
    navycraft.preview.save()
    return true,string.format("Fired %s (%d projectile%s)",weapon.display,fired,fired==1 and "" or "s")
end

function W.fire_aa(construct,player,options)
    options=options or {}
    local owner=player_name(player)
    if not S.authorized(construct,owner,"crew") then return false,"crew permission required" end
    if navycraft.machinery and options.fire_control then
        local ready,message=navycraft.machinery.prepare_fire(construct,-1,options)
        if not ready then return false,message end
    end
    local ammo=construct.systems.ammo or {}
    if (ammo.aa_round or 0)<1 then return false,"no AA rounds" end
    ammo.aa_round=ammo.aa_round-1
    local target_id=options.target_id or construct.systems.target_id
    local target=target_id and navycraft.preview.get_by_id(target_id) or nil
    local yaw=construct.yaw
    local pitch=0
    if target then
        local delta=vector.subtract(target.position,construct.position)
        yaw=math.atan2 and math.atan2(delta.x,delta.z) or math.atan(delta.x,delta.z)
        pitch=math.atan2 and math.atan2(delta.y,math.sqrt(delta.x*delta.x+delta.z*delta.z)) or 0
    end
    local virtual=copy(D.weapons[0]);virtual.id=-1;virtual.display="AA Gun";virtual.kind="aa";virtual.speed=55;virtual.range=300;virtual.blast=2;virtual.count=1
    local origin=vector.add(construct.position,{x=0,y=2,z=0})
    if not spawn_projectile(construct,owner,virtual,origin,yaw,pitch,target_id,nil,nil,options.launch_velocity) then
        ammo.aa_round=ammo.aa_round+1
        return false,"could not create AA projectile"
    end
    if navycraft.effects then navycraft.effects.weapon_fire(construct,origin,virtual) end
    navycraft.preview.save();return true,"AA gun fired"
end

function W.reload_from_player(construct,player,amount)
    local stack=player:get_wielded_item()
    local name=stack:get_name()
    local def=core.registered_items[name]
    local ammo_key=def and def._navycraft_ammo
    if not ammo_key then return false,"hold NavyCraft ammunition" end
    amount=math.max(1,math.min(tonumber(amount) or stack:get_count(),stack:get_count()))
    construct.systems.ammo[ammo_key]=(construct.systems.ammo[ammo_key] or 0)+amount
    stack:take_item(amount);player:set_wielded_item(stack)
    navycraft.preview.save()
    return true,string.format("Loaded %d %s (total %d)",amount,ammo_key,construct.systems.ammo[ammo_key])
end

function W.set_weapon(construct,weapon_id)
    weapon_id=tonumber(weapon_id)
    if not D.weapons[weapon_id] then return false,"weapon type must be 0-9" end
    construct.systems.selected_weapon=weapon_id
    return true,D.weapons[weapon_id].display
end

function W.list(construct)
    local out={}
    for _,id in ipairs(D.weapon_order) do
        local def=D.weapons[id]
        local count=0
        for _,weapon in ipairs(construct.profile.weapons or {}) do if weapon.type==id then count=count+1 end end
        if count>0 then out[#out+1]=string.format("%d:%s x%d ammo=%d",id,def.display,count,construct.systems.ammo[def.ammo] or 0) end
    end
    return #out>0 and table.concat(out," | ") or "no weapon mounts"
end

function W.admin_explosion(position,level,debug_markers,owner)
    level=math.max(1,math.min(100,math.floor(tonumber(level) or 1)))
    if debug_markers then
        local radius=math.min(level,20) -- diagnostic marker safety cap; source accepted 100.
        local placed=0
        for x=-radius,radius do for y=-radius,radius do for z=-radius,radius do
            local distance=math.sqrt(x*x+y*y+z*z)
            if distance<=radius and distance>=radius-1.5 then
                local pos=vector.add(position,{x=x,y=y,z=z})
                local node=core.get_node_or_nil(pos)
                if node and node.name=="air" then
                    core.set_node(pos,{name="nc_navycraft:explosion_debug"})
                    local ref_power=math.max(0,math.floor(level*50*(1-distance/(radius+.001))))
                    local meta=core.get_meta(pos);meta:set_string("infotext",string.format("refPower=%d distance=%.1f",ref_power,distance));placed=placed+1
                end
            end
        end end end
        return true,string.format("Placed %d explosion propagation debug markers",placed)
    end
    local virtual={blast=level,kind="admin"}
    navycraft.preview.damage_radius(position,math.max(2,math.sqrt(level)*2),level,owner or "","admin_explosion")
    damage_world(position,virtual,owner or "")
    add_particles(position,"nc_frame.png^[colorize:#ff6600:220",math.min(200,level*5))
    return true,"Boom level "..level
end

function W.register_items_and_nodes()
    local ammo={
        cannon_shell="Cannon Shell",fireball_shell="Fireball Shell",torpedo_mk1="Torpedo Mk I",
        torpedo_mk2="Torpedo Mk II",torpedo_mk3="Torpedo Mk III",depth_charge="Depth Charge",
        bomb="Bomb",aa_round="AA Round",
    }
    for key,description in pairs(ammo) do
        core.register_craftitem("nc_navycraft:"..key,{
            description=description,inventory_image="nc_frame.png^[colorize:#ccaa55:190",
            stack_max=99,_navycraft_ammo=key,
        })
    end
    for _,id in ipairs(D.weapon_order) do
        local weapon=D.weapons[id]
        core.register_node("nc_navycraft:weapon_"..weapon.key,{
            description="NavyCraft "..weapon.display,
            tiles={"nc_frame.png^[colorize:#663333:180"},paramtype2="facedir",
            groups={cracky=2,navycraft_component=1,navycraft_hull=1,navycraft_weapon=1,navycraft_weight=100,navycraft_armour=2},
            _navycraft_component="weapon",_navycraft_weapon_type=id,
            on_construct=function(pos)
                local meta=core.get_meta(pos);meta:set_int("weapon_type",id);meta:set_string("infotext",weapon.display)
            end,
        })
    end
end

return W

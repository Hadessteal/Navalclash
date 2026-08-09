-- Native construct-attached audio and visual effects.
-- Events are sent in ship-local coordinates so they remain attached while the vessel moves.
local E = {}
local SHIP_AUDIO_ENABLED = false

local muted_ship_sounds = {
    nc_engine_loop = true,
    nc_alarm = true,
    nc_hull_hit = true,
}

local function muted_audio(definition)
    if SHIP_AUDIO_ENABLED then return false end
    if definition.kind == "sound_loop_start" then return true end
    if definition.kind == "sound" and muted_ship_sounds[definition.sound] then return true end
    return false
end

local function local_from_world(construct, world)
    local dx = world.x - construct.position.x
    local dz = world.z - construct.position.z
    local c, s = math.cos(construct.yaw or 0), math.sin(construct.yaw or 0)
    return {x=c*dx+s*dz, y=world.y-construct.position.y, z=-s*dx+c*dz}
end

local function local_bounds(construct)
    local minp, maxp
    for _,entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            local p=entry.local_pos
            if not minp then
                minp={x=p.x,y=p.y,z=p.z};maxp={x=p.x,y=p.y,z=p.z}
            else
                minp.x=math.min(minp.x,p.x);minp.y=math.min(minp.y,p.y);minp.z=math.min(minp.z,p.z)
                maxp.x=math.max(maxp.x,p.x);maxp.y=math.max(maxp.y,p.y);maxp.z=math.max(maxp.z,p.z)
            end
        end
    end
    return minp or {x=0,y=0,z=0}, maxp or {x=0,y=0,z=0}
end

local function fallback_world_pos(construct, local_pos)
    if navycraft.preview and navycraft.preview.world_position then
        return navycraft.preview.world_position(construct, local_pos)
    end
    local c,s=math.cos(construct.yaw or 0),math.sin(construct.yaw or 0)
    return {x=construct.position.x+c*local_pos.x-s*local_pos.z,
        y=construct.position.y+local_pos.y,
        z=construct.position.z+s*local_pos.x+c*local_pos.z}
end

local function fallback(construct, definition)
    local pos=fallback_world_pos(construct,definition.local_pos or {x=0,y=0,z=0})
    if definition.kind=="sound" and core.sound_play and definition.sound then
        core.sound_play(definition.sound,{pos=pos,gain=definition.gain or 1,
            max_hear_distance=definition.max_distance or 64,pitch=definition.pitch or 1},true)
    elseif (definition.kind=="particles" or definition.kind=="light_flash") and core.add_particlespawner then
        local amount=definition.amount or 8
        core.add_particlespawner({amount=amount,time=.08,minpos=vector.subtract(pos,.25),maxpos=vector.add(pos,.25),
            minvel={x=-2,y=0,z=-2},maxvel={x=2,y=3,z=2},minacc={x=0,y=-2,z=0},maxacc={x=0,y=-1,z=0},
            minexptime=definition.lifetime_min or .2,maxexptime=definition.lifetime_max or 1,
            minsize=definition.size_min or .25,maxsize=definition.size_max or 1,
            glow=definition.glow or 0,collisiondetection=definition.collision or false,
            texture=definition.texture or "nc_frame.png"})
    end
end

function E.emit(construct, definition)
    if not construct then return false,"missing construct" end
    definition=definition or {}
    if muted_audio(definition) then return true end
    if construct.native_id and type(core.emit_dynamic_construct_effect)=="function" then
        local ok,err=core.emit_dynamic_construct_effect(construct.native_id,definition)
        return ok~=nil and ok~=false,err
    end
    fallback(construct,definition)
    return true
end

function E.emit_world(construct, world, definition)
    definition=definition or {}
    definition.local_pos=local_from_world(construct,world)
    return E.emit(construct,definition)
end

local function set_persistent(construct, key, active, definition)
    construct._native_effects=construct._native_effects or {}
    local old=construct._native_effects[key]
    local effect_id=definition.effect_id
    if active then
        local signature=table.concat({definition.sound or "",definition.preset or "",
            string.format("%.2f",definition.pitch or 1),string.format("%.2f",definition.gain or 1),
            tostring(definition.amount or 0)},"|")
        if old==signature then return end
        if old then
            E.emit(construct,{kind=definition.kind=="sound_loop_start" and "sound_loop_stop" or "emitter_stop",
                effect_id=effect_id,preset=definition.preset or "generic"})
        end
        E.emit(construct,definition)
        construct._native_effects[key]=signature
    elseif old then
        E.emit(construct,{kind=definition.kind=="sound_loop_start" and "sound_loop_stop" or "emitter_stop",
            effect_id=effect_id,preset=definition.preset or "generic"})
        construct._native_effects[key]=nil
    end
end

function E.update(construct, dt)
    if not construct.systems then return end
    local s=construct.systems
    local minp,maxp=local_bounds(construct)
    local center={x=(minp.x+maxp.x)/2,y=(minp.y+maxp.y)/2,z=(minp.z+maxp.z)/2}
    local stern={x=center.x,y=math.max(minp.y,center.y-.5),z=minp.z-.6}
    local deck={x=center.x,y=maxp.y+.6,z=center.z}
    local speed=math.abs(construct.forward_speed or 0)
    local throttle=math.max(0,math.min(1,s.throttle or 0))

    set_persistent(construct,"engine_sound",false,{
        kind="sound_loop_start",effect_id=1001,preset="engine",local_pos=center,
        sound="nc_engine_loop",gain=.3+.55*throttle,pitch=.75+.65*throttle,max_distance=100})
    set_persistent(construct,"exhaust",s.engines_on and not s.submerged_mode and not s.sinking,{
        kind="emitter_start",effect_id=1002,preset="exhaust",local_pos=stern,direction={x=0,y=.25,z=-1},
        amount=math.floor(5+18*throttle),size_min=.25,size_max=.9,lifetime_min=.5,lifetime_max=1.8,
        texture="nc_smoke.png"})
    set_persistent(construct,"wake",speed>.25 and not s.submerged_mode and not s.sinking,{
        kind="emitter_start",effect_id=1003,preset="wake",local_pos=stern,direction={x=0,y=.3,z=-1},
        amount=math.floor(6+math.min(35,speed*4)),size_min=.18,size_max=.55,lifetime_min=.35,lifetime_max=1.1,
        texture="nc_splash.png",collision=false})
    set_persistent(construct,"flood",(s.flooding or 0)>1 and not s.submerged_mode,{
        kind="emitter_start",effect_id=1004,preset="flood",local_pos={x=center.x,y=minp.y+.2,z=center.z},
        direction={x=0,y=1,z=0},amount=math.floor(math.min(30,3+(s.flooding or 0))),
        size_min=.12,size_max=.35,lifetime_min=.25,lifetime_max=.8,texture="nc_splash.png"})
    local damaged=(s.hull_integrity or 1)<.65
    set_persistent(construct,"damage_smoke",damaged and not s.sinking,{
        kind="emitter_start",effect_id=1005,preset=(s.hull_integrity or 1)<.35 and "fire" or "smoke",
        local_pos=deck,direction={x=0,y=1,z=0},amount=(s.hull_integrity or 1)<.35 and 18 or 8,
        size_min=.3,size_max=1.1,lifetime_min=.6,lifetime_max=2.2,
        texture=(s.hull_integrity or 1)<.35 and "nc_fire.png" or "nc_smoke.png",glow=(s.hull_integrity or 1)<.35 and 10 or 0})
    if s.sinking and not construct._sinking_effect_announced then
        construct._sinking_effect_announced=true
        E.emit(construct,{kind="sound",preset="flood",local_pos=center,sound="nc_alarm",gain=.9,max_distance=120})
        E.emit(construct,{kind="particles",preset="splash",local_pos=deck,amount=36,size_min=.25,size_max=1.2,
            lifetime_min=.4,lifetime_max=1.5,texture="nc_splash.png"})
    elseif not s.sinking then construct._sinking_effect_announced=nil end
end

function E.weapon_fire(construct, world_origin, weapon)
    local local_pos=local_from_world(construct,world_origin)
    local preset=(weapon.kind=="torpedo" and "torpedo") or
        (weapon.kind=="depth_charge" and "depth_charge") or "muzzle"
    local sound=(weapon.kind=="torpedo" and "nc_torpedo_launch") or
        (weapon.kind=="depth_charge" and "nc_depth_charge") or "nc_cannon"
    E.emit(construct,{kind="sound",preset=preset,local_pos=local_pos,sound=sound,gain=1,
        pitch=.94+math.random()*.12,max_distance=180})
    E.emit(construct,{kind="light_flash",preset="muzzle",local_pos=local_pos,direction={x=0,y=0,z=1},
        amount=weapon.kind=="torpedo" and 8 or 18,size_min=.25,size_max=1.1,
        lifetime_min=.08,lifetime_max=.35,texture=weapon.kind=="torpedo" and "nc_bubble.png" or "nc_spark.png",glow=14})
end

function E.damage(construct, world_pos, removed)
    E.emit_world(construct,world_pos,{kind="particles",preset="damage_sparks",amount=math.min(48,6+removed*3),
        size_min=.12,size_max=.45,lifetime_min=.15,lifetime_max=.7,texture="nc_spark.png",glow=10,collision=true})
    E.emit_world(construct,world_pos,{kind="sound",preset="damage_sparks",sound="nc_hull_hit",gain=.65,max_distance=90})
end

return E

-- Native server-authoritative projectile bridge for Milestone 4I.
-- The C++ engine owns projectile motion, guidance, swept construct hits and construct damage.
-- Lua only supplies Luanti-map collision queries and mirrors native damage into gameplay state.
local P = {last_events = {}, enabled = false}

local function round_position(position)
    return {
        x = math.floor((position.x or 0) + 0.5),
        y = math.floor((position.y or 0) + 0.5),
        z = math.floor((position.z or 0) + 0.5),
    }
end

local function protocol_version()
    if type(core.get_dynamic_construct_protocol_version) ~= "function" then return 0 end
    local ok, value = pcall(core.get_dynamic_construct_protocol_version)
    return ok and tonumber(value) or 0
end

function P.native_available()
    return protocol_version() >= 6 and
        type(core.spawn_dynamic_construct_projectile) == "function" and
        type(core.step_dynamic_construct_projectiles) == "function" and
        type(core.impact_dynamic_construct_projectile) == "function"
end

local function is_liquid_node(node)
    if not node or node.name == "ignore" then return false end
    local def = core.registered_nodes[node.name]
    if not def then return false end
    return def.liquidtype == "source" or def.liquidtype == "flowing" or
        (def.groups and ((def.groups.water or 0) > 0 or (def.groups.liquid or 0) > 0))
end

local function native_target_id(target_id)
    if not target_id or not navycraft.preview then return nil end
    local target = navycraft.preview.get_by_id(target_id)
    return target and target.native_id or nil
end

local function spec_for(weapon, origin, depth_y)
    local kind = weapon.kind == "shell" and "shell" or weapon.kind
    local is_falling = kind == "bomb" or kind == "depth_charge"
    local is_ballistic = kind == "shell" or kind == "fireball" or kind == "aa"
    local is_torpedo = kind == "torpedo"
    local speed = math.max(0.1, tonumber(weapon.speed) or 1)
    return {
        kind = kind,
        radius = is_torpedo and 0.24 or (is_falling and 0.22 or 0.08),
        gravity = (is_falling or is_ballistic) and 9.81 or 0,
        drag = is_torpedo and 0.015 or 0,
        arming_time = tonumber(weapon.arming) or 0.25,
        maximum_age = math.max(5, (tonumber(weapon.range) or 200) / speed + 5),
        maximum_range = tonumber(weapon.range) or 200,
        blast_radius = math.max(1.5, math.sqrt(tonumber(weapon.blast) or 1) * 1.8),
        blast_power = tonumber(weapon.blast) or 1,
        penetration = (tonumber(weapon.blast) or 1) * (is_torpedo and 1.5 or 0.75),
        guided = weapon.guided == true,
        guidance_turn_rate = weapon.guided and 1.5 or 0,
        preferred_depth = depth_y or origin.y,
        requires_water = is_torpedo,
        detonate_on_expiry = is_falling,
    }
end

function P.spawn(construct, owner, weapon, origin, direction, target_id, depth_y, launch_velocity)
    if not P.native_available() or not construct.native_id then
        return nil, "native projectile engine unavailable"
    end
    local velocity = launch_velocity and {
        x = tonumber(launch_velocity.x) or 0,
        y = tonumber(launch_velocity.y) or 0,
        z = tonumber(launch_velocity.z) or 0,
    } or vector.multiply(direction, tonumber(weapon.speed) or 1)
    if not launch_velocity and (weapon.kind == "bomb" or weapon.kind == "depth_charge") then
        velocity.y = math.min(velocity.y, -0.1)
    end
    local definition = spec_for(weapon, origin, depth_y)
    definition.owner = owner or ""
    definition.position = origin
    definition.velocity = velocity
    definition.target_id = native_target_id(target_id)
    local id, error_message = core.spawn_dynamic_construct_projectile(
        construct.native_id, definition)
    if not id then return nil, error_message or "native projectile rejected" end
    return id
end

local function group_damage(node_damage)
    local grouped = {}
    for _, damage in ipairs(node_damage or {}) do
        if damage.construct_id then
            local key = tostring(damage.construct_id)
            grouped[key] = grouped[key] or {}
            grouped[key][#grouped[key] + 1] = damage
        end
    end
    return grouped
end

local function mirror_impact(event)
    local impact = event and event.impact
    local explosion = impact and impact.explosion
    if not explosion then return end
    local projectile = event.projectile or {}
    for native_id, damage in pairs(group_damage(explosion.node_damage)) do
        if navycraft.preview and navycraft.preview.apply_native_damage then
            navycraft.preview.apply_native_damage(native_id, damage, impact.position,
                projectile.owner or "", projectile.kind or "native_projectile")
        end
    end
    -- The native core owns construct damage. Luanti map nodes, players and Lua
    -- entities still belong to the server game environment, so apply that part
    -- once from the authoritative impact event.
    if navycraft.weapons and navycraft.weapons.apply_native_world_explosion then
        navycraft.weapons.apply_native_world_explosion(impact.position, projectile)
    end
end

local function first_world_hit(start_pos, end_pos, projectile)
    if type(core.raycast) ~= "function" then return nil end
    local ok, ray = pcall(core.raycast, start_pos, end_pos, false, true)
    if not ok or not ray then return nil end
    for pointed in ray do
        if pointed.type == "node" then
            local node = core.get_node_or_nil(pointed.under)
            local liquid = is_liquid_node(node)
            if projectile.kind == "torpedo" or projectile.kind == "depth_charge" then
                if not liquid then
                    return pointed.intersection_point or pointed.under, "terrain"
                end
            else
                return pointed.intersection_point or pointed.under,
                    liquid and "water" or "terrain"
            end
        end
    end
    return nil
end

local function manual_impact(projectile, position, kind)
    local event, error_message = core.impact_dynamic_construct_projectile(
        projectile.id, position, kind)
    if not event then
        core.log("warning", "[NavyCraft] native projectile impact failed: " ..
            tostring(error_message))
        return nil
    end
    mirror_impact(event)
    return event
end

local function process_events(events)
    local processed = {}
    for _, event in ipairs(events or {}) do
        local projectile = event.projectile or {}
        if event.event == "impact" then
            mirror_impact(event)
            processed[#processed + 1] = event
        elseif event.event == "update" then
            local position, kind = first_world_hit(
                projectile.previous_position or projectile.position,
                projectile.position, projectile)
            if position then
                local impact_event = manual_impact(projectile, position, kind)
                if impact_event then processed[#processed + 1] = impact_event end
            elseif projectile.requires_water and (projectile.age or 0) > 0.25 then
                local node = core.get_node_or_nil and core.get_node_or_nil(round_position(projectile.position))
                if node and not is_liquid_node(node) then
                    local impact_event = manual_impact(projectile, projectile.position, "water")
                    if impact_event then processed[#processed + 1] = impact_event end
                else
                    processed[#processed + 1] = event
                end
            else
                processed[#processed + 1] = event
            end
        else
            processed[#processed + 1] = event
        end
    end
    P.last_events = processed
    return processed
end

function P.step(delta_seconds)
    if not P.native_available() then return {} end
    local remaining = math.max(0, math.min(tonumber(delta_seconds) or 0, 1.0))
    local all = {}
    while remaining > 0 do
        local step = math.min(remaining, 0.05)
        local events, error_message = core.step_dynamic_construct_projectiles(step)
        if not events then
            core.log("error", "[NavyCraft] native projectile step failed: " ..
                tostring(error_message))
            break
        end
        for _, event in ipairs(process_events(events)) do all[#all + 1] = event end
        remaining = remaining - step
    end
    P.last_events = all
    return all
end

function P.count()
    if type(core.get_dynamic_construct_projectiles) ~= "function" then return 0 end
    local projectiles = core.get_dynamic_construct_projectiles()
    return type(projectiles) == "table" and #projectiles or 0
end

P.enabled = P.native_available()
if P.enabled then
    core.register_globalstep(P.step)
    core.log("action", "[NavyCraft] native server-authoritative projectile engine enabled")
end

return P

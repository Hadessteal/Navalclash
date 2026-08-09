-- Native NavyCraft fire-control and AI gunnery bridge.
-- Source terminology follows firecontrol/TDC controls from the supplied Bukkit code.
-- Continuous lead solving and automatic batteries are Luanti engine adaptations.
local D = navycraft.definitions
local F = {battery_map = {}, accumulator = 0, last_orders = {}, last_solutions = {}}

local function protocol_version()
    if type(core.get_dynamic_construct_protocol_version) ~= "function" then return 0 end
    local ok, value = pcall(core.get_dynamic_construct_protocol_version)
    return ok and tonumber(value) or 0
end

function F.native_available()
    return protocol_version() >= 7 and
        type(core.observe_dynamic_construct_target) == "function" and
        type(core.solve_dynamic_construct_fire_control) == "function" and
        type(core.configure_dynamic_construct_battery) == "function" and
        type(core.step_dynamic_construct_fire_control) == "function"
end

local function now_seconds()
    if type(core.get_us_time) == "function" then return core.get_us_time() / 1000000 end
    return os.clock()
end

local function velocity_of(construct)
    local yaw = construct.yaw or 0
    local speed = construct.forward_speed or 0
    local direction = core.yaw_to_dir and core.yaw_to_dir(yaw) or {x = -math.sin(yaw), y = 0, z = math.cos(yaw)}
    return {
        x = (direction.x or 0) * speed,
        y = construct.vertical_speed or 0,
        z = (direction.z or 0) * speed,
    }
end

local function classification(construct)
    local kind = construct and construct.profile and construct.profile.craft_type or "unknown"
    if kind == "aircraft" or kind == "airship" then return "air" end
    if kind == "submarine" then return "submarine" end
    if kind == "tank" then return "ground" end
    if kind == "boat" or kind == "ship" or kind == "freeship" or kind == "halfship" then
        return "surface"
    end
    return "unknown"
end

local function contact_mode(construct)
    local systems = construct.systems or {}
    if systems.sonar_mode and systems.sonar_mode ~= "off" then return systems.sonar_mode end
    if systems.radar_on then return "radar" end
    return "detector"
end

local function sensor_name(mode)
    if mode == "passive" then return "passive_sonar" end
    if mode == "active" or mode == "hf" then return "active_sonar" end
    if mode == "radar" then return "radar" end
    return "visual"
end

local function game_target_id(native_id)
    if native_id == nil then return nil end
    for id, construct in pairs(navycraft.preview.get_all()) do
        if tostring(construct.native_id) == tostring(native_id) then return id end
    end
    return nil
end

local function native_target(construct_id)
    local target = navycraft.preview.get_by_id(construct_id)
    return target and target.native_id or nil, target
end

local function observe_construct(construct, sample_time)
    if not F.native_available() or not construct.native_id then return end
    local mode = contact_mode(construct)
    local contacts = navycraft.systems.sensor_contacts(construct, mode)
    for _, contact in ipairs(contacts) do
        local target_native_id, target = native_target(contact.id)
        if target_native_id and target then
            local ok, error_message = core.observe_dynamic_construct_target(
                construct.native_id,
                {
                    target_id = target_native_id,
                    sample_time = sample_time,
                    position = target.position,
                    velocity = velocity_of(target),
                    classification = classification(target),
                    sensor = sensor_name(mode),
                    confidence = math.max(0.05, math.min(1, contact.strength or 0.5)),
                })
            if not ok and error_message then
                core.log("warning", "[NavyCraft] target observation rejected: " ..
                    tostring(error_message))
            end
        end
    end
end

local function weapon_mount_local(construct, weapon_id)
    for _, mount in ipairs(construct.profile and construct.profile.weapons or {}) do
        if mount.type == weapon_id then
            local node = construct.nodes and construct.nodes[mount.node_index]
            if node and not node.destroyed then return node.local_pos end
        end
    end
    return {x = 0, y = 1, z = 0}
end

local function component_local(construct, component)
    for _, node in ipairs(construct.nodes or {}) do
        if not node.destroyed then
            local definition = core.registered_nodes and core.registered_nodes[node.name]
            if definition and definition._navycraft_component == component then
                return node.local_pos
            end
        end
    end
    return {x = 0, y = 1, z = 0}
end

local function weapon_spec(weapon, construct, mode)
    local kind = weapon.kind == "shell" and "shell" or weapon.kind
    local falling = kind == "bomb" or kind == "depth_charge"
    local torpedo = kind == "torpedo"
    local gravity = falling and 9.81 or 0
    -- The original cannon code generated a parabolic path. Native shells use
    -- physical gravity from M4J so the fire-control solution and projectile agree.
    if kind == "shell" or kind == "fireball" or kind == "aa" then gravity = 9.81 end
    return {
        kind = kind,
        muzzle_speed = math.max(0.1, tonumber(weapon.speed) or 1),
        gravity = gravity,
        drag = torpedo and 0.015 or 0,
        minimum_range = torpedo and math.max(5, tonumber(weapon.arming) or 5) or 0,
        maximum_range = tonumber(weapon.range) or 200,
        maximum_lead_time = math.max(5, (tonumber(weapon.range) or 200) /
            math.max(0.1, tonumber(weapon.speed) or 1) + 5),
        minimum_pitch = torpedo and -0.25 or -math.pi / 2,
        maximum_pitch = torpedo and 0.25 or math.pi / 2,
        preferred_depth = construct.systems.weapon_depth or construct.position.y,
        guided = weapon.guided == true,
        guidance_turn_rate = weapon.guided and 1.5 or 0,
        arc = construct.systems.fire_control_arc or "low",
        mode = mode,
    }
end

local function aa_weapon()
    return {
        id = -1, display = "Automatic AA", kind = "aa", speed = 55,
        range = 300, blast = 2, ammo = "aa_round", count = 1,
    }
end

local function battery_weapon(construct, mode)
    if mode == "defensive" then return aa_weapon() end
    return D.weapons[tonumber(construct.systems.selected_weapon) or 0]
end

local function battery_key(construct)
    return tostring(construct.native_id or construct.id)
end

local function remove_battery(construct)
    local key = battery_key(construct)
    local entry = F.battery_map[key]
    if entry and type(core.remove_dynamic_construct_battery) == "function" then
        pcall(core.remove_dynamic_construct_battery, entry.id)
    end
    F.battery_map[key] = nil
    if construct.systems then construct.systems.fire_control_battery_id = nil end
end

local function ensure_battery(construct, current_time)
    if not F.native_available() or not construct.native_id or not construct.systems then return end
    local mode = construct.systems.fire_control_mode or "manual"
    if mode == "manual" then remove_battery(construct); return end
    local weapon = battery_weapon(construct, mode)
    if not weapon then remove_battery(construct); return end
    local key = battery_key(construct)
    local existing = F.battery_map[key]
    local target_native_id = native_target(construct.systems.target_id)
    local local_pos = mode == "defensive" and component_local(construct, "aa_gun") or
        weapon_mount_local(construct, weapon.id)
    local signature = table.concat({mode, tostring(weapon.id), tostring(target_native_id or ""),
        tostring(construct.systems.fire_control_arc or "low"), tostring(local_pos.x or 0),
        tostring(local_pos.y or 0), tostring(local_pos.z or 0)}, ":")
    if existing and existing.signature == signature then return end
    local definition = weapon_spec(weapon, construct, mode)
    definition.id = existing and existing.id or construct.systems.fire_control_battery_id
    definition.muzzle_local = local_pos
    definition.mode = mode == "auto" and "automatic" or mode
    definition.allowed_class = mode == "defensive" and "air" or nil
    definition.target_id = target_native_id
    definition.minimum_confidence = mode == "defensive" and 0.30 or 0.20
    definition.reload_seconds = mode == "defensive" and 0.50 or D.source.weapon_timeout
    definition.next_ready_time = current_time
    if existing and type(core.get_dynamic_construct_fire_control_batteries) == "function" then
        local batteries = core.get_dynamic_construct_fire_control_batteries(construct.native_id) or {}
        for _, battery in ipairs(batteries) do
            if tostring(battery.id) == tostring(existing.id) then
                definition.next_ready_time = battery.next_ready_time or current_time
                break
            end
        end
    end
    definition.traverse_centre = 0
    definition.traverse_half_width = math.pi
    definition.enabled = true
    local id, error_message = core.configure_dynamic_construct_battery(
        construct.native_id, definition)
    if not id then
        core.log("warning", "[NavyCraft] battery configuration failed: " ..
            tostring(error_message))
        return
    end
    F.battery_map[key] = {id = id, construct_id = construct.id, weapon_id = weapon.id, target_id = construct.systems.target_id, signature = signature}
    construct.systems.fire_control_battery_id = id
end

function F.solve(construct, weapon_id, target_id, options)
    options = options or {}
    if not construct or not construct.native_id then return nil, "native vessel required" end
    if not F.native_available() then return nil, "native fire-control engine unavailable" end
    local weapon = tonumber(weapon_id) == -1 and aa_weapon() or D.weapons[tonumber(weapon_id)]
    if not weapon then return nil, "unknown weapon" end
    target_id = target_id or construct.systems.target_id
    local target_native_id, target = native_target(target_id)
    if not target_native_id or not target then return nil, "target not found" end
    local sample_time = options.solution_time or now_seconds()
    core.observe_dynamic_construct_target(construct.native_id, {
        target_id = target_native_id,
        sample_time = sample_time,
        position = target.position,
        velocity = velocity_of(target),
        classification = classification(target),
        sensor = sensor_name(contact_mode(construct)),
        confidence = options.confidence or 0.95,
    })
    local definition = weapon_spec(weapon, construct, "manual")
    definition.solution_time = sample_time
    definition.muzzle_local = tonumber(weapon_id) == -1 and
        component_local(construct, "aa_gun") or weapon_mount_local(construct, tonumber(weapon_id))
    local solution, error_message = core.solve_dynamic_construct_fire_control(
        construct.native_id, target_native_id, definition)
    if not solution then return nil, error_message or "solution failed" end
    F.last_solutions[construct.id] = solution
    return solution
end

function F.set_mode(construct, mode)
    mode = (mode or ""):lower()
    if mode == "automatic" then mode = "auto" end
    if mode ~= "manual" and mode ~= "auto" and mode ~= "defensive" then
        return false, "fire-control mode must be manual, auto or defensive"
    end
    construct.systems.fire_control_mode = mode
    if mode == "manual" then remove_battery(construct) end
    navycraft.preview.save()
    return true, "Fire control " .. mode:upper()
end

function F.set_tdc_mode(construct, mode)
    mode = (mode or ""):lower()
    if mode ~= "straight" and mode ~= "periscope" and mode ~= "auto" then
        return false, "TDC mode must be straight, periscope or auto"
    end
    construct.systems.tdc_mode = mode
    construct.systems.tube_auto = mode == "auto"
    navycraft.preview.save()
    return true, "TDC " .. mode:upper()
end

function F.format_solution(solution)
    if not solution then return "no solution" end
    if not solution.valid then return "no solution: " .. tostring(solution.reason or "invalid") end
    return string.format("range=%.1f lead=%.2fs yaw=%.1f pitch=%.1f miss=%.2f confidence=%.2f",
        solution.range or 0, solution.intercept_time or 0, math.deg(solution.yaw or 0),
        math.deg(solution.pitch or 0), solution.miss_distance or 0,
        solution.track_confidence or 0)
end

function F.status(construct)
    local systems = construct.systems or {}
    local target = systems.target_id or "none"
    local battery = systems.fire_control_battery_id or "none"
    local solution = F.last_solutions[construct.id]
    local solution_text = solution and F.format_solution(solution) or "no solution"
    return string.format("mode=%s tdc=%s target=%s battery=%s %s",
        systems.fire_control_mode or "manual", systems.tdc_mode or "straight",
        target, tostring(battery), solution_text)
end

local function execute_order(order)
    local entry = F.battery_map[tostring(order.shooter_id)]
    if not entry then
        for _, candidate in pairs(F.battery_map) do
            if tostring(candidate.id) == tostring(order.battery_id) then entry = candidate; break end
        end
    end
    if not entry then return end
    local construct = navycraft.preview.get_by_id(entry.construct_id)
    if not construct or not order.solution or not order.solution.valid then return end
    local options = {
        target_id = game_target_id(order.target_id) or entry.target_id or construct.systems.target_id,
        launch_velocity = order.solution.launch_velocity,
        aim_point = order.solution.aim_point,
        yaw = order.solution.yaw,
        pitch = order.solution.pitch,
        fire_control = true,
    }
    local ok, message
    if entry.weapon_id == -1 then
        ok, message = navycraft.weapons.fire_aa(construct, construct.systems.owner, options)
    else
        ok, message = navycraft.weapons.fire(construct, construct.systems.owner,
            entry.weapon_id, options)
    end
    if not ok and message and message ~= "weapon is reloading" then
        core.log("action", "[NavyCraft] automatic battery withheld fire: " .. tostring(message))
    end
end

function F.step(delta_seconds)
    if not F.native_available() then return end
    F.accumulator = F.accumulator + math.max(0, tonumber(delta_seconds) or 0)
    if F.accumulator < 0.20 then return end
    F.accumulator = 0
    local current_time = now_seconds()
    local active_constructs = {}
    for _, construct in pairs(navycraft.preview.get_all()) do
        if construct.native_id and construct.systems then
            active_constructs[construct.id] = true
            observe_construct(construct, current_time)
            ensure_battery(construct, current_time)
        end
    end
    for key, entry in pairs(F.battery_map) do
        if not active_constructs[entry.construct_id] then
            if type(core.remove_dynamic_construct_battery) == "function" then
                pcall(core.remove_dynamic_construct_battery, entry.id)
            end
            F.battery_map[key] = nil
        end
    end
    local orders, error_message = core.step_dynamic_construct_fire_control(current_time, 10.0)
    if not orders then
        core.log("error", "[NavyCraft] fire-control step failed: " .. tostring(error_message))
        return
    end
    F.last_orders = orders
    for _, order in ipairs(orders) do execute_order(order) end
end

if F.native_available() then
    core.register_globalstep(F.step)
    core.log("action", "[NavyCraft] native fire-control and AI gunnery enabled")
end

return F

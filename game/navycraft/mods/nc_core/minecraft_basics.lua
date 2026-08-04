-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Minecraft-like development controls, digging and command aliases.

local B = {}

core.override_item("", {
    range = 5,
    tool_capabilities = {
        full_punch_interval = 0.5,
        max_drop_level = 3,
        groupcaps = {
            cracky = {times = {[1]=0.55, [2]=0.35, [3]=0.20}, uses = 0, maxlevel = 3},
            crumbly = {times = {[1]=0.45, [2]=0.25, [3]=0.12}, uses = 0, maxlevel = 3},
            choppy = {times = {[1]=0.55, [2]=0.35, [3]=0.20}, uses = 0, maxlevel = 3},
            snappy = {times = {[1]=0.35, [2]=0.20, [3]=0.10}, uses = 0, maxlevel = 3},
            oddly_breakable_by_hand = {
                times = {[1]=0.35, [2]=0.20, [3]=0.10},
                uses = 0,
                maxlevel = 3,
            },
        },
        damage_groups = {fleshy = 1},
        punch_attack_uses = 0,
    },
})

local function spawn_position()
    local configured = core.settings:get("static_spawnpoint")
    if configured and core.string_to_pos then
        local parsed = core.string_to_pos(configured)
        if parsed then return parsed end
    end
    local y = core.get_spawn_level and core.get_spawn_level(0, 0)
    return {x = 0, y = (y or 9) + 1, z = 0}
end

local function reset_velocity(player)
    local velocity = player:get_velocity()
    if velocity and player.add_velocity then
        player:add_velocity({x=-velocity.x, y=-velocity.y, z=-velocity.z})
    end
end

local function set_mode(name, mode)
    mode = mode:lower():trim()
    if mode == "1" or mode == "creative" or mode == "c" then
        mode = "creative"
    elseif mode == "0" or mode == "survival" or mode == "s" then
        mode = "survival"
    else
        return false, "Usage: /gamemode creative|survival"
    end

    local player = core.get_player_by_name(name)
    if not player then return false, "Player unavailable" end
    player:get_meta():set_string("nc_gamemode", mode)

    local privs = core.get_player_privs(name)
    if mode == "creative" then
        privs.fly = true
        privs.fast = true
        privs.noclip = true
    else
        privs.fly = nil
        privs.fast = nil
        privs.noclip = nil
    end
    core.set_player_privs(name, privs)
    return true, "Set own game mode to " .. mode
end

core.register_chatcommand("gamemode", {
    params = "creative|survival",
    description = "Set your game mode",
    privs = {interact = true},
    func = set_mode,
})

core.register_chatcommand("gmc", {
    description = "Set creative mode",
    privs = {interact = true},
    func = function(name) return set_mode(name, "creative") end,
})

core.register_chatcommand("gms", {
    description = "Set survival mode",
    privs = {interact = true},
    func = function(name) return set_mode(name, "survival") end,
})

core.register_chatcommand("spawn", {
    description = "Return to world spawn",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        local state = navycraft and (navycraft.construct_state or navycraft.preview)
        if state then state.clear_passenger(name) end
        player:set_pos(spawn_position())
        reset_velocity(player)
        return true, "Teleported to spawn"
    end,
})

core.register_chatcommand("tp", {
    params = "<x> <y> <z>",
    description = "Teleport to coordinates",
    privs = {interact = true},
    func = function(name, param)
        local x, y, z = param:match("^%s*(-?[%d%.]+)%s+(-?[%d%.]+)%s+(-?[%d%.]+)%s*$")
        if not x then return false, "Usage: /tp <x> <y> <z>" end
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        local state = navycraft and (navycraft.construct_state or navycraft.preview)
        if state then state.clear_passenger(name) end
        player:set_pos({x=tonumber(x), y=tonumber(y), z=tonumber(z)})
        reset_velocity(player)
        return true, string.format("Teleported to %.1f %.1f %.1f", x, y, z)
    end,
})

core.register_chatcommand("give", {
    params = "<item> [count]",
    description = "Give yourself an item",
    privs = {interact = true},
    func = function(name, param)
        local item, count = param:match("^%s*(%S+)%s*(%d*)%s*$")
        if not item or item == "" then return false, "Usage: /give <item> [count]" end
        if not core.registered_items[item] then return false, "Unknown item: " .. item end
        count = tonumber(count) or 1
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        local leftover = player:get_inventory():add_item("main", ItemStack(item .. " " .. count))
        if not leftover:is_empty() then core.add_item(player:get_pos(), leftover) end
        return true, "Gave " .. name .. " " .. count .. " of " .. item
    end,
})

core.register_chatcommand("clear", {
    description = "Clear your inventory",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        local inv = player:get_inventory()
        inv:set_list("main", {})
        inv:set_list("craft", {})
        return true, "Cleared inventory"
    end,
})

core.register_chatcommand("kill", {
    description = "Kill yourself",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        player:set_hp(0, {type="set_hp", from="mod"})
        return true
    end,
})

-- Make commands case-insensitive, so /NC_KIT behaves like /nc_kit.
core.register_on_chatcommand(function(name, command, params)
    local lower = command:lower()
    if lower == command then return false end
    local definition = core.registered_chatcommands[lower]
    if not definition then return false end

    local allowed, missing = core.check_player_privs(name, definition.privs or {})
    if not allowed then
        core.chat_send_player(name,
            "You don't have permission (missing: " .. table.concat(missing, ", ") .. ")")
        return true
    end

    local ok, result = definition.func(name, params or "")
    if result and result ~= "" then core.chat_send_player(name, result) end
    if ok == false and not result then
        core.chat_send_player(name, "Invalid command usage")
    end
    return true
end)

core.register_on_joinplayer(function(player)
    if player:get_meta():get_string("nc_gamemode") == "" then
        player:get_meta():set_string("nc_gamemode", "creative")
    end
end)

B.spawn_position = spawn_position
return B

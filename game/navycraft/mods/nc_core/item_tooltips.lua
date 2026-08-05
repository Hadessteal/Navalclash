-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Compact Minecraft-inspired inventory for NavyCraft.

local T = {}
local selected_hud = {}
local last_index = {}
local hide_generation = {}
local e_was_down = {}
local inventory_open = {}
local timer = 0

local function item_description(stack)
    if not stack or stack:is_empty() then return "" end
    local definition = stack:get_definition() or {}
    local description = definition.description
    if not description or description == "" then
        description = stack:get_name()
    end
    return description or ""
end

local function item_detail(stack)
    if not stack or stack:is_empty() then return "" end
    local definition = stack:get_definition() or {}
    local groups = definition.groups or {}
    local parts = {}

    if definition._navycraft_component then
        parts[#parts + 1] = "Component: " .. tostring(definition._navycraft_component)
    end
    if groups.navycraft_hull then
        parts[#parts + 1] = "Ship block"
    end
    if groups.navycraft_helm then
        parts[#parts + 1] = "Helm"
    end
    if groups.navycraft_weight then
        parts[#parts + 1] = "Weight " .. tostring(groups.navycraft_weight)
    end
    return table.concat(parts, "  •  ")
end

local function inventory_formspec()
    return table.concat({
        "formspec_version[6]",
        "size[10.3,9.45]",
        "no_prepend[]",
        "bgcolor[#00000000;false]",
        "background9[0,0;10.3,9.45;nc_mc_inventory.png;false;12]",
        "listcolors[#8b8b8b;#b8b8b8;#373737;#100f10;#ffffff]",
        "style_type[list;size=0.78;spacing=0.14]",
        "style_type[button;border=false;bgcolor=#777777;bgcolor_hovered=#999999;bgcolor_pressed=#555555;textcolor=#202020]",
        "style[close;border=false;bgcolor=#c6c6c6;bgcolor_hovered=#dedede;bgcolor_pressed=#a0a0a0;textcolor=#303030]",

        "label[0.55,0.35;Inventory]",
        "image[0.75,0.95;2.6,3.25;nc_player_silhouette.png]",
        "label[4.30,0.55;Crafting]",
        "list[current_player;craft;4.40,1.05;2,2;0]",
        "label[6.65,1.72;→]",
        "list[current_player;craftpreview;7.35,1.55;1,1;0]",

        "label[0.55,4.43;Inventory]",
        "list[current_player;main;0.78,4.82;9,3;9]",
        "list[current_player;main;0.78,8.18;9,1;0]",

        "listring[current_player;main]",
        "listring[current_player;craft]",
        "listring[current_player;main]",
        "button_exit[8.92,0.22;0.75,0.55;close;X]",
    })
end

local function ensure_hud(player)
    local name = player:get_player_name()
    if selected_hud[name] then return end

    selected_hud[name] = {
        title = player:hud_add({
            type = "text",
            position = {x = 0.5, y = 0.92},
            offset = {x = 0, y = -49},
            alignment = {x = 0, y = 0},
            number = 0xFFFFFF,
            text = "",
            size = {x = 1.0, y = 1.0},
            style = 1,
            z_index = 50,
        }),
        detail = player:hud_add({
            type = "text",
            position = {x = 0.5, y = 0.92},
            offset = {x = 0, y = -29},
            alignment = {x = 0, y = 0},
            number = 0xD0D0D0,
            text = "",
            size = {x = 0.85, y = 0.85},
            z_index = 50,
        }),
    }
end

local function hide_later(name)
    hide_generation[name] = (hide_generation[name] or 0) + 1
    local generation = hide_generation[name]
    core.after(2.5, function()
        if hide_generation[name] ~= generation then return end
        local player = core.get_player_by_name(name)
        local ids = selected_hud[name]
        if not player or not ids then return end
        player:hud_change(ids.title, "text", "")
        player:hud_change(ids.detail, "text", "")
    end)
end

local function show_selected(player)
    ensure_hud(player)
    local name = player:get_player_name()
    local stack = player:get_wielded_item()
    local ids = selected_hud[name]
    player:hud_change(ids.title, "text", item_description(stack))
    player:hud_change(ids.detail, "text", item_detail(stack))
    hide_later(name)
end

local function apply_inventory(player)
    local inv = player:get_inventory()
    if inv then
        if inv:get_size("main") < 36 then inv:set_size("main", 36) end
        if inv:get_size("craft") < 4 then inv:set_size("craft", 4) end
        if inv:get_size("craftpreview") < 1 then inv:set_size("craftpreview", 1) end
        inv:set_width("main", 9)
        inv:set_width("craft", 2)
    end
    player:set_inventory_formspec(inventory_formspec())
end

local function open_inventory(player)
    apply_inventory(player)
    local name = player:get_player_name()
    core.show_formspec(name, "", inventory_formspec())
    inventory_open[name] = true
end

local function close_inventory(player)
    local name = player:get_player_name()
    if core.close_formspec then
        core.close_formspec(name, "")
    else
        core.show_formspec(name, "", "")
    end
    inventory_open[name] = false
end

local function toggle_inventory(player)
    if inventory_open[player:get_player_name()] then
        close_inventory(player)
    else
        open_inventory(player)
    end
end

core.register_on_joinplayer(function(player)
    apply_inventory(player)
    ensure_hud(player)
    local name = player:get_player_name()
    last_index[name] = player:get_wield_index()
    e_was_down[name] = false
    inventory_open[name] = false
    core.after(0.2, function()
        local current = core.get_player_by_name(name)
        if current then show_selected(current) end
    end)
end)

core.register_on_leaveplayer(function(player)
    local name = player:get_player_name()
    selected_hud[name] = nil
    last_index[name] = nil
    hide_generation[name] = nil
    e_was_down[name] = nil
    inventory_open[name] = nil
end)

core.register_on_player_receive_fields(function(player, formname, fields)
    if formname ~= "" then return false end
    if fields.quit then
        inventory_open[player:get_player_name()] = false
    end
    return false
end)

core.register_globalstep(function(dtime)
    timer = timer + dtime
    if timer < 0.05 then return end
    timer = 0

    for _, player in ipairs(core.get_connected_players()) do
        local name = player:get_player_name()
        local index = player:get_wield_index()
        if last_index[name] ~= index then
            last_index[name] = index
            show_selected(player)
        end

        local controls = player:get_player_control()
        local e_down = controls and controls.aux1 or false
        if e_down and not e_was_down[name] then
            toggle_inventory(player)
        end
        e_was_down[name] = e_down
    end
end)

core.register_chatcommand("inventory", {
    description = "Open inventory",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        open_inventory(player)
        return true
    end,
})

core.register_chatcommand("nc_inventory", {
    description = "Open inventory",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        open_inventory(player)
        return true
    end,
})

core.register_chatcommand("nc_tooltips", {
    description = "Refresh inventory and selected-item names",
    privs = {interact = true},
    func = function(name)
        local player = core.get_player_by_name(name)
        if not player then return false, "Player unavailable" end
        apply_inventory(player)
        show_selected(player)
        return true, "Inventory refreshed"
    end,
})

T.refresh = apply_inventory
T.formspec = inventory_formspec
T.open = open_inventory
return T

local H = {}
local states = {}
local accum = 0

local function vessel_for(name)
    local c = navycraft.preview.get_for_owner(name)
    if c then return c end
    for _, x in pairs(navycraft.preview.get_all()) do
        if x.systems and (name == x.systems.captain or (x.systems.crew or {})[name]) then
            return x
        end
    end
end
H.vessel_for = vessel_for

local function clamp(value, low, high)
    value = tonumber(value) or 0
    if value < low then return low end
    if value > high then return high end
    return value
end

local function colour(palette, key)
    return tonumber((palette[key] or "#FFFFFFFF"):sub(2, 7), 16) or 0xFFFFFF
end

local function limit(text, maximum)
    text = tostring(text or "")
    if #text <= maximum then return text end
    return text:sub(1, maximum - 3) .. "..."
end

local function heading(yaw)
    local degrees = (math.deg(yaw or 0) % 360 + 360) % 360
    return math.floor(degrees + .5)
end

local function bar(value, width)
    value = clamp(value, 0, 1)
    local filled = math.floor(value * width + .5)
    return string.rep("#", filled) .. string.rep(".", width - filled)
end

local function hull_colour_key(percent)
    if percent <= 35 then return "critical" end
    if percent <= 65 then return "warning" end
    return "good"
end

local function flood_colour_key(flooding)
    flooding = tonumber(flooding) or 0
    if flooding >= 10 then return "critical" end
    if flooding >= 3 then return "warning" end
    return "good"
end

local function severity_colour_key(severity)
    if severity == "critical" then return "critical" end
    if severity == "warning" then return "warning" end
    if severity == "success" then return "good" end
    return "accent"
end

local function gear_text(gear)
    gear = tonumber(gear) or 0
    if gear > 0 then return "F" .. tostring(gear) end
    if gear < 0 then return "R" .. tostring(math.abs(gear)) end
    return "N"
end

local function rudder_text(rudder)
    rudder = tonumber(rudder) or 0
    if math.abs(rudder) < .05 then return "MID" end
    if rudder < 0 then return "L" .. string.format("%.1f", math.abs(rudder)) end
    return "R" .. string.format("%.1f", rudder)
end

local function role_for(name, c)
    if not c then return "ON FOOT" end
    return string.upper(navycraft.campaign.crew.role(c, name) or "passenger")
end

local function mission_progress(name)
    local m = navycraft.campaign.missions.active(name)
    if not m then return "No active contract" end
    local progress = m.kind == "patrol" and ("stage " .. tostring(m.stage or 1))
        or string.format("%.0f/%.0f", m.progress or 0, m.target or 0)
    return m.title .. " - " .. progress
end

local function station_text(name, c)
    if not c then return "Explore, build, or place a helm to command a vessel" end
    local s = c.systems or {}
    local role = navycraft.campaign.crew.role(c, name) or "passenger"
    if role == "engineer" then
        return string.format("ENGINES %s   PUMPS %s %.0f   BALLAST %.0f%%",
            s.engines_on and "RUN" or "STOP", s.pump_on and "ON" or "OFF",
            s.pump_charge or 0, s.ballast_air_percent or 100)
    elseif role == "gunner" then
        return string.format("WEAPON %s   TARGET %s   FIRE CTRL %s",
            tostring(s.selected_weapon or 0), tostring(s.target_id or "none"),
            tostring(s.fire_control_mode or "manual"))
    elseif role == "sensor" then
        return string.format("RADAR %s   SONAR %s   TARGET %s",
            s.radar_on and "ON" or "OFF", tostring(s.sonar_mode or "off"),
            tostring(s.target_id or "none"))
    elseif role == "helm" then
        return string.format("HELM ACTIVE   PWR %d%%   RUDDER %s",
            math.floor((s.throttle or 0) * 100 + .5), rudder_text(s.rudder))
    elseif role == "captain" or role == "executive" then
        local crew = 0
        for _ in pairs(s.crew or {}) do crew = crew + 1 end
        return string.format("COMMAND   CREW %d   NAV %s   TARGET %s",
            crew, tostring(s.navigation_status or "manual"), tostring(s.target_id or "none"))
    end
    return string.upper(role)
end

local function readouts(name)
    local career = navycraft.campaign.career.get(name)
    local rank = navycraft.campaign.career.rank(name)
    local faction = navycraft.campaign.factions.membership(name)
    local c = vessel_for(name)
    local s = c and c.systems or {}
    local rank_name = rank.display or rank.name or "Crew"
    local faction_name = faction and tostring(faction.faction):upper() or "UNALIGNED"
    local header = string.format("NAVALCLASH  %s  %s  %dc",
        role_for(name, c), rank_name, navycraft.campaign.career.balance(name))
    local vessel_name = c and (s.custom_name or (c.profile and c.profile.craft_type) or "Vessel")
        or "No active vessel"
    local speed = c and math.abs(c.forward_speed or 0) or 0
    local vessel = string.format("%s   SPD %.1f   HDG %03d   %s",
        limit(vessel_name, 18), speed, heading(c and c.yaw or 0), faction_name)
    local hull = math.floor(clamp(s.hull_integrity or 1, 0, 1) * 100 + .5)
    local flood = tonumber(s.flooding) or 0
    local damage = string.format("HULL [%s] %3d%%   FLOOD %4.1f",
        bar(hull / 100, 10), hull, flood)
    local drive = c and string.format("GEAR %s   POWER %3d%%   RUD %s   COND %3d%%",
        gear_text(s.gear), math.floor((s.throttle or 0) * 100 + .5),
        rudder_text(s.rudder), math.floor(clamp(s.maintenance_condition or 1, 0, 1) * 100 + .5))
        or "GEAR N   POWER   0%   RUD MID   COND 100%"
    return {
        header = header,
        vessel = vessel,
        damage = damage,
        drive = drive,
        station = station_text(name, c),
        mission = "OBJECTIVE  " .. mission_progress(name),
        hull_key = hull_colour_key(hull),
        flood_key = flood_colour_key(flood),
    }
end

local function add(player, def)
    if not player.hud_add then return nil end
    return player:hud_add(def)
end

local function change(player, id, field, value)
    if id and player.hud_change then player:hud_change(id, field, value) end
end

local function remove(player, id)
    if id and player.hud_remove then player:hud_remove(id) end
end

local function panel(player, x, y, w, h)
    return add(player, {
        type = "image",
        position = {x = 0, y = 0},
        offset = {x = x, y = y},
        scale = {x = w, y = h},
        text = "nc_hud_panel.png",
        alignment = {x = 1, y = 1},
        z_index = -100,
    })
end

local function text(player, x, y, color, scale)
    return add(player, {
        type = "text",
        position = {x = 0, y = 0},
        offset = {x = x, y = y},
        text = "",
        number = color,
        alignment = {x = 1, y = 1},
        style = 1,
        size = {x = scale, y = scale},
    })
end

function H.rebuild(name)
    local player = core.get_player_by_name(name)
    if not player then return end
    local old = states[name]
    if old then
        for _, id in pairs(old.ids) do remove(player, id) end
    end
    local pref = navycraft.campaign.presentation.get(name)
    if pref.hud_mode == "off" then states[name] = {ids = {}}; return end

    local palette = navycraft.campaign.presentation.palette(name)
    local font_scale = pref.large_text and 1.12 or .92
    local header_scale = pref.large_text and 1.18 or .98
    local alert_scale = pref.large_text and 1.35 or 1.12
    local full = pref.hud_mode == "full"
    local ids = {}

    ids.background = panel(player, 8, 8, 7.7, full and 6.5 or 3.65)
    ids.header = text(player, 18, 16, colour(palette, "accent"), header_scale)
    ids.vessel = text(player, 18, 34, colour(palette, "text"), font_scale)
    ids.damage = text(player, 18, 52, colour(palette, "good"), font_scale)
    ids.drive = text(player, 18, 70, colour(palette, "warning"), font_scale)
    if full then
        ids.station = text(player, 18, 91, colour(palette, "text"), font_scale)
        ids.mission = text(player, 18, 110, colour(palette, "warning"), font_scale)
    end
    ids.alert = text(player, 18, full and 134 or 94, colour(palette, "critical"), alert_scale)

    states[name] = {ids = ids}
    H.refresh(name)
end

function H.refresh(name)
    local player = core.get_player_by_name(name)
    if not player then return end
    local state = states[name]
    if not state then H.rebuild(name); state = states[name] end
    local pref = navycraft.campaign.presentation.get(name)
    local palette = navycraft.campaign.presentation.palette(name)
    local r = readouts(name)
    change(player, state.ids.header, "text", r.header)
    change(player, state.ids.vessel, "text", r.vessel)
    change(player, state.ids.damage, "text", r.damage)
    change(player, state.ids.damage, "number", colour(palette, r.flood_key == "critical" and "critical" or r.hull_key))
    change(player, state.ids.drive, "text", r.drive)
    change(player, state.ids.station, "text", r.station)
    change(player, state.ids.mission, "text", r.mission)
    local alert = navycraft.campaign.notifications.latest(name)
    local alert_text = alert and (string.upper(alert.severity or "alert") .. "  " .. alert.text) or ""
    change(player, state.ids.alert, "text", alert_text)
    change(player, state.ids.alert, "number", colour(palette, severity_colour_key(alert and alert.severity)))
    if pref.hud_mode ~= "off" and vessel_for(name) then
        local c = vessel_for(name)
        local s = c.systems or {}
        if (s.hull_integrity or 1) < .5 then
            navycraft.campaign.notifications.push(name, "critical", "Hull integrity below 50 percent", {key = "hull-low", cooldown = 15})
        end
        if (s.flooding or 0) > 5 then
            navycraft.campaign.notifications.push(name, "warning", "Flooding rising: " .. string.format("%.1f", s.flooding), {key = "flooding", cooldown = 15})
        end
    end
end

function H.step(dt)
    accum = accum + (dt or 0)
    if accum < .25 then return end
    accum = 0
    for _, player in ipairs(core.get_connected_players()) do
        H.refresh(player:get_player_name())
    end
end

function H.remove(name)
    local player = core.get_player_by_name(name)
    local state = states[name]
    if player and state then
        for _, id in pairs(state.ids) do remove(player, id) end
    end
    states[name] = nil
end

return H

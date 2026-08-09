local C = {}

local function copy(value)
    if type(value) ~= "table" then return value end
    local out = {}
    for k, v in pairs(value) do out[k] = copy(v) end
    return out
end

local styles = {
    bottom = {
        recent = {
            anchor = "bottom_left",
            margin_left = 14,
            margin_right = 14,
            margin_top = 8,
            margin_bottom = 92,
            font = "standard",
            font_size = 17,
            line_spacing = 1,
            text_color = {a = 245, r = 235, g = 242, b = 250},
        },
        console = {
            anchor = "bottom_left",
            margin_left = 0,
            margin_right = 0,
            margin_top = 0,
            margin_bottom = 0,
            font = "mono",
            font_size = 18,
            line_spacing = 2,
            height_speed = 9,
            text_color = {a = 255, r = 245, g = 248, b = 252},
            prompt_color = {a = 255, r = 255, g = 255, b = 255},
            background_color = {a = 190, r = 4, g = 8, b = 14},
        },
    },
    top = {
        recent = {
            anchor = "top_left",
            margin_left = 10,
            margin_right = 20,
            margin_top = 5,
            margin_bottom = 62,
            font = "default",
            font_size = 0,
            line_spacing = 0,
        },
        console = {
            anchor = "top_left",
            margin_left = 0,
            margin_right = 0,
            margin_top = 0,
            margin_bottom = 0,
            font = "mono",
            font_size = 0,
            line_spacing = 0,
            height_speed = 5,
        },
    },
    compact = {
        recent = {
            anchor = "bottom_left",
            margin_left = 12,
            margin_right = 12,
            margin_top = 8,
            margin_bottom = 78,
            font = "standard",
            font_size = 14,
            line_spacing = 0,
        },
        console = {
            anchor = "bottom_left",
            margin_left = 8,
            margin_right = 8,
            margin_top = 8,
            margin_bottom = 8,
            font = "mono",
            font_size = 15,
            line_spacing = 0,
            height_speed = 10,
            background_color = {a = 180, r = 0, g = 0, b = 0},
        },
    },
}

local active = "bottom"

local function engine_ready()
    return type(core.set_chat_style) == "function"
end

function C.apply(name)
    name = name or active
    local style = styles[name]
    if not style then return false, "Unknown chat style: " .. tostring(name) end
    active = name
    if not engine_ready() then
        core.log("warning", "[NavyCraft] chat style API is not available in this engine build")
        return false, "This engine build does not expose Lua chat styling"
    end
    local ok, message = core.set_chat_style(copy(style))
    if not ok then return false, message or "Chat style rejected by engine" end
    return true, "Chat style set to " .. name
end

function C.status()
    if type(core.get_chat_style) ~= "function" then
        return "Chat style API unavailable in this engine build"
    end
    local current = core.get_chat_style()
    local recent = current and current.recent or {}
    local console = current and current.console or {}
    return string.format(
        "chat=%s recent=%s console=%s font=%s:%s/%s:%s",
        active,
        tostring(recent.anchor),
        tostring(console.anchor),
        tostring(recent.font),
        tostring(recent.font_size),
        tostring(console.font),
        tostring(console.font_size))
end

core.register_on_mods_loaded(function()
    C.apply(active)
end)

core.register_chatcommand("nc_chat_style", {
    params = "bottom|top|compact|status",
    description = "Change NavyCraft chat window layout without rebuilding the engine",
    privs = {interact = true},
    func = function(_, param)
        local value = (param or ""):lower()
        if value == "" or value == "status" then return true, C.status() end
        return C.apply(value)
    end,
})

return C

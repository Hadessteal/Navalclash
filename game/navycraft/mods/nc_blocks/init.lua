local modpath = core.get_modpath(core.get_current_modname())
local registry = dofile(modpath .. "/block_registry.lua")

nc_blocks = rawget(_G, "nc_blocks") or {}
nc_blocks.registry = registry

local function register_block(entry)
    local definition = {
        description = entry.name,
        tiles = {entry.texture},
        groups = entry.groups or {oddly_breakable_by_hand = 1},
        _navycraft_block = entry.id,
    }

    if entry.id:find("glass", 1, true) then
        definition.drawtype = "glasslike"
        definition.paramtype = "light"
        definition.sunlight_propagates = true
    elseif entry.id:find("leaves", 1, true) then
        definition.drawtype = "allfaces_optional"
        definition.paramtype = "light"
    elseif entry.id:find("sapling", 1, true) or entry.id:find("flower", 1, true) or entry.id:find("mushroom", 1, true) or entry.id == "dead_bush" then
        definition.drawtype = "plantlike"
        definition.paramtype = "light"
        definition.walkable = false
        definition.buildable_to = true
        definition.selection_box = {type = "fixed", fixed = {-0.3, -0.5, -0.3, 0.3, 0.35, 0.3}}
    elseif entry.id:find("water", 1, true) then
        definition.drawtype = "liquid"
        definition.paramtype = "light"
        definition.walkable = false
        definition.pointable = false
        definition.diggable = false
        definition.buildable_to = true
        definition.liquidtype = "source"
        definition.liquid_alternative_flowing = "nc_core:water_flowing"
        definition.liquid_alternative_source = "nc_core:water_source"
        definition.liquid_viscosity = 1
        definition.post_effect_color = {a = 103, r = 24, g = 92, b = 140}
    elseif entry.id:find("lava", 1, true) then
        definition.light_source = 8
        definition.groups = entry.groups or {cracky = 1}
    end

    core.register_node("nc_blocks:" .. entry.id, definition)
end

for _, entry in ipairs(registry) do
    register_block(entry)
end

dofile(modpath .. "/worldgen.lua")

core.log("action", string.format("[NavyCraft] registered %d generic block definitions", #registry))

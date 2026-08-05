navycraft = rawget(_G,"navycraft") or {}
local modpath=core.get_modpath(core.get_current_modname())
navycraft.definitions=dofile(modpath.."/definitions.lua")
navycraft.effects=dofile(modpath.."/effects.lua")
navycraft.systems=dofile(modpath.."/systems.lua")
navycraft.projectiles=dofile(modpath.."/projectiles.lua")
navycraft.weapons=dofile(modpath.."/weapons.lua")
navycraft.fire_control=dofile(modpath.."/fire_control.lua")
navycraft.machinery=dofile(modpath.."/machinery.lua")
navycraft.fluids=dofile(modpath.."/fluids.lua")
navycraft.storage=dofile(modpath.."/storage.lua")
navycraft.routes=dofile(modpath.."/routes.lua")
navycraft.navigation=dofile(modpath.."/navigation.lua")
navycraft.structure=dofile(modpath.."/structure.lua")
navycraft.radio=dofile(modpath.."/radio.lua")
navycraft.controls=dofile(modpath.."/controls.lua")
navycraft.weapons.register_items_and_nodes()
navycraft.nodes=dofile(modpath.."/nodes.lua")
if navycraft.dynamic_interactions then
    navycraft.dynamic_interactions.register_handler(function(context, event)
        if event.action ~= "place" and event.action ~= "activate" then return false end
        local construct = context.get_game_construct()
        if not construct or not context.player then return false end
        local index = navycraft.preview.find_node_index(construct, context.node_pos)
        if not index then return false end
        return navycraft.nodes.handle_moving_interaction(
            construct, index, context.player, "rightclick")
    end)
end
dofile(modpath.."/commands.lua")
navycraft.preview.register_launch_hook(navycraft.systems.launch_hook)
navycraft.preview.register_step_hook(navycraft.systems.step)
navycraft.preview.register_step_hook(navycraft.navigation.step)
navycraft.preview.register_boarding_filter(navycraft.systems.boarding_filter)
navycraft.preview.register_interaction_hook(navycraft.nodes.handle_moving_interaction)
navycraft.preview.register_damage_hook(navycraft.systems.damage_hook)
navycraft.preview.register_dock_hook(function(construct)
    if navycraft.shipyard and navycraft.shipyard.reward_craft then navycraft.shipyard.reward_craft(construct) end
    return true
end)
core.log("action","[NavyCraft] complete source-parity gameplay layer Milestone 4N loaded")

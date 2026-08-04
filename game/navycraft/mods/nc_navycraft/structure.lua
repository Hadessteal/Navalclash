-- Native structural connectivity and wreck-fragment bridge for Milestone 4L.
local M={states={},last_events={},last_step_frame=nil}

local function protocol_version()
    if type(core.get_dynamic_construct_protocol_version)~="function" then return 0 end
    local ok,value=pcall(core.get_dynamic_construct_protocol_version)
    return ok and tonumber(value)or 0
end
function M.native_available()
    return protocol_version()>=9 and
        type(core.step_dynamic_construct_structure)=="function" and
        type(core.get_dynamic_construct_structure)=="function"
end

local function water_level()
    if type(core.get_mapgen_setting)=="function" then
        local ok,value=pcall(core.get_mapgen_setting,"water_level")
        if ok and tonumber(value) then return tonumber(value) end
    end
    return 1
end

local function sync_game_flooding()
    if type(core.set_dynamic_construct_flooding)~="function" then return end
    for _,construct in pairs(navycraft.preview.get_all and navycraft.preview.get_all() or{})do
        if construct.native_id and construct.systems then
            local systems=construct.systems
            local displacement=math.max(1,tonumber(systems.displacement)or tonumber(systems.block_count)or #(construct.nodes or{}))
            local flooded=tonumber(systems.flooded_volume)or tonumber(systems.flooding)or 0
            pcall(core.set_dynamic_construct_flooding,construct.native_id,math.max(0,math.min(1,flooded/displacement)))
        end
    end
end

local function apply_state(state)
    if not state or not state.construct_id then return end
    M.states[tostring(state.construct_id)]=state
    for _,construct in pairs(navycraft.preview.get_all and navycraft.preview.get_all() or{})do
        if tostring(construct.native_id)==tostring(state.construct_id) and construct.systems then
            construct.systems.native_structural_integrity=state.integrity
            construct.systems.native_flooded_fraction=state.flooded_fraction
            construct.systems.native_component_count=state.component_count
            construct.systems.native_structural_role=state.role
            construct.systems.native_buoyancy_force=state.buoyancy_force
            construct.systems.native_weight_force=state.weight_force
            if state.sinking then construct.systems.sinking=true end
            break
        end
    end
end

local function emit_split_effect(event)
    if type(core.emit_dynamic_construct_effect)~="function" then return end
    for _,fragment_id in ipairs(event.fragment_ids or{})do
        pcall(core.emit_dynamic_construct_effect,fragment_id,{
            kind="particles",preset="damage_sparks",local_pos={x=0,y=0,z=0},
            texture="nc_spark.png",amount=24,lifetime_min=.25,lifetime_max=.8,
            size_min=.35,size_max=1.3,velocity={x=0,y=1.4,z=0},glow=8,
        })
        pcall(core.emit_dynamic_construct_effect,fragment_id,{
            kind="sound",preset="damage_sparks",local_pos={x=0,y=0,z=0},
            sound="nc_hull_hit",gain=.9,pitch=1,max_distance=96,
        })
    end
end

function M.step(dt)
    if not M.native_available() then return {} end
    sync_game_flooding()
    local events,err=core.step_dynamic_construct_structure(math.max(0,math.min(tonumber(dt)or 0,1)),{
        water_level=water_level(),split_enabled=true,minimum_fragment_nodes=1,
        maximum_enclosure_cells=500000,apply_fragment_physics=true,
        apply_primary_physics=false,recenter_fragments=true,
    })
    if not events then
        core.log("error","[NavyCraft] native structural step failed: "..tostring(err))
        return {}
    end
    for _,event in ipairs(events)do
        apply_state(event.state)
        if event.event=="split"then
            emit_split_effect(event)
            if navycraft.fluids then for _,fragment_id in ipairs(event.fragment_ids or{})do navycraft.fluids.configure_fragment(fragment_id)end end
            core.log("action",string.format("[NavyCraft] construct %s split into %d detached fragment(s)",
                tostring(event.construct_id),#(event.fragment_ids or{})))
        elseif event.event=="sinking"then
            core.log("action","[NavyCraft] construct "..tostring(event.construct_id).." is sinking")
        end
    end
    M.last_events=events
    return events
end

function M.status(construct)
    if not construct or not construct.native_id then return "native structure unavailable" end
    local state=M.states[tostring(construct.native_id)]
    if not state and type(core.get_dynamic_construct_structure)=="function"then
        local ok,value=pcall(core.get_dynamic_construct_structure,construct.native_id)
        if ok then state=value;apply_state(value)end
    end
    if not state then return "structural state pending" end
    return string.format("role=%s components=%d nodes=%d integrity=%.1f%% flooded=%.1f%% mass=%.2f displacement=%.2f %s",
        tostring(state.role),tonumber(state.component_count)or 0,tonumber(state.node_count)or 0,
        100*(tonumber(state.integrity)or 0),100*(tonumber(state.flooded_fraction)or 0),
        tonumber(state.mass)or 0,tonumber(state.effective_displacement)or 0,
        state.sunk and"SUNK"or(state.sinking and"SINKING"or(state.afloat and"AFLOAT"or"NEGATIVE BUOYANCY")))
end

function M.force_split(construct)
    if not M.native_available()or not construct or not construct.native_id then
        return false,"native structural engine unavailable"
    end
    local events,err=core.force_dynamic_construct_split(construct.native_id,{
        water_level=water_level(),minimum_fragment_nodes=1,apply_fragment_physics=true,
    })
    if not events then return false,err end
    local fragments=0
    for _,event in ipairs(events)do
        if event.event=="split"then fragments=fragments+#(event.fragment_ids or{});emit_split_effect(event)end
        apply_state(event.state)
    end
    return true,fragments>0 and("Detached "..fragments.." fragment(s)")or"Construct is still connected"
end

M.enabled=M.native_available()
if M.enabled then
    core.register_globalstep(M.step)
    core.log("action","[NavyCraft] native structural splitting and wreck buoyancy enabled")
end
return M

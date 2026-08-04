-- Native construct-local liquid, compartment, pump and specialised-node bridge for M4N.
local M={states={},accumulator=0}

local function protocol_version()
    if type(core.get_dynamic_construct_protocol_version)~="function" then return 0 end
    local ok,value=pcall(core.get_dynamic_construct_protocol_version)
    return ok and tonumber(value)or 0
end

function M.native_available()
    return protocol_version()>=11 and
        type(core.configure_dynamic_construct_liquid_compartment)=="function" and
        type(core.configure_dynamic_construct_liquid_port)=="function" and
        type(core.step_dynamic_construct_liquids)=="function" and
        type(core.get_dynamic_construct_liquids)=="function"
end

local function key(id)return tostring(id)end
local function pkey(p)return tostring(p.x)..":"..tostring(p.y)..":"..tostring(p.z)end

local function bilge_cells(nodes)
    local occupied,cells={},{}
    for _,node in ipairs(nodes or{})do
        local p=node.local_pos or node.pos
        if p and not node.destroyed then occupied[pkey(p)]=true end
    end
    local seen={}
    for _,node in ipairs(nodes or{})do
        local p=node.local_pos or node.pos
        if p and not node.destroyed then
            local candidate={x=math.floor(p.x),y=math.floor(p.y)-1,z=math.floor(p.z)}
            local k=pkey(candidate)
            if not occupied[k]and not seen[k]then cells[#cells+1]=candidate;seen[k]=true end
            if #cells>=4096 then break end
        end
    end
    if #cells==0 then cells[1]={x=0,y=-1,z=0} end
    return cells
end

local function configure_ports(state,systems)
    local ballast=systems and tonumber(systems.ballast_mode)or 0
    local pump=systems and systems.pump_on~=false
    state.source_id=core.configure_dynamic_construct_liquid_port({
        id=state.source_id,compartment_id=state.compartment_id,position=state.cells[1],
        kind="pump_in",liquid="water",rate=ballast==1 and 24 or 0,enabled=ballast==1,
    })
    state.drain_id=core.configure_dynamic_construct_liquid_port({
        id=state.drain_id,compartment_id=state.compartment_id,position=state.cells[1],
        kind="pump_out",liquid="water",rate=pump and 32 or 0,enabled=pump,
    })
end

function M.configure_native(construct_id,nodes,systems)
    if not M.native_available()or not construct_id then return nil end
    local k=key(construct_id);local state=M.states[k]
    local cells=bilge_cells(nodes)
    local signature=tostring(#cells)..":"..tostring(nodes and #nodes or 0)
    if not state or state.signature~=signature then
        state={construct_id=construct_id,cells=cells,signature=signature}
        local id,err=core.configure_dynamic_construct_liquid_compartment(construct_id,{
            name="bilge",cells=cells,sealed=true,allow_mixing=false,
            horizontal_flow_rate=8,vertical_flow_rate=24,
        })
        if not id then core.log("warning","[NavyCraft] liquid compartment rejected: "..tostring(err));return nil end
        state.compartment_id=id;M.states[k]=state
    end
    configure_ports(state,systems)
    return state
end

function M.configure(construct)
    if not construct or not construct.native_id then return nil end
    return M.configure_native(construct.native_id,construct.nodes,construct.systems)
end

function M.configure_fragment(fragment_id)
    if not M.native_available()then return nil end
    local ok,data=pcall(core.get_dynamic_construct,fragment_id)
    if not ok or type(data)~="table"then return nil end
    return M.configure_native(fragment_id,data.nodes or{},nil)
end

local function apply_status(status)
    local construct
    for _,candidate in pairs(navycraft.preview.get_all and navycraft.preview.get_all()or{})do
        if tostring(candidate.native_id)==tostring(status.definition and status.definition.construct_id or status.construct_id)then construct=candidate;break end
    end
    if not construct or not construct.systems then return end
    construct.systems.native_liquid_fraction=tonumber(status.fill_fraction)or 0
    construct.systems.native_liquid_units=tonumber(status.total_units)or 0
    local displacement=math.max(1,tonumber(construct.systems.displacement)or tonumber(construct.profile and construct.profile.displacement)or 1)
    construct.systems.flooding=math.max(0,construct.systems.native_liquid_fraction*displacement)
end

function M.step(dt)
    if not M.native_available()then return end
    M.accumulator=M.accumulator+math.max(0,tonumber(dt)or 0)
    if M.accumulator<.1 then return end
    local delta=math.min(M.accumulator,.5);M.accumulator=0
    local active={}
    for _,construct in pairs(navycraft.preview.get_all and navycraft.preview.get_all()or{})do
        if construct.native_id then active[key(construct.native_id)]=true;M.configure(construct)end
    end
    for k in pairs(M.states)do if not active[k]and not M.states[k].fragment then M.states[k]=nil end end
    local result,err=core.step_dynamic_construct_liquids(delta)
    if not result then core.log("error","[NavyCraft] liquid step failed: "..tostring(err));return end
    for _,status in ipairs(result.compartments or{})do apply_status(status)end
end

function M.status(construct)
    if not construct or not construct.native_id then return "native liquids unavailable"end
    M.configure(construct)
    local data,err=core.get_dynamic_construct_liquids(construct.native_id)
    if not data then return tostring(err or"liquid state unavailable")end
    local compartments=data.compartments or{};local total,capacity=0,0
    for _,status in ipairs(compartments)do total=total+(tonumber(status.total_units)or 0);capacity=capacity+(tonumber(status.capacity_units)or 0)end
    return string.format("liquids=native cells=%d compartments=%d fill=%.1f%% units=%d/%d pump=%s ballast=%d",
        #(data.cells or{}),#compartments,capacity>0 and total/capacity*100 or 0,math.floor(total+.5),math.floor(capacity+.5),
        construct.systems and construct.systems.pump_on~=false and"ON"or"OFF",
        construct.systems and tonumber(construct.systems.ballast_mode)or 0)
end

local function register_special_nodes()
    if type(core.register_dynamic_construct_special_node)~="function"then return end
    for _,definition in ipairs({
        {name="nc_navycraft:pump",breathable=true,attachable_platform=true},
        {name="nc_navycraft:ballast",breathable=false,liquid_permeable=true,attachable_platform=true},
        {name="nc_navycraft:rudder",breathable=true,attachable_platform=true},
    })do pcall(core.register_dynamic_construct_special_node,definition)end
end

M.enabled=M.native_available()
if M.enabled then
    register_special_nodes()
    core.register_globalstep(M.step)
    core.log("action","[NavyCraft] native construct-local liquids and specialised nodes enabled")
end
return M

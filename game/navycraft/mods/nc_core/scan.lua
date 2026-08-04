-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
local M = {}
local neighbours = {
    {x = 1, y = 0, z = 0}, {x = -1, y = 0, z = 0},
    {x = 0, y = 1, z = 0}, {x = 0, y = -1, z = 0},
    {x = 0, y = 0, z = 1}, {x = 0, y = 0, z = -1},
}

function M.default_allowed(pos, node)
    if node.name == "air" or node.name == "ignore" then return false end
    local definition = core.registered_nodes[node.name]
    if not definition then return false end
    if definition.liquidtype and definition.liquidtype ~= "none" then return false end
    local groups = definition.groups or {}
    return (groups.navycraft_hull or 0) > 0 or
        (groups.navycraft_component or 0) > 0
end

local function key(pos)
    return pos.x .. ":" .. pos.y .. ":" .. pos.z
end

function M.connected_nodes(origin, options)
    options = options or {}
    local max_nodes = options.max_nodes or 5000
    local is_allowed = options.is_allowed or M.default_allowed
    local first = core.get_node_or_nil(origin)
    if not first then return nil, "origin mapblock is not loaded" end
    if not is_allowed(origin, first) then
        return nil, "origin node is not an allowed craft block (" .. first.name .. ")"
    end

    local queue = {vector.new(origin)}
    local cursor = 1
    local visited = {[key(origin)] = true}
    local nodes = {}
    local minp = vector.new(origin)
    local maxp = vector.new(origin)

    while cursor <= #queue do
        local pos = queue[cursor]
        cursor = cursor + 1
        local node = core.get_node_or_nil(pos)
        if node and is_allowed(pos, node) then
            local metadata = core.serialize(core.get_meta(pos):to_table())
            nodes[#nodes + 1] = {
                pos = vector.new(pos),
                name = node.name,
                param1 = node.param1,
                param2 = node.param2,
                metadata = metadata,
            }
            minp.x = math.min(minp.x, pos.x)
            minp.y = math.min(minp.y, pos.y)
            minp.z = math.min(minp.z, pos.z)
            maxp.x = math.max(maxp.x, pos.x)
            maxp.y = math.max(maxp.y, pos.y)
            maxp.z = math.max(maxp.z, pos.z)
            if #nodes > max_nodes then
                return nil, "craft exceeds node limit of " .. max_nodes
            end
            for _, offset in ipairs(neighbours) do
                local next_pos = vector.add(pos, offset)
                local next_key = key(next_pos)
                if not visited[next_key] then
                    visited[next_key] = true
                    local next_node = core.get_node_or_nil(next_pos)
                    if next_node and is_allowed(next_pos, next_node) then
                        queue[#queue + 1] = next_pos
                    end
                end
            end
        end
    end

    return {origin = vector.new(origin), nodes = nodes, minp = minp, maxp = maxp}
end

return M

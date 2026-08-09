-- Copyright (c) 2026 Brett Martinelli. All rights reserved.
-- Lua fallback visual shell for native constructs.

local V = {}

local BLOCK_ENTITY = "nc_core:construct_visual_block"
local ANCHOR_ENTITY = "nc_core:construct_visual_anchor"
local ATTACH_OFFSET_SCALE = 10
local ANCHOR_TEXTURE = "nc_frame.png^[opacity:0"
local DIRS = {
    {x = 1, y = 0, z = 0}, {x = -1, y = 0, z = 0},
    {x = 0, y = 1, z = 0}, {x = 0, y = -1, z = 0},
    {x = 0, y = 0, z = 1}, {x = 0, y = 0, z = -1},
}

local function has_entities()
    return type(core.register_entity) == "function" and type(core.add_entity) == "function"
end

local function texture_name(tile)
    if type(tile) == "table" then
        return tile.name or tile.image or tile.texture or tile[1] or "nc_frame.png"
    end
    return tile or "nc_frame.png"
end

local function node_textures(node_name)
    local def = core.registered_nodes[node_name] or {}
    local tiles = def.tiles or {}
    local textures = {}
    if #tiles == 0 then
        for i = 1, 6 do textures[i] = "nc_frame.png" end
    elseif #tiles == 1 then
        local tile = texture_name(tiles[1])
        for i = 1, 6 do textures[i] = tile end
    else
        for i = 1, 6 do textures[i] = texture_name(tiles[i] or tiles[1]) end
    end
    return textures
end

local function key(pos)
    return string.format("%.3f:%.3f:%.3f", pos.x or 0, pos.y or 0, pos.z or 0)
end

local function compute_bounds(nodes)
    local bounds = {
        minp = {x = math.huge, y = math.huge, z = math.huge},
        maxp = {x = -math.huge, y = -math.huge, z = -math.huge},
    }
    for _, entry in ipairs(nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            local p = entry.local_pos
            bounds.minp.x = math.min(bounds.minp.x, p.x)
            bounds.minp.y = math.min(bounds.minp.y, p.y)
            bounds.minp.z = math.min(bounds.minp.z, p.z)
            bounds.maxp.x = math.max(bounds.maxp.x, p.x)
            bounds.maxp.y = math.max(bounds.maxp.y, p.y)
            bounds.maxp.z = math.max(bounds.maxp.z, p.z)
        end
    end
    if bounds.minp.x == math.huge then
        bounds.minp = {x = 0, y = 0, z = 0}
        bounds.maxp = {x = 0, y = 0, z = 0}
    end
    return bounds
end

local function local_center(construct, local_pos)
    local preview = rawget(_G, "navycraft") and navycraft.preview or nil
    local world = preview and preview.world_position and preview.world_position(construct, local_pos)
    if world then
        return world
    end
    return vector.add(construct.position or {x = 0, y = 0, z = 0}, local_pos or {x = 0, y = 0, z = 0})
end

local function attach_offset(local_pos)
    local pos = local_pos or {x = 0, y = 0, z = 0}
    return {
        x = (pos.x or 0) * ATTACH_OFFSET_SCALE,
        y = (pos.y or 0) * ATTACH_OFFSET_SCALE,
        z = (pos.z or 0) * ATTACH_OFFSET_SCALE,
    }
end

local function construct_for_id(id)
    local nc = rawget(_G, "navycraft")
    return nc and nc.preview and nc.preview.get_by_id and nc.preview.get_by_id(id) or nil
end

local function native_renderer_active()
    local nc = rawget(_G, "navycraft")
    local construct = nc and nc.construct or nil
    if construct and type(construct.native_renderer_available) == "function" then
        local ok, available = pcall(construct.native_renderer_available)
        return ok and available == true
    end
    return false
end

local function remove_block_visuals(shell)
    if not shell then return end
    for index, existing in pairs(shell.blocks or {}) do
        if existing.object and existing.object.remove then existing.object:remove() end
        shell.blocks[index] = nil
    end
    if shell.collider and shell.collider.remove then shell.collider:remove() end
    shell.collider = nil
end

local function visible_entry(construct, index, occupied)
    local entry = construct.nodes and construct.nodes[index]
    if not entry or entry.destroyed then return false end
    local def = core.registered_nodes[entry.name] or {}
    if def._navycraft_component or def._navycraft_helm then return true end
    local local_pos = entry.local_pos
    for _, dir in ipairs(DIRS) do
        local neighbour = key(vector.add(local_pos, dir))
        if not occupied[neighbour] then return true end
    end
    return false
end

local function block_properties(textures)
    return {
        physical = true,
        collide_with_objects = true,
        pointable = true,
        visual = "cube",
        visual_size = {x = 1, y = 1, z = 1},
        collisionbox = {-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
        selectionbox = {-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
        textures = textures or {"nc_frame.png", "nc_frame.png", "nc_frame.png", "nc_frame.png", "nc_frame.png", "nc_frame.png"},
        static_save = false,
    }
end

local function tag_block_entity(object, construct, index, entry)
    local lua = object and object.get_luaentity and object:get_luaentity()
    if not lua then return end
    lua.construct_id = construct.id
    lua.node_index = index
    lua.local_pos = entry.local_pos and vector.new(entry.local_pos) or nil
    lua.node_name = entry.name
end

if has_entities() then
    core.register_entity(ANCHOR_ENTITY, {
        initial_properties = {
            physical = false,
            collide_with_objects = false,
            pointable = false,
            visual = "cube",
            visual_size = {x = 1, y = 1, z = 1},
            textures = {ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE},
            static_save = false,
        },
    })

    core.register_entity(BLOCK_ENTITY, {
        initial_properties = block_properties(),
        on_activate = function(self)
            if self.object and self.object.set_armor_groups then
                self.object:set_armor_groups({immortal = 1})
            end
        end,
        on_step = function(self)
            if not self.object then return end
            if self.object.set_velocity then self.object:set_velocity({x = 0, y = 0, z = 0}) end
            if self.object.set_acceleration then self.object:set_acceleration({x = 0, y = 0, z = 0}) end
            if self.object.get_attach and self.object:get_attach() then return end
            local construct = construct_for_id(self.construct_id)
            if construct and self.local_pos and self.object.set_pos then
                self.object:set_pos(local_center(construct, self.local_pos))
            end
        end,
        on_rightclick = function(self, clicker)
            local nc = rawget(_G, "navycraft")
            local construct = construct_for_id(self.construct_id)
            if construct and nc.nodes and nc.nodes.handle_moving_interaction then
                nc.nodes.handle_moving_interaction(construct, self.node_index, clicker, "rightclick")
            end
        end,
    })

end

-- Keep all visible block entities parented to one construct anchor. Free
-- physics per block tears the ship apart because Luanti solves each object
-- independently; attachment gives the shell one transform until nodes are
-- actually destroyed.
local function attach_supported()
    return has_entities()
end

local function ensure_shell(construct)
    if not has_entities() or not construct then return nil end
    construct._visual_shell = construct._visual_shell or {blocks = {}, revision = -1}
    return construct._visual_shell
end

local function spawn_anchor(construct, shell)
    if not attach_supported() then return nil end
    if shell.anchor and shell.anchor.get_pos and shell.anchor:get_pos() then
        shell.anchor:set_pos(vector.new(construct.position or {x = 0, y = 0, z = 0}))
        shell.anchor:set_yaw(construct.yaw or 0)
        return shell.anchor
    end
    local anchor = core.add_entity(vector.new(construct.position or {x = 0, y = 0, z = 0}), ANCHOR_ENTITY)
    if not anchor then return nil, "unable to create construct visual anchor" end
    if type(anchor.set_properties) == "function" then
        anchor:set_properties({
            visual = "cube",
            visual_size = {x = 1, y = 1, z = 1},
            textures = {ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE, ANCHOR_TEXTURE},
        })
    end
    if type(anchor.set_yaw) == "function" then anchor:set_yaw(construct.yaw or 0) end
    shell.anchor = anchor
    return anchor
end

local function spawn_collider(construct, shell)
    return nil
end

local function sync_collider(construct, shell)
    return true
end

local function ensure_block(shell, construct, index, entry, anchor)
    local existing = shell.blocks[index]
    local textures = node_textures(entry.name)
    local should_attach = attach_supported() and anchor ~= nil
    local offset = attach_offset(entry.local_pos)
    local world = local_center(construct, entry.local_pos or {x = 0, y = 0, z = 0})

    if existing and existing.object and existing.object.get_pos and existing.object:get_pos() then
        tag_block_entity(existing.object, construct, index, entry)
        if existing.name ~= entry.name then
            if type(existing.object.set_properties) == "function" then
                existing.object:set_properties(block_properties(textures))
            end
            existing.name = entry.name
        elseif type(existing.object.set_properties) == "function" then
            existing.object:set_properties(block_properties(textures))
        end
        if type(existing.object.set_velocity) == "function" then existing.object:set_velocity({x = 0, y = 0, z = 0}) end
        if type(existing.object.set_acceleration) == "function" then existing.object:set_acceleration({x = 0, y = 0, z = 0}) end
        if should_attach and type(existing.object.set_attach) == "function" then
            existing.object:set_attach(anchor, "", offset, {x = 0, y = 0, z = 0}, true)
        elseif type(existing.object.set_pos) == "function" then
            existing.object:set_pos(world)
        end
        return existing.object
    end

    local object = core.add_entity(world, BLOCK_ENTITY)
    if not object then return nil, "unable to create construct visual block" end
    if type(object.set_properties) == "function" then
        object:set_properties(block_properties(textures))
    end
    tag_block_entity(object, construct, index, entry)
    if type(object.set_velocity) == "function" then object:set_velocity({x = 0, y = 0, z = 0}) end
    if type(object.set_acceleration) == "function" then object:set_acceleration({x = 0, y = 0, z = 0}) end
    if should_attach and type(object.set_attach) == "function" then
        object:set_attach(anchor, "", offset, {x = 0, y = 0, z = 0}, true)
    elseif type(object.set_pos) == "function" then
        object:set_pos(world)
    end
    shell.blocks[index] = {object = object, name = entry.name}
    return object
end

local function rebuild_shell(construct, shell)
    if not shell then return true end
    local occupied = {}
    for _, entry in ipairs(construct.nodes or {}) do
        if not entry.destroyed and entry.local_pos then
            occupied[key(entry.local_pos)] = true
        end
    end

    local anchor = spawn_anchor(construct, shell)
    if attach_supported() and not anchor then
        return false, "unable to create construct visual anchor"
    end

    for index, entry in ipairs(construct.nodes or {}) do
        local should_show = visible_entry(construct, index, occupied)
        local existing = shell.blocks[index]
        if should_show then
            local ok, error_message = ensure_block(shell, construct, index, entry, anchor)
            if not ok then return false, error_message end
        elseif existing and existing.object and existing.object.remove then
            existing.object:remove()
            shell.blocks[index] = nil
        end
    end

    for index, existing in pairs(shell.blocks) do
        if not (construct.nodes and construct.nodes[index]) or construct.nodes[index].destroyed then
            if existing.object and existing.object.remove then existing.object:remove() end
            shell.blocks[index] = nil
        end
    end

    local synced, collider_error = sync_collider(construct, shell)
    if not synced then
        return false, collider_error
    end

    shell.revision = shell.revision + 1
    return true
end

function V.spawn(construct)
    if not has_entities() or not construct then return true end
    local shell = ensure_shell(construct)
    if not shell then return true end
    if native_renderer_active() then
        remove_block_visuals(shell)
        local anchor = spawn_anchor(construct, shell)
        return anchor ~= nil or true
    end
    return rebuild_shell(construct, shell)
end

function V.anchor(construct)
    if not has_entities() or not construct then return nil end
    local shell = ensure_shell(construct)
    if not shell then return nil end
    if native_renderer_active() then remove_block_visuals(shell) end
    return spawn_anchor(construct, shell)
end

function V.sync(construct)
    if not has_entities() or not construct or not construct._visual_shell then return true end
    local shell = construct._visual_shell
    if native_renderer_active() then
        remove_block_visuals(shell)
        local anchor = spawn_anchor(construct, shell)
        if anchor and type(anchor.set_pos) == "function" then
            anchor:set_pos(vector.new(construct.position or {x = 0, y = 0, z = 0}))
        end
        if anchor and type(anchor.set_yaw) == "function" then
            anchor:set_yaw(construct.yaw or 0)
        end
        return true
    end
    local anchor = shell.anchor
    if anchor and anchor.get_pos and not anchor:get_pos() then
        shell.anchor = nil
        anchor = nil
    end
    if attach_supported() then
        if not anchor then
            local ok, error_message = V.spawn(construct)
            return ok, error_message
        end
        if type(anchor.set_pos) == "function" then
            anchor:set_pos(vector.new(construct.position or {x = 0, y = 0, z = 0}))
        end
        if type(anchor.set_yaw) == "function" then
            anchor:set_yaw(construct.yaw or 0)
        end
    end
    local ok, error_message = rebuild_shell(construct, shell)
    if not ok then return ok, error_message end
    return sync_collider(construct, shell)
end

function V.remove(construct)
    if not construct or not construct._visual_shell then return true end
    local shell = construct._visual_shell
    for _, existing in pairs(shell.blocks or {}) do
        if existing.object and existing.object.remove then existing.object:remove() end
    end
    if shell.anchor and shell.anchor.remove then shell.anchor:remove() end
    if shell.collider and shell.collider.remove then shell.collider:remove() end
    construct._visual_shell = nil
    return true
end

return V

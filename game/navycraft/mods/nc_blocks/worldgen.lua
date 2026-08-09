local W = {}

local function cid(name)
    local id = core.get_content_id(name)
    if id == nil then
        error("missing NavyCraft worldgen node: " .. name)
    end
    return id
end

local C = {
    air = cid("air"),
    water = cid("nc_core:water_source"),
    stone = cid("nc_blocks:stone"),
    dirt = cid("nc_blocks:dirt"),
    grass = cid("nc_blocks:grass"),
    sand = cid("nc_blocks:sand"),
    red_sand = cid("nc_blocks:red_sand"),
    gravel = cid("nc_blocks:gravel"),
    clay = cid("nc_blocks:clay"),
    snow = cid("nc_blocks:snow"),
    ice = cid("nc_blocks:ice"),
    oak_log = cid("nc_blocks:oak_wood"),
    oak_leaves = cid("nc_blocks:oak_leaves"),
    spruce_log = cid("nc_blocks:spruce_wood"),
    spruce_leaves = cid("nc_blocks:spruce_leaves"),
    cactus = cid("nc_blocks:cactus"),
    dead_bush = cid("nc_blocks:dead_bush"),
    coal_ore = cid("nc_blocks:coal_ore"),
    iron_ore = cid("nc_blocks:iron_ore"),
    gold_ore = cid("nc_blocks:gold_ore"),
    diamond_ore = cid("nc_blocks:diamond_ore"),
}

local SEA_LEVEL = 1
local WORLD_BOTTOM = -64
local SURFACE_DEPTH = 4
local logged_first_chunk = false

local n_terrain = {
    offset = 0,
    scale = 1,
    spread = {x = 384, y = 384, z = 384},
    seed = 91441,
    octaves = 5,
    persist = 0.52,
    lacunarity = 2.0,
}

local n_ridge = {
    offset = 0,
    scale = 1,
    spread = {x = 96, y = 96, z = 96},
    seed = 14633,
    octaves = 3,
    persist = 0.48,
    lacunarity = 2.0,
}

local n_biome = {
    offset = 0,
    scale = 1,
    spread = {x = 720, y = 720, z = 720},
    seed = 58321,
    octaves = 4,
    persist = 0.5,
    lacunarity = 2.0,
}

local n_humidity = {
    offset = 0,
    scale = 1,
    spread = {x = 640, y = 640, z = 640},
    seed = 23911,
    octaves = 4,
    persist = 0.5,
    lacunarity = 2.0,
}

local n_feature = {
    offset = 0,
    scale = 1,
    spread = {x = 48, y = 48, z = 48},
    seed = 79531,
    octaves = 2,
    persist = 0.45,
    lacunarity = 2.0,
}

local function hash2(x, z, salt)
    local n = math.sin(x * 127.1 + z * 311.7 + salt * 74.7) * 43758.5453123
    return n - math.floor(n)
end

local function height_at(terrain, ridge, index)
    local continents = terrain[index]
    local hills = math.abs(ridge[index])
    local height = SEA_LEVEL - 3 + continents * 24 + hills * 13
    if continents < -0.32 then
        height = SEA_LEVEL - 10 + continents * 18
    elseif continents > 0.48 then
        height = height + (continents - 0.48) * 36
    end
    return math.floor(height + 0.5)
end

local function biome_at(height, temp, humidity)
    if height <= SEA_LEVEL - 2 then return "ocean" end
    if height <= SEA_LEVEL + 2 then return "beach" end
    if temp < -0.33 then return "snow" end
    if temp > 0.38 and humidity < -0.18 then return "desert" end
    if humidity > 0.22 then
        if temp < -0.08 then return "taiga" end
        return "forest"
    end
    if height > 28 then return "hills" end
    return "plains"
end

local function surface_node(biome)
    if biome == "beach" then return C.sand end
    if biome == "desert" then return C.sand end
    if biome == "snow" or biome == "taiga" then return C.snow end
    if biome == "ocean" then return C.gravel end
    return C.grass
end

local function subsurface_node(biome)
    if biome == "beach" or biome == "desert" then return C.sand end
    if biome == "ocean" then return C.gravel end
    return C.dirt
end

local function maybe_ore(x, y, z, base)
    if base ~= C.stone then return base end
    local r = hash2(x + y * 13, z - y * 17, 921)
    if y < -40 and r < 0.002 then return C.diamond_ore end
    if y < -18 and r < 0.006 then return C.gold_ore end
    if y < 24 and r < 0.018 then return C.iron_ore end
    if y < 42 and r < 0.030 then return C.coal_ore end
    return base
end

local function set_tree(area, data, x, y, z, minp, maxp, log_id, leaves_id)
    if y + 6 > maxp.y or y < minp.y then return end
    for trunk_y = y, y + 4 do
        if trunk_y >= minp.y and trunk_y <= maxp.y then
            data[area:index(x, trunk_y, z)] = log_id
        end
    end
    for yy = y + 3, y + 6 do
        local radius = yy == y + 6 and 1 or 2
        for dx = -radius, radius do
            for dz = -radius, radius do
                local ax, az = x + dx, z + dz
                if ax >= minp.x and ax <= maxp.x and az >= minp.z and az <= maxp.z then
                    if math.abs(dx) + math.abs(dz) <= radius + 1 then
                        local vi = area:index(ax, yy, az)
                        if data[vi] == C.air then
                            data[vi] = leaves_id
                        end
                    end
                end
            end
        end
    end
end

local function place_feature(area, data, x, y, z, biome, minp, maxp, feature_value)
    if y + 1 > maxp.y or y < minp.y then return end
    local chance = hash2(x, z, 417)
    if biome == "forest" and feature_value > 0.18 and chance < 0.12 then
        set_tree(area, data, x, y + 1, z, minp, maxp, C.oak_log, C.oak_leaves)
    elseif biome == "taiga" and feature_value > 0.10 and chance < 0.10 then
        set_tree(area, data, x, y + 1, z, minp, maxp, C.spruce_log, C.spruce_leaves)
    elseif biome == "plains" and feature_value > 0.44 and chance < 0.018 then
        set_tree(area, data, x, y + 1, z, minp, maxp, C.oak_log, C.oak_leaves)
    elseif biome == "desert" and chance < 0.025 then
        local height = 2 + math.floor(hash2(x, z, 881) * 3)
        for yy = y + 1, math.min(y + height, maxp.y) do
            data[area:index(x, yy, z)] = C.cactus
        end
    elseif biome == "desert" and chance < 0.06 then
        data[area:index(x, y + 1, z)] = C.dead_bush
    end
end

function W.generate(minp, maxp, seed)
    if not logged_first_chunk then
        core.log("action", string.format(
            "[NavyCraft] nc_blocks worldgen generating first chunk from %s to %s",
            core.pos_to_string(minp),
            core.pos_to_string(maxp)))
        logged_first_chunk = true
    end
    local vm, emin, emax = core.get_mapgen_object("voxelmanip")
    if not vm then return end
    local area = VoxelArea:new({MinEdge = emin, MaxEdge = emax})
    local data = vm:get_data()
    local size2d = {x = maxp.x - minp.x + 1, y = maxp.z - minp.z + 1, z = 1}
    local origin2d = {x = minp.x, y = minp.z, z = 0}
    local terrain = core.get_perlin_map(n_terrain, size2d):get_2d_map_flat(origin2d)
    local ridge = core.get_perlin_map(n_ridge, size2d):get_2d_map_flat(origin2d)
    local biome_noise = core.get_perlin_map(n_biome, size2d):get_2d_map_flat(origin2d)
    local humidity_noise = core.get_perlin_map(n_humidity, size2d):get_2d_map_flat(origin2d)
    local feature_noise = core.get_perlin_map(n_feature, size2d):get_2d_map_flat(origin2d)

    local index2d = 1
    for z = minp.z, maxp.z do
        for x = minp.x, maxp.x do
            local height = height_at(terrain, ridge, index2d)
            local biome = biome_at(height, biome_noise[index2d], humidity_noise[index2d])
            local surface = surface_node(biome)
            local fill = subsurface_node(biome)

            for y = minp.y, maxp.y do
                local vi = area:index(x, y, z)
                if y <= WORLD_BOTTOM then
                    data[vi] = C.stone
                elseif y <= height - SURFACE_DEPTH then
                    data[vi] = maybe_ore(x, y, z, C.stone)
                elseif y < height then
                    data[vi] = fill
                elseif y == height then
                    data[vi] = surface
                elseif y <= SEA_LEVEL then
                    data[vi] = C.water
                else
                    data[vi] = C.air
                end
            end

            if height >= minp.y and height <= maxp.y and height > SEA_LEVEL + 1 then
                place_feature(area, data, x, height, z, biome, minp, maxp, feature_noise[index2d])
            end
            index2d = index2d + 1
        end
    end

    vm:set_data(data)
    vm:calc_lighting()
    vm:write_to_map()
end

core.register_alias("mapgen_stone", "nc_blocks:stone")
core.register_alias("mapgen_dirt", "nc_blocks:dirt")
core.register_alias("mapgen_dirt_with_grass", "nc_blocks:grass")
core.register_alias("mapgen_sand", "nc_blocks:sand")
core.register_alias("mapgen_water_source", "nc_core:water_source")
core.register_alias("mapgen_river_water_source", "nc_core:water_source")
core.register_alias("mapgen_tree", "nc_blocks:oak_wood")
core.register_alias("mapgen_leaves", "nc_blocks:oak_leaves")
core.register_alias("mapgen_lava_source", "air")
core.register_alias("mapgen_apple", "air")

core.register_on_generated(function(minp, maxp, seed)
    W.generate(minp, maxp, seed)
end)

nc_blocks.worldgen = W
return W

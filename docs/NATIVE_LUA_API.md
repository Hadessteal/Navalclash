# Native construct Lua API — Milestone 4I

The engine overlay registers these server-side functions on `core`. Construct IDs are decimal strings and should remain strings in Lua.

## Lifecycle and motion

- `core.create_dynamic_construct(definition)`
- `core.get_dynamic_construct(id, include_nodes)`
- `core.set_dynamic_construct_transform(id, transform)`
- `core.set_dynamic_construct_velocity(id, velocity, yaw_velocity)`
- `core.list_dynamic_constructs()`
- `core.remove_dynamic_construct(id)`

`create_dynamic_construct` accepts an origin, owner, yaw and an array of world-positioned node snapshots. The engine converts them to integer construct-local positions. Construct transforms are advanced by the authoritative fixed-step server simulation; Lua cannot manually step the registry.

## Construct raycast

```lua
local hit, err = core.raycast_dynamic_constructs(
    {x = 0, y = 10, z = 0},
    {x = 0, y = 10, z = 20}
)
```

A hit contains:

```lua
{
    id = "1",
    node_pos = {x = 0, y = 0, z = 4},
    local_point = {x = 0.5, y = 0.5, z = 4.0},
    world_point = {x = 12.2, y = 8.5, z = 31.7},
    normal = {x = 0, y = 0, z = -1},
    distance = 6.4,
}
```

It returns `nil` when no construct is hit, or `nil, error` for invalid arguments.

## Surface velocity

```lua
local velocity, err = core.get_dynamic_construct_surface_velocity(id, world_point)
```

Returns the velocity at a world-space point, including linear motion and yaw rotation. This supports passenger and projectile velocity inheritance.

## Local node edits

```lua
core.set_dynamic_construct_node(id, {x = 2, y = 1, z = 4}, {
    name = "nc_core:frame",
    param1 = 0,
    param2 = 0,
    metadata = "",
})

core.set_dynamic_construct_node(id, {x = 2, y = 1, z = 4}, nil)
```

The first form inserts or replaces a construct-local node. Passing `nil` removes it.

## Section inspection

```lua
local sections, err = core.get_dynamic_construct_sections(id)
```

Returns section positions, node counts and revisions. This is primarily a diagnostic API; native networking will own production section replication.

## Milestone 4E construct-local state and callbacks

```lua
core.get_dynamic_construct_node_state(id, local_pos)
core.set_dynamic_construct_metadata(id, local_pos, key, value_or_nil)
core.set_dynamic_construct_inventory(id, local_pos, list_name, definition_or_nil)
core.start_dynamic_construct_timer(id, local_pos, timeout, elapsed)
core.stop_dynamic_construct_timer(id, local_pos)
core.poll_dynamic_construct_events()
core.resolve_dynamic_construct_timer(id, event_id, restart, timeout_override)
```

`get_dynamic_construct_node_state` returns `fields`, `inventories`, `timer` and `revision`. Inventory definitions contain `width` and a `stacks` array of serialized item-stack strings.

The game adapter exposes explicit `_navycraft_*` callbacks rather than invoking standard node callbacks with false world positions. See `MILESTONE_4E_ENGINE.md`.


## Milestone 4F transactions and persistence

```lua
core.resolve_dynamic_construct_mutation(id, event_id, {
    accepted = true,
    protected_violation = false,
    replace_existing = false,
    node = {name = "nc_core:frame", param1 = 0, param2 = 0},
    wielded_item_after = "nc_core:frame 7",
    drops = {"nc_core:frame"},
    reason = "placed",
})

core.initialise_dynamic_construct_persistence(database_path)
core.sync_dynamic_construct_persistence()
core.get_dynamic_construct_actions(id, limit)
core.rollback_dynamic_construct_action(action_id)
core.dynamic_construct_local_to_world(id, local_position)
```

`resolve_dynamic_construct_mutation` commits or rejects a pending dig/place event. It returns `resolved`, `node_changed`, `message` and `action_id`. The target snapshot is checked again before commit.

Persistence stores complete construct snapshots and an action journal in SQLite. Rollback restores the full pre-action snapshot because ordinary static-map rollback positions cannot represent a translated or rotated vessel.

The game adapter's `context:show_formspec(specification)` opens a normal Luanti formspec with construct-local inventory lists mirrored through a detached inventory session.

## Milestone 4H construct effects

```lua
core.emit_dynamic_construct_effect(id, {
    kind = "sound_loop_start",
    effect_id = 1001,
    preset = "engine",
    local_pos = {x = 0, y = 1, z = -3},
    direction = {x = 0, y = 0, z = -1},
    velocity = {x = 0, y = 0, z = 0},
    sound = "nc_engine_loop",
    gain = 0.8,
    pitch = 1.1,
    max_distance = 100,
})
```

Accepted `kind` values are `sound`, `sound_loop_start`, `sound_loop_stop`, `particles`, `emitter_start`, `emitter_stop` and `light_flash`. Persistent sounds and emitters should use a stable numeric `effect_id`; stop events use the same ID.

Accepted presets are `generic`, `engine`, `wake`, `exhaust`, `smoke`, `fire`, `flood`, `muzzle`, `explosion`, `torpedo`, `depth_charge`, `splash` and `damage_sparks`. Optional particle fields include `texture`, `amount`, `size_min`, `size_max`, `lifetime_min`, `lifetime_max`, `glow` and `collision`. All positions and directions are construct-local.



## Milestone 4I native projectiles

```lua
local projectile_id, err = core.spawn_dynamic_construct_projectile(source_id, {
    kind = "torpedo",
    owner = "captain",
    position = {x = 10, y = -3, z = 20},
    velocity = {x = 0, y = 0, z = 9},
    target_id = target_construct_id,
    radius = 0.24,
    arming_time = 5,
    maximum_age = 60,
    maximum_range = 500,
    blast_radius = 6.7,
    blast_power = 14,
    penetration = 21,
    guided = true,
    guidance_turn_rate = 1.5,
    preferred_depth = -3,
    requires_water = true,
})

local events = core.step_dynamic_construct_projectiles(0.05)
local active = core.get_dynamic_construct_projectiles()
local impact = core.impact_dynamic_construct_projectile(
    projectile_id, {x = 20, y = -3, z = 40}, "terrain")
```

`kind` accepts `shell`, `fireball`, `torpedo`, `depth_charge`, `bomb`, `aa` or `anti_aircraft`. Construct IDs remain decimal strings. The spawn call requires an existing source construct.

`step_dynamic_construct_projectiles` returns authoritative `spawn`, `update`, `impact` and `remove` events. Impact events contain an explosion table with affected constructs and per-node power, armour, destruction and breach results.

`impact_dynamic_construct_projectile` is the server map-collision bridge. Its final argument accepts `terrain`, `water` or `expired`. The native engine removes the projectile, applies construct damage, persists changed vessels and broadcasts the impact.


## Milestone 4J native fire control

```lua
core.observe_dynamic_construct_target(observer_id, {
    target_id = target_construct_id,
    sample_time = server_seconds,
    position = {x = 20, y = 3, z = 40},
    velocity = {x = 2, y = 0, z = -1},
    classification = "surface",
    sensor = "radar",
    confidence = 0.9,
})

local solution = core.solve_dynamic_construct_fire_control(
    shooter_id, target_id, {
        kind = "shell",
        muzzle_local = {x = 0, y = 2, z = 4},
        muzzle_speed = 50,
        gravity = 9.81,
        drag = 0,
        minimum_range = 0,
        maximum_range = 500,
        maximum_lead_time = 20,
        minimum_pitch = -1.2,
        maximum_pitch = 1.2,
        arc = "low",
        solution_time = server_seconds,
    })

local battery_id = core.configure_dynamic_construct_battery(shooter_id, {
    mode = "automatic",
    target_id = target_id,
    muzzle_local = {x = 0, y = 2, z = 4},
    kind = "shell",
    muzzle_speed = 50,
    gravity = 9.81,
    maximum_range = 500,
    reload_seconds = 5,
    minimum_confidence = 0.25,
    traverse_centre = 0,
    traverse_half_width = 3.14159,
})

local orders = core.step_dynamic_construct_fire_control(server_seconds, 10)
local tracks = core.get_dynamic_construct_fire_control_tracks(shooter_id)
local batteries = core.get_dynamic_construct_fire_control_batteries(shooter_id)
core.remove_dynamic_construct_battery(battery_id)
```

Target `classification` accepts `unknown`, `surface`, `submarine`, `air` or `ground`. Sensor names accept `visual`, `radar`, `passive_sonar`, `active_sonar` or `data_link`. Battery mode accepts `manual`, `automatic` or `defensive`.

A solution returns `valid`, construct IDs, muzzle position, aim point, world `launch_velocity`, yaw, pitch, intercept time, range, closure rate, miss estimate, confidence and reason. Fire orders are advisory server decisions; the NavyCraft Lua layer still performs ammunition, launcher, mount and reload validation.

## Milestone 4K native navigation

```lua
core.configure_dynamic_construct_navigation(construct_id, {
    mode = "route",                 -- manual, hold, route or formation
    domain = "surface",             -- surface, submersible, air or ground
    maximum_speed = 8,
    maximum_reverse_speed = 2,
    maximum_acceleration = 2,
    maximum_deceleration = 3,
    maximum_yaw_rate = math.rad(20),
    maximum_yaw_acceleration = math.rad(45),
    maximum_vertical_speed = 2,
    arrival_radius = 3,
    lookahead = 24,
    obstacle_margin = 3,
    separation_distance = 10,
    avoidance_strength = 1.8,
    stuck_timeout = 8,
    recovery_seconds = 3,
    allow_reverse = true,
    loop = false,
    apply_to_construct = false,
})

core.set_dynamic_construct_route(construct_id, {
    {
        position = {x = 100, y = 0, z = 200},
        arrival_radius = 4,
        target_speed = 6,
        stop = false,
    },
    {
        position = {x = 150, y = 0, z = 240},
        arrival_radius = 4,
        target_speed = 2,
        stop = true,
    },
}, false)

core.observe_dynamic_construct_obstacle(construct_id, {
    id = 1,
    position = {x = 110, y = 0, z = 205},
    velocity = {x = 0, y = 0, z = 0},
    radius = 4,
    sample_time = server_seconds,
    expires_at = server_seconds + 0.35,
    hard = true,
})

local commands = core.step_dynamic_construct_navigation(delta_seconds, server_seconds)
local state = core.get_dynamic_construct_navigation(construct_id)
core.set_dynamic_construct_navigation_enabled(construct_id, true)
core.clear_dynamic_construct_navigation(construct_id)
```

Formation mode additionally accepts `leader_id` and `formation_offset`. Hold mode accepts `hold_position`. When `apply_to_construct` is false, each command returns advisory `forward_speed`, `vertical_speed`, `yaw_rate`, target, distance, waypoint, avoidance and recovery fields for the gameplay layer to apply. When true, the engine writes the desired native velocity directly.

Obstacle IDs are scoped to the observing construct. Dynamic construct separation is generated natively; the supplied game bridge contributes short-lived static-terrain observations from `core.raycast`.

## Milestone 4L structural connectivity and fragments

Protocol capability version 9 adds server-side structural state without adding a new network opcode.

```lua
local events = core.step_dynamic_construct_structure(delta_seconds, {
    water_level = 1,
    gravity = 9.81,
    fluid_density = 1,
    flooding_rate = 0.12,
    linear_water_drag = 0.8,
    angular_water_drag = 0.6,
    maximum_vertical_acceleration = 9.81,
    sink_depth = 128,
    minimum_fragment_nodes = 1,
    maximum_enclosure_cells = 500000,
    split_enabled = true,
    apply_fragment_physics = true,
    apply_primary_physics = false,
    recenter_fragments = true,
})

local state = core.get_dynamic_construct_structure(construct_id)
local states = core.get_dynamic_construct_structure(nil)
local split_events = core.force_dynamic_construct_split(construct_id, options)
core.set_dynamic_construct_flooding(construct_id, flooded_fraction)
```

Structural events use `updated`, `split`, `sinking` and `sunk`. A split event returns `fragment_ids` and `moved_nodes`. State fields include `role`, `component_count`, `node_count`, `control_nodes`, `breach_count`, `mass`, `solid_volume`, `enclosed_volume`, `submerged_volume`, `effective_displacement`, `flooded_fraction`, `integrity`, force values and afloat/sinking flags.

New fragments are ordinary native constructs. Existing transform and section packets replicate them, and the persistence service saves them during the next sync.

## Milestone 4M articulated machinery

Protocol capability version 10 adds native construct-local joints and turrets. Revolute positions are radians; prismatic positions are node units.

```lua
local yaw_joint = core.configure_dynamic_construct_joint(construct_id, {
    name = "main_turret_yaw",
    kind = "revolute",              -- revolute or prismatic
    control_mode = "position",      -- position, velocity or oscillate
    pivot = {x = 0, y = 3, z = 2},
    axis = {x = 0, y = 1, z = 0},
    nodes = {
        {x = 0, y = 3, z = 2},
        {x = 0, y = 3, z = 3},
    },
    minimum = -math.pi,
    maximum = math.pi,
    maximum_speed = math.rad(90),
    maximum_acceleration = math.rad(240),
    enabled = true,
    render_enabled = true,
    collision_enabled = true,
})

local pitch_joint = core.configure_dynamic_construct_joint(construct_id, {
    name = "main_turret_pitch",
    parent_id = yaw_joint,
    kind = "revolute",
    pivot = {x = 0, y = 3.25, z = 2.5},
    axis = {x = 1, y = 0, z = 0},
    nodes = {{x = 0, y = 3, z = 4}},
    minimum = math.rad(-8),
    maximum = math.rad(45),
    maximum_speed = math.rad(60),
    maximum_acceleration = math.rad(180),
})

local turret = core.configure_dynamic_construct_turret(construct_id, {
    name = "main_battery",
    yaw_joint_id = yaw_joint,
    pitch_joint_id = pitch_joint,
    muzzle_local = {x = 0, y = 3.25, z = 5},
    forward_local = {x = 0, y = 0, z = 1},
    alignment_tolerance = math.rad(2),
    projectile_radius = 0.08,
    maximum_range = 800,
    stabilised = true,
})

local status = core.aim_dynamic_construct_turret(turret.id, solution.aim_point)
local clear, obstruction, updated_status =
    core.dynamic_construct_line_of_fire(turret.id, solution.aim_point)

core.set_dynamic_construct_joint(yaw_joint, {target_position = math.rad(30)})
core.set_dynamic_construct_joint(yaw_joint, {velocity = math.rad(15)})
core.set_dynamic_construct_joint(yaw_joint, {enabled = false})

local step_result = core.step_dynamic_construct_articulations(delta_seconds, server_seconds)
local all = core.get_dynamic_construct_articulations(construct_id)
core.clear_dynamic_construct_turret_target(turret.id)
core.remove_dynamic_construct_joint(yaw_joint)
```

`configure_dynamic_construct_turret` returns a complete turret status table, including its assigned `id`. `aim_dynamic_construct_turret` accepts either a vector or a fire-control solution table containing `aim_point`.

`dynamic_construct_line_of_fire` returns three values: a boolean, an optional obstruction table and the current turret status. The obstruction includes construct/articulation IDs, local node position, local/world hit points and distance.

Removing a parent joint recursively removes its descendants and any turret that depends on them. Joint definitions and state snapshots are replicated to clients automatically.

## Milestone 4N articulated physics and construct liquids

Protocol capability version 11 extends rider state with an optional articulation ID and adds persistent construct-local liquid APIs.

```lua
local compartment_id = core.configure_dynamic_construct_liquid_compartment(construct_id, {
    name = "engine_room_bilge",
    cells = {
        {x = -2, y = -1, z = -4},
        {x = -1, y = -1, z = -4},
    },
    sealed = true,
    allow_mixing = false,
    horizontal_flow_rate = 8,
    vertical_flow_rate = 24,
})

local pump_id = core.configure_dynamic_construct_liquid_port({
    compartment_id = compartment_id,
    position = {x = -2, y = -1, z = -4},
    kind = "pump_out", -- pump_in, pump_out or breach
    liquid = "water",
    rate = 32,
    enabled = true,
})

core.set_dynamic_construct_liquid(
    construct_id, {x = -2, y = -1, z = -4}, "water", 8, 0)

local step = core.step_dynamic_construct_liquids(delta_seconds)
local state = core.get_dynamic_construct_liquids(construct_id)

core.register_dynamic_construct_special_node({
    name = "example:ladder",
    climbable = true,
    breathable = true,
    liquid_permeable = true,
    attachable_platform = true,
    damage_per_second = 0,
    conveyor_velocity = {x = 0, y = 0, z = 0},
    conveyor_acceleration = 8,
})
```

Liquid levels are integers from zero to eight. Reconfiguring a port with the same `id` updates its rate and enabled state. Compartment status includes capacity, total units, fill fraction, escaped units, mixed-liquid state and liquid centre of mass.

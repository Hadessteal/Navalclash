# Milestone 4L — structural connectivity, fragments and buoyancy

Milestone 4L turns destructive node edits into structural consequences. A vessel is no longer treated as one permanent object after launch: when damage removes the final load-bearing connection between two areas, the native server separates those areas into independent constructs.

## Connectivity model

The engine performs a six-neighbour flood fill over ship-local nodes. Registered material properties decide whether a node is load-bearing. The default resolver treats hull, frame, machinery and control nodes as structural, while lightweight attachments such as signs, torches, ladders, rails, wires, antennae and radio fittings attach to a component without joining two components together.

This distinction prevents a decorative cable or sign from unrealistically holding two halves of a ship together.

A custom material resolver can override:

- mass;
- displaced volume;
- structural strength;
- structural/non-structural classification;
- watertight behavior;
- control-node status.

The default material table is an engine adaptation. It is not presented as a recovered Bukkit NavyCraft material table.

## Primary section selection

When multiple components are found, the original construct ID remains with the best primary component. Selection is deterministic and prioritises:

1. control-node count;
2. total structural strength;
3. mass;
4. node count;
5. stable local-coordinate ordering.

This normally keeps the helm and operational systems on the original vessel while detached bow, stern, superstructure or armour sections become fragments.

## Fragment creation

Each detached component receives a new construct ID. The engine transfers:

- nodes and parameters;
- node metadata;
- construct-local inventories;
- node timers;
- owner;
- yaw and yaw velocity;
- linear velocity.

Fragments are recentered around an integer local pivot. Their transform is shifted by the same amount, so every node remains in exactly the same world-space position. Linear velocity also includes the rotational point velocity at the new pivot, preventing pieces from losing the tangential motion they had at the instant of separation.

The original component keeps its existing local coordinate system to avoid invalidating gameplay references to helms, engines, weapons and other controls.

## Enclosed air and displacement

For bounded components up to the configured cell limit, the engine flood-fills empty cells from the exterior of the local bounds. Empty cells not reachable from the exterior count as enclosed air volume.

Each section tracks:

- material mass;
- solid displaced volume;
- enclosed air volume;
- submerged volume;
- effective displacement after flooding;
- buoyancy and weight forces;
- vertical acceleration;
- integrity;
- breach count and flooded fraction.

This is deliberately section-local. A sealed detached compartment can float, while a heavy armour slab or opened compartment sinks independently.

## Flooding and damage integration

M4I projectile impacts already report destroyed nodes and below-waterline breaches. M4L records those breach counts in the structural state. Flooding rises over time according to breach count, component size and the configured flooding rate.

If enclosed volume disappears after a structural edit, the engine also treats the lost enclosed-air fraction as immediate flooding. This handles a compartment being opened without requiring a separate projectile callback.

The gameplay layer may call `set_dynamic_construct_flooding` to synchronise source-derived pump and flooding systems with the native calculation.

## Physics boundary

By default, native buoyancy and drag are applied to detached fragments and uncontrolled wrecks. The surviving primary section remains under the existing NavyCraft propulsion, ballast and buoyancy systems, avoiding two controllers fighting over the same vessel.

Fragment physics updates velocity and transform server-side and uses the existing transform replication path. No additional client packet opcode is required; protocol capability version 9 advertises the structural API.

## Lua APIs

```lua
local events = core.step_dynamic_construct_structure(delta_seconds, {
    water_level = 1,
    gravity = 9.81,
    fluid_density = 1,
    flooding_rate = 0.12,
    linear_water_drag = 0.8,
    angular_water_drag = 0.6,
    minimum_fragment_nodes = 1,
    maximum_enclosure_cells = 500000,
    split_enabled = true,
    apply_fragment_physics = true,
    apply_primary_physics = false,
    recenter_fragments = true,
})

local state = core.get_dynamic_construct_structure(construct_id)
local all_states = core.get_dynamic_construct_structure(nil)
local events = core.force_dynamic_construct_split(construct_id)
core.set_dynamic_construct_flooding(construct_id, 0.35)
```

Events are `updated`, `split`, `sinking` or `sunk`. Split events include the original construct ID, source ID, fragment IDs and moved-node count. State records include role, component and node counts, mass, volumes, flooding, integrity, forces and afloat/sinking flags.

## Controls

```text
/ship structure status
/ship structure split
/nc_structure
/navycraft structure
```

The forced split command only separates components that are already disconnected; it does not cut intact hull nodes.

## Verification

Tests cover:

- primary selection by helm/control nodes;
- separation after bridge removal;
- exact world-space preservation after recentering;
- rotational velocity inheritance;
- metadata transfer;
- heavy-fragment sinking;
- enclosed-air flood fill;
- decorative nodes not bridging structural components;
- a 5,000-node structural split benchmark;
- Lua status, flooding and forced-split integration;
- repeated overlay application;
- AddressSanitizer and UndefinedBehaviorSanitizer.

## Remaining runtime boundary

The complete patched Luanti executable still needs a full upstream build and multiplayer runtime pass. Real-world tuning remains for material masses, waterline sampling, large irregular hulls, fragment-to-fragment collision immediately after separation, unloaded-map behavior and long-lived wreck cleanup.

M4L does not yet implement bending stress, fatigue, elastic deformation or finite-element fracture. Connectivity changes occur after nodes are destroyed or explicitly removed.

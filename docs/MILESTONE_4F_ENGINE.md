# Milestone 4F — transactional interaction, formspecs and persistence

Milestone 4F turns M4E's construct-local events into server-authoritative transactions. A moving node is not changed until protection, item and node logic has accepted the operation. Accepted mutations are persisted and journalled; rejected operations leave the construct unchanged.

## Implemented

### Transactional digging and placement

- Completed digs and placements create pending mutation events instead of modifying the vessel immediately.
- The event captures the target node snapshot, actor, wielded item, local hit data and proposed placement.
- Lua resolves the event with an explicit accepted/rejected result.
- The native engine verifies that the target has not changed while the callback was running.
- Accepted changes update the construct, mark affected sections dirty, broadcast the section and write an action record.
- Rejected and protection-blocked attempts are also journalled for auditing.

### Digging semantics

The game adapter now performs:

- `core.is_protected` and `core.record_protection_violation` checks using the current transformed world position;
- construct-specific `_navycraft_can_dig` and `_navycraft_on_dig` callbacks;
- Luanti dig-parameter calculation for tool wear;
- node-drop calculation and delivery;
- wield-stack updates;
- construct-local destruct and after-destruct callbacks.

### Placement semantics

The adapter now performs:

- clicked-node right-click handling before default placement;
- protection checks for the authoritative adjacent local cell;
- registered-node and `buildable_to` validation;
- facedir, wallmounted and colour-param2 calculation;
- construct-specific `_navycraft_on_place` handling;
- item consumption outside creative mode;
- construct and after-place callbacks.

### Construct formspecs and inventories

- A construct-local formspec session identifies the construct and local node position.
- Native inventory lists are mirrored into a per-player detached inventory while the form is open.
- Move, put and take callbacks validate quantities and synchronise changes back to native construct state.
- Receive-fields events retain the construct-local context.
- Sessions are cleaned up on form close and player leave.

This bridge uses Luanti's normal formspec and detached-inventory GUI rather than introducing a second GUI protocol.

### SQLite persistence

The native persistence service stores:

- complete versioned construct snapshots;
- current node revision and update time;
- accepted, rejected and protection-blocked actions;
- actor, action type, local position, item before/after, drops and reason;
- complete before and after snapshots for rollback.

SQLite uses WAL mode and normal synchronous durability. The game initialises the database at `navycraft_constructs.sqlite` inside the world directory, periodically synchronises the registry and flushes during shutdown.

### Construct-local rollback

Ordinary Luanti rollback actions target static map coordinates. A moving construct node has no stable map coordinate, so M4F records a dedicated construct-local journal instead of writing misleading map-node rollback entries.

- `/nc_history <construct-id> [limit]` lists recent native actions.
- `/nc_rollback <action-id>` restores the complete construct snapshot from immediately before that action and broadcasts a full replacement state.

### Native API additions

- `core.resolve_dynamic_construct_mutation`
- `core.initialise_dynamic_construct_persistence`
- `core.sync_dynamic_construct_persistence`
- `core.get_dynamic_construct_actions`
- `core.rollback_dynamic_construct_action`
- `core.dynamic_construct_local_to_world`

The native construct protocol version is now 4.

## Verification

- Release build with warnings treated as errors.
- Transactional dig/place tests, including stale-target rejection.
- SQLite save, load, action-history and rollback tests.
- Construct metadata, inventory, timer, networking, collision and rider regression tests.
- Client, server and Lua API compatibility compilation.
- Overlay fixture applied twice without duplicate changes.
- Lua syntax and gameplay smoke tests, including formspec, protection, wear, drops and mutation resolution.
- Storefront-neutral platform build.
- AddressSanitizer and UndefinedBehaviorSanitizer pass.

## Remaining boundary

- The complete modified Luanti client and server executable still requires a real upstream-source build and runtime pass.
- Construct callbacks use the explicit `_navycraft_*` contract; arbitrary third-party mods written only for static map nodes are not automatically virtualised.
- The detached-inventory formspec bridge is functional but still needs an in-engine GUI runtime test.
- Database migration, corruption recovery, backup policy and long-duration durability testing remain for the stress-test phase.
- Specialised drawtypes, lighting and real node materials are still rendered by the simplified native mesh path.

The next engine slice is M4G: real node materials, facedir/nodebox geometry, lighting, transparency and animated texture support in the native construct renderer.

# Milestone 4M — articulated machinery, turrets and line of fire

Milestone 4M allows part of a moving construct to move relative to the construct itself. A vessel remains one persistent native construct, but selected local nodes can belong to a hierarchy of rotating or sliding joints.

## Generic articulation model

`ConstructArticulationEngine` supports two joint kinds:

- **revolute** — rotation around a construct-local pivot and axis;
- **prismatic** — translation along a construct-local axis.

Every joint can be controlled by a target position, commanded velocity or an oscillating scan range. Speed and acceleration limits are applied each simulation step. Joints can be nested, so a child inherits all parent transforms.

A turret is represented by a yaw joint and, when the model supplies a separate barrel group, an optional child pitch joint. The same system can represent doors, lifts, rudders, radar dishes and other machinery.

## Node ownership and rendering

Each articulated joint owns a list of construct-local nodes. The client excludes those nodes from the vessel's ordinary static section mesh and builds separate MapBlockMesh-backed meshes for the joint. Joint roots are attached to the construct's parent scene node and to one another according to the hierarchy.

The client receives reliable joint definitions and interpolated state snapshots. Existing Luanti node materials, drawtypes, lighting and animation continue to be used because the articulated meshes use the same M4G MapBlockMesh path.

## Turret aiming

A stabilised turret stores a world-space target. On every server step the engine transforms that target into construct and parent-joint space, computes desired yaw and pitch, clamps both to joint limits, and drives the joints toward the solution using their acceleration limits.

The returned turret status includes:

- desired and current yaw/pitch;
- world muzzle and forward direction;
- angular error;
- target, aligned and obstructed flags;
- the first blocking construct-local node when applicable.

NavyCraft fire-control solutions pass their predicted `aim_point` to this system. Manual or automatic fire is withheld while the turret is slewing.

## Line-of-fire obstruction

The obstruction query starts at the transformed muzzle and tests toward the target. Nodes owned by the firing turret and its descendants are excluded, preventing the barrel from blocking itself. Static vessel nodes and nodes owned by other joints remain valid blockers.

The query operates in construct-local space and inverse-transforms the ray for each articulated node group. A projectile radius can expand node bounds, so a physically wide shell cannot pass through a gap that only its centre ray fits through.

## Replication

Protocol command `0x6B` carries versioned `NCAR` packets:

- joint definition;
- joint state snapshot;
- joint removal;
- construct articulation reset.

`TOCLIENT_NUM_MSG_TYPES` is now `0x6C`. Late-joining clients receive every joint definition followed by its current state. Construct removal clears all associated joint state.

## Lua API

Protocol capability version 10 exposes configuration, control, query and line-of-fire functions documented in `NATIVE_LUA_API.md`.

The supplied gameplay bridge currently creates a yaw articulation for each detected weapon mount and a turret record tied to that joint. Radar components can use oscillating revolute joints. Fire still passes through the existing NavyCraft ammunition, permission, reload and launcher-safety checks.

## Verification

Tests cover:

- yaw and child-pitch transforms;
- arbitrary-axis engine transform composition;
- prismatic doors;
- elevated target convergence;
- joint node ownership;
- own-turret exclusion;
- superstructure obstruction;
- packet round trips and safety limits;
- stale snapshot rejection;
- client interpolation and extrapolation;
- gameplay turret setup, aim, clear line of fire and articulation stepping.

Release and sanitizer builds pass, along with client/server compatibility targets and repeated overlay application.

## Remaining boundary

The complete patched Luanti executable has not been linked and visually tested. The scene adapter expresses joint rotations as Irrlicht Euler components, so cardinal axes are the validated practical configuration even though the engine-independent affine math accepts arbitrary axes.

M4M does not yet merge articulated subassemblies into player, creature or dropped-item platform contacts. A player cannot currently stand on a moving lift or rotating turret and inherit that subassembly's motion. Articulated collision is used for construct-local line-of-fire tests, not yet for the main moving-body collision response.
